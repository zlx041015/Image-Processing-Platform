#include "ImageLabProcessor.h"

#include "BMPReader.h"

#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {
int clampToByte(int value) {
    return std::clamp(value, 0, 255);
}
}

QImage ImageLabProcessor::loadGrayscaleImageFromFile(const QString& filePath, QString* errorMessage) {
    QFileInfo info(filePath);
    const QString suffix = info.suffix().toLower();

    if (suffix == QStringLiteral("bmp")) {
        BMPReader reader(filePath);
        if (!reader.load(errorMessage)) {
            return {};
        }
        QImage image = reader.decode(errorMessage);
        return image.convertToFormat(QImage::Format_Grayscale8);
    }

    QImage image(filePath);
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"无法加载图像文件");
        }
        return {};
    }
    return image.convertToFormat(QImage::Format_Grayscale8);
}

QImage ImageLabProcessor::addSaltPepperNoise(const QImage& img, double density) {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    QImage out = gray.copy();
    density = std::clamp(density, 0.0, 1.0);
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    for (int y = 0; y < out.height(); ++y) {
        uchar* line = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            if (dist(rng) < density) {
                line[x] = dist(rng) < 0.5 ? 0 : 255;
            }
        }
    }
    return out;
}

QImage ImageLabProcessor::addImpulseNoise(const QImage& img, double density) {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    QImage out = gray.copy();
    density = std::clamp(density, 0.0, 1.0);
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::uniform_int_distribution<int> impulse(0, 255);
    for (int y = 0; y < out.height(); ++y) {
        uchar* line = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            if (dist(rng) < density) {
                line[x] = static_cast<uchar>(impulse(rng));
            }
        }
    }
    return out;
}

QImage ImageLabProcessor::addGaussianNoise(const QImage& img, double mean, double sigma) {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    QImage out = gray.copy();
    sigma = std::max(0.0, sigma);
    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<double> dist(mean, sigma);
    for (int y = 0; y < out.height(); ++y) {
        uchar* line = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            line[x] = static_cast<uchar>(clampToByte(static_cast<int>(std::lround(line[x] + dist(rng)))));
        }
    }
    return out;
}

QImage ImageLabProcessor::meanFilter(const QImage& img, int kernelSize) {
    kernelSize = std::max(3, kernelSize | 1);
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }
    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int radius = kernelSize / 2;
    const int area = kernelSize * kernelSize;
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int sum = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    sum += qGray(gray.pixel(std::clamp(x + kx, 0, gray.width() - 1), std::clamp(y + ky, 0, gray.height() - 1)));
                }
            }
            dst[x] = static_cast<uchar>(clampToByte(sum / area));
        }
    }
    return out;
}

QImage ImageLabProcessor::medianFilter(const QImage& img, int kernelSize) {
    kernelSize = std::max(3, kernelSize | 1);
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }
    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int radius = kernelSize / 2;
    std::vector<int> window;
    window.reserve(kernelSize * kernelSize);
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            window.clear();
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    window.push_back(qGray(gray.pixel(std::clamp(x + kx, 0, gray.width() - 1), std::clamp(y + ky, 0, gray.height() - 1))));
                }
            }
            auto mid = window.begin() + static_cast<std::ptrdiff_t>(window.size() / 2);
            std::nth_element(window.begin(), mid, window.end());
            dst[x] = static_cast<uchar>(*mid);
        }
    }
    return out;
}

QImage ImageLabProcessor::maxFilter(const QImage& img, int kernelSize) {
    kernelSize = std::max(3, kernelSize | 1);
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }
    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int radius = kernelSize / 2;
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int best = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    best = std::max(best, qGray(gray.pixel(std::clamp(x + kx, 0, gray.width() - 1), std::clamp(y + ky, 0, gray.height() - 1))));
                }
            }
            dst[x] = static_cast<uchar>(best);
        }
    }
    return out;
}

