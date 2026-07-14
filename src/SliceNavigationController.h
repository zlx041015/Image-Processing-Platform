#pragma once

#include <QObject>
#include <QVector3D>

class SliceNavigationController : public QObject {
    Q_OBJECT

public:
    struct VolumeGeometry {
        int width = 0;
        int height = 0;
        int depth = 0;
        double spacingX = 1.0;
        double spacingY = 1.0;
        double spacingZ = 1.0;
        QVector3D origin = QVector3D(0, 0, 0);
        QVector3D rowDirection = QVector3D(1, 0, 0);
        QVector3D columnDirection = QVector3D(0, 1, 0);
        QVector3D sliceDirection = QVector3D(0, 0, 1);
    };

    struct State {
        int axialIndex = 0;
        int coronalIndex = 0;
        int sagittalIndex = 0;
        QVector3D voxel = QVector3D(0, 0, 0);
        QVector3D world = QVector3D(0, 0, 0);
    };

    explicit SliceNavigationController(QObject* parent = nullptr);

    void configure(const VolumeGeometry& geometry);
    const VolumeGeometry& geometry() const { return m_geometry; }
    const State& state() const { return m_state; }

    void setAxialIndex(int index);
    void setCoronalIndex(int index);
    void setSagittalIndex(int index);

    void pickFromAxialView(int x, int y);
    void pickFromCoronalView(int x, int y);
    void pickFromSagittalView(int x, int y);

signals:
    void stateChanged(const SliceNavigationController::State& state);

private:
    void clampState();
    void updateDerivedCoordinates();
    void emitIfChanged(const State& previous);

    VolumeGeometry m_geometry;
    State m_state;
};
