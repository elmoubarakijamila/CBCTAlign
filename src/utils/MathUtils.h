/**
 * @file MathUtils.h
 * @brief Fonctions mathématiques utilitaires
 */
#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <Eigen/Dense>
#include <vector>
#include <cmath>

namespace CBCTAlign {
namespace MathUtils {

inline double degToRad(double deg) { return deg * M_PI / 180.0; }
inline double radToDeg(double rad) { return rad * 180.0 / M_PI; }

Eigen::Matrix3d eulerToRotation(double rx, double ry, double rz);
Eigen::Vector3d rotationToEuler(const Eigen::Matrix3d& R);
Eigen::Matrix4d makeTransform(const Eigen::Matrix3d& R, const Eigen::Vector3d& t);

/**
 * @brief Calcul du SSIM — source unique (utilisé par ValidationMetrics aussi)
 */
double computeSSIM(const std::vector<float>& img1, const std::vector<float>& img2,
                   int width, int height);

} // namespace MathUtils
} // namespace CBCTAlign
#endif
