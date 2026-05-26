/**
 * @file RigidRegistration.h
 * @brief Rigid 3D registration using Mattes Mutual Information
 *
 * VERSION 4 - Adapted for real CBCT data (MOULID_HICHAM T0↔T3, 11 months)
 * ========================================================================
 *
 * Changes vs v3:
 *   [A] Init method configurable: GEOMETRY | MOMENTS (default: MOMENTS)
 *       → Moments uses intensity-weighted center of mass, more robust
 *         for real longitudinal data with different patient positioning.
 *
 *   [B] Automatic Otsu mask on fixed image to exclude air
 *       → Air around patient has different noise between acquisitions.
 *         Without masking, MI degrades 10-30% on real data.
 *         Enabled by default (useFixedImageMask = true).
 *
 *   [C] Multi-level observer: tracks resolution level transitions
 *       → Debug which level causes divergence (was always 0 before).
 *
 *   [D] Transform export to ITK .tfm file
 *       → Required for reproducibility in SoftwareX publication.
 *
 *   [E] Parameters validation in align() prologue
 *       → Fails fast on inconsistent shrinkFactors / smoothingSigmas sizes.
 *
 *   [F] Diagnostics emitted via new signal levelChanged(int)
 *
 * All v3 fixes preserved:
 *   - SetDoEstimateLearningRateAtEachIteration(true)
 *   - samplingPercentage = 0.25
 *   - numberOfHistogramBins = 50
 *   - applyTransform() overload with reference grid
 */
#ifndef RIGIDREGISTRATION_H
#define RIGIDREGISTRATION_H

#include <QObject>
#include <memory>
#include <Eigen/Dense>
#include <itkImage.h>
#include <itkEuler3DTransform.h>
#include <itkCommand.h>

namespace CBCTAlign {

class CBCTVolume;
using ImageType = itk::Image<float, 3>;

struct RegistrationResult {
    Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();
    Eigen::Vector3d translation = Eigen::Vector3d::Zero();
    Eigen::Vector3d rotationAngles = Eigen::Vector3d::Zero();
    double finalMetric = 0.0;
    int iterations = 0;
    bool converged = false;
    QString errorMessage;

    // [V4] Diagnostics
    double initialMetric = 0.0;          // MI before optimization
    int numberOfLevelsCompleted = 0;     // Multi-resolution levels done
    double metricImprovement = 0.0;      // initial - final (positive = good)
};

class RigidRegistration : public QObject {
    Q_OBJECT

public:
    using TransformType = itk::Euler3DTransform<double>;
    using TransformPointer = TransformType::Pointer;
    using ImagePointer = ImageType::Pointer;

    /// [V4] Initialization method for transform center
    enum class InitMethod {
        Geometry,  // Align geometric centers of fixed/moving (original v3)
        Moments,   // Align intensity-weighted mass centers (recommended v4)
        Landmark   // [V6] Init from landmark-based translation (e.g. ANS)
    };

    struct Parameters {
        int numberOfHistogramBins = 50;
        double samplingPercentage = 0.25;
        int maxIterations = 200;
        double minStepLength = 0.001;
        double maxStepLength = 2.0;
        double relaxationFactor = 0.5;
        double convergenceThreshold = 1e-6;
        std::vector<int>    shrinkFactors   = {4, 2, 1};
        std::vector<double> smoothingSigmas = {2.0, 1.0, 0.0};

        // [V4] New fields
        InitMethod initMethod = InitMethod::Moments;
    Eigen::Vector3d initialTranslation = Eigen::Vector3d::Zero();  // V6: for Landmark init
        bool useFixedImageMask = true;     // Otsu mask on fixed image
        float maskAirThreshold = 0.0f;     // If > 0, use hard threshold
                                           // instead of Otsu (HU value).
                                           // Typical: -500.0f for HU data,
                                           //          200.0f for non-HU.
        bool saveTransformTfm = false;     // Save ITK .tfm file
        QString transformOutputPath;       // Output .tfm path if save enabled
    };

    explicit RigidRegistration(QObject* parent = nullptr);

    void setParameters(const Parameters& params) { m_params = params; }
    Parameters getParameters() const { return m_params; }

    RegistrationResult align(const CBCTVolume& fixed, const CBCTVolume& moving);

    // v3 API - backward compatible
    std::shared_ptr<CBCTVolume> applyTransform(
        const CBCTVolume& vol, const Eigen::Matrix4d& transform);
    std::shared_ptr<CBCTVolume> applyTransform(
        const CBCTVolume& vol, TransformPointer transform);

    // v3 API - apply transform with reference grid (critical for same-grid output)
    std::shared_ptr<CBCTVolume> applyTransform(
        const CBCTVolume& moving,
        const CBCTVolume& referenceVolume,
        const Eigen::Matrix4d& transform);
    std::shared_ptr<CBCTVolume> applyTransform(
        const CBCTVolume& moving,
        const CBCTVolume& referenceVolume,
        TransformPointer transform);

    void cancel() { m_cancelled = true; }

signals:
    void progressUpdated(int iteration, double metric, int level);
    void levelChanged(int newLevel);            // [V4] Multi-resolution transition
    void maskComputed(int voxelsInMask,         // [V4] Otsu mask stats
                      int totalVoxels,
                      float threshold);
    void registrationFinished(bool success);

private:
    Parameters m_params;
    bool m_cancelled = false;

    Eigen::Matrix4d transformToMatrix(TransformPointer transform) const;
    TransformPointer matrixToTransform(const Eigen::Matrix4d& matrix) const;

    /// [V4] Validates that Parameters are internally consistent
    bool validateParameters(QString& errorMsg) const;

    /// [V4] Internal observer tracking iterations AND level changes
    class RegistrationObserver : public itk::Command {
    public:
        using Self = RegistrationObserver;
        using Pointer = itk::SmartPointer<Self>;
        itkNewMacro(Self);

        void SetRegistration(RigidRegistration* reg) { m_registration = reg; }
        void SetOptimizer(const itk::Object* opt) { m_optimizer = opt; }

        void Execute(itk::Object* caller, const itk::EventObject& event) override;
        void Execute(const itk::Object* caller, const itk::EventObject& event) override;

    private:
        RigidRegistration*  m_registration = nullptr;
        const itk::Object*  m_optimizer = nullptr;
        int m_iteration = 0;
        int m_level = 0;
    };
};

} // namespace CBCTAlign
#endif
