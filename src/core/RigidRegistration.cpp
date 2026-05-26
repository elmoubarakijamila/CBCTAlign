/**
 * @file RigidRegistration.cpp
 * @brief Rigid 3D registration implementation
 *
 * VERSION 4 - See header for full changelog
 */

#include "RigidRegistration.h"
#include "CBCTVolume.h"
#include "Logger.h"

// ITK core
#include <itkImageRegistrationMethodv4.h>
#include <itkMattesMutualInformationImageToImageMetricv4.h>
#include <itkRegularStepGradientDescentOptimizerv4.h>
#include <itkCenteredTransformInitializer.h>
#include <itkResampleImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkRegistrationParameterScalesFromPhysicalShift.h>

// [V4] ITK masking
#include <itkImageMaskSpatialObject.h>
#include <itkOtsuThresholdImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkBinaryFillholeImageFilter.h>
#include <itkBinaryErodeImageFilter.h>
#include <itkBinaryBallStructuringElement.h>
#include <itkImageRegionConstIterator.h>

// [V4] ITK transform I/O
#include <itkTransformFileWriter.h>

namespace CBCTAlign {

// Types used for masking
using MaskPixelType       = unsigned char;
using MaskImageType       = itk::Image<MaskPixelType, 3>;
using MaskSpatialObjectType = itk::ImageMaskSpatialObject<3>;

// -----------------------------------------------------------------------------
// Build a binary mask separating patient tissue from surrounding air.
// - Otsu thresholding when params.maskAirThreshold <= 0
// - Hard threshold when params.maskAirThreshold > 0 (HU-aware)
// - Hole filling + small erosion to avoid boundary noise
// -----------------------------------------------------------------------------
static MaskImageType::Pointer buildAirExclusionMask(
    ImageType::Pointer fixedImage,
    float hardThreshold,
    int& voxelsInMask,
    int& totalVoxels,
    float& thresholdUsed)
{
    MaskImageType::Pointer mask;

    if (hardThreshold > 0.0f) {
        // ---- Hard threshold path (HU-aware) --------------------------------
        using BinThreshType = itk::BinaryThresholdImageFilter<ImageType, MaskImageType>;
        auto bin = BinThreshType::New();
        bin->SetInput(fixedImage);
        bin->SetLowerThreshold(hardThreshold);
        bin->SetUpperThreshold(4096.0f);
        bin->SetInsideValue(1);
        bin->SetOutsideValue(0);
        bin->Update();
        mask = bin->GetOutput();
        thresholdUsed = hardThreshold;
    } else {
        // ---- Otsu path (data-driven) ---------------------------------------
        using OtsuType = itk::OtsuThresholdImageFilter<ImageType, MaskImageType>;
        auto otsu = OtsuType::New();
        otsu->SetInput(fixedImage);
        otsu->SetInsideValue(0);    // air -> 0
        otsu->SetOutsideValue(1);   // tissue -> 1
        otsu->Update();
        mask = otsu->GetOutput();
        thresholdUsed = static_cast<float>(otsu->GetThreshold());
    }

    // ---- Fill internal holes (air pockets inside sinuses, etc.) ------------
    using FillholeType = itk::BinaryFillholeImageFilter<MaskImageType>;
    auto fill = FillholeType::New();
    fill->SetInput(mask);
    fill->SetForegroundValue(1);
    fill->Update();
    mask = fill->GetOutput();

    // ---- Light erosion to avoid boundary voxels corrupting MI -------------
    using StructElementType = itk::BinaryBallStructuringElement<MaskPixelType, 3>;
    StructElementType ball;
    ball.SetRadius(1);  // 1 voxel = 1mm at 1mm spacing
    ball.CreateStructuringElement();

    using ErodeType = itk::BinaryErodeImageFilter<MaskImageType, MaskImageType, StructElementType>;
    auto erode = ErodeType::New();
    erode->SetInput(mask);
    erode->SetKernel(ball);
    erode->SetErodeValue(1);
    erode->Update();
    mask = erode->GetOutput();

    // ---- Stats for logging -------------------------------------------------
    voxelsInMask = 0;
    totalVoxels  = 0;
    itk::ImageRegionConstIterator<MaskImageType> it(
        mask, mask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        ++totalVoxels;
        if (it.Get() != 0) ++voxelsInMask;
    }

    return mask;
}

// Constructor
RigidRegistration::RigidRegistration(QObject* parent)
    : QObject(parent), m_cancelled(false)
{
}

// [V4] Parameters validation
bool RigidRegistration::validateParameters(QString& errorMsg) const
{
    if (m_params.shrinkFactors.empty()) {
        errorMsg = "shrinkFactors cannot be empty";
        return false;
    }
    if (m_params.shrinkFactors.size() != m_params.smoothingSigmas.size()) {
        errorMsg = QString("shrinkFactors (%1) and smoothingSigmas (%2) "
                           "must have same size")
            .arg(m_params.shrinkFactors.size())
            .arg(m_params.smoothingSigmas.size());
        return false;
    }
    if (m_params.samplingPercentage <= 0.0 || m_params.samplingPercentage > 1.0) {
        errorMsg = QString("samplingPercentage must be in (0, 1], got %1")
            .arg(m_params.samplingPercentage);
        return false;
    }
    if (m_params.numberOfHistogramBins < 10) {
        errorMsg = "numberOfHistogramBins too low (< 10)";
        return false;
    }
    if (m_params.saveTransformTfm && m_params.transformOutputPath.isEmpty()) {
        errorMsg = "saveTransformTfm enabled but transformOutputPath is empty";
        return false;
    }
    return true;
}

// Main align() function
RegistrationResult RigidRegistration::align(
    const CBCTVolume& fixed, const CBCTVolume& moving)
{
    RegistrationResult result;
    result.converged = false;
    m_cancelled = false;

    Logger::instance().info("=== Rigid Registration v4 ===");

    // [V4] Prologue checks -----------------------------------------------------
    QString paramErr;
    if (!validateParameters(paramErr)) {
        result.errorMessage = paramErr;
        Logger::instance().error(result.errorMessage);
        return result;
    }
    if (!fixed.isLoaded() || !moving.isLoaded()) {
        result.errorMessage = "Volumes not loaded";
        Logger::instance().error(result.errorMessage);
        return result;
    }

    try {
        ImagePointer fixedImage = fixed.getITKImage();
        ImagePointer movingImage = moving.getITKImage();

        // Log dimensions
        auto fs = fixedImage->GetLargestPossibleRegion().GetSize();
        auto fsp = fixedImage->GetSpacing();
        auto ms = movingImage->GetLargestPossibleRegion().GetSize();
        auto msp = movingImage->GetSpacing();
        Logger::instance().info(QString("  Fixed : %1x%2x%3, spacing=%4mm")
            .arg(fs[0]).arg(fs[1]).arg(fs[2]).arg(fsp[0], 0, 'f', 3));
        Logger::instance().info(QString("  Moving: %1x%2x%3, spacing=%4mm")
            .arg(ms[0]).arg(ms[1]).arg(ms[2]).arg(msp[0], 0, 'f', 3));

        // ----------------------------------------------------------------
        // Transform + initializer
        // ----------------------------------------------------------------
        auto transform = TransformType::New();

        using InitializerType = itk::CenteredTransformInitializer<
            TransformType, ImageType, ImageType>;
        auto initializer = InitializerType::New();
        initializer->SetTransform(transform);
        initializer->SetFixedImage(fixedImage);
        initializer->SetMovingImage(movingImage);

        // [V6] Configurable init method (Moments / Geometry / Landmark)
        if (m_params.initMethod == InitMethod::Landmark) {
            // V6: Direct translation from landmark vector (ANS_fixed - ANS_moving)
            // The transform center stays at the fixed image center for stability
            initializer->GeometryOn();
            initializer->InitializeTransform();  // sets center
            // Override translation with landmark-based vector
            TransformType::OutputVectorType lmTrans;
            lmTrans[0] = m_params.initialTranslation.x();
            lmTrans[1] = m_params.initialTranslation.y();
            lmTrans[2] = m_params.initialTranslation.z();
            transform->SetTranslation(lmTrans);
            Logger::instance().info("Init: LANDMARK (ANS-based translation)");
        } else {
            if (m_params.initMethod == InitMethod::Moments) {
                initializer->MomentsOn();
                Logger::instance().info("Init: MOMENTS "
                    "(intensity-weighted center of mass)");
            } else {
                initializer->GeometryOn();
                Logger::instance().info("Init: GEOMETRY (geometric center)");
            }
            initializer->InitializeTransform();
        }

        auto c = transform->GetCenter();
        auto t0 = transform->GetTranslation();
        Logger::instance().info(QString("  Center: [%1, %2, %3]")
            .arg(c[0], 0, 'f', 2).arg(c[1], 0, 'f', 2).arg(c[2], 0, 'f', 2));
        Logger::instance().info(QString("  Initial translation: [%1, %2, %3] mm")
            .arg(t0[0], 0, 'f', 3).arg(t0[1], 0, 'f', 3).arg(t0[2], 0, 'f', 3));

        // ----------------------------------------------------------------
        // Metric (Mattes MI)
        // ----------------------------------------------------------------
        using MetricType = itk::MattesMutualInformationImageToImageMetricv4<
            ImageType, ImageType>;
        auto metric = MetricType::New();
        metric->SetNumberOfHistogramBins(m_params.numberOfHistogramBins);

        // [V4] Otsu mask -------------------------------------------------
        MaskSpatialObjectType::Pointer maskSO;  // keep alive in scope
        if (m_params.useFixedImageMask) {
            Logger::instance().info("Building air exclusion mask on fixed image...");

            int vIn = 0, vTot = 0;
            float thrUsed = 0.0f;
            auto maskImg = buildAirExclusionMask(
                fixedImage, m_params.maskAirThreshold, vIn, vTot, thrUsed);

            maskSO = MaskSpatialObjectType::New();
            maskSO->SetImage(maskImg);
            maskSO->Update();
            metric->SetFixedImageMask(maskSO);

            double pct = (vTot > 0) ? (100.0 * vIn / vTot) : 0.0;
            Logger::instance().info(QString(
                "  Mask: %1/%2 voxels (%3%), threshold=%4")
                .arg(vIn).arg(vTot).arg(pct, 0, 'f', 1).arg(thrUsed, 0, 'f', 1));
            emit maskComputed(vIn, vTot, thrUsed);
        } else {
            Logger::instance().info("Fixed image mask: DISABLED");
        }

        // ----------------------------------------------------------------
        // Optimizer (Regular Step Gradient Descent)
        // ----------------------------------------------------------------
        using OptimizerType = itk::RegularStepGradientDescentOptimizerv4<double>;
        auto optimizer = OptimizerType::New();
        optimizer->SetLearningRate(m_params.maxStepLength);
        optimizer->SetMinimumStepLength(m_params.minStepLength);
        optimizer->SetRelaxationFactor(m_params.relaxationFactor);
        optimizer->SetNumberOfIterations(m_params.maxIterations);
        optimizer->SetGradientMagnitudeTolerance(m_params.convergenceThreshold);
        optimizer->SetDoEstimateLearningRateOnce(false);
        optimizer->SetDoEstimateLearningRateAtEachIteration(true);

        using ScalesEstimatorType =
            itk::RegistrationParameterScalesFromPhysicalShift<MetricType>;
        auto scalesEstimator = ScalesEstimatorType::New();
        scalesEstimator->SetMetric(metric);
        scalesEstimator->SetTransformForward(true);
        optimizer->SetScalesEstimator(scalesEstimator);

        // ----------------------------------------------------------------
        // Registration method (multi-resolution)
        // ----------------------------------------------------------------
        using RegistrationType = itk::ImageRegistrationMethodv4<
            ImageType, ImageType, TransformType>;
        auto registration = RegistrationType::New();

        registration->SetFixedImage(fixedImage);
        registration->SetMovingImage(movingImage);
        registration->SetMetric(metric);
        registration->SetOptimizer(optimizer);
        registration->SetInitialTransform(transform);

        const unsigned int numberOfLevels = m_params.shrinkFactors.size();

        RegistrationType::ShrinkFactorsArrayType shrinkFactorsPerLevel;
        shrinkFactorsPerLevel.SetSize(numberOfLevels);
        for (unsigned int i = 0; i < numberOfLevels; ++i)
            shrinkFactorsPerLevel[i] = m_params.shrinkFactors[i];

        RegistrationType::SmoothingSigmasArrayType smoothingSigmasPerLevel;
        smoothingSigmasPerLevel.SetSize(numberOfLevels);
        for (unsigned int i = 0; i < numberOfLevels; ++i)
            smoothingSigmasPerLevel[i] = m_params.smoothingSigmas[i];

        registration->SetNumberOfLevels(numberOfLevels);
        registration->SetShrinkFactorsPerLevel(shrinkFactorsPerLevel);
        registration->SetSmoothingSigmasPerLevel(smoothingSigmasPerLevel);
        registration->SetSmoothingSigmasAreSpecifiedInPhysicalUnits(true);

        registration->SetMetricSamplingStrategy(RegistrationType::RANDOM);
        registration->SetMetricSamplingPercentage(m_params.samplingPercentage);

        // ----------------------------------------------------------------
        // Observers (iteration + level)
        // ----------------------------------------------------------------
        auto observer = RegistrationObserver::New();
        observer->SetRegistration(this);
        observer->SetOptimizer(optimizer);
        optimizer->AddObserver(itk::IterationEvent(), observer);

        // [V4] Also observe level changes on the registration method
        registration->AddObserver(
            itk::MultiResolutionIterationEvent(), observer);

        // ----------------------------------------------------------------
        // Log params summary
        // ----------------------------------------------------------------
        Logger::instance().info(QString(
            "Params: bins=%1, sampling=%2%%, maxIter=%3, step=[%4,%5]")
            .arg(m_params.numberOfHistogramBins)
            .arg(m_params.samplingPercentage * 100, 0, 'f', 0)
            .arg(m_params.maxIterations)
            .arg(m_params.minStepLength, 0, 'f', 4)
            .arg(m_params.maxStepLength, 0, 'f', 1));

        QString levels;
        for (unsigned int i = 0; i < numberOfLevels; ++i) {
            levels += QString("(shrink=%1,sigma=%2)")
                .arg(m_params.shrinkFactors[i])
                .arg(m_params.smoothingSigmas[i], 0, 'f', 1);
            if (i+1 < numberOfLevels) levels += " → ";
        }
        Logger::instance().info(QString("Multi-resolution: %1").arg(levels));

        // [V4] Compute initial metric value (before optimization)
        double initialMI = 0.0;
        try {
            metric->SetFixedImage(fixedImage);
            metric->SetMovingImage(movingImage);
            metric->SetTransform(transform);
            metric->Initialize();
            initialMI = metric->GetValue();
            Logger::instance().info(QString("Initial MI = %1")
                .arg(initialMI, 0, 'f', 6));
        } catch (...) {
            Logger::instance().info("Initial MI evaluation skipped");
        }

        // ----------------------------------------------------------------
        // Run
        // ----------------------------------------------------------------
        Logger::instance().info("Running optimization...");
        registration->Update();

        // ----------------------------------------------------------------
        // Collect results
        // ----------------------------------------------------------------
        auto finalTransform = registration->GetModifiableTransform();

        result.transform = transformToMatrix(finalTransform);
        result.translation = Eigen::Vector3d(
            finalTransform->GetTranslation()[0],
            finalTransform->GetTranslation()[1],
            finalTransform->GetTranslation()[2]);
        result.rotationAngles = Eigen::Vector3d(
            finalTransform->GetAngleX(),
            finalTransform->GetAngleY(),
            finalTransform->GetAngleZ());
        result.finalMetric = optimizer->GetValue();
        result.iterations = optimizer->GetCurrentIteration();
        result.initialMetric = initialMI;
        result.metricImprovement = initialMI - result.finalMetric;
        result.numberOfLevelsCompleted = numberOfLevels;
        result.converged = true;

        // ----------------------------------------------------------------
        // [V4] Save .tfm file if requested
        // ----------------------------------------------------------------
        if (m_params.saveTransformTfm &&
            !m_params.transformOutputPath.isEmpty())
        {
            try {
                using TransformWriterType = itk::TransformFileWriterTemplate<double>;
                auto writer = TransformWriterType::New();
                writer->SetInput(finalTransform);
                writer->SetFileName(m_params.transformOutputPath.toStdString());
                writer->Update();
                Logger::instance().info(QString("Transform saved: %1")
                    .arg(m_params.transformOutputPath));
            } catch (const itk::ExceptionObject& ex) {
                Logger::instance().error(QString(
                    "Failed to save .tfm: %1").arg(ex.what()));
            }
        }

        // ----------------------------------------------------------------
        // Final logs
        // ----------------------------------------------------------------
        Logger::instance().info("=== Registration DONE ===");
        Logger::instance().info(QString("  MI initial = %1")
            .arg(result.initialMetric, 0, 'f', 6));
        Logger::instance().info(QString("  MI final   = %1")
            .arg(result.finalMetric, 0, 'f', 6));
        Logger::instance().info(QString("  Improvement = %1")
            .arg(result.metricImprovement, 0, 'f', 6));
        Logger::instance().info(QString("  Iterations  = %1")
            .arg(result.iterations));
        Logger::instance().info(QString("  Translation = [%1, %2, %3] mm")
            .arg(result.translation.x(), 0, 'f', 3)
            .arg(result.translation.y(), 0, 'f', 3)
            .arg(result.translation.z(), 0, 'f', 3));
        Logger::instance().info(QString("  Rotation    = [%1, %2, %3] deg")
            .arg(result.rotationAngles.x() * 180.0 / M_PI, 0, 'f', 3)
            .arg(result.rotationAngles.y() * 180.0 / M_PI, 0, 'f', 3)
            .arg(result.rotationAngles.z() * 180.0 / M_PI, 0, 'f', 3));

        emit registrationFinished(true);

    } catch (const itk::ExceptionObject& ex) {
        result.errorMessage = QString::fromStdString(ex.what());
        Logger::instance().error(QString("Registration error: %1")
            .arg(result.errorMessage));
        emit registrationFinished(false);
    }

    return result;
}

// applyTransform - v3 API (uses moving grid as reference)
std::shared_ptr<CBCTVolume> RigidRegistration::applyTransform(
    const CBCTVolume& vol, const Eigen::Matrix4d& transform)
{
    return applyTransform(vol, matrixToTransform(transform));
}

std::shared_ptr<CBCTVolume> RigidRegistration::applyTransform(
    const CBCTVolume& vol, TransformPointer transform)
{
    if (!vol.isLoaded()) return nullptr;
    Logger::instance().info("Apply transform (moving grid)...");

    using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
    auto resampler = ResampleFilterType::New();

    using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;
    auto interpolator = InterpolatorType::New();

    ImagePointer inputImage = vol.getITKImage();
    resampler->SetInput(inputImage);
    resampler->SetInterpolator(interpolator);
    resampler->SetTransform(transform);
    resampler->SetReferenceImage(inputImage);
    resampler->UseReferenceImageOn();
    resampler->SetDefaultPixelValue(-1000);
    resampler->Update();

    auto result = std::make_shared<CBCTVolume>();
    result->setITKImage(resampler->GetOutput());
    result->setName(vol.getName() + "_aligned");
    Logger::instance().info("Transform applied (moving grid)");
    return result;
}

// applyTransform - v3 overload with fixed reference grid (critical!)
std::shared_ptr<CBCTVolume> RigidRegistration::applyTransform(
    const CBCTVolume& moving,
    const CBCTVolume& referenceVolume,
    const Eigen::Matrix4d& transform)
{
    return applyTransform(moving, referenceVolume, matrixToTransform(transform));
}

std::shared_ptr<CBCTVolume> RigidRegistration::applyTransform(
    const CBCTVolume& moving,
    const CBCTVolume& referenceVolume,
    TransformPointer transform)
{
    if (!moving.isLoaded() || !referenceVolume.isLoaded()) return nullptr;

    Logger::instance().info("Apply transform (REFERENCE grid = fixed)...");

    using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
    auto resampler = ResampleFilterType::New();

    using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;
    auto interpolator = InterpolatorType::New();

    ImagePointer movingImage = moving.getITKImage();
    ImagePointer refImage = referenceVolume.getITKImage();

    resampler->SetInput(movingImage);
    resampler->SetInterpolator(interpolator);
    resampler->SetTransform(transform);
    resampler->SetReferenceImage(refImage);
    resampler->UseReferenceImageOn();
    resampler->SetDefaultPixelValue(-1000);
    resampler->Update();

    auto result = std::make_shared<CBCTVolume>();
    result->setITKImage(resampler->GetOutput());
    result->setName(moving.getName() + "_aligned");

    auto outSize = resampler->GetOutput()->GetLargestPossibleRegion().GetSize();
    auto outOrigin = resampler->GetOutput()->GetOrigin();
    Logger::instance().info(QString("  Output: %1x%2x%3, origin=[%4,%5,%6]")
        .arg(outSize[0]).arg(outSize[1]).arg(outSize[2])
        .arg(outOrigin[0], 0, 'f', 2)
        .arg(outOrigin[1], 0, 'f', 2)
        .arg(outOrigin[2], 0, 'f', 2));
    return result;
}

// Eigen <-> ITK conversions
Eigen::Matrix4d RigidRegistration::transformToMatrix(
    TransformPointer transform) const
{
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    auto itkMatrix = transform->GetMatrix();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            matrix(i, j) = itkMatrix(i, j);
    auto offset = transform->GetOffset();
    matrix(0, 3) = offset[0];
    matrix(1, 3) = offset[1];
    matrix(2, 3) = offset[2];
    return matrix;
}

RigidRegistration::TransformPointer RigidRegistration::matrixToTransform(
    const Eigen::Matrix4d& matrix) const
{
    auto transform = TransformType::New();
    TransformType::MatrixType rotation;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            rotation(i, j) = matrix(i, j);
    TransformType::OutputVectorType translation;
    translation[0] = matrix(0, 3);
    translation[1] = matrix(1, 3);
    translation[2] = matrix(2, 3);
    transform->SetMatrix(rotation);
    transform->SetTranslation(translation);
    return transform;
}

// Observer - handles both IterationEvent and MultiResolutionIterationEvent
void RigidRegistration::RegistrationObserver::Execute(
    itk::Object* caller, const itk::EventObject& event)
{
    Execute(static_cast<const itk::Object*>(caller), event);
}

void RigidRegistration::RegistrationObserver::Execute(
    const itk::Object* caller, const itk::EventObject& event)
{
    // [V4] Handle level transitions (from Registration method)
    if (itk::MultiResolutionIterationEvent().CheckEvent(&event)) {
        ++m_level;
        m_iteration = 0;  // reset iter counter per level
        Logger::instance().info(QString(
            "  --- Multi-resolution level %1 started ---").arg(m_level));
        if (m_registration) {
            emit m_registration->levelChanged(m_level);
        }
        return;
    }

    // Handle optimizer iteration events
    if (!itk::IterationEvent().CheckEvent(&event)) return;
    if (!m_optimizer) return;

    auto optimizer = dynamic_cast<
        const itk::RegularStepGradientDescentOptimizerv4<double>*>(m_optimizer);
    if (!optimizer) return;

    ++m_iteration;
    double metric = optimizer->GetValue();
    double stepLength = optimizer->GetCurrentStepLength();

    if (m_registration) {
        emit m_registration->progressUpdated(m_iteration, metric, m_level);
    }

    if (m_iteration % 10 == 0) {
        Logger::instance().info(QString(
            "  [L%1] Iter %2: MI = %3, step = %4")
            .arg(m_level).arg(m_iteration)
            .arg(metric, 0, 'f', 6).arg(stepLength, 0, 'f', 6));
    }
}

} // namespace CBCTAlign
