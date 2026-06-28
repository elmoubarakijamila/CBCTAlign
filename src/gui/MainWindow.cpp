/**
 * @file MainWindow.cpp
 */
#include "MainWindow.h"
#include "../core/LandmarkIO.h"
#include "../utils/Logger.h"
#include <QFrame>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QGroupBox>
#include <QHeaderView>
#include <QScrollArea>
#include <QProcess>
#include <memory>
#include <QPixmap>
#include <QImage>
#include <QSettings>
#include <QStandardPaths>
#include <QListView>
#include <QTreeView>
#include <QApplication>
#include <QDialog>
#include <QToolButton>
#include <QKeyEvent>



namespace {

    const QString BASE_DATA_DIR = qEnvironmentVariable(
        "CBCTALIGN_DATA_DIR", QDir::homePath() + "/CBCTAlign_data");
    const QString DEFAULT_DICOM_DIR    = BASE_DATA_DIR + "/MOULID_HICHAM/CBCT";
    const QString DEFAULT_NIFTI_DIR    = BASE_DATA_DIR + "/MOULID_HICHAM/nifti";
    const QString DEFAULT_LANDMARK_DIR = BASE_DATA_DIR + "/MOULID_HICHAM/nifti";   
    const QString DEFAULT_CSV_DIR      = BASE_DATA_DIR + "/MOULID_HICHAM";
    const QString DEFAULT_OUTPUT_DIR   = BASE_DATA_DIR + "/results";

    inline QString lastDirOrDefault(const QString& key, const QString& defaultDir) {
        QSettings settings("LAROSERI", "CBCTAlign");
        QString d = settings.value(key, defaultDir).toString();
        return QDir(d).exists() ? d : defaultDir;
    }



    inline void rememberDir(const QString& key, const QString& path) {
        if (path.isEmpty()) return;
        QSettings settings("LAROSERI", "CBCTAlign");
        QFileInfo info(path);
        QString dir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
        settings.setValue(key, dir);
    }
}

static const char* GREEN_BTN =
    "QPushButton { background-color:#4CAF50; color:white; font-weight:bold; "
    "padding:7px; border-radius:4px; font-size:12px; }"
    "QPushButton:hover { background-color:#45a049; }"
    "QPushButton:disabled { background-color:#ccc; color:#666; }";

static const char* GREEN_BTN_SM =
    "QPushButton { background-color:#4CAF50; color:white; "
    "padding:5px; border-radius:3px; font-size:11px; }"
    "QPushButton:hover { background-color:#45a049; }";

namespace CBCTAlign {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("CBCTAlign — Spatio-Temporal CBCT Alignment");
    resize(1400, 900);


    QDir().mkpath(DEFAULT_OUTPUT_DIR);

    setupUI();
    connectSignals();
    createMenuBar();
    Logger::instance().info("GUI initialized");
}

void MainWindow::setupUI()
{
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    auto* splitter = new QSplitter(Qt::Horizontal);


    m_stepList = new QListWidget;
    m_stepList->setMaximumWidth(240);
    m_stepList->setMinimumWidth(220);
    m_stepList->addItem("1 · Load CBCT");
    m_stepList->addItem("2 · Landmarks");
    m_stepList->addItem("3 · Registration");
    m_stepList->addItem("4 · Extraction");
    m_stepList->addItem("5 · Validation");
    m_stepList->setCurrentRow(0);
    m_stepList->setStyleSheet(
        "QListWidget { font-size:17px; border:1px solid #ccc; "
        "border-radius:4px; background:#fafafa; }"
        "QListWidget::item { padding:16px 12px; border-bottom:1px solid #eee; }"
        "QListWidget::item:selected { background:#4CAF50; color:white; }");
    splitter->addWidget(m_stepList);

    m_leftPanel = new QWidget;
    auto* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setSpacing(4);
    leftLayout->setContentsMargins(4, 4, 4, 4);

    m_grpVolumes = new QGroupBox("1 · Load CBCT Volumes");
    auto* volLayout = new QVBoxLayout(m_grpVolumes);
    volLayout->setSpacing(3);

    m_volumeList = new QListWidget;
    m_volumeList->setMaximumHeight(80);
    volLayout->addWidget(m_volumeList);

    auto* btnRow = new QHBoxLayout;
    m_btnLoadDICOM = new QPushButton("DICOM");
    m_btnLoadNIfTI = new QPushButton("NIfTI");
    m_btnLoadDICOM->setStyleSheet(GREEN_BTN_SM);
    m_btnLoadNIfTI->setStyleSheet(GREEN_BTN_SM);
    btnRow->addWidget(m_btnLoadDICOM);
    btnRow->addWidget(m_btnLoadNIfTI);
    volLayout->addLayout(btnRow);

    leftLayout->addWidget(m_grpVolumes);


    


    m_grpLandmarks = new QGroupBox("Cephalometric Landmarks");
    auto* lmLayout = new QVBoxLayout(m_grpLandmarks);
    lmLayout->setSpacing(3);

    auto* lmBtnRow = new QHBoxLayout;
    m_btnDetect = new QPushButton("🔍 Detect (ALI_CBCT)");
    m_btnLoadJSON = new QPushButton("Load JSON");
    m_btnLoadCSV = new QPushButton("Load CSV");
    m_btnDetect->setStyleSheet(GREEN_BTN_SM);
    m_btnLoadJSON->setStyleSheet(GREEN_BTN_SM);
    m_btnLoadCSV->setStyleSheet(GREEN_BTN_SM);
    lmBtnRow->addWidget(m_btnDetect);
    lmBtnRow->addWidget(m_btnLoadJSON);
    lmBtnRow->addWidget(m_btnLoadCSV);
    lmLayout->addLayout(lmBtnRow);

    m_tableLandmarks = new QTableWidget(0, 6);
    m_tableLandmarks->setHorizontalHeaderLabels({"TP", "Name", "Abbr.", "X (mm)", "Y (mm)", "Z (mm)"});
    m_tableLandmarks->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableLandmarks->verticalHeader()->setVisible(false);
    m_tableLandmarks->setMaximumHeight(120);
    m_tableLandmarks->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableLandmarks->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableLandmarks->setAlternatingRowColors(true);
    m_tableLandmarks->setStyleSheet(
        "QTableWidget { font-size:11px; }"
        "QHeaderView::section { background:#e0e0e0; font-weight:bold; font-size:10px; }");
    lmLayout->addWidget(m_tableLandmarks);
    m_tableLandmarks->setVisible(false);   // table affichee dans la zone de droite (etape Landmarks)

    m_lblLandmarkStatus = new QLabel("No landmarks loaded");
    m_lblLandmarkStatus->setAlignment(Qt::AlignCenter);
    m_lblLandmarkStatus->setStyleSheet("color:#999; font-size:11px; font-style:italic;");
    lmLayout->addWidget(m_lblLandmarkStatus);
    leftLayout->addWidget(m_grpLandmarks);


    m_grpSliceControls = new QGroupBox("Slice Navigation");
    m_grpSliceControls->setEnabled(false);
    auto* sliceLayout = new QVBoxLayout(m_grpSliceControls);
    sliceLayout->setSpacing(3);

    sliceLayout->addWidget(new QLabel("Slice number:"));
    auto* sliceNumRow = new QHBoxLayout;
    m_sliderSliceNum = new QSlider(Qt::Horizontal);
    m_sliderSliceNum->setRange(0, 49);
    m_sliderSliceNum->setValue(25);
    m_sliderSliceNum->setTickPosition(QSlider::TicksBelow);
    m_sliderSliceNum->setTickInterval(5);
    m_spinSliceNum = new QSpinBox;
    m_spinSliceNum->setRange(0, 49);
    m_spinSliceNum->setValue(25);
    m_spinSliceNum->setPrefix("Slice ");
    sliceNumRow->addWidget(m_sliderSliceNum, 3);
    sliceNumRow->addWidget(m_spinSliceNum, 1);
    sliceLayout->addLayout(sliceNumRow);

    m_lblSliceInfo = new QLabel("Slice 26/50 — d = 0.0 mm");
    m_lblSliceInfo->setAlignment(Qt::AlignCenter);
    m_lblSliceInfo->setStyleSheet(
        "font-weight:bold; color:#4CAF50; font-size:12px; "
        "padding:3px; background:#f0f8ff; border-radius:3px;");
    sliceLayout->addWidget(m_lblSliceInfo);

    m_btnLoadSlices = new QPushButton("Load Results...");
    m_btnLoadSlices->setStyleSheet(GREEN_BTN_SM);
    sliceLayout->addWidget(m_btnLoadSlices);
    leftLayout->addWidget(m_grpSliceControls);



    leftLayout->addStretch();
    splitter->addWidget(m_leftPanel);

    auto* rightPanel = new QWidget;
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_lblTitle = new QLabel("");
    m_lblTitle->setStyleSheet("font-size:13pt; font-weight:bold; color:#333; padding:4px;");
    m_lblTitle->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(m_lblTitle);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);  // scroll H si besoin
    m_scrollArea->setStyleSheet("QScrollArea { border:none; background:#f8f8f8; }");
    m_scrollContent = new QWidget;
    m_rightContentLayout = new QVBoxLayout(m_scrollContent);
    m_rightContentLayout->setSpacing(0);
    m_rightContentLayout->setContentsMargins(4, 0, 4, 4);

    m_scrollArea->setWidget(m_scrollContent);
    rightLayout->addWidget(m_scrollArea);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);  
    splitter->setStretchFactor(1, 2);  
    splitter->setStretchFactor(2, 6);  

    mainLayout->addWidget(splitter);
    statusBar()->showMessage("Ready — Load 2+ CBCT volumes to start");


    m_leftPanel->setVisible(false);
    m_grpVolumes->setVisible(false);
    m_grpLandmarks->setVisible(false);
    m_grpSliceControls->setVisible(false);
    displayStep1Attributes();
}

