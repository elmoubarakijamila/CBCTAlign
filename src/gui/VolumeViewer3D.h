// VolumeViewer3D.h - Visualisation 3D avec VTK
#ifndef VOLUMEVIEWER3D_H
#define VOLUMEVIEWER3D_H

#include <QWidget>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkVolume.h>
#include <vtkProperty.h>

#include "CBCTVolume.h"
#include "CephalometricDetector.h"
#include "SlicePlaneCalculator.h"

namespace CBCTAlign {

class VolumeViewer3D : public QWidget {
    Q_OBJECT
public:
    explicit VolumeViewer3D(QWidget* parent = nullptr);
    void setVolume(CBCTVolume* volume);
    void setLandmarks(const std::vector<Landmark>& landmarks);
    void setSlicePlane(SliceOrientation orientation, double position);

private:
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkVolume> m_volume;
    
    void setupPipeline();
    void updateLandmarkActors();
};

} // namespace CBCTAlign

#endif 
