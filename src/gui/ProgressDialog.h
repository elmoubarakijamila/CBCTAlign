// ProgressDialog.h
#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

namespace CBCTAlign {

class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(QWidget* parent = nullptr);
    void setProgress(int value);
    void setMessage(const QString& message);
    void setStage(const QString& stage);

signals:
    void cancelRequested();

private:
    QProgressBar* m_progressBar;
    QLabel* m_lblMessage;
    QLabel* m_lblStage;
    QPushButton* m_btnCancel;
};

} // namespace CBCTAlign

#endif 
