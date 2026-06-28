/**
 * @file RigidRegistration.h
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


    double initialMetric = 0.0;          
    int numberOfLevelsCompleted = 0;   
    double metricImprovement = 0.0;     
};

class RigidRegistration : public QObject {
    Q_OBJECT

public:
    using TransformType = itk::Euler3DTransform<double>;
    using TransformPointer = TransformType::Pointer;
    using ImagePointer = ImageType::Pointer;


    enum class InitMethod {
        Geometry,  
        Moments,   
        Landmark  
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


        InitMethod initMethod = InitMethod::Moments;
    Eigen::Vector3d initialTranslation = Eigen::Vector3d::Zero();  
        bool useFixedImageMask = true;     
        float maskAirThreshold = 0.0f;    
                                           


        bool saveTransformTfm = false;     
        QString transformOutputPath;      
    };

    explicit RigidRegistration(QObject* parent = nullptr);

    void setParameters(const Parameters& params) { m_params = params; }
    Parameters getParameters() const { return m_params; }

    RegistrationResult align(const CBCTVolume& fixed, const CBCTVolume& moving);


    std::shared_ptr<CBCTVolume> applyTransform(
        const CBCTVolume& vol, const Eigen::Matrix4d& transform);
    std::shared_ptr<CBCTVolume> applyTransform(
        const CBCTVolume& vol, TransformPointer transform);


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
    void levelChanged(int newLevel);            
    void maskComputed(int voxelsInMask,       
                      int totalVoxels,
                      float threshold);
    void registrationFinished(bool success);

private:
    Parameters m_params;
    bool m_cancelled = false;

    Eigen::Matrix4d transformToMatrix(TransformPointer transform) const;
    TransformPointer matrixToTransform(const Eigen::Matrix4d& matrix) const;


    bool validateParameters(QString& errorMsg) const;


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
