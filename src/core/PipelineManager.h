#ifndef PIPELINEMANAGER_H
#define PIPELINEMANAGER_H

#include <QObject>
#include <QStringList>
#include <memory>
#include <vector>

#include "CBCTVolume.h"
#include "RigidRegistration.h"
#include "CephalometricDetector.h"
#include "SlicePlaneCalculator.h"
#include "SliceExtractor.h"
#include "ValidationMetrics.h"

namespace CBCTAlign {

enum class PipelineStage {
    Loading, Preprocessing, Registration,
    LandmarkDetection, SliceExtraction, Validation, Finished
};

class PipelineManager : public QObject
{
    Q_OBJECT

public:

    struct RegistrationReport {
        int    timepoint = 0;          
        double deltaLandmarkRaw = 0.0; 
        double deltaLandmark = -1.0;  
        double finalMetric = 0.0;     
        int    iterations = 0;
        bool   converged = false;
        qint64 elapsedMs = 0;          
    };

    struct ExtractionReport {
        int    timepoints = 0;
        int    slicesPerOrientation = 0;
        int    totalSlices = 0;
        QString refLandmark;
        QString outputDir;
        bool   ok = false;
    };
    explicit PipelineManager(QObject* parent = nullptr);

    void setLoadedVolumes(const std::vector<std::shared_ptr<CBCTVolume>>& volumes);

    void runFullPipeline(const QStringList& cbctPaths,
                         const QString& outputDir,
                         const QString& refLandmark = "Sella",  
                         double sliceDistance = 0.0,
                         int slicesPerOrientation = 50,
                         double sliceRangeMm = 25.0);

    void cancel();


    void setManualLandmarks(const std::vector<Landmark>& landmarks);

    void setManualLandmarksMulti(const std::vector<std::vector<Landmark>>& allTpLm);


    bool runRegistrationOnly(const QString& refLandmark = "ANS",
                             double deltaThresholdMm = 2.0);

    ExtractionReport runExtractionOnly(const QString& outputDir,
                                       const QString& refLandmark = "ANS",
                                       int slicesPerOrientation = 50,
                                       double sliceRangeMm = 25.0);


    std::vector<RegistrationReport> getRegistrationReports() const { return m_registrationReports; }
    bool hasManualLandmarks() const { return m_useManualLandmarks; }


    QString getEffectiveRefLandmark() const;

    std::vector<std::shared_ptr<CBCTVolume>> getAlignedVolumes() const { return m_alignedVolumes; }
    std::vector<Slice2D> getSlices() const { return m_slices; }
    std::vector<std::vector<Landmark>> getLandmarks() const { return m_landmarks; }

signals:
    void stageStarted(PipelineStage stage);
    void stageCompleted(PipelineStage stage, bool success);
    void progressUpdated(int percent, const QString& message);
    void pipelineFinished(bool success);

private:
    bool loadVolumes(const QStringList& paths);
    bool preprocessVolumes();
    bool registerVolumes();
    bool detectLandmarks();
    bool extractSlices(const QString& refLandmark, int numSlices, double rangeMm);
    bool validateSlices();
    bool saveResults(const QString& outputDir);


    QString findBestRefLandmark(const QString& requested) const;

    std::vector<std::shared_ptr<CBCTVolume>> m_volumes;
    std::vector<std::shared_ptr<CBCTVolume>> m_alignedVolumes;
    std::vector<std::vector<Landmark>> m_landmarks;
    std::vector<Slice2D> m_slices;

    bool m_volumesPreloaded = false;
    bool m_cancelled = false;


    bool m_useManualLandmarks = false;
    std::vector<Landmark> m_manualLandmarks;
    std::vector<std::vector<Landmark>> m_allTimepointLandmarks;  
    std::vector<RegistrationResult> m_registrationResults;      


    std::vector<RegistrationReport> m_registrationReports;
    double m_deltaThresholdMm = 2.0;

    RigidRegistration m_registration;
    CephalometricDetector m_detector;
    SlicePlaneCalculator m_planeCalc;
    SliceExtractor m_extractor;
    ValidationMetrics m_validator;
    double m_lastAlignedDeltaP = -1.0;  

    QString m_outputDir;
};

} // namespace CBCTAlign
#endif
