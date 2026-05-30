#include "ImageRestoration.h"

#include <QVector>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace {
constexpr double kPi = 3.14159265358979323846;
using Complex = std::complex<double>;

struct SpectrumData {
    int width = 0;
    int height = 0;
    int originalWidth = 0;
    int originalHeight = 0;
    QVector<Complex> values;

    bool isValid() const {
        return width > 0 && height > 0 && values.size() == width * height;
    }
};

int nextPowerOfTwo(int value) {
    int result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

int clampToByte(double value) {
    return std::clamp(static_cast<int>(std::lround(value)), 0, 255);
}

int percentileFromHistogram(const QVector<int>& hist, int total, double percentile) {
    const int target = std::clamp(static_cast<int>(std::round(total * percentile)), 0, std::max(0, total - 1));
    int cumulative = 0;
    for (int i = 0; i < hist.size(); ++i) {
        cumulative += hist[i];
        if (cumulative > target) {
            return i;
        }
    }
    return hist.size() - 1;
}

void fft1d(QVector<Complex>& line, bool inverse) {
    const int size = line.size();
    for (int i = 1, j = 0; i < size; ++i) {
        int bit = size >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(line[i], line[j]);
        }
    }

    for (int len = 2; len <= size; len <<= 1) {
        const double angle = 2.0 * kPi / len * (inverse ? 1.0 : -1.0);
        const Complex wLen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < size; i += len) {
            Complex w(1.0, 0.0);
            for (int j = 0; j < len / 2; ++j) {
                const Complex u = line[i + j];
                const Complex v = line[i + j + len / 2] * w;
                line[i + j] = u + v;
                line[i + j + len / 2] = u - v;
                w *= wLen;
            }
        }
    }

    if (inverse) {
        for (Complex& value : line) {
            value /= static_cast<double>(size);
        }
    }
}

void fft2d(QVector<Complex>& data, int width, int height, bool inverse) {
    QVector<Complex> line;
    line.reserve(std::max(width, height));

    for (int y = 0; y < height; ++y) {
        line.resize(width);
        for (int x = 0; x < width; ++x) {
            line[x] = data[y * width + x];
        }
        fft1d(line, inverse);
        for (int x = 0; x < width; ++x) {
            data[y * width + x] = line[x];
        }
    }

    for (int x = 0; x < width; ++x) {
        line.resize(height);
        for (int y = 0; y < height; ++y) {
            line[y] = data[y * width + x];
        }
        fft1d(line, inverse);
        for (int y = 0; y < height; ++y) {
            data[y * width + x] = line[y];
        }
    }
}

SpectrumData fourierTransform(const QImage& image) {
    SpectrumData spectrum;
    if (image.isNull()) {
        return spectrum;
    }

    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    spectrum.originalWidth = gray.width();
    spectrum.originalHeight = gray.height();
    spectrum.width = nextPowerOfTwo(gray.width());
    spectrum.height = nextPowerOfTwo(gray.height());
    spectrum.values.resize(spectrum.width * spectrum.height);

    for (int y = 0; y < spectrum.height; ++y) {
        for (int x = 0; x < spectrum.width; ++x) {
            double value = 0.0;
            if (x < gray.width() && y < gray.height()) {
                value = qGray(gray.pixel(x, y));
            }
            if ((x + y) & 1) {
                value = -value;
            }
            spectrum.values[y * spectrum.width + x] = Complex(value, 0.0);
        }
    }

    fft2d(spectrum.values, spectrum.width, spectrum.height, false);
    return spectrum;
}

