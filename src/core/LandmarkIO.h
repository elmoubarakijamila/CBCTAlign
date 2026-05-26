#ifndef LANDMARKIO_H
#define LANDMARKIO_H

#include <QString>
#include <QList>
#include <Eigen/Dense>
#include "CephalometricDetector.h"

namespace CBCTAlign {

struct LandmarkEntry {
    QString patientID;
    QString timepoint;
    QString name;
    Eigen::Vector3d position;
    double confidence;
};

class LandmarkIO {
public:
    /// Sauvegarde les landmarks dans un fichier CSV
    static bool save(const QString& filepath, const QList<LandmarkEntry>& entries);

    /// Charge les landmarks depuis un fichier CSV
    static QList<LandmarkEntry> load(const QString& filepath);

    /// Charge les landmarks depuis un fichier .mrk.json de 3D Slicer (ALI_CBCT)
    static QList<LandmarkEntry> loadSlicerJSON(const QString& filepath,
                                                const QString& patientID = "",
                                                const QString& timepoint = "T0");

    /// Convertit les entries en vector<Landmark> pour un timepoint donne
    static std::vector<Landmark> toLandmarks(
        const QList<LandmarkEntry>& entries,
        const QString& timepoint);
};

} // namespace CBCTAlign
#endif
