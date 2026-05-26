/**
 * @file ComparisonWidget_IMPROVED.cpp
 * @brief Implémentation du widget de comparaison multi-temps
 */

#include "ComparisonWidget.h"
#include "../core/SlicePlaneCalculator.h"
#include <QVBoxLayout>
#include <QGroupBox>

namespace CBCTAlign {

ComparisonWidget::ComparisonWidget(QWidget* parent)
    : QWidget(parent)
    , m_syncZoom(true)
    , m_numColumns(3)  // 3 coupes par ligne par défaut
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // Titre
    auto* titleLabel = new QLabel("Comparaison Multi-Temps");
    titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold; padding: 5px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Scroll area pour la grille
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    m_container = new QWidget;
    m_gridLayout = new QGridLayout(m_container);
    m_gridLayout->setSpacing(10);
    m_gridLayout->setContentsMargins(10, 10, 10, 10);
    
    m_scrollArea->setWidget(m_container);
    mainLayout->addWidget(m_scrollArea);
}

void ComparisonWidget::setSlices(const std::vector<Slice2D>& slices, SliceOrientation orientation)
{
    clearViewers();
    
    if (slices.empty()) {
        auto* emptyLabel = new QLabel("Aucune coupe à afficher.\nExécutez le pipeline puis cliquez sur 'Mettre à jour les coupes'.");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: gray; font-style: italic; padding: 50px;");
        m_gridLayout->addWidget(emptyLabel, 0, 0);
        return;
    }
    
    setupGrid(slices.size());
    
    QString orientStr = SlicePlaneCalculator::orientationToString(orientation);
    
    int row = 0;
    int col = 0;
    
    for (size_t i = 0; i < slices.size(); ++i) {
        const auto& slice = slices[i];
        
        // Créer un groupe pour chaque coupe
        auto* groupBox = new QGroupBox;
        auto* groupLayout = new QVBoxLayout(groupBox);
        groupLayout->setSpacing(5);
        groupLayout->setContentsMargins(5, 5, 5, 5);
        
        // Label titre
        auto* titleLabel = new QLabel(QString("<b>%1</b>").arg(slice.timepoint));
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font-size: 11pt; color: #2196F3;");
        groupLayout->addWidget(titleLabel);
        
        // Viewer de la coupe
        auto* viewer = new SliceViewer2D;
        viewer->setSlice(slice);
        viewer->setMinimumSize(250, 250);
        viewer->setMaximumSize(400, 400);
        groupLayout->addWidget(viewer);
        
        m_viewers.push_back(viewer);
        
        // Label info
        auto* infoLabel = new QLabel(QString("%1\n%2 × %3 pixels\nSpacing: %4 mm")
            .arg(orientStr)
            .arg(slice.width)
            .arg(slice.height)
            .arg(slice.pixelSpacing, 0, 'f', 2));
        infoLabel->setAlignment(Qt::AlignCenter);
        infoLabel->setStyleSheet("font-size: 9pt; color: #666;");
        groupLayout->addWidget(infoLabel);
        
        // Ajouter dans la grille
        m_gridLayout->addWidget(groupBox, row, col);
        
        // Passer à la colonne suivante
        col++;
        if (col >= m_numColumns) {
            col = 0;
            row++;
        }
    }
    
    // Ajuster la taille du container
    m_container->adjustSize();
}

void ComparisonWidget::setSynchronizedZoom(bool enabled)
{
    m_syncZoom = enabled;
    
    // Connecter les signaux de zoom de tous les viewers
}

void ComparisonWidget::clear()
{
    clearViewers();
}

void ComparisonWidget::clearViewers()
{
    // Supprimer tous les widgets de la grille
    QLayoutItem* item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    m_viewers.clear();
    m_labels.clear();
}

void ComparisonWidget::setupGrid(int numSlices)
{
    // Déterminer le nombre optimal de colonnes
    if (numSlices <= 2) {
        m_numColumns = 2;
    } else if (numSlices <= 6) {
        m_numColumns = 3;
    } else {
        m_numColumns = 4;
    }
}

} // namespace CBCTAlign
