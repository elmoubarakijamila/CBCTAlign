/**
 * @file PipelineManager.cpp
 */
#include "PipelineManager.h"
#include "Logger.h"
#include "SliceNormalizer.h"
#include <QMap>
#include <QPair>
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace CBCTAlign {

PipelineManager::PipelineManager(QObject* parent)
    : QObject(parent)
{
}

void PipelineManager::setManualLandmarks(const std::vector<Landmark>& landmarks)
{
    m_manualLandmarks = landmarks;
    m_useManualLandmarks = true;


    for (const auto& lm : landmarks) {
        if (lm.abbreviation == "S" || lm.name == "Sella") {
            Logger::instance().info(
                QString("[MANUAL] Landmarks manuels definis - Sella: [%1, %2, %3]")
                    .arg(lm.position.x(), 0, 'f', 2)
                    .arg(lm.position.y(), 0, 'f', 2)
                    .arg(lm.position.z(), 0, 'f', 2));
            return;
        }
    }

    for (const auto& lm : landmarks) {
        if (lm.abbreviation == "N" || lm.name == "Nasion") {
            Logger::instance().info(
                QString("[MANUAL] Landmarks manuels definis - Nasion: [%1, %2, %3]")
                    .arg(lm.position.x(), 0, 'f', 2)
                    .arg(lm.position.y(), 0, 'f', 2)
                    .arg(lm.position.z(), 0, 'f', 2));
            return;
        }
    }

    if (!landmarks.empty()) {
        Logger::instance().info(
            QString("[MANUAL] Landmarks manuels definis - %1 landmarks, premier: %2")
                .arg(landmarks.size()).arg(landmarks[0].name));
    }
}


void PipelineManager::setManualLandmarksMulti(
    const std::vector<std::vector<Landmark>>& allTpLm)
{
    m_allTimepointLandmarks = allTpLm;
    if (!allTpLm.empty()) {
        m_manualLandmarks = allTpLm[0];  
        m_useManualLandmarks = true;
        Logger::instance().info(QString(
            "[V5] Multi-timepoint landmarks: %1 timepoints").arg(allTpLm.size()));
    }
}

void PipelineManager::setLoadedVolumes(const std::vector<std::shared_ptr<CBCTVolume>>& volumes)
{
    m_volumes = volumes;
    m_volumesPreloaded = true;

    for (size_t i = 0; i < m_volumes.size(); ++i) {
        m_volumes[i]->setName(QString("T%1").arg(i));
    }
    Logger::instance().info(QString("Pipeline: %1 volumes pre-charges").arg(m_volumes.size()));
}


QString PipelineManager::findBestRefLandmark(const QString& requested) const
{
    if (m_landmarks.empty() || m_landmarks[0].empty())
        return requested;

    const auto& lmsT0 = m_landmarks[0];


    for (const auto& lm : lmsT0) {
        if (lm.name.contains(requested, Qt::CaseInsensitive) ||
            lm.abbreviation.contains(requested, Qt::CaseInsensitive)) {
            return lm.name;
        }
    }


    for (const auto& lm : lmsT0) {
        if (lm.name == "ANS") {
            Logger::instance().info("Utilisation de ANS comme point fixe (fallback)");
            return "ANS";
        }
    }


    for (const auto& lm : lmsT0) {
        if (lm.name == "Sella" || lm.abbreviation == "S") {
            return "Sella";
        }
    }


    for (const auto& lm : lmsT0) {
        if (lm.name == "Nasion" || lm.abbreviation == "N") {
            return "Nasion";
        }
    }

    return lmsT0[0].name;
}

QString PipelineManager::getEffectiveRefLandmark() const
{
    if (m_landmarks.empty() || m_landmarks[0].empty())
        return "Sella";


    for (const auto& lm : m_landmarks[0]) {
        if (lm.name == "Sella" || lm.abbreviation == "S")
            return "Sella";
    }
    for (const auto& lm : m_landmarks[0]) {
        if (lm.name == "Nasion" || lm.abbreviation == "N")
            return "Nasion";
    }
    return m_landmarks[0][0].name;
}



void PipelineManager::runFullPipeline(const QStringList& cbctPaths,
                                       const QString& outputDir,
                                       const QString& refLandmark,
                                       double /*sliceDistance*/,
                                       int slicesPerOrientation,
                                       double sliceRangeMm)
{
    m_cancelled = false;
    m_outputDir = outputDir;
    Logger::instance().info("========== DEMARRAGE PIPELINE ==========");
    Logger::instance().info(QString("Point fixe demande: %1").arg(refLandmark));
    QDir().mkpath(outputDir);


    emit stageStarted(PipelineStage::Loading);
    if (!m_volumesPreloaded && !loadVolumes(cbctPaths)) {
        emit stageCompleted(PipelineStage::Loading, false);
        emit pipelineFinished(false);
        return;
    }
    emit stageCompleted(PipelineStage::Loading, true);
    if (m_cancelled) { emit pipelineFinished(false); return; }


    emit stageStarted(PipelineStage::Preprocessing);
    if (!preprocessVolumes()) {
        emit stageCompleted(PipelineStage::Preprocessing, false);
        emit pipelineFinished(false);
        return;
    }
    emit stageCompleted(PipelineStage::Preprocessing, true);
    if (m_cancelled) { emit pipelineFinished(false); return; }


    emit stageStarted(PipelineStage::Registration);
    if (!registerVolumes()) {
        emit stageCompleted(PipelineStage::Registration, false);
        emit pipelineFinished(false);
        return;
    }
    emit stageCompleted(PipelineStage::Registration, true);
    if (m_cancelled) { emit pipelineFinished(false); return; }


    emit stageStarted(PipelineStage::LandmarkDetection);
    if (!detectLandmarks()) {
        emit stageCompleted(PipelineStage::LandmarkDetection, false);
        emit pipelineFinished(false);
        return;
    }
    emit stageCompleted(PipelineStage::LandmarkDetection, true);
    if (m_cancelled) { emit pipelineFinished(false); return; }


    QString effectiveRef = findBestRefLandmark(refLandmark);
    Logger::instance().info(QString("Point fixe effectif: %1").arg(effectiveRef));


    emit stageStarted(PipelineStage::SliceExtraction);
    if (!extractSlices(effectiveRef, slicesPerOrientation, sliceRangeMm)) {
        emit stageCompleted(PipelineStage::SliceExtraction, false);
        emit pipelineFinished(false);
        return;
    }
    emit stageCompleted(PipelineStage::SliceExtraction, true);


    emit stageStarted(PipelineStage::Validation);
    validateSlices();
    saveResults(outputDir);
    emit stageCompleted(PipelineStage::Validation, true);

    m_volumesPreloaded = false;
    Logger::instance().info("========== PIPELINE TERMINE ==========");
    emit pipelineFinished(true);
}

void PipelineManager::cancel()
{
    m_cancelled = true;
    m_registration.cancel();
    Logger::instance().info("Pipeline annule");
}

bool PipelineManager::loadVolumes(const QStringList& paths)
{
    Logger::instance().info(QString("Chargement de %1 volumes...").arg(paths.size()));
    m_volumes.clear();

    for (int i = 0; i < paths.size(); ++i) {
        emit progressUpdated(i * 100 / paths.size(),
                             QString("Chargement %1...").arg(QFileInfo(paths[i]).fileName()));

        auto vol = std::make_shared<CBCTVolume>();
        QFileInfo fi(paths[i]);

        bool ok = fi.isDir()
            ? vol->loadFromDICOM(paths[i])
            : vol->loadFromNIfTI(paths[i]);

        if (ok) {
            vol->setName(QString("T%1").arg(i));
            m_volumes.push_back(vol);
        } else {
            Logger::instance().error(QString("Echec chargement: %1").arg(paths[i]));
            return false;
        }
    }
    return !m_volumes.empty();
}

bool PipelineManager::preprocessVolumes()
{
    if (m_volumes.empty()) {
        Logger::instance().error("Aucun volume a pretraiter");
        return false;
    }

    for (size_t i = 0; i < m_volumes.size(); ++i) {
        emit progressUpdated(static_cast<int>(i * 100 / m_volumes.size()),
                             QString("Pretraitement %1...").arg(m_volumes[i]->getName()));
        m_volumes[i]->applyMedianFilter(3);
    }
    return true;
}


bool PipelineManager::registerVolumes()
{
    if (m_volumes.size() < 2) {
        Logger::instance().error("Il faut au moins 2 volumes");
        return false;
    }

    Logger::instance().info(QString("Registration: reference = %1").arg(m_volumes[0]->getName()));


    auto refOrigin = m_volumes[0]->getOrigin();
    auto refSpacing = m_volumes[0]->getSpacing();
    auto refSize = m_volumes[0]->getSize();
    Logger::instance().info(QString("  Grille T0: %1x%2x%3, spacing=[%4,%5,%6], origin=[%7,%8,%9]")
        .arg(refSize.x()).arg(refSize.y()).arg(refSize.z())
        .arg(refSpacing.x(), 0, 'f', 3).arg(refSpacing.y(), 0, 'f', 3).arg(refSpacing.z(), 0, 'f', 3)
        .arg(refOrigin.x(), 0, 'f', 2).arg(refOrigin.y(), 0, 'f', 2).arg(refOrigin.z(), 0, 'f', 2));

    m_alignedVolumes.clear();
    m_alignedVolumes.push_back(m_volumes[0]);
    m_registrationResults.clear();  

    for (size_t i = 1; i < m_volumes.size(); ++i) {
        emit progressUpdated(static_cast<int>(i * 100 / m_volumes.size()),
                             QString("Registration T%1 -> T0...").arg(i));


        auto tiOrigin = m_volumes[i]->getOrigin();
        auto tiSize = m_volumes[i]->getSize();
        Logger::instance().info(QString("  Grille T%1: %2x%3x%4, origin=[%5,%6,%7]")
            .arg(i).arg(tiSize.x()).arg(tiSize.y()).arg(tiSize.z())
            .arg(tiOrigin.x(), 0, 'f', 2).arg(tiOrigin.y(), 0, 'f', 2).arg(tiOrigin.z(), 0, 'f', 2));


        if (!m_allTimepointLandmarks.empty() &&
            i < m_allTimepointLandmarks.size() &&
            !m_allTimepointLandmarks[0].empty() &&
            !m_allTimepointLandmarks[i].empty()) {
            Eigen::Vector3d ans_T0 = m_allTimepointLandmarks[0][0].position;
            Eigen::Vector3d ans_Ti = m_allTimepointLandmarks[i][0].position;
            Eigen::Vector3d initTrans = ans_Ti - ans_T0;

            auto params = m_registration.getParameters();
            params.initMethod = RigidRegistration::InitMethod::Landmark;
            params.initialTranslation = initTrans;
            m_registration.setParameters(params);

            Logger::instance().info(
                QString("  [V6] Landmark init: ANS_T0=[%1,%2,%3] ANS_T%4=[%5,%6,%7]")
                    .arg(ans_T0.x(), 0, 'f', 2).arg(ans_T0.y(), 0, 'f', 2).arg(ans_T0.z(), 0, 'f', 2)
                    .arg(i)
                    .arg(ans_Ti.x(), 0, 'f', 2).arg(ans_Ti.y(), 0, 'f', 2).arg(ans_Ti.z(), 0, 'f', 2));
            Logger::instance().info(
                QString("  [V6] Initial translation = [%1, %2, %3] mm")
                    .arg(initTrans.x(), 0, 'f', 2).arg(initTrans.y(), 0, 'f', 2).arg(initTrans.z(), 0, 'f', 2));
        }

        auto result = m_registration.align(*m_volumes[0], *m_volumes[i]);

        if (result.converged) {
            m_registrationResults.push_back(result); 

            auto aligned = m_registration.applyTransform(
                *m_volumes[i], *m_volumes[0], result.transform);

            aligned->setName(QString("T%1_aligned").arg(i));
            m_alignedVolumes.push_back(aligned);

            Logger::instance().info(QString("T%1 aligne sur grille T0: MI=%2, iter=%3")
                .arg(i).arg(result.finalMetric, 0, 'f', 4).arg(result.iterations));


            auto alignedOrigin = aligned->getOrigin();
            auto alignedSize = aligned->getSize();
            Logger::instance().info(QString("  T%1_aligned: %2x%3x%4, origin=[%5,%6,%7] (doit = T0)")
                .arg(i).arg(alignedSize.x()).arg(alignedSize.y()).arg(alignedSize.z())
                .arg(alignedOrigin.x(), 0, 'f', 2)
                .arg(alignedOrigin.y(), 0, 'f', 2)
                .arg(alignedOrigin.z(), 0, 'f', 2));
        } else {
            Logger::instance().error(QString("Echec registration T%1").arg(i));
            return false;
        }
        if (m_cancelled) return false;
    }
    return true;
}

bool PipelineManager::detectLandmarks()
{
    Logger::instance().info("=== Detection cephalometrique ===");
    m_landmarks.clear();

    if (m_useManualLandmarks) {
        Logger::instance().info("[MANUAL/ALI_CBCT] Utilisation des landmarks fournis");
        for (const auto& lm : m_manualLandmarks) {
            Logger::instance().info(QString("  %1 (%2) = [%3, %4, %5] conf=%6")
                .arg(lm.name).arg(lm.abbreviation)
                .arg(lm.position.x(), 0, 'f', 2)
                .arg(lm.position.y(), 0, 'f', 2)
                .arg(lm.position.z(), 0, 'f', 2)
                .arg(lm.confidence, 0, 'f', 2));
        }

        if (!m_allTimepointLandmarks.empty() &&
            m_allTimepointLandmarks.size() == m_alignedVolumes.size()) {
            m_landmarks = m_allTimepointLandmarks;
            Logger::instance().info(
                QString("[V5] Using per-timepoint landmarks (%1 sets)")
                    .arg(m_allTimepointLandmarks.size()));
        } else {

            for (size_t i = 0; i < m_alignedVolumes.size(); ++i) {
                m_landmarks.push_back(m_manualLandmarks);
            }
        }
        return true;
    }


    for (size_t i = 0; i < m_alignedVolumes.size(); ++i) {
        emit progressUpdated(static_cast<int>(i * 100 / m_alignedVolumes.size()),
                             QString("Landmarks %1...").arg(m_alignedVolumes[i]->getName()));

        auto lms = m_detector.detectAll(*m_alignedVolumes[i]);
        m_landmarks.push_back(lms);

        Logger::instance().info(QString("%1: %2 landmarks")
            .arg(m_alignedVolumes[i]->getName()).arg(lms.size()));

        for (const auto& lm : lms) {
            Logger::instance().info(QString("  %1: [%2, %3, %4] conf=%5")
                .arg(lm.name)
                .arg(lm.position.x(), 0, 'f', 2).arg(lm.position.y(), 0, 'f', 2)
                .arg(lm.position.z(), 0, 'f', 2).arg(lm.confidence, 0, 'f', 2));
        }
    }


    if (m_landmarks.size() >= 2) {
        Logger::instance().info("=== Ecarts landmarks post-registration ===");
        size_t n = std::min(m_landmarks[0].size(), m_landmarks[1].size());
        for (size_t j = 0; j < n; ++j) {
            double dist = (m_landmarks[0][j].position - m_landmarks[1][j].position).norm();
            Logger::instance().info(QString("  %1: dT0-T1 = %2 mm %3")
                .arg(m_landmarks[0][j].name)
                .arg(dist, 0, 'f', 2)
                .arg(dist < 2.0 ? "OK" : dist < 5.0 ? "WARN" : "FAIL"));
        }
    }

    return !m_landmarks.empty();
}


bool PipelineManager::extractSlices(const QString& refLandmark,
                                     int numSlices,
                                     double rangeMm)
{
    Logger::instance().info(QString("=== Extraction coupes (ref=%1, N=%2, range=+/-%3mm) ===")
        .arg(refLandmark).arg(numSlices).arg(rangeMm));

    m_slices.clear();

    if (m_landmarks.empty()) {
        Logger::instance().error("Pas de landmarks detectes");
        return false;
    }


    Landmark refLmT0;
    bool found = false;
    for (const auto& lm : m_landmarks[0]) {
        if (lm.name.contains(refLandmark, Qt::CaseInsensitive) ||
            lm.abbreviation.contains(refLandmark, Qt::CaseInsensitive)) {
            refLmT0 = lm;
            found = true;
            break;
        }
    }

    if (!found) {
        if (!m_landmarks[0].empty()) {
            refLmT0 = m_landmarks[0][0];
            Logger::instance().warning(QString("Landmark '%1' non trouve dans T0, utilisation de '%2'")
                .arg(refLandmark).arg(refLmT0.name));
        } else {
            Logger::instance().error("Aucun landmark dans T0");
            return false;
        }
    }

    Logger::instance().info(QString("Point fixe UNIQUE (T0): %1 (%2) [%3, %4, %5]")
        .arg(refLmT0.name).arg(refLmT0.abbreviation)
        .arg(refLmT0.position.x(), 0, 'f', 2)
        .arg(refLmT0.position.y(), 0, 'f', 2)
        .arg(refLmT0.position.z(), 0, 'f', 2));

    for (size_t i = 0; i < m_alignedVolumes.size(); ++i) {
        auto org = m_alignedVolumes[i]->getOrigin();
        auto sz = m_alignedVolumes[i]->getSize();
        auto sp = m_alignedVolumes[i]->getSpacing();
        Eigen::Vector3d maxPhys = org + Eigen::Vector3d(
            (sz.x()-1) * sp.x(), (sz.y()-1) * sp.y(), (sz.z()-1) * sp.z());
        Logger::instance().info(QString("  %1: origin=[%2,%3,%4], max=[%5,%6,%7], size=%8x%9x%10")
            .arg(m_alignedVolumes[i]->getName())
            .arg(org.x(), 0, 'f', 2).arg(org.y(), 0, 'f', 2).arg(org.z(), 0, 'f', 2)
            .arg(maxPhys.x(), 0, 'f', 2).arg(maxPhys.y(), 0, 'f', 2).arg(maxPhys.z(), 0, 'f', 2)
            .arg(sz.x()).arg(sz.y()).arg(sz.z()));
    }


    auto refOrg = m_alignedVolumes[0]->getOrigin();
    auto refSz = m_alignedVolumes[0]->getSize();
    auto refSp = m_alignedVolumes[0]->getSpacing();
    Eigen::Vector3d refMax = refOrg + Eigen::Vector3d(
        (refSz.x()-1) * refSp.x(), (refSz.y()-1) * refSp.y(), (refSz.z()-1) * refSp.z());


    double sellaCoords[3] = {refLmT0.position.x(), refLmT0.position.y(), refLmT0.position.z()};
    double orgCoords[3] = {refOrg.x(), refOrg.y(), refOrg.z()};
    double maxCoords[3] = {refMax.x(), refMax.y(), refMax.z()};
    QString axisNames[3] = {"X(Sagittal)", "Y(Coronal)", "Z(Axial)"};

    for (int a = 0; a < 3; ++a) {
        double lo = sellaCoords[a] - rangeMm;
        double hi = sellaCoords[a] + rangeMm;
        if (lo < orgCoords[a] || hi > maxCoords[a]) {
            Logger::instance().warning(
                QString("  ATTENTION %1: range [%2,%3] depasse le volume [%4,%5]")
                    .arg(axisNames[a])
                    .arg(lo, 0, 'f', 1).arg(hi, 0, 'f', 1)
                    .arg(orgCoords[a], 0, 'f', 1).arg(maxCoords[a], 0, 'f', 1));
        }
    }

    m_planeCalc.setReferenceLandmark(refLmT0);


    double sellaXYZ[3] = {refLmT0.position.x(), refLmT0.position.y(), refLmT0.position.z()};
    double dMinPerAxis[3], dMaxPerAxis[3], stepPerAxis[3];
    
    for (int a = 0; a < 3; ++a) {
        double lo = std::max(sellaXYZ[a] - rangeMm, orgCoords[a]);
        double hi = std::min(sellaXYZ[a] + rangeMm, maxCoords[a]);
        dMinPerAxis[a] = lo - sellaXYZ[a];
        dMaxPerAxis[a] = hi - sellaXYZ[a]; 
        stepPerAxis[a] = (numSlices > 1) ? (dMaxPerAxis[a] - dMinPerAxis[a]) / (numSlices - 1) : 0.0;
    }

    Logger::instance().info(QString("Coupes: %1 par orientation, range=+/-%2mm")
        .arg(numSlices).arg(rangeMm, 0, 'f', 1));
    Logger::instance().info(QString("  Sagittal(X): [%1, %2]mm  Coronal(Y): [%3, %4]mm  Axial(Z): [%5, %6]mm")
        .arg(dMinPerAxis[0], 0, 'f', 1).arg(dMaxPerAxis[0], 0, 'f', 1)
        .arg(dMinPerAxis[1], 0, 'f', 1).arg(dMaxPerAxis[1], 0, 'f', 1)
        .arg(dMinPerAxis[2], 0, 'f', 1).arg(dMaxPerAxis[2], 0, 'f', 1));

    QStringList orientNames = {"Axial", "Coronal", "Sagittal"};

    for (size_t i = 0; i < m_alignedVolumes.size(); ++i) {
        Logger::instance().info(QString("--- Extraction %1 (point fixe T0: %2) ---")
            .arg(m_alignedVolumes[i]->getName()).arg(refLmT0.name));

        int sliceCount = 0;
        int outOfBoundsCount = 0;

        for (auto orient : {SliceOrientation::Axial,
                            SliceOrientation::Coronal,
                            SliceOrientation::Sagittal}) {

            int orientIdx = static_cast<int>(orient);
            
            int axisIdx = (orient == SliceOrientation::Axial) ? 2 :
                          (orient == SliceOrientation::Coronal) ? 1 : 0;
            double axisDMin = dMinPerAxis[axisIdx];
            double axisDMax = dMaxPerAxis[axisIdx];
            double axisStep = stepPerAxis[axisIdx];

            for (int k = 0; k < numSlices; ++k) {
                double d = axisDMin + k * axisStep;

                auto plane = m_planeCalc.computePlane(orient, d);
                Slice2D slice = m_extractor.extractSlice(*m_alignedVolumes[i], plane);
                slice.timepoint = QString("T%1").arg(i);
                slice.sourceCBCT = m_alignedVolumes[i]->getName();
                m_slices.push_back(slice);

                sliceCount++;
            }

            Logger::instance().info(QString("  %1: %2 coupes [%3mm -> %4mm]")
                .arg(orientNames[orientIdx]).arg(numSlices)
                .arg(axisDMin, 0, 'f', 1).arg(axisDMax, 0, 'f', 1));
        }

        int percent = static_cast<int>((i + 1) * 100 / m_alignedVolumes.size());
        emit progressUpdated(percent,
            QString("%1: %2 coupes extraites").arg(m_alignedVolumes[i]->getName()).arg(sliceCount));
    }

    Logger::instance().info(QString("Total: %1 coupes (%2 volumes x 3 orientations x %3 coupes)")
        .arg(m_slices.size())
        .arg(m_alignedVolumes.size()).arg(numSlices));

    return !m_slices.empty();
}



bool PipelineManager::validateSlices()
{
    if (m_alignedVolumes.size() < 2 || m_slices.empty()) return false;

    Logger::instance().info("=== Validation MCAGPC ===");


    Eigen::Vector3d refT0 = Eigen::Vector3d::Zero();
    Eigen::Vector3d refT1 = Eigen::Vector3d::Zero();
    QString refName = "?";

    if (m_landmarks.size() >= 2) {

        for (const auto& lm : m_landmarks[0]) {
            if (lm.abbreviation == "AN" || lm.name == "ANS") {
                refT0 = lm.position; refName = "ANS"; break;
            }
        }
        for (const auto& lm : m_landmarks[1]) {
            if (lm.abbreviation == "AN" || lm.name == "ANS") {
                refT1 = lm.position; break;
            }
        }
    }


    if (m_lastAlignedDeltaP >= 0.0) {

        refT1 = refT0 + Eigen::Vector3d(m_lastAlignedDeltaP, 0.0, 0.0);
        Logger::instance().info(QString("  [MCAGPC] Utilise dANS aligned = %1mm")
            .arg(m_lastAlignedDeltaP, 0, 'f', 2));
    }

    int slicesPerVol = static_cast<int>(m_slices.size() / m_alignedVolumes.size());
    int slicesPerOrient = slicesPerVol / 3;
    int midIdx = slicesPerOrient / 2;

    QStringList orientNames = {"Axial", "Coronal", "Sagittal"};

    for (int o = 0; o < 3; ++o) {
        int idxT0 = o * slicesPerOrient + midIdx;
        int idxT1 = slicesPerVol + o * slicesPerOrient + midIdx;

        if (idxT0 < static_cast<int>(m_slices.size()) &&
            idxT1 < static_cast<int>(m_slices.size())) {

            double mcagpc = m_validator.computeMCAGPC(
                m_slices[idxT0], m_slices[idxT1],
                refT0, refT1);

            Logger::instance().info(QString("  MCAGPC %1 (d=0, ref=%2): %3 %4")
                .arg(orientNames[o]).arg(refName)
                .arg(mcagpc, 0, 'f', 4)
                .arg(mcagpc > 0.85 ? "OK" : mcagpc > 0.7 ? "WARN" : "FAIL"));
        }
    }

    return true;
}

bool PipelineManager::saveResults(const QString& outputDir)
{
    Logger::instance().info("=== Sauvegarde ===");


    for (size_t i = 0; i < m_alignedVolumes.size(); ++i) {
        QString path = QDir(outputDir).filePath(QString("T%1_aligned.nii.gz").arg(i));
        if (m_alignedVolumes[i]->saveToNIfTI(path))
            Logger::instance().info(QString("Volume: %1").arg(path));
    }

    QStringList orientNames = {"Axial", "Coronal", "Sagittal"};
    int slicesPerVol = static_cast<int>(m_slices.size() / m_alignedVolumes.size());
    int slicesPerOrient = slicesPerVol / 3;
    int numTimepoints = static_cast<int>(m_alignedVolumes.size());


    SliceNormalizer normalizer;
    normalizer.setBlackThreshold(8);
    normalizer.setSafetyMargin(2);
    bool normalizationEnabled = true;

    Logger::instance().info(
        QString("[NORMALIZE] Normalisation au plus petit FOV : %1")
            .arg(normalizationEnabled ? "ACTIVEE" : "DESACTIVEE"));


    QMap<QPair<int,int>, SliceNormalizer::BoundingBox> commonBBoxes;

    if (normalizationEnabled && numTimepoints >= 2) {
        Logger::instance().info("[NORMALIZE] Calcul des bbox communes...");

        for (int o = 0; o < 3; ++o) {
            int normalizedCount = 0;
            for (int k = 0; k < slicesPerOrient; ++k) {
                std::vector<QImage> tripletImages;
                for (int tp = 0; tp < numTimepoints; ++tp) {
                    int idx = tp * slicesPerVol + o * slicesPerOrient + k;
                    if (idx >= static_cast<int>(m_slices.size())) break;
                    tripletImages.push_back(m_slices[idx].toQImage(300, 2400));
                }
                if (static_cast<int>(tripletImages.size()) != numTimepoints) continue;

                int smallestIdx = normalizer.detectSmallestFovIndex(tripletImages);
                auto bbox = normalizer.detectContentBBox(tripletImages[smallestIdx]);
                if (bbox.valid) {
                    commonBBoxes[qMakePair(o, k)] = bbox;
                    ++normalizedCount;
                }
            }
            Logger::instance().info(
                QString("[NORMALIZE]   %1 : %2/%3 triplets bbox calculee")
                    .arg(orientNames[o]).arg(normalizedCount).arg(slicesPerOrient));
        }
    }


    for (size_t vi = 0; vi < m_alignedVolumes.size(); ++vi) {
        for (int o = 0; o < 3; ++o) {
            QString subDir = QDir(outputDir).filePath(
                QString("T%1/%2").arg(vi).arg(orientNames[o]));
            QDir().mkpath(subDir);

            for (int k = 0; k < slicesPerOrient; ++k) {
                int idx = static_cast<int>(vi) * slicesPerVol + o * slicesPerOrient + k;
                if (idx >= static_cast<int>(m_slices.size())) break;

                QString path = QDir(subDir).filePath(
                    QString("slice_%1.png").arg(k, 3, 10, QChar('0')));

                QImage img = m_slices[idx].toQImage(300, 2400);

                auto bboxKey = qMakePair(o, k);
                if (normalizationEnabled && commonBBoxes.contains(bboxKey)) {
                    img = normalizer.cropImage(img, commonBBoxes[bboxKey]);
                }

                img.save(path);
            }
            Logger::instance().info(QString("  T%1/%2: %3 coupes sauvegardees")
                .arg(vi).arg(orientNames[o]).arg(slicesPerOrient));
        }
    }


    if (m_landmarks.size() >= 2 && !m_registrationResults.empty()) {
        Logger::instance().info("=== V7: Metriques landmark POST-registration (point fixe T0) ===");
        for (size_t i = 1; i < m_landmarks.size() && (i-1) < m_registrationResults.size(); ++i) {
            if (m_landmarks[i].empty() || m_landmarks[0].empty()) continue;
            const Eigen::Matrix4d& tf = m_registrationResults[i-1].transform;

            for (size_t j = 0; j < std::min(m_landmarks[0].size(), m_landmarks[i].size()); ++j) {
                Eigen::Vector3d ans_T0 = m_landmarks[0][j].position;
                Eigen::Vector3d ans_Ti = m_landmarks[i][j].position;

                Eigen::Vector4d p4(ans_Ti.x(), ans_Ti.y(), ans_Ti.z(), 1.0);
                Eigen::Vector4d p4_aligned = tf.inverse() * p4;
                Eigen::Vector3d ans_Ti_aligned(p4_aligned[0], p4_aligned[1], p4_aligned[2]);

                double distRaw = (ans_T0 - ans_Ti).norm();
                double distAligned = (ans_T0 - ans_Ti_aligned).norm();
                if (j == 0) m_lastAlignedDeltaP = distAligned;  

                Logger::instance().info(QString("  %1: dT0-T%2 raw=%3mm  aligned=%4mm %5")
                    .arg(m_landmarks[0][j].name)
                    .arg(i)
                    .arg(distRaw, 0, 'f', 2)
                    .arg(distAligned, 0, 'f', 2)
                    .arg(distAligned < 2.0 ? "OK" :
                         distAligned < 5.0 ? "WARN" : "FAIL"));
            }
        }
    } else if (m_landmarks.size() >= 2) {
        Logger::instance().info("=== Metriques landmark (point fixe T0) ===");
        size_t n = std::min(m_landmarks[0].size(), m_landmarks[1].size());
        for (size_t j = 0; j < n; ++j) {
            double dist = (m_landmarks[0][j].position - m_landmarks[1][j].position).norm();
            Logger::instance().info(QString("  %1: dT0-T1 = %2 mm %3")
                .arg(m_landmarks[0][j].name)
                .arg(dist, 0, 'f', 2)
                .arg(dist < 2.0 ? "OK" : dist < 5.0 ? "WARN" : "FAIL"));
        }
    }
    return true;
}


bool PipelineManager::runRegistrationOnly(const QString& /*refLandmark*/,
                                          double deltaThresholdMm)
{
    m_cancelled = false;
    m_deltaThresholdMm = deltaThresholdMm;
    m_registrationReports.clear();

    Logger::instance().info("========== REGISTRATION ONLY ==========");

    if (m_volumes.size() < 2) {
        Logger::instance().error("Au moins 2 volumes requis pour le recalage");
        return false;
    }
    if (m_allTimepointLandmarks.empty()) {
        Logger::instance().error("Aucun landmark charge (faire l'etape 2 d'abord)");
        return false;
    }


    emit stageStarted(PipelineStage::Preprocessing);
    if (!preprocessVolumes()) {
        emit stageCompleted(PipelineStage::Preprocessing, false);
        return false;
    }
    emit stageCompleted(PipelineStage::Preprocessing, true);
    if (m_cancelled) return false;


    emit stageStarted(PipelineStage::Registration);
    QElapsedTimer timer;

    m_alignedVolumes.clear();
    m_alignedVolumes.push_back(m_volumes[0]);
    m_registrationResults.clear();

    for (size_t i = 1; i < m_volumes.size(); ++i) {
        emit progressUpdated(static_cast<int>(i * 100 / m_volumes.size()),
                             QString("Registration T%1 -> T0...").arg(i));


        Eigen::Vector3d ans_T0 = m_allTimepointLandmarks[0][0].position;
        Eigen::Vector3d ans_Ti = (i < m_allTimepointLandmarks.size() &&
                                  !m_allTimepointLandmarks[i].empty())
                                 ? m_allTimepointLandmarks[i][0].position
                                 : ans_T0;
        Eigen::Vector3d initTrans = ans_Ti - ans_T0;

        auto params = m_registration.getParameters();
        params.initMethod = RigidRegistration::InitMethod::Landmark;
        params.initialTranslation = initTrans;
        m_registration.setParameters(params);

        timer.restart();
        auto result = m_registration.align(*m_volumes[0], *m_volumes[i]);
        qint64 ms = timer.elapsed();

        RegistrationReport rep;
        rep.timepoint = static_cast<int>(i);
        rep.converged = result.converged;
        rep.finalMetric = result.finalMetric;
        rep.iterations = result.iterations;
        rep.elapsedMs = ms;
        rep.deltaLandmarkRaw = initTrans.norm();   

        if (result.converged) {
            m_registrationResults.push_back(result);
            auto aligned = m_registration.applyTransform(
                *m_volumes[i], *m_volumes[0], result.transform);
            aligned->setName(QString("T%1_aligned").arg(i));
            m_alignedVolumes.push_back(aligned);


            Eigen::Vector4d p4(ans_Ti.x(), ans_Ti.y(), ans_Ti.z(), 1.0);
            Eigen::Vector4d p4a = result.transform.inverse() * p4;
            Eigen::Vector3d ans_Ti_aligned(p4a[0], p4a[1], p4a[2]);
            rep.deltaLandmark = (ans_T0 - ans_Ti_aligned).norm();

            Logger::instance().info(
                QString("  T%1: dANS raw=%2mm aligned=%3mm MI=%4 %5")
                    .arg(i)
                    .arg(rep.deltaLandmarkRaw, 0, 'f', 2)
                    .arg(rep.deltaLandmark, 0, 'f', 2)
                    .arg(rep.finalMetric, 0, 'f', 4)
                    .arg(rep.deltaLandmark < deltaThresholdMm ? "OK" : "WARN"));
        } else {
            rep.deltaLandmark = -1.0;  
            Logger::instance().error(QString("Echec registration T%1").arg(i));
        }

        m_registrationReports.push_back(rep);
        if (m_cancelled) { emit stageCompleted(PipelineStage::Registration, false); return false; }
    }

    emit stageCompleted(PipelineStage::Registration, true);
    Logger::instance().info("========== REGISTRATION ONLY TERMINE ==========");
    emit pipelineFinished(true);
    return true;
}


PipelineManager::ExtractionReport
PipelineManager::runExtractionOnly(const QString& outputDir,
                                   const QString& refLandmark,
                                   int slicesPerOrientation,
                                   double sliceRangeMm)
{
    ExtractionReport rep;
    rep.outputDir = outputDir;
    rep.refLandmark = refLandmark;

    Logger::instance().info("========== EXTRACTION ONLY ==========");


    if (m_alignedVolumes.size() < 2) {
        Logger::instance().error("Aucun volume recale. Lancer Registration d'abord.");
        return rep;
    }

    if (m_landmarks.empty()) {

        if (!m_allTimepointLandmarks.empty()) {
            m_landmarks = m_allTimepointLandmarks;
        } else {
            Logger::instance().error("Aucun landmark. Faire l'etape 2 (detection) d'abord.");
            return rep;
        }
    }

    m_outputDir = outputDir;
    QDir().mkpath(outputDir);


    QString effectiveRef = findBestRefLandmark(refLandmark);
    Logger::instance().info(QString("Ancre effective: %1").arg(effectiveRef));


    emit stageStarted(PipelineStage::SliceExtraction);
    if (!extractSlices(effectiveRef, slicesPerOrientation, sliceRangeMm)) {
        emit stageCompleted(PipelineStage::SliceExtraction, false);
        return rep;
    }
    emit stageCompleted(PipelineStage::SliceExtraction, true);


    saveResults(outputDir);


    rep.timepoints = static_cast<int>(m_alignedVolumes.size());
    rep.totalSlices = static_cast<int>(m_slices.size());
    rep.slicesPerOrientation = slicesPerOrientation;
    rep.ok = !m_slices.empty();

    Logger::instance().info(QString("Extraction terminee: %1 coupes (%2 TP x 3 x %3)")
        .arg(rep.totalSlices).arg(rep.timepoints).arg(slicesPerOrientation));


    validateSlices();

    Logger::instance().info("========== EXTRACTION ONLY TERMINE ==========");
    return rep;
}
} // namespace CBCTAlign
