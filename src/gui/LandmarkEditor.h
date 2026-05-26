// LandmarkEditor.h
#ifndef LANDMARKEDITOR_H
#define LANDMARKEDITOR_H

#include <QWidget>
#include <QTableWidget>
#include "CephalometricDetector.h"

namespace CBCTAlign {

class LandmarkEditor : public QWidget {
    Q_OBJECT
public:
    explicit LandmarkEditor(QWidget* parent = nullptr);
    void setLandmarks(const std::vector<Landmark>& landmarks);

signals:
    void landmarkModified(const QString& name, const Eigen::Vector3d& position);

private:
    QTableWidget* m_table;
};

} // namespace CBCTAlign

#endif // LANDMARKEDITOR_H
