/**
 * @file SliceNormalizer.cpp
 */
#include "SliceNormalizer.h"
#include "Logger.h"

#include <QDebug>
#include <algorithm>

namespace CBCTAlign {

SliceNormalizer::SliceNormalizer()
{
}


SliceNormalizer::BoundingBox
SliceNormalizer::detectContentBBox(const QImage& image) const
{
    BoundingBox bbox;
    if (image.isNull() || image.width() == 0 || image.height() == 0) {
        return bbox;
    }

    QImage gray = image;
    if (gray.format() != QImage::Format_Grayscale8) {
        gray = gray.convertToFormat(QImage::Format_Grayscale8);
    }

    const int W = gray.width();
    const int H = gray.height();

    int minX = W, maxX = -1;
    int minY = H, maxY = -1;

    for (int y = 0; y < H; ++y) {
        const uchar* row = gray.constScanLine(y);
        for (int x = 0; x < W; ++x) {
            if (row[x] > m_blackThreshold) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < 0) {
        return bbox;
    }

    bbox.left   = minX;
    bbox.right  = maxX;
    bbox.top    = minY;
    bbox.bottom = maxY;
    bbox.valid  = true;

    return bbox;
}


QImage SliceNormalizer::cropImage(const QImage& image,
                                   const BoundingBox& bbox) const
{
    if (!bbox.valid || image.isNull()) return image;

    int top    = std::min(image.height() - 1, bbox.top    + m_safetyMargin);
    int left   = std::min(image.width()  - 1, bbox.left   + m_safetyMargin);
    int bottom = std::max(0, bbox.bottom - m_safetyMargin);
    int right  = std::max(0, bbox.right  - m_safetyMargin);

    if (bottom <= top || right <= left) return image;

    int w = right  - left + 1;
    int h = bottom - top  + 1;

    return image.copy(left, top, w, h);
}


int SliceNormalizer::detectSmallestFovIndex(const std::vector<QImage>& images) const
{
    int smallestIdx = 0;
    long smallestArea = -1;

    for (size_t i = 0; i < images.size(); ++i) {
        BoundingBox bbox = detectContentBBox(images[i]);
        if (!bbox.valid) continue;

        long area = static_cast<long>(bbox.width()) *
                    static_cast<long>(bbox.height());
        if (smallestArea < 0 || area < smallestArea) {
            smallestArea = area;
            smallestIdx  = static_cast<int>(i);
        }
    }
    return smallestIdx;
}

} // namespace CBCTAlign
