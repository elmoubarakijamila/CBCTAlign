// SliceViewer2D.h
#ifndef SLICEVIEWER2D_H
#define SLICEVIEWER2D_H
#include <QWidget>
#include <QLabel>
#include <QImage>
#include "SliceExtractor.h"
namespace CBCTAlign {
class SliceViewer2D : public QWidget {
    Q_OBJECT
public:
    explicit SliceViewer2D(QWidget* parent = nullptr);
    void setSlice(const Slice2D& slice);
    void setWindowLevel(double center, double width);
private:
    QLabel* m_imageLabel;
    Slice2D m_currentSlice;
    double m_windowCenter = 400;
    double m_windowWidth = 1500;
    void updateDisplay();
};
} // namespace CBCTAlign
#endif
