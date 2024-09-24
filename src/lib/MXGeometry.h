/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef MXGEOMETRY_H
#define MXGEOMETRY_H

#include <deque>
#include <cmath>

namespace libdrawio {
  struct MXPoint {
    double x, y;
    MXPoint() : x(), y() {}
    MXPoint(double x, double y) : x(x), y(y) {}
    MXPoint(const MXPoint &mxpoint) = default;
    MXPoint &operator=(const MXPoint &mxpoint) = default;
    inline bool operator==(const MXPoint other)
    {
      return x == other.x && y == other.y;
    }
    inline MXPoint rotate(MXPoint center, double angle)
    {
      double r = sqrt(pow(x - center.x, 2) + pow(y - center.y, 2));
      double old_angle = x != center.x ? 
        atan((y - center.y) / (x - center.x)) + (x > center.x ? 0 : pi) : 
        (y <= center.y ? -1 : 1) * pi / 2;
      return MXPoint(center.x + r*cos(old_angle - angle), 
                     center.y + r*sin(old_angle - angle));
    }
    friend MXPoint operator+(MXPoint p1, MXPoint p2) {
      return MXPoint(p1.x + p2.x, p1.y + p2.y);
    }
  };

  struct MXGeometry {
    double x, y, width, height;
    MXPoint sourcePoint, targetPoint;
    std::deque<MXPoint> points;
    double offset;
    bool relative;
    MXGeometry()
      : x(), y(), width(), height(), sourcePoint(), targetPoint(),
        points(), offset(), relative(false) {}
    MXGeometry(const MXGeometry &mxgeometry) = default;
    MXGeometry &operator=(const MXGeometry &mxgeometry) = default;
  };
}

#endif

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
