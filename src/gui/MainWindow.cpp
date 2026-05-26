/**
 * @file MainWindow.cpp
 * @brief CBCTAlign — Multi-orientation visualization
 *
 * Right panel layout (NO grid, using VBox + HBox):
 *
 *   [        T0         T1       ]   ← header row
 *   --- Axial ---                    ← orientation label
 *   [   [img]       [img]       ]   ← images side by side
 *   --- Coronal ---
 *   [   [img]       [img]       ]
 *   --- Sagittal ---
 *   [   [img]       [img]       ]
 */
#include "MainWindow.h"
#include "../core/LandmarkIO.h"
#include "../utils/Logger.h"

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
#include <QPixmap>
#include <QImage>
#include <QSettings>
#include <QStandardPaths>
#include <QListView>
#include <QTreeView>

// Default paths for CBCTAlign file dialogs (LAROSERI data convention)
// Adjust BASE_DATA_DIR if data lives elsewhere on another machine.
namespace {
    const QString BASE_DATA_DIR        = QDir::homePath() + "/Desktop/PHD/artcile2026/artcileSoftwarX/dataCBCT";
    const QString DEFAULT_DICOM_DIR    = BASE_DATA_DIR + "/MOULID_HICHAM/CBCT";
    const QString DEFAULT_NIFTI_DIR    = BASE_DATA_DIR + "/MOULID_HICHAM/nifti";
    const QString DEFAULT_LANDMARK_DIR = BASE_DATA_DIR + "/MOULID_HICHAM/nifti";   // .mrk.json live here
    const QString DEFAULT_CSV_DIR      = BASE_DATA_DIR + "/MOULID_HICHAM";
    const QString DEFAULT_OUTPUT_DIR   = BASE_DATA_DIR + "/results";

    // Returns the last directory used (via QSettings) or the default at first launch.
    // Falls back to default if the saved path no longer exists.
    inline QString lastDirOrDefault(const QString& key, const QString& defaultDir) {
        QSettings settings("LAROSERI", "CBCTAlign");
        QString d = settings.value(key, defaultDir).toString();
        return QDir(d).exists() ? d : defaultDir;
    }

    // Stores the parent directory of the given path for next time.
    // Accepts either a file path (uses parent dir) or a directory path (uses it directly).
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

    // Ensure default output directory exists (created on first run)
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

    auto* leftPanel = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(4);
    leftLayout->setContentsMargins(4, 4, 4, 4);

    // --- CBCT Volumes ---
    auto* grpVol = new QGroupBox("CBCT Volumes");
    auto* volLayout = new QVBoxLayout(grpVol);
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

    m_btnRunPipeline = new QPushButton("▶ Run Pipeline");
    m_btnRunPipeline->setStyleSheet(GREEN_BTN);
    m_btnRunPipeline->setEnabled(false);
    volLayout->addWidget(m_btnRunPipeline);

    m_btnStop = new QPushButton("Stop");
    m_btnStop->setEnabled(false);
    volLayout->addWidget(m_btnStop);
    leftLayout->addWidget(grpVol);

    // --- Cephalometric Landmarks ---
    m_grpLandmarks = new QGroupBox("Cephalometric Landmarks");
    auto* lmLayout = new QVBoxLayout(m_grpLandmarks);
    lmLayout->setSpacing(3);

    auto* lmBtnRow = new QHBoxLayout;
    m_btnLoadJSON = new QPushButton("Load JSON");
    m_btnLoadCSV = new QPushButton("Load CSV");
    m_btnLoadJSON->setStyleSheet(GREEN_BTN_SM);
    m_btnLoadCSV->setStyleSheet(GREEN_BTN_SM);
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

    m_lblLandmarkStatus = new QLabel("No landmarks loaded");
    m_lblLandmarkStatus->setAlignment(Qt::AlignCenter);
    m_lblLandmarkStatus->setStyleSheet("color:#999; font-size:11px; font-style:italic;");
    lmLayout->addWidget(m_lblLandmarkStatus);
    leftLayout->addWidget(m_grpLandmarks);

