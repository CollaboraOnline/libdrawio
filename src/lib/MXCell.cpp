/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "MXCell.h"
#include "DRAWIOTypes.h"
#include "MXGeometry.h"
#include "libdrawio_xml.h"
#include "librevenge/RVNGPropertyList.h"
#include "librevenge/RVNGPropertyListVector.h"
#include "librevenge/RVNGString.h"
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/none.hpp>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <ostream>
#include <string>
#include <sstream>
#include <set>
#include <vector>
#include <deque>
#include <iostream>
#include <librevenge/librevenge.h>

namespace libdrawio {
  struct PathContext {
    librevenge::RVNGPropertyListVector path;

    PathContext(MXCell& cell)
    {
      origin = MXPoint(cell.geometry.x/100, cell.geometry.y/100);
      width = cell.geometry.width/100; height = cell.geometry.height/100;
      center = MXPoint(width/2, height/2);
      direction = cell.style.direction;
      angle = -cell.style.rotation * pi / 180;
      if (vertical(direction)) {
        origin.x += (width - height) / 2;
        origin.y += (height - width) / 2;
        double t = width; width = height; height = t;
        t = center.x; center.x = center.y; center.y = t;
      }
      if (direction == SOUTH) angle -= pi/2;
      if (direction == WEST) angle -= pi;
      if (direction == NORTH) angle += pi/2;
    }

    void moveTo(double x, double y)
    {
      addStep("M", {MXPoint(x, y)});
    }

    void lineTo(double x, double y)
    {
      addStep("L", {MXPoint(x, y)});
    }

    void quadTo(double x1, double y1, double x, double y)
    {
      addStep("Q", {MXPoint(x, y), MXPoint(x1, y1)});
    }

    void curveTo(double x1, double y1, double x2, double y2, double x, double y)
    {
      addStep("C", {MXPoint(x, y), MXPoint(x1, y1), MXPoint(x2, y2)});
    }

    void close()
    {
      addStep("Z", {});
    }

    void addPoints(const std::vector<MXPoint>& points, bool close_path)
    {
      if (points.empty()) return;
      MXPoint p = points[0];
      moveTo(p.x, p.y);
      int i = 1;
      while (i < points.size() - (close_path ? 0 : 1)) {
        p = points[i];
        lineTo(p.x, p.y);
        i++;
      }
      if (close_path) close();
      else {
        p = points[i];
        lineTo(p.x, p.y);
      }
    }

    MXPoint center, origin;
    double width, height, angle;
    Direction direction;

  private:
    void addStep(const char* action, std::vector<MXPoint>&& points)
    {
      for (auto& point : points) {
        point = origin + point.rotate(center, angle);
      }
      librevenge::RVNGPropertyList step;
      step.insert("librevenge:path-action", action);
      if (points.size() >= 1) {
        step.insert("svg:x", points[0].x);
        step.insert("svg:y", points[0].y);
      } if (points.size() >= 2) {
        step.insert("svg:x1", points[1].x);
        step.insert("svg:y1", points[1].y);
      } if (points.size() >= 3) {
        step.insert("svg:x2", points[2].x);
        step.insert("svg:y2", points[2].y);
      }
      path.append(step);
    }
  };
  
  int MXCell::draw_count = 0;

