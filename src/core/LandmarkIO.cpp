/**
 * @file LandmarkIO.cpp
 */
#include "LandmarkIO.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>

namespace CBCTAlign {

bool LandmarkIO::save(const QString& filepath,
                      const QList<LandmarkEntry>& entries)
{
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "Cannot write to " << filepath.toStdString() << std::endl;
        return false;
    }

    QTextStream out(&file);
    out << "PatientID,Timepoint,Landmark,X,Y,Z,Confidence\n";

    for (const auto& entry : entries) {
        out << QString("%1,%2,%3,%4,%5,%6,%7\n")
            .arg(entry.patientID)
            .arg(entry.timepoint)
            .arg(entry.name)
            .arg(entry.position.x(), 0, 'f', 2)
            .arg(entry.position.y(), 0, 'f', 2)
            .arg(entry.position.z(), 0, 'f', 2)
            .arg(entry.confidence, 0, 'f', 1);
    }

    file.close();
    std::cout << "Landmarks saved to " << filepath.toStdString() << std::endl;
    return true;
}

QList<LandmarkEntry> LandmarkIO::load(const QString& filepath)
{
    QList<LandmarkEntry> entries;
    QFile file(filepath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Cannot read " << filepath.toStdString() << std::endl;
        return entries;
    }

    QTextStream in(&file);
    QString header = in.readLine();

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(',');
        if (fields.size() < 7) continue;

        LandmarkEntry entry;
        entry.patientID  = fields[0];
        entry.timepoint  = fields[1];
        entry.name       = fields[2];
        entry.position.x() = fields[3].toDouble();
        entry.position.y() = fields[4].toDouble();
        entry.position.z() = fields[5].toDouble();
        entry.confidence   = fields[6].toDouble();

        entries.append(entry);
    }

    file.close();
    std::cout << "Loaded " << entries.size() << " landmarks from "
              << filepath.toStdString() << std::endl;
    return entries;
}


QList<LandmarkEntry> LandmarkIO::loadSlicerJSON(const QString& filepath,
                                                   const QString& patientID,
                                                   const QString& timepoint)
{
    QList<LandmarkEntry> entries;

    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Cannot open JSON: " << filepath.toStdString() << std::endl;
        return entries;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull()) {
        std::cerr << "JSON parse error: " << parseError.errorString().toStdString() << std::endl;
        return entries;
    }

    QJsonObject root = doc.object();
    QJsonArray markups = root["markups"].toArray();
    if (markups.isEmpty()) {
        std::cerr << "No 'markups' array in JSON" << std::endl;
        return entries;
    }

    QJsonObject markup0 = markups[0].toObject();
    QString coordSystem = markup0["coordinateSystem"].toString("LPS");
    bool isRAS = (coordSystem.toUpper() == "RAS");
    QJsonArray controlPoints = markup0["controlPoints"].toArray();

    QMap<QString, QString> labelMap;
    labelMap["S"]     = "Sella";
    labelMap["N"]     = "Nasion";
    labelMap["Ba"]    = "Basion";
    labelMap["RPo"]   = "RPorion";
    labelMap["LPo"]   = "LPorion";
    labelMap["RFZyg"] = "ZygionRight";
    labelMap["LFZyg"] = "ZygionLeft";
    labelMap["C2"]    = "C2";
    labelMap["C3"]    = "C3";
    labelMap["C4"]    = "C4";
    labelMap["ANS"]   = "ANS";
    labelMap["PNS"]   = "PNS";
    labelMap["A"]     = "A-point";
    labelMap["ANS"]   = "ANS";
    labelMap["PNS"]   = "PNS";
    labelMap["A"]     = "A";

    for (const QJsonValue& cpVal : controlPoints) {
        QJsonObject cp = cpVal.toObject();
        QString label = cp["label"].toString();
        QJsonArray pos = cp["position"].toArray();

        if (pos.size() < 3) continue;

        double x = pos[0].toDouble();
        double y = pos[1].toDouble();
        double z = pos[2].toDouble();

        if (isRAS) {
            x = -x;
            y = -y;
            // z stays the same
        }

        LandmarkEntry entry;
        entry.patientID = patientID;
        entry.timepoint = timepoint;
        entry.name = labelMap.value(label, label); 
        entry.position = Eigen::Vector3d(x, y, z);
        entry.confidence = 0.85; 

        entries.append(entry);

        std::cout << "  [JSON] " << label.toStdString()
                  << " -> " << entry.name.toStdString()
                  << " at [" << x << ", " << y << ", " << z << "] "
                  << coordSystem.toStdString() << std::endl;
    }

    std::cout << "Loaded " << entries.size() << " landmarks from Slicer JSON: "
              << filepath.toStdString() << std::endl;
    return entries;
}

std::vector<Landmark> LandmarkIO::toLandmarks(
    const QList<LandmarkEntry>& entries,
    const QString& timepoint)
{
    std::vector<Landmark> lms;

    for (const auto& entry : entries) {
        if (entry.timepoint != timepoint) continue;

        Landmark lm;
        lm.name = entry.name;
        lm.position = entry.position;
        lm.confidence = entry.confidence;


        if (entry.name == "Sella")          lm.abbreviation = "S";
        else if (entry.name == "Nasion")    lm.abbreviation = "N";
        else if (entry.name == "Basion")    lm.abbreviation = "Ba";
        else if (entry.name == "Menton")    lm.abbreviation = "Me";
        else if (entry.name == "ZygionLeft")  lm.abbreviation = "ZL";
        else if (entry.name == "ZygionRight") lm.abbreviation = "ZR";
        else lm.abbreviation = entry.name.left(2);

        lms.push_back(lm);
    }

    return lms;
}

} // namespace CBCTAlign