void MainWindow::connectSignals()
{
    connect(m_btnLoadDICOM,  &QPushButton::clicked, this, &MainWindow::onLoadDICOM);
    connect(m_btnLoadNIfTI,  &QPushButton::clicked, this, &MainWindow::onLoadNIfTI);
    connect(m_btnLoadSlices, &QPushButton::clicked, this, &MainWindow::onLoadSlicesFromDisk);
    connect(m_btnLoadJSON,   &QPushButton::clicked, this, &MainWindow::onLoadLandmarksJSON);
    connect(m_btnLoadCSV,    &QPushButton::clicked, this, &MainWindow::onLoadLandmarksCSV);
    connect(m_btnDetect,     &QPushButton::clicked, this, &MainWindow::onDetectLandmarks);
    connect(m_sliderSliceNum, &QSlider::valueChanged, this, &MainWindow::onSliceNumChanged);
    connect(m_spinSliceNum, QOverload<int>::of(&QSpinBox::valueChanged),
            m_sliderSliceNum, &QSlider::setValue);
    connect(m_stepList, &QListWidget::currentRowChanged,
            this, &MainWindow::onStepClicked);
}

void MainWindow::createMenuBar()
{

    auto* mFile = menuBar()->addMenu("&File");
    mFile->addAction("Open DICOM...", this, &MainWindow::onLoadDICOM);
    mFile->addAction("Open NIfTI...", this, &MainWindow::onLoadNIfTI);
    mFile->addAction("Load Results Folder...", this, &MainWindow::onLoadSlicesFromDisk);  // ← AJOUT
    mFile->addSeparator();
    mFile->addAction("Load Landmarks JSON...", this, &MainWindow::onLoadLandmarksJSON);
    mFile->addAction("Load Landmarks CSV...", this, &MainWindow::onLoadLandmarksCSV);
    mFile->addSeparator();
    mFile->addAction("Quit", this, &QMainWindow::close);


    auto* mLandmarks = menuBar()->addMenu("&Landmarks");
    mLandmarks->addAction("Detect (ALI_CBCT)...", [this]() {
        QMessageBox::information(this, "Landmark Detection",
            "Automatic cephalometric landmark detection via ALI_CBCT.\n"
            "(Integration in progress)");
    });
    mLandmarks->addAction("Edit Landmarks...", [this]() {
        QMessageBox::information(this, "Edit Landmarks",
            "Manual refinement of landmark coordinates.\n"
            "(Integration in progress)");
    });


    auto* mReg = menuBar()->addMenu("&Registration");
    mReg->addAction("Run Pipeline", this, &MainWindow::onRunPipeline);
    mReg->addSeparator();
    mReg->addAction("Registration Settings...", [this]() {
        QMessageBox::information(this, "Registration Settings",
            "Mutual Information bins, multi-resolution levels, sampling rate.\n"
            "(Integration in progress)");
    });


    auto* mView = menuBar()->addMenu("&View");
    mView->addAction("Comparison Grid (3xN)", [this]() {
        QMessageBox::information(this, "View",
            "Synchronized 3xN comparison grid (already shown in the main panel).");
    });
    mView->addAction("Overlay / Difference...", [this]() {
        QMessageBox::information(this, "Overlay / Difference",
            "Overlay and difference visualization of aligned slices.\n"
            "(Integration in progress)");
    });
    mView->addAction("MCAGPC Details...", [this]() {
        QMessageBox::information(this, "MCAGPC Details",
            "Detailed MCAGPC scores: SSIM, NCC, and landmark displacement per orientation.\n"
            "(Integration in progress)");
    });


    auto* mExport = menuBar()->addMenu("&Export");
    mExport->addAction("Export Aligned Volumes (NIfTI)...", [this]() {
        QMessageBox::information(this, "Export",
            "Export aligned volumes in NIfTI format.\n(Integration in progress)");
    });
    mExport->addAction("Export Slices (PNG)...", [this]() {
        QMessageBox::information(this, "Export",
            "Export 2D slices as structured PNG series.\n(Integration in progress)");
    });
    mExport->addAction("Export MCAGPC Report (CSV)...", [this]() {
        QMessageBox::information(this, "Export",
            "Export MCAGPC validation metrics as CSV.\n(Integration in progress)");
    });


    auto* mHelp = menuBar()->addMenu("&Help");
    mHelp->addAction("About", [this]() {
        QMessageBox::about(this, "CBCTAlign",
            "CBCTAlign v2.0\n\n"
            "Spatio-Temporal CBCT Alignment\n"
            "Guided by Cephalometric Landmarks\n\n"
            "MCAGPC Metric — SoftwareX 2026");
    });
}



void MainWindow::onLoadDICOM()
{

    QFileDialog dialog(this, "Select DICOM Folders (1 per timepoint, Ctrl/Shift+click for multi)");
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true); 
    dialog.setDirectory(lastDirOrDefault("lastDicomDir", DEFAULT_DICOM_DIR));


    QListView* listView = dialog.findChild<QListView*>("listView");
    if (listView) listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QTreeView* treeView = dialog.findChild<QTreeView*>();
    if (treeView) treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    if (dialog.exec() != QDialog::Accepted) return;

    QStringList dirs = dialog.selectedFiles();
    if (dirs.isEmpty()) return;

    rememberDir("lastDicomDir", dirs.first());


    dirs.sort();

    int loaded = 0;
    int failed = 0;

    for (const QString& dir : dirs) {

        if (!QDir(dir).exists()) {
            failed++;
            continue;
        }

        auto vol = std::make_shared<CBCTVolume>();
        if (vol->loadFromDICOM(dir)) {
            m_volumes.push_back(vol);
            m_volumeList->addItem(
                QString("T%1: %2").arg(m_volumes.size()-1)
                                  .arg(QFileInfo(dir).fileName()));
            loaded++;
            Logger::instance().info(
                QString("Loaded T%1 (DICOM): %2").arg(m_volumes.size()-1)
                                                  .arg(QFileInfo(dir).fileName()));
        } else {
            failed++;
            Logger::instance().info(
                QString("FAILED to load DICOM: %1").arg(QFileInfo(dir).fileName()));
        }
    }



    if (failed > 0) {
        QMessageBox::warning(this, "Partial Load",
            QString("Loaded %1 DICOM folder(s), failed to load %2 folder(s).\nCheck logs for details.")
                .arg(loaded).arg(failed));
    } else if (loaded > 0) {
        statusBar()->showMessage(
            QString("%1 DICOM volume(s) loaded").arg(loaded), 5000);
    }
    if (m_stepList->currentRow() == 0) displayStep1Attributes();
}
void MainWindow::onLoadNIfTI()
{

    QStringList paths = QFileDialog::getOpenFileNames(
        this, "Select NIfTI files (select 1 file per timepoint, in T0..Tn order)",
        lastDirOrDefault("lastNiftiDir", DEFAULT_NIFTI_DIR),
        "NIfTI (*.nii *.nii.gz)");

    if (paths.isEmpty()) return;
    rememberDir("lastNiftiDir", paths.first());


    paths.sort();

    int loaded = 0;
    int failed = 0;

    for (const QString& path : paths) {
        auto vol = std::make_shared<CBCTVolume>();
        if (vol->loadFromNIfTI(path)) {
            m_volumes.push_back(vol);
            m_volumePaths.push_back(path);   
            m_volumeList->addItem(
                QString("T%1: %2").arg(m_volumes.size()-1)
                                  .arg(QFileInfo(path).fileName()));
            loaded++;
            Logger::instance().info(
                QString("Loaded T%1: %2").arg(m_volumes.size()-1)
                                          .arg(QFileInfo(path).fileName()));
        } else {
            failed++;
            Logger::instance().info(
                QString("FAILED to load: %1").arg(QFileInfo(path).fileName()));
        }
    }



    if (failed > 0) {
        QMessageBox::warning(this, "Partial Load",
            QString("Loaded %1 file(s), failed to load %2 file(s).\nCheck logs for details.")
                .arg(loaded).arg(failed));
    } else {
        statusBar()->showMessage(
            QString("%1 NIfTI volume(s) loaded").arg(loaded), 5000);
    }
    if (m_stepList->currentRow() == 0) displayStep1Attributes();
}


void MainWindow::onLoadLandmarksJSON()
{

    QStringList fps = QFileDialog::getOpenFileNames(
        this,
        "Load Landmarks JSON (select 1 file per timepoint, in T0..Tn order)",
        lastDirOrDefault("lastLandmarkDir", DEFAULT_LANDMARK_DIR),
        "Slicer Markups (*.mrk.json *.json);;All (*)");

    if (fps.isEmpty()) return;
    rememberDir("lastLandmarkDir", fps.first());
    fps.sort();  
    m_allTimepointLandmarks.clear();
    m_manualLandmarks.clear();
    int totalLm = 0;

    for (int tp = 0; tp < fps.size(); ++tp) {
        QFileInfo fi(fps[tp]);
        QString pid = fi.completeBaseName().replace(".mrk", "")
                                            .split('_').value(0, "Patient");
        QString tpName = QString("T%1").arg(tp);

        auto entries = LandmarkIO::loadSlicerJSON(fps[tp], pid, tpName);
        if (entries.isEmpty()) {
            QMessageBox::warning(this, "Error",
                QString("No landmarks in %1").arg(fi.fileName()));
            continue;
        }
        auto lms = LandmarkIO::toLandmarks(entries, tpName);
        m_allTimepointLandmarks.push_back(lms);
        totalLm += lms.size();

        Logger::instance().info(QString("Loaded %1 landmarks from %2 -> %3")
            .arg(lms.size()).arg(fi.fileName()).arg(tpName));
    }


    if (!m_allTimepointLandmarks.empty()) {
        m_manualLandmarks = m_allTimepointLandmarks[0];  
    }
    m_useManualLandmarks = !m_manualLandmarks.empty();
    updateLandmarkTable();

    QString ref = "?";
    for (const auto& lm : m_manualLandmarks) {
        if (lm.name == "ANS"    || lm.abbreviation == "ANS") { ref = "ANS";    break; }
        if (lm.name == "Sella"  || lm.abbreviation == "S")   { ref = "Sella";  break; }
        if (lm.name == "Nasion" || lm.abbreviation == "N")   { ref = "Nasion"; break; }
    }

    statusBar()->showMessage(
        QString("%1 landmarks across %2 timepoints (ref: %3)")
            .arg(totalLm).arg(fps.size()).arg(ref), 8000);
}

