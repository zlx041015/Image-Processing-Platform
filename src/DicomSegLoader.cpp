#include "DicomSegLoader.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <algorithm>
#include <functional>

namespace {
struct Tag {
    quint16 group = 0;
    quint16 element = 0;
    bool operator==(const Tag& other) const { return group == other.group && element == other.element; }
};

struct SegFrameInfo {
    int segmentNumber = 0;
    QString referencedSopInstanceUid;
};

quint16 read16(const QByteArray& data, qsizetype offset) {
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data.constData() + offset));
}

quint32 read32(const QByteArray& data, qsizetype offset) {
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + offset));
}

Tag readTag(const QByteArray& data, qsizetype offset) {
    return {read16(data, offset), read16(data, offset + 2)};
}

struct ElementHeader {
    Tag tag;
    QString vr;
    quint32 len = 0;
    qsizetype valuePos = 0;
    qsizetype nextPos = 0;
    bool valid = false;
};

ElementHeader readElement(const QByteArray& data, qsizetype pos) {
    ElementHeader h;
    if (pos + 8 > data.size()) {
        return h;
    }
    h.tag = readTag(data, pos);
    pos += 4;
    h.vr = QString::fromLatin1(data.constData() + pos, 2);
    pos += 2;
    if (h.vr == "OB" || h.vr == "OW" || h.vr == "OF" || h.vr == "SQ" || h.vr == "UT" || h.vr == "UN") {
        pos += 2;
        if (pos + 4 > data.size()) return h;
        h.len = read32(data, pos);
        pos += 4;
    } else {
        if (pos + 2 > data.size()) return h;
        h.len = read16(data, pos);
        pos += 2;
    }
    h.valuePos = pos;
    h.nextPos = pos + h.len;
    h.valid = h.nextPos <= data.size() || h.len == 0xFFFFFFFF;
    return h;
}

void parseDataset(
    const QByteArray& data,
    qsizetype start,
    qsizetype end,
    const std::function<void(const ElementHeader&, const QByteArray&)>& onLeaf,
    const std::function<void(const ElementHeader&, qsizetype, qsizetype)>& onSequence
) {
    qsizetype pos = start;
    while (pos + 8 <= end && pos + 8 <= data.size()) {
        const Tag tag = readTag(data, pos);
        if (tag.group == 0xFFFE && (tag.element == 0xE0DD || tag.element == 0xE00D)) {
            break;
        }
        const ElementHeader h = readElement(data, pos);
        if (!h.valid) {
            break;
        }
        if (h.vr == "SQ") {
            onSequence(h, h.valuePos, h.len == 0xFFFFFFFF ? end : h.nextPos);
        } else {
            onLeaf(h, data.mid(h.valuePos, h.len));
        }
        pos = h.nextPos;
        if (h.len == 0xFFFFFFFF) {
            break;
        }
    }
}

void parseSequenceItems(
    const QByteArray& data,
    qsizetype start,
    qsizetype end,
    const std::function<void(qsizetype, qsizetype)>& onItem
) {
    qsizetype pos = start;
    while (pos + 8 <= data.size() && pos < end) {
        const Tag tag = readTag(data, pos);
        if (tag.group == 0xFFFE && tag.element == 0xE0DD) {
            break;
        }
        if (!(tag.group == 0xFFFE && tag.element == 0xE000)) {
            break;
        }
        pos += 4;
        const quint32 len = read32(data, pos);
        pos += 4;
        const qsizetype itemStart = pos;
        const qsizetype itemEnd = (len == 0xFFFFFFFF) ? end : std::min<qsizetype>(data.size(), pos + len);
        onItem(itemStart, itemEnd);
        pos = itemEnd;
        if (len == 0xFFFFFFFF) {
            break;
        }
    }
}

QImage unpackBinaryFrame(const QByteArray& pixelData, int frameIndex, int rows, int cols) {
    const int bitsPerFrame = rows * cols;
    const int bytesPerFrame = (bitsPerFrame + 7) / 8;
    const qsizetype start = static_cast<qsizetype>(frameIndex) * bytesPerFrame;
    if (start + bytesPerFrame > pixelData.size()) {
        return {};
    }
    QImage frame(cols, rows, QImage::Format_Grayscale8);
    frame.fill(0);
    const uchar* src = reinterpret_cast<const uchar*>(pixelData.constData() + start);
    int bitIndex = 0;
    for (int y = 0; y < rows; ++y) {
        uchar* dst = frame.scanLine(y);
        for (int x = 0; x < cols; ++x, ++bitIndex) {
            const int byteIndex = bitIndex / 8;
            const int inByte = bitIndex % 8;
            dst[x] = ((src[byteIndex] >> inByte) & 0x01) ? 255 : 0;
        }
    }
    return frame;
}
}

