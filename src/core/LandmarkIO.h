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

    static bool save(const QString& filepath, const QList<LandmarkEntry>& entries);


    static QList<LandmarkEntry> load(const QString& filepath);


    static QList<LandmarkEntry> loadSlicerJSON(const QString& filepath,
                                                const QString& patientID = "",
                                                const QString& timepoint = "T0");


    static std::vector<Landmark> toLandmarks(
        const QList<LandmarkEntry>& entries,
        const QString& timepoint);
};

} // namespace CBCTAlign
#endif
