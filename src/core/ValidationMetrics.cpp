/**
 * @file ValidationMetrics.cpp
 */
#include "ValidationMetrics.h"
#include "MathUtils.h"
#include "Logger.h"

#include <cmath>
#include <algorithm>
#include <numeric>

namespace CBCTAlign {

ValidationMetrics::ValidationMetrics(QObject* parent) : QObject(parent) {}

double ValidationMetrics::computeSSIM(const Slice2D& s1, const Slice2D& s2)
{
    if (s1.width != s2.width || s1.height != s2.height) {
        Logger::instance().warning("SSIM: dimensions différentes");
        return 0.0;
    }

    return MathUtils::computeSSIM(s1.data, s2.data, s1.width, s1.height);
}

double ValidationMetrics::computeNCC(const Slice2D& s1, const Slice2D& s2)
{
    if (s1.width != s2.width || s1.height != s2.height) return 0.0;

    const int n = s1.width * s1.height;
    if (n == 0) return 0.0;

    double mean1 = std::accumulate(s1.data.begin(), s1.data.end(), 0.0) / n;
    double mean2 = std::accumulate(s2.data.begin(), s2.data.end(), 0.0) / n;

    double num = 0, den1 = 0, den2 = 0;
    for (int i = 0; i < n; ++i) {
        double d1 = s1.data[i] - mean1;
        double d2 = s2.data[i] - mean2;
        num  += d1 * d2;
        den1 += d1 * d1;
        den2 += d2 * d2;
    }

    double denom = std::sqrt(den1 * den2);
    return denom < 1e-10 ? 0.0 : num / denom;
}

double ValidationMetrics::computeMI(const CBCTVolume& v1, const CBCTVolume& v2)
{
    if (!v1.isLoaded() || !v2.isLoaded()) return 0.0;

    const int numBins = 50;
    std::vector<std::vector<double>> joint(numBins, std::vector<double>(numBins, 0));
    std::vector<double> h1(numBins, 0), h2(numBins, 0);

    auto size = v1.getSize();
    if (size != v2.getSize()) return 0.0;

    const int step = 4;
    int count = 0;

    for (int z = 0; z < size.z(); z += step)
        for (int y = 0; y < size.y(); y += step)
            for (int x = 0; x < size.x(); x += step) {
                int b1 = std::clamp(static_cast<int>((v1.getVoxelValue(x,y,z) + 1000) / 4000.0 * numBins), 0, numBins-1);
                int b2 = std::clamp(static_cast<int>((v2.getVoxelValue(x,y,z) + 1000) / 4000.0 * numBins), 0, numBins-1);
                joint[b1][b2]++;
                h1[b1]++;
                h2[b2]++;
                count++;
            }

    if (count == 0) return 0.0;

    for (int i = 0; i < numBins; ++i) {
        h1[i] /= count;
        h2[i] /= count;
        for (int j = 0; j < numBins; ++j)
            joint[i][j] /= count;
    }

    double H1 = 0, H2 = 0, H12 = 0;
    for (int i = 0; i < numBins; ++i) {
        if (h1[i] > 1e-10) H1 -= h1[i] * std::log(h1[i]);
        if (h2[i] > 1e-10) H2 -= h2[i] * std::log(h2[i]);
        for (int j = 0; j < numBins; ++j)
            if (joint[i][j] > 1e-10)
                H12 -= joint[i][j] * std::log(joint[i][j]);
    }
    return H1 + H2 - H12;
}

double ValidationMetrics::computeDSC(const CBCTVolume& v1, const CBCTVolume& v2, double threshold)
{
    if (!v1.isLoaded() || !v2.isLoaded()) return 0.0;

    auto size = v1.getSize();
    long intersection = 0, sum1 = 0, sum2 = 0;

    const int step = 2;
    for (int z = 0; z < size.z(); z += step)
        for (int y = 0; y < size.y(); y += step)
            for (int x = 0; x < size.x(); x += step) {
                bool b1 = v1.getVoxelValue(x,y,z) > threshold;
                bool b2 = v2.getVoxelValue(x,y,z) > threshold;
                if (b1) sum1++;
                if (b2) sum2++;
                if (b1 && b2) intersection++;
            }

    return (sum1 + sum2 == 0) ? 0.0 : 2.0 * intersection / (sum1 + sum2);
}


double ValidationMetrics::computeMCAGPC(const Slice2D& s1, const Slice2D& s2,
                                         const Eigen::Vector3d& lm1,
                                         const Eigen::Vector3d& lm2,
                                         double maxError)
{
    double ssimVal = computeSSIM(s1, s2);
    double nccVal  = computeNCC(s1, s2);
    double deltaP  = (lm1 - lm2).norm();
    double lmTerm  = 1.0 - std::min(deltaP / maxError, 1.0);

    double mcagpc = alpha * ssimVal + beta * nccVal + gamma * lmTerm;

    Logger::instance().info(QString("MCAGPC: SSIM=%1, NCC=%2, ΔP=%3mm → %4")
        .arg(ssimVal, 0, 'f', 4).arg(nccVal, 0, 'f', 4)
        .arg(deltaP, 0, 'f', 2).arg(mcagpc, 0, 'f', 4));

    return mcagpc;
}

double ValidationMetrics::computeIFST(const std::vector<double>& mcagpcValues,
                                       const std::vector<double>& timeDeltas,
                                       double lambda)
{
    if (mcagpcValues.empty()) return 0.0;

    int n = static_cast<int>(std::min(mcagpcValues.size(), timeDeltas.size()));
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += mcagpcValues[i] * std::exp(-lambda * std::abs(timeDeltas[i]));

    return sum / n;
}

ValidationResult ValidationMetrics::validateRegistration(const CBCTVolume& fixed,
                                                          const CBCTVolume& /*moving*/,
                                                          const CBCTVolume& registered)
{
    ValidationResult r;
    if (!fixed.isLoaded() || !registered.isLoaded()) {
        r.errorMessage = "Volumes non chargés";
        return r;
    }

    emit validationProgress(0, "MI...");
    r.MI = computeMI(fixed, registered);
    emit validationProgress(50, "DSC...");
    r.DSC = computeDSC(fixed, registered);
    r.isValid = true;

    Logger::instance().info(QString("Validation: MI=%1, DSC=%2").arg(r.MI, 0, 'f', 4).arg(r.DSC, 0, 'f', 4));
    emit validationProgress(100, "Terminé");
    return r;
}

ValidationResult ValidationMetrics::validateSliceCorrespondence(
    const std::vector<Slice2D>& slicesT0,
    const std::vector<Slice2D>& slicesTi,
    const std::vector<Landmark>& landmarksT0,
    const std::vector<Landmark>& landmarksTi)
{
    ValidationResult r;
    if (slicesT0.empty() || slicesTi.empty()) {
        r.errorMessage = "Pas de coupes";
        return r;
    }


    Eigen::Vector3d nT0 = Eigen::Vector3d::Zero(), nTi = Eigen::Vector3d::Zero();
    for (const auto& lm : landmarksT0) if (lm.abbreviation == "N") nT0 = lm.position;
    for (const auto& lm : landmarksTi) if (lm.abbreviation == "N") nTi = lm.position;

    int count = static_cast<int>(std::min(slicesT0.size(), slicesTi.size()));
    double ssimSum = 0, nccSum = 0;
    for (int i = 0; i < count; ++i) {
        ssimSum += computeSSIM(slicesT0[i], slicesTi[i]);
        nccSum  += computeNCC(slicesT0[i], slicesTi[i]);
    }

    r.SSIM = ssimSum / count;
    r.NCC  = nccSum / count;
    r.MCAGPC = computeMCAGPC(slicesT0[count/2], slicesTi[count/2], nT0, nTi);
    r.isValid = true;

    Logger::instance().info(QString("Correspondance: SSIM=%1, NCC=%2, MCAGPC=%3")
        .arg(r.SSIM, 0, 'f', 4).arg(r.NCC, 0, 'f', 4).arg(r.MCAGPC, 0, 'f', 4));
    return r;
}

} // namespace CBCTAlign
