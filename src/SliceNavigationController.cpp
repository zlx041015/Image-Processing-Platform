#include "SliceNavigationController.h"

#include <algorithm>

SliceNavigationController::SliceNavigationController(QObject* parent)
    : QObject(parent) {}

void SliceNavigationController::configure(const VolumeGeometry& geometry) {
    m_geometry = geometry;
    m_state.axialIndex = std::clamp(geometry.depth / 2, 0, std::max(0, geometry.depth - 1));
    m_state.coronalIndex = std::clamp(geometry.height / 2, 0, std::max(0, geometry.height - 1));
    m_state.sagittalIndex = std::clamp(geometry.width / 2, 0, std::max(0, geometry.width - 1));
    updateDerivedCoordinates();
    emit stateChanged(m_state);
}

void SliceNavigationController::setAxialIndex(int index) {
    State previous = m_state;
    m_state.axialIndex = index;
    clampState();
    updateDerivedCoordinates();
    emitIfChanged(previous);
}

void SliceNavigationController::setCoronalIndex(int index) {
    State previous = m_state;
    m_state.coronalIndex = index;
    clampState();
    updateDerivedCoordinates();
    emitIfChanged(previous);
}

void SliceNavigationController::setSagittalIndex(int index) {
    State previous = m_state;
    m_state.sagittalIndex = index;
    clampState();
    updateDerivedCoordinates();
    emitIfChanged(previous);
}

void SliceNavigationController::pickFromAxialView(int x, int y) {
    State previous = m_state;
    m_state.sagittalIndex = x;
    m_state.coronalIndex = y;
    clampState();
    updateDerivedCoordinates();
    emitIfChanged(previous);
}

void SliceNavigationController::pickFromCoronalView(int x, int y) {
    State previous = m_state;
    m_state.sagittalIndex = x;
    m_state.axialIndex = std::max(0, m_geometry.depth - 1 - y);
    clampState();
    updateDerivedCoordinates();
    emitIfChanged(previous);
}

void SliceNavigationController::pickFromSagittalView(int x, int y) {
    State previous = m_state;
    m_state.axialIndex = std::max(0, m_geometry.depth - 1 - x);
    m_state.coronalIndex = y;
    clampState();
    updateDerivedCoordinates();
    emitIfChanged(previous);
}

void SliceNavigationController::clampState() {
    m_state.axialIndex = std::clamp(m_state.axialIndex, 0, std::max(0, m_geometry.depth - 1));
    m_state.coronalIndex = std::clamp(m_state.coronalIndex, 0, std::max(0, m_geometry.height - 1));
    m_state.sagittalIndex = std::clamp(m_state.sagittalIndex, 0, std::max(0, m_geometry.width - 1));
}

void SliceNavigationController::updateDerivedCoordinates() {
    m_state.voxel = QVector3D(
        static_cast<float>(m_state.sagittalIndex),
        static_cast<float>(m_state.coronalIndex),
        static_cast<float>(m_state.axialIndex)
    );

    const QVector3D world =
        m_geometry.origin +
        m_geometry.rowDirection * static_cast<float>(m_state.sagittalIndex * m_geometry.spacingX) +
        m_geometry.columnDirection * static_cast<float>(m_state.coronalIndex * m_geometry.spacingY) +
        m_geometry.sliceDirection * static_cast<float>(m_state.axialIndex * m_geometry.spacingZ);
    m_state.world = world;
}

void SliceNavigationController::emitIfChanged(const State& previous) {
    if (previous.axialIndex != m_state.axialIndex ||
        previous.coronalIndex != m_state.coronalIndex ||
        previous.sagittalIndex != m_state.sagittalIndex) {
        emit stateChanged(m_state);
    }
}