QImage inverseToImage(const SpectrumData& spectrum, bool normalize) {
    if (!spectrum.isValid()) {
        return {};
    }

    QVector<Complex> spatial = spectrum.values;
    fft2d(spatial, spectrum.width, spectrum.height, true);

    QImage out(spectrum.originalWidth, spectrum.originalHeight, QImage::Format_Grayscale8);
    QVector<double> values(out.width() * out.height());
    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::lowest();

    for (int y = 0; y < out.height(); ++y) {
        for (int x = 0; x < out.width(); ++x) {
            double value = spatial[y * spectrum.width + x].real();
            if ((x + y) & 1) {
                value = -value;
            }
            values[y * out.width() + x] = value;
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }
    }

    const double range = std::max(1.0, maxValue - minValue);
    for (int y = 0; y < out.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            double value = values[y * out.width() + x];
            if (normalize) {
                value = (value - minValue) / range * 255.0;
            }
            dst[x] = static_cast<uchar>(clampToByte(value));
        }
    }
    return out;
}

QImage robustContrastStretch(const QImage& image, double lowPercentile = 0.01, double highPercentile = 0.995) {
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    QVector<int> hist(256, 0);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            ++hist[line[x]];
        }
    }

    const int total = gray.width() * gray.height();
    int low = percentileFromHistogram(hist, total, lowPercentile);
    int high = percentileFromHistogram(hist, total, highPercentile);
    if (high <= low) {
        low = 0;
        high = 255;
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    const double scale = 255.0 / std::max(1, high - low);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* src = gray.constScanLine(y);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            dst[x] = static_cast<uchar>(clampToByte((src[x] - low) * scale));
        }
    }
    return out;
}

QImage unsharpMask(const QImage& image, double amount) {
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    const int kernel[3][3] = {
        {1, 2, 1},
        {2, 4, 2},
        {1, 2, 1}
    };
    QImage out(gray.size(), QImage::Format_Grayscale8);
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return static_cast<int>(gray.constScanLine(y)[x]);
    };

    for (int y = 0; y < gray.height(); ++y) {
        const uchar* src = gray.constScanLine(y);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int blur = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    blur += sample(x + kx, y + ky) * kernel[ky + 1][kx + 1];
                }
            }
            blur /= 16;
            const double value = src[x] + amount * (src[x] - blur);
            dst[x] = static_cast<uchar>(clampToByte(value));
        }
    }
    return out;
}

QImage localContrastEnhance(const QImage& image, double amount) {
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int radius = 5;
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return static_cast<int>(gray.constScanLine(y)[x]);
    };

    for (int y = 0; y < gray.height(); ++y) {
        const uchar* src = gray.constScanLine(y);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int sum = 0;
            int count = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    sum += sample(x + kx, y + ky);
                    ++count;
                }
            }
            const double localMean = static_cast<double>(sum) / count;
            const double value = localMean + (src[x] - localMean) * (1.0 + amount);
            dst[x] = static_cast<uchar>(clampToByte(value));
        }
    }
    return out;
}

QImage edgeRecover(const QImage& image, double amount) {
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return static_cast<int>(gray.constScanLine(y)[x]);
    };

    for (int y = 0; y < gray.height(); ++y) {
        const uchar* src = gray.constScanLine(y);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            const int lap = 4 * sample(x, y) - sample(x - 1, y) - sample(x + 1, y) - sample(x, y - 1) - sample(x, y + 1);
            dst[x] = static_cast<uchar>(clampToByte(src[x] + amount * lap));
        }
    }
    return out;
}

QImage suppressBorderArtifacts(const QImage& image, int margin = 8) {
    QImage out = image.convertToFormat(QImage::Format_Grayscale8);
    if (out.isNull() || out.width() <= margin * 2 || out.height() <= margin * 2) {
        return out;
    }

    const int w = out.width();
    const int h = out.height();
    for (int y = 0; y < h; ++y) {
        uchar* line = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const int dist = std::min({x, y, w - 1 - x, h - 1 - y});
            if (dist >= margin) {
                continue;
            }
            const int sx = std::clamp(x, margin, w - margin - 1);
            const int sy = std::clamp(y, margin, h - margin - 1);
            const double t = static_cast<double>(dist) / margin;
            const int inner = out.constScanLine(sy)[sx];
            line[x] = static_cast<uchar>(clampToByte(t * line[x] + (1.0 - t) * inner));
        }
    }
    return out;
}

