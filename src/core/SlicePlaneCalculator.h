/**
 * @file SlicePlaneCalculator.h
 */

#ifndef SLICEPLANECALCULATOR_H
#define SLICEPLANECALCULATOR_H

#include <QObject>
#include <Eigen/Dense>
#include "CBCTVolume.h"
#include "CephalometricDetector.h"

namespace CBCTAlign {


enum class SliceOrientation {
    Axial,     
    Coronal,    
    Sagittal    
};


struct SlicePlane {
    Eigen::Vector3d normal;         
    Eigen::Vector3d referencePoint; 
    double distance;                
    SliceOrientation orientation;   
    QString referenceLandmark;      
};


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

#endif 