void MainWindow::onLoadLandmarksCSV()
{
    QString fp = QFileDialog::getOpenFileName(
        this, "Load CSV",
        lastDirOrDefault("lastCsvDir", DEFAULT_CSV_DIR),
        "CSV (*.csv);;All (*)");
    if (fp.isEmpty()) return;
    rememberDir("lastCsvDir", fp);

    auto entries = LandmarkIO::load(fp);
    if (entries.isEmpty()) { QMessageBox::warning(this, "Error", "No landmarks found"); return; }

    m_manualLandmarks = LandmarkIO::toLandmarks(entries, "T0");
    if (m_manualLandmarks.empty()) {
        for (const auto& e : entries) {
            Landmark lm; lm.name=e.name; lm.position=e.position;
            lm.confidence=e.confidence; lm.abbreviation=e.name.left(2);
            m_manualLandmarks.push_back(lm);
        }
    }
    m_useManualLandmarks = !m_manualLandmarks.empty();
    updateLandmarkTable();
    statusBar()->showMessage(QString("%1 landmarks loaded").arg(m_manualLandmarks.size()), 5000);
}

void MainWindow::onDetectLandmarks()
{

    QString niftiPath = QFileDialog::getOpenFileName(
        this, "Select NIfTI volume for landmark detection",
        lastDirOrDefault("lastNiftiDir", DEFAULT_NIFTI_DIR),
        "NIfTI (*.nii *.nii.gz)");
    if (niftiPath.isEmpty()) return;
    rememberDir("lastNiftiDir", niftiPath);


    const QString aliBase   = qEnvironmentVariable(
        "ALI_CBCT_HOME", QDir::homePath() + "/ALI_CBCT_home");
    const QString pySlicer  = aliBase + "/Slicer-5.6.2-linux-amd64/bin/PythonSlicer";
    const QString aliScript = aliBase + "/SlicerAutomatedDentalTools/ALI_CBCT/ALI_CBCT.py";
    const QString models    = aliBase + "/ALI_CBCT_models/Upper_Bones_v2";
    const QString outDir    = "/tmp/ali_out";
    const QString tempDir   = "/tmp/ali_temp";


    if (!QFileInfo::exists(pySlicer) || !QFileInfo::exists(aliScript)) {
        QMessageBox::critical(this, "ALI_CBCT not found",
            "PythonSlicer or ALI_CBCT.py not found.\nCheck paths in onDetectLandmarks().");
        return;
    }
    QDir().mkpath(outDir);
    QDir().mkpath(tempDir);


    QStringList args;
    args << aliScript
         << niftiPath
         << models
         << "\"ANS\""            
         << outDir
         << tempDir
         << "false"
         << "[1,0.3]"
         << "[1,1]"
         << "[64,64,64]"
         << "10";


    m_btnDetect->setEnabled(false);
    m_btnDetect->setText("Detecting...");
    statusBar()->showMessage("Running ALI_CBCT detection...");
    QApplication::processEvents();  

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(pySlicer, args);

    if (!proc.waitForStarted(5000)) {
        QMessageBox::critical(this, "Error", "Failed to start ALI_CBCT process.");
        m_btnDetect->setEnabled(true);
        m_btnDetect->setText("🔍 Detect (ALI_CBCT)");
        return;
    }


    proc.waitForFinished(300000);

    QString output = proc.readAll();
    Logger::instance().info("ALI_CBCT output:\n" + output);

    m_btnDetect->setEnabled(true);
    m_btnDetect->setText("🔍 Detect (ALI_CBCT)");


    QFileInfo niftiInfo(niftiPath);
    QString baseName = niftiInfo.completeBaseName().replace(".nii", "");

    QDir od(outDir);
    QStringList jsons = od.entryList({"*_lm_Pred_*.mrk.json"}, QDir::Files, QDir::Time);
    if (jsons.isEmpty()) {
        QMessageBox::warning(this, "No result",
            "Detection finished but no JSON was produced.\nCheck logs for ALI_CBCT errors.");
        return;
    }

    QString jsonPath = od.filePath(jsons.first());  

    auto entries = LandmarkIO::loadSlicerJSON(jsonPath, baseName, "T0");
    if (entries.isEmpty()) {
        QMessageBox::warning(this, "Parse error",
            QString("Could not parse landmarks from %1").arg(jsonPath));
        return;
    }
    auto lms = LandmarkIO::toLandmarks(entries, "T0");


    m_allTimepointLandmarks.clear();
    m_allTimepointLandmarks.push_back(lms);
    m_manualLandmarks = lms;
    m_useManualLandmarks = true;
    updateLandmarkTable();

    statusBar()->showMessage(
        QString("Detected %1 landmark(s) from %2")
            .arg(lms.size()).arg(niftiInfo.fileName()), 8000);
    QMessageBox::information(this, "Detection complete",
        QString("ALI_CBCT detected %1 landmark(s).\nSaved JSON: %2")
            .arg(lms.size()).arg(jsonPath));
}

void MainWindow::updateLandmarkTable()
{
    m_tableLandmarks->setRowCount(0);
    if (m_allTimepointLandmarks.empty()) {
        m_lblLandmarkStatus->setText("No landmarks loaded");
        m_lblLandmarkStatus->setStyleSheet("color:#999; font-size:11px; font-style:italic;");
        return;
    }


    m_tableLandmarks->setColumnCount(6);
    m_tableLandmarks->setHorizontalHeaderLabels({"TP", "Name", "Abbr.", "X (mm)", "Y (mm)", "Z (mm)"});
    m_tableLandmarks->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);


    int totalRows = 0;
    for (const auto& tpLms : m_allTimepointLandmarks)
        totalRows += tpLms.size();
    m_tableLandmarks->setRowCount(totalRows);

    int row = 0;
    QColor tpColors[] = {QColor(33,150,243,40),    
                         QColor(76,175,80,40),    
                         QColor(255,152,0,40)};    

    for (size_t tp = 0; tp < m_allTimepointLandmarks.size(); ++tp) {
        const auto& lms = m_allTimepointLandmarks[tp];
        QColor bg = tpColors[tp % 3];
        QString tpName = QString("T%1").arg(tp);

        for (const auto& lm : lms) {
            auto* c0 = new QTableWidgetItem(tpName);
            auto* c1 = new QTableWidgetItem(lm.name);
            auto* c2 = new QTableWidgetItem(lm.abbreviation);
            auto* c3 = new QTableWidgetItem(QString::number(lm.position.x(),'f',2));
            auto* c4 = new QTableWidgetItem(QString::number(lm.position.y(),'f',2));
            auto* c5 = new QTableWidgetItem(QString::number(lm.position.z(),'f',2));

            c0->setTextAlignment(Qt::AlignCenter);
            c0->setFont(QFont("",-1,QFont::Bold));
            c2->setTextAlignment(Qt::AlignCenter);
            c3->setTextAlignment(Qt::AlignCenter);
            c4->setTextAlignment(Qt::AlignCenter);
            c5->setTextAlignment(Qt::AlignCenter);

            for (auto* x : {c0, c1, c2, c3, c4, c5})
                x->setBackground(bg);

            m_tableLandmarks->setItem(row, 0, c0);
            m_tableLandmarks->setItem(row, 1, c1);
            m_tableLandmarks->setItem(row, 2, c2);
            m_tableLandmarks->setItem(row, 3, c3);
            m_tableLandmarks->setItem(row, 4, c4);
            m_tableLandmarks->setItem(row, 5, c5);
            row++;
        }
    }

    int totalLm = 0;
    for (const auto& tpLms : m_allTimepointLandmarks)
        totalLm += tpLms.size();
    m_lblLandmarkStatus->setText(
        QString("%1 landmarks across %2 timepoints").arg(totalLm).arg(m_allTimepointLandmarks.size()));
    m_lblLandmarkStatus->setStyleSheet("color:#2E7D32; font-size:11px; font-weight:bold;");

    if (m_stepList && m_stepList->currentRow() == 1) displayLandmarksStep();  
}



void MainWindow::onRunPipeline()
{
    if (m_volumes.size()<2) { QMessageBox::warning(this,"Error","At least 2 volumes required!"); return; }

    m_currentOutputDir = QFileDialog::getExistingDirectory(
        this, "Select Output Folder",
        lastDirOrDefault("lastOutputDir", DEFAULT_OUTPUT_DIR));
    if (m_currentOutputDir.isEmpty()) return;
    rememberDir("lastOutputDir", m_currentOutputDir);

    std::vector<std::shared_ptr<CBCTVolume>> work;
    for (size_t i=0; i<m_volumes.size(); ++i) {
        statusBar()->showMessage(QString("Resampling T%1...").arg(i));
        auto c=std::make_shared<CBCTVolume>();
        c->setITKImage(m_volumes[i]->getITKImage());
        c->setName(m_volumes[i]->getName());
        c->resampleIsotropic(0.3);
        work.push_back(c);
    }

    m_pipeline = std::make_unique<PipelineManager>(this);
    connect(m_pipeline.get(), &PipelineManager::progressUpdated, this, &MainWindow::onPipelineProgress);
    connect(m_pipeline.get(), &PipelineManager::pipelineFinished, this, &MainWindow::onPipelineFinished);
    m_pipeline->setLoadedVolumes(work);
    if (m_useManualLandmarks && !m_manualLandmarks.empty()) {

        if (m_allTimepointLandmarks.size() >= 2)
            m_pipeline->setManualLandmarksMulti(m_allTimepointLandmarks);
        else
            m_pipeline->setManualLandmarks(m_manualLandmarks);
    }

    statusBar()->showMessage("Pipeline running...");

    QString ref="Sella";
    for (const auto& lm:m_manualLandmarks) {
        if (lm.name=="Sella"||lm.abbreviation=="S") { ref="Sella"; break; }
        if (lm.name=="Nasion"||lm.abbreviation=="N") ref="Nasion";
    }
    m_pipeline->runFullPipeline(QStringList(), m_currentOutputDir, ref, 0.0, 300, 25.0);
}

void MainWindow::onPipelineProgress(int pct, const QString& msg)
{
    statusBar()->showMessage(QString("%1 (%2%)").arg(msg).arg(pct));
}

