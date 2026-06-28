/**
 * @file CephalometricDetector.h
 */
#ifndef CEPHALOMETRICDETECTOR_H
#define CEPHALOMETRICDETECTOR_H

#include <QObject>
#include <QString>
#include <QMap>
#include <vector>
#include <Eigen/Dense>
#include <itkImage.h>

namespace CBCTAlign {

class CBCTVolume;

using ImageType = itk::Image<float, 3>;
using ImagePointer = ImageType::Pointer;


struct Landmark {
    QString name;
    QString abbreviation;
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
    double confidence = 0.0;
    bool isManuallyCorrect = false;
    QString description;
};

class CephalometricDetector : public QObject
{
    Q_OBJECT

public:
    explicit CephalometricDetector(QObject* parent = nullptr);

    struct Parameters {
        double boneThresholdHU = 300.0;
        double corticalThresholdHU = 700.0;
        double searchRadiusMm = 30.0;
    };

    void setParameters(const Parameters& params) { m_params = params; }
    Parameters getParameters() const { return m_params; }


    std::vector<Landmark> detectAll(const CBCTVolume& volume);


    void setManualCorrection(const QString& name, const Eigen::Vector3d& position);
    void clearManualCorrections();


    std::vector<Landmark> getLandmarks() const { return m_landmarks; }
    Landmark getLandmark(const QString& name) const;
    double computeDetectionError(const QString& name, const Eigen::Vector3d& reference) const;

signals:
    void detectionProgress(int percent, const QString& landmark);
    void detectionFinished(int landmarkCount);

private:
    Parameters m_params;
    std::vector<Landmark> m_landmarks;
    QMap<QString, Eigen::Vector3d> m_manualCorrections;


    ImagePointer segmentBone(const CBCTVolume& volume);
    double estimateMidlinePlane(const CBCTVolume& volume);


    Eigen::Vector3d findMenton(ImagePointer boneMask, const CBCTVolume& volume);
    Eigen::Vector3d findNasion(ImagePointer boneMask, const CBCTVolume& volume);
    Eigen::Vector3d findPogonion(ImagePointer boneMask, const CBCTVolume& volume,
                                  const Eigen::Vector3d& menton);
    Eigen::Vector3d findENA(ImagePointer boneMask, const CBCTVolume& volume);
    std::pair<Eigen::Vector3d, Eigen::Vector3d> findZygions(
        ImagePointer boneMask, const CBCTVolume& volume);

    bool validateGeometricConsistency();
};

} // namespace CBCTAlign
#endif