QImage blendImages(const QImage& a, const QImage& b, double aWeight) {
    QImage ga = a.convertToFormat(QImage::Format_Grayscale8);
    QImage gb = b.convertToFormat(QImage::Format_Grayscale8);
    if (ga.isNull()) {
        return gb;
    }
    if (gb.isNull()) {
        return ga;
    }

    const int width = std::min(ga.width(), gb.width());
    const int height = std::min(ga.height(), gb.height());
    QImage out(width, height, QImage::Format_Grayscale8);
    aWeight = std::clamp(aWeight, 0.0, 1.0);
    for (int y = 0; y < height; ++y) {
        const uchar* la = ga.constScanLine(y);
        const uchar* lb = gb.constScanLine(y);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            dst[x] = static_cast<uchar>(clampToByte(aWeight * la[x] + (1.0 - aWeight) * lb[x]));
        }
    }
    return out;
}

QImage detailEnhance(const QImage& image, RestorationModel model, bool wiener) {
    QImage out = robustContrastStretch(image, 0.005, 0.995);
    if (model == RestorationModel::AtmosphericTurbulence) {
        out = localContrastEnhance(out, wiener ? 1.15 : 0.8);
        out = unsharpMask(out, wiener ? 2.1 : 1.35);
        out = edgeRecover(out, wiener ? 0.32 : 0.2);
    } else {
        out = localContrastEnhance(out, wiener ? 0.45 : 0.35);
        out = unsharpMask(out, wiener ? 0.85 : 0.65);
        out = edgeRecover(out, wiener ? 0.12 : 0.08);
    }
    return robustContrastStretch(out, 0.005, 0.995);
}

QImage finalizeRestoration(const QImage& restored, const QImage& degraded, RestorationModel model, bool wiener) {
    if (model == RestorationModel::AtmosphericTurbulence) {
        QImage restoredDetail = robustContrastStretch(restored, 0.01, 0.995);
        restoredDetail = unsharpMask(restoredDetail, wiener ? 0.55 : 0.35);
        return suppressBorderArtifacts(robustContrastStretch(restoredDetail, 0.01, 0.995), 10);
    }
    QImage restoredDetail = detailEnhance(restored, model, wiener);
    if (model == RestorationModel::MotionBlur) {
        restoredDetail = suppressBorderArtifacts(restoredDetail, 12);
        restoredDetail = unsharpMask(restoredDetail, wiener ? 0.45 : 0.25);
        return robustContrastStretch(restoredDetail, 0.005, 0.995);
    }
    QImage degradedDetail = detailEnhance(degraded, model, true);
    return robustContrastStretch(blendImages(restoredDetail, degradedDetail, wiener ? 0.45 : 0.55), 0.005, 0.995);
}

Complex atmosphericTransfer(int x, int y, int width, int height, double k) {
    const double u = x - width / 2.0;
    const double v = y - height / 2.0;
    const double d2 = u * u + v * v;
    const double gain = std::exp(-std::max(0.0, k) * std::pow(d2, 5.0 / 6.0));
    return Complex(gain, 0.0);
}

Complex motionTransfer(int x, int y, int width, int height, double a, double b, double t) {
    const double u = x - width / 2.0;
    const double v = y - height / 2.0;
    const double value = kPi * (u * a + v * b);

    double magnitude = 1.0;
    if (std::abs(value) > 1e-8) {
        magnitude = t * std::sin(value) / value;
    } else {
        magnitude = t;
    }
    return magnitude * Complex(std::cos(-value), std::sin(-value));
}

Complex transferAt(RestorationModel model, const RestorationParams& params, int x, int y, int width, int height) {
    if (model == RestorationModel::AtmosphericTurbulence) {
        return atmosphericTransfer(x, y, width, height, params.turbulenceK);
    }
    return motionTransfer(x, y, width, height, params.motionA, params.motionB, params.motionT);
}