void MainWindow::onPipelineFinished(bool ok)
{
    if (ok) {
        statusBar()->showMessage("Pipeline completed!", 5000);
        m_resultsDir = m_currentOutputDir;
        scanResultsDirectory();
        m_grpSliceControls->setEnabled(true);
        m_pipelineCompleted = true;
        if (!m_timepointDirs.isEmpty()) {
            if (m_stepList->currentRow() == 4) displayCurrentSlices();
            else m_stepList->setCurrentRow(4);
        }
        QMessageBox::information(this, "Success",
            QString("Pipeline completed!\nSlices saved to:\n%1").arg(m_currentOutputDir));
    } else {
        statusBar()->showMessage("Pipeline failed", 5000);
        QMessageBox::critical(this, "Error", "Pipeline failed. Check logs.");
    }
    m_pipeline.reset();
}

// Slice Navigation

void MainWindow::onSliceNumChanged(int value)
{
    m_currentSliceNum = value;
    m_spinSliceNum->setValue(value);
    double d = -25.0 + value * 50.0 / (m_numSlices - 1);
    m_lblSliceInfo->setText(QString("Slice %1/%2 — d = %3 mm")
        .arg(value+1).arg(m_numSlices).arg(d,0,'f',1));
    displayCurrentSlices();
}

void MainWindow::onLoadSlicesFromDisk()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Results Folder (T0/, T1/, ...)",
        lastDirOrDefault("lastResultsDir", DEFAULT_OUTPUT_DIR));
    if (dir.isEmpty()) return;
    rememberDir("lastResultsDir", dir);

    m_resultsDir = dir;
    scanResultsDirectory();
    m_grpSliceControls->setEnabled(true);
    if (!m_timepointDirs.isEmpty()) {
        if (m_stepList->currentRow() == 4) displayCurrentSlices();
        else m_stepList->setCurrentRow(4);
    }
}

void MainWindow::scanResultsDirectory()
{
    m_timepointDirs.clear();
    QDir dir(m_resultsDir);
    for (const auto& e : dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot, QDir::Name)) {
        if (e.startsWith("T")) {
            QDir tp(QDir(m_resultsDir).filePath(e));
            if (tp.exists("Axial")||tp.exists("Coronal")||tp.exists("Sagittal"))
                m_timepointDirs.append(e);
        }
    }
    m_timepointDirs.sort();
    if (m_timepointDirs.isEmpty()) {
        QMessageBox::warning(this, "Empty", "No T0/, T1/... folders found.");
        return;
    }
    QDir first(QDir(m_resultsDir).filePath(m_timepointDirs[0]+"/Axial"));
    m_numSlices = first.entryList({"slice_*.png"}, QDir::Files).size();
    if (m_numSlices==0) m_numSlices=50;

    m_sliderSliceNum->setRange(0, m_numSlices-1);
    m_spinSliceNum->setRange(0, m_numSlices-1);


    m_sliderSliceNum->blockSignals(true);
    m_spinSliceNum->blockSignals(true);
    m_sliderSliceNum->setValue(m_numSlices/2);
    m_spinSliceNum->setValue(m_numSlices/2);
    m_sliderSliceNum->blockSignals(false);
    m_spinSliceNum->blockSignals(false);
    m_currentSliceNum = m_numSlices/2;
    statusBar()->showMessage(QString("%1 timepoints: %2").arg(m_timepointDirs.size()).arg(m_timepointDirs.join(", ")), 5000);
}


void MainWindow::onStepClicked(int row)
{


    m_leftPanel->setVisible(false);

    m_grpVolumes->setVisible(false);
    if (m_grpDetect) m_grpDetect->setVisible(row == 1);
    m_grpLandmarks->setVisible(row == 1);
    m_grpSliceControls->setVisible(false);      

    if (row == 0) {
        displayStep1Attributes();
    } else if (row == 1) {
        displayLandmarksStep();
    } else if (row == 4) {
        displayCurrentSlices();
    } else if (row == 2) {
        displayRegistrationStep();
    } else if (row == 3) {
        displayExtractionStep();
    }
}


void MainWindow::displayStep1Attributes()
{
    QLayoutItem* child;
    while ((child = m_rightContentLayout->takeAt(0)) != nullptr) {
        if (child->layout()) {
            QLayoutItem* sub;
            while ((sub = child->layout()->takeAt(0)) != nullptr) {
                if (sub->widget()) delete sub->widget();
                delete sub;
            }
        }
        if (child->widget()) delete child->widget();
        delete child;
    }

    m_lblTitle->setText("Step 1 — Load CBCT Volumes");


    auto* dropZone = new QWidget;
    dropZone->setObjectName("dropZone");
    dropZone->setAttribute(Qt::WA_StyledBackground, true);
    dropZone->setStyleSheet(
        "QWidget#dropZone { border:2px dashed #bbb; border-radius:8px; background:#fafafa; }");
    auto* dz = new QVBoxLayout(dropZone);
    dz->setContentsMargins(20, 32, 20, 32);
    dz->setSpacing(10);

    auto* dzIcon = new QLabel("⬆");
    dzIcon->setAlignment(Qt::AlignCenter);
    dzIcon->setStyleSheet("font-size:34px; color:#aaa; border:none; background:transparent;");
    dz->addWidget(dzIcon);

    auto* dzText = new QLabel("Glisser des fichiers CBCT ici, ou");
    dzText->setAlignment(Qt::AlignCenter);
    dzText->setStyleSheet("font-size:13px; color:#888; border:none; background:transparent;");
    dz->addWidget(dzText);

    auto* dzBtnRow = new QHBoxLayout;
    dzBtnRow->addStretch();
    auto* btnD = new QPushButton("DICOM");
    auto* btnN = new QPushButton("NIfTI");
    btnD->setStyleSheet(GREEN_BTN);
    btnN->setStyleSheet(GREEN_BTN);
    btnD->setFixedWidth(130);
    btnN->setFixedWidth(130);
    connect(btnD, &QPushButton::clicked, this, &MainWindow::onLoadDICOM);
    connect(btnN, &QPushButton::clicked, this, &MainWindow::onLoadNIfTI);
    dzBtnRow->addWidget(btnD);
    dzBtnRow->addWidget(btnN);
    dzBtnRow->addStretch();
    dz->addLayout(dzBtnRow);

    m_rightContentLayout->addWidget(dropZone);


    if (m_volumes.empty()) {
        auto* hint = new QLabel("No CBCT volumes loaded yet.");
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color:#999; font-size:13px; padding:24px;");
        m_rightContentLayout->addWidget(hint);
        m_rightContentLayout->addStretch();
        return;
    }

    int nVol = static_cast<int>(m_volumes.size());
    auto* table = new QTableWidget(nVol, 5);
    table->setHorizontalHeaderLabels(
        {"Vol", "Shape (voxels)", "Spacing (mm)", "Origin (mm)", "FOV (mm)"});
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setStyleSheet(
        "QTableWidget { font-size:12px; }"
        "QHeaderView::section { background:#e0e0e0; font-weight:bold; font-size:11px; padding:5px; }");

    QColor tpColors[] = {QColor(33,150,243,30), QColor(76,175,80,30), QColor(255,152,0,30)};

    for (int i = 0; i < nVol; ++i) {
        auto sz = m_volumes[i]->getSize();
        auto sp = m_volumes[i]->getSpacing();
        auto og = m_volumes[i]->getOrigin();

        QString shapeStr = QString("%1x%2x%3").arg(sz.x()).arg(sz.y()).arg(sz.z());
        QString spacingStr = QString::number(sp.x(), 'f', 2);
        QString originStr = QString("%1, %2, %3")
            .arg(og.x(), 0, 'f', 1).arg(og.y(), 0, 'f', 1).arg(og.z(), 0, 'f', 1);
        double fovX = sz.x() * sp.x(), fovY = sz.y() * sp.y(), fovZ = sz.z() * sp.z();
        QString fovStr = QString("%1x%2x%3")
            .arg(fovX, 0, 'f', 0).arg(fovY, 0, 'f', 0).arg(fovZ, 0, 'f', 0);

        auto* cVol    = new QTableWidgetItem(QString("T%1").arg(i));
        cVol->setFont(QFont("", -1, QFont::Bold));
        auto* cShape  = new QTableWidgetItem(shapeStr);
        auto* cSpace  = new QTableWidgetItem(spacingStr);
        auto* cOrigin = new QTableWidgetItem(originStr);
        auto* cFov    = new QTableWidgetItem(fovStr);

        QColor bg = tpColors[i % 3];
        for (auto* c : {cVol, cShape, cSpace, cOrigin, cFov}) {
            c->setTextAlignment(Qt::AlignCenter);
            c->setBackground(bg);
        }

        table->setItem(i, 0, cVol);
        table->setItem(i, 1, cShape);
        table->setItem(i, 2, cSpace);
        table->setItem(i, 3, cOrigin);
        table->setItem(i, 4, cFov);
    }

    table->setMaximumHeight(60 + nVol * 38);
    m_rightContentLayout->addWidget(table);
    m_rightContentLayout->addStretch();
}

