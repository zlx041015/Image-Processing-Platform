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
    int padTop = 0;
    int padLeft = 0;
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

int reflectIndex(int index, int size) {
    if (size <= 1) {
        return 0;
    }
    while (index < 0 || index >= size) {
        if (index < 0) {
            index = -index - 1;
        } else {
            index = 2 * size - index - 1;
        }
    }
    return index;
}

int clampToByte(double value) {
    return std::clamp(static_cast<int>(std::lround(value)), 0, 255);
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
    spectrum.width = nextPowerOfTwo(gray.width() * 2);
    spectrum.height = nextPowerOfTwo(gray.height() * 2);
    spectrum.padLeft = (spectrum.width - gray.width()) / 2;
    spectrum.padTop = (spectrum.height - gray.height()) / 2;
    spectrum.values.resize(spectrum.width * spectrum.height);

    for (int y = 0; y < spectrum.height; ++y) {
        for (int x = 0; x < spectrum.width; ++x) {
            const int srcX = reflectIndex(x - spectrum.padLeft, gray.width());
            const int srcY = reflectIndex(y - spectrum.padTop, gray.height());
            double value = qGray(gray.pixel(srcX, srcY));
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
            const int sx = x + spectrum.padLeft;
            const int sy = y + spectrum.padTop;
            double value = spatial[sy * spectrum.width + sx].real();
            if ((sx + sy) & 1) {
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

QImage normalizeForDisplay(const QImage& image) {
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
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
    const int lowTarget = std::max(0, static_cast<int>(std::round(total * 0.005)));
    const int highTarget = std::max(0, static_cast<int>(std::round(total * 0.995)));
    int cumulative = 0;
    int minValue = 0;
    int maxValue = 255;
    for (int i = 0; i < 256; ++i) {
        cumulative += hist[i];
        if (cumulative > lowTarget) {
            minValue = i;
            break;
        }
    }
    cumulative = 0;
    for (int i = 0; i < 256; ++i) {
        cumulative += hist[i];
        if (cumulative > highTarget) {
            maxValue = i;
            break;
        }
    }

    if (maxValue <= minValue) {
        minValue = 0;
        maxValue = 255;
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    const double scale = 255.0 / (maxValue - minValue);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* src = gray.constScanLine(y);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            dst[x] = static_cast<uchar>(clampToByte((src[x] - minValue) * scale));
        }
    }
    return out;
}

QImage boxBlur3x3(const QImage& image) {
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
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
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int sum = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    sum += sample(x + kx, y + ky);
                }
            }
            dst[x] = static_cast<uchar>(sum / 9);
        }
    }
    return out;
}

QImage unsharpMask(const QImage& image, double amount) {
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    const QImage blur = boxBlur3x3(gray);
    QImage out(gray.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* src = gray.constScanLine(y);
        const uchar* low = blur.constScanLine(y);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            const double value = src[x] + amount * (src[x] - low[x]);
            dst[x] = static_cast<uchar>(clampToByte(value));
        }
    }
    return out;
}

QImage localContrastBoost(const QImage& image, double amount) {
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int radius = 2;
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

QImage postEnhanceRestoration(const QImage& image, RestorationModel model, bool wiener) {
    QImage out = normalizeForDisplay(image);
    if (model == RestorationModel::AtmosphericTurbulence) {
        out = unsharpMask(out, wiener ? 0.55 : 0.35);
        return normalizeForDisplay(out);
    }

    out = localContrastBoost(out, wiener ? 0.42 : 0.28);
    out = unsharpMask(out, wiener ? 1.10 : 0.75);
    out = normalizeForDisplay(out);
    return out;
}

double idealLowPassGain(double distance, double cutoff) {
    return distance <= cutoff ? 1.0 : 0.0;
}

double butterworthLowPassGain(double distance, double cutoff, int order) {
    const double safeCutoff = std::max(1.0, cutoff);
    const int safeOrder = std::max(1, order);
    return 1.0 / (1.0 + std::pow(distance / safeCutoff, 2.0 * safeOrder));
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
    return normalizeForDisplay(inverseToImage(spectrum, false));
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
    constexpr double epsilon = 1e-8;
    const double inverseFloor = model == RestorationModel::AtmosphericTurbulence ? 0.02 : 0.05;
    const int butterworthOrder = 2;

    for (int y = 0; y < spectrum.height; ++y) {
        for (int x = 0; x < spectrum.width; ++x) {
            const int index = y * spectrum.width + x;
            const Complex h = transferAt(model, params, x, y, spectrum.width, spectrum.height);
            const double h2 = std::norm(h);
            const double dx = x - centerX;
            const double dy = y - centerY;
            const double distance = std::sqrt(dx * dx + dy * dy);
            const double cutoffGain = cutoffEnabled ? butterworthLowPassGain(distance, cutoff, butterworthOrder) : 1.0;

            if (wiener) {
                const double lowPass = model == RestorationModel::MotionBlur
                    ? butterworthLowPassGain(distance, cutoffEnabled ? cutoff : 70.0, butterworthOrder)
                    : cutoffGain;
                spectrum.values[index] *= (std::conj(h) / (h2 + k + epsilon)) * lowPass;
            } else if (std::abs(h) >= inverseFloor && h2 > epsilon) {
                spectrum.values[index] *= (Complex(1.0, 0.0) / h) * cutoffGain;
            } else {
                spectrum.values[index] = Complex(0.0, 0.0);
            }
        }
    }
    return postEnhanceRestoration(inverseToImage(spectrum, false), model, wiener);
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
