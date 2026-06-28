/**
 * @file ComparisonWidget.cpp
 */

#include "ComparisonWidget.h"
#include "../core/SlicePlaneCalculator.h"
#include <QVBoxLayout>
#include <QGroupBox>

namespace CBCTAlign {

ComparisonWidget::ComparisonWidget(QWidget* parent)
    : QWidget(parent)
    , m_syncZoom(true)
    , m_numColumns(3) 
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    

    auto* titleLabel = new QLabel("Comparaison Multi-Temps");
    titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold; padding: 5px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    

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
        auto* emptyLabel = new QLabel("No slices to display.\nRun the pipeline, then click 'Refresh slices'.");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: gray; font-style: italic; padding: 50px;");
        m_gridLayout->addWidget(emptyLabel, 0, 0);
        return;
    }
    
    setupGrid(slices.size());
    
    QString orientStr = SlicePlaneCalculator::orientationToString(orientation);
    
    int row = 0;
    int col = 1;  
    m_gridLayout->setColumnStretch(0, 1);
    m_gridLayout->setColumnStretch(m_numColumns + 1, 1);
    
    for (size_t i = 0; i < slices.size(); ++i) {
        const auto& slice = slices[i];
        

        auto* groupBox = new QGroupBox;
        auto* groupLayout = new QVBoxLayout(groupBox);
        groupLayout->setSpacing(5);
        groupLayout->setContentsMargins(5, 5, 5, 5);
        

        auto* titleLabel = new QLabel(QString("<b>%1</b>").arg(slice.timepoint));
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font-size: 11pt; color: #2196F3;");
        groupLayout->addWidget(titleLabel);
        

        auto* viewer = new SliceViewer2D;
        viewer->setSlice(slice);
        viewer->setMinimumSize(250, 250);
        viewer->setMaximumSize(400, 400);
        groupLayout->addWidget(viewer);
        
        m_viewers.push_back(viewer);
        

        auto* infoLabel = new QLabel(QString("%1\n%2 × %3 pixels\nSpacing: %4 mm")
            .arg(orientStr)
            .arg(slice.width)
            .arg(slice.height)
            .arg(slice.pixelSpacing, 0, 'f', 2));
        infoLabel->setAlignment(Qt::AlignCenter);
        infoLabel->setStyleSheet("font-size: 9pt; color: #666;");
        groupLayout->addWidget(infoLabel);
        

        m_gridLayout->addWidget(groupBox, row, col);
        

        col++;
        if (col >= m_numColumns) {
            col = 0;
            row++;
        }
    }
    

    m_container->adjustSize();
}

void ComparisonWidget::setSynchronizedZoom(bool enabled)
{
    m_syncZoom = enabled;
    

}

void ComparisonWidget::clear()
{
    clearViewers();
}

void ComparisonWidget::clearViewers()
{

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

    if (numSlices <= 2) {
        m_numColumns = 2;
    } else if (numSlices <= 6) {
        m_numColumns = 3;
    } else {
        m_numColumns = 4;
    }
}

} // namespace CBCTAlign