void MainWindow::displayLandmarksStep()
{
    QLayoutItem* child;
    while ((child = m_rightContentLayout->takeAt(0)) != nullptr) {
        if (child->layout()) {
            QLayoutItem* sub;
            while ((sub = child->layout()->takeAt(0)) != nullptr) {
                if (sub->widget()) delete sub->widget();
                delete sub;
            }
        }
        if (child->widget()) delete child->widget();
        delete child;
    }

    m_lblTitle->setText("Step 2 — Cephalometric Landmark Detection");

    auto* card = new QWidget;
    card->setObjectName("lmCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(
        "QWidget#lmCard { background:#ffffff; border:1px solid #e0e0e0; border-radius:10px; }");
    card->setMinimumWidth(720);
    card->setMaximumWidth(900);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(12);
    card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);


    auto* lblModel = new QLabel("ALI_CBCT model");
    lblModel->setStyleSheet("font-size:13px; color:#666; border:none;");
    cardLayout->addWidget(lblModel);

    m_comboModel = new QComboBox;
    m_comboModel->addItem("Upper_Bones_v2 — 14 landmarks");
    m_comboModel->addItem("Cranial_Base — 10 landmarks");
    m_comboModel->addItem("models_for_ali — ANS, S");
    m_comboModel->setStyleSheet("QComboBox { font-size:14px; padding:6px; }");
    if (!m_detectModel.isEmpty()) m_comboModel->setCurrentText(m_detectModel);
    cardLayout->addWidget(m_comboModel);


    auto* lblLm = new QLabel("Landmarks to detect");
    lblLm->setStyleSheet("font-size:13px; color:#666; border:none; margin-top:6px;");
    cardLayout->addWidget(lblLm);

    auto* checksContainer = new QWidget;
    auto* checksGrid = new QGridLayout(checksContainer);
    checksGrid->setContentsMargins(0, 0, 0, 0);
    checksGrid->setHorizontalSpacing(18);
    checksGrid->setVerticalSpacing(4);
    cardLayout->addWidget(checksContainer);


    m_lblFovNote = new QLabel;
    m_lblFovNote->setWordWrap(true);
    m_lblFovNote->setStyleSheet("color:#E65100; font-size:10px; font-style:italic; border:none;");
    m_lblFovNote->setVisible(false);

    double fovZ = 0.0;
    if (!m_volumes.empty()) {
        auto sz = m_volumes[0]->getSize();
        auto sp = m_volumes[0]->getSpacing();
        fovZ = sz.z() * sp.z();
    }
    const bool reducedFov = (fovZ > 0.0 && fovZ < 70.0);

    auto fillChecks = [this, checksGrid, reducedFov](const QString& model) {
        QLayoutItem* it;
        while ((it = checksGrid->takeAt(0)) != nullptr) {
            if (it->widget()) delete it->widget();
            delete it;
        }
        m_landmarkChecks.clear();


        QString modelDir = "Upper_Bones_v2";
        QString def = "ANS";
        if (model.startsWith("Cranial_Base"))      { modelDir = "Cranial_Base";  def = "S";   }
        else if (model.startsWith("models_for_ali")){ modelDir = "models_for_ali"; def = "ANS"; }


        const QString aliBase = qEnvironmentVariable(
            "ALI_CBCT_HOME", QDir::homePath() + "/ALI_CBCT_home");
        QDir md(aliBase + "/ALI_CBCT_models/" + modelDir);
        QStringList pts = md.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);


        const QStringList peripheral = QStringList()
            << "S" << "N" << "Ba" << "LPo" << "RPo" << "LFZyg" << "RFZyg"
            << "ROr" << "LOr" << "RInfOr" << "LInfOr" << "RNC" << "LNC";

        if (pts.isEmpty()) {  
            m_lblFovNote->setText("⚠ Model folder not found: " + md.absolutePath());
            m_lblFovNote->setVisible(true);
            return;
        }

        int col = 0, rowg = 0;
        for (const QString& p : pts) {
            auto* cb = new QCheckBox(p);
            cb->setStyleSheet("QCheckBox { font-size:14px; padding:2px; }");
            if (p == def) { cb->setChecked(true); cb->setText(p + "  (ref)"); }
            if (reducedFov && peripheral.contains(p)) {
                cb->setChecked(false);
                cb->setEnabled(false);
                cb->setStyleSheet("QCheckBox { font-size:14px; padding:2px; color:#aaa; }");
            }
            m_landmarkChecks[p] = cb;
            checksGrid->addWidget(cb, rowg, col);
            if (++col == 2) { col = 0; ++rowg; }
        }

        if (reducedFov) {
            m_lblFovNote->setText(
                "⚠ Reduced FOV (Z < 70 mm): peripheral landmarks disabled "
                "(absent in a reduced dental CBCT).");
            m_lblFovNote->setVisible(true);
        } else {
            m_lblFovNote->setVisible(false);
        }
    };
    fillChecks(m_comboModel->currentText());
    connect(m_comboModel, &QComboBox::currentTextChanged, this,
            [this, fillChecks](const QString& t) {
                m_detectModel = t;
                fillChecks(t);
            });

    cardLayout->addWidget(m_lblFovNote);


    auto* btnLaunch = new QPushButton("Detect landmarks");
    btnLaunch->setStyleSheet(
        "QPushButton { background-color:#4CAF50; color:white; font-weight:bold; "
        "padding:9px; border-radius:6px; font-size:13px; }"
        "QPushButton:hover { background-color:#45a049; }"
        "QPushButton:disabled { background-color:#ccc; color:#666; }");
    connect(btnLaunch, &QPushButton::clicked, this, [this]() {
        QTimer::singleShot(0, this, [this]() { onDetectLandmarksRich(); });
    });
    cardLayout->addWidget(btnLaunch);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:#e0e0e0; border:none;");
    cardLayout->addWidget(sep);


    if (m_allTimepointLandmarks.empty()) {
        auto* hint = new QLabel("No landmarks detected yet.\nClick \"Detect landmarks\".");
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color:#999; font-size:12px; padding:18px; border:none;");
        cardLayout->addWidget(hint);
    } else {
        int totalRows = 0;
        for (const auto& tp : m_allTimepointLandmarks) totalRows += static_cast<int>(tp.size());

        auto* table = new QTableWidget(totalRows, 6);
        table->setHorizontalHeaderLabels({"TP", "Name", "Abbr.", "X (mm)", "Y (mm)", "Z (mm)"});
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setStyleSheet(
            "QTableWidget { font-size:13px; border:1px solid #eee; border-radius:6px; }"
            "QHeaderView::section { background:#e0e0e0; font-weight:bold; font-size:12px; padding:6px; }");

        QColor tpColors[] = {QColor(33,150,243,40), QColor(76,175,80,40), QColor(255,152,0,40)};
        int r = 0;
        for (size_t tp = 0; tp < m_allTimepointLandmarks.size(); ++tp) {
            QColor bg = tpColors[tp % 3];
            QString tpName = QString("T%1").arg(tp);
            for (const auto& lm : m_allTimepointLandmarks[tp]) {
                QString vals[6] = {
                    tpName, lm.name, lm.abbreviation,
                    QString::number(lm.position.x(), 'f', 2),
                    QString::number(lm.position.y(), 'f', 2),
                    QString::number(lm.position.z(), 'f', 2)
                };
                for (int c = 0; c < 6; ++c) {
                    auto* it = new QTableWidgetItem(vals[c]);
                    it->setTextAlignment(Qt::AlignCenter);
                    it->setBackground(bg);
                    if (c == 0) it->setFont(QFont("", -1, QFont::Bold));
                    table->setItem(r, c, it);
                }
                ++r;
            }
        }

        const int headerH = 38;
        const int rowH = 40;
        const int tableH = headerH + totalRows * rowH + 4;
        table->setMinimumHeight(tableH);
        table->setMaximumHeight(tableH);
        table->verticalHeader()->setDefaultSectionSize(rowH);
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        cardLayout->addWidget(table);

        auto* status = new QLabel(QString("%1 landmark(s) · %2 timepoint(s)")
                                  .arg(totalRows).arg(m_allTimepointLandmarks.size()));
        status->setAlignment(Qt::AlignCenter);
        status->setStyleSheet("color:#2E7D32; font-size:12px; font-weight:bold; border:none;");
        cardLayout->addWidget(status);
    }

    auto* centerRow = new QHBoxLayout;
    centerRow->addStretch(1);
    centerRow->addWidget(card, 0, Qt::AlignTop);
    centerRow->addStretch(1);
    m_rightContentLayout->addLayout(centerRow);
    m_rightContentLayout->addStretch();
}


void MainWindow::onDetectLandmarksRich()
{

    QStringList niftiPaths;
    for (const QString& p : m_volumePaths) {
        if (p.endsWith(".nii") || p.endsWith(".nii.gz"))
            niftiPaths << p;
    }
    if (niftiPaths.isEmpty()) {

        QStringList chosen = QFileDialog::getOpenFileNames(
            this, "Select NIfTI volume(s) for detection (Ctrl/Shift+click for multiple)",
            lastDirOrDefault("lastNiftiDir", DEFAULT_NIFTI_DIR),
            "NIfTI (*.nii *.nii.gz)");
        if (chosen.isEmpty()) return;
        chosen.sort();                       
        rememberDir("lastNiftiDir", chosen.first());
        niftiPaths = chosen;
    }


    QStringList selected;
    for (auto it = m_landmarkChecks.begin(); it != m_landmarkChecks.end(); ++it) {
        if (it.value()->isChecked() && it.value()->isEnabled())
            selected << it.key();
    }
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "No landmark", "Select at least one landmark to detect.");
        return;
    }


    QStringList quoted;
    for (const QString& lm : selected) quoted << QString("\"%1\"").arg(lm);
    const QString lmArg = quoted.join(",");


    QString model = "Upper_Bones_v2";
    if (m_comboModel) {
        const int idx = m_comboModel->currentIndex();
        if (idx == 1) model = "Cranial_Base";
        else if (idx == 2) model = "models_for_ali";
    }


    const QString aliBase   = qEnvironmentVariable(
        "ALI_CBCT_HOME", QDir::homePath() + "/ALI_CBCT_home");
    const QString pySlicer  = aliBase + "/Slicer-5.6.2-linux-amd64/bin/PythonSlicer";
    const QString aliScript = aliBase + "/SlicerAutomatedDentalTools/ALI_CBCT/ALI_CBCT.py";
    const QString modelsDir = aliBase + "/ALI_CBCT_models/" + model;
    const QString outDir    = "/tmp/ali_out";
    const QString tempDir   = "/tmp/ali_temp";

    if (!QFileInfo::exists(pySlicer) || !QFileInfo::exists(aliScript)) {
        QMessageBox::critical(this, "ALI_CBCT not found",
            "PythonSlicer or ALI_CBCT.py not found.");
        return;
    }
    if (!QFileInfo::exists(modelsDir)) {
        QMessageBox::critical(this, "Model not found",
            QString("Model directory not found:\n%1").arg(modelsDir));
        return;
    }
    QDir().mkpath(outDir);
    QDir().mkpath(tempDir);


    m_allTimepointLandmarks.clear();
    QStringList failedAll;   

    for (int tp = 0; tp < niftiPaths.size(); ++tp) {
        const QString niftiPath = niftiPaths[tp];
        const QString tpName = QString("T%1").arg(tp);
        QFileInfo niftiInfo(niftiPath);

        statusBar()->showMessage(
            QString("ALI_CBCT : %1 (%2 landmark(s))...").arg(tpName).arg(selected.size()));
        QApplication::processEvents();


        QString baseName = niftiInfo.completeBaseName();
        baseName.replace(".nii", "");
        {
            QDir od(outDir);
            for (const QString& old : od.entryList({baseName + "_lm_Pred_*.mrk.json"}, QDir::Files))
                od.remove(old);
        }

        QStringList args;
        args << aliScript << niftiPath << modelsDir << lmArg
             << outDir << tempDir << "false" << "[1,0.3]" << "[1,1]" << "[64,64,64]" << "10";

        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(pySlicer, args);
        if (!proc.waitForStarted(5000)) {
            failedAll << QString("%1: process start failed").arg(tpName);
            continue;
        }
        proc.waitForFinished(300000);
        QString output = proc.readAll();
        Logger::instance().info(QString("[%1] ALI_CBCT output:\n").arg(tpName) + output);


        for (const QString& lm : selected)
            if (output.contains(lm + " landmark not found") ||
                output.contains("Fails for " + lm))
                failedAll << QString("%1 / %2").arg(tpName, lm);


        QDir od(outDir);
        QStringList jsons = od.entryList({baseName + "_lm_Pred_*.mrk.json"}, QDir::Files, QDir::Time);
        if (jsons.isEmpty()) {

            m_allTimepointLandmarks.push_back({});
            continue;
        }
        const QString jsonPath = od.filePath(jsons.first());


        const QString destJson = niftiInfo.absolutePath() + "/" + jsons.first();
        QFile::remove(destJson);                 
        if (QFile::copy(jsonPath, destJson))
            Logger::instance().info("Saved landmarks JSON: " + destJson);
        else
            Logger::instance().info("WARNING: could not copy JSON to " + destJson);

        auto entries = LandmarkIO::loadSlicerJSON(jsonPath, baseName, tpName);
        auto lms = LandmarkIO::toLandmarks(entries, tpName);
        m_allTimepointLandmarks.push_back(lms);
    }


    if (!m_allTimepointLandmarks.empty())
        m_manualLandmarks = m_allTimepointLandmarks[0];
    m_useManualLandmarks = !m_manualLandmarks.empty();


    displayLandmarksStep();


    int totalTP = m_allTimepointLandmarks.size();
    int totalLm = 0;
    for (const auto& v : m_allTimepointLandmarks) totalLm += static_cast<int>(v.size());
    statusBar()->showMessage(
        QString("Detection done: %1 landmark(s) across %2 timepoint(s)").arg(totalLm).arg(totalTP), 8000);

    if (!failedAll.isEmpty()) {
        QMessageBox::warning(this, "Detection incomplete",
            QString("Some landmarks could not be located (likely outside the FOV):\n\n%1")
                .arg(failedAll.join("\n")));
    }
}

