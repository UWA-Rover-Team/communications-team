
#include <cmath>
#include <stdexcept>
#include "trilateralisation.h"
#include <Arduino.h>
// #include <iostream>


    TrilaterationUtils::Point3D TrilaterationUtils::Trilaterate(const Point3D& p1, double r1, const Point3D& p2, double r2, const Point3D& p3, double r3) {
        double x1 = p1.x;
        double y1 = p1.y;
        double z1 = p1.z;

        double x2 = p2.x;
        double y2 = p2.y;
        double z2 = p2.z;

        double x3 = p3.x;
        double y3 = p3.y;
        double z3 = p3.z;

        double d = 2 * ((x2 - x1) * ((z3 - z1) * (y2 - y1) - (y3 - y1) * (z2 - z1)) -
                        (z2 - z1) * ((y3 - y1) * (x2 - x1) - (x3 - x1) * (y2 - y1)) +
                        (y2 - y1) * ((x3 - x1) * (z2 - z1) - (z3 - z1) * (x2 - x1)));

        if (std::abs(d) < 1e-6) {
            throw std::invalid_argument("The three points are collinear, trilateration is not possible.");
        }

        double r1Square = r1 * r1;
        double r2Square = r2 * r2;
        double r3Square = r3 * r3;

        double a = (r1Square - r2Square + x2 * x2 - x1 * x1 + y2 * y2 - y1 * y1 + z2 * z2 - z1 * z1) / d;
        double b = (r1Square - r3Square + x3 * x3 - x1 * x1 + y3 * y3 - y1 * y1 + z3 * z3 - z1 * z1) / d;

        double x = x1 + a * (x2 - x1) + b * (x3 - x1);
        double y = y1 + a * (y2 - y1) + b * (y3 - y1);
        double z = z1 + a * (z2 - z1) + b * (z3 - z1);

        return {x, y, z};
    }
int main() {
    // Example: Trilateration in 3D space
    // Given three points in 3D space and their respective distances from an unknown point,
    // we can use the `Trilaterate` function to estimate the coordinates of the unknown point.
    {
        TrilaterationUtils::Point3D p1 = {0.0, 0.0, 0.0};
        double r1 = 5.0;

        TrilaterationUtils::Point3D p2 = {10.0, 0.0, 0.0};
        double r2 = 7.0;

        TrilaterationUtils::Point3D p3 = {5.0, 10.0, 0.0};
        double r3 = 8.0;

        try {
            TrilaterationUtils::Point3D estimatedPoint = TrilaterationUtils::Trilaterate(p1, r1, p2, r2, p3, r3);
            // std::cout << "Estimated coordinates of the unknown point: (" << estimatedPoint.x << ", "
                    //   << estimatedPoint.y << ", " << estimatedPoint.z << ")\n";
        } catch (const std::exception& e) {
            // std::cout << "Error: " << e.what() << "\n";
        }
    }

    return 0;
}
                    