/**
 * @file SliceNormalizer.h
 * @brief Normalisation des coupes 2D au plus petit FOV detecte.
 *
 * PROBLEME :
 *   Apres registration sur la grille de T0 (FOV craniofacial 103x103mm),
 *   les volumes T1 (FOV dental 83x83mm) et T2 sont resamples sur cette
 *   grande grille. T1 a des bords noirs car son contenu reel ne couvre
 *   que ~80% de la grille.
 *
 * SOLUTION :
 *   1. Pour chaque triplet (T0, T1, T2) au meme index de slice et
 *      meme orientation, detecter la bbox du contenu reel de T1.
 *   2. Cropper les 3 timepoints sur cette bbox commune.
 *   3. Resultat : 3 coupes de meme taille montrant exactement la
 *      meme zone anatomique.
 */
#ifndef SLICE_NORMALIZER_H
#define SLICE_NORMALIZER_H

#include <QString>
#include <QImage>
#include <QRect>
#include <vector>

namespace CBCTAlign {

class SliceNormalizer
{
public:
    struct BoundingBox {
        int top    = 0;
        int left   = 0;
        int bottom = 0;
        int right  = 0;
        bool valid = false;

        int width()  const { return right  - left + 1; }
        int height() const { return bottom - top  + 1; }
        QRect toQRect() const { return QRect(left, top, width(), height()); }
    };

    SliceNormalizer();
    ~SliceNormalizer() = default;

    /// Seuil de detection du noir (0-255). Default: 8.
    void setBlackThreshold(int threshold) { m_blackThreshold = threshold; }

    /// Marge interieure de securite (pixels). Default: 2.
    void setSafetyMargin(int margin) { m_safetyMargin = margin; }

    /// Active/desactive la normalisation. Default: true.
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    /// Detecte la bbox du contenu non-noir d'une image.
    BoundingBox detectContentBBox(const QImage& image) const;

    /// Crop une image avec la bbox donnee (applique la marge de securite).
    QImage cropImage(const QImage& image, const BoundingBox& bbox) const;

    /// Trouve l'index de l'image avec le plus petit FOV dans une liste.
    int detectSmallestFovIndex(const std::vector<QImage>& images) const;

private:
    int  m_blackThreshold = 8;
    int  m_safetyMargin   = 2;
    bool m_enabled        = true;
};

} // namespace CBCTAlign

#endif // SLICE_NORMALIZER_H
