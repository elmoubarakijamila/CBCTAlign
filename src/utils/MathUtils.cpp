/**
 * @file MathUtils.cpp
 * @brief Fonctions mathématiques
 */
#include "MathUtils.h"

namespace CBCTAlign {
namespace MathUtils {

Eigen::Matrix3d eulerToRotation(double rx, double ry, double rz)
{
    Eigen::Matrix3d Rx, Ry, Rz;
    Rx << 1,0,0, 0,cos(rx),-sin(rx), 0,sin(rx),cos(rx);
    Ry << cos(ry),0,sin(ry), 0,1,0, -sin(ry),0,cos(ry);
    Rz << cos(rz),-sin(rz),0, sin(rz),cos(rz),0, 0,0,1;
    return Rz * Ry * Rx;
}

Eigen::Vector3d rotationToEuler(const Eigen::Matrix3d& R)
{
    double sy = sqrt(R(0,0)*R(0,0) + R(1,0)*R(1,0));
    bool singular = sy < 1e-6;

    double x, y, z;
    if (!singular) {
        x = atan2(R(2,1), R(2,2));
        y = atan2(-R(2,0), sy);
        z = atan2(R(1,0), R(0,0));
    } else {
        x = atan2(-R(1,2), R(1,1));
        y = atan2(-R(2,0), sy);
        z = 0;
    }
    return {x, y, z};
}

Eigen::Matrix4d makeTransform(const Eigen::Matrix3d& R, const Eigen::Vector3d& t)
{
    Eigen::Matrix4d T = Eigen::Matrix4d::Identity();
    T.block<3,3>(0,0) = R;
    T.block<3,1>(0,3) = t;
    return T;
}

double computeSSIM(const std::vector<float>& img1, const std::vector<float>& img2,
                   int width, int height)
{
    if (img1.size() != img2.size() || img1.empty()) return 0.0;

    const int n = width * height;
    const double C1 = 0.01 * 0.01 * 4000.0 * 4000.0;
    const double C2 = 0.03 * 0.03 * 4000.0 * 4000.0;

    double sum1 = 0, sum2 = 0;
    for (int i = 0; i < n; ++i) { sum1 += img1[i]; sum2 += img2[i]; }
    double m1 = sum1 / n, m2 = sum2 / n;

    double var1 = 0, var2 = 0, cov = 0;
    for (int i = 0; i < n; ++i) {
        double d1 = img1[i] - m1, d2 = img2[i] - m2;
        var1 += d1*d1; var2 += d2*d2; cov += d1*d2;
    }
    var1 /= (n-1); var2 /= (n-1); cov /= (n-1);

    return ((2*m1*m2 + C1)*(2*cov + C2)) / ((m1*m1 + m2*m2 + C1)*(var1 + var2 + C2));
}

} // namespace MathUtils
} // namespace CBCTAlign