    // --- Slice Navigation ---
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
        "font-weight:bold; color:#2196F3; font-size:12px; "
        "padding:3px; background:#f0f8ff; border-radius:3px;");
    sliceLayout->addWidget(m_lblSliceInfo);

    m_btnLoadSlices = new QPushButton("Load Results...");
    m_btnLoadSlices->setStyleSheet(GREEN_BTN_SM);
    sliceLayout->addWidget(m_btnLoadSlices);
    leftLayout->addWidget(m_grpSliceControls);

    // --- MCAGPC Metrics: COMMENTED OUT (hidden) ---
    // m_grpMetrics = new QGroupBox("MCAGPC Metrics");
    // ... table creation commented out ...
    // leftLayout->addWidget(m_grpMetrics);

    leftLayout->addStretch();
    splitter->addWidget(leftPanel);

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
    m_scrollArea->setStyleSheet("QScrollArea { border:none; background:#f8f8f8; }");

    m_scrollContent = new QWidget;
    m_rightContentLayout = new QVBoxLayout(m_scrollContent);
    m_rightContentLayout->setSpacing(0);
    m_rightContentLayout->setContentsMargins(4, 0, 4, 4);

    m_scrollArea->setWidget(m_scrollContent);
    rightLayout->addWidget(m_scrollArea);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 5);

    mainLayout->addWidget(splitter);
    statusBar()->showMessage("Ready — Load 2+ CBCT volumes to start");
}

void MainWindow::connectSignals()
{
    connect(m_btnLoadDICOM,  &QPushButton::clicked, this, &MainWindow::onLoadDICOM);
    connect(m_btnLoadNIfTI,  &QPushButton::clicked, this, &MainWindow::onLoadNIfTI);
    connect(m_btnRunPipeline,&QPushButton::clicked, this, &MainWindow::onRunPipeline);
    connect(m_btnStop, &QPushButton::clicked, [this]() { if (m_pipeline) m_pipeline->cancel(); });
    connect(m_btnLoadSlices, &QPushButton::clicked, this, &MainWindow::onLoadSlicesFromDisk);
    connect(m_btnLoadJSON,   &QPushButton::clicked, this, &MainWindow::onLoadLandmarksJSON);
    connect(m_btnLoadCSV,    &QPushButton::clicked, this, &MainWindow::onLoadLandmarksCSV);
    connect(m_sliderSliceNum, &QSlider::valueChanged, this, &MainWindow::onSliceNumChanged);
    connect(m_spinSliceNum, QOverload<int>::of(&QSpinBox::valueChanged),
            m_sliderSliceNum, &QSlider::setValue);
}

void MainWindow::createMenuBar()
{
    auto* mFile = menuBar()->addMenu("&File");
    mFile->addAction("Open DICOM...", this, &MainWindow::onLoadDICOM);
    mFile->addAction("Open NIfTI...", this, &MainWindow::onLoadNIfTI);
    mFile->addSeparator();
    mFile->addAction("Load Landmarks JSON...", this, &MainWindow::onLoadLandmarksJSON);
    mFile->addAction("Load Landmarks CSV...", this, &MainWindow::onLoadLandmarksCSV);
    mFile->addSeparator();
    mFile->addAction("Quit", this, &QMainWindow::close);

    auto* mHelp = menuBar()->addMenu("&Help");
    mHelp->addAction("About", [this]() {
        QMessageBox::about(this, "CBCTAlign",
            "CBCTAlign v2.0\n\n"
            "Spatio-Temporal CBCT Alignment\n"
            "Guided by Cephalometric Landmarks\n\n"
            "MCAGPC Metric — SoftwareX 2026");
    });
}

// Volume Loading

