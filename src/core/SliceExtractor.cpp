/**
 * @file SliceExtractor.cpp
 * @brief Extraction basée sur POSITION PHYSIQUE
 */
#include "SliceExtractor.h"
#include "Logger.h"

#include <QFile>
#include <QDataStream>
#include <algorithm>
#include <cmath>

namespace CBCTAlign {

SliceExtractor::SliceExtractor(QObject* parent)
    : QObject(parent)
{
}

Slice2D SliceExtractor::extractSliceAtPhysicalPosition(
    const CBCTVolume& volume,
    const Eigen::Vector3d& planeOrigin,
    const Eigen::Vector3d& planeNormal,
    SliceOrientation orientation)
{
    Slice2D slice;
    slice.plane.orientation = orientation;
    slice.plane.normal = planeNormal;
    slice.plane.referencePoint = planeOrigin;

    if (!volume.isLoaded()) {
        Logger::instance().error("Volume non chargé");
        return slice;
    }

    auto size = volume.getSize();
    auto spacing = volume.getSpacing();
    auto origin = volume.getOrigin();

    // Calculer l'index à partir de la position physique
    int sliceIndex;
    double physCoord;

    switch (orientation) {
        case SliceOrientation::Axial:
            physCoord = planeOrigin.z();
            sliceIndex = std::clamp(
                static_cast<int>(std::round((physCoord - origin.z()) / spacing.z())),
                0, size.z() - 1);
            slice.width = size.x();
            slice.height = size.y();
            slice.pixelSpacing = spacing.x();
            break;

        case SliceOrientation::Coronal:
            physCoord = planeOrigin.y();
            sliceIndex = std::clamp(
                static_cast<int>(std::round((physCoord - origin.y()) / spacing.y())),
                0, size.y() - 1);
            slice.width = size.x();
            slice.height = size.z();
            slice.pixelSpacing = spacing.x();
            break;

        case SliceOrientation::Sagittal:
            physCoord = planeOrigin.x();
            sliceIndex = std::clamp(
                static_cast<int>(std::round((physCoord - origin.x()) / spacing.x())),
                0, size.x() - 1);
            slice.width = size.y();
            slice.height = size.z();
            slice.pixelSpacing = spacing.y();
            break;
    }

    // Extraire les pixels
    slice.data.resize(slice.width * slice.height);

    for (int h = 0; h < slice.height; ++h) {
        for (int w = 0; w < slice.width; ++w) {
            switch (orientation) {
                case SliceOrientation::Axial:
                    slice.data[h * slice.width + w] = volume.getVoxelValue(w, h, sliceIndex);
                    break;
                case SliceOrientation::Coronal:
                    slice.data[h * slice.width + w] = volume.getVoxelValue(w, sliceIndex, h);
                    break;
                case SliceOrientation::Sagittal:
                    slice.data[h * slice.width + w] = volume.getVoxelValue(sliceIndex, w, h);
                    break;
            }
        }
    }

    Logger::instance().info(QString("Coupe %1: pos=%2mm, index=%3, %4x%5")
        .arg(SlicePlaneCalculator::orientationToString(orientation))
        .arg(physCoord, 0, 'f', 2).arg(sliceIndex)
        .arg(slice.width).arg(slice.height));

    return slice;
}

Slice2D SliceExtractor::extractSlice(const CBCTVolume& volume, const SlicePlane& plane)
{
    if (!volume.isLoaded()) {
        Logger::instance().error("Volume non chargé");
        return Slice2D();
    }

    // Position physique = point de ref + distance × normale
    Eigen::Vector3d planeOrigin = plane.referencePoint + plane.distance * plane.normal;
    return extractSliceAtPhysicalPosition(volume, planeOrigin, plane.normal, plane.orientation);
}

Slice2D SliceExtractor::extractSliceAtIndex(const CBCTVolume& volume,
                                             SliceOrientation orientation,
                                             int sliceIndex)
{
    Slice2D slice;
    slice.plane.orientation = orientation;

    auto size = volume.getSize();
    auto spacing = volume.getSpacing();

    switch (orientation) {
        case SliceOrientation::Axial:
            slice.width = size.x(); slice.height = size.y();
            slice.pixelSpacing = spacing.x();
            sliceIndex = std::clamp(sliceIndex, 0, size.z() - 1);
            break;
        case SliceOrientation::Coronal:
            slice.width = size.x(); slice.height = size.z();
            slice.pixelSpacing = spacing.x();
            sliceIndex = std::clamp(sliceIndex, 0, size.y() - 1);
            break;
        case SliceOrientation::Sagittal:
            slice.width = size.y(); slice.height = size.z();
            slice.pixelSpacing = spacing.y();
            sliceIndex = std::clamp(sliceIndex, 0, size.x() - 1);
            break;
    }

    slice.data.resize(slice.width * slice.height);

    for (int h = 0; h < slice.height; ++h) {
        for (int w = 0; w < slice.width; ++w) {
            switch (orientation) {
                case SliceOrientation::Axial:
                    slice.data[h * slice.width + w] = volume.getVoxelValue(w, h, sliceIndex);
                    break;
                case SliceOrientation::Coronal:
                    slice.data[h * slice.width + w] = volume.getVoxelValue(w, sliceIndex, h);
                    break;
                case SliceOrientation::Sagittal:
                    slice.data[h * slice.width + w] = volume.getVoxelValue(sliceIndex, w, h);
                    break;
            }
        }
    }
    return slice;
}


QImage Slice2D::toQImage(double windowCenter, double windowWidth) const
{
    QImage image(width, height, QImage::Format_Grayscale8);
    double wMin = windowCenter - windowWidth / 2.0;

    for (int y = 0; y < height; ++y) {
        uchar* line = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            double normalized = std::clamp((data[y * width + x] - wMin) / windowWidth, 0.0, 1.0);
            line[x] = static_cast<uchar>(normalized * 255);
        }
    }
    return image;
}

bool Slice2D::saveToFile(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream s(&file);
    s << width << height << pixelSpacing;
    for (float v : data) s << v;
    return true;
}

} // namespace CBCTAlign
