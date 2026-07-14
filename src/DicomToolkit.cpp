#include "DicomToolkit.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QPolygonF>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
struct ParsedDicomSlice {
    QImage image;
    QString fileName;
    QString sopInstanceUid;
    QString seriesInstanceUid;
    QString modality;
    double slicePosition = 0.0;
    int instanceNumber = 0;
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    double pixelSpacingX = 1.0;
    double pixelSpacingY = 1.0;
    double sliceThickness = 1.0;
    QVector3D origin = QVector3D(0, 0, 0);
    QVector3D rowDirection = QVector3D(1, 0, 0);
    QVector3D columnDirection = QVector3D(0, 1, 0);
    QVector3D sliceDirection = QVector3D(0, 0, 1);
    double windowCenter = 40.0;
    double windowWidth = 400.0;
};

int readUint16(const QByteArray& data, qsizetype offset, bool littleEndian) {
    const uchar* p = reinterpret_cast<const uchar*>(data.constData() + offset);
    return littleEndian ? qFromLittleEndian<quint16>(p) : qFromBigEndian<quint16>(p);
}

quint32 readUint32(const QByteArray& data, qsizetype offset, bool littleEndian) {
    const uchar* p = reinterpret_cast<const uchar*>(data.constData() + offset);
    return littleEndian ? qFromLittleEndian<quint32>(p) : qFromBigEndian<quint32>(p);
}

double parseDoubleValue(const QByteArray& value, double fallback = 0.0) {
    bool ok = false;
    const double parsed = QString::fromLatin1(value).trimmed().split('\\').value(0).toDouble(&ok);
    return ok ? parsed : fallback;
}

int parseIntValue(const QByteArray& value, int fallback = 0) {
    bool ok = false;
    const int parsed = QString::fromLatin1(value).trimmed().toInt(&ok);
    return ok ? parsed : fallback;
}

double parseSlicePosition(const QByteArray& value) {
    const QString text = QString::fromLatin1(value).trimmed();
    const QStringList parts = text.split('\\', Qt::SkipEmptyParts);
    if (parts.size() >= 3) {
        bool ok = false;
        const double z = parts[2].toDouble(&ok);
        if (ok) return z;
    }
    return text.toDouble();
}

QVector3D parseVector3(const QByteArray& value, const QVector3D& fallback = QVector3D(0, 0, 0)) {
    const QStringList parts = QString::fromLatin1(value).trimmed().split('\\', Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        return fallback;
    }
    bool okX = false, okY = false, okZ = false;
    const float x = parts[0].toFloat(&okX);
    const float y = parts[1].toFloat(&okY);
    const float z = parts[2].toFloat(&okZ);
    return (okX && okY && okZ) ? QVector3D(x, y, z) : fallback;
}

QImage normalizeSignedTo8(const QVector<double>& values, int width, int height) {
    if (values.isEmpty() || width <= 0 || height <= 0) {
        return {};
    }
    double minV = std::numeric_limits<double>::max();
    double maxV = std::numeric_limits<double>::lowest();
    for (double v : values) {
        minV = std::min(minV, v);
        maxV = std::max(maxV, v);
    }
    const double range = std::max(1.0, maxV - minV);
    QImage img(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        uchar* line = img.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const double v = values[y * width + x];
            line[x] = static_cast<uchar>(std::clamp(static_cast<int>((v - minV) * 255.0 / range), 0, 255));
        }
    }
    return img;
}

