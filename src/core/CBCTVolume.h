/**
 * @file CBCTVolume.h
 */
#ifndef CBCTVOLUME_H
#define CBCTVOLUME_H

#include <QString>
#include <QObject>
#include <memory>

#include <itkImage.h>
#include <itkSmartPointer.h>
#include <Eigen/Dense>

namespace CBCTAlign {

using ImageType = itk::Image<float, 3>;
using ImagePointer = ImageType::Pointer;

class CBCTVolume : public QObject
{
    Q_OBJECT

public:
    explicit CBCTVolume(QObject* parent = nullptr);
    ~CBCTVolume() override = default;


    bool loadFromDICOM(const QString& dirPath);
    bool loadFromNIfTI(const QString& filePath);
    bool saveToNIfTI(const QString& filePath) const;


    void resampleIsotropic(double spacing_mm = 0.3);
    void normalizeIntensity();
    void applyMedianFilter(int kernelSize = 3);
    void histogramMatch(const CBCTVolume& reference);


    Eigen::Vector3d getOrigin() const;
    Eigen::Vector3d getSpacing() const;
    Eigen::Vector3i getSize() const;
    Eigen::Matrix3d getDirection() const;


    float getVoxelValue(int x, int y, int z) const;
    float getInterpolatedValue(const Eigen::Vector3d& physicalPoint) const;


    Eigen::Vector3i physicalToIndex(const Eigen::Vector3d& point) const;
    Eigen::Vector3d indexToPhysical(const Eigen::Vector3i& index) const;


    bool isLoaded() const { return m_isLoaded; }


    ImagePointer getITKImage() const { return m_volume; }
    void setITKImage(ImagePointer image);


    QString getName() const { return m_name; }
    void setName(const QString& name) { m_name = name; }
    QString getFilePath() const { return m_sourcePath; }

    struct Statistics { float min, max, mean, stdDev; };
    Statistics computeStatistics() const;

signals:
    void loadingProgress(int percent, const QString& message);
    void volumeChanged();

private:
    ImagePointer m_volume;
    QString m_name;
    QString m_sourcePath;
    bool m_isLoaded = false;
};

} // namespace CBCTAlign
#endif // CBCTVOLUME_H
