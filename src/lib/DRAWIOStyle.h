/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef DRAWIOSTYLE_H
#define DRAWIOSTYLE_H

#include <boost/optional.hpp>
#include <boost/optional/optional.hpp>
#include "DRAWIOTypes.h"

namespace libdrawio {
  struct DRAWIOStyle {
    double opacity; // opacity in [0,100]
    bool orthogonal;
    boost::optional<double> entryX, entryY; // in [0,1]
    boost::optional<double> exitX, exitY; // same range as above
    double entryDx, entryDy, exitDx, exitDy;
    bool wrapText;
    double rotation; // rotation in [0,360]
    boost::optional<Color> fillColor = Color(255, 255, 255, 1);
    double margin;
    boost::optional<Color> gradientColor;
    Direction gradientDirection;
    boost::optional<Color> strokeColor = Color(0, 0, 0, 1);
    double strokeWidth;
    bool flipH, flipV;
    bool shadow;
    bool dashed;
    bool rounded;
    bool curved;
    boost::optional<Direction> portConstraint;
    boost::optional<Direction> sourcePortConstraint;
    boost::optional<Direction> targetPortConstraint;
    Shape shape = RECTANGLE;
    Perimeter perimeter = RECTANGLE_P;
    EdgeStyle edgeStyle = STRAIGHT;
    Direction direction = EAST;
    boost::optional<Direction> startDir;
    boost::optional<Direction> endDir;
    bool startFixed;
    bool endFixed;
    double calloutLength = 30;
    double calloutWidth = 20;
    double calloutPosition = 0.5;
    double calloutTipPosition = 0.5;
    double processBarSize = 0.1;
    double parallelogramSize = 20;
    double hexagonSize = 20;
    double stepSize = 20;
    double trapezoidSize = 20;
    double cardSize = 20;
    double storageX = 20;
    double storageY = 20;
    double documentSize = 0.3;
    double tapeSize = 0.4;
    double dataStorageSize = 20;
    bool fixedSize = true;
    boost::optional<MarkerType> startArrow;
    bool startFill = true;
    double startSize = 6;
    boost::optional<MarkerType> endArrow = CLASSIC;
    bool endFill = true;
    double endSize = 6;
    AlignH align = CENTER;
    AlignV verticalAlign = MIDDLE;
    AlignH position = CENTER;
    AlignV verticalPosition = MIDDLE;
  };

  struct DRAWIOTextStyle {
    librevenge::RVNGString fontFamily = "Helvetica";
    double fontSize = 12;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    boost::optional<Color> fontColor = Color(0, 0, 0, 1);
    boost::optional<Color> backgroundColor = boost::none;
    boost::optional<Color> borderColor = boost::none;
  };
}

#endif

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
