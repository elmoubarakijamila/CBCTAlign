// VolumeViewer3D.cpp
#include "VolumeViewer3D.h"
#include <QVBoxLayout>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkImageData.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>

namespace CBCTAlign {

VolumeViewer3D::VolumeViewer3D(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    layout->addWidget(m_vtkWidget);
    
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(0.1, 0.1, 0.15);
    
    m_vtkWidget->renderWindow()->AddRenderer(m_renderer);
    
    auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    m_vtkWidget->interactor()->SetInteractorStyle(style);
}

void VolumeViewer3D::setVolume(CBCTVolume* volume) {
    if (!volume || !volume->isLoaded()) return;
    setupPipeline();
}

void VolumeViewer3D::setLandmarks(const std::vector<Landmark>& landmarks) {

    for (const auto& lm : landmarks) {
        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetCenter(lm.position.x(), lm.position.y(), lm.position.z());
        sphere->SetRadius(2.0);
        
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphere->GetOutputPort());
        
        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(1.0, 0.0, 0.0);
        
        m_renderer->AddActor(actor);
    }
    m_vtkWidget->renderWindow()->Render();
}

void VolumeViewer3D::setSlicePlane(SliceOrientation orientation, double position) {

}

void VolumeViewer3D::setupPipeline() {

    auto colorFunc = vtkSmartPointer<vtkColorTransferFunction>::New();
    colorFunc->AddRGBPoint(-1000, 0.0, 0.0, 0.0);
    colorFunc->AddRGBPoint(0, 0.5, 0.3, 0.2);      
    colorFunc->AddRGBPoint(500, 0.9, 0.8, 0.7);   
    colorFunc->AddRGBPoint(1500, 1.0, 1.0, 1.0);  
    
    auto opacityFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
    opacityFunc->AddPoint(-1000, 0.0);
    opacityFunc->AddPoint(0, 0.0);
    opacityFunc->AddPoint(300, 0.1);
    opacityFunc->AddPoint(1000, 0.5);
    
    m_renderer->ResetCamera();
    m_vtkWidget->renderWindow()->Render();
}

void VolumeViewer3D::updateLandmarkActors() {}

} // namespace CBCTAlign
