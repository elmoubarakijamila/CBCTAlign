/**
 * @file CephalometricDetector.cpp
 */
#include "CephalometricDetector.h"
#include "CBCTVolume.h"
#include "Logger.h"

#include <itkBinaryThresholdImageFilter.h>
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkBinaryBallStructuringElement.h>

#include <cmath>

namespace CBCTAlign {

CephalometricDetector::CephalometricDetector(QObject* parent)
    : QObject(parent)
{
}

std::vector<Landmark> CephalometricDetector::detectAll(const CBCTVolume& volume)
{
    Logger::instance().info("=== Détection céphalométrique ===");
    m_landmarks.clear();

    if (!volume.isLoaded()) {
        Logger::instance().error("Volume non chargé");
        emit detectionFinished(0);
        return m_landmarks;
    }


    emit detectionProgress(10, "Segmentation osseuse...");
    ImagePointer boneMask = segmentBone(volume);


    emit detectionProgress(25, "Menton...");
    Landmark menton;
    menton.name = "Menton";
    menton.abbreviation = "Me";
    menton.description = "Point médian le plus inférieur de la symphyse mentonnière";
    menton.position = findMenton(boneMask, volume);
    menton.confidence = 0.85;
    m_landmarks.push_back(menton);


    emit detectionProgress(40, "Nasion...");
    Landmark nasion;
    nasion.name = "Nasion";
    nasion.abbreviation = "N";
    nasion.description = "Point médian de la suture frontonasale";
    nasion.position = findNasion(boneMask, volume);
    nasion.confidence = 0.90;
    m_landmarks.push_back(nasion);


    emit detectionProgress(55, "Pogonion...");
    Landmark pogonion;
    pogonion.name = "Pogonion";
    pogonion.abbreviation = "Pog";
    pogonion.description = "Point le plus antérieur de la symphyse mandibulaire";
    pogonion.position = findPogonion(boneMask, volume, menton.position);
    pogonion.confidence = 0.82;
    m_landmarks.push_back(pogonion);


    emit detectionProgress(70, "ENA...");
    Landmark ena;
    ena.name = "Épine Nasale Antérieure";
    ena.abbreviation = "ENA";
    ena.description = "Point le plus antérieur du maxillaire sur la ligne médiane";
    ena.position = findENA(boneMask, volume);
    ena.confidence = 0.78;
    m_landmarks.push_back(ena);


    emit detectionProgress(85, "Zygions...");
    auto [zyL, zyR] = findZygions(boneMask, volume);

    Landmark zygionL;
    zygionL.name = "Zygion Gauche";
    zygionL.abbreviation = "ZyL";
    zygionL.position = zyL;
    zygionL.confidence = 0.88;
    m_landmarks.push_back(zygionL);

    Landmark zygionR;
    zygionR.name = "Zygion Droit";
    zygionR.abbreviation = "ZyR";
    zygionR.position = zyR;
    zygionR.confidence = 0.88;
    m_landmarks.push_back(zygionR);


    for (auto& lm : m_landmarks) {
        if (m_manualCorrections.contains(lm.name)) {
            lm.position = m_manualCorrections[lm.name];
            lm.isManuallyCorrect = true;
            lm.confidence = 1.0;
        }
    }

    validateGeometricConsistency();

    emit detectionProgress(100, "Terminé");
    emit detectionFinished(static_cast<int>(m_landmarks.size()));
    return m_landmarks;
}

void CephalometricDetector::setManualCorrection(const QString& name, const Eigen::Vector3d& position)
{
    m_manualCorrections[name] = position;
}

void CephalometricDetector::clearManualCorrections()
{
    m_manualCorrections.clear();
}

Landmark CephalometricDetector::getLandmark(const QString& name) const
{
    for (const auto& lm : m_landmarks) {
        if (lm.name == name || lm.abbreviation == name)
            return lm;
    }
    return Landmark();
}

double CephalometricDetector::computeDetectionError(const QString& name,
                                                     const Eigen::Vector3d& reference) const
{
    return (getLandmark(name).position - reference).norm();
}


ImagePointer CephalometricDetector::segmentBone(const CBCTVolume& volume)
{
    using ThresholdType = itk::BinaryThresholdImageFilter<ImageType, ImageType>;
    auto threshold = ThresholdType::New();
    threshold->SetInput(volume.getITKImage());
    threshold->SetLowerThreshold(m_params.boneThresholdHU);
    threshold->SetUpperThreshold(3000);
    threshold->SetInsideValue(1);
    threshold->SetOutsideValue(0);
    threshold->Update();

    using SEType = itk::BinaryBallStructuringElement<float, 3>;
    SEType se;
    se.SetRadius(2);
    se.CreateStructuringElement();

    using ClosingType = itk::BinaryMorphologicalClosingImageFilter<ImageType, ImageType, SEType>;
    auto closing = ClosingType::New();
    closing->SetInput(threshold->GetOutput());
    closing->SetKernel(se);
    closing->SetForegroundValue(1);
    closing->Update();

    return closing->GetOutput();
}

double CephalometricDetector::estimateMidlinePlane(const CBCTVolume& volume)
{
    auto origin = volume.getOrigin();
    auto size = volume.getSize();
    auto spacing = volume.getSpacing();
    return origin.x() + (size.x() * spacing.x()) / 2.0;
}

Eigen::Vector3d CephalometricDetector::findMenton(ImagePointer boneMask, const CBCTVolume& volume)
{
    double midX = estimateMidlinePlane(volume);
    auto spacing = volume.getSpacing();
    auto origin = volume.getOrigin();
    auto size = volume.getSize();

    int midIdx = static_cast<int>((midX - origin.x()) / spacing.x());
    int range = static_cast<int>(m_params.searchRadiusMm / spacing.x());

    Eigen::Vector3d best = origin;
    double lowestZ = origin.z() + size.z() * spacing.z();

    itk::ImageRegionConstIteratorWithIndex<ImageType> it(boneMask, boneMask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0.5f && std::abs(it.GetIndex()[0] - midIdx) <= range) {
            double z = origin.z() + it.GetIndex()[2] * spacing.z();
            if (z < lowestZ) {
                lowestZ = z;
                best = Eigen::Vector3d(
                    origin.x() + it.GetIndex()[0] * spacing.x(),
                    origin.y() + it.GetIndex()[1] * spacing.y(), z);
            }
        }
    }
    return best;
}

