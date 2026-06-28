/**
 * @file ComparisonWidget.h
 */

#ifndef COMPARISONWIDGET_H
#define COMPARISONWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <vector>

#include "SliceViewer2D.h"
#include "SliceExtractor.h"

namespace CBCTAlign {

class ComparisonWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ComparisonWidget(QWidget* parent = nullptr);

    void setSlices(const std::vector<Slice2D>& slices, SliceOrientation orientation);

    void setSynchronizedZoom(bool enabled);

    void clear();
    
signals:
    void sliceClicked(int timepoint);
    void windowLevelChanged(double center, double width);
    
private:
    void clearViewers();
    void setupGrid(int numSlices);
    
    QScrollArea* m_scrollArea;
    QWidget* m_container;
    QGridLayout* m_gridLayout;
    
    std::vector<SliceViewer2D*> m_viewers;
    std::vector<QLabel*> m_labels;
    
    bool m_syncZoom;
    int m_numColumns;  
};

} // namespace CBCTAlign

#endif 
