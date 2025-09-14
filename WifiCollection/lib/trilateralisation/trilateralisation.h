#ifndef TRILATERALISATION_H
#define TRILATERALISATION_H

#include <cmath>
#include <stdexcept>
#include <Arduino.h>


namespace TrilaterationUtils {
    struct Point3D {
        double x;
        double y;
        double z;
    };

    Point3D Trilaterate(const Point3D& p1, double r1, const Point3D& p2, double r2, const Point3D& p3, double r3);
}
#endif 