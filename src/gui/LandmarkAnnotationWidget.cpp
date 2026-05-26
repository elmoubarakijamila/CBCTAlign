/**
 * @file LandmarkAnnotationWidget.cpp
 * @brief Widget d'annotation manuelle + détection automatique ALI_CBCT
 *
 * MODIFICATIONS v2 (Solution B - Dual Mode):
 *   - Ajout du landmark "Sella" (S) comme point fixe supporté
 *   - hasMinimumLandmarks() accepte Sella OU Nasion
 *   - Bouton "Auto-détection ALI_CBCT" → appelle Slicer CLI
 *   - Import fichier .mrk.json (format 3D Slicer Markups)
 *   - Conversion coordonnées LPS (Slicer) → physiques (ITK/CBCTAlign)
 */
#include "LandmarkAnnotationWidget.h"
#include "CBCTVolume.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <iostream>

namespace CBCTAlign {

LandmarkAnnotationWidget::LandmarkAnnotationWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
}

LandmarkAnnotationWidget::~LandmarkAnnotationWidget()
{
    if (m_aliProcess) {
        m_aliProcess->kill();
        m_aliProcess->deleteLater();
    }
}

//  SETUP UI

void LandmarkAnnotationWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ─── Top bar ───
    QHBoxLayout* topBar = new QHBoxLayout();
    m_volumeLabel = new QLabel("Aucun volume charge");
    m_volumeLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    topBar->addWidget(m_volumeLabel);
    topBar->addStretch();
    topBar->addWidget(new QLabel("Point a annoter :"));
    m_landmarkSelector = new QComboBox();
    m_landmarkSelector->addItems(m_landmarkNames);
    m_landmarkSelector->setMinimumWidth(150);
    topBar->addWidget(m_landmarkSelector);
    mainLayout->addLayout(topBar);

    // ─── Mode automatique ALI_CBCT ───
    QGroupBox* grpAuto = new QGroupBox("Detection Automatique (ALI_CBCT)");
    grpAuto->setStyleSheet("QGroupBox { font-weight: bold; }");
    QHBoxLayout* autoLayout = new QHBoxLayout(grpAuto);

    m_btnAutoDetect = new QPushButton("Detecter Sella (ALI_CBCT)");
    m_btnAutoDetect->setStyleSheet(
        "QPushButton { background-color: #9C27B0; color: white; "
        "font-weight: bold; padding: 8px 16px; }"
        "QPushButton:disabled { background-color: #ccc; }");
    m_btnAutoDetect->setEnabled(false);
    m_btnAutoDetect->setToolTip(
        "Lance ALI_CBCT via 3D Slicer pour detecter automatiquement\n"
        "le point Sella (S) dans le volume CBCT.\n"
        "Necessite: Slicer + SlicerAutomatedDentalTools + modeles Cranial_Base");
    autoLayout->addWidget(m_btnAutoDetect);

    m_btnImportJSON = new QPushButton("Importer .mrk.json");
    m_btnImportJSON->setStyleSheet(
        "QPushButton { background-color: #607D8B; color: white; padding: 8px 12px; }");
    m_btnImportJSON->setToolTip(
        "Importer un fichier .mrk.json genere par ALI_CBCT dans 3D Slicer.\n"
        "Format: Slicer Markups Fiducial JSON (coordonnees LPS).");
    autoLayout->addWidget(m_btnImportJSON);

    autoLayout->addStretch();
    mainLayout->addWidget(grpAuto);

    // ─── 3 slice navigation sliders ───
    QGroupBox* grpManual = new QGroupBox("Annotation Manuelle (MPR)");
    grpManual->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout* manualLayout = new QVBoxLayout(grpManual);

    QGridLayout* viewGrid = new QGridLayout();

    // Axial
    QVBoxLayout* axialLayout = new QVBoxLayout();
    axialLayout->addWidget(new QLabel("Axial (Z)"));
    m_axialSlider = new QSlider(Qt::Horizontal);
    axialLayout->addWidget(m_axialSlider);
    m_axialLabel = new QLabel("Coupe: 0/0");
    axialLayout->addWidget(m_axialLabel);
    viewGrid->addLayout(axialLayout, 0, 0);

    // Sagittal
    QVBoxLayout* sagLayout = new QVBoxLayout();
    sagLayout->addWidget(new QLabel("Sagittal (X)"));
    m_sagittalSlider = new QSlider(Qt::Horizontal);
    sagLayout->addWidget(m_sagittalSlider);
    m_sagittalLabel = new QLabel("Coupe: 0/0");
    sagLayout->addWidget(m_sagittalLabel);
    viewGrid->addLayout(sagLayout, 0, 1);

    // Coronal
    QVBoxLayout* corLayout = new QVBoxLayout();
    corLayout->addWidget(new QLabel("Coronal (Y)"));
    m_coronalSlider = new QSlider(Qt::Horizontal);
    corLayout->addWidget(m_coronalSlider);
    m_coronalLabel = new QLabel("Coupe: 0/0");
    corLayout->addWidget(m_coronalLabel);
    viewGrid->addLayout(corLayout, 0, 2);

    manualLayout->addLayout(viewGrid);

    // Info: current physical coordinates
    QLabel* infoLabel = new QLabel(
        "Naviguez avec les sliders pour positionner le point fixe.\n"
        "La position du crosshair = intersection des 3 plans.");
    infoLabel->setStyleSheet("color: #555; font-style: italic;");
    manualLayout->addWidget(infoLabel);

    // Button to set landmark at current slider position
    QPushButton* setBtn = new QPushButton("Placer le point ici");
    setBtn->setStyleSheet("background-color:#FF9800;color:white;font-weight:bold;padding:6px;");
    connect(setBtn, &QPushButton::clicked, [this]() {
        double px = m_origin[0] + m_sagittalIndex * m_spacing[0];
        double py = m_origin[1] + m_coronalIndex * m_spacing[1];
        double pz = m_origin[2] + m_axialIndex * m_spacing[2];
        onViewClicked(0, px, py);
    });
    manualLayout->addWidget(setBtn);

    mainLayout->addWidget(grpManual);

    // ─── Landmark table ───
    QGroupBox* landmarkGroup = new QGroupBox("Points annotes");
    QVBoxLayout* landmarkLayout = new QVBoxLayout(landmarkGroup);
    m_landmarkTable = new QTableWidget(0, 6);
    m_landmarkTable->setHorizontalHeaderLabels(
        {"Statut", "Nom", "Source", "X (mm)", "Y (mm)", "Z (mm)"});
    m_landmarkTable->horizontalHeader()->setStretchLastSection(true);
    m_landmarkTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_landmarkTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    landmarkLayout->addWidget(m_landmarkTable);

    QHBoxLayout* tableButtons = new QHBoxLayout();
    m_modifyBtn = new QPushButton("Modifier");
    m_removeBtn = new QPushButton("Supprimer");
    m_modifyBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);
    tableButtons->addWidget(m_modifyBtn);
    tableButtons->addWidget(m_removeBtn);
    tableButtons->addStretch();
    landmarkLayout->addLayout(tableButtons);
    mainLayout->addWidget(landmarkGroup);

    // ─── Action buttons ───
    QHBoxLayout* actionBar = new QHBoxLayout();
    m_saveBtn = new QPushButton("Sauvegarder");
    m_loadBtn = new QPushButton("Charger CSV");
    m_clearBtn = new QPushButton("Effacer tout");
    m_validateBtn = new QPushButton("Valider et continuer");
    m_validateBtn->setStyleSheet(
        "QPushButton { background-color: #2196F3; color: white; "
        "font-weight: bold; padding: 8px 16px; }");
    m_validateBtn->setEnabled(false);
    actionBar->addWidget(m_saveBtn);
    actionBar->addWidget(m_loadBtn);
    actionBar->addWidget(m_clearBtn);
    actionBar->addStretch();
    actionBar->addWidget(m_validateBtn);
    mainLayout->addLayout(actionBar);

    // ─── Status bar ───
    m_statusLabel = new QLabel("Pret. Chargez un volume pour commencer.");
    m_statusLabel->setStyleSheet("color: #666; padding: 4px;");
    mainLayout->addWidget(m_statusLabel);

    updateLandmarkTable();
}

