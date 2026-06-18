/**
 * @file ValidationMetrics.h
 * @brief Métriques de validation incluant la contribution MCAGPC
 */
#ifndef VALIDATIONMETRICS_H
#define VALIDATIONMETRICS_H

#include <QObject>
#include <vector>
#include <Eigen/Dense>

#include "CBCTVolume.h"
#include "SliceExtractor.h"
#include "CephalometricDetector.h"

namespace CBCTAlign {

struct ValidationResult {
    double TRE  = 0;
    double MI   = 0;
    double DSC  = 0;
    double SSIM = 0;
    double NCC  = 0;
    double MCAGPC = 0;
    double IFST = 0;
    bool isValid = false;
    QString errorMessage;
};

/**
 * @class ValidationMetrics
 * @brief Calculateur de métriques incluant la contribution MCAGPC
 *
 * MCAGPC = α·SSIM + β·NCC + γ·(1 - ΔP/ΔP_max)
 * IFST   = (1/N) × Σ MCAGPC × exp(-λ·|Δt|)
 */
class ValidationMetrics : public QObject
{
    Q_OBJECT

public:
    explicit ValidationMetrics(QObject* parent = nullptr);

    // Métriques standard
    double computeSSIM(const Slice2D& s1, const Slice2D& s2);
    double computeNCC(const Slice2D& s1, const Slice2D& s2);
    double computeMI(const CBCTVolume& v1, const CBCTVolume& v2);
    double computeDSC(const CBCTVolume& v1, const CBCTVolume& v2, double threshold = 300.0);

    // Contribution originale
    double computeMCAGPC(const Slice2D& s1, const Slice2D& s2,
                         const Eigen::Vector3d& lm1, const Eigen::Vector3d& lm2,
                         double maxError = 2.0);

    double computeIFST(const std::vector<double>& mcagpcValues,
                       const std::vector<double>& timeDeltas,
                       double lambda = 0.1);

    // Validation complète
    ValidationResult validateRegistration(const CBCTVolume& fixed,
                                          const CBCTVolume& moving,
                                          const CBCTVolume& registered);

    ValidationResult validateSliceCorrespondence(
        const std::vector<Slice2D>& slicesT0,
        const std::vector<Slice2D>& slicesTi,
        const std::vector<Landmark>& landmarksT0,
        const std::vector<Landmark>& landmarksTi);

    // Pondérations MCAGPC
    double alpha = 0.5;
    double beta  = 0.3;
    double gamma = 0.2;

signals:
    void validationProgress(int percent, const QString& metric);
};

} // namespace CBCTAlign
#endif
