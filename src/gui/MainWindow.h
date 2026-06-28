/**
 * @file MainWindow.h
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTableWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMap>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QScrollArea>
#include <memory>
#include <QDialog>
#include <QToolButton>
#include <vector>
#include <QDoubleSpinBox>
#include "../core/PipelineManager.h"
#include "../core/CBCTVolume.h"
#include "../core/SliceExtractor.h"
#include "../core/LandmarkIO.h"

namespace CBCTAlign {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);


private slots:
    void onLoadDICOM();
    void onLoadNIfTI();
    void onRunPipeline();
    void onPipelineFinished(bool success);
    void onPipelineProgress(int percent, const QString& msg);
    void onSliceNumChanged(int value);
    void onLoadSlicesFromDisk();
    void onLoadLandmarksJSON();
    void onLoadLandmarksCSV();
    void onDetectLandmarks();         
    void onDetectLandmarksRich();      
    void displayRegistrationStep();   
    void onRunRegistration();         
    void displayExtractionStep();
    void onRunExtraction();
    void showZoomedSlice(const QString& timepoint, const QString& orientation);
    void onWindowLevelChanged();


private:
    void setupUI();
    void connectSignals();
    void createMenuBar();
    void scanResultsDirectory();
    void displayCurrentSlices();
    void updateLandmarkTable();
    void onStepClicked(int row);          
    void displayStep1Attributes();        
    void displayLandmarksStep();  


    QSpinBox* m_spinNumSlices = nullptr;
    QDoubleSpinBox* m_spinRangeMm = nullptr;
    PipelineManager::ExtractionReport m_extractionReport;
    QString m_extractionOutputDir;


    QListWidget*   m_stepList;
    QWidget*       m_leftPanel;        

    QGroupBox*     m_grpVolumes;       
    QListWidget*   m_volumeList;

    QPushButton*   m_btnLoadDICOM;
    QPushButton*   m_btnLoadNIfTI;



    QGroupBox*   m_grpDetect = nullptr;
    QComboBox*   m_comboModel = nullptr;
    QComboBox* m_comboRefLm = nullptr;     
    QDoubleSpinBox* m_spinDeltaThr = nullptr; 
    QWidget*     m_detectChecksContainer = nullptr;   
    QPushButton* m_btnLaunchDetect = nullptr;
    QLabel*      m_lblFovNote;
    QMap<QString, QCheckBox*> m_landmarkChecks;   
    QString      m_detectModel;                   
    QStringList  m_volumePaths;                               

    QGroupBox*     m_grpLandmarks;
    QPushButton*   m_btnLoadJSON;
    QPushButton*   m_btnLoadCSV;
    QPushButton*   m_btnDetect;       
    QTableWidget*  m_tableLandmarks;
    QLabel*        m_lblLandmarkStatus;

    QGroupBox*     m_grpSliceControls;
    QSlider*       m_sliderSliceNum;

    QSlider* m_sliderLevel = nullptr;
    QSlider* m_sliderWidth = nullptr;
    QLabel*  m_lblLevelVal = nullptr;
    QLabel*  m_lblWidthVal = nullptr;
    int m_wlLevel = 300;    
    int m_wlWidth = 2400;    
    QSpinBox*      m_spinSliceNum;
    QLabel*        m_lblSliceInfo;
    QPushButton*   m_btnLoadSlices;


    QLabel*        m_lblTitle;
    QScrollArea*   m_scrollArea;
    QWidget*       m_scrollContent;
    QVBoxLayout*   m_rightContentLayout;

    // Data
    std::vector<std::shared_ptr<CBCTVolume>> m_volumes;
    std::unique_ptr<PipelineManager> m_pipeline;
    std::vector<Landmark> m_manualLandmarks;
    std::vector<std::vector<Landmark>> m_allTimepointLandmarks; 
    std::vector<PipelineManager::RegistrationReport> m_registrationReports;
    bool m_useManualLandmarks = false;

    QString     m_currentOutputDir;
    QString     m_resultsDir;
    QStringList m_timepointDirs;
    int         m_currentSliceNum = 25;
    int         m_numSlices = 50;
    bool        m_pipelineCompleted = false;
};

} // namespace CBCTAlign
#endif
