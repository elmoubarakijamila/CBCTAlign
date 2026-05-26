/**
 * @file ComparisonWidget_IMPROVED.h
 * @brief Widget amélioré pour comparaison multi-temps avec grid layout
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
    
    /**
     * @brief Afficher les coupes de tous les timepoints
     * @param slices Vecteur de coupes (une par timepoint)
     * @param orientation Orientation des coupes
     */
    void setSlices(const std::vector<Slice2D>& slices, SliceOrientation orientation);
    
    /**
     * @brief Activer/désactiver le zoom synchronisé
     */
    void setSynchronizedZoom(bool enabled);
    
    /**
     * @brief Effacer toutes les vues
     */
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
    int m_numColumns;  // Nombre de colonnes dans la grille
};

} // namespace CBCTAlign

#endif // COMPARISONWIDGET_H