bool parseDicomSlice(const QString& filePath, ParsedDicomSlice* outSlice, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot open file");
        return false;
    }

    const QByteArray data = file.readAll();
    if (data.size() < 8) {
        if (errorMessage) *errorMessage = QStringLiteral("file is too small to be a readable DICOM object");
        return false;
    }

    const bool hasPreamble = data.size() >= 132 && data.mid(128, 4) == "DICM";

    bool datasetLittleEndian = true;
    bool datasetExplicitVR = true;
    qsizetype pos = hasPreamble ? 132 : 0;
    int rows = 0, cols = 0, bitsAllocated = 16, pixelRepresentation = 0;
    double rescaleSlope = 1.0;
    double rescaleIntercept = 0.0;
    QString seriesInstanceUid;
    QString sopInstanceUid;
    QString modality;
    int instanceNumber = 0;
    double slicePosition = 0.0;
    double pixelSpacingX = 1.0;
    double pixelSpacingY = 1.0;
    double sliceThickness = 1.0;
    QVector3D origin(0, 0, 0);
    QVector3D rowDirection(1, 0, 0);
    QVector3D columnDirection(0, 1, 0);
    double windowCenter = 40.0;
    double windowWidth = 400.0;
    QVector<double> pixels;

    auto need = [&](qsizetype n) { return pos + n <= data.size(); };

    while (need(8)) {
        const quint16 group = readUint16(data, pos, true);
        const quint16 element = readUint16(data, pos + 2, true);
        pos += 4;

        const bool metaHeader = group == 0x0002;
        const bool littleEndian = metaHeader ? true : datasetLittleEndian;
        const bool explicitVR = metaHeader ? true : datasetExplicitVR;

        QString vr;
        quint32 len = 0;
        if (explicitVR) {
            vr = QString::fromLatin1(data.constData() + pos, 2);
            pos += 2;
            if (vr == "OB" || vr == "OW" || vr == "OF" || vr == "SQ" || vr == "UT" || vr == "UN") {
                pos += 2;
                if (!need(4)) break;
                len = readUint32(data, pos, littleEndian);
                pos += 4;
            } else {
                if (!need(2)) break;
                len = readUint16(data, pos, littleEndian);
                pos += 2;
            }
        } else {
            if (!need(4)) break;
            len = readUint32(data, pos, littleEndian);
            pos += 4;
        }

        if (len == 0xFFFFFFFF || !need(len)) {
            if (errorMessage) *errorMessage = QStringLiteral("unsupported undefined-length tag");
            return false;
        }

        const QByteArray value = data.mid(pos, len);
        pos += len;

        if (group == 0x0002 && element == 0x0010) {
            const QByteArray ts = value.trimmed();
            if (ts.contains("1.2.840.10008.1.2.2")) {
                datasetLittleEndian = false;
                datasetExplicitVR = true;
            } else if (ts.contains("1.2.840.10008.1.2.1")) {
                datasetLittleEndian = true;
                datasetExplicitVR = true;
            } else if (ts.contains("1.2.840.10008.1.2")) {
                datasetLittleEndian = true;
                datasetExplicitVR = false;
            }
            continue;
        }

        if (group == 0x0008 && element == 0x0018) {
            sopInstanceUid = QString::fromLatin1(value).trimmed();
        } else if (group == 0x0008 && element == 0x0060) {
            modality = QString::fromLatin1(value).trimmed();
        } else if (group == 0x0020 && element == 0x000E) {
            seriesInstanceUid = QString::fromLatin1(value).trimmed();
        } else if (group == 0x0020 && element == 0x0013) {
            instanceNumber = parseIntValue(value, instanceNumber);
        } else if (group == 0x0020 && element == 0x0032) {
            slicePosition = parseSlicePosition(value);
        } else if (group == 0x0020 && element == 0x1041) {
            slicePosition = parseDoubleValue(value, slicePosition);
        } else if (group == 0x0028 && element == 0x0010) {
            rows = readUint16(value, 0, littleEndian);
        } else if (group == 0x0028 && element == 0x0011) {
            cols = readUint16(value, 0, littleEndian);
        } else if (group == 0x0028 && element == 0x0030) {
            const QStringList parts = QString::fromLatin1(value).trimmed().split('\\', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                pixelSpacingY = parts[0].toDouble();
                pixelSpacingX = parts[1].toDouble();
            }
        } else if (group == 0x0028 && element == 0x0100) {
            bitsAllocated = readUint16(value, 0, littleEndian);
        } else if (group == 0x0028 && element == 0x0103) {
            pixelRepresentation = readUint16(value, 0, littleEndian);
        } else if (group == 0x0018 && element == 0x0050) {
            sliceThickness = std::max(0.1, parseDoubleValue(value, sliceThickness));
        } else if (group == 0x0028 && element == 0x1050) {
            windowCenter = parseDoubleValue(value, windowCenter);
        } else if (group == 0x0028 && element == 0x1051) {
            windowWidth = std::max(1.0, parseDoubleValue(value, windowWidth));
        } else if (group == 0x0028 && element == 0x1052) {
            rescaleIntercept = parseDoubleValue(value, rescaleIntercept);
        } else if (group == 0x0028 && element == 0x1053) {
            const double slope = parseDoubleValue(value, rescaleSlope);
            rescaleSlope = std::abs(slope) < 1e-12 ? 1.0 : slope;
        } else if (group == 0x0020 && element == 0x0037) {
            const QStringList parts = QString::fromLatin1(value).trimmed().split('\\', Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                rowDirection = QVector3D(parts[0].toFloat(), parts[1].toFloat(), parts[2].toFloat());
                columnDirection = QVector3D(parts[3].toFloat(), parts[4].toFloat(), parts[5].toFloat());
            }
        } else if (group == 0x0020 && element == 0x0032) {
            origin = parseVector3(value, origin);
            slicePosition = parseSlicePosition(value);
        } else if (group == 0x7FE0 && element == 0x0010) {
            if (rows <= 0 || cols <= 0) {
                if (errorMessage) *errorMessage = QStringLiteral("missing rows or columns before pixel data");
                return false;
            }
            if (bitsAllocated != 16 && bitsAllocated != 8) {
                if (errorMessage) *errorMessage = QStringLiteral("unsupported bits allocated");
                return false;
            }
            const int pixelCount = rows * cols;
            pixels.resize(pixelCount);
            if (bitsAllocated == 8) {
                for (int i = 0; i < pixelCount && i < value.size(); ++i) {
                    pixels[i] = static_cast<unsigned char>(value[i]);
                }
            } else {
                for (int i = 0; i < pixelCount; ++i) {
                    const qsizetype offset = i * 2;
                    if (offset + 1 >= value.size()) break;
                    const int raw = readUint16(value, offset, littleEndian);
                    const int signedRaw = pixelRepresentation == 0 ? raw : static_cast<qint16>(raw);
                    pixels[i] = signedRaw * rescaleSlope + rescaleIntercept;
                }
            }
            break;
        }
    }

    if (pixels.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("unsupported dicom pixel encoding or missing pixel data");
        return false;
    }

    outSlice->image = normalizeSignedTo8(pixels, cols, rows);
    outSlice->fileName = QFileInfo(filePath).fileName();
    outSlice->sopInstanceUid = sopInstanceUid;
    outSlice->seriesInstanceUid = seriesInstanceUid;
    outSlice->modality = modality;
    outSlice->slicePosition = slicePosition;
    outSlice->instanceNumber = instanceNumber;
    outSlice->width = cols;
    outSlice->height = rows;
    outSlice->bitDepth = bitsAllocated;
    outSlice->pixelSpacingX = pixelSpacingX;
    outSlice->pixelSpacingY = pixelSpacingY;
    outSlice->sliceThickness = sliceThickness;
    outSlice->origin = origin;
    outSlice->rowDirection = rowDirection.normalized();
    outSlice->columnDirection = columnDirection.normalized();
    outSlice->sliceDirection = QVector3D::crossProduct(outSlice->rowDirection, outSlice->columnDirection).normalized();
    outSlice->windowCenter = windowCenter;
    outSlice->windowWidth = windowWidth;
    return true;
}

