#include "SegmentVolume.h"

#include <algorithm>
#include <array>
#include <deque>

namespace {
QVector<QImage> makeEmptyMaskVolume(int width, int height, int depth) {
    QVector<QImage> masks;
    masks.reserve(depth);
    for (int z = 0; z < depth; ++z) {
        QImage mask(width, height, QImage::Format_Grayscale8);
        mask.fill(0);
        masks.append(mask);
    }
    return masks;
}

QVector<QImage> thresholdSegmentation(const QVector<QImage>& slices, int threshold) {
    if (slices.isEmpty()) {
        return {};
    }
    QVector<QImage> masks = makeEmptyMaskVolume(slices.first().width(), slices.first().height(), static_cast<int>(slices.size()));
    for (qsizetype z = 0; z < slices.size(); ++z) {
        const QImage gray = slices[z].convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < gray.height(); ++y) {
            const uchar* src = gray.constScanLine(y);
            uchar* dst = masks[z].scanLine(y);
            for (int x = 0; x < gray.width(); ++x) {
                dst[x] = src[x] >= threshold ? 255 : 0;
            }
        }
    }
    return masks;
}

QVector<QImage> otsuSegmentation(const QVector<QImage>& slices) {
    if (slices.isEmpty()) {
        return {};
    }

    std::array<int, 256> histogram{};
    histogram.fill(0);
    int total = 0;
    for (const QImage& slice : slices) {
        const QImage gray = slice.convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < gray.height(); ++y) {
            const uchar* row = gray.constScanLine(y);
            for (int x = 0; x < gray.width(); ++x) {
                ++histogram[row[x]];
                ++total;
            }
        }
    }
    if (total == 0) {
        return {};
    }

    double weightedSum = 0.0;
    for (int i = 0; i < 256; ++i) {
        weightedSum += static_cast<double>(i * histogram[i]);
    }

    int threshold = 0;
    double backgroundWeight = 0.0;
    double backgroundSum = 0.0;
    double bestVariance = -1.0;
    for (int i = 0; i < 256; ++i) {
        backgroundWeight += histogram[i];
        if (backgroundWeight <= 0.0) {
            continue;
        }
        const double foregroundWeight = total - backgroundWeight;
        if (foregroundWeight <= 0.0) {
            break;
        }
        backgroundSum += static_cast<double>(i * histogram[i]);
        const double backgroundMean = backgroundSum / backgroundWeight;
        const double foregroundMean = (weightedSum - backgroundSum) / foregroundWeight;
        const double variance = backgroundWeight * foregroundWeight *
            (backgroundMean - foregroundMean) * (backgroundMean - foregroundMean);
        if (variance > bestVariance) {
            bestVariance = variance;
            threshold = i;
        }
    }

    return thresholdSegmentation(slices, threshold);
}

QVector<QImage> regionGrowSegmentation(
    const QVector<QImage>& slices,
    const QVector<SegmentationSeedPoint>& seeds,
    int tolerance
) {
    if (slices.isEmpty() || seeds.isEmpty()) {
        return {};
    }

    const int depth = static_cast<int>(slices.size());
    const int height = slices.first().height();
    const int width = slices.first().width();
    QVector<QImage> graySlices;
    graySlices.reserve(depth);
    for (const QImage& slice : slices) {
        graySlices.append(slice.convertToFormat(QImage::Format_Grayscale8));
    }

    QVector<int> fgValues;
    QVector<int> bgValues;
    for (const auto& seed : seeds) {
        if (seed.x < 0 || seed.x >= width || seed.y < 0 || seed.y >= height || seed.z < 0 || seed.z >= depth) {
            continue;
        }
        const int value = graySlices[seed.z].constScanLine(seed.y)[seed.x];
        if (seed.foreground) {
            fgValues.append(value);
        } else {
            bgValues.append(value);
        }
    }
    if (fgValues.isEmpty()) {
        return {};
    }

    double fgMean = 0.0;
    for (int v : fgValues) fgMean += v;
    fgMean /= fgValues.size();
    double bgMean = 0.0;
    if (!bgValues.isEmpty()) {
        for (int v : bgValues) bgMean += v;
        bgMean /= bgValues.size();
    }

    const int voxelCount = width * height * depth;
    QVector<uchar> visited(voxelCount, 0);
    QVector<uchar> maskData(voxelCount, 0);
    auto idxOf = [=](int x, int y, int z) { return z * width * height + y * width + x; };

    std::deque<std::array<int, 3>> q;
    for (const auto& seed : seeds) {
        if (!seed.foreground) continue;
        if (seed.x < 0 || seed.x >= width || seed.y < 0 || seed.y >= height || seed.z < 0 || seed.z >= depth) continue;
        q.push_back({seed.x, seed.y, seed.z});
        visited[idxOf(seed.x, seed.y, seed.z)] = 1;
    }

    const int dx[6] = {1, -1, 0, 0, 0, 0};
    const int dy[6] = {0, 0, 1, -1, 0, 0};
    const int dz[6] = {0, 0, 0, 0, 1, -1};

    while (!q.empty()) {
        const auto [x, y, z] = q.front();
        q.pop_front();
        const int val = graySlices[z].constScanLine(y)[x];
        const bool closeToFg = std::abs(val - fgMean) <= tolerance;
        const bool fartherFromBg = bgValues.isEmpty() || std::abs(val - fgMean) <= std::abs(val - bgMean);
        if (!closeToFg || !fartherFromBg) {
            continue;
        }

        maskData[idxOf(x, y, z)] = 255;
        for (int i = 0; i < 6; ++i) {
            const int nx = x + dx[i];
            const int ny = y + dy[i];
            const int nz = z + dz[i];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height || nz < 0 || nz >= depth) {
                continue;
            }
            const int nidx = idxOf(nx, ny, nz);
            if (visited[nidx]) continue;
            visited[nidx] = 1;
            q.push_back({nx, ny, nz});
        }
    }

    QVector<QImage> masks = makeEmptyMaskVolume(width, height, depth);
    for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < height; ++y) {
            uchar* dst = masks[z].scanLine(y);
            for (int x = 0; x < width; ++x) {
                dst[x] = maskData[idxOf(x, y, z)];
            }
        }
    }
    return masks;
}

