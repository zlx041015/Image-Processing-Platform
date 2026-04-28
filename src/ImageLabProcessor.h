#pragma once

#include <QImage>
#include <QString>

class ImageLabProcessor {
public:
    static QImage loadGrayscaleImageFromFile(const QString& filePath, QString* errorMessage = nullptr);

    static QImage addSaltPepperNoise(const QImage& img, double density);
    static QImage addImpulseNoise(const QImage& img, double density);
    static QImage meanFilter(const QImage& img, int kernelSize);
    static QImage medianFilter(const QImage& img, int kernelSize);
    static QImage maxFilter(const QImage& img, int kernelSize);

    static QImage sobelEdgeDetect(const QImage& img, int kernelSize, int threshold);
    static QImage prewittEdgeDetect(const QImage& img, int kernelSize, int threshold);
    static QImage laplacianEdgeDetect(const QImage& img, int kernelSize, int threshold);

    static QImage step1_originalImage(const QImage& img);
    static QImage step2_laplacianProcess(const QImage& img);
    static QImage step3_sharpenImage(const QImage& original, const QImage& laplacian);
    static QImage step4_sobelProcess(const QImage& img);
    static QImage step5_meanFilterGradient(const QImage& sobel);
    static QImage step6_maskImage(const QImage& sharpened, const QImage& gradient);
    static QImage step7_addOriginalAndMask(const QImage& original, const QImage& mask);
    static QImage step8_gammaTransform(const QImage& enhanced, double gamma = 0.8);
};
