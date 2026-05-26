/**
 * @file MainWindow.h
 * @brief CBCTAlign Main Window
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
#include <QSpinBox>
#include <QVBoxLayout>
#include <QScrollArea>
#include <memory>
#include <vector>

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

private:
    void setupUI();
    void connectSignals();
    void createMenuBar();
    void scanResultsDirectory();
    void displayCurrentSlices();
    void updateLandmarkTable();

    // Left panel
    QListWidget*   m_volumeList;
    QPushButton*   m_btnLoadDICOM;
    QPushButton*   m_btnLoadNIfTI;
    QPushButton*   m_btnRunPipeline;
    QPushButton*   m_btnStop;

    QGroupBox*     m_grpLandmarks;
    QPushButton*   m_btnLoadJSON;
    QPushButton*   m_btnLoadCSV;
    QTableWidget*  m_tableLandmarks;
    QLabel*        m_lblLandmarkStatus;

    QGroupBox*     m_grpSliceControls;
    QSlider*       m_sliderSliceNum;
    QSpinBox*      m_spinSliceNum;
    QLabel*        m_lblSliceInfo;
    QPushButton*   m_btnLoadSlices;

    // Right panel
    QLabel*        m_lblTitle;
    QScrollArea*   m_scrollArea;
    QWidget*       m_scrollContent;
    QVBoxLayout*   m_rightContentLayout;

    // Data
    std::vector<std::shared_ptr<CBCTVolume>> m_volumes;
    std::unique_ptr<PipelineManager> m_pipeline;
    std::vector<Landmark> m_manualLandmarks;
    std::vector<std::vector<Landmark>> m_allTimepointLandmarks;  // V4: ANS per timepoint
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