bool sortSlices(const ParsedDicomSlice& a, const ParsedDicomSlice& b) {
    if (!qFuzzyIsNull(QVector3D::dotProduct(a.origin, a.sliceDirection) - QVector3D::dotProduct(b.origin, b.sliceDirection))) {
        return QVector3D::dotProduct(a.origin, a.sliceDirection) < QVector3D::dotProduct(b.origin, b.sliceDirection);
    }
    if (std::abs(a.slicePosition - b.slicePosition) > 1e-6) {
        return a.slicePosition < b.slicePosition;
    }
    if (a.instanceNumber != b.instanceNumber) {
        return a.instanceNumber < b.instanceNumber;
    }
    return a.fileName < b.fileName;
}
}

bool DicomToolkit::loadDicomFile(const QString& filePath, DicomSeriesData* outSeries, QString* errorMessage) {
    ParsedDicomSlice slice;
    if (!parseDicomSlice(filePath, &slice, errorMessage)) {
        return false;
    }
    outSeries->rawSlices = {slice.image};
    outSeries->sliceNames = {slice.fileName};
    outSeries->sopInstanceUids = {slice.sopInstanceUid};
    outSeries->slicePositions = {slice.slicePosition};
    outSeries->seriesInstanceUid = slice.seriesInstanceUid;
    outSeries->modality = slice.modality;
    outSeries->width = slice.width;
    outSeries->height = slice.height;
    outSeries->bitDepth = slice.bitDepth;
    outSeries->pixelSpacingX = slice.pixelSpacingX;
    outSeries->pixelSpacingY = slice.pixelSpacingY;
    outSeries->sliceThickness = slice.sliceThickness;
    outSeries->origin = slice.origin;
    outSeries->rowDirection = slice.rowDirection;
    outSeries->columnDirection = slice.columnDirection;
    outSeries->sliceDirection = slice.sliceDirection;
    outSeries->defaultWindowCenter = slice.windowCenter;
    outSeries->defaultWindowWidth = slice.windowWidth;
    return true;
}

