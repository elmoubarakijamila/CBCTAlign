/**
 * @file CBCTVolume.cpp
 * @brief Implémentation de la classe CBCTVolume
 */
#include "CBCTVolume.h"
#include "Logger.h"

#include <QDir>
#include <QFileInfo>

#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>
#include <itkNiftiImageIO.h>
#include <itkResampleImageFilter.h>
#include <itkBSplineInterpolateImageFunction.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkMedianImageFilter.h>
#include <itkHistogramMatchingImageFilter.h>
#include <itkStatisticsImageFilter.h>
#include <itkImageRegionIterator.h>


namespace CBCTAlign {

CBCTVolume::CBCTVolume(QObject* parent)
    : QObject(parent), m_volume(nullptr), m_isLoaded(false)
{
}

bool CBCTVolume::loadFromDICOM(const QString& dirPath)
{
    Logger::instance().info(QString("Chargement DICOM depuis: %1").arg(dirPath));

    try {
        emit loadingProgress(0, "Recherche des fichiers DICOM...");

        auto nameGenerator = itk::GDCMSeriesFileNames::New();
        nameGenerator->SetUseSeriesDetails(true);
        nameGenerator->AddSeriesRestriction("0008|0021");
        nameGenerator->SetDirectory(dirPath.toStdString());

        const auto& seriesUID = nameGenerator->GetSeriesUIDs();
        if (seriesUID.empty()) {
            Logger::instance().error("Aucune série DICOM trouvée");
            return false;
        }

        // Sélectionner la série avec le plus de fichiers
        std::string bestUID = seriesUID.front();
        size_t maxFiles = 0;
        for (const auto& uid : seriesUID) {
            auto files = nameGenerator->GetFileNames(uid);
            if (files.size() > maxFiles) {
                maxFiles = files.size();
                bestUID = uid;
            }
        }
        Logger::instance().info(QString("Série sélectionnée: %1 fichiers").arg(maxFiles));

        emit loadingProgress(30, QString("Chargement de %1 coupes...").arg(maxFiles));

        auto reader = itk::ImageSeriesReader<ImageType>::New();
        auto dicomIO = itk::GDCMImageIO::New();
        reader->SetImageIO(dicomIO);
        reader->SetFileNames(nameGenerator->GetFileNames(bestUID));
        reader->ForceOrthogonalDirectionOff();
        reader->Update();

        m_volume = reader->GetOutput();
        m_sourcePath = dirPath;
        m_isLoaded = true;
        m_name = QFileInfo(dirPath).baseName();

        emit loadingProgress(100, "Chargement terminé");
        emit volumeChanged();

        auto sz = getSize();
        auto sp = getSpacing();
        Logger::instance().info(QString("Volume: %1x%2x%3, espacement: %4x%5x%6 mm")
            .arg(sz.x()).arg(sz.y()).arg(sz.z())
            .arg(sp.x(), 0, 'f', 3).arg(sp.y(), 0, 'f', 3).arg(sp.z(), 0, 'f', 3));

        return true;

    } catch (const itk::ExceptionObject& ex) {
        Logger::instance().error(QString("Erreur ITK DICOM: %1").arg(ex.what()));
        return false;
    }
}

bool CBCTVolume::loadFromNIfTI(const QString& filePath)
{
    Logger::instance().info(QString("Chargement NIfTI: %1").arg(filePath));

    try {
        emit loadingProgress(0, "Lecture NIfTI...");

        auto reader = itk::ImageFileReader<ImageType>::New();
        reader->SetFileName(filePath.toStdString());
        reader->SetImageIO(itk::NiftiImageIO::New());

        emit loadingProgress(50, "Décodage...");
        reader->Update();

        m_volume = reader->GetOutput();
        m_sourcePath = filePath;
        m_name = QFileInfo(filePath).baseName();
        m_isLoaded = true;

        emit loadingProgress(100, "Terminé");
        emit volumeChanged();

        auto sz = getSize();
        auto sp = getSpacing();
        Logger::instance().info(QString("NIfTI: %1x%2x%3, espacement: %4x%5x%6 mm")
            .arg(sz.x()).arg(sz.y()).arg(sz.z())
            .arg(sp.x(), 0, 'f', 3).arg(sp.y(), 0, 'f', 3).arg(sp.z(), 0, 'f', 3));

        return true;

    } catch (const itk::ExceptionObject& ex) {
        Logger::instance().error(QString("Erreur NIfTI: %1").arg(ex.what()));
        return false;
    }
}

bool CBCTVolume::saveToNIfTI(const QString& filePath) const
{
    if (!m_isLoaded) return false;

    try {
        auto writer = itk::ImageFileWriter<ImageType>::New();
        writer->SetFileName(filePath.toStdString());
        writer->SetInput(m_volume);
        writer->Update();
        Logger::instance().info(QString("Sauvegardé: %1").arg(filePath));
        return true;
    } catch (const itk::ExceptionObject& ex) {
        Logger::instance().error(QString("Erreur sauvegarde: %1").arg(ex.what()));
        return false;
    }
}

void CBCTVolume::resampleIsotropic(double spacing_mm)
{
    if (!m_isLoaded) return;

    Logger::instance().info(QString("Resampling isotrope: %1 mm").arg(spacing_mm, 0, 'f', 3));

    auto resampler = itk::ResampleImageFilter<ImageType, ImageType>::New();

    auto interpolator = itk::BSplineInterpolateImageFunction<ImageType, double>::New();
    interpolator->SetSplineOrder(3);
    resampler->SetInterpolator(interpolator);

    ImageType::SpacingType newSpacing;
    newSpacing.Fill(spacing_mm);

    auto inputSize = m_volume->GetLargestPossibleRegion().GetSize();
    auto inputSpacing = m_volume->GetSpacing();

    ImageType::SizeType newSize;
    for (int i = 0; i < 3; ++i) {
        newSize[i] = static_cast<unsigned int>(inputSize[i] * inputSpacing[i] / spacing_mm);
    }

    resampler->SetInput(m_volume);
    resampler->SetSize(newSize);
    resampler->SetOutputSpacing(newSpacing);
    resampler->SetOutputOrigin(m_volume->GetOrigin());
    resampler->SetOutputDirection(m_volume->GetDirection());
    resampler->SetDefaultPixelValue(-1000);
    resampler->Update();

    m_volume = resampler->GetOutput();
    emit volumeChanged();
}

void CBCTVolume::normalizeIntensity()
{
    if (!m_isLoaded) return;

    auto stats = itk::StatisticsImageFilter<ImageType>::New();
    stats->SetInput(m_volume);
    stats->Update();

    float mean = stats->GetMean();
    float stdDev = stats->GetSigma();

    Logger::instance().info(QString("Normalisation: μ=%1, σ=%2").arg(mean, 0, 'f', 2).arg(stdDev, 0, 'f', 2));

    itk::ImageRegionIterator<ImageType> it(m_volume, m_volume->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        it.Set((it.Get() - mean) / stdDev);
    }
    emit volumeChanged();
}

void CBCTVolume::applyMedianFilter(int kernelSize)
{
    if (!m_isLoaded) return;

    Logger::instance().info(QString("Filtre médian %1x%1x%1").arg(kernelSize));

    auto median = itk::MedianImageFilter<ImageType, ImageType>::New();
    ImageType::SizeType radius;
    radius.Fill(kernelSize / 2);
    median->SetRadius(radius);
    median->SetInput(m_volume);
    median->Update();

    m_volume = median->GetOutput();
    emit volumeChanged();
}


Eigen::Vector3d CBCTVolume::getOrigin() const
{
    if (!m_isLoaded) return Eigen::Vector3d::Zero();
    auto o = m_volume->GetOrigin();
    return {o[0], o[1], o[2]};
}

Eigen::Vector3d CBCTVolume::getSpacing() const
{
    if (!m_isLoaded) return Eigen::Vector3d::Ones();
    auto s = m_volume->GetSpacing();
    return {s[0], s[1], s[2]};
}

Eigen::Vector3i CBCTVolume::getSize() const
{
    if (!m_isLoaded) return Eigen::Vector3i::Zero();
    auto s = m_volume->GetLargestPossibleRegion().GetSize();
    return {static_cast<int>(s[0]), static_cast<int>(s[1]), static_cast<int>(s[2])};
}

Eigen::Matrix3d CBCTVolume::getDirection() const
{
    if (!m_isLoaded) return Eigen::Matrix3d::Identity();
    auto dir = m_volume->GetDirection();
    Eigen::Matrix3d result;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            result(i, j) = dir(i, j);
    return result;
}

float CBCTVolume::getVoxelValue(int x, int y, int z) const
{
    if (!m_isLoaded) return 0.0f;
    ImageType::IndexType index = {{x, y, z}};
    if (!m_volume->GetLargestPossibleRegion().IsInside(index))
        return -1000.0f;
    return m_volume->GetPixel(index);
}

float CBCTVolume::getInterpolatedValue(const Eigen::Vector3d& physicalPoint) const
{
    if (!m_isLoaded) return 0.0f;

    auto interpolator = itk::LinearInterpolateImageFunction<ImageType, double>::New();
    interpolator->SetInputImage(m_volume);

    ImageType::PointType point;
    point[0] = physicalPoint.x();
    point[1] = physicalPoint.y();
    point[2] = physicalPoint.z();

    if (interpolator->IsInsideBuffer(point))
        return static_cast<float>(interpolator->Evaluate(point));
    return -1000.0f;
}

Eigen::Vector3i CBCTVolume::physicalToIndex(const Eigen::Vector3d& point) const
{
    if (!m_isLoaded) return Eigen::Vector3i::Zero();
    ImageType::PointType p; p[0] = point.x(); p[1] = point.y(); p[2] = point.z();
    ImageType::IndexType idx;
    m_volume->TransformPhysicalPointToIndex(p, idx);
    return {static_cast<int>(idx[0]), static_cast<int>(idx[1]), static_cast<int>(idx[2])};
}

Eigen::Vector3d CBCTVolume::indexToPhysical(const Eigen::Vector3i& index) const
{
    if (!m_isLoaded) return Eigen::Vector3d::Zero();
    ImageType::IndexType idx = {{index.x(), index.y(), index.z()}};
    ImageType::PointType point;
    m_volume->TransformIndexToPhysicalPoint(idx, point);
    return {point[0], point[1], point[2]};
}

void CBCTVolume::setITKImage(ImagePointer image)
{
    m_volume = image;
    m_isLoaded = (image != nullptr);
    emit volumeChanged();
}

CBCTVolume::Statistics CBCTVolume::computeStatistics() const
{
    Statistics s = {0, 0, 0, 0};
    if (!m_isLoaded) return s;

    auto stats = itk::StatisticsImageFilter<ImageType>::New();
    stats->SetInput(m_volume);
    stats->Update();

    s.min = stats->GetMinimum();
    s.max = stats->GetMaximum();
    s.mean = stats->GetMean();
    s.stdDev = stats->GetSigma();
    return s;
}

void CBCTVolume::histogramMatch(const CBCTVolume& reference)
{
    if (!m_isLoaded || !reference.isLoaded()) return;
    Logger::instance().info("Histogram Matching...");
    auto matcher = itk::HistogramMatchingImageFilter<ImageType, ImageType>::New();
    matcher->SetSourceImage(m_volume);
    matcher->SetReferenceImage(reference.getITKImage());
    matcher->SetNumberOfHistogramLevels(1024);
    matcher->SetNumberOfMatchPoints(7);
    matcher->ThresholdAtMeanIntensityOn();
    matcher->Update();
    m_volume = matcher->GetOutput();
    Logger::instance().info("Histogram Matching termine");
    emit volumeChanged();
}

} // namespace CBCTAlign
