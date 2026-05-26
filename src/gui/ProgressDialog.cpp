// ProgressDialog.cpp
#include "ProgressDialog.h"
#include <QVBoxLayout>

namespace CBCTAlign {

ProgressDialog::ProgressDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Pipeline en cours...");
    setMinimumWidth(400);
    setModal(true);
    
    auto* layout = new QVBoxLayout(this);
    
    m_lblStage = new QLabel("Initialisation...");
    m_lblStage->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_lblStage);
    
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    layout->addWidget(m_progressBar);
    
    m_lblMessage = new QLabel("");
    layout->addWidget(m_lblMessage);
    
    m_btnCancel = new QPushButton("Annuler");
    connect(m_btnCancel, &QPushButton::clicked, this, &ProgressDialog::cancelRequested);
    layout->addWidget(m_btnCancel);
}

void ProgressDialog::setProgress(int value) { 
    m_progressBar->setValue(value); 
}

void ProgressDialog::setMessage(const QString& message) { 
    m_lblMessage->setText(message); 
}

void ProgressDialog::setStage(const QString& stage) { 
    m_lblStage->setText(stage); 
}

} // namespace CBCTAlign
