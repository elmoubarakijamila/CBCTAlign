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
    explicit PipelineManager(QObject* parent = nullptr);

    void setLoadedVolumes(const std::vector<std::shared_ptr<CBCTVolume>>& volumes);

    void runFullPipeline(const QStringList& cbctPaths,
                         const QString& outputDir,
                         const QString& refLandmark = "Sella",  // MODIFIE: Sella par defaut
                         double sliceDistance = 0.0,
                         int slicesPerOrientation = 50,
                         double sliceRangeMm = 25.0);

    void cancel();

    /// Definir des landmarks manuels (bypass la detection auto)
    void setManualLandmarks(const std::vector<Landmark>& landmarks);
    /// V5: Set landmarks per timepoint (one vector per volume)
    void setManualLandmarksMulti(const std::vector<std::vector<Landmark>>& allTpLm);
    bool hasManualLandmarks() const { return m_useManualLandmarks; }

    /// Retourne le nom du point fixe effectivement utilise
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

    /// Trouve le meilleur point fixe parmi les landmarks disponibles
    /// Priorite: refLandmark demande > Sella > Nasion > premier disponible
    QString findBestRefLandmark(const QString& requested) const;

    std::vector<std::shared_ptr<CBCTVolume>> m_volumes;
    std::vector<std::shared_ptr<CBCTVolume>> m_alignedVolumes;
    std::vector<std::vector<Landmark>> m_landmarks;
    std::vector<Slice2D> m_slices;

    bool m_volumesPreloaded = false;
    bool m_cancelled = false;

    // Landmarks manuels
    bool m_useManualLandmarks = false;
    std::vector<Landmark> m_manualLandmarks;
    std::vector<std::vector<Landmark>> m_allTimepointLandmarks;  // V5
    std::vector<RegistrationResult> m_registrationResults;  // V7: store transforms  // V5: per-timepoint landmarks

    RigidRegistration m_registration;
    CephalometricDetector m_detector;
    SlicePlaneCalculator m_planeCalc;
    SliceExtractor m_extractor;
    ValidationMetrics m_validator;

    QString m_outputDir;
};

} // namespace CBCTAlign
#endif