void MainWindow::onLoadDICOM()
{
    // Selection multi-dossiers DICOM (Ctrl+clic / Shift+clic)
    // QFileDialog::getExistingDirectory ne supporte pas la selection multiple,
    // donc on cree un dialog manuel avec ListView.selectionMode = ExtendedSelection
    QFileDialog dialog(this, "Select DICOM Folders (1 per timepoint, Ctrl/Shift+click for multi)");
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);  // necessaire pour selection multiple
    dialog.setDirectory(lastDirOrDefault("lastDicomDir", DEFAULT_DICOM_DIR));

    // Activer la selection multiple sur les vues internes
    QListView* listView = dialog.findChild<QListView*>("listView");
    if (listView) listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QTreeView* treeView = dialog.findChild<QTreeView*>();
    if (treeView) treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    if (dialog.exec() != QDialog::Accepted) return;

    QStringList dirs = dialog.selectedFiles();
    if (dirs.isEmpty()) return;

    rememberDir("lastDicomDir", dirs.first());

    // Tri alphabetique pour ordre temporel T0, T1, T2...
    dirs.sort();

    int loaded = 0;
    int failed = 0;

    for (const QString& dir : dirs) {
        // QFileDialog en mode Directory + selection multiple peut renvoyer
        // le parent au lieu du dossier selectionne dans certains cas.
        // On verifie que c'est bien un dossier valide.
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

    m_btnRunPipeline->setEnabled(m_volumes.size() >= 2);

    if (failed > 0) {
        QMessageBox::warning(this, "Partial Load",
            QString("Loaded %1 DICOM folder(s), failed to load %2 folder(s).\nCheck logs for details.")
                .arg(loaded).arg(failed));
    } else if (loaded > 0) {
        statusBar()->showMessage(
            QString("%1 DICOM volume(s) loaded").arg(loaded), 5000);
    }
}
void MainWindow::onLoadNIfTI()
{
    // Selection multiple : Ctrl+clic ou Shift+clic pour selectionner plusieurs fichiers
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "Select NIfTI files (select 1 file per timepoint, in T0..Tn order)",
        lastDirOrDefault("lastNiftiDir", DEFAULT_NIFTI_DIR),
        "NIfTI (*.nii *.nii.gz)");

    if (paths.isEmpty()) return;
    rememberDir("lastNiftiDir", paths.first());

    // Tri alphabetique : MOULID_T0, MOULID_T1, MOULID_T2 -> ordre temporel
    paths.sort();

    int loaded = 0;
    int failed = 0;

    for (const QString& path : paths) {
        auto vol = std::make_shared<CBCTVolume>();
        if (vol->loadFromNIfTI(path)) {
            m_volumes.push_back(vol);
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

    m_btnRunPipeline->setEnabled(m_volumes.size() >= 2);

    if (failed > 0) {
        QMessageBox::warning(this, "Partial Load",
            QString("Loaded %1 file(s), failed to load %2 file(s).\nCheck logs for details.")
                .arg(loaded).arg(failed));
    } else {
        statusBar()->showMessage(
            QString("%1 NIfTI volume(s) loaded").arg(loaded), 5000);
    }
}

// Landmarks

void MainWindow::onLoadLandmarksJSON()
{
    // V4: Charge PLUSIEURS .mrk.json (un par timepoint, ordre alphabetique = T0..Tn)
    QStringList fps = QFileDialog::getOpenFileNames(
        this,
        "Load Landmarks JSON (select 1 file per timepoint, in T0..Tn order)",
        lastDirOrDefault("lastLandmarkDir", DEFAULT_LANDMARK_DIR),
        "Slicer Markups (*.mrk.json *.json);;All (*)");

    if (fps.isEmpty()) return;
    rememberDir("lastLandmarkDir", fps.first());
    fps.sort();  // T0, T1, T2, T3 dans l'ordre alphabetique

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

    // Pour la GUI : afficher T0
    // Pour la GUI : afficher TOUS les timepoints dans le tableau
    if (!m_allTimepointLandmarks.empty()) {
        m_manualLandmarks = m_allTimepointLandmarks[0];  // T0 reste la reference
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

void MainWindow::updateLandmarkTable()
{
    m_tableLandmarks->setRowCount(0);
    if (m_allTimepointLandmarks.empty()) {
        m_lblLandmarkStatus->setText("No landmarks loaded");
        m_lblLandmarkStatus->setStyleSheet("color:#999; font-size:11px; font-style:italic;");
        return;
    }

    // En-tetes avec colonne Timepoint
    m_tableLandmarks->setColumnCount(6);
    m_tableLandmarks->setHorizontalHeaderLabels({"TP", "Name", "Abbr.", "X (mm)", "Y (mm)", "Z (mm)"});
    m_tableLandmarks->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Compter le total de lignes (tous landmarks de tous timepoints)
    int totalRows = 0;
    for (const auto& tpLms : m_allTimepointLandmarks)
        totalRows += tpLms.size();
    m_tableLandmarks->setRowCount(totalRows);

    int row = 0;
    QColor tpColors[] = {QColor(33,150,243,40),    // T0 bleu
                         QColor(76,175,80,40),     // T1 vert
                         QColor(255,152,0,40)};    // T2 orange

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
}

// Pipeline

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
        // V5: Si on a charge plusieurs JSON (multi-timepoint), passer tout
        if (m_allTimepointLandmarks.size() >= 2)
            m_pipeline->setManualLandmarksMulti(m_allTimepointLandmarks);
        else
            m_pipeline->setManualLandmarks(m_manualLandmarks);
    }

    m_btnRunPipeline->setEnabled(false);
    m_btnStop->setEnabled(true);
    statusBar()->showMessage("Pipeline running...");

    QString ref="Sella";
    for (const auto& lm:m_manualLandmarks) {
        if (lm.name=="Sella"||lm.abbreviation=="S") { ref="Sella"; break; }
        if (lm.name=="Nasion"||lm.abbreviation=="N") ref="Nasion";
    }
    m_pipeline->runFullPipeline(QStringList(), m_currentOutputDir, ref, 0.0, 50, 25.0);
}

void MainWindow::onPipelineProgress(int pct, const QString& msg)
{
    statusBar()->showMessage(QString("%1 (%2%)").arg(msg).arg(pct));
}

void MainWindow::onPipelineFinished(bool ok)
{
    m_btnRunPipeline->setEnabled(true);
    m_btnStop->setEnabled(false);
    if (ok) {
        statusBar()->showMessage("Pipeline completed!", 5000);
        m_resultsDir = m_currentOutputDir;
        scanResultsDirectory();
        m_grpSliceControls->setEnabled(true);
        m_pipelineCompleted = true;
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
    m_sliderSliceNum->setValue(m_numSlices/2);
    statusBar()->showMessage(QString("%1 timepoints: %2").arg(m_timepointDirs.size()).arg(m_timepointDirs.join(", ")), 5000);
    displayCurrentSlices();
}

// Display: VBox approach — no grid, no spacing issues
//
//   Header:  [  T0  ] [  T1  ]
//   "Axial"
//   [img T0] [img T1]
//   "Coronal"
//   [img T0] [img T1]
//   "Sagittal"
//   [img T0] [img T1]

void MainWindow::displayCurrentSlices()
{
    if (m_resultsDir.isEmpty() || m_timepointDirs.isEmpty()) return;

    // Clear previous content
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

    QString sliceFile = QString("slice_%1.png").arg(m_currentSliceNum, 3, 10, QChar('0'));
    QStringList orients = {"Axial", "Coronal", "Sagittal"};
    int numTP = m_timepointDirs.size();

    // Compute image size
    int vpW = m_scrollArea->viewport()->width() - 20;
    int vpH = m_scrollArea->viewport()->height() - 60;
    int imgSize = qMin(qMax(150, vpW / qMax(1, numTP) - 8),
                       qMax(150, vpH / 3 - 30));

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(4);
    for (int t = 0; t < numTP; ++t) {
        auto* lbl = new QLabel(m_timepointDirs[t]);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedHeight(20);
        lbl->setStyleSheet("font-size:14px; font-weight:bold; color:#333;");
        headerRow->addWidget(lbl);
    }
    m_rightContentLayout->addLayout(headerRow);

    for (int o = 0; o < 3; ++o) {
        // Orientation label — small, left-aligned, tight
        auto* orientLabel = new QLabel(orients[o]);
        orientLabel->setStyleSheet(
            "font-size:11px; font-weight:bold; color:#555; padding:1px 4px; margin:0px;");
        orientLabel->setFixedHeight(16);
        m_rightContentLayout->addWidget(orientLabel);

        // Image row
        auto* imgRow = new QHBoxLayout;
        imgRow->setSpacing(4);
        imgRow->setContentsMargins(0, 0, 0, 0);

        for (int t = 0; t < numTP; ++t) {
            QString path = QDir(m_resultsDir).filePath(
                QString("%1/%2/%3").arg(m_timepointDirs[t], orients[o], sliceFile));

            auto* imgLabel = new QLabel;
            imgLabel->setAlignment(Qt::AlignCenter);
            imgLabel->setFixedSize(imgSize, imgSize);

            QImage img(path);
            if (!img.isNull()) {
                QPixmap px = QPixmap::fromImage(img).scaled(
                    imgSize - 2, imgSize - 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imgLabel->setPixmap(px);
                imgLabel->setStyleSheet("border:1px solid #888; background:black;");
            } else {
                imgLabel->setText("—");
                imgLabel->setStyleSheet("border:1px dashed #bbb; color:#aaa; background:#f0f0f0;");
            }
            imgLabel->setToolTip(QString("%1 — %2 — Slice %3")
                .arg(m_timepointDirs[t], orients[o]).arg(m_currentSliceNum));
            imgRow->addWidget(imgLabel);
        }
        m_rightContentLayout->addLayout(imgRow);
    }

    m_rightContentLayout->addStretch();

    m_lblTitle->setText(QString("Slice %1/%2 — %3 timepoints × 3 orientations")
        .arg(m_currentSliceNum+1).arg(m_numSlices).arg(numTP));
}

} // namespace CBCTAlign