  void MXCell::draw(librevenge::RVNGDrawingInterface *painter,
                    std::map<librevenge::RVNGString, MXCell> id_map) {
    librevenge::RVNGPropertyList propList;
    if (!id.empty()) {
      propList.insert("draw:id", id);
      propList.insert("xml:id", id);
    }
    std::string style_name = "gr_" + std::to_string(draw_count);
    librevenge::RVNGPropertyList styleProps = getStyle();
    styleProps.insert("style:display-name", style_name.c_str());
    propList.insert("draw:style-name", style_name.c_str());
    painter->setStyle(styleProps);

    painter->openGroup(librevenge::RVNGPropertyList());

    if (edge) {
      setEndPoints(id_map);
      setWaypoints(id_map);
      calculateBounds();
      if (!source_id.empty()) {
        propList.insert("draw:start-shape", source_id);
      }
      if (!target_id.empty()) {
        propList.insert("draw:end-shape", target_id);
      }

      propList.insert("svg:x1", geometry.sourcePoint.x / 100.);
      propList.insert("svg:y1", geometry.sourcePoint.y / 100.);
      propList.insert("svg:x2", geometry.targetPoint.x / 100.);
      propList.insert("svg:y2", geometry.targetPoint.y / 100.);
      
      propList.insert("svg:d", getPath());

      painter->drawConnector(propList);
    }
    else if (vertex) {
      if (!parent_id.empty())
      {
        MXCell parent = id_map[parent_id];
        geometry.x += parent.geometry.x;
        geometry.y += parent.geometry.y;
      }

      double rx = geometry.width / 200.; double ry = geometry.height / 200.;
      double cx = geometry.x / 100. + rx; double cy = geometry.y / 100. + ry;
      PathContext c(*this);
      double w = c.width; double h = c.height;
      double angle = -style.rotation * boost::math::double_constants::pi / 180;
      if (style.shape == RECTANGLE) {
        propList.insert("svg:x", geometry.x / 100.);
        propList.insert("svg:y", geometry.y / 100.);
        propList.insert("svg:width", geometry.width / 100.);
        propList.insert("svg:height", geometry.height / 100.);
        double dx = sqrt(pow(rx, 2.) + pow(ry, 2.))*cos(atan(ry/rx)-angle) - rx;
        double dy = sqrt(pow(rx, 2.) + pow(ry, 2.))*sin(atan(ry/rx)-angle) - ry;
        librevenge::RVNGString sValue = "translate(";
        sValue.append(std::to_string(-geometry.x / 100.).c_str());
        sValue.append("in,");
        sValue.append(std::to_string(-geometry.y / 100.).c_str());
        sValue.append("in) rotate(");
        sValue.append(std::to_string(angle).c_str());
        sValue.append(") translate(");
        sValue.append(std::to_string(cx - rx - dx).c_str());
        sValue.append("in,");
        sValue.append(std::to_string(cy - ry - dy).c_str());
        sValue.append("in)");
        propList.insert("draw:transform", sValue);
        painter->drawRectangle(propList);
      }
      else if (style.shape == ELLIPSE) {
        propList.insert("svg:rx", rx);
        propList.insert("svg:ry", ry);
        propList.insert("svg:cx", cx);
        propList.insert("svg:cy", cy);
        propList.insert("librevenge:rotate", -style.rotation);
        painter->drawEllipse(propList);
      }
      else if (style.shape == TRIANGLE) {
        c.addPoints({MXPoint(0,0), MXPoint(w,h/2), MXPoint(0,h)}, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == CALLOUT) {
        double s = std::max(0., style.calloutLength/100);
        double dx = w * std::max(0., std::min(1., style.calloutPosition));
        double dx2 = w * std::max(0., std::min(1., style.calloutTipPosition));
        double base = std::max(0., style.calloutWidth/100);
        c.addPoints({
          MXPoint(0, 0), MXPoint(w, 0), MXPoint(w, h-s), MXPoint(dx+base, h-s), 
          MXPoint(dx2, h), MXPoint(dx, h-s), MXPoint(0, h-s)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == PROCESS) {
        double inset = w * std::max(0., std::min(1., style.processBarSize));
        c.moveTo(inset, 0);
        c.lineTo(inset, h);
        c.close();
        c.moveTo(w-inset, 0);
        c.lineTo(w-inset, h);
        c.close();
        c.addPoints({
          MXPoint(0, 0), MXPoint(w, 0), MXPoint(w, h), MXPoint(0, h)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == RHOMBUS) {
        c.addPoints({
          MXPoint(w/2, 0), MXPoint(w, h/2), MXPoint(w/2, h), MXPoint(0, h/2)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == PARALLELOGRAM) {
        double dx = style.parallelogramSize/100;
        c.addPoints({
          MXPoint(0, h), MXPoint(dx, 0), MXPoint(w, 0), MXPoint(w-dx, h)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == HEXAGON) {
        double s = style.hexagonSize/100;
        c.addPoints({
          MXPoint(s, 0), MXPoint(w-s, 0), MXPoint(w, h/2), 
          MXPoint(w-s, h), MXPoint(s, h), MXPoint(0, h/2)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == STEP) {
        double s = style.stepSize/100;
        c.addPoints({
          MXPoint(0, 0), MXPoint(w-s, 0), MXPoint(w, h/2), 
          MXPoint(w-s, h), MXPoint(0, h), MXPoint(s, h/2)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == TRAPEZOID) {
        double dx = style.trapezoidSize/100;
        c.addPoints({
          MXPoint(0, h), MXPoint(dx, 0), MXPoint(w-dx, 0), MXPoint(w, h)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == CARD) {
        double s = style.cardSize/100;
        c.addPoints({
          MXPoint(s, 0), MXPoint(w, 0), MXPoint(w, h), 
          MXPoint(0, h), MXPoint(0, s)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == INTERNAL_STORAGE) {
        double dx = style.storageX/100; double dy = style.storageY/100;
        c.moveTo(0, dy);
        c.lineTo(w, dy);
        c.close();
        c.moveTo(dx, 0);
        c.lineTo(dx, h);
        c.close();
        c.addPoints({
          MXPoint(0, 0), MXPoint(w, 0), MXPoint(w, h), MXPoint(0, h)
        }, true);
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == OR) {
        c.moveTo(0, 0);
        c.quadTo(w, 0, w, h/2);
        c.quadTo(w, h, 0, h);
        c.close();
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == XOR) {
        c.moveTo(0, 0);
        c.quadTo(w, 0, w, h/2);
        c.quadTo(w, h, 0, h);
        c.quadTo(w/2, h/2, 0, 0);
        c.close();
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == DOCUMENT) {
        double dy = h * style.documentSize;
        double fy = 1.4;
        c.moveTo(0, 0);
        c.lineTo(w, 0);
        c.lineTo(w, h-dy/2);
        c.quadTo(w*3/4, h-dy*fy, w/2, h-dy/2);
        c.quadTo(w/4, h-dy*(1-fy), 0, h-dy/2);
        c.close();
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == TAPE) {
        double dy = h * style.tapeSize;
        double fy = 1.4;
        c.moveTo(0, dy/2);
        c.quadTo(w/4, dy*fy, w/2, dy/2);
        c.quadTo(w*3/4, dy*(1-fy), w, dy/2);
        c.lineTo(w, h-dy/2);
        c.quadTo(w*3/4, h-dy*fy, w/2, h-dy/2);
        c.quadTo(w/4, h-dy*(1-fy), 0, h-dy/2);
        c.close();
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
      else if (style.shape == DATA_STORAGE) {
        double s = style.dataStorageSize/100;
        c.moveTo(s, 0);
        c.lineTo(w, 0);
        c.quadTo(w-2*s, h/2, w, h);
        c.lineTo(s, h);
        c.quadTo(s-2*s, h/2, s, 0);
        c.close();
        propList.insert("svg:d", c.path);
        painter->drawPath(propList);
      }
    }

    propList.clear();
    propList.insert("librevenge:span-id", draw_count);
    propList.insert("svg:x", (geometry.x + (int)style.position*geometry.width) / 100.);
    propList.insert("svg:y", (geometry.y + (int)style.verticalPosition*geometry.height) / 100.);
    propList.insert("svg:width", geometry.width / 100.);
    propList.insert("svg:height", geometry.height / 100.);
    librevenge::RVNGPropertyList textStyleProps = getTextStyle();
    textStyleProps.insert("librevenge:span-id", draw_count);
    propList.insert("fo:text-align", to_string(style.align).c_str());
    propList.insert("draw:textarea-vertical-align", to_string(style.verticalAlign).c_str());
    painter->setStyle(styleProps);
    painter->startTextObject(propList);
    if (!data.label.empty()) {
      painter->defineCharacterStyle(textStyleProps);
      painter->openParagraph(propList);
      painter->openSpan(propList);
      painter->insertText(processText(data.label));
      painter->closeSpan();
      painter->closeParagraph();
    }
    painter->endTextObject();
    painter->closeGroup();
    draw_count++;
  }

  void MXCell::calculateBounds() {
    if (vertex)
      bounds = {0, 0, 21600, 21600};
    else {
      int x = (int)std::min(geometry.sourcePoint.x, geometry.targetPoint.x);
      int y = (int)std::min(geometry.sourcePoint.y, geometry.targetPoint.y);
      int width =
        std::abs((int)(geometry.sourcePoint.x - geometry.targetPoint.x));
      int height =
        std::abs((int)(geometry.sourcePoint.y - geometry.targetPoint.y));
      bounds = {x, y, width, height};
    }
  }

  std::string MXCell::getViewBox() {
    std::ostringstream out;
    out << bounds.x << ' ' << bounds.y << ' '
        << bounds.width << ' ' << bounds.height;
    return out.str();
  }

  librevenge::RVNGPropertyListVector MXCell::getPath() {
    librevenge::RVNGPropertyListVector out;
    librevenge::RVNGPropertyList i;
    if (style.edgeStyle == STRAIGHT) {
      i.insert("librevenge:path-action", "M");
      i.insert("svg:x", geometry.sourcePoint.x / 100.);
      i.insert("svg:y", geometry.sourcePoint.y / 100.);
      out.append(i); i.clear();
      for (unsigned int j = 0; j < geometry.points.size(); j++) {
        MXPoint p = geometry.points.at(j);
        i.insert("librevenge:path-action", "L");
        i.insert("svg:x", p.x / 100.);
        i.insert("svg:y", p.y / 100.);
        out.append(i); i.clear();
      }
      i.insert("librevenge:path-action", "L");
      i.insert("svg:x", geometry.targetPoint.x / 100.);
      i.insert("svg:y", geometry.targetPoint.y / 100.);
      out.append(i);
    }
    else if (style.edgeStyle == ORTHOGONAL) {
      i.insert("librevenge:path-action", "M");
      i.insert("svg:x", geometry.sourcePoint.x / 100.);
      i.insert("svg:y", geometry.sourcePoint.y / 100.);
      out.append(i); i.clear();
      for (unsigned int j = 0; j < geometry.points.size(); j++) {
        MXPoint p = geometry.points.at(j);
        i.insert("librevenge:path-action", "L");
        i.insert("svg:x", p.x / 100.);
        i.insert("svg:y", p.y / 100.);
        out.append(i); i.clear();
      }
      i.insert("librevenge:path-action", "L");
      i.insert("svg:x", geometry.targetPoint.x / 100.);
      i.insert("svg:y", geometry.targetPoint.y / 100.);
      out.append(i); i.clear();
    }
    return out;
  }

  librevenge::RVNGPropertyList MXCell::getPoint(
    double x, double y, double cx, double cy, double angle
  )
  {
    librevenge::RVNGPropertyList point;
    double r = sqrt(pow(x - cx, 2) + pow(y - cy, 2));
    double old_angle = x != cx ?
      atan((y-cy) / (x-cx)) + (x > cx ? 0 : boost::math::double_constants::pi) :
      (y <= cy ? -1 : 1) * boost::math::double_constants::pi / 2;
    point.insert("svg:x", cx + r*cos(old_angle - angle));
    point.insert("svg:y", cy + r*sin(old_angle - angle));
    return point;
  }

  void MXCell::setStyle() {
    std::string style_s = style_str.cstr();
    std::vector<std::string> tokens;
    boost::algorithm::split(tokens, style_s, boost::is_any_of(";"));
    std::map<std::string, std::string> style_m;
    for (auto t : tokens) {
      auto n = t.find('=');
      style_m[t.substr(0, n)] = n <= t.size() ? t.substr(n+1) : "";
    }
    auto it = style_m.find("entryX"); if (it != style_m.end())
      style.entryX = std::stod(it->second);
    it = style_m.find("entryY"); if (it != style_m.end())
      style.entryY = std::stod(it->second);
    it = style_m.find("exitX"); if (it != style_m.end())
      style.exitX = std::stod(it->second);
    it = style_m.find("exitY"); if (it != style_m.end())
      style.exitY = std::stod(it->second);
    it = style_m.find("entryDx"); if (it != style_m.end())
      style.entryDx = std::stod(it->second);
    it = style_m.find("entryDy"); if (it != style_m.end())
      style.entryDy = std::stod(it->second);
    it = style_m.find("exitDx"); if (it != style_m.end())
      style.exitDx = std::stod(it->second);
    it = style_m.find("exitDy"); if (it != style_m.end())
      style.exitDy = std::stod(it->second);
    style.startFixed =
      (style.exitX.has_value() && style.exitY.has_value()) || source_id.empty();
    style.endFixed =
      (style.entryX.has_value() && style.entryY.has_value()) || target_id.empty();
    it = style_m.find("sourcePortConstraint"); if (it != style_m.end()) {
      if (it->second == "north") style.sourcePortConstraint = NORTH;
      else if (it->second == "east") style.sourcePortConstraint = EAST;
      else if (it->second == "south") style.sourcePortConstraint = SOUTH;
      else if (it->second == "west") style.sourcePortConstraint = WEST;
    }
    it = style_m.find("targetPortConstraint"); if (it != style_m.end()) {
      if (it->second == "north") style.targetPortConstraint = NORTH;
      else if (it->second == "east") style.targetPortConstraint = EAST;
      else if (it->second == "south") style.targetPortConstraint = SOUTH;
      else if (it->second == "west") style.targetPortConstraint = WEST;
    }
    it = style_m.find("PortConstraint"); if (it != style_m.end()) {
      if (it->second == "north") style.portConstraint = NORTH;
      else if (it->second == "east") style.portConstraint = EAST;
      else if (it->second == "south") style.portConstraint = SOUTH;
      else if (it->second == "west") style.portConstraint = WEST;
    }
    it = style_m.find("ellipse"); if (it != style_m.end()) {
      style.shape = ELLIPSE;
      style.perimeter = ELLIPSE_P;
    }
    it = style_m.find("triangle"); if (it != style_m.end()) {
      style.shape = TRIANGLE;
      style.perimeter = TRIANGLE_P;
    }
    it = style_m.find("rhombus"); if (it != style_m.end()) {
      style.shape = RHOMBUS;
      style.perimeter = RHOMBUS_P;
    }
    it = style_m.find("shape"); if (it != style_m.end()) {
      if (it->second == "callout") style.shape = CALLOUT;
      else if (it->second == "process") style.shape = PROCESS;
      else if (it->second == "parallelogram") style.shape = PARALLELOGRAM;
      else if (it->second == "hexagon") style.shape = HEXAGON;
      else if (it->second == "step") style.shape = STEP;
      else if (it->second == "trapezoid") style.shape = TRAPEZOID;
      else if (it->second == "card") style.shape = CARD;
      else if (it->second == "internalStorage") style.shape = INTERNAL_STORAGE;
      else if (it->second == "or") style.shape = OR;
      else if (it->second == "xor") style.shape = XOR;
      else if (it->second == "document") style.shape = DOCUMENT;
      else if (it->second == "tape") style.shape = TAPE;
      else if (it->second == "dataStorage") style.shape = DATA_STORAGE;
    }
    it = style_m.find("perimeter"); if (it != style_m.end()) {
      if (it->second == "rectanglePerimeter") style.perimeter = RECTANGLE_P;
      else if (it->second == "ellipsePerimeter") style.perimeter = ELLIPSE_P;
      else if (it->second == "trianglePerimeter") style.perimeter = TRIANGLE_P;
      else if (it->second == "calloutPerimeter") style.perimeter = CALLOUT_P;
      else if (it->second == "rhombusPerimeter") style.perimeter = RHOMBUS_P;
      else if (it->second == "parallelogramPerimeter") style.perimeter = PARALLELOGRAM_P;
      else if (it->second == "hexagonPerimeter2") style.perimeter = HEXAGON_P;
      else if (it->second == "stepPerimeter") style.perimeter = STEP_P;
      else if (it->second == "trapezoidPerimeter") style.perimeter = TRAPEZOID_P;
    }
    it = style_m.find("direction"); if (it != style_m.end()) {
      if (it->second == "north") style.direction = NORTH;
      else if (it->second == "east") style.direction = EAST;
      else if (it->second == "south") style.direction = SOUTH;
      else if (it->second == "west") style.direction = WEST;
    }
    it = style_m.find("fixedSize"); if (it != style_m.end()) {
      std::istringstream(it->second) >> style.fixedSize;
    }
    it = style_m.find("size"); if (it != style_m.end()) {
      if (style.shape == CALLOUT) style.calloutLength = std::stod(it->second);
      else if (style.shape == PROCESS) style.processBarSize = std::stod(it->second);
      else if (style.shape == PARALLELOGRAM)
        style.parallelogramSize = std::stod(it->second);
      else if (style.shape == HEXAGON) style.hexagonSize = std::stod(it->second);
      else if (style.shape == STEP) {
        style.stepSize = std::stod(it->second);
        if (!style.fixedSize)
          style.stepSize *=
            (style.direction == NORTH || style.direction == SOUTH
             ? geometry.height : geometry.width);
      }
      else if (style.shape == TRAPEZOID) style.trapezoidSize = std::stod(it->second);
      else if (style.shape == CARD) style.cardSize = std::stod(it->second);
      else if (style.shape == DOCUMENT) style.documentSize = std::stod(it->second);
      else if (style.shape == TAPE) style.tapeSize = std::stod(it->second);
      else if (style.shape == DATA_STORAGE) style.dataStorageSize = std::stod(it->second);
    }
    it = style_m.find("base"); if (it != style_m.end()) {
      if (style.shape == CALLOUT) style.calloutWidth = std::stod(it->second);
    }
    it = style_m.find("position"); if (it != style_m.end()) {
      if (style.shape == CALLOUT) style.calloutPosition = std::stod(it->second);
    }
    it = style_m.find("position2"); if (it != style_m.end()) {
      if (style.shape == CALLOUT) style.calloutTipPosition = std::stod(it->second);
    }
    it = style_m.find("dx"); if (it != style_m.end()) {
      if (style.shape == INTERNAL_STORAGE) style.storageX = std::stod(it->second);
    }
    it = style_m.find("dy"); if (it != style_m.end()) {
      if (style.shape == INTERNAL_STORAGE) style.storageY = std::stod(it->second);
    }
    it = style_m.find("fillColor"); if (it != style_m.end()) {
      if (it->second == "none") style.fillColor = boost::none;
      else if (it->second == "default");
      else style.fillColor = xmlStringToColor((xmlChar*)(it->second.c_str()));
    }
    it = style_m.find("strokeColor"); if (it != style_m.end()) {
      if (it->second == "none") style.strokeColor = boost::none;
      else if (it->second == "default");
      else style.strokeColor = xmlStringToColor((xmlChar*)(it->second.c_str()));
    }
    it = style_m.find("startArrow"); if (it != style_m.end()) {
      if (it->second == "none") style.startArrow = boost::none;
      else if (it->second == "classic") style.startArrow = CLASSIC;
    }
    it = style_m.find("startFill"); if (it != style_m.end())
      style.startFill = xmlStringToBool((xmlChar*)(it->second.c_str()));
    it = style_m.find("startSize"); if (it != style_m.end())
      style.startSize = xmlStringToDouble((xmlChar*)(it->second.c_str()));
    it = style_m.find("endArrow"); if (it != style_m.end()) {
      if (it->second == "none") style.endArrow = boost::none;
      else if (it->second == "classic") style.endArrow = CLASSIC;
    }
    it = style_m.find("endFill"); if (it != style_m.end())
      style.endFill = xmlStringToBool((xmlChar*)(it->second.c_str()));
    it = style_m.find("endSize"); if (it != style_m.end())
      style.endSize = xmlStringToDouble((xmlChar*)(it->second.c_str()));
    it = style_m.find("rotation"); if (it != style_m.end())
      style.rotation = xmlStringToDouble((xmlChar*)(it->second.c_str()));
    it = style_m.find("edgeStyle"); if (it != style_m.end()) {
      if (it->second == "orthogonalEdgeStyle") style.edgeStyle = ORTHOGONAL;
    }
    it = style_m.find("fontFamily"); if (it != style_m.end())
      text_style.fontFamily = it->second.c_str();
    it = style_m.find("fontSize"); if (it != style_m.end())
      text_style.fontSize = xmlStringToDouble((xmlChar*)(it->second.c_str()));
    it = style_m.find("fontStyle"); if (it != style_m.end()) {
      int style = xmlStringToDouble((xmlChar*)(it->second.c_str()));
      text_style.bold = style & 1;
      text_style.italic = style & 2;
      text_style.underline = style & 4;
    }
    it = style_m.find("fontColor"); if (it != style_m.end()) {
      if (it->second == "none") text_style.fontColor = boost::none;
      else if (it->second == "default"); 
      else text_style.fontColor = xmlStringToColor((xmlChar*)(it->second.c_str()));
    }
    it = style_m.find("labelBackgroundColor"); if (it != style_m.end()) {
      if (it->second == "none") text_style.backgroundColor = boost::none;
      else if (it->second == "default");
      else text_style.backgroundColor = xmlStringToColor((xmlChar *)(it->second.c_str()));
    }
    it = style_m.find("labelBorderColor"); if (it != style_m.end()) {
      if (it->second == "none") text_style.borderColor = boost::none;
      else if (it->second == "default");
      else text_style.borderColor = xmlStringToColor((xmlChar *)(it->second.c_str()));
    }
    it = style_m.find("align"); if (it != style_m.end()) {
      if (it->second == "left") style.align = LEFT;
      else if (it->second == "center") style.align = CENTER;
      else if (it->second == "right") style.align = RIGHT;
    }
    it = style_m.find("verticalAlign"); if (it != style_m.end()) {
      if (it->second == "top") style.verticalAlign = TOP;
      else if (it->second == "middle") style.verticalAlign = MIDDLE;
      else if (it->second == "bottom") style.verticalAlign = BOTTOM;
    }
    it = style_m.find("labelPosition"); if (it != style_m.end()) {
      if (it->second == "left") style.position = LEFT;
      else if (it->second == "center") style.position = CENTER;
      else if (it->second == "right") style.position = RIGHT;
    }
    it = style_m.find("verticalLabelPosition"); if (it != style_m.end()) {
      if (it->second == "top") style.verticalPosition = TOP;
      else if (it->second == "middle") style.verticalPosition = MIDDLE;
      else if (it->second == "bottom") style.verticalPosition = BOTTOM;
    }
  }

  void MXCell::setEndPoints(std::map<librevenge::RVNGString, MXCell> id_map) {
    // calculates endpoints for an edge
    // necessary because draw.io doesn't store endpoint coordinates
    // if the edge is attached to a vertex.
    if (!edge) return;
    if (!source_id.empty() && style.startFixed) {
      MXCell source = id_map[source_id];
      setEndpointInShape(style.exitX.get(), style.exitY.get(), source,
                         geometry.sourcePoint, style.exitDx, style.exitDy);
    }
    if (!target_id.empty() && style.endFixed) {
      MXCell target = id_map[target_id];
      setEndpointInShape(style.entryX.get(), style.entryY.get(), target,
                         geometry.targetPoint, style.entryDx, style.entryDy);
    }
    if (style.edgeStyle == STRAIGHT) {
      if (style.startFixed && style.endFixed) return;
      double startX, startY, endX, endY;
      if (style.startFixed) {
        startX = geometry.sourcePoint.x; startY = geometry.sourcePoint.y;
      } else {
        MXCell source = id_map[source_id];
        startX = source.geometry.x + source.geometry.width / 2;
        startY = source.geometry.y + source.geometry.height / 2;
      }
      if (style.endFixed) {
        endX = geometry.targetPoint.x; endY = geometry.targetPoint.y;
      } else {
        MXCell target = id_map[target_id];
        endX = target.geometry.x + target.geometry.width / 2;
        endY = target.geometry.y + target.geometry.height / 2;
      }
      if (!style.startFixed) {
        MXCell source = id_map[source_id];
        double inX, inY;
        if (geometry.points.empty()) {
          inX = endX; inY = endY;
        } else {
          inX = geometry.points[0].x; inY = geometry.points[0].y;
        }
        double angle = std::atan2(inY - startY, inX - startX);
        double facing_angle =
          boost::math::double_constants::pi * ((int)source.style.direction - 1) / 2;
        angle -= facing_angle;
        angle -= source.style.rotation * boost::math::double_constants::pi / 180;
        double m = std::tan(angle);
        double outX, outY;
        if (std::abs(m) > 1) {
          outY = inY < startY ? 0  : 1;
          outX = 0.5 + (inY < startY ? -0.5 : 0.5) / m;
        } else {
          outX = inX < startX ? 0 : 1;
          outY = 0.5 + m * (inX < startX ? -0.5 : 0.5);
        }
        setEndpointInShape(outX, outY, source, geometry.sourcePoint);
      }
      if (!style.endFixed) {
        MXCell target = id_map[target_id];
        double inX, inY;
        if (geometry.points.empty()) {
          inX = startX; inY = startY;
        } else {
          inX = geometry.points.back().x; inY = geometry.points.back().y;
        }
        double angle = std::atan2(endY - inY, endX - inX);
        double facing_angle =
          boost::math::double_constants::pi * ((int)target.style.direction - 1) / 2;
        angle += boost::math::double_constants::pi;
        angle -= facing_angle;
        angle -= target.style.rotation * boost::math::double_constants::pi / 180;
        double m = std::tan(angle);
        double outX, outY;
        if (std::abs(m) > 1) {
          outY = endY < inY ? 1  : 0;
          outX = 0.5 + (endY < inY ? 0.5 : -0.5) / m;
        } else {
          outX = endX < inX ? 1 : 0;
          outY = 0.5 + m * (endX < inX ? 0.5 : -0.5);
        }
        setEndpointInShape(outX, outY, target, geometry.targetPoint);
      }
    }
    else if (style.edgeStyle == ORTHOGONAL) {
      bool source_shape = !source_id.empty(); bool target_shape = !target_id.empty();
      double startX, startY, startWidth, startHeight, endX, endY, endWidth, endHeight;
      if (source_shape) {
        MXCell source = id_map[source_id];
        startX = source.geometry.x; startY = source.geometry.y;
        startWidth = source.geometry.width; startHeight = source.geometry.height;
      } else {
        startX = geometry.sourcePoint.x; startY = geometry.sourcePoint.y;
        startWidth = 0; startHeight = 0;
      }
      if (target_shape) {
        MXCell target = id_map[target_id];
        endX = target.geometry.x; endY = target.geometry.y;
        endWidth = target.geometry.width; endHeight = target.geometry.height;
      } else {
        endX = geometry.targetPoint.x; endY = geometry.targetPoint.y;
        endWidth = 0; endHeight = 0;
      }
      style.startDir = (endX + endWidth < startX   ? WEST  :
                        endX > startX + startWidth ? EAST  :
                        endY <= startY             ? NORTH :
                                                     SOUTH);
      style.endDir = (endY + endHeight < startY   ? SOUTH :
                      endY > startY + startHeight ? NORTH :
                      endX + endWidth < startX    ? EAST  :
                      endX > startX + startWidth  ? WEST  :
                      endX > startX              ? style.startDir :
                      style.startDir == NORTH     ? SOUTH :
                                                    NORTH);
      if (style.startFixed) {
        startX = geometry.sourcePoint.x; startY = geometry.sourcePoint.y;
        startWidth = 0; startHeight = 0;
      }
      if (style.endFixed) {
        endX = geometry.targetPoint.x; endY = geometry.targetPoint.y;
        endWidth = 0; endHeight = 0;
      }
      if (!geometry.points.empty()) {
        int size = geometry.points.size();
        MXPoint p = geometry.points.front();
        MXPoint q = geometry.points.back();
        bool new_start =
          (p.x < startX || p.x > startX + startWidth)
          && (p.y < startY || p.y > startY + startHeight);
        bool new_end =
          (q.x < endX || q.x > endX + endWidth)
          && (q.y < endY || q.y > endY + endHeight);
        if (!new_start) {
          style.startDir = (p.x < startX ? WEST :
                            p.y < startY ? NORTH :
                            p.x > startX + startWidth ? EAST : SOUTH);
        }
        if (!new_end) {
          style.endDir = (q.x < endX ? WEST :
                          q.y < endY ? NORTH :
                          q.x > endX + endWidth ? EAST : SOUTH);
        }
        if (new_start) {
          if ((!new_end
               && ((style.endDir == WEST || style.endDir == EAST)
                   == size % 2))) {
            style.startDir = (p.x < startX ? WEST : EAST);
            geometry.points.push_front(MXPoint(p.x, startY + startHeight / 2));
          } else {
            style.startDir = (p.y < startY ? NORTH : SOUTH);
            geometry.points.push_front(MXPoint(startX + startWidth / 2, p.y));
          }
          if (new_end) {
            if (size % 2 == 0) {
              style.endDir = (q.y < endY ? NORTH : SOUTH);
              geometry.points.push_back(MXPoint(endX + endWidth / 2, q.y));
            } else {
              style.endDir = (q.x < endX ? WEST : EAST);
              geometry.points.push_back(MXPoint(q.x, endY + endHeight / 2));
            }
          }
        } else {
          if (new_end || size == 1) {
            if (size % 2
                == (style.startDir == EAST || style.startDir == WEST)) {
              style.endDir = (q.x < endX ? WEST : EAST);
              geometry.points.push_back(MXPoint(q.x, endY + endHeight / 2));
            } else {
              style.endDir = (q.y < endY ? NORTH : SOUTH);
              geometry.points.push_back(MXPoint(endX + endWidth / 2, q.y));
            }
          }
        }
      }
      if (!style.startFixed) {
        MXCell source = id_map[source_id];
        double angle =
          boost::math::double_constants::pi * ((int)style.startDir.get() - 1) / 2;
        double facing_angle =
          boost::math::double_constants::pi * ((int)source.style.direction - 1) / 2;
        angle -= facing_angle;
        angle -= source.style.rotation * boost::math::double_constants::pi / 180;
        double m = std::tan(angle);
        double outX, outY;
        if (std::abs(m) > 1) {
          outY = style.startDir == NORTH ? 0 : 1;
          outX = 0.5 + (style.startDir == NORTH ? -0.5 : 0.5) / m;
        } else {
          outX = style.startDir == WEST ? 0 : 1;
          outY = 0.5 + m * (style.startDir == WEST ? -0.5 : 0.5);
        }
        setEndpointInShape(outX, outY, source, geometry.sourcePoint);
        if (source.style.rotation == 0 && !geometry.points.empty()) {
          MXPoint p = geometry.points.front();
          if (style.startDir == NORTH || style.startDir == SOUTH) {
            geometry.sourcePoint.x = p.x;
          } else {
            geometry.sourcePoint.y = p.y;
          }
        }
      } else if (!source_id.empty()) {
        MXCell source = id_map[source_id];
        if (std::fmod(source.style.rotation, 90) == 0) {
          double rx = source.geometry.width / 2;
          double ry = source.geometry.height / 2;
          double cx = source.geometry.x + rx;
          double cy = source.geometry.y + ry;
          MXPoint p = geometry.sourcePoint;
          if (std::fmod(std::floor(source.style.rotation / 90), 2) == 1) {
            double t = rx; rx = ry; ry = t;
          }
          if (p.y == cy + ry) {
            style.startDir = SOUTH;
          } else if (p.y == cy - ry) {
            style.startDir = NORTH;
          } else if (p.x == cx - rx) {
            style.startDir = WEST;
          } else if (p.x == cx + rx) {
            style.startDir = EAST;
          }
        }
      }
      if (!style.endFixed) {
        MXCell target = id_map[target_id];
        double angle =
          boost::math::double_constants::pi * ((int)style.endDir.get() - 1) / 2;
        double facing_angle =
          boost::math::double_constants::pi * ((int)target.style.direction - 1) / 2;
        angle -= facing_angle;
        angle -= target.style.rotation * boost::math::double_constants::pi / 180;
        double m = std::tan(angle);
        double outX, outY;
        if (std::abs(m) > 1) {
          outY = style.endDir == NORTH ? 0 : 1;
          outX = 0.5 + (style.endDir == NORTH ? -0.5 : 0.5) / m;
        } else {
          outX = style.endDir == WEST ? 0 : 1;
          outY = 0.5 + m * (style.endDir == WEST ? -0.5 : 0.5);
        }
        setEndpointInShape(outX, outY, target, geometry.targetPoint);
        if (target.style.rotation == 0 && !geometry.points.empty()) {
          MXPoint p = geometry.points.back();
          if (style.endDir == NORTH || style.endDir == SOUTH) {
            geometry.targetPoint.x = p.x;
          } else {
            geometry.targetPoint.y = p.y;
          }
        }
      } else if (!target_id.empty()) {
        MXCell target = id_map[target_id];
        if (std::fmod(target.style.rotation, 90) == 0) {
          double rx = target.geometry.width / 2;
          double ry = target.geometry.height / 2;
          double cx = target.geometry.x + rx;
          double cy = target.geometry.y + ry;
          MXPoint p = geometry.targetPoint;
          if (std::fmod(std::floor(target.style.rotation / 90), 2) == 1) {
            double t = rx; rx = ry; ry = t;
          }
          if (p.y == cy + ry) {
            style.endDir = SOUTH;
          } else if (p.y == cy - ry) {
            style.endDir = NORTH;
          } else if (p.x == cx - rx) {
            style.endDir = WEST;
          } else if (p.x == cx + rx) {
            style.endDir = EAST;
          }
        }
      }
    }

    if (!source_id.empty() && !id_map[source_id].parent_id.empty()) {
      MXCell parent = id_map[id_map[source_id].parent_id];
      geometry.sourcePoint.x += parent.geometry.x;
      geometry.sourcePoint.y += parent.geometry.y;
    }
    if (!target_id.empty() && !id_map[target_id].parent_id.empty()) {
      MXCell parent = id_map[id_map[target_id].parent_id];
      geometry.targetPoint.x += parent.geometry.x;
      geometry.targetPoint.y += parent.geometry.y;
    }
  }

  void MXCell::setEndpointInShape(double outX, double outY, const MXCell& shape,
                                  MXPoint& point, double dx, double dy)
  {
    bool perimeter = outX == 0 || outX == 1 || outY == 0 || outY == 1;
    switch (shape.style.direction) {
    case EAST:
    case WEST:
      outX += dx / shape.geometry.width; outY += dy / shape.geometry.height;
      break;
    case NORTH:
    case SOUTH:
      outX += dx / shape.geometry.height; outY += dy / shape.geometry.width;
      break;
    }
    if (perimeter) adjustEndpoint(outX, outY, shape);
    double x, y;
    switch (shape.style.direction) {
    case EAST:
      x = (shape.geometry.x
           + (outX * shape.geometry.width));
      y = (shape.geometry.y
           + (outY * shape.geometry.height));
      break;
    case WEST:
      x = (shape.geometry.x
           + ((1 - outX) * shape.geometry.width));
      y = (shape.geometry.y
           + ((1 - outY) * shape.geometry.height));
      break;
    case NORTH:
      x = (shape.geometry.x
           + (outY * shape.geometry.width));
      y = (shape.geometry.y
           + ((1 - outX) * shape.geometry.height));
      break;
    case SOUTH:
      x = (shape.geometry.x
           + ((1 - outY) * shape.geometry.width));
      y = (shape.geometry.y
           + (outX * shape.geometry.height));
      break;
    }
    double cx = shape.geometry.x + shape.geometry.width / 2;
    double cy = shape.geometry.y + shape.geometry.height / 2;
    double angle = -shape.style.rotation * boost::math::double_constants::pi / 180;
    librevenge::RVNGPropertyList p = getPoint(x, y, cx, cy, angle);
    point.x = p["svg:x"]->getDouble();
    point.y = p["svg:y"]->getDouble();
  }

  void MXCell::adjustEndpoint(double& outX, double& outY, const MXCell& shape)
  {
    double x = outX; double y = outY;
    if (0 < x && x < 1 && 0 < y && y < 1) {
      outX = 0.5; outY = 0.5;
    } else if (shape.style.perimeter == TRIANGLE_P) {
      // update exitX, exitY to connect with diagonal edges of triangle
      if (y < 0.5 && x > 0) {
        double m = (x - 0.5) / (y - 0.5);
        outY = (0.5 - m/2) / (2 - m);
        outX = 2 * outY;
      } else if (y > 0.5 && x > 0) {
        double m = (x - 0.5) / (y - 0.5);
        outY = (1.5 + m/2) / (2 + m);
        outX = 2 - 2 * outY;
      }
    } else if (shape.style.perimeter == ELLIPSE_P) {
      if (x != 0.5) {
        double m = (y - 0.5) / (x - 0.5);
        double t = atan(m) + (x < 0.5 ? boost::math::double_constants::pi : 0);
        outX = 0.5 + 0.5*cos(t);
        outY = 0.5 + 0.5*sin(t);
      }
    } else if (shape.style.perimeter == RHOMBUS_P) {
      if (x < 0.5 && y < 0.5) {
        double m = (y - 0.5) / (x - 0.5);
        outX = m / (2 + 2*m);
        outY = -outX + 0.5;
      } else if (x < 0.5 && y > 0.5) {
        double m = (y - 0.5) / (x - 0.5);
        outX = m / (2*m - 2);
        outY = outX + 0.5;
      } else if (x > 0.5 && y < 0.5) {
        double m = (y - 0.5) / (x - 0.5);
        outX = (m - 2) / (2*m - 2);
        outY = outX - 0.5;
      } else if (x > 0.5 && y > 0.5) {
        double m = (y - 0.5) / (x - 0.5);
        outX = (m + 2) / (2*m + 2);
        outY = -outX + 1.5;
      }
    } else if (shape.style.perimeter == PARALLELOGRAM_P) {
      double c =
        (shape.style.parallelogramSize
         / (shape.style.direction == NORTH || shape.style.direction == SOUTH
            ? shape.geometry.height : shape.geometry.width));
      c = std::min(c, 0.5);
      if (c != 0 && x != 0.5) {
        double m = (y - 0.5) / (x - 0.5);
        if (x < c && y < 1) {
          outX = (m*c + c) / (2*m*c + 2);
          outY = 1 - outX/c;
        } else if (x > 1 - c && y > 0) {
          outX = (m*c - c + 2) / (2*m*c + 2);
          outY = (1 - outX) / c;
        }
      }
    } else if (shape.style.perimeter == HEXAGON_P) {
      double c =
        (shape.style.hexagonSize
         / (shape.style.direction == NORTH || shape.style.direction == SOUTH
            ? shape.geometry.height : shape.geometry.width));
      c = std::min(c, 0.5);
      if (c != 0 && x != 0.5) {
        double m = (y - 0.5) / (x - 0.5);
        if (x < c && y < 0.5) {
          outX = c * m / (2*c*m + 1);
          outY = -outX/(2*c) + 0.5;
        } else if (x > 1 - c && y < 0.5) {
          outX = (m*c - 1) / (2*m*c - 1);
          outY = (outX + c - 1) / (2*c);
        } else if (x < c && y > 0.5) {
          outX = m*c / (2*m*c - 1);
          outY = outX/(2*c) + 0.5;
        } else if (x > 1 - c && y > 0.5) {
          outX = (m*c + 1) / (2*m*c + 1);
          outY = (1 + c - outX) / (2*c);
        }
      }
    } else if (shape.style.perimeter == STEP_P) {
      double c =
        (shape.style.stepSize
         / (shape.style.direction == NORTH || shape.style.direction == SOUTH
            ? shape.geometry.height : shape.geometry.width));
      if (x == 0 && c > 0.5 && 0 < y && y < 1) {
        outX = 0.5; outY = 0.5;
      } else if (y == 0.5) {
        outX = x == 0 ? c : 1;
      } else {
        double m = (x - 0.5) / (y - 0.5);
        if (x == 0 && y < 0.5) {
          outY = (1 - m) / (4*c - 2*m);
          outX = 2*c*outY;
        } else if (x == 0 && y > 0.5) {
          outY = (4*c - 1 + m) / (4*c + 2*m);
          outX = 2*c - 2*c*outY;
        } else if (x > 1 - c && y < 0.5) {
          outY = (1 - 2*c + m) / (2*m - 4*c);
          outX = 2*c*outY + 1 - c;
        } else if (x > 1 - c && y > 0.5) {
          outY = (1 + 2*c + m) / (4*c + 2*m);
          outX = 1 + c - 2*c*outY;
        }
      }
    } else if (shape.style.perimeter == TRAPEZOID_P) {
      double c =
        (shape.style.trapezoidSize
         / (shape.style.direction == NORTH || shape.style.direction == SOUTH
            ? shape.geometry.height : shape.geometry.width));
      c = std::min(c, 0.5);
      if (c != 0 && x != 0.5) {
        double m = (y - 0.5) / (x - 0.5);
        if (x < c && y < 1) {
          outX = (c + m*c) / (2*m*c + 2);
          outY = 1 - outX/c;
        } else if (x > 1 - c && y < 1) {
          outX = (c + m*c - 2) / (2*m*c - 2);
          outY = (outX + c - 1) / c;
        }
      }
    }
  }

  void MXCell::setWaypoints(std::map<librevenge::RVNGString, MXCell> id_map)
  {
    if (!edge) return;
    if (style.edgeStyle == ORTHOGONAL && geometry.points.empty()) {
      double sourceX, sourceY, sourceWidth, sourceHeight;
      double targetX, targetY, targetWidth, targetHeight;
      if (!source_id.empty()) {
        MXCell source = id_map[source_id];
        sourceX = source.geometry.x; sourceY = source.geometry.y;
        if (!source.parent_id.empty()) {
          MXCell parent = id_map[source.parent_id];
          sourceX += parent.geometry.x; sourceY += parent.geometry.y;
        }
        sourceWidth = source.geometry.width; sourceHeight = source.geometry.height;
      } else {
        sourceX = geometry.sourcePoint.x; sourceY = geometry.sourcePoint.y;
        sourceWidth = 0; sourceHeight = 0;
      }
      if (!target_id.empty()) {
        MXCell target = id_map[target_id];
        targetX = target.geometry.x; targetY = target.geometry.y;
        if (!target.parent_id.empty()) {
          MXCell parent = id_map[target.parent_id];
          sourceX += parent.geometry.x;
          sourceY += parent.geometry.y;
        }
        targetWidth = target.geometry.width; targetHeight = target.geometry.height;
      } else {
        targetX = geometry.targetPoint.x; targetY = geometry.targetPoint.y;
        targetWidth = 0; targetHeight = 0;
      }
      MXPoint p = geometry.sourcePoint;
      Direction p_dir = style.startDir.get();
      MXPoint q = geometry.targetPoint;
      Direction q_dir = style.endDir.get();
      if (p == q) return;
      if (p_dir == NORTH) {
        p.y -= 20;
      } else if (p_dir == EAST) {
        p.x += 20;
      } else if (p_dir == SOUTH) {
        p.y += 20;
      } else if (p_dir == WEST) {
        p.x -= 20;
      }
      bool start = true;
      bool hugSource = true;
      while (p_dir != opposite(q_dir) || !pointsTo(p, q, p_dir)
             || (p.x != q.x && vertical(p_dir))
             || (p.y != q.y && horizontal(p_dir))) {
        double& change = (vertical(p_dir) ? p.y : p.x);
        double x = hugSource ? sourceX : targetX;
        double y = hugSource ? sourceY : targetY;
        double width = hugSource ? sourceWidth : targetWidth;
        double height = hugSource ? sourceHeight : targetHeight;
        bool overlap =
            (vertical(p_dir) ?
             ((targetY < sourceY && sourceY < targetY + targetHeight)
              || (sourceY < targetY && targetY < sourceY + sourceHeight)) :
             ((targetX < sourceX && sourceX < targetX + targetWidth)
              || (sourceX < targetX && targetX < sourceX + sourceWidth)));
        if (perpendicular(p_dir, q_dir) && pointsTo(p, q, p_dir) && pointsTo(q, p, q_dir)) {
          bool obstruction_s = false;
          if (horizontal(p_dir)) {
            if (sourceX - 20 < q.x && q.x < sourceX + sourceWidth + 20) {
              if (p.y < sourceY) {
                obstruction_s = sourceY + sourceHeight/2 < targetY + targetHeight/2;
              } else if (p.y > sourceY + sourceHeight) {
                obstruction_s = sourceY + sourceHeight/2 > targetY + targetHeight/2;
              }
            } else if (sourceY - 20 < p.y && p.y < sourceY + sourceHeight + 20) {
              if (q.x < sourceX) {
                obstruction_s = sourceX + sourceWidth/2 < targetX + targetWidth/2;
              } else if (q.x > sourceX + sourceWidth) {
                obstruction_s = sourceX + sourceWidth/2 > targetX + targetWidth/2;
              }
            }
          } else {
            if (sourceX - 20 < p.x && p.x < sourceX + sourceWidth + 20) {
              if (q.y < sourceY) {
                obstruction_s = sourceY + sourceHeight/2 < targetY + targetHeight/2;
              } else if (q.y > sourceY + sourceHeight) {
                obstruction_s = sourceY + sourceHeight/2 > targetY + targetHeight/2;
              }
            } else if (sourceY - 20 < q.y && q.y < sourceY + sourceHeight + 20) {
              if (p.x < sourceX) {
                obstruction_s = sourceX + sourceWidth/2 < targetX + targetWidth/2;
              } else if (p.x > sourceX + sourceWidth) {
                obstruction_s = sourceX + sourceWidth/2 > targetX + targetWidth/2;
              }
            }
          }
          bool obstruction_t =
            (horizontal(p_dir)
             ? targetY - 20 < p.y && p.y < targetY + targetHeight + 20
             : targetX - 20 < p.x && p.x < targetX + targetWidth + 20);
          if (targetX <= p.x && p.x <= targetX + targetHeight
              && targetY <= p.y && p.y <= targetY + targetHeight) {
            obstruction_t = false;
          }
          if (obstruction_s) {
            change = (p_dir == NORTH ? y - 20 :
                      p_dir == EAST ? x + width + 20 :
                      p_dir == SOUTH ? y + height + 20 : x - 20);
            if (vertical(p_dir)) {
              p_dir = (q.x < p.x ? WEST : EAST);
            } else {
              p_dir = (q.y < p.y ? NORTH : SOUTH);
            }
          } else if (obstruction_t) {
            change = (p_dir == NORTH ? (sourceY + targetY + targetHeight) / 2 :
                      p_dir == EAST ? (sourceX + targetX + sourceWidth) / 2 :
                      p_dir == SOUTH ? (sourceY + targetY + sourceHeight) / 2 :
                      (sourceX + targetX + targetWidth) / 2);
            if (vertical(p_dir)) {
              p_dir = (sourceX < p.x ? WEST : EAST);
            } else {
              p_dir = (sourceY < p.y ? NORTH : SOUTH);
            }
            hugSource = false;
          } else {
            change = (horizontal(p_dir) ? q.x : q.y);
            p_dir = opposite(q_dir);
          }
        } else if (p_dir == opposite(q_dir) && pointsTo(p, q, p_dir)) {
          double dist =
            (vertical(p_dir) ?
             std::min(std::abs(targetY + targetHeight - sourceY),
                      std::abs(sourceY + sourceHeight - targetY)) :
             std::min(std::abs(targetX + targetWidth - sourceX),
                      std::abs(sourceX + sourceWidth - targetX)));
          if (dist >= 40) {
            change = (p_dir == NORTH ? (sourceY + targetY + targetHeight) / 2 :
                      p_dir == EAST ? (sourceX + targetX + sourceWidth) / 2 :
                      p_dir == SOUTH ? (sourceY + targetY + sourceHeight) / 2 :
                      (sourceX + targetX + targetWidth) / 2);
            if (vertical(p_dir)) {
              p_dir = (q.x < p.x ? WEST : EAST);
            } else {
              p_dir = (q.y < p.y ? NORTH : SOUTH);
            }
            hugSource = false;
          } else {
            change = (p_dir == NORTH ? y - 20 :
                      p_dir == EAST ? x + width + 20 :
                      p_dir == SOUTH ? y + height + 20 : x - 20);
            if (vertical(p_dir)) {
              p_dir = (q.x < p.x ? WEST : EAST);
            } else {
              p_dir = (q.y < p.y ? NORTH : SOUTH);
            }
          }
        } else if (perpendicular(p_dir, q_dir)) {
          if (targetX <= p.x && p.x <= targetX + targetHeight
              && targetY <= p.y && p.y <= targetY + targetHeight) {
            q_dir = opposite(q_dir);
            continue;
          }
          double dist =
            (vertical(p_dir) ?
             std::min(std::abs(targetY + targetHeight - sourceY),
                      std::abs(sourceY + sourceHeight - targetY)) :
             std::min(std::abs(targetX + targetWidth - sourceX),
                      std::abs(sourceX + sourceWidth - targetX)));
          if (pointsTo(p, q, p_dir) && dist >= 40 && !overlap) {
            change = (p_dir == NORTH ? (sourceY + targetY + targetHeight) / 2 :
                      p_dir == EAST ? (sourceX + targetX + sourceWidth) / 2 :
                      p_dir == SOUTH ? (sourceY + targetY + sourceHeight) / 2 :
                      (sourceX + targetX + targetWidth) / 2);
            if (vertical(p_dir)) {
              p_dir = (q.x < p.x ? WEST : EAST);
            } else {
              p_dir = (q.y < p.y ? NORTH : SOUTH);
            }
            hugSource = false;
          } else {
            change = (p_dir == NORTH ? y - 20 :
                      p_dir == EAST ? x + width + 20 :
                      p_dir == SOUTH ? y + height + 20 : x - 20);
            if (start) {
              if (vertical(p_dir)) {
                p_dir = (targetX + targetWidth / 2 < sourceX + sourceWidth / 2
                         ? WEST : EAST);
              } else {
                p_dir = (targetY + targetHeight / 2 < sourceY + sourceHeight / 2
                         ? NORTH : SOUTH);
              }
            } else {
              if (vertical(p_dir)) {
                p_dir = (q.x < p.x ? WEST : EAST);
              } else {
                p_dir = (q.y < p.y ? NORTH : SOUTH);
              }
            }
          }
        } else if (p_dir == opposite(q_dir)) {
          change = (p_dir == NORTH ? y - 20 :
                    p_dir == EAST ? x + width + 20 :
                    p_dir == SOUTH ? y + height + 20 : x - 20);
          if (start) {
            if (vertical(p_dir)) {
              p_dir = (targetX + targetWidth / 2 < sourceX + sourceWidth / 2
                       ? WEST : EAST);
            } else {
              p_dir = (targetY + targetHeight / 2 < sourceY + sourceHeight / 2
                       ? NORTH : SOUTH);
            }
          } else {
            if (vertical(p_dir)) {
              p_dir = (q.x < p.x ? WEST : EAST);
            } else {
              p_dir = (q.y < p.y ? NORTH : SOUTH);
            }
          }
        } else if (p_dir == q_dir) {
          double test = vertical(p_dir) ? p.x : p.y;
          double target = vertical(p_dir) ? q.x : q.y;
          if (test == target) {
            q_dir = opposite(q_dir);
            continue;
          }
          if (targetX <= p.x && p.x <= targetX + targetHeight
              && targetY <= p.y && p.y <= targetY + targetHeight) {
            q_dir = opposite(q_dir);
            continue;
          }
          bool obstruction_s = false;
          if (vertical(p_dir)
              && sourceX - 20 < q.x && q.x < sourceX + sourceWidth + 20) {
            if (p.y < sourceY) {
              obstruction_s = sourceY + sourceHeight/2 < targetY + targetHeight/2;
            } else if (p.y > sourceY + sourceHeight) {
              obstruction_s = sourceY + sourceHeight/2 > targetY + targetHeight/2;
            }
          } else if (horizontal(p_dir)
                     && sourceY - 20 < q.y && q.y < sourceY + sourceHeight + 2
                     && sourceY - 20 < p.y && p.y < sourceY + sourceHeight + 20) {
            if (p.x < sourceX) {
              obstruction_s = sourceX + sourceWidth/2 < targetX + targetWidth/2;
            } else if (p.x > sourceX + sourceWidth) {
              obstruction_s = sourceX + sourceWidth/2 > targetX + targetWidth/2;
            }
          }
          bool obstruction_t = false;
          if (vertical(p_dir)
              && targetX - 20 < p.x && p.x < targetX + targetWidth + 20) {
            if (p.y < sourceY) {
              obstruction_t = sourceY + sourceHeight/2 > targetY + targetHeight/2;
            } else if (p.y > sourceY + sourceHeight) {
              obstruction_t = sourceY + sourceHeight/2 < targetY + targetHeight/2;
            }
          } else  if (horizontal(p_dir)
                      && targetY - 20 < p.y && p.y < targetY + targetHeight + 20) {
            if (p.x < sourceX) {
              obstruction_t = sourceX + sourceWidth/2 > targetX + targetWidth/2;
            } else if (p.x > sourceX + sourceWidth) {
              obstruction_t = sourceX + sourceWidth/2 < targetX + targetWidth/2;
            }
          }
          if (targetX <= p.x && p.x <= targetX + targetHeight
              && targetY <= p.y && p.y <= targetY + targetHeight) {
            obstruction_t = false;
          }
          if (!obstruction_t || obstruction_s) {
            if (p_dir == NORTH) {
              change = std::min(change, targetY - 20);
            } else if (p_dir == EAST) {
              change = std::max(change, targetX + targetWidth + 20);
            } else if (p_dir == SOUTH) {
              change = std::max(change, targetY + targetHeight + 20);
            } else {
              change = std::min(change, targetX - 20);
            }
            if (vertical(p_dir)) {
              p_dir = (q.x < p.x ? WEST : EAST);
            } else {
              p_dir = (q.y < p.y ? NORTH : SOUTH);
            }
          } else {
            change = (p_dir == NORTH ? (sourceY + targetY + targetHeight) / 2 :
                      p_dir == EAST ? (sourceX + targetX + sourceWidth) / 2 :
                      p_dir == SOUTH ? (sourceY + targetY + sourceHeight) / 2 :
                      (sourceX + targetX + targetWidth) / 2);
            if (vertical(p_dir)) {
              p_dir = (sourceX < p.x ? WEST : EAST);
            } else {
              p_dir = (sourceY < p.y ? NORTH : SOUTH);
            }
            hugSource = false;
          }
        }
        geometry.points.push_back(p);
        start = false;
      }
    }
  }

  bool MXCell::pointsTo(MXPoint p, MXPoint q, Direction dir)
  {
    switch (dir) {
    case NORTH:
      return q.y < p.y;
    case EAST:
      return q.x > p.x;
    case SOUTH:
      return q.y > p.y;
    case WEST:
      return q.x < p.x;
    }
  }

  librevenge::RVNGString MXCell::processText(librevenge::RVNGString input) {
    librevenge::RVNGString out;
    bool skipping = false;
    for (unsigned long i = 0; i < input.size(); i++) {
      char c = input.cstr()[i];
      if (c == '<') skipping = true;
      if (!skipping) out.append(c);
      if (c == '>') skipping = false;
    }
    return out;
  }

  std::string MXCell::getMarkerViewBox(MarkerType marker) {
    switch (marker) {
    case CLASSIC:
      return "0 0 40 40";
    };
  }

  std::string MXCell::getMarkerPath(MarkerType marker) {
    switch (marker) {
    case CLASSIC:
      return "M 20 0 L 40 40 L 20 30 L 0 40 Z";
    }
  }

  librevenge::RVNGPropertyList MXCell::getStyle() {
    librevenge::RVNGPropertyList styleProps;
    if (!style.fillColor.has_value()) styleProps.insert("draw:fill", "none");
    else {
      styleProps.insert("draw:fill", "solid");
      styleProps.insert("draw:fill-color", style.fillColor->to_string().c_str());
    }
    if (!style.strokeColor.has_value()) styleProps.insert("draw:stroke", "none");
    else {
      styleProps.insert("draw:stroke", "solid");
      styleProps.insert("svg:stroke-color", style.strokeColor->to_string().c_str());
      if (edge && (style.endFill || style.startFill)) {
        styleProps.remove("draw:fill"); styleProps.remove("draw:fill-color");
        styleProps.insert("draw:fill", "solid");
        styleProps.insert("draw:fill-color", styleProps["svg:stroke-color"]->getStr());
      }
    }
    if (style.startArrow.has_value() && edge) {
      styleProps.insert("draw:marker-start-viewbox",
                        getMarkerViewBox(style.startArrow.get()).c_str());
      styleProps.insert("draw:marker-start-path",
                        getMarkerPath(style.startArrow.get()).c_str());
      styleProps.insert("draw:marker-start-width", style.startSize / 100.);
    }
    if (style.endArrow.has_value() && edge) {
      styleProps.insert("draw:marker-end-viewbox",
                        getMarkerViewBox(style.endArrow.get()).c_str());
      styleProps.insert("draw:marker-end-path",
                        getMarkerPath(style.endArrow.get()).c_str());
      styleProps.insert("draw:marker-end-width", style.endSize / 100.);
    }

    return styleProps;
  }

  librevenge::RVNGPropertyList MXCell::getTextStyle() {
    librevenge::RVNGPropertyList styleProps;

    styleProps.insert("style:font-name", text_style.fontFamily);
    styleProps.insert("fo:font-size", text_style.fontSize * 0.75, librevenge::RVNG_POINT);
    if (text_style.fontColor.has_value()) {
      styleProps.insert("fo:color", text_style.fontColor->to_string().c_str());
    }
    if (text_style.backgroundColor.has_value()) {
      styleProps.insert("fo:background-color", text_style.backgroundColor->to_string().c_str());
    }
    styleProps.insert("fo:font-weight", text_style.bold ? "bold" : "normal");
    styleProps.insert("fo:font-style", text_style.italic ? "italic" : "normal");
    styleProps.insert("style:text-underline-style", text_style.underline ? "solid" : "none");

    return styleProps;
  }
} // namespace libdrawio

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
