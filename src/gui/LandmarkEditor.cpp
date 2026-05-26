// LandmarkEditor.cpp
#include "LandmarkEditor.h"
#include <QVBoxLayout>
#include <QHeaderView>

namespace CBCTAlign {

LandmarkEditor::LandmarkEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    
    m_table = new QTableWidget;
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Nom", "X", "Y", "Z", "Confiance"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    
    layout->addWidget(m_table);
}

void LandmarkEditor::setLandmarks(const std::vector<Landmark>& landmarks) {
    m_table->setRowCount(landmarks.size());
    for (size_t i = 0; i < landmarks.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(landmarks[i].name));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(landmarks[i].position.x(), 'f', 2)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(landmarks[i].position.y(), 'f', 2)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(landmarks[i].position.z(), 'f', 2)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(landmarks[i].confidence, 'f', 2)));
    }
}

} // namespace CBCTAlign