void LandmarkAnnotationWidget::setupConnections()
{
    connect(m_axialSlider, &QSlider::valueChanged,
            this, &LandmarkAnnotationWidget::onAxialSliderChanged);
    connect(m_sagittalSlider, &QSlider::valueChanged,
            this, &LandmarkAnnotationWidget::onSagittalSliderChanged);
    connect(m_coronalSlider, &QSlider::valueChanged,
            this, &LandmarkAnnotationWidget::onCoronalSliderChanged);

    connect(m_saveBtn, &QPushButton::clicked, this, &LandmarkAnnotationWidget::onSave);
    connect(m_loadBtn, &QPushButton::clicked, this, &LandmarkAnnotationWidget::onLoad);
    connect(m_clearBtn, &QPushButton::clicked, this, &LandmarkAnnotationWidget::onClear);
    connect(m_validateBtn, &QPushButton::clicked, this, &LandmarkAnnotationWidget::onValidate);
    connect(m_removeBtn, &QPushButton::clicked, this, &LandmarkAnnotationWidget::onRemoveLandmark);
    connect(m_modifyBtn, &QPushButton::clicked, this, &LandmarkAnnotationWidget::onModifyLandmark);

    // ALI_CBCT
    connect(m_btnAutoDetect, &QPushButton::clicked, this, &LandmarkAnnotationWidget::onAutoDetectALI);
    connect(m_btnImportJSON, &QPushButton::clicked, this, &LandmarkAnnotationWidget::onImportSlicerJSON);

    connect(m_landmarkTable, &QTableWidget::itemSelectionChanged,
            [this]() {
                int row = m_landmarkTable->currentRow();
                bool valid = (row >= 0 && row < m_landmarkNames.size());
                bool isSet = valid && m_landmarks.contains(m_landmarkNames[row]);
                m_removeBtn->setEnabled(isSet);
                m_modifyBtn->setEnabled(isSet);
            });
}

//  ALI_CBCT PATHS CONFIGURATION

void LandmarkAnnotationWidget::setALICBCTPaths(const QString& slicerPython,
                                                  const QString& modelsDir)
{
    m_slicerPythonPath = slicerPython;
    m_aliModelsDir = modelsDir;
    std::cout << "ALI_CBCT paths configured:" << std::endl;
    std::cout << "  PythonSlicer: " << slicerPython.toStdString() << std::endl;
    std::cout << "  Models: " << modelsDir.toStdString() << std::endl;
}

//  VOLUME LOADING

void LandmarkAnnotationWidget::setVolume(
    std::shared_ptr<CBCTVolume> volume,
    const QString& patientID,
    const QString& timepoint)
{
    m_volume = volume;
    m_patientID = patientID;
    m_timepoint = timepoint;

    auto itkImg = volume->getITKImage();
    if (!itkImg) return;

    auto region = itkImg->GetLargestPossibleRegion();
    auto size = region.GetSize();
    auto spacing = itkImg->GetSpacing();
    auto origin = itkImg->GetOrigin();

    m_dims[0] = size[0]; m_dims[1] = size[1]; m_dims[2] = size[2];
    m_spacing[0] = spacing[0]; m_spacing[1] = spacing[1]; m_spacing[2] = spacing[2];
    m_origin[0] = origin[0]; m_origin[1] = origin[1]; m_origin[2] = origin[2];

    m_axialSlider->setRange(0, m_dims[2] - 1);
    m_axialSlider->setValue(m_dims[2] / 2);
    m_sagittalSlider->setRange(0, m_dims[0] - 1);
    m_sagittalSlider->setValue(m_dims[0] / 2);
    m_coronalSlider->setRange(0, m_dims[1] - 1);
    m_coronalSlider->setValue(m_dims[1] / 2);

    m_volumeLabel->setText(
        QString("Volume : %1 - %2 [%3x%4x%5, spacing=%6mm]")
            .arg(patientID).arg(timepoint)
            .arg(m_dims[0]).arg(m_dims[1]).arg(m_dims[2])
            .arg(m_spacing[0], 0, 'f', 3));

    // Activer le bouton auto-detect si les chemins sont configurés
    m_btnAutoDetect->setEnabled(!m_slicerPythonPath.isEmpty() && !m_aliModelsDir.isEmpty());

    updateSliceLabels();
    m_statusLabel->setText("Volume charge. Utilisez le mode Auto ou Manuel.");
    std::cout << "Volume loaded for annotation: " << patientID.toStdString()
              << " " << timepoint.toStdString() << std::endl;
}

//  NAVIGATION

void LandmarkAnnotationWidget::onAxialSliderChanged(int value)
{
    m_axialIndex = value;
    updateSliceLabels();
}

void LandmarkAnnotationWidget::onSagittalSliderChanged(int value)
{
    m_sagittalIndex = value;
    updateSliceLabels();
}

void LandmarkAnnotationWidget::onCoronalSliderChanged(int value)
{
    m_coronalIndex = value;
    updateSliceLabels();
}

void LandmarkAnnotationWidget::updateSliceLabels()
{
    double px = m_origin[0] + m_sagittalIndex * m_spacing[0];
    double py = m_origin[1] + m_coronalIndex * m_spacing[1];
    double pz = m_origin[2] + m_axialIndex * m_spacing[2];

    m_axialLabel->setText(
        QString("Z: %1/%2 (%3 mm)").arg(m_axialIndex).arg(m_dims[2] - 1).arg(pz, 0, 'f', 2));
    m_sagittalLabel->setText(
        QString("X: %1/%2 (%3 mm)").arg(m_sagittalIndex).arg(m_dims[0] - 1).arg(px, 0, 'f', 2));
    m_coronalLabel->setText(
        QString("Y: %1/%2 (%3 mm)").arg(m_coronalIndex).arg(m_dims[1] - 1).arg(py, 0, 'f', 2));
}

void LandmarkAnnotationWidget::updateViews()
{
    // Placeholder - no VTK rendering in this simplified version
}

//  MANUAL LANDMARK PLACEMENT

void LandmarkAnnotationWidget::onViewClicked(int /*viewIndex*/,
                                              double /*clickX*/, double /*clickY*/)
{
    if (!m_volume) return;

    double px = m_origin[0] + m_sagittalIndex * m_spacing[0];
    double py = m_origin[1] + m_coronalIndex * m_spacing[1];
    double pz = m_origin[2] + m_axialIndex * m_spacing[2];
    Eigen::Vector3d physPos(px, py, pz);

    QString landmarkName = m_landmarkSelector->currentText();
    m_landmarks[landmarkName] = physPos;

    updateLandmarkTable();
    m_validateBtn->setEnabled(hasMinimumLandmarks());

    m_statusLabel->setText(
        QString("%1 annote manuellement a (%2, %3, %4) mm")
            .arg(landmarkName)
            .arg(physPos.x(), 0, 'f', 2)
            .arg(physPos.y(), 0, 'f', 2)
            .arg(physPos.z(), 0, 'f', 2));

    std::cout << "[MANUAL] Landmark " << landmarkName.toStdString()
              << " set at [" << px << ", " << py << ", " << pz << "]" << std::endl;

    emit landmarksChanged();
}

Eigen::Vector3d LandmarkAnnotationWidget::viewClickToPhysical(
    int viewIndex, double clickX, double clickY)
{
    double physX, physY, physZ;
    switch (viewIndex) {
        case 0:
            physX = m_origin[0] + clickX * m_spacing[0];
            physY = m_origin[1] + clickY * m_spacing[1];
            physZ = m_origin[2] + m_axialIndex * m_spacing[2];
            break;
        case 1:
            physX = m_origin[0] + m_sagittalIndex * m_spacing[0];
            physY = m_origin[1] + clickX * m_spacing[1];
            physZ = m_origin[2] + clickY * m_spacing[2];
            break;
        case 2:
            physX = m_origin[0] + clickX * m_spacing[0];
            physY = m_origin[1] + m_coronalIndex * m_spacing[1];
            physZ = m_origin[2] + clickY * m_spacing[2];
            break;
        default:
            physX = physY = physZ = 0;
    }
    return Eigen::Vector3d(physX, physY, physZ);
}

void LandmarkAnnotationWidget::physicalToSliceIndices(
    const Eigen::Vector3d& phys, int& axial, int& sagittal, int& coronal)
{
    sagittal = qBound(0, static_cast<int>((phys.x() - m_origin[0]) / m_spacing[0]), m_dims[0] - 1);
    coronal = qBound(0, static_cast<int>((phys.y() - m_origin[1]) / m_spacing[1]), m_dims[1] - 1);
    axial = qBound(0, static_cast<int>((phys.z() - m_origin[2]) / m_spacing[2]), m_dims[2] - 1);
}

//  ALI_CBCT AUTO-DETECTION

void LandmarkAnnotationWidget::onAutoDetectALI()
{
    if (!m_volume) {
        QMessageBox::warning(this, "Erreur", "Chargez un volume CBCT d'abord.");
        return;
    }

    if (m_slicerPythonPath.isEmpty() || m_aliModelsDir.isEmpty()) {
        QMessageBox::warning(this, "Configuration manquante",
            "Les chemins ALI_CBCT ne sont pas configures.\n\n"
            "Veuillez configurer:\n"
            "  - PythonSlicer: chemin vers le binaire PythonSlicer\n"
            "  - Modeles: dossier contenant les modeles Cranial_Base\n\n"
            "Exemple:\n"
            "  PythonSlicer: /path/to/Slicer/bin/PythonSlicer\n"
            "  Modeles: /path/to/ALI_CBCT_models/Cranial_Base/");
        return;
    }

    // Vérifier que le fichier NIfTI existe
    // Le chemin est reconstruit à partir du volume chargé
    if (m_currentNIfTIPath.isEmpty()) {
        QMessageBox::warning(this, "Erreur",
            "Chemin du fichier NIfTI non disponible.\n"
            "Veuillez recharger le volume via l'interface.");
        return;
    }

    m_statusLabel->setText("Detection ALI_CBCT en cours... (peut prendre 30-60s)");
    m_btnAutoDetect->setEnabled(false);
    m_btnAutoDetect->setText("Detection en cours...");

    // Construire le script Python pour appeler ALI_CBCT
    // On lance PythonSlicer avec un script inline qui:
    // 1. Charge le volume
    // 2. Lance ALI_CBCT avec les modèles Cranial_Base
    // 3. Sauvegarde le résultat en .mrk.json
    QString outputJson = QDir::tempPath() + "/" + m_patientID + "_" + m_timepoint + "_ali_pred.mrk.json";
    
    QString pythonScript = QString(
        "import sys; sys.argv = ['', '--no-main-window']\n"
        "import slicer\n"
        "import json\n"
        "import os\n"
        "\n"
        "# Load volume\n"
        "volumeNode = slicer.util.loadVolume('%1')\n"
        "\n"
        "# Import ALI_CBCT\n"
        "from ALI_CBCT import ALI_CBCTLogic\n"
        "logic = ALI_CBCTLogic()\n"
        "\n"
        "# Configure\n"
        "modelsDir = '%2'\n"
        "landmarks = ['S']  # Sella only\n"
        "\n"
        "# Create output markup node\n"
        "markupNode = slicer.mrmlScene.AddNewNodeByClass('vtkMRMLMarkupsFiducialNode')\n"
        "markupNode.SetName('ALI_Pred')\n"
        "\n"
        "# Run prediction\n"
        "logic.process(volumeNode, markupNode, modelsDir, landmarks)\n"
        "\n"
        "# Save as JSON\n"
        "slicer.util.saveNode(markupNode, '%3')\n"
        "\n"
        "print('ALI_CBCT_OUTPUT: ' + '%3')\n"
        "sys.exit(0)\n"
    ).arg(m_currentNIfTIPath).arg(m_aliModelsDir).arg(outputJson);

    // Write script to temp file
    QString scriptPath = QDir::tempPath() + "/ali_cbct_detect.py";
    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        scriptFile.write(pythonScript.toUtf8());
        scriptFile.close();
    }

    // Launch QProcess
    if (m_aliProcess) {
        m_aliProcess->kill();
        m_aliProcess->deleteLater();
    }
    m_aliProcess = new QProcess(this);
    m_aliProcess->setProperty("outputJson", outputJson);

    connect(m_aliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LandmarkAnnotationWidget::onAutoDetectFinished);

    std::cout << "[ALI_CBCT] Launching: " << m_slicerPythonPath.toStdString()
              << " " << scriptPath.toStdString() << std::endl;

    m_aliProcess->start(m_slicerPythonPath, QStringList() << scriptPath);
}

void LandmarkAnnotationWidget::onAutoDetectFinished(int exitCode,
                                                      QProcess::ExitStatus exitStatus)
{
    m_btnAutoDetect->setEnabled(true);
    m_btnAutoDetect->setText("Detecter Sella (ALI_CBCT)");

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString errorOutput = m_aliProcess->readAllStandardError();
        std::cerr << "[ALI_CBCT] Error output:\n" << errorOutput.toStdString() << std::endl;

        m_statusLabel->setText("Echec de la detection ALI_CBCT. Voir la console.");
        QMessageBox::warning(this, "Echec ALI_CBCT",
            QString("La detection automatique a echoue (code=%1).\n\n"
                    "Utilisez le mode manuel ou importez un fichier .mrk.json existant.\n\n"
                    "Erreur:\n%2")
            .arg(exitCode).arg(errorOutput.left(500)));
        return;
    }

    // Lire le fichier JSON de sortie
    QString outputJson = m_aliProcess->property("outputJson").toString();
    std::cout << "[ALI_CBCT] Detection terminee. Lecture de: "
              << outputJson.toStdString() << std::endl;

    if (loadFromSlicerJSON(outputJson)) {
        m_statusLabel->setText(
            QString("ALI_CBCT: Sella detecte avec succes! Verifiez la position dans le tableau."));
        
        // Positionner les sliders sur le Sella détecté
        if (m_landmarks.contains("Sella")) {
            int ax, sag, cor;
            physicalToSliceIndices(m_landmarks["Sella"], ax, sag, cor);
            m_axialSlider->setValue(ax);
            m_sagittalSlider->setValue(sag);
            m_coronalSlider->setValue(cor);
        }
    } else {
        m_statusLabel->setText("Echec lecture du fichier de resultats ALI_CBCT.");
    }
}

//  IMPORT .mrk.json (3D Slicer Markups format)

void LandmarkAnnotationWidget::onImportSlicerJSON()
{
    QString filepath = QFileDialog::getOpenFileName(
        this, "Importer un fichier Slicer Markups JSON",
        QDir::homePath(),
        "Slicer Markups JSON (*.mrk.json *.json)");
    if (filepath.isEmpty()) return;

    if (loadFromSlicerJSON(filepath)) {
        m_statusLabel->setText(QString("Import reussi depuis %1").arg(filepath));
    } else {
        QMessageBox::warning(this, "Erreur d'import",
            "Impossible de lire les landmarks depuis ce fichier.\n"
            "Verifiez qu'il s'agit d'un fichier .mrk.json valide de 3D Slicer.");
    }
}

bool LandmarkAnnotationWidget::loadFromSlicerJSON(const QString& filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Cannot open: " << filepath.toStdString() << std::endl;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull()) {
        std::cerr << "JSON parse error: " << parseError.errorString().toStdString() << std::endl;
        return false;
    }

    QJsonObject root = doc.object();

    // Navigate: markups[0].controlPoints[]
    QJsonArray markups = root["markups"].toArray();
    if (markups.isEmpty()) {
        std::cerr << "No markups found in JSON" << std::endl;
        return false;
    }

    QJsonObject markup0 = markups[0].toObject();
    QString coordSystem = markup0["coordinateSystem"].toString("LPS");
    QJsonArray controlPoints = markup0["controlPoints"].toArray();

    if (controlPoints.isEmpty()) {
        std::cerr << "No control points found" << std::endl;
        return false;
    }

    QMap<QString, QString> labelToName;
    labelToName["S"]    = "Sella";
    labelToName["N"]    = "Nasion";
    labelToName["Ba"]   = "Basion";
    labelToName["RPo"]  = "RPorion";
    labelToName["LPo"]  = "LPorion";
    labelToName["RFZyg"] = "ZygionRight";
    labelToName["LFZyg"] = "ZygionLeft";

    int imported = 0;
    for (const QJsonValue& cpVal : controlPoints) {
        QJsonObject cp = cpVal.toObject();
        QString label = cp["label"].toString();
        QJsonArray pos = cp["position"].toArray();

        if (pos.size() < 3) continue;

        double x = pos[0].toDouble();
        double y = pos[1].toDouble();
        double z = pos[2].toDouble();

        // 3D Slicer utilise LPS (Left-Posterior-Superior)
        // ITK/CBCTAlign utilise aussi LPS en interne
        // Donc: pas de conversion nécessaire si le volume est
        //       chargé avec ITK (les coordonnées sont déjà LPS)
        //
        // NOTE: Si le volume utilise RAS (Right-Anterior-Superior),
        Eigen::Vector3d position(x, y, z);

        // Trouver le nom CBCTAlign correspondant
        QString name = labelToName.value(label, label);

        // Stocker le landmark
        m_landmarks[name] = position;
        imported++;

        std::cout << "[ALI_CBCT] Imported " << label.toStdString()
                  << " (" << name.toStdString() << ")"
                  << " at [" << x << ", " << y << ", " << z << "] "
                  << coordSystem.toStdString() << std::endl;
    }

    if (imported > 0) {
        updateLandmarkTable();
        m_validateBtn->setEnabled(hasMinimumLandmarks());
        emit landmarksChanged();

        std::cout << "[ALI_CBCT] Total imported: " << imported << " landmarks" << std::endl;
        return true;
    }

    return false;
}

