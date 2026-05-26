// FileIO.h
#ifndef FILEIO_H
#define FILEIO_H

#include <QString>
#include <QStringList>

namespace CBCTAlign {

namespace FileIO {

QStringList findDICOMSeries(const QString& directory);
bool isDICOMFile(const QString& filePath);
bool isNIfTIFile(const QString& filePath);
QString generateOutputPath(const QString& baseDir, const QString& prefix, const QString& extension);

} // namespace FileIO
} // namespace CBCTAlign
#endif
