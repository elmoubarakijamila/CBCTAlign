// SliceViewer2D.cpp
#include "SliceViewer2D.h"
#include <QVBoxLayout>
#include <QPixmap>
#include <algorithm>

namespace CBCTAlign {

SliceViewer2D::SliceViewer2D(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    m_imageLabel = new QLabel;
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(200, 200);
    m_imageLabel->setStyleSheet("background-color: black;");
    layout->addWidget(m_imageLabel);
}

void SliceViewer2D::setSlice(const Slice2D& slice) {
    m_currentSlice = slice;
    
    if (!slice.data.empty()) {
        float maxVal = *std::max_element(slice.data.begin(), slice.data.end());
        
        if (maxVal <= 255.0f) {

            m_windowCenter = 127.0;
            m_windowWidth = 255.0;
        } else {

            m_windowCenter = 400.0;
            m_windowWidth = 1500.0;
        }
    }
    
    updateDisplay();
}

void SliceViewer2D::setWindowLevel(double center, double width) {
    m_windowCenter = center;
    m_windowWidth = width;
    updateDisplay();
}

void SliceViewer2D::updateDisplay() {
    if (m_currentSlice.data.empty()) return;
    
    QImage img = m_currentSlice.toQImage(m_windowCenter, m_windowWidth);
    QPixmap pixmap = QPixmap::fromImage(img).scaled(
        m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(pixmap);
}

} // namespace CBCTAlign
