//
//  Copyright (C) 2015 Greg Landrum
//
//   @@ All Rights Reserved @@
//  This file is part of the RDKit.
//  The contents are covered by the terms of the BSD license
//  which is included in the file license.txt, found at the root
//  of the RDKit source tree.
//
// derived from Dave Cosgrove's MolDraw2D
//

#include "MolDraw2DCairo.h"
#include <cairo.h>

namespace RDKit {
void MolDraw2DCairo::initDrawing() {
  PRECONDITION(dp_cr, "no draw context");
  cairo_select_font_face(dp_cr, "sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(dp_cr, fontSize());
  cairo_set_line_cap(dp_cr, CAIRO_LINE_CAP_BUTT);
}

// ****************************************************************************
void MolDraw2DCairo::finishDrawing() {}

// ****************************************************************************
void MolDraw2DCairo::setColour(const DrawColour &col) {
  PRECONDITION(dp_cr, "no draw context");
  MolDraw2D::setColour(col);
  cairo_set_source_rgb(dp_cr, col.r, col.g, col.b);
}

// ****************************************************************************
void MolDraw2DCairo::drawLine(const Point2D &cds1, const Point2D &cds2) {
  PRECONDITION(dp_cr, "no draw context");
  Point2D c1 = getDrawCoords(cds1);
  Point2D c2 = getDrawCoords(cds2);

  unsigned int width = getDrawLineWidth();

  std::string dashString = "";

  cairo_set_line_width(dp_cr, width);

  const DashPattern &dashes = dash();
  if (dashes.size()) {
    auto *dd = new double[dashes.size()];
    std::copy(dashes.begin(), dashes.end(), dd);
    cairo_set_dash(dp_cr, dd, dashes.size(), 0);
    delete[] dd;
  } else {
    cairo_set_dash(dp_cr, nullptr, 0, 0);
  }

  cairo_move_to(dp_cr, c1.x, c1.y);
  cairo_line_to(dp_cr, c2.x, c2.y);
  cairo_stroke(dp_cr);
}

void MolDraw2DCairo::drawWavyLine(const Point2D &cds1, const Point2D &cds2,
                                  const DrawColour &col1,
                                  const DrawColour &col2,
                                  unsigned int nSegments, double vertOffset) {
  PRECONDITION(dp_cr, "no draw context");
  PRECONDITION(nSegments > 1, "too few segments");
  RDUNUSED_PARAM(col2);

  if (nSegments % 2) {
    ++nSegments;  // we're going to assume an even number of segments
  }

  Point2D perp = calcPerpendicular(cds1, cds2);
  Point2D delta = (cds2 - cds1);
  perp *= vertOffset;
  delta /= nSegments;

  Point2D c1 = getDrawCoords(cds1);

  unsigned int width = getDrawLineWidth();
  cairo_set_line_width(dp_cr, width);
  cairo_set_dash(dp_cr, nullptr, 0, 0);
  setColour(col1);
  cairo_move_to(dp_cr, c1.x, c1.y);
  for (unsigned int i = 0; i < nSegments; ++i) {
    Point2D startpt = cds1 + delta * i;
    Point2D segpt = getDrawCoords(startpt + delta);
    Point2D cpt1 =
        getDrawCoords(startpt + delta / 3. + perp * (i % 2 ? -1 : 1));
    Point2D cpt2 =
        getDrawCoords(startpt + delta * 2. / 3. + perp * (i % 2 ? -1 : 1));
    // if (i == nSegments / 2 && col2 != col1) setColour(col2);
    cairo_curve_to(dp_cr, cpt1.x, cpt1.y, cpt2.x, cpt2.y, segpt.x, segpt.y);
  }
  cairo_stroke(dp_cr);
}

// ****************************************************************************
// draw the char, with the bottom left hand corner at cds
void MolDraw2DCairo::drawChar(char c, const Point2D &cds) {
  PRECONDITION(dp_cr, "no draw context");
  char txt[2];
  txt[0] = c;
  txt[1] = 0;

  cairo_set_font_size(dp_cr, drawFontSize());
  if(c == '.') {
    cairo_set_font_size(dp_cr, 1.5 * drawFontSize());
  }
  cairo_text_extents_t extents;
  cairo_text_extents(dp_cr, txt, &extents);
  Point2D c1 = cds;
  cairo_move_to(dp_cr, c1.x, c1.y);
  cairo_show_text(dp_cr, txt);
  cairo_stroke(dp_cr);
  // set the font size back to molecule units so getStringSize
  // still works properly.
  cairo_set_font_size(dp_cr, fontSize());
}

// ****************************************************************************
void MolDraw2DCairo::drawPolygon(const std::vector<Point2D> &cds) {
  PRECONDITION(dp_cr, "no draw context");
  PRECONDITION(cds.size() >= 3, "must have at least three points");

  unsigned int width = getDrawLineWidth();

  cairo_line_cap_t olinecap = cairo_get_line_cap(dp_cr);
  cairo_line_join_t olinejoin = cairo_get_line_join(dp_cr);

  cairo_set_line_cap(dp_cr, CAIRO_LINE_CAP_BUTT);
  cairo_set_line_join(dp_cr, CAIRO_LINE_JOIN_BEVEL);
  cairo_set_dash(dp_cr, nullptr, 0, 0);
  cairo_set_line_width(dp_cr, width);

  if(!cds.empty()) {
    Point2D lc = getDrawCoords(cds.front());
    cairo_move_to(dp_cr, lc.x, lc.y);
    for (unsigned int i = 1; i < cds.size(); ++i) {
      Point2D lci = getDrawCoords(cds[i]);
      cairo_line_to(dp_cr, lci.x, lci.y);
    }
  }

  if (fillPolys()) {
    cairo_close_path(dp_cr);
    cairo_fill_preserve(dp_cr);
  }
  cairo_stroke(dp_cr);
  cairo_set_line_cap(dp_cr, olinecap);
  cairo_set_line_join(dp_cr, olinejoin);
}

// ****************************************************************************
void MolDraw2DCairo::clearDrawing() {
  PRECONDITION(dp_cr, "no draw context");
  setColour(drawOptions().backgroundColour);
  cairo_rectangle(dp_cr, 0, 0, width(), height());
  cairo_fill(dp_cr);
}

// ****************************************************************************
// using the current scale, work out the size of the label in molecule
// coordinates
void MolDraw2DCairo::getStringSize(const std::string &label,
                                   double &label_width,
                                   double &label_height) const {
  PRECONDITION(dp_cr, "no draw context");
  label_width = 0.0;
  label_height = 0.0;

  TextDrawType draw_mode = TextDrawNormal;
  cairo_set_font_size(dp_cr, fontSize());

  bool had_a_super = false;

  char txt[2];
  txt[1] = 0;
  for (int i = 0, is = label.length(); i < is; ++i) {
    // setStringDrawMode moves i along to the end of any <sub> or <sup>
    // markup
    if ('<' == label[i] && setStringDrawMode(label, draw_mode, i)) {
      continue;
    }

    txt[0] = label[i];
    cairo_text_extents_t extents;
    cairo_text_extents(dp_cr, txt, &extents);
    double twidth = extents.x_advance, theight = extents.height;

    label_height = std::max(label_height, theight);
    double char_width = twidth;
    if (TextDrawSubscript == draw_mode) {
      char_width *= 0.75;
    } else if (TextDrawSuperscript == draw_mode) {
      char_width *= 0.75;
      had_a_super = true;
    }
    label_width += char_width;
  }

  // subscript keeps its bottom in line with the bottom of the bit chars,
  // superscript goes above the original char top by a quarter
  if (had_a_super) {
    label_height *= 1.25;
  }
  label_height *= 1.2;  // empirical

}
// ****************************************************************************
namespace {
cairo_status_t grab_str(void *closure, const unsigned char *data,
                        unsigned int len) {
  auto *str_ptr = (std::string *)closure;
  (*str_ptr) += std::string((const char *)data, len);
  return CAIRO_STATUS_SUCCESS;
}
}  // namespace
std::string MolDraw2DCairo::getDrawingText() const {
  PRECONDITION(dp_cr, "no draw context");
  std::string res = "";
  cairo_surface_t *surf = cairo_get_target(dp_cr);
  cairo_surface_write_to_png_stream(surf, &grab_str, (void *)&res);
  return res;
};

void MolDraw2DCairo::writeDrawingText(const std::string &fName) const {
  PRECONDITION(dp_cr, "no draw context");
  cairo_surface_t *surf = cairo_get_target(dp_cr);
  if(cairo_surface_write_to_png(surf, fName.c_str()) != CAIRO_STATUS_SUCCESS) {
    std::cerr << "Failed to write PNG file " << fName << std::endl;
  }
};

}  // namespace RDKit