QImage erode2D(const QImage& mask) {
    QImage out(mask.size(), QImage::Format_Grayscale8);
    out.fill(0);
    for (int y = 1; y < mask.height() - 1; ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 1; x < mask.width() - 1; ++x) {
            bool keep = true;
            for (int ky = -1; ky <= 1 && keep; ++ky) {
                const uchar* src = mask.constScanLine(y + ky);
                for (int kx = -1; kx <= 1; ++kx) {
                    if (src[x + kx] == 0) {
                        keep = false;
                        break;
                    }
                }
            }
            dst[x] = keep ? 255 : 0;
        }
    }
    return out;
}

QImage dilate2D(const QImage& mask) {
    QImage out(mask.size(), QImage::Format_Grayscale8);
    out.fill(0);
    for (int y = 1; y < mask.height() - 1; ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 1; x < mask.width() - 1; ++x) {
            bool on = false;
            for (int ky = -1; ky <= 1 && !on; ++ky) {
                const uchar* src = mask.constScanLine(y + ky);
                for (int kx = -1; kx <= 1; ++kx) {
                    if (src[x + kx] > 0) {
                        on = true;
                        break;
                    }
                }
            }
            dst[x] = on ? 255 : 0;
        }
    }
    return out;
}

void removeSmallComponents2D(QImage& mask, int minSize) {
    const int w = mask.width();
    const int h = mask.height();
    QVector<int> labels(w * h, 0);
    int currentLabel = 0;
    auto idx = [=](int x, int y) { return y * w + x; };
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (mask.constScanLine(y)[x] == 0 || labels[idx(x, y)] != 0) continue;
            ++currentLabel;
            QVector<QPoint> points;
            std::deque<QPoint> q;
            q.push_back(QPoint(x, y));
            labels[idx(x, y)] = currentLabel;
            while (!q.empty()) {
                QPoint p = q.front();
                q.pop_front();
                points.append(p);
                for (int i = 0; i < 4; ++i) {
                    const int nx = p.x() + dx[i];
                    const int ny = p.y() + dy[i];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    if (mask.constScanLine(ny)[nx] == 0) continue;
                    if (labels[idx(nx, ny)] != 0) continue;
                    labels[idx(nx, ny)] = currentLabel;
                    q.push_back(QPoint(nx, ny));
                }
            }
            if (points.size() < minSize) {
                for (const QPoint& p : points) {
                    mask.scanLine(p.y())[p.x()] = 0;
                }
            }
        }
    }
}

void fillHoles2D(QImage& mask) {
    const int w = mask.width();
    const int h = mask.height();
    QVector<uchar> visited(w * h, 0);
    auto idx = [=](int x, int y) { return y * w + x; };
    std::deque<QPoint> q;
    for (int x = 0; x < w; ++x) {
        if (mask.constScanLine(0)[x] == 0) { q.push_back(QPoint(x, 0)); visited[idx(x, 0)] = 1; }
        if (mask.constScanLine(h - 1)[x] == 0) { q.push_back(QPoint(x, h - 1)); visited[idx(x, h - 1)] = 1; }
    }
    for (int y = 0; y < h; ++y) {
        if (mask.constScanLine(y)[0] == 0) { q.push_back(QPoint(0, y)); visited[idx(0, y)] = 1; }
        if (mask.constScanLine(y)[w - 1] == 0) { q.push_back(QPoint(w - 1, y)); visited[idx(w - 1, y)] = 1; }
    }
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        QPoint p = q.front();
        q.pop_front();
        for (int i = 0; i < 4; ++i) {
            const int nx = p.x() + dx[i];
            const int ny = p.y() + dy[i];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (visited[idx(nx, ny)] || mask.constScanLine(ny)[nx] > 0) continue;
            visited[idx(nx, ny)] = 1;
            q.push_back(QPoint(nx, ny));
        }
    }
    for (int y = 0; y < h; ++y) {
        uchar* line = mask.scanLine(y);
        for (int x = 0; x < w; ++x) {
            if (line[x] == 0 && !visited[idx(x, y)]) {
                line[x] = 255;
            }
        }
    }
}

void postProcessMasks(QVector<QImage>& masks, const SegmentationParams& params) {
    for (QImage& mask : masks) {
        for (int i = 0; i < params.openIterations; ++i) {
            mask = dilate2D(erode2D(mask));
        }
        for (int i = 0; i < params.closeIterations; ++i) {
            mask = erode2D(dilate2D(mask));
        }
        removeSmallComponents2D(mask, params.minComponentSize);
        for (int i = 0; i < params.holeFillIterations; ++i) {
            fillHoles2D(mask);
        }
    }
}
}

QVector<QImage> SegmentVolume::run(
    const QVector<QImage>& slices,
    SegmentationMethod method,
    const QVector<SegmentationSeedPoint>& seeds,
    const SegmentationParams& params
) {
    QVector<QImage> masks;
    if (method == SegmentationMethod::Threshold) {
        masks = thresholdSegmentation(slices, params.threshold);
    } else if (method == SegmentationMethod::Otsu) {
        masks = otsuSegmentation(slices);
    } else {
        masks = regionGrowSegmentation(slices, seeds, params.tolerance);
    }
    if (!masks.isEmpty()) {
        postProcessMasks(masks, params);
    }
    return masks;
}