QImage degradeImage(const QImage& image, RestorationModel model, const RestorationParams& params) {
    SpectrumData spectrum = fourierTransform(image);
    if (!spectrum.isValid()) {
        return {};
    }

    for (int y = 0; y < spectrum.height; ++y) {
        for (int x = 0; x < spectrum.width; ++x) {
            spectrum.values[y * spectrum.width + x] *= transferAt(model, params, x, y, spectrum.width, spectrum.height);
        }
    }
    return inverseToImage(spectrum, false);
}

QImage restoreImage(const QImage& degraded, RestorationModel model, const RestorationParams& params, bool cutoffEnabled, bool wiener) {
    SpectrumData spectrum = fourierTransform(degraded);
    if (!spectrum.isValid()) {
        return {};
    }

    const double cutoff = std::max(1.0, params.inverseCutoff);
    const double k = std::max(0.0, params.wienerK);
    const double centerX = spectrum.width / 2.0;
    const double centerY = spectrum.height / 2.0;
    constexpr double epsilon = 1e-6;

    for (int y = 0; y < spectrum.height; ++y) {
        for (int x = 0; x < spectrum.width; ++x) {
            const int index = y * spectrum.width + x;
            const Complex h = transferAt(model, params, x, y, spectrum.width, spectrum.height);
            const double h2 = std::norm(h);
            const double dx = x - centerX;
            const double dy = y - centerY;
            const double distance = std::sqrt(dx * dx + dy * dy);

            if (cutoffEnabled && distance > cutoff) {
                spectrum.values[index] = Complex(0.0, 0.0);
                continue;
            }

            if (wiener && model == RestorationModel::AtmosphericTurbulence) {
                const int iterations = std::clamp(static_cast<int>(std::round(24.0 / std::sqrt(k + 0.01))), 18, 90);
                const double factor = 1.0 - std::pow(std::max(0.0, 1.0 - h2), iterations);
                if (h2 > epsilon) {
                    spectrum.values[index] *= std::conj(h) * (factor / h2);
                } else {
                    spectrum.values[index] = Complex(0.0, 0.0);
                }
            } else if (wiener) {
                const double regularization = model == RestorationModel::MotionBlur
                    ? std::max(0.002, k)
                    : k;
                spectrum.values[index] *= std::conj(h) / (h2 + regularization + epsilon);
            } else if (h2 > epsilon) {
                spectrum.values[index] /= h;
            } else {
                spectrum.values[index] = Complex(0.0, 0.0);
            }
        }
    }
    return finalizeRestoration(inverseToImage(spectrum, false), degraded, model, wiener);
}

}

QImage ImageRestoration::atmosphericTurbulenceDegrade(const QImage& image, double k) {
    RestorationParams params;
    params.turbulenceK = k;
    return degradeImage(image, RestorationModel::AtmosphericTurbulence, params);
}

QImage ImageRestoration::motionBlurDegrade(const QImage& image, double a, double b, double t) {
    RestorationParams params;
    params.motionA = a;
    params.motionB = b;
    params.motionT = t;
    return degradeImage(image, RestorationModel::MotionBlur, params);
}

QImage ImageRestoration::inverseFilter(
    const QImage& degraded,
    RestorationModel model,
    const RestorationParams& params
) {
    return restoreImage(degraded, model, params, false, false);
}

QImage ImageRestoration::inverseFilterWithCutoff(
    const QImage& degraded,
    RestorationModel model,
    const RestorationParams& params
) {
    return restoreImage(degraded, model, params, true, false);
}

QImage ImageRestoration::wienerFilter(
    const QImage& degraded,
    RestorationModel model,
    const RestorationParams& params
) {
    return restoreImage(degraded, model, params, false, true);
}
