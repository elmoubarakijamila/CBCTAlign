/**
 * @file SliceExtractor.h
 */
#ifndef SLICEEXTRACTOR_H
#define SLICEEXTRACTOR_H

#include <QObject>
#include <QImage>
#include <vector>
#include <Eigen/Dense>

#include "CBCTVolume.h"
#include "SlicePlaneCalculator.h"

namespace CBCTAlign {


struct Slice2D {
    int width = 0;
    int height = 0;
    double pixelSpacing = 1.0;
    std::vector<float> data;

    SlicePlane plane;
    QString timepoint;
    QString sourceCBCT;

    QImage toQImage(double windowCenter = 0, double windowWidth = 2000) const;
    bool saveToFile(const QString& path) const;
};


class SliceExtractor : public QObject {
    Q_OBJECT

public:
    explicit SliceExtractor(QObject* parent = nullptr);


    Slice2D extractSlice(const CBCTVolume& volume, const SlicePlane& plane);


    Slice2D extractSliceAtPhysicalPosition(
        const CBCTVolume& volume,
        const Eigen::Vector3d& planeOrigin,
        const Eigen::Vector3d& planeNormal,
        SliceOrientation orientation);


    Slice2D extractSliceAtIndex(const CBCTVolume& volume,
                                 SliceOrientation orientation,
                                 int sliceIndex);

signals:
    void extractionProgress(int percent);
};

} // namespace CBCTAlign
#endif