QImage ImageLabProcessor::sobelEdgeDetect(const QImage& img, int kernelSize, int threshold) {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    const int size = kernelSize >= 5 ? 5 : 3;
    const QVector<int> deriv = (size == 5)
        ? QVector<int>{-1, -2, 0, 2, 1}
        : QVector<int>{-1, 0, 1};
    const QVector<int> smooth = (size == 5)
        ? QVector<int>{1, 4, 6, 4, 1}
        : QVector<int>{1, 2, 1};
    const double scale = (size == 5) ? 48.0 : 4.0;
    const int radius = size / 2;
    threshold = std::clamp(threshold, 0, 255);

    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return qGray(gray.pixel(x, y));
    };

    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            double gx = 0.0;
            double gy = 0.0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    const int value = sample(x + kx, y + ky);
                    const int sx = deriv[kx + radius] * smooth[ky + radius];
                    const int sy = smooth[kx + radius] * deriv[ky + radius];
                    gx += value * sx;
                    gy += value * sy;
                }
            }
            const double magnitude = std::sqrt(gx * gx + gy * gy) / scale;
            dst[x] = static_cast<uchar>(clampToByte(static_cast<int>(magnitude)) >= threshold ? 255 : 0);
        }
    }
    return out;
}

QImage ImageLabProcessor::prewittEdgeDetect(const QImage& img, int kernelSize, int threshold) {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    const int size = kernelSize >= 5 ? 5 : 3;
    const QVector<int> deriv = (size == 5)
        ? QVector<int>{-2, -1, 0, 1, 2}
        : QVector<int>{-1, 0, 1};
    const QVector<int> smooth = QVector<int>(size, 1);
    const double scale = (size == 5) ? 25.0 : 3.0;
    const int radius = size / 2;
    threshold = std::clamp(threshold, 0, 255);

    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return qGray(gray.pixel(x, y));
    };

    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            double gx = 0.0;
            double gy = 0.0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    const int value = sample(x + kx, y + ky);
                    const int sx = deriv[kx + radius] * smooth[ky + radius];
                    const int sy = smooth[kx + radius] * deriv[ky + radius];
                    gx += value * sx;
                    gy += value * sy;
                }
            }
            const double magnitude = std::sqrt(gx * gx + gy * gy) / scale;
            dst[x] = static_cast<uchar>(clampToByte(static_cast<int>(magnitude)) >= threshold ? 255 : 0);
        }
    }
    return out;
}

QImage ImageLabProcessor::laplacianEdgeDetect(const QImage& img, int kernelSize, int threshold) {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    threshold = std::clamp(threshold, 0, 255);
    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return qGray(gray.pixel(x, y));
    };

    if (kernelSize >= 5) {
        const int kernel[5][5] = {
            {0, 0, -1, 0, 0},
            {0, -1, -2, -1, 0},
            {-1, -2, 16, -2, -1},
            {0, -1, -2, -1, 0},
            {0, 0, -1, 0, 0}
        };
        const int radius = 2;
        const double scale = 16.0;
        for (int y = 0; y < gray.height(); ++y) {
            uchar* dst = out.scanLine(y);
            for (int x = 0; x < gray.width(); ++x) {
                double sum = 0.0;
                for (int ky = -radius; ky <= radius; ++ky) {
                    for (int kx = -radius; kx <= radius; ++kx) {
                        sum += sample(x + kx, y + ky) * kernel[ky + radius][kx + radius];
                    }
                }
                const int intensity = clampToByte(static_cast<int>(std::abs(sum) / scale));
                dst[x] = static_cast<uchar>(intensity >= threshold ? 255 : 0);
            }
        }
        return out;
    }

    const int kernel[3][3] = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}
    };
    const int radius = 1;
    const double scale = 4.0;
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            double sum = 0.0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    sum += sample(x + kx, y + ky) * kernel[ky + radius][kx + radius];
                }
            }
            const int intensity = clampToByte(static_cast<int>(std::abs(sum) / scale));
            dst[x] = static_cast<uchar>(intensity >= threshold ? 255 : 0);
        }
    }
    return out;
}

QImage ImageLabProcessor::step1_originalImage(const QImage& img) {
    return img.convertToFormat(QImage::Format_Grayscale8);
}

