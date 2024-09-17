/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef DRAWIOTYPES_H
#define DRAWIOTYPES_H

#include "librevenge/RVNGBinaryData.h"
#include <ios>
#include <map>
#include <sstream>
#include <string>
#include <iomanip>
#include <map>

namespace libdrawio {
  enum Direction {
    NORTH = 0,
    EAST,
    SOUTH,
    WEST
  };

  inline Direction opposite(Direction dir)
  {
    return (Direction)(((int)dir + 2) % 4);
  }

  inline bool horizontal(Direction dir)
  {
    return (int)dir % 2 == 1;
  }

  inline bool vertical(Direction dir)
  {
    return !horizontal(dir);
  }

  inline bool perpendicular(Direction dir1, Direction dir2)
  {
    return std::abs((int)dir1 - (int)dir2) % 2 == 1;
  }

  enum AlignH {
    LEFT,
    CENTER,
    RIGHT,
  };

  inline std::string to_string(AlignH a)
  {
    static std::map<AlignH, std::string> map = {
      {LEFT, "left"},
      {CENTER, "center"}, 
      {RIGHT, "right"}
    };
    return map[a];
  }

  enum AlignV {
    TOP,
    MIDDLE,
    BOTTOM
  };

  inline std::string to_string(AlignV a)
  {
    static std::map<AlignV, std::string> map = {
      {TOP, "top"},
      {MIDDLE, "middle"}, 
      {BOTTOM, "bottom"}
    };
    return map[a];
  }

  enum Shape {
    RECTANGLE,
    ELLIPSE,
    TRIANGLE,
    CALLOUT,
    PROCESS,
    RHOMBUS,
    PARALLELOGRAM,
    HEXAGON,
    STEP,
    TRAPEZOID,
    CARD,
    INTERNAL_STORAGE,
    OR,
    XOR,
    DOCUMENT,
    TAPE,
    DATA_STORAGE
  };

  enum Perimeter {
    RECTANGLE_P,
    ELLIPSE_P,
    TRIANGLE_P,
    CALLOUT_P,
    RHOMBUS_P,
    PARALLELOGRAM_P,
    HEXAGON_P,
    STEP_P,
    TRAPEZOID_P,
  };

  enum EdgeStyle {
    STRAIGHT,
    ORTHOGONAL
  };

  enum MarkerType {
    CLASSIC
  };

  struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    Color(unsigned char red, unsigned char green,
          unsigned char blue, unsigned char alpha)
      : r(red), g(green), b(blue), a(alpha) {}
    Color() : r(0), g(0), b(0), a(0) {}
    inline bool operator==(const Color &col) {
      return ((r == col.r) && (g == col.g) && (b == col.b) && (a == col.a));
    }
    inline bool operator!=(const Color &col) {
      return !operator==(col);
    }
    inline bool operator!() const {
      return (!r && !g && !b && !a);
    }
    std::string to_string() {
      std::stringstream out;
      out << "#" << std::hex
          << std::setw(2) << (unsigned)r
          << std::setw(2) << (unsigned)g
          << std::setw(2) << (unsigned)b;
      return out.str();
    }
  };

  enum TextFormat {
    DRAWIO_TEXT_ANSI = 0,
    DRAWIO_TEXT_SYMBOL,
    DRAWIO_TEXT_GREEK,
    DRAWIO_TEXT_TURKISH,
    DRAWIO_TEXT_VIETNAMESE,
    DRAWIO_TEXT_HEBREW,
    DRAWIO_TEXT_ARABIC,
    DRAWIO_TEXT_BALTIC,
    DRAWIO_TEXT_RUSSIAN,
    DRAWIO_TEXT_THAI,
    DRAWIO_TEXT_CENTRAL_EUROPE,
    DRAWIO_TEXT_JAPANESE,
    DRAWIO_TEXT_KOREAN,
    DRAWIO_TEXT_CHINESE_SIMPLIFIED,
    DRAWIO_TEXT_CHINESE_TRADITIONAL,
    DRAWIO_TEXT_UTF8,
    DRAWIO_TEXT_UTF16
  };

  class DRAWIOName {
  public:
    DRAWIOName(const librevenge::RVNGBinaryData &data, TextFormat format)
      : m_data(data), m_format(format) {}
    DRAWIOName() : m_data(), m_format(DRAWIO_TEXT_ANSI) {}
    DRAWIOName(const DRAWIOName &name) = default;
    DRAWIOName &operator=(const DRAWIOName &name) = default;
    bool empty() const {
      return !m_data.size();
    }
    void clear() {
      m_data.clear();
      m_format = DRAWIO_TEXT_ANSI;
    }
    librevenge::RVNGBinaryData m_data;
    TextFormat m_format;
  };
}

#endif

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