bool DicomToolkit::loadDicomDirectory(
    const QString& directoryPath,
    DicomSeriesData* outSeries,
    QString* errorMessage,
    const QString& preferredSeriesUid
) {
    QVector<ParsedDicomSlice> allSlices;
    QDirIterator it(directoryPath, {"*.dcm", "*.dicom", "*.ima", "*.DCM", "*.DICOM", "*.IMA"},
        QDir::Files, QDirIterator::Subdirectories);

    QString firstError;
    while (it.hasNext()) {
        const QString path = it.next();
        ParsedDicomSlice slice;
        QString localError;
        if (parseDicomSlice(path, &slice, &localError)) {
            if (slice.modality == QStringLiteral("SEG")) {
                continue;
            }
            allSlices.append(slice);
        } else if (firstError.isEmpty()) {
            firstError = QFileInfo(path).fileName() + QStringLiteral(": ") + localError;
        }
    }

    if (allSlices.isEmpty()) {
        if (errorMessage) *errorMessage = firstError.isEmpty() ? QStringLiteral("no readable dicom slices found") : firstError;
        return false;
    }

    QString seriesUid = preferredSeriesUid;
    if (seriesUid.isEmpty()) {
        QHash<QString, int> counts;
        for (const auto& slice : allSlices) {
            counts[slice.seriesInstanceUid] += 1;
        }
        int best = -1;
        for (auto itCount = counts.constBegin(); itCount != counts.constEnd(); ++itCount) {
            if (itCount.value() > best) {
                best = itCount.value();
                seriesUid = itCount.key();
            }
        }
    }

    QVector<ParsedDicomSlice> selected;
    for (const auto& slice : allSlices) {
        if (seriesUid.isEmpty() || slice.seriesInstanceUid == seriesUid) {
            selected.append(slice);
        }
    }
    if (selected.isEmpty()) {
        selected = allSlices;
    }
    std::sort(selected.begin(), selected.end(), sortSlices);

    outSeries->rawSlices.clear();
    outSeries->sliceNames.clear();
    outSeries->sopInstanceUids.clear();
    outSeries->slicePositions.clear();
    outSeries->seriesInstanceUid = selected.first().seriesInstanceUid;
    outSeries->modality = selected.first().modality;
    outSeries->width = selected.first().width;
    outSeries->height = selected.first().height;
    outSeries->bitDepth = selected.first().bitDepth;
    outSeries->pixelSpacingX = selected.first().pixelSpacingX;
    outSeries->pixelSpacingY = selected.first().pixelSpacingY;
    outSeries->sliceThickness = selected.first().sliceThickness;
    outSeries->origin = selected.first().origin;
    outSeries->rowDirection = selected.first().rowDirection;
    outSeries->columnDirection = selected.first().columnDirection;
    outSeries->sliceDirection = selected.first().sliceDirection;
    outSeries->defaultWindowCenter = selected.first().windowCenter;
    outSeries->defaultWindowWidth = selected.first().windowWidth;
    for (const auto& slice : selected) {
        if (slice.width != outSeries->width || slice.height != outSeries->height) {
            continue;
        }
        outSeries->rawSlices.append(slice.image);
        outSeries->sliceNames.append(slice.fileName);
        outSeries->sopInstanceUids.append(slice.sopInstanceUid);
        outSeries->slicePositions.append(slice.slicePosition);
    }
    if (outSeries->rawSlices.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("no consistent slice stack found");
        return false;
    }
    return true;
}