bool DicomSegLoader::loadSegFile(
    const QString& segFilePath,
    const DicomSeriesData& ctSeries,
    DicomSegData* outSeg,
    QString* errorMessage
) {
    if (!outSeg) {
        if (errorMessage) *errorMessage = QStringLiteral("invalid output pointer");
        return false;
    }

    QFile file(segFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot open SEG file");
        return false;
    }
    const QByteArray data = file.readAll();
    if (data.size() < 8) {
        if (errorMessage) *errorMessage = QStringLiteral("file is too small to be a readable DICOM SEG object");
        return false;
    }
    const qsizetype datasetStart = (data.size() >= 132 && data.mid(128, 4) == "DICM") ? 132 : 0;

    QString modality;
    int rows = 0;
    int cols = 0;
    int numberOfFrames = 0;
    int bitsAllocated = 0;
    QString segmentLabel;
    QString segmentAlgorithmType;
    QString referencedSeriesInstanceUid;
    QByteArray pixelData;
    QVector<SegFrameInfo> frames;

    parseDataset(
        data, datasetStart, data.size(),
        [&](const ElementHeader& h, const QByteArray& value) {
            if (h.tag == Tag{0x0008, 0x0060}) modality = QString::fromLatin1(value).trimmed();
            else if (h.tag == Tag{0x0028, 0x0010}) rows = read16(value, 0);
            else if (h.tag == Tag{0x0028, 0x0011}) cols = read16(value, 0);
            else if (h.tag == Tag{0x0028, 0x0008}) numberOfFrames = QString::fromLatin1(value).trimmed().toInt();
            else if (h.tag == Tag{0x0028, 0x0100}) bitsAllocated = read16(value, 0);
            else if (h.tag == Tag{0x7FE0, 0x0010}) pixelData = value;
        },
        [&](const ElementHeader& h, qsizetype seqStart, qsizetype seqEnd) {
            if (h.tag == Tag{0x0062, 0x0002}) {
                parseSequenceItems(data, seqStart, seqEnd, [&](qsizetype itemStart, qsizetype itemEnd) {
                    parseDataset(data, itemStart, itemEnd,
                        [&](const ElementHeader& eh, const QByteArray& value) {
                            if (eh.tag == Tag{0x0062, 0x0005}) segmentLabel = QString::fromLatin1(value).trimmed();
                            else if (eh.tag == Tag{0x0062, 0x0008}) segmentAlgorithmType = QString::fromLatin1(value).trimmed();
                        },
                        [&](const ElementHeader&, qsizetype, qsizetype) {});
                });
            } else if (h.tag == Tag{0x0008, 0x1115}) {
                parseSequenceItems(data, seqStart, seqEnd, [&](qsizetype itemStart, qsizetype itemEnd) {
                    parseDataset(data, itemStart, itemEnd,
                        [&](const ElementHeader& eh, const QByteArray& value) {
                            if (eh.tag == Tag{0x0020, 0x000E}) {
                                referencedSeriesInstanceUid = QString::fromLatin1(value).trimmed();
                            }
                        },
                        [&](const ElementHeader&, qsizetype, qsizetype) {});
                });
            } else if (h.tag == Tag{0x5200, 0x9230}) {
                parseSequenceItems(data, seqStart, seqEnd, [&](qsizetype itemStart, qsizetype itemEnd) {
                    SegFrameInfo frame;
                    parseDataset(data, itemStart, itemEnd,
                        [&](const ElementHeader&, const QByteArray&) {},
                        [&](const ElementHeader& inner, qsizetype innerStart, qsizetype innerEnd) {
                            if (inner.tag == Tag{0x0062, 0x000A}) {
                                parseSequenceItems(data, innerStart, innerEnd, [&](qsizetype s, qsizetype e) {
                                    parseDataset(data, s, e,
                                        [&](const ElementHeader& eh, const QByteArray& value) {
                                            if (eh.tag == Tag{0x0062, 0x000B}) {
                                                if (value.size() >= 2) {
                                                    frame.segmentNumber = read16(value, 0);
                                                }
                                            }
                                        },
                                        [&](const ElementHeader&, qsizetype, qsizetype) {});
                                });
                            } else if (inner.tag == Tag{0x0008, 0x9124}) {
                                parseSequenceItems(data, innerStart, innerEnd, [&](qsizetype s, qsizetype e) {
                                    parseDataset(data, s, e,
                                        [&](const ElementHeader&, const QByteArray&) {},
                                        [&](const ElementHeader& eh2, qsizetype s2, qsizetype e2) {
                                            if (eh2.tag == Tag{0x0008, 0x2112}) {
                                                parseSequenceItems(data, s2, e2, [&](qsizetype s3, qsizetype e3) {
                                                    parseDataset(data, s3, e3,
                                                        [&](const ElementHeader& eh3, const QByteArray& value) {
                                                            if (eh3.tag == Tag{0x0008, 0x1155}) {
                                                                frame.referencedSopInstanceUid = QString::fromLatin1(value).trimmed();
                                                            }
                                                        },
                                                        [&](const ElementHeader&, qsizetype, qsizetype) {});
                                                });
                                            }
                                        });
                                });
                            }
                        });
                    frames.append(frame);
                });
            }
        });

    if (modality != QStringLiteral("SEG")) {
        if (errorMessage) *errorMessage = QStringLiteral("file modality is not SEG");
        return false;
    }
    if (rows <= 0 || cols <= 0 || numberOfFrames <= 0 || bitsAllocated != 1 || pixelData.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("unsupported SEG structure");
        return false;
    }
    if (referencedSeriesInstanceUid != ctSeries.seriesInstanceUid) {
        if (errorMessage) *errorMessage = QStringLiteral("SEG does not reference current CT series");
        return false;
    }
    if (frames.size() != numberOfFrames) {
        if (errorMessage) *errorMessage = QStringLiteral("frame metadata count mismatch");
        return false;
    }

    outSeg->segmentLabel = segmentLabel;
    outSeg->segmentAlgorithmType = segmentAlgorithmType;
    outSeg->referencedSeriesInstanceUid = referencedSeriesInstanceUid;
    outSeg->alignedMaskSlices.clear();
    outSeg->alignedMaskSlices.reserve(ctSeries.rawSlices.size());
    for (const QImage& slice : ctSeries.rawSlices) {
        QImage mask(slice.size(), QImage::Format_Grayscale8);
        mask.fill(0);
        outSeg->alignedMaskSlices.append(mask);
    }
    outSeg->referencedSopInstanceUids = ctSeries.sopInstanceUids;

    int mappedFrames = 0;
    int nonEmptySlices = 0;
    int firstNonEmpty = -1;
    int lastNonEmpty = -1;

    for (int i = 0; i < numberOfFrames; ++i) {
        if (frames[i].segmentNumber != 1 || frames[i].referencedSopInstanceUid.isEmpty()) {
            continue;
        }
        const int targetIndex = ctSeries.sopInstanceUids.indexOf(frames[i].referencedSopInstanceUid);
        if (targetIndex < 0 || targetIndex >= outSeg->alignedMaskSlices.size()) {
            continue;
        }
        ++mappedFrames;
        const QImage frameMask = unpackBinaryFrame(pixelData, i, rows, cols);
        if (frameMask.isNull()) {
            continue;
        }
        QImage& dst = outSeg->alignedMaskSlices[targetIndex];
        bool frameHasForeground = false;
        for (int y = 0; y < dst.height(); ++y) {
            const uchar* src = frameMask.constScanLine(y);
            uchar* line = dst.scanLine(y);
            for (int x = 0; x < dst.width(); ++x) {
                if (src[x] > 0) {
                    line[x] = 255;
                    frameHasForeground = true;
                }
            }
        }
        if (frameHasForeground) {
            if (firstNonEmpty < 0) {
                firstNonEmpty = targetIndex;
            }
            lastNonEmpty = targetIndex;
        }
    }

    for (int z = 0; z < outSeg->alignedMaskSlices.size(); ++z) {
        const QImage mask = outSeg->alignedMaskSlices[z].convertToFormat(QImage::Format_Grayscale8);
        bool hasForeground = false;
        for (int y = 0; y < mask.height() && !hasForeground; ++y) {
            const uchar* row = mask.constScanLine(y);
            for (int x = 0; x < mask.width(); ++x) {
                if (row[x] > 0) {
                    hasForeground = true;
                    break;
                }
            }
        }
        if (hasForeground) {
            ++nonEmptySlices;
        }
    }

    if (mappedFrames == 0 || nonEmptySlices == 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("SEG 已读取，但没有有效帧映射到当前 CT 序列");
        }
        return false;
    }

    outSeg->valid = true;
    if (errorMessage) {
        *errorMessage = QStringLiteral("SEG 映射成功：帧 %1，非空切片 %2，范围 [%3, %4]")
            .arg(mappedFrames)
            .arg(nonEmptySlices)
            .arg(firstNonEmpty)
            .arg(lastNonEmpty);
    }
    return true;
}

bool DicomSegLoader::findAndLoadSegForSeries(
    const QString& rootPath,
    const DicomSeriesData& ctSeries,
    DicomSegData* outSeg,
    QString* errorMessage
) {
    QDirIterator it(rootPath, {"*.dcm", "*.dicom", "*.ima", "*.DCM", "*.DICOM", "*.IMA"},
        QDir::Files, QDirIterator::Subdirectories);
    QString firstError;
    while (it.hasNext()) {
        const QString path = it.next();
        DicomSegData seg;
        QString localError;
        if (loadSegFile(path, ctSeries, &seg, &localError)) {
            *outSeg = seg;
            return true;
        }
        if (firstError.isEmpty()) {
            firstError = localError;
        }
    }
    if (errorMessage) {
        *errorMessage = firstError.isEmpty() ? QStringLiteral("no SEG object found for current series") : firstError;
    }
    return false;
}
