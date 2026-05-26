/**
 * @file SlicePlaneCalculator.h
 * @brief Calcul des plans de coupe guidés par points céphalométriques
 */

#ifndef SLICEPLANECALCULATOR_H
#define SLICEPLANECALCULATOR_H

#include <QObject>
#include <Eigen/Dense>
#include "CBCTVolume.h"
#include "CephalometricDetector.h"

namespace CBCTAlign {

/**
 * @enum SliceOrientation
 * @brief Orientations de coupe disponibles
 */
enum class SliceOrientation {
    Axial,      ///< Plan horizontal (XY), normal = Z
    Coronal,    ///< Plan frontal (XZ), normal = Y
    Sagittal    ///< Plan latéral (YZ), normal = X
};

/**
 * @struct SlicePlane
 * @brief Définition mathématique d'un plan de coupe
 */
struct SlicePlane {
    Eigen::Vector3d normal;         ///< Vecteur normal unitaire
    Eigen::Vector3d referencePoint; ///< Point de référence (landmark)
    double distance;                ///< Distance signée au plan
    SliceOrientation orientation;   ///< Type d'orientation
    QString referenceLandmark;      ///< Nom du landmark de référence
};

/**
 * @class SlicePlaneCalculator
 * @brief Calculateur de plans de coupe guidés par landmarks
 */
class SlicePlaneCalculator : public QObject
{
    Q_OBJECT
    
public:
    explicit SlicePlaneCalculator(QObject* parent = nullptr);
    
    void setReferenceLandmark(const Landmark& landmark);
    SlicePlane computePlane(SliceOrientation orientation, double distance);
    static Eigen::Vector3d getNormal(SliceOrientation orientation);
    static QString orientationToString(SliceOrientation orientation);
    
private:
    Landmark m_referenceLandmark;
    bool m_hasReference = false;
};

} // namespace CBCTAlign

#endif // SLICEPLANECALCULATOR_H