void MainWindow::displayRegistrationStep()
{

    QLayoutItem* child;
    while ((child = m_rightContentLayout->takeAt(0)) != nullptr) {
        if (child->layout()) {
            QLayoutItem* sub;
            while ((sub = child->layout()->takeAt(0)) != nullptr) {
                if (sub->widget()) delete sub->widget();
                delete sub;
            }
        }
        if (child->widget()) delete child->widget();
        delete child;
    }

    m_lblTitle->setText("Step 3 — Rigid Registration");

    auto* card = new QWidget;
    card->setObjectName("regCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(
        "QWidget#regCard { background:#ffffff; border:1px solid #e0e0e0; border-radius:10px; }");
    card->setMinimumWidth(720);
    card->setMaximumWidth(900);
    card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(12);


    auto* lblRef = new QLabel("Reference landmark (fixed anchor)");
    lblRef->setStyleSheet("font-size:13px; color:#666; border:none;");
    cardLayout->addWidget(lblRef);

    m_comboRefLm = new QComboBox;

    if (!m_allTimepointLandmarks.empty() && !m_allTimepointLandmarks[0].empty()) {
        for (const auto& lm : m_allTimepointLandmarks[0])
            m_comboRefLm->addItem(QString("%1 (T0)").arg(lm.name));
    } else {
        m_comboRefLm->addItem("ANS (T0)");
    }
    m_comboRefLm->setStyleSheet("QComboBox { font-size:14px; padding:6px; }");
    cardLayout->addWidget(m_comboRefLm);

 
    auto* lblMetric = new QLabel(""); lblMetric->setVisible(false);
    lblMetric->setStyleSheet("font-size:12px; color:#888; border:none;");
    cardLayout->addWidget(lblMetric);


    auto* thrRow = new QHBoxLayout;
    auto* lblThr = new QLabel(""); lblThr->setVisible(false);
    m_spinDeltaThr = new QDoubleSpinBox(this);
    m_spinDeltaThr->setRange(0.1, 10.0);
    m_spinDeltaThr->setSingleStep(0.1);
    m_spinDeltaThr->setValue(2.0);
    m_spinDeltaThr->setVisible(false);
    thrRow->addStretch();
    cardLayout->addLayout(thrRow);


    auto* btnRun = new QPushButton("Run Registration");
    btnRun->setStyleSheet(
        "QPushButton { background-color:#4CAF50; color:white; font-weight:bold; "
        "padding:9px; border-radius:6px; font-size:13px; }"
        "QPushButton:hover { background-color:#45a049; }"
        "QPushButton:disabled { background-color:#ccc; color:#666; }");
    connect(btnRun, &QPushButton::clicked, this, [this]() {
        QTimer::singleShot(0, this, [this]() { onRunRegistration(); });
    });
    cardLayout->addWidget(btnRun);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:#e0e0e0; border:none;");
    cardLayout->addWidget(sep);


    if (m_registrationReports.empty()) {
        auto* hint = new QLabel("No registration yet.\nLoad volumes + landmarks (Steps 1-2), then click \"Run Registration\".");
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color:#999; font-size:12px; padding:18px; border:none;");
        cardLayout->addWidget(hint);
    } else {
        const int nRows = static_cast<int>(m_registrationReports.size());
        auto* table = new QTableWidget(nRows, 5);
        table->setHorizontalHeaderLabels(
            {"TP", "ΔLandmark (mm)", "Status", "MI", "Time"});
        table->setColumnHidden(1, true);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setStyleSheet(
            "QTableWidget { font-size:13px; border:1px solid #eee; border-radius:6px; }"
            "QHeaderView::section { background:#e0e0e0; font-weight:bold; font-size:12px; padding:6px; }");

        for (int r = 0; r < nRows; ++r) {
            const auto& rep = m_registrationReports[r];
            const bool ok = rep.converged && rep.deltaLandmark >= 0.0
                            && rep.deltaLandmark < m_spinDeltaThr->value();

            QString deltaStr = rep.deltaLandmark >= 0.0
                ? QString::number(rep.deltaLandmark, 'f', 2) : "—";
            QString statusStr = !rep.converged ? "✗ failed"
                              : ok ? "✓ OK" : "⚠ > thr";
            QString miStr = QString::number(rep.finalMetric, 'f', 4);
            QString timeStr = QString("%1s").arg(rep.elapsedMs / 1000.0, 0, 'f', 1);

            QString vals[5] = {
                QString("T%1→T0").arg(rep.timepoint),
                deltaStr, statusStr, miStr, timeStr
            };

            QColor bg = ok ? QColor(76,175,80,40) : QColor(244,67,54,40);
            for (int c = 0; c < 5; ++c) {
                auto* it = new QTableWidgetItem(vals[c]);
                it->setTextAlignment(Qt::AlignCenter);
                it->setBackground(bg);
                if (c == 0) it->setFont(QFont("", -1, QFont::Bold));
                table->setItem(r, c, it);
            }
        }

        const int rowH = 40;
        const int tableH = 38 + nRows * rowH + 4;
        table->setMinimumHeight(tableH);
        table->setMaximumHeight(tableH);
        table->verticalHeader()->setDefaultSectionSize(rowH);
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        cardLayout->addWidget(table);


        int okCount = 0;
        for (const auto& rep : m_registrationReports)
            if (rep.converged && rep.deltaLandmark >= 0.0
                && rep.deltaLandmark < m_spinDeltaThr->value()) ++okCount;
        auto* status = new QLabel(QString("%1/%2 registrations within threshold")
                                  .arg(okCount).arg(nRows));
        status->setAlignment(Qt::AlignCenter);
        status->setStyleSheet("color:#2E7D32; font-size:12px; font-weight:bold; border:none;");
        cardLayout->addWidget(status);
    }

    auto* centerRow = new QHBoxLayout;
    centerRow->addStretch(1);
    centerRow->addWidget(card, 0, Qt::AlignTop);
    centerRow->addStretch(1);
    m_rightContentLayout->addLayout(centerRow);
    m_rightContentLayout->addStretch();
}

void MainWindow::onRunRegistration()
{
    if (m_volumes.size() < 2) {
        QMessageBox::warning(this, "Registration",
            "Load at least 2 CBCT volumes (Step 1) before registration.");
        return;
    }
    if (m_allTimepointLandmarks.empty() || m_allTimepointLandmarks[0].empty()) {
        QMessageBox::warning(this, "Registration",
            "Detect landmarks first (Step 2): registration uses ANS_T0 as the anchor.");
        return;
    }

    statusBar()->showMessage("Running rigid registration...");
    QApplication::processEvents();


    std::vector<std::shared_ptr<CBCTVolume>> work;
    for (size_t i = 0; i < m_volumes.size(); ++i) {
        auto c = std::make_shared<CBCTVolume>();
        c->setITKImage(m_volumes[i]->getITKImage());
        c->setName(m_volumes[i]->getName());
        c->resampleIsotropic(0.3);
        work.push_back(c);
    }

    m_pipeline = std::make_unique<PipelineManager>(this);
    m_pipeline->setLoadedVolumes(work);
    m_pipeline->setManualLandmarksMulti(m_allTimepointLandmarks);

    const double thr = m_spinDeltaThr ? m_spinDeltaThr->value() : 2.0;
    bool ok = m_pipeline->runRegistrationOnly("ANS", thr);


    m_registrationReports = m_pipeline->getRegistrationReports();

    displayRegistrationStep(); 

    if (ok)
        statusBar()->showMessage(
            QString("Registration done: %1 timepoint(s)").arg(m_registrationReports.size()), 8000);
    else
        statusBar()->showMessage("Registration finished with errors (see logs)", 8000);


}