//  LANDMARK TABLE

void LandmarkAnnotationWidget::updateLandmarkTable()
{
    m_landmarkTable->setRowCount(m_landmarkNames.size());
    for (int i = 0; i < m_landmarkNames.size(); ++i) {
        const QString& name = m_landmarkNames[i];
        bool isSet = m_landmarks.contains(name);

        auto statusItem = new QTableWidgetItem(isSet ? "V" : " ");
        statusItem->setTextAlignment(Qt::AlignCenter);
        if (isSet) {
            statusItem->setBackground(QColor("#C8E6C9")); // vert clair
        }
        m_landmarkTable->setItem(i, 0, statusItem);
        m_landmarkTable->setItem(i, 1, new QTableWidgetItem(name));

        // Source: "Auto" si importé via ALI, "Manuel" sinon
        // (pour simplifier, on ne distingue pas encore — on pourrait ajouter un QMap<QString, QString> m_landmarkSources)
        QString source = isSet ? "Manuel" : "-";
        m_landmarkTable->setItem(i, 2, new QTableWidgetItem(source));

        if (isSet) {
            const auto& pos = m_landmarks[name];
            m_landmarkTable->setItem(i, 3, new QTableWidgetItem(QString::number(pos.x(), 'f', 2)));
            m_landmarkTable->setItem(i, 4, new QTableWidgetItem(QString::number(pos.y(), 'f', 2)));
            m_landmarkTable->setItem(i, 5, new QTableWidgetItem(QString::number(pos.z(), 'f', 2)));
        } else {
            m_landmarkTable->setItem(i, 3, new QTableWidgetItem("-"));
            m_landmarkTable->setItem(i, 4, new QTableWidgetItem("-"));
            m_landmarkTable->setItem(i, 5, new QTableWidgetItem("-"));
        }
    }
}

//  SAVE / LOAD / CLEAR / VALIDATE

void LandmarkAnnotationWidget::onSave()
{
    QString filepath = QFileDialog::getSaveFileName(
        this, "Sauvegarder les landmarks",
        QDir::homePath() + "/" + m_patientID + "_landmarks.csv",
        "Fichiers CSV (*.csv)");
    if (filepath.isEmpty()) return;

    QList<LandmarkEntry> entries;
    for (auto it = m_landmarks.begin(); it != m_landmarks.end(); ++it) {
        LandmarkEntry entry;
        entry.patientID = m_patientID;
        entry.timepoint = m_timepoint;
        entry.name = it.key();
        entry.position = it.value();
        entry.confidence = 1.0;
        entries.append(entry);
    }

    if (LandmarkIO::save(filepath, entries)) {
        m_statusLabel->setText("Landmarks sauvegardes dans " + filepath);
        emit landmarksSaved(filepath);
    }
}

void LandmarkAnnotationWidget::onLoad()
{
    QString filepath = QFileDialog::getOpenFileName(
        this, "Charger des landmarks",
        QDir::homePath(), "Fichiers CSV (*.csv);;Slicer JSON (*.mrk.json *.json)");
    if (filepath.isEmpty()) return;

    // Detect format by extension
    if (filepath.endsWith(".mrk.json") || filepath.endsWith(".json")) {
        loadFromSlicerJSON(filepath);
    } else {
        loadLandmarksFromFile(filepath);
    }
}

