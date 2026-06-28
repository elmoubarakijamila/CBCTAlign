/**
 * @file SliceNormalizer.h
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


    void setBlackThreshold(int threshold) { m_blackThreshold = threshold; }


    void setSafetyMargin(int margin) { m_safetyMargin = margin; }


    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }


    BoundingBox detectContentBBox(const QImage& image) const;


    QImage cropImage(const QImage& image, const BoundingBox& bbox) const;


    int detectSmallestFovIndex(const std::vector<QImage>& images) const;

private:
    int  m_blackThreshold = 8;
    int  m_safetyMargin   = 2;
    bool m_enabled        = true;
};

} // namespace CBCTAlign

#endif 