void MainWindow::displayExtractionStep()
{
    QLayoutItem* child;
    while ((child = m_rightContentLayout->takeAt(0)) != nullptr) {
        if (child->layout()) {
            QLayoutItem* sub;
            while ((sub = child->layout()->takeAt(0)) != nullptr) {
                if (sub->widget()) delete sub->widget();
                delete sub;
            }
        }
        if (child->widget()) delete child->widget();
        delete child;
    }

    m_lblTitle->setText("Step 4 — Slice Extraction");

    auto* card = new QWidget;
    card->setObjectName("extCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(
        "QWidget#extCard { background:#ffffff; border:1px solid #e0e0e0; border-radius:10px; }");
    card->setMinimumWidth(720);
    card->setMaximumWidth(900);
    card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(12);

    auto* lblInfo = new QLabel("Extract 2D slices around the fixed anchor (ANS T0), "
                               "in 3 orientations, then normalize the FOV.");
    lblInfo->setWordWrap(true);
    lblInfo->setStyleSheet("font-size:12px; color:#666; border:none;");
    cardLayout->addWidget(lblInfo);


    auto* rowN = new QHBoxLayout;
    auto* lblN = new QLabel("Slices per orientation:");
    lblN->setStyleSheet("font-size:13px; color:#666; border:none;");
    m_spinNumSlices = new QSpinBox;
    m_spinNumSlices->setRange(1, 500);
    m_spinNumSlices->setValue(300);
    m_spinNumSlices->setStyleSheet("QSpinBox { font-size:14px; padding:4px; }");
    rowN->addWidget(lblN);
    rowN->addWidget(m_spinNumSlices);
    rowN->addStretch();
    cardLayout->addLayout(rowN);


    auto* rowR = new QHBoxLayout;
    auto* lblR = new QLabel("Range around anchor (± mm):");
    lblR->setStyleSheet("font-size:13px; color:#666; border:none;");
    m_spinRangeMm = new QDoubleSpinBox;
    m_spinRangeMm->setRange(1.0, 100.0);
    m_spinRangeMm->setSingleStep(1.0);
    m_spinRangeMm->setValue(25.0);
    m_spinRangeMm->setStyleSheet("QDoubleSpinBox { font-size:14px; padding:4px; }");
    rowR->addWidget(lblR);
    rowR->addWidget(m_spinRangeMm);
    rowR->addStretch();
    cardLayout->addLayout(rowR);


    auto* btnRun = new QPushButton("Run Extraction");
    btnRun->setStyleSheet(
        "QPushButton { background-color:#4CAF50; color:white; font-weight:bold; "
        "padding:9px; border-radius:6px; font-size:13px; }"
        "QPushButton:hover { background-color:#45a049; }"
        "QPushButton:disabled { background-color:#ccc; color:#666; }");
    connect(btnRun, &QPushButton::clicked, this, [this]() {
        QTimer::singleShot(0, this, [this]() { onRunExtraction(); });
    });
    cardLayout->addWidget(btnRun);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:#e0e0e0; border:none;");
    cardLayout->addWidget(sep);


    if (!m_extractionReport.ok) {
        auto* hint = new QLabel("No extraction yet.\nRun Registration (Step 3) first, "
                                "then click \"Run Extraction\".");
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color:#999; font-size:12px; padding:18px; border:none;");
        cardLayout->addWidget(hint);
    } else {
        auto* res = new QLabel(QString(
            "✓ Extraction done\n\n"
            "Timepoints: %1\n"
            "Slices per orientation: %2\n"
            "Total slices: %3  (= %1 × 3 orientations × %2)\n"
            "Anchor: %4\n\n"
            "Saved to: %5")
            .arg(m_extractionReport.timepoints)
            .arg(m_extractionReport.slicesPerOrientation)
            .arg(m_extractionReport.totalSlices)
            .arg(m_extractionReport.refLandmark)
            .arg(m_extractionReport.outputDir));
        res->setWordWrap(true);
        res->setStyleSheet("color:#2E7D32; font-size:13px; border:none; padding:8px;");
        cardLayout->addWidget(res);

        auto* hintView = new QLabel("→ Go to Step 5 (Validation) to view slices and MCAGPC metrics.");
        hintView->setStyleSheet("color:#45a049; font-size:12px; border:none; padding:4px;");
        cardLayout->addWidget(hintView);
    }

    auto* centerRow = new QHBoxLayout;
    centerRow->addStretch(1);
    centerRow->addWidget(card, 0, Qt::AlignTop);
    centerRow->addStretch(1);
    m_rightContentLayout->addLayout(centerRow);
    m_rightContentLayout->addStretch();
}

void MainWindow::onRunExtraction()
{
    if (!m_pipeline) {
        QMessageBox::warning(this, "Extraction",
            "Run Registration (Step 3) first.\n"
            "Extraction reuses the aligned volumes from registration.");
        return;
    }


    m_extractionOutputDir = QFileDialog::getExistingDirectory(
        this, "Select Output Folder for slices",
        lastDirOrDefault("lastOutputDir", DEFAULT_OUTPUT_DIR));
    if (m_extractionOutputDir.isEmpty()) return;
    rememberDir("lastOutputDir", m_extractionOutputDir);

    const int nSlices = m_spinNumSlices ? m_spinNumSlices->value() : 50;
    const double rangeMm = m_spinRangeMm ? m_spinRangeMm->value() : 25.0;

    statusBar()->showMessage("Running slice extraction...");
    QApplication::processEvents();

    m_extractionReport = m_pipeline->runExtractionOnly(
        m_extractionOutputDir, "ANS", nSlices, rangeMm);

    displayExtractionStep(); 

    if (m_extractionReport.ok) {
        statusBar()->showMessage(
            QString("Extraction done: %1 slices saved").arg(m_extractionReport.totalSlices), 8000);

        m_resultsDir = m_extractionOutputDir;
        scanResultsDirectory();            
        m_grpSliceControls->setEnabled(true); 
    } else {
        statusBar()->showMessage("Extraction failed (see logs)", 8000);
    }
}

static QImage applyWindowLevel(const QImage& src, int level, int width)
{
    if (src.isNull()) return src;
    QImage gray = src.convertToFormat(QImage::Format_Grayscale8);


    double scale = 255.0 / 2000.0;
    double centre = level * scale;         
    double demi   = (width * scale) / 2.0;  
    if (demi < 1.0) demi = 1.0;
    double lo = centre - demi;
    double hi = centre + demi;


    uchar lut[256];
    for (int v = 0; v < 256; ++v) {
        double out;
        if (v <= lo)      out = 0.0;
        else if (v >= hi) out = 255.0;
        else              out = (v - lo) / (hi - lo) * 255.0;
        lut[v] = static_cast<uchar>(qBound(0.0, out, 255.0));
    }

    const int W = gray.width(), H = gray.height();
    for (int y = 0; y < H; ++y) {
        uchar* row = gray.scanLine(y);
        for (int x = 0; x < W; ++x)
            row[x] = lut[row[x]];
    }
    return gray;
}