QImage DicomToolkit::applyWindowLevel(const QImage& image, double windowCenter, double windowWidth) {
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }
    const double low = windowCenter - windowWidth / 2.0;
    const double high = windowCenter + windowWidth / 2.0;
    const double range = std::max(1.0, high - low);
    QImage out(gray.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* src = gray.constScanLine(y);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            const double v = src[x];
            dst[x] = static_cast<uchar>(std::clamp(static_cast<int>((v - low) * 255.0 / range), 0, 255));
        }
    }
    return out;
}

QImage DicomToolkit::buildCoronalSlice(const QVector<QImage>& slices, int rowIndex) {
    if (slices.isEmpty()) {
        return {};
    }
    const int depth = static_cast<int>(slices.size());
    const int width = slices.first().width();
    const int height = slices.first().height();
    const int row = std::clamp(rowIndex, 0, height - 1);
    QImage out(width, depth, QImage::Format_Grayscale8);
    for (int z = 0; z < depth; ++z) {
        const QImage slice = slices[z].convertToFormat(QImage::Format_Grayscale8);
        const uchar* src = slice.constScanLine(row);
        uchar* dst = out.scanLine(depth - 1 - z);
        std::memcpy(dst, src, width);
    }
    return out;
}

QImage DicomToolkit::buildSagittalSlice(const QVector<QImage>& slices, int columnIndex) {
    if (slices.isEmpty()) {
        return {};
    }
    const int depth = static_cast<int>(slices.size());
    const int width = slices.first().width();
    const int height = slices.first().height();
    const int col = std::clamp(columnIndex, 0, width - 1);
    QImage out(depth, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        uchar* dst = out.scanLine(y);
        for (int z = 0; z < depth; ++z) {
            const QImage slice = slices[z].convertToFormat(QImage::Format_Grayscale8);
            dst[depth - 1 - z] = slice.constScanLine(y)[col];
        }
    }
    return out;
}

QImage DicomToolkit::renderVolumePreview(const QVector<QImage>& slices, int axialSlice, int coronalSlice, int sagittalSlice) {
    if (slices.isEmpty()) {
        return {};
    }
    const int depth = static_cast<int>(slices.size());
    const int height = slices.first().height();
    const int width = slices.first().width();
    const int axial = std::clamp(axialSlice, 0, depth - 1);
    const int coronal = std::clamp(coronalSlice, 0, height - 1);
    const int sagittal = std::clamp(sagittalSlice, 0, width - 1);

    const int canvasW = 460;
    const int canvasH = 360;
    const QPointF center(canvasW * 0.56, canvasH * 0.72);
    QImage canvas(canvasW, canvasH, QImage::Format_RGB32);
    canvas.fill(QColor("#0b111a"));

    auto project = [&](double x, double y, double z) {
        const double nx = x - width / 2.0;
        const double ny = y - height / 2.0;
        const double nz = z - depth / 2.0;
        return QPointF(center.x() + (nx - ny) * 0.75, center.y() + (nx + ny) * 0.32 - nz * 1.1);
    };

    QPainter painter(&canvas);
    const int stepX = std::max(1, width / 96);
    const int stepY = std::max(1, height / 96);
    const int stepZ = std::max(1, depth / 72);
    for (int z = 0; z < depth; z += stepZ) {
        const QImage slice = slices[z].convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < height; y += stepY) {
            const uchar* row = slice.constScanLine(y);
            for (int x = 0; x < width; x += stepX) {
                const int g = row[x];
                if (g < 28) continue;
                const QPointF p = project(x, y, z);
                painter.fillRect(QRectF(p.x(), p.y(), 2, 2), QColor(g, g, g, 160));
            }
        }
    }

    painter.setPen(QPen(QColor("#ef4444"), 2));
    painter.drawPolygon(QPolygonF({
        project(0, 0, axial),
        project(width - 1, 0, axial),
        project(width - 1, height - 1, axial),
        project(0, height - 1, axial)
    }));
    painter.setPen(QPen(QColor("#22c55e"), 2));
    painter.drawPolygon(QPolygonF({
        project(0, coronal, 0),
        project(width - 1, coronal, 0),
        project(width - 1, coronal, depth - 1),
        project(0, coronal, depth - 1)
    }));
    painter.setPen(QPen(QColor("#eab308"), 2));
    painter.drawPolygon(QPolygonF({
        project(sagittal, 0, 0),
        project(sagittal, height - 1, 0),
        project(sagittal, height - 1, depth - 1),
        project(sagittal, 0, depth - 1)
    }));
    painter.end();
    return canvas;
}
