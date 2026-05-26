// FileIO.cpp
#include "FileIO.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>

namespace CBCTAlign {
namespace FileIO {

QStringList findDICOMSeries(const QString& directory) {
    QStringList result;
    QDirIterator it(directory, QStringList() << "*.dcm" << "*.DCM",
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result << it.next();
    }
    result.sort();
    return result;
}

bool isDICOMFile(const QString& filePath) {
    QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == "dcm" || suffix == "dicom";
}

bool isNIfTIFile(const QString& filePath) {
    QString name = QFileInfo(filePath).fileName().toLower();
    return name.endsWith(".nii") || name.endsWith(".nii.gz");
}

QString generateOutputPath(const QString& baseDir, const QString& prefix, const QString& extension) {
    int counter = 0;
    QString path;
    do {
        path = QDir(baseDir).filePath(QString("%1_%2.%3").arg(prefix).arg(counter++).arg(extension));
    } while (QFileInfo::exists(path));
    return path;
}

} // namespace FileIO
} // namespace CBCTAlign
