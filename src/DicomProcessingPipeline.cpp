#include "DicomProcessingPipeline.h"

#include "FourierExperimentWidget.h"
#include "ImageHistTools.h"
#include "ImageLabProcessor.h"

namespace {
QImage applySingle(const QImage& image, const DicomProcessRequest& request) {
    const QString& action = request.actionName;
    if (action == QString::fromUtf8(u8"线性拉伸")) {
        return ImageHistTools::linearStretch(image);
    }
    if (action == QString::fromUtf8(u8"均衡化")) {
        return ImageHistTools::equalizeHist(image);
    }
    if (action == QString::fromUtf8(u8"均值滤波")) {
        return ImageLabProcessor::meanFilter(image, request.kernelSize);
    }
    if (action == QString::fromUtf8(u8"中值滤波")) {
        return ImageLabProcessor::medianFilter(image, request.kernelSize);
    }
    if (action == QString::fromUtf8(u8"最大值滤波")) {
        return ImageLabProcessor::maxFilter(image, request.kernelSize);
    }
    if (action == QString::fromUtf8(u8"锐化处理") || action == QString::fromUtf8(u8"锐化")) {
        const QImage lap = ImageLabProcessor::step2_laplacianProcess(image);
        return ImageLabProcessor::step3_sharpenImage(image, lap);
    }
    if (action == QString::fromUtf8(u8"Gamma 变换")) {
        return ImageLabProcessor::step8_gammaTransform(image, request.gamma);
    }
    if (action == QString::fromUtf8(u8"理想低通") ||
        action == QString::fromUtf8(u8"巴特沃斯低通") ||
        action == QString::fromUtf8(u8"理想高通") ||
        action == QString::fromUtf8(u8"巴特沃斯高通") ||
        action == QString::fromUtf8(u8"同态滤波") ||
        action == QString::fromUtf8(u8"FFT 频谱") ||
        action == QString::fromUtf8(u8"IFFT 重建")) {
        return FourierExperimentWidget::processFrequencyImage(
            image, action, request.cutoff, request.order, request.gammaLow, request.gammaHigh, request.c);
    }
    return image;
}
}

bool DicomProcessingPipeline::apply(
    const QVector<QImage>& inputSlices,
    DicomProcessingScope scope,
    int currentSliceIndex,
    const DicomProcessRequest& request,
    QVector<QImage>* outputSlices,
    QString* errorMessage
) {
    if (!outputSlices) {
        if (errorMessage) *errorMessage = QStringLiteral("invalid output pointer");
        return false;
    }
    if (inputSlices.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("empty input volume");
        return false;
    }

    QVector<QImage> result = inputSlices;
    if (scope == DicomProcessingScope::CurrentSlice) {
        const int idx = std::clamp(currentSliceIndex, 0, static_cast<int>(inputSlices.size()) - 1);
        result[idx] = applySingle(inputSlices[idx], request);
    } else {
        for (int i = 0; i < result.size(); ++i) {
            result[i] = applySingle(inputSlices[i], request);
        }
    }

    *outputSlices = result;
    return true;
}
