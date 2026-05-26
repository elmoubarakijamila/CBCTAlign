/**
 * @file SlicePlaneCalculator.cpp
 * @brief Implémentation du calculateur de plans de coupe
 */

#include "SlicePlaneCalculator.h"
#include "Logger.h"

namespace CBCTAlign {

SlicePlaneCalculator::SlicePlaneCalculator(QObject* parent)
    : QObject(parent)
{
}

void SlicePlaneCalculator::setReferenceLandmark(const Landmark& landmark)
{
    m_referenceLandmark = landmark;
    m_hasReference = true;
    Logger::instance().info(QString("Landmark de référence: %1 [%2, %3, %4]")
        .arg(landmark.name)
        .arg(landmark.position.x(), 0, 'f', 2)
        .arg(landmark.position.y(), 0, 'f', 2)
        .arg(landmark.position.z(), 0, 'f', 2));
}

SlicePlane SlicePlaneCalculator::computePlane(SliceOrientation orientation, double distance)
{
    SlicePlane plane;
    plane.orientation = orientation;
    plane.distance = distance;
    plane.normal = getNormal(orientation);
    plane.referencePoint = m_hasReference ? m_referenceLandmark.position : Eigen::Vector3d::Zero();
    plane.referenceLandmark = m_hasReference ? m_referenceLandmark.name : "Centre";
    
    return plane;
}

Eigen::Vector3d SlicePlaneCalculator::getNormal(SliceOrientation orientation)
{
    switch (orientation) {
        case SliceOrientation::Axial:    return Eigen::Vector3d(0, 0, 1);
        case SliceOrientation::Coronal:  return Eigen::Vector3d(0, 1, 0);
        case SliceOrientation::Sagittal: return Eigen::Vector3d(1, 0, 0);
    }
    return Eigen::Vector3d(0, 0, 1);
}

QString SlicePlaneCalculator::orientationToString(SliceOrientation orientation)
{
    switch (orientation) {
        case SliceOrientation::Axial:    return "Axial";
        case SliceOrientation::Coronal:  return "Coronal";
        case SliceOrientation::Sagittal: return "Sagittal";
    }
    return "Unknown";
}

} // namespace CBCTAlign
