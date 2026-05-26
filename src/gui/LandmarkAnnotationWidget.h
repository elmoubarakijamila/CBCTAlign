#ifndef LANDMARKANNOTATIONWIDGET_H
#define LANDMARKANNOTATIONWIDGET_H

#include <QWidget>
#include <QSlider>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QProcess>
#include <Eigen/Dense>
#include <vector>
#include <memory>
#include "CephalometricDetector.h"
#include "LandmarkIO.h"

namespace CBCTAlign {

class CBCTVolume;

class LandmarkAnnotationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LandmarkAnnotationWidget(QWidget* parent = nullptr);
    ~LandmarkAnnotationWidget();

    void setVolume(std::shared_ptr<CBCTVolume> volume,
                   const QString& patientID,
                   const QString& timepoint);

    /// Retourne les landmarks annotes sous forme de vector<Landmark>
    std::vector<Landmark> getLandmarks() const;

    void loadLandmarksFromFile(const QString& filepath);
    bool hasMinimumLandmarks() const;

    /// Charge les landmarks depuis un fichier .mrk.json de 3D Slicer (ALI_CBCT)
    bool loadFromSlicerJSON(const QString& filepath);

    /// Configure le chemin vers PythonSlicer et les modeles ALI_CBCT
    void setALICBCTPaths(const QString& slicerPython, const QString& modelsDir);
    void setCurrentNIfTIPath(const QString& path) { m_currentNIfTIPath = path; }

signals:
    void landmarksChanged();
    void landmarksValidated(const std::vector<Landmark>& landmarks);
    void landmarksSaved(const QString& filepath);

private slots:
    void onAxialSliderChanged(int value);
    void onSagittalSliderChanged(int value);
    void onCoronalSliderChanged(int value);
    void onViewClicked(int viewIndex, double x, double y);
    void onRemoveLandmark();
    void onModifyLandmark();
    void onSave();
    void onLoad();
    void onClear();
    void onValidate();

    // ALI_CBCT auto-detection
    void onAutoDetectALI();
    void onAutoDetectFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onImportSlicerJSON();

private:
    void setupUI();
    void setupConnections();
    void updateViews();
    void updateLandmarkTable();
    void updateSliceLabels();
    Eigen::Vector3d viewClickToPhysical(int viewIndex, double clickX, double clickY);
    void physicalToSliceIndices(const Eigen::Vector3d& phys,
                                int& axial, int& sagittal, int& coronal);

    std::shared_ptr<CBCTVolume> m_volume;
    QString m_patientID;
    QString m_timepoint;
    QString m_currentNIfTIPath;  // Chemin du fichier NIfTI courant

    QMap<QString, Eigen::Vector3d> m_landmarks;
    QStringList m_landmarkNames = {"Sella", "Nasion", "Menton", "ZygionLeft", "ZygionRight"};

    int m_axialIndex = 0;
    int m_sagittalIndex = 0;
    int m_coronalIndex = 0;
    int m_dims[3] = {0, 0, 0};
    double m_spacing[3] = {1.0, 1.0, 1.0};
    double m_origin[3] = {0.0, 0.0, 0.0};

    // UI widgets
    QSlider* m_axialSlider;
    QSlider* m_sagittalSlider;
    QSlider* m_coronalSlider;
    QLabel* m_axialLabel;
    QLabel* m_sagittalLabel;
    QLabel* m_coronalLabel;
    QComboBox* m_landmarkSelector;
    QTableWidget* m_landmarkTable;
    QLabel* m_volumeLabel;
    QLabel* m_statusLabel;
    QPushButton* m_saveBtn;
    QPushButton* m_loadBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_validateBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_modifyBtn;

    // ALI_CBCT auto-detection
    QPushButton* m_btnAutoDetect;
    QPushButton* m_btnImportJSON;
    QProcess* m_aliProcess = nullptr;
    QString m_slicerPythonPath;
    QString m_aliModelsDir;
};

} // namespace CBCTAlign
#endif
