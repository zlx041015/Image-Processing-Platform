#pragma once

#include <QImage>

enum class RestorationModel {
    AtmosphericTurbulence,
    MotionBlur
};

struct RestorationParams {
    double turbulenceK = 0.0025;
    double motionA = 0.1;
    double motionB = 0.1;
    double motionT = 1.0;
    double inverseCutoff = 50.0;
    double wienerK = 0.003;
};

class ImageRestoration {
public:
    static QImage atmosphericTurbulenceDegrade(const QImage& image, double k);
    static QImage motionBlurDegrade(const QImage& image, double a, double b, double t);
    static QImage normalizeForDisplay(const QImage& image);

    static QImage inverseFilter(
        const QImage& degraded,
        RestorationModel model,
        const RestorationParams& params
    );
    static QImage inverseFilterWithCutoff(
        const QImage& degraded,
        RestorationModel model,
        const RestorationParams& params
    );
    static QImage wienerFilter(
        const QImage& degraded,
        RestorationModel model,
        const RestorationParams& params
    );
};