Eigen::Vector3d CephalometricDetector::findNasion(ImagePointer boneMask, const CBCTVolume& volume)
{
    double midX = estimateMidlinePlane(volume);
    auto spacing = volume.getSpacing();
    auto origin = volume.getOrigin();
    auto size = volume.getSize();

    int midIdx = static_cast<int>((midX - origin.x()) / spacing.x());
    int range = static_cast<int>(m_params.searchRadiusMm / spacing.x());
    int zStart = static_cast<int>(size.z() * 0.6);
    int zEnd = static_cast<int>(size.z() * 0.85);

    Eigen::Vector3d best = origin;
    double maxY = origin.y();

    itk::ImageRegionConstIteratorWithIndex<ImageType> it(boneMask, boneMask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0.5f) {
            auto idx = it.GetIndex();
            if (idx[2] >= zStart && idx[2] <= zEnd && std::abs(idx[0] - midIdx) <= range) {
                double y = origin.y() + idx[1] * spacing.y();
                if (y > maxY) {
                    maxY = y;
                    best = Eigen::Vector3d(origin.x() + idx[0] * spacing.x(), y,
                                           origin.z() + idx[2] * spacing.z());
                }
            }
        }
    }
    return best;
}

Eigen::Vector3d CephalometricDetector::findPogonion(ImagePointer boneMask,
                                                     const CBCTVolume& volume,
                                                     const Eigen::Vector3d& menton)
{
    auto spacing = volume.getSpacing();
    auto origin = volume.getOrigin();

    int midIdx = static_cast<int>((menton.x() - origin.x()) / spacing.x());
    int rangeX = static_cast<int>(10.0 / spacing.x());

    Eigen::Vector3d best = menton;
    double maxY = menton.y();

    itk::ImageRegionConstIteratorWithIndex<ImageType> it(boneMask, boneMask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0.5f) {
            auto idx = it.GetIndex();
            double z = origin.z() + idx[2] * spacing.z();
            if (z >= menton.z() && z <= menton.z() + 30.0 && std::abs(idx[0] - midIdx) <= rangeX) {
                double y = origin.y() + idx[1] * spacing.y();
                if (y > maxY) {
                    maxY = y;
                    best = Eigen::Vector3d(origin.x() + idx[0] * spacing.x(), y, z);
                }
            }
        }
    }
    return best;
}