void MainWindow::displayCurrentSlices()
{

    if (m_resultsDir.isEmpty()) {
        QString candidate = lastDirOrDefault("lastResultsDir", DEFAULT_OUTPUT_DIR);

        QDir d(candidate);
        bool hasTimepoints = false;
        for (const auto& e : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (e.startsWith("T")) {
                QDir tp(d.filePath(e));
                if (tp.exists("Axial") || tp.exists("Coronal") || tp.exists("Sagittal")) {
                    hasTimepoints = true;
                    break;
                }
            }
        }
        if (hasTimepoints) {
            m_resultsDir = candidate;
        }
    }


    if (!m_resultsDir.isEmpty() && m_timepointDirs.isEmpty()) {
        scanResultsDirectory();
    }


    QLayoutItem* child;
    while ((child = m_rightContentLayout->takeAt(0)) != nullptr) {
        if (child->layout()) {
            QLayoutItem* sub;
            while ((sub = child->layout()->takeAt(0)) != nullptr) {
                if (sub->widget()) delete sub->widget();
                delete sub;
            }
        }
        if (child->widget()) delete child->widget();
        delete child;
    }

    m_lblTitle->setText("Step 5 — Validation & Visualization");


    if (m_resultsDir.isEmpty() || m_timepointDirs.isEmpty()) {
        auto* hint = new QLabel(
            "No slices to display yet.\n\n"
            "Run Extraction (Step 4) first, or use \"Load Results...\".");
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color:#999; font-size:13px; padding:40px;");
        m_rightContentLayout->addWidget(hint);
        m_rightContentLayout->addStretch();
        return;
    }


    auto* ctrlCard = new QWidget;
    ctrlCard->setObjectName("ctrlCard");
    ctrlCard->setAttribute(Qt::WA_StyledBackground, true);
    ctrlCard->setStyleSheet(
        "QWidget#ctrlCard { background:#ffffff; border:1px solid #e0e0e0; border-radius:10px; }");
    ctrlCard->setMaximumWidth(900);
    auto* ctrlLayout = new QVBoxLayout(ctrlCard);
    ctrlLayout->setContentsMargins(18, 14, 18, 14);
    ctrlLayout->setSpacing(10);


    static int sIdx[3] = {-1, -1, -1};
    for (int k = 0; k < 3; ++k)
        if (sIdx[k] < 0 || sIdx[k] >= m_numSlices) sIdx[k] = m_numSlices / 2;


    static QTimer* sliceTimer = nullptr;
    if (!sliceTimer) { sliceTimer = new QTimer(this); sliceTimer->setSingleShot(true); }
    disconnect(sliceTimer, nullptr, this, nullptr);
    connect(sliceTimer, &QTimer::timeout, this, [this]() { displayCurrentSlices(); });


    auto* rowWL = new QHBoxLayout;
    auto* lblWin = new QLabel("Window:");
    lblWin->setStyleSheet("font-size:13px; color:#666; border:none;");
    lblWin->setFixedWidth(60);

    auto* lblLvl = new QLabel("Level");
    lblLvl->setStyleSheet("font-size:12px; color:#888; border:none;");
    auto* sliderLvl = new QSlider(Qt::Horizontal);
    sliderLvl->setRange(0, 2000);
    sliderLvl->setValue(m_wlLevel);
    sliderLvl->setFixedWidth(140);
    auto* lblLvlVal = new QLabel(QString::number(m_wlLevel));
    lblLvlVal->setStyleSheet("font-size:12px; color:#333; border:none;");
    lblLvlVal->setFixedWidth(40);

    auto* lblWid = new QLabel("Width");
    lblWid->setStyleSheet("font-size:12px; color:#888; border:none;");
    auto* sliderWid = new QSlider(Qt::Horizontal);
    sliderWid->setRange(100, 4000);
    sliderWid->setValue(m_wlWidth);
    sliderWid->setFixedWidth(140);
    auto* lblWidVal = new QLabel(QString::number(m_wlWidth));
    lblWidVal->setStyleSheet("font-size:12px; color:#333; border:none;");
    lblWidVal->setFixedWidth(45);

    auto* btnReset = new QPushButton("Reset");
    btnReset->setStyleSheet("QPushButton { font-size:11px; padding:3px 8px; }");

    rowWL->addWidget(lblWin);
    rowWL->addWidget(lblLvl);
    rowWL->addWidget(sliderLvl);
    rowWL->addWidget(lblLvlVal);
    rowWL->addSpacing(12);
    rowWL->addWidget(lblWid);
    rowWL->addWidget(sliderWid);
    rowWL->addWidget(lblWidVal);
    rowWL->addStretch();
    rowWL->addWidget(btnReset);
    ctrlLayout->addLayout(rowWL);


    static QTimer* wlTimer = nullptr;
    if (!wlTimer) { wlTimer = new QTimer(this); wlTimer->setSingleShot(true); }

    auto applyWL = [this, lblLvlVal, lblWidVal, sliderLvl, sliderWid]() {
        m_wlLevel = sliderLvl->value();
        m_wlWidth = sliderWid->value();
        lblLvlVal->setText(QString::number(m_wlLevel));
        lblWidVal->setText(QString::number(m_wlWidth));
    };
    connect(sliderLvl, &QSlider::valueChanged, this, [this, applyWL]() {
        applyWL();
        wlTimer->stop();
        wlTimer->start(150);   
    });
    connect(sliderWid, &QSlider::valueChanged, this, [this, applyWL]() {
        applyWL();
        wlTimer->stop();
        wlTimer->start(150);
    });

    disconnect(wlTimer, nullptr, this, nullptr);
    connect(wlTimer, &QTimer::timeout, this, [this]() { displayCurrentSlices(); });

    connect(btnReset, &QPushButton::clicked, this, [this]() {
        m_wlLevel = 300; m_wlWidth = 2400;
        displayCurrentSlices();
    });


    auto* ctrlRow = new QHBoxLayout;
    ctrlRow->addStretch(1);
    ctrlRow->addWidget(ctrlCard, 0, Qt::AlignTop);
    ctrlRow->addStretch(1);
    m_rightContentLayout->addLayout(ctrlRow);


    QStringList orients = {"Axial", "Coronal", "Sagittal"};
    int numTP = m_timepointDirs.size();


    const int MIN_IMG = 200;  
    const int MAX_IMG = 340;  
    const int labelW  = 90;    
    const int spacing = 8;

    int vpW = m_scrollArea->viewport()->width() - 20;

    int fitSize = (vpW - labelW - (numTP - 1) * spacing) / qMax(1, numTP);
    int imgSize = qBound(MIN_IMG, fitSize, MAX_IMG);


    int gridW = labelW + numTP * imgSize + (numTP - 1) * spacing;

    bool needsHScroll = gridW > vpW;

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(spacing);
    if (!needsHScroll) headerRow->addStretch(); 
    headerRow->addSpacing(labelW);               
    for (int t = 0; t < numTP; ++t) {
        auto* lbl = new QLabel(m_timepointDirs[t]);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedWidth(imgSize);
        lbl->setStyleSheet("font-size:15px; font-weight:bold; color:#333;");
        headerRow->addWidget(lbl);
    }
    headerRow->addStretch();
    m_rightContentLayout->addLayout(headerRow);

    for (int o = 0; o < 3; ++o) {   
        auto* imgRow = new QHBoxLayout;
        imgRow->setSpacing(spacing);
        if (!needsHScroll) imgRow->addStretch(); 


        auto* leftCol = new QWidget;
        leftCol->setFixedWidth(labelW);
        auto* leftColLay = new QVBoxLayout(leftCol);
        leftColLay->setContentsMargins(0, 0, 0, 0);
        leftColLay->setSpacing(4);
        auto* orientLabel = new QLabel(orients[o]);
        orientLabel->setAlignment(Qt::AlignCenter);
        orientLabel->setStyleSheet("font-size:12px; font-weight:bold; color:#555;");
        auto* sliceSpin = new QSpinBox;
        sliceSpin->setRange(0, m_numSlices - 1);
        sliceSpin->setValue(sIdx[o]);
        sliceSpin->setToolTip("Numero de coupe a afficher");
        sliceSpin->setStyleSheet("QSpinBox { font-size:12px; padding:2px; }");
        connect(sliceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, o](int v) {
            sIdx[o] = v;
            sliceTimer->stop();
            sliceTimer->start(250);
        });
        leftColLay->addStretch();
        leftColLay->addWidget(orientLabel);
        leftColLay->addWidget(sliceSpin);
        leftColLay->addStretch();
        imgRow->addWidget(leftCol);


        QString sliceFile = QString("slice_%1.png").arg(sIdx[o], 3, 10, QChar('0'));

        for (int t = 0; t < numTP; ++t) {
            QString path = QDir(m_resultsDir).filePath(
                QString("%1/%2/%3").arg(m_timepointDirs[t], orients[o], sliceFile));


            auto* container = new QWidget;
            container->setFixedSize(imgSize, imgSize);

            auto* imgLabel = new QLabel(container);
            imgLabel->setGeometry(0, 0, imgSize, imgSize);
            imgLabel->setAlignment(Qt::AlignCenter);

            QImage img(path);
            if (!img.isNull()) {
                img = applyWindowLevel(img, m_wlLevel, m_wlWidth);
                QPixmap px = QPixmap::fromImage(img).scaled(
                    imgSize - 2, imgSize - 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imgLabel->setPixmap(px);
                imgLabel->setStyleSheet("border:1px solid #888; background:black;");
            } else {
                imgLabel->setText("—");
                imgLabel->setStyleSheet("border:1px dashed #bbb; color:#aaa; background:#f0f0f0;");
            }


            auto* btnZoom = new QToolButton(container);
            btnZoom->setText("⛶");
            btnZoom->setToolTip("Agrandir");
            btnZoom->setCursor(Qt::PointingHandCursor);
            btnZoom->setStyleSheet(
                "QToolButton { background:rgba(0,0,0,0.55); color:white; border:none; "
                "border-radius:4px; font-size:16px; padding:2px 6px; }"
                "QToolButton:hover { background:rgba(76,175,80,0.9); }");
            btnZoom->setGeometry(imgSize - 34, 4, 30, 28);

            QString tp = m_timepointDirs[t];
            QString orient = orients[o];
            connect(btnZoom, &QToolButton::clicked, this, [this, tp, orient, o]() {
                m_currentSliceNum = sIdx[o];
                showZoomedSlice(tp, orient);
            });

            imgRow->addWidget(container);
        }
        imgRow->addStretch();
        m_rightContentLayout->addLayout(imgRow);
    }

    m_rightContentLayout->addStretch();
}

void MainWindow::showZoomedSlice(const QString& timepoint, const QString& orientation)
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString("%1 — %2").arg(timepoint, orientation));
    dlg->resize(720, 800);
    dlg->setStyleSheet("QDialog { background:#1e1e1e; }");

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);


    auto* lblTitle = new QLabel;
    lblTitle->setAlignment(Qt::AlignCenter);
    lblTitle->setStyleSheet("color:white; font-size:14px; font-weight:bold;");
    layout->addWidget(lblTitle);

    
    auto* lblImg = new QLabel;
    lblImg->setAlignment(Qt::AlignCenter);
    lblImg->setStyleSheet("background:black; border:1px solid #444;");
    lblImg->setMinimumSize(640, 640);
    layout->addWidget(lblImg, 1);

    
    auto* navRow = new QHBoxLayout;
    auto* btnPrev = new QPushButton("◄ Prev");
    auto* btnNext = new QPushButton("Next ►");
    auto* lblNav = new QLabel;
    lblNav->setAlignment(Qt::AlignCenter);
    lblNav->setStyleSheet("color:white; font-size:13px;");
    btnPrev->setStyleSheet("QPushButton { padding:6px 14px; }");
    btnNext->setStyleSheet("QPushButton { padding:6px 14px; }");
    navRow->addWidget(btnPrev);
    navRow->addStretch();
    navRow->addWidget(lblNav);
    navRow->addStretch();
    navRow->addWidget(btnNext);
    layout->addLayout(navRow);


    auto sliceIdx = std::make_shared<int>(m_currentSliceNum);

    auto refresh = [this, lblImg, lblTitle, lblNav, timepoint, orientation, sliceIdx]() {
        QString f = QString("slice_%1.png").arg(*sliceIdx, 3, 10, QChar('0'));
        QString path = QDir(m_resultsDir).filePath(
            QString("%1/%2/%3").arg(timepoint, orientation, f));
        QImage img(path);
        if (!img.isNull()) {
            img = applyWindowLevel(img, m_wlLevel, m_wlWidth);
            QPixmap px = QPixmap::fromImage(img).scaled(
                640, 640, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            lblImg->setPixmap(px);
        } else {
            lblImg->setText("— (no image) —");
            lblImg->setStyleSheet("background:black; color:#888; border:1px solid #444;");
        }
        lblTitle->setText(QString("%1 — %2 — Slice %3").arg(timepoint, orientation).arg(*sliceIdx));
        lblNav->setText(QString("%1 / %2").arg(*sliceIdx + 1).arg(m_numSlices));
    };

    connect(btnPrev, &QPushButton::clicked, dlg, [this, sliceIdx, refresh]() {
        if (*sliceIdx > 0) { (*sliceIdx)--; refresh(); }
    });
    connect(btnNext, &QPushButton::clicked, dlg, [this, sliceIdx, refresh]() {
        if (*sliceIdx < m_numSlices - 1) { (*sliceIdx)++; refresh(); }
    });

    refresh();
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}


void MainWindow::onWindowLevelChanged()
{
    displayCurrentSlices();
}
} // namespace CBCTAlign