QImage ImageLabProcessor::step2_laplacianProcess(const QImage& img) {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    const int kernel[3][3] = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}
    };
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int sum = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    const int sx = std::clamp(x + kx, 0, gray.width() - 1);
                    const int sy = std::clamp(y + ky, 0, gray.height() - 1);
                    sum += qGray(gray.pixel(sx, sy)) * kernel[ky + 1][kx + 1];
                }
            }
            dst[x] = static_cast<uchar>(clampToByte(std::abs(sum)));
        }
    }
    return out;
}

QImage ImageLabProcessor::step3_sharpenImage(const QImage& original, const QImage& laplacian) {
    QImage src = original.convertToFormat(QImage::Format_Grayscale8);
    QImage lap = laplacian.convertToFormat(QImage::Format_Grayscale8);
    if (src.isNull() || lap.isNull() || src.size() != lap.size()) {
        return {};
    }

    QImage out(src.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < src.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* sLine = src.constScanLine(y);
        const uchar* lLine = lap.constScanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            dst[x] = static_cast<uchar>(clampToByte(sLine[x] + lLine[x]));
        }
    }
    return out;
}

QImage ImageLabProcessor::step4_sobelProcess(const QImage& img) {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int gx = 0;
            int gy = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    const int sx = std::clamp(x + kx, 0, gray.width() - 1);
                    const int sy = std::clamp(y + ky, 0, gray.height() - 1);
                    const int p = qGray(gray.pixel(sx, sy));
                    const int sxKernel[3][3] = {
                        {-1, 0, 1},
                        {-2, 0, 2},
                        {-1, 0, 1}
                    };
                    const int syKernel[3][3] = {
                        {1, 2, 1},
                        {0, 0, 0},
                        {-1, -2, -1}
                    };
                    gx += p * sxKernel[ky + 1][kx + 1];
                    gy += p * syKernel[ky + 1][kx + 1];
                }
            }
            const int magnitude = clampToByte(static_cast<int>(std::sqrt(static_cast<double>(gx * gx + gy * gy)) / 4.0));
            dst[x] = static_cast<uchar>(magnitude);
        }
    }
    return out;
}

QImage ImageLabProcessor::step5_meanFilterGradient(const QImage& sobel) {
    return meanFilter(sobel, 3);
}

QImage ImageLabProcessor::step6_maskImage(const QImage& sharpened, const QImage& gradient) {
    QImage a = sharpened.convertToFormat(QImage::Format_Grayscale8);
    QImage b = gradient.convertToFormat(QImage::Format_Grayscale8);
    if (a.isNull() || b.isNull() || a.size() != b.size()) {
        return {};
    }

    QImage out(a.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < a.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* aLine = a.constScanLine(y);
        const uchar* bLine = b.constScanLine(y);
        for (int x = 0; x < a.width(); ++x) {
            dst[x] = static_cast<uchar>((aLine[x] * bLine[x]) / 255);
        }
    }
    return out;
}

QImage ImageLabProcessor::step7_addOriginalAndMask(const QImage& original, const QImage& mask) {
    QImage a = original.convertToFormat(QImage::Format_Grayscale8);
    QImage b = mask.convertToFormat(QImage::Format_Grayscale8);
    if (a.isNull() || b.isNull() || a.size() != b.size()) {
        return {};
    }

    QImage out(a.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < a.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* aLine = a.constScanLine(y);
        const uchar* bLine = b.constScanLine(y);
        for (int x = 0; x < a.width(); ++x) {
            dst[x] = static_cast<uchar>(clampToByte(aLine[x] + bLine[x]));
        }
    }
    return out;
}

QImage ImageLabProcessor::step8_gammaTransform(const QImage& enhanced, double gamma) {
    QImage gray = enhanced.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    gamma = std::max(0.1, gamma);
    QImage out(gray.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            const double normalized = line[x] / 255.0;
            const int value = static_cast<int>(std::pow(normalized, gamma) * 255.0 + 0.5);
            dst[x] = static_cast<uchar>(clampToByte(value));
        }
    }
    return out;
}