Eigen::Vector3d CephalometricDetector::findENA(ImagePointer boneMask, const CBCTVolume& volume)
{
    double midX = estimateMidlinePlane(volume);
    auto spacing = volume.getSpacing();
    auto origin = volume.getOrigin();
    auto size = volume.getSize();

    int midIdx = static_cast<int>((midX - origin.x()) / spacing.x());
    int range = static_cast<int>(5.0 / spacing.x());
    int zStart = static_cast<int>(size.z() * 0.35);
    int zEnd = static_cast<int>(size.z() * 0.55);

    Eigen::Vector3d best = origin;
    double maxY = origin.y();

    itk::ImageRegionConstIteratorWithIndex<ImageType> it(boneMask, boneMask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0.5f) {
            auto idx = it.GetIndex();
            if (idx[2] >= zStart && idx[2] <= zEnd && std::abs(idx[0] - midIdx) <= range) {
                double y = origin.y() + idx[1] * spacing.y();
                if (y > maxY) {
                    maxY = y;
                    best = Eigen::Vector3d(origin.x() + idx[0] * spacing.x(), y,
                                           origin.z() + idx[2] * spacing.z());
                }
            }
        }
    }
    return best;
}

std::pair<Eigen::Vector3d, Eigen::Vector3d> CephalometricDetector::findZygions(
    ImagePointer boneMask, const CBCTVolume& volume)
{
    double midX = estimateMidlinePlane(volume);
    auto spacing = volume.getSpacing();
    auto origin = volume.getOrigin();
    auto size = volume.getSize();

    int zStart = static_cast<int>(size.z() * 0.45);
    int zEnd = static_cast<int>(size.z() * 0.65);

    Eigen::Vector3d zyL = origin, zyR = origin;
    double minX = origin.x() + size.x() * spacing.x();
    double maxX = origin.x();

    itk::ImageRegionConstIteratorWithIndex<ImageType> it(boneMask, boneMask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0.5f) {
            auto idx = it.GetIndex();
            if (idx[2] >= zStart && idx[2] <= zEnd) {
                double x = origin.x() + idx[0] * spacing.x();
                double y = origin.y() + idx[1] * spacing.y();
                double z = origin.z() + idx[2] * spacing.z();
                if (x < minX && x < midX) { minX = x; zyL = {x, y, z}; }
                if (x > maxX && x > midX) { maxX = x; zyR = {x, y, z}; }
            }
        }
    }
    return {zyL, zyR};
}

bool CephalometricDetector::validateGeometricConsistency()
{
    if (m_landmarks.size() < 3) return true;

    Landmark nasion, menton;
    bool hasN = false, hasMe = false;
    for (const auto& lm : m_landmarks) {
        if (lm.abbreviation == "N")  { nasion = lm; hasN = true; }
        if (lm.abbreviation == "Me") { menton = lm; hasMe = true; }
    }

    if (hasN && hasMe) {
        if (nasion.position.z() <= menton.position.z()) {
            Logger::instance().warning("Nasion sous le Menton — incohérent!");
            return false;
        }
        double dist = (nasion.position - menton.position).norm();
        if (dist < 80 || dist > 200) {
            Logger::instance().warning(QString("Distance N-Me = %1 mm — suspecte").arg(dist));
            return false;
        }
    }
    return true;
}

} // namespace CBCTAlign