void LandmarkAnnotationWidget::loadLandmarksFromFile(const QString& filepath)
{
    auto entries = LandmarkIO::load(filepath);
    m_landmarks.clear();
    for (const auto& entry : entries) {
        m_landmarks[entry.name] = entry.position;
    }
    if (!entries.isEmpty()) {
        m_patientID = entries.first().patientID;
        m_timepoint = entries.first().timepoint;
    }
    updateLandmarkTable();
    m_validateBtn->setEnabled(hasMinimumLandmarks());
    m_statusLabel->setText(QString("Charge %1 landmarks depuis %2").arg(entries.size()).arg(filepath));
    emit landmarksChanged();
}

void LandmarkAnnotationWidget::onClear()
{
    if (QMessageBox::question(this, "Confirmation",
            "Effacer tous les landmarks annotes ?") != QMessageBox::Yes)
        return;
    m_landmarks.clear();
    updateLandmarkTable();
    m_validateBtn->setEnabled(false);
    m_statusLabel->setText("Tous les landmarks ont ete effaces.");
    emit landmarksChanged();
}

void LandmarkAnnotationWidget::onValidate()
{
    if (!hasMinimumLandmarks()) {
        QMessageBox::warning(this, "Landmark manquant",
            "Au moins Sella (S) ou Nasion (N) doit etre annote\n"
            "comme point fixe de reference pour l'alignement.\n\n"
            "Utilisez la detection automatique ALI_CBCT\n"
            "ou annotez manuellement avec les sliders.");
        return;
    }
    std::vector<Landmark> lms = getLandmarks();
    emit landmarksValidated(lms);

    // Determine which reference point is used
    QString refName = m_landmarks.contains("Sella") ? "Sella" : "Nasion";
    const auto& refPos = m_landmarks[refName];
    m_statusLabel->setText(
        QString("Landmarks valides. Point fixe: %1 [%2, %3, %4] mm")
            .arg(refName)
            .arg(refPos.x(), 0, 'f', 2)
            .arg(refPos.y(), 0, 'f', 2)
            .arg(refPos.z(), 0, 'f', 2));
}

void LandmarkAnnotationWidget::onRemoveLandmark()
{
    int row = m_landmarkTable->currentRow();
    if (row < 0 || row >= m_landmarkNames.size()) return;
    m_landmarks.remove(m_landmarkNames[row]);
    updateLandmarkTable();
    m_validateBtn->setEnabled(hasMinimumLandmarks());
    emit landmarksChanged();
}

void LandmarkAnnotationWidget::onModifyLandmark()
{
    int row = m_landmarkTable->currentRow();
    if (row < 0 || row >= m_landmarkNames.size()) return;
    m_landmarkSelector->setCurrentIndex(row);
    m_statusLabel->setText(
        QString("Naviguez avec les sliders puis cliquez 'Placer le point ici' pour repositionner %1")
            .arg(m_landmarkNames[row]));
}

//  ACCESSORS

std::vector<Landmark> LandmarkAnnotationWidget::getLandmarks() const
{
    std::vector<Landmark> lms;
    for (auto it = m_landmarks.begin(); it != m_landmarks.end(); ++it) {
        Landmark lm;
        lm.name = it.key();
        lm.position = it.value();
        lm.confidence = 1.0;

        // Abbreviations
        if (it.key() == "Sella")        lm.abbreviation = "S";
        else if (it.key() == "Nasion")  lm.abbreviation = "N";
        else if (it.key() == "Menton")  lm.abbreviation = "Me";
        else if (it.key() == "Basion")  lm.abbreviation = "Ba";
        else if (it.key() == "ZygionLeft")  lm.abbreviation = "ZL";
        else if (it.key() == "ZygionRight") lm.abbreviation = "ZR";
        else lm.abbreviation = it.key().left(2);

        lms.push_back(lm);
    }
    return lms;
}

// Priorité: Sella (détectable par ALI_CBCT dans le FOV)
bool LandmarkAnnotationWidget::hasMinimumLandmarks() const
{
    return m_landmarks.contains("Sella") || m_landmarks.contains("Nasion");
}

} // namespace CBCTAlign
