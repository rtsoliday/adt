/**
 * @file adt_qt.cc
 * @brief Minimal Qt-based entry for ADT.
 *
 * Provides the Qt-based front end for the Array Display Tool.
 *
 * @author Robert Soliday
 * @copyright
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 * Distributed subject to a Software License Agreement found in the file LICENSE that is included with this distribution.
 */

#include "aps.icon"
#include "adtVersion.h"

#include <epicsVersion.h>
#include <QSysInfo>
#include <QApplication>
#include <QBitmap>
#include <QImage>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QByteArray>
#include <QFontMetrics>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPen>
#include <QMessageBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QLineEdit>
#include <QWidget>
#include <QList>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QTimer>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPointer>
#include <QPixmap>
#include <QProcess>
#include <QTemporaryFile>
#include <limits>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <ctime>

#include <SDDS.h>
#include <QDir>
#include <cadef.h>

#define INITFILENAME "adtrc"

static constexpr int GRIDDIVISIONS = 5;
static const char *PVID = "ADTPV";
static constexpr double LARGEVAL = 1e40;
static const char *SDDSID = "SDDS1";
static const char *SNAPID = "ADTSNAP";

static const double scale[] = {
  0.001, 0.002, 0.005, 0.010, 0.020, 0.050, 0.100, 0.200, 0.500,
  1.000, 2.000, 5.000, 10.00, 20.00, 50.00, 100.0, 200.0, 500.0,
  1000., 2000., 5000.
};
static constexpr int NSCALES = sizeof(scale) / sizeof(scale[0]);

static const char *defaultColors[] = {
  "Red", "Blue", "Green3", "Gold", "Magenta", "Cyan", "Orange",
  "Purple", "SpringGreen", "Chartreuse", "DeepSkyBlue",
  "MediumSpringGreen", "Tomato", "Tan", "Grey75", "Black"
};
static constexpr int NCOLORS = sizeof(defaultColors) / sizeof(defaultColors[0]);
static constexpr int NSAVE = 5;

static bool markers = true, lines = true, bars = false, grid = true,
  autoclear = true, showmaxmin = true, fillmaxmin = true, statMode = false,
  refOn = true, referenceLoaded = false;
static int diffSet = -1, displaySet = -1, nsect = 0;
static QColor displayColor("Grey40");
static double nstat = 0.0, nstatTime = 0.0, stotal = 0.0;
static QVector<QString> latNames;
static QVector<double> latS, latLen;
static QVector<short> latHeight;

struct AreaData
{
  int index = 0;
  int currScale = 0;
  double centerVal = 0.0;
  double oldCenterVal = 0.0;
  double xmin = 0.0;
  double xmax = 0.0;
  bool initialized = false;
  bool tempclear = false;
  bool tempnodraw = false;
  int xStart = 0;
  int xEnd = -1;
};
struct ArrayData;

struct GetCallbackData
{
  ArrayData *arr = nullptr;
  int index = 0;
};

struct ArrayData
{
  int index = 0;
  int nvals = 0;
  QVector<QString> names;
  QVector<double> vals;
  QVector<double> s;
  QVector<double> minVals;
  QVector<double> maxVals;
  QVector<bool> conn;
  QVector<chid> chids;
  QVector<GetCallbackData> cbData;
  bool zoom = false;
  QString heading;
  QString units;
  double scaleFactor = 1.0;
  double sdev = 0.0;
  double avg = 0.0;
  double maxVal = 0.0;
  double runSdev = 0.0;
  double runAvg = 0.0;
  double runMax = 0.0;
  AreaData *area = nullptr;
  QColor color;
  QVector<double> saveVals[NSAVE];
  QVector<double> refVals;
};

class PlotWidget;
class AreaWidget;

static PlotWidget *zoomPlot = nullptr;
static AreaData *zoomAreaPtr = nullptr;
static AreaWidget *zoomAreaWidget = nullptr;
static void updateZoomCenter();
static void updateZoomInterval();
static void getCallback(struct event_handler_args args);

struct LoadItem
{
  QString label;
  QString file;
};

struct LoadMenuInfo
{
  QString title;
  QList<LoadItem> items;
};

/**
 * @brief Construct the plotting rectangle.
 *
 * @param w Widget width in pixels.
 * @param h Widget height in pixels.
 * @param headerHeight Height reserved for the header area.
 * @return QRect defining the plot bounds.
 */
static QRect makePlotRect(int w, int h, int headerHeight = 0)
{
  return QRect(0, headerHeight, w - 1, h - headerHeight - 1);
}

/**
 * @brief Wrap an index within bounds.
 *
 * @param idx Candidate index value.
 * @param n Size of the range.
 * @return Wrapped index in [0, n).
 */
static int wrapIndex(int idx, int n)
{
  idx %= n;
  return idx < 0 ? idx + n : idx;
}

/**
 * @brief Free arrays of strings allocated by SDDS routines.
 *
 * @param n Number of strings.
 * @param p Pointer to array of C strings.
 */
static void freeSddsStrings(int n, char **p)
{
  if (!p)
    return;
  for (int i = 0; i < n; ++i)
    SDDS_Free(p[i]);
  SDDS_Free(p);
}

/**
 * @brief Read initialization file for menu configuration.
 *
 * Parses the ADT rc file to populate menus and directories.
 *
 * @param filename Path to the initialization file.
 * @param adtHome ADT installation directory.
 * @param pvDirectory Output path for standard PV directory.
 * @param customDirectory Output path for custom PV directory.
 * @return List of menu items discovered in the file.
 */
static QList<LoadMenuInfo> readInitFile(const QString &filename,
  const QString &adtHome, QString &pvDirectory, QString &customDirectory)
{
  QList<LoadMenuInfo> menus;
  pvDirectory = adtHome + "/pv";
  customDirectory = pvDirectory;
  if (filename.isEmpty())
    return menus;
  SDDS_TABLE table;
  QByteArray fname = filename.toUtf8();
  if (!SDDS_InitializeInput(&table, fname.data()))
    return menus;
  int foundLabels = SDDS_CheckColumn(&table, const_cast<char *>("ADTMenuLabel"),
    NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY;
  bool first = true;
  while (SDDS_ReadTable(&table) > 0) {
    if (first) {
      char *dir = NULL;
      if (SDDS_GetParameter(&table, const_cast<char *>("ADTPVDirectory"), &dir)) {
        pvDirectory = dir;
        customDirectory = dir;
        SDDS_Free(dir);
      }
      char *subdir = NULL;
      if (SDDS_GetParameter(&table, const_cast<char *>("ADTPVSubDirectory"),
        &subdir)) {
        if (subdir[0]) {
          pvDirectory = adtHome + "/" + subdir;
          customDirectory = pvDirectory;
        } else {
          pvDirectory = adtHome;
          customDirectory = adtHome;
        }
        SDDS_Free(subdir);
      }
      first = false;
    }
    LoadMenuInfo menu;
    char *title = NULL;
    if (SDDS_GetParameter(&table, const_cast<char *>("ADTMenuTitle"), &title)) {
      menu.title = title;
      SDDS_Free(title);
    }
    int rows = SDDS_CountRowsOfInterest(&table);
    char **files = (char **)SDDS_GetColumn(&table,
      const_cast<char *>("ADTPVFile"));
    char **labels = foundLabels ? (char **)SDDS_GetColumn(&table,
      const_cast<char *>("ADTMenuLabel")) : NULL;
    for (int i = 0; i < rows; ++i) {
      QString fname = pvDirectory + "/" + files[i];
      QString lab = labels ? labels[i] : files[i];
      menu.items.append({lab, fname});
    }
    freeSddsStrings(rows, files);
    freeSddsStrings(rows, labels);
    menus.append(menu);
  }
  SDDS_Terminate(&table);
  return menus;
}

/**
 * @brief Read lattice file and extract sector information.
 *
 * @param filename Path to the lattice file.
 * @param nsectOut Returns the number of sectors.
 * @param stotalOut Returns the total lattice length.
 * @return true on success, false on failure.
 */
static bool readLatticeFile(const QString &filename, int &nsectOut,
  double &stotalOut)
{
  latNames.clear(); latS.clear(); latLen.clear(); latHeight.clear();
  SDDS_TABLE table;
  QByteArray fname = filename.toUtf8();
  if (!SDDS_InitializeInput(&table, fname.data()))
    return false;
  if (SDDS_ReadTable(&table) != 1)
    return false;
  bool ok = false;
  char *type = NULL;
  type = SDDS_GetParameterAsString(&table, const_cast<char *>("ADTFileType"), NULL);
  if (type && !strcmp(type, "ADTLATTICE")) {
    int32_t ns;
    double st;
    if (SDDS_GetParameterAsLong(&table, const_cast<char *>("Nsectors"), &ns) &&
        SDDS_GetParameterAsDouble(&table, const_cast<char *>("Stotal"), &st)) {
      nsectOut = static_cast<int>(ns);
      stotalOut = st;
      int rows = SDDS_CountRowsOfInterest(&table);
      char **names = (char **)SDDS_GetColumn(&table,
        const_cast<char *>("Name"));
      double *scol = (double *)SDDS_GetColumn(&table,
        const_cast<char *>("S"));
      double *len = (double *)SDDS_GetColumn(&table,
        const_cast<char *>("Length"));
      short *height = (short *)SDDS_GetColumn(&table,
        const_cast<char *>("SymbolHeight"));
      if (names && scol && len && height) {
        for (int i = 0; i < rows; ++i) {
          latNames.append(names[i]);
          latS.append(scol[i]);
          latLen.append(len[i]);
          latHeight.append(height[i]);
        }
        ok = true;
      }
      freeSddsStrings(rows, names);
      SDDS_Free(scol);
      SDDS_Free(len);
      SDDS_Free(height);
    }
  }
  SDDS_Free(type);
  SDDS_Terminate(&table);
  return ok;
}

class LogoWidget : public QWidget
{
public:
  explicit LogoWidget(QWidget *parent = nullptr) : QWidget(parent)
  {
    setMinimumSize(890, 185);
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    QBitmap bitmap = QBitmap::fromData(QSize(aps_width, aps_height),
      aps_bits, QImage::Format_MonoLSB);
    p.drawPixmap(50, (height() - aps_height) / 2, bitmap);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    p.setFont(font);
    QFontMetrics fm(font);
    int fheight = fm.height();
    int linespacing = static_cast<int>(1.5 * fheight);
    int ystart = (height() - fheight) / 2 - fheight / 2;

    auto drawCentered = [this, &p, &fm](const QString &text, int y)
    {
      int width = fm.horizontalAdvance(text);
      int xstart = (this->width() - width) / 2;
      p.drawText(xstart, y, text);
    };

    drawCentered("Array Display Tool", ystart - linespacing);
    drawCentered("Written by Dwarfs in the Waterfall Glenn", ystart);
    drawCentered(QString("Version %1 %2").arg(ADT_VERSION_STRING)
      .arg(EPICS_VERSION_STRING), ystart + linespacing);

    drawCentered(QString("Running %1 %2 on %3")
      .arg(QSysInfo::kernelType())
      .arg(QSysInfo::kernelVersion())
      .arg(QSysInfo::machineHostName()), ystart + 2 * linespacing);

    drawCentered("Please Send Comments and Bugs to soliday@anl.gov",
      ystart + 3 * linespacing);
  }
};

/**
 * @brief Widget for displaying a single ADT plotting area.
 */
class PlotWidget : public QWidget
{
public:
  PlotWidget(AreaData *adata, const QVector<ArrayData *> &arrays,
    QWidget *parent = nullptr)
    : QWidget(parent), area(adata), arrayPtrs(arrays)
  {
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    if (pixmap.size() != size()) {
      pixmap = QPixmap(size());
      pixmap.fill(Qt::white);
    }
    QPainter pmap(&pixmap);
    if (autoclear || area->tempclear)
      pmap.fillRect(pixmap.rect(), Qt::white);
    area->tempclear = false;

    QRect plotRect = makePlotRect(pixmap.width(), pixmap.height());

    pmap.setPen(Qt::black);
    pmap.drawRect(plotRect);

    double upd = scale[area->currScale];
    double ymin = area->centerVal - upd * GRIDDIVISIONS;
    double ymax = area->centerVal + upd * GRIDDIVISIONS;
    int bottom = plotRect.bottom();
    double yscale = plotRect.height() / (ymax - ymin);

    auto mapY = [&](double v) {
      // Allow values to map outside of the visible plotting area so that
      // data points and connecting lines can extend beyond the edges when
      // necessary. This avoids clamping out-of-range values to the plot
      // boundaries, which previously caused points to appear on the edge of
      // the graph.
      return bottom - static_cast<int>((v - ymin) * yscale);
    };

    bool refOnLoaded = referenceLoaded && refOn;
    bool diffOn = diffSet >= 0;
    auto diffVal = [&](ArrayData *arr, const QVector<double> &vec, int idx) {
      double v = vec[idx];
      if (refOnLoaded && arr->refVals.size() == arr->nvals)
        v -= arr->refVals[idx];
      if (diffOn && arr->saveVals[diffSet].size() == arr->nvals)
        v -= arr->saveVals[diffSet][idx];
      return v;
    };

    if (grid) {
      pmap.setPen(Qt::gray);
      for (int i = -GRIDDIVISIONS; i <= GRIDDIVISIONS; ++i) {
        int y = mapY(area->centerVal + i * upd);
        pmap.drawLine(plotRect.left(), y, plotRect.right(), y);
      }
    }
    pmap.setPen(Qt::black);

    int y0 = mapY(area->centerVal);

    if (showmaxmin) {
      for (auto arr : arrayPtrs) {
        if (arr->nvals < 1 || arr->minVals.size() != arr->nvals ||
            arr->maxVals.size() != arr->nvals)
          continue;
        if (area == zoomAreaPtr) {
          int nvals = arr->nvals;
          int start = wrapIndex(area->xStart, nvals);
          int end = wrapIndex(area->xEnd, nvals);
          int count = (end >= start) ?
            (end - start + 1) : (nvals - start + end + 1);
          if (count < 1)
            continue;
          double smin = arr->s[start];
          double smax = arr->s[end] + (end < start ? stotal : 0.0);
          double range = smax - smin;
          auto xpos = [&](int idx) {
            double sval = arr->s[idx];
            if (idx < start)
              sval += stotal;
            double frac = (sval - smin) / range;
            return plotRect.left() + frac * plotRect.width();
          };
          if (fillmaxmin) {
            QVector<QPointF> poly(2 * count);
            for (int i = 0; i < count; ++i) {
              int idx = (start + i) % nvals;
              poly[i] = QPointF(xpos(idx),
                mapY(diffVal(arr, arr->minVals, idx)));
            }
            for (int i = 0; i < count; ++i) {
              int idx = (start + count - 1 - i) % nvals;
              poly[count + i] = QPointF(xpos(idx),
                mapY(diffVal(arr, arr->maxVals, idx)));
            }
            pmap.save();
            pmap.setPen(Qt::NoPen);
            pmap.setBrush(Qt::lightGray);
            pmap.drawPolygon(poly.constData(), poly.size());
            pmap.restore();
          } else {
            pmap.setPen(arr->color);
            tmpPts.resize(count);
            for (int i = 0; i < count; ++i) {
              int idx = (start + i) % nvals;
              tmpPts[i] = QPointF(xpos(idx),
                mapY(diffVal(arr, arr->minVals, idx)));
            }
            pmap.drawPolyline(tmpPts.constData(), count);
            for (int i = 0; i < count; ++i) {
              int idx = (start + i) % nvals;
              tmpPts[i] = QPointF(xpos(idx),
                mapY(diffVal(arr, arr->maxVals, idx)));
            }
            pmap.drawPolyline(tmpPts.constData(), count);
            pmap.setPen(Qt::black);
          }
        } else {
          int start = area->xStart;
          int end = area->xEnd >= area->xStart ? area->xEnd + 1 : arr->nvals;
          if (start < 0)
            start = 0;
          if (end > arr->nvals)
            end = arr->nvals;
          int count = end - start;
          if (count < 1)
            continue;
          double xstep = plotRect.width() /
            static_cast<double>(count);
          double xstart = plotRect.left() + xstep / 2.0;
          if (fillmaxmin) {
            QVector<QPointF> poly(2 * count);
            for (int i = 0; i < count; ++i) {
              int idx = start + i;
              poly[i] = QPointF(xstart + i * xstep,
                mapY(diffVal(arr, arr->minVals, idx)));
            }
            for (int i = 0; i < count; ++i) {
              int idx = start + count - 1 - i;
              poly[count + i] = QPointF(xstart + (count - 1 - i) * xstep,
                mapY(diffVal(arr, arr->maxVals, idx)));
            }
            pmap.save();
            pmap.setPen(Qt::NoPen);
            pmap.setBrush(Qt::lightGray);
            pmap.drawPolygon(poly.constData(), poly.size());
            pmap.restore();
          } else {
            pmap.setPen(arr->color);
            tmpPts.resize(count);
            for (int i = start; i < end; ++i)
              tmpPts[i - start] = QPointF(xstart + (i - start) * xstep,
                mapY(diffVal(arr, arr->minVals, i)));
            pmap.drawPolyline(tmpPts.constData(), count);
            for (int i = start; i < end; ++i)
              tmpPts[i - start] = QPointF(xstart + (i - start) * xstep,
                mapY(diffVal(arr, arr->maxVals, i)));
            pmap.drawPolyline(tmpPts.constData(), count);
            pmap.setPen(Qt::black);
          }
        }
      }
    }

    pmap.setPen(Qt::black);
    pmap.drawLine(plotRect.left(), y0, plotRect.right(), y0);

    if (area->tempnodraw) {
      area->tempnodraw = false;
      QPainter pw(this);
      pw.drawPixmap(0, 0, pixmap);
      return;
    }

      auto drawArray = [&](ArrayData *arr, const QVector<double> &vec,
        const QColor &clr) {
        if (arr->nvals < 1 || vec.size() != arr->nvals)
          return;
        pmap.setPen(clr);
        if (area == zoomAreaPtr) {
          int nvals = arr->nvals;
          int start = wrapIndex(area->xStart, nvals);
          int end = wrapIndex(area->xEnd, nvals);
          int count = (end >= start) ?
            (end - start + 1) : (nvals - start + end + 1);
          if (count < 1)
            return;
          double smin, smax;
          if (count >= nvals) {
            smin = 0.0;
            smax = stotal;
          } else {
            smin = arr->s[start];
            smax = arr->s[end] + (end < start ? stotal : 0.0);
          }
          double range = smax - smin;
          double xscale = plotRect.width() / range;
          if (lines || markers)
            tmpPts.resize(count);
          for (int i = 0; i < count; ++i) {
            int idx = (start + i) % nvals;
            double sval = arr->s[idx];
            // Only adjust the coordinate for wrapped indices when we are
            // displaying less than a full set of values. When the zoom span
            // covers the entire array (count >= nvals), `start` may be
            // non-zero but all points should map within [0, stotal] without
            // an offset. Adjusting by `stotal` in that case pushed the left
            // side of the plot off-screen, leaving behind artifacts from a
            // previous paint and producing lines with incorrect colors.
            if (count < nvals && idx < start)
              sval += stotal;
            double x = plotRect.left() + (sval - smin) * xscale;
            int y = mapY(diffVal(arr, vec, idx));
            int xi = static_cast<int>(x);
            if (bars)
              pmap.drawLine(xi, y0, xi, y);
            if (lines || markers)
              tmpPts[i] = QPointF(x, y);
          }
          if (lines)
            pmap.drawPolyline(tmpPts.constData(), count);
          if (markers) {
            QPen oldPen = pmap.pen();
            QPen markerPen(clr, 3);
            markerPen.setCapStyle(Qt::SquareCap);
            pmap.setPen(markerPen);
            pmap.drawPoints(tmpPts.constData(), count);
            pmap.setPen(oldPen);
          }
        } else {
          int start = area->xStart;
          int end = area->xEnd >= area->xStart ? area->xEnd + 1 : arr->nvals;
          if (start < 0)
            start = 0;
          if (end > arr->nvals)
            end = arr->nvals;
          int count = end - start;
          if (count < 1)
            return;
          double xstep = plotRect.width() /
            static_cast<double>(count);
          double x = plotRect.left() + xstep / 2.0;
          if (lines || markers)
            tmpPts.resize(count);
          for (int i = start; i < end; ++i, x += xstep) {
            int y = mapY(diffVal(arr, vec, i));
            int xi = static_cast<int>(x);
            if (bars)
              pmap.drawLine(xi, y0, xi, y);
            if (lines || markers)
              tmpPts[i - start] = QPointF(x, y);
          }
          if (lines)
            pmap.drawPolyline(tmpPts.constData(), count);
          if (markers) {
            QPen oldPen = pmap.pen();
            QPen markerPen(clr, 3);
            markerPen.setCapStyle(Qt::SquareCap);
            pmap.setPen(markerPen);
            pmap.drawPoints(tmpPts.constData(), count);
            pmap.setPen(oldPen);
          }
        }
      };

      if (displaySet >= 0) {
        for (auto arr : arrayPtrs) {
          QColor clr = displayColor;
          if (clr == arr->color)
            clr = Qt::green;
          drawArray(arr, arr->saveVals[displaySet], clr);
        }
      }

      for (auto arr : arrayPtrs)
        drawArray(arr, arr->vals, arr->color);

      if (this == zoomPlot && nsect > 0 && stotal > 0.0 && !arrayPtrs.isEmpty() &&
          arrayPtrs[0]->nvals > 0) {
      pmap.setPen(Qt::black);
      pmap.setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
      int bottom = plotRect.bottom();
      int nvals = arrayPtrs[0]->nvals;
      int start = wrapIndex(area->xStart, nvals);
      int end = wrapIndex(area->xEnd, nvals);
      int span = (end >= start) ? (end - start + 1) :
        (nvals - start + end + 1);
      double smin, smax;
      if (span >= nvals) {
        smin = 0.0;
        smax = stotal;
      } else {
        smin = arrayPtrs[0]->s[start];
        smax = arrayPtrs[0]->s[end] + (end < start ? stotal : 0.0);
      }
      double range = smax - smin;
      auto xfroms = [&](double sval) {
        double s = sval;
        if (s < smin)
          s += stotal;
        if (s > smax)
          s -= stotal;
        return plotRect.left() + ((s - smin) / range) * plotRect.width();
      };
      int bunit = static_cast<int>(-0.015 * plotRect.height());
      for (int i = 0; i < latS.size(); ++i) {
        double s = latS[i];
        double sadd = s;
        if (sadd < smin)
          sadd += stotal;
        if (sadd > smax)
          sadd -= stotal;
        if (sadd >= smin && sadd <= smax) {
          int x1 = static_cast<int>(xfroms(s));
          int unit = latHeight[i] * bunit;
          pmap.drawLine(x1, y0, x1, y0 + unit);
          if (latLen[i] > 0) {
            double s2 = s + latLen[i];
            int x2 = static_cast<int>(xfroms(s2));
            if (x2 >= x1) {
              pmap.drawLine(x1, y0 + unit, x2, y0 + unit);
            } else {
              pmap.drawLine(x1, y0 + unit, plotRect.right(), y0 + unit);
              pmap.drawLine(plotRect.left(), y0 + unit, x2, y0 + unit);
            }
            pmap.drawLine(x2, y0 + unit, x2, y0);
          }
        }
      }
      for (int i = 0; i < nsect; ++i) {
        double snumber = ((i + 0.5) * stotal) / nsect;
        if (snumber < smin)
          snumber += stotal;
        if (snumber > smax)
          snumber -= stotal;
        if (snumber > smin && snumber < smax) {
          double frac = (snumber - smin) / range;
          int x = static_cast<int>(plotRect.left() + frac * plotRect.width());
          pmap.drawLine(x, bottom, x, bottom - 2);
          QString label = QString::number(i + 1);
          QRect tr(x - 10, bottom - 15, 20, 16);
          pmap.drawText(tr, Qt::AlignTop | Qt::AlignHCenter, label);
        }
      }
    }

    QPainter pw(this);
    pw.drawPixmap(0, 0, pixmap);
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      QRect plotRect = makePlotRect(width(), height());
      if (!plotRect.contains(event->pos()) || arrayPtrs.isEmpty()) {
        QWidget::mousePressEvent(event);
        return;
      }
      int nvals = arrayPtrs[0]->nvals;
      if (nvals < 1) {
        QWidget::mousePressEvent(event);
        return;
      }
      int nmid = 0;
      if (this == zoomPlot) {
        int start = wrapIndex(area->xStart, nvals);
        int end = wrapIndex(area->xEnd, nvals);
        int count = (end >= start) ?
          (end - start + 1) : (nvals - start + end + 1);
        double smin, smax, range;
        if (count >= nvals) {
          smin = 0.0;
          smax = stotal;
        } else {
          smin = arrayPtrs[0]->s[start];
          smax = arrayPtrs[0]->s[end] + (end < start ? stotal : 0.0);
        }
        range = smax - smin;
        double frac = (event->pos().x() - plotRect.left()) /
          static_cast<double>(plotRect.width());
        double sval = smin + frac * range;
        double best = std::fabs(arrayPtrs[0]->s[start] - sval);
        nmid = start;
        for (int i = 1; i < count; ++i) {
          int idx = (start + i) % nvals;
          double s = arrayPtrs[0]->s[idx];
          if (idx < start)
            s += stotal;
          double diff = std::fabs(s - sval);
          if (diff < best) {
            best = diff;
            nmid = idx;
          }
        }
      } else {
        double xstep = nvals > 1 ? plotRect.width() /
          static_cast<double>(nvals - 1) : 0.0;
        nmid = static_cast<int>((event->pos().x() - plotRect.left()) /
          xstep + 0.5);
      }
      QString info;
      for (auto arr : arrayPtrs) {
        info += arr->heading + "\n";
        for (int off = -1; off <= 1; ++off) {
          int idx = wrapIndex(nmid + off, arr->nvals);
          double val = arr->scaleFactor * arr->vals[idx];
          QString line = QString("%1%2 %3  %4\n")
            .arg(idx == nmid ? "->" : "  ")
            .arg(idx + 1)
            .arg(arr->names[idx])
            .arg(val, 7, 'f', 3);
          info += line;
        }
      }
      if (infoBox)
        infoBox->close();
      infoBox = new QMessageBox(QMessageBox::Information, "Information",
        info, QMessageBox::NoButton, this);
      infoBox->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
      infoBox->setAttribute(Qt::WA_DeleteOnClose);
      infoBox->setModal(false);
      infoBox->show();
    } else if (event->button() == Qt::RightButton) {
      if (this == zoomPlot && zoomAreaPtr && !arrayPtrs.isEmpty()) {
        QRect plotRect = makePlotRect(width(), height());
        if (!plotRect.contains(event->pos())) {
          QWidget::mousePressEvent(event);
          return;
        }
        rightDown = true;
        lastRightX = event->pos().x();
        dragRemainder = 0.0;
      } else {
        QWidget::mousePressEvent(event);
      }
    } else if (event->button() == Qt::MiddleButton) {
      if (zoomPlot && zoomAreaPtr && this != zoomPlot && !arrayPtrs.isEmpty()) {
        QRect plotRect = makePlotRect(width(), height());
        if (!plotRect.contains(event->pos())) {
          QWidget::mousePressEvent(event);
          return;
        }
        int nvals = arrayPtrs[0]->nvals;
        if (nvals < 1) {
          QWidget::mousePressEvent(event);
          return;
        }
        double xstep = nvals > 1 ? plotRect.width() /
          static_cast<double>(nvals - 1) : 0.0;
        int idx = static_cast<int>((event->pos().x() - plotRect.left()) /
          xstep + 0.5);
        int span = (zoomAreaPtr->xEnd >= zoomAreaPtr->xStart) ?
          (zoomAreaPtr->xEnd - zoomAreaPtr->xStart + 1) :
          (nvals - zoomAreaPtr->xStart + zoomAreaPtr->xEnd + 1);
        if (span < 1)
          span = 1;
        int half = span / 2;
        int start = wrapIndex(idx - half, nvals);
        int end = wrapIndex(start + span - 1, nvals);
        zoomAreaPtr->xStart = start;
        zoomAreaPtr->xEnd = end;
        zoomPlot->update();
        updateZoomCenter();
      } else {
        QWidget::mousePressEvent(event);
      }
    } else {
      QWidget::mousePressEvent(event);
    }
  }

  void wheelEvent(QWheelEvent *event) override
  {
    if (this != zoomPlot || !zoomAreaPtr || arrayPtrs.isEmpty()) {
      QWidget::wheelEvent(event);
      return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QPoint pos = event->position().toPoint();
#else
    QPoint pos = event->pos();
#endif
    QRect plotRect = makePlotRect(width(), height());
    if (!plotRect.contains(pos)) {
      QWidget::wheelEvent(event);
      return;
    }
    int nvals = arrayPtrs[0]->nvals;
    if (nvals < 1) {
      QWidget::wheelEvent(event);
      return;
    }
    int start = wrapIndex(zoomAreaPtr->xStart, nvals);
    int end = wrapIndex(zoomAreaPtr->xEnd, nvals);
    int span = (end >= start) ?
      (end - start + 1) : (nvals - start + end + 1);
    if (span < 1)
      span = 1;

    double frac = (pos.x() - plotRect.left()) /
      static_cast<double>(plotRect.width());
    if (frac < 0.0)
      frac = 0.0;
    if (frac > 1.0)
      frac = 1.0;
    int pivot = wrapIndex(start +
      static_cast<int>(frac * (span - 1) + 0.5), nvals);

    int steps = event->angleDelta().y() / 120;
    if (steps != 0) {
      double factor = std::pow(1.2, steps);
      int newSpan = static_cast<int>(span / factor + 0.5);
      if (newSpan < 1)
        newSpan = 1;
      if (newSpan > nvals)
        newSpan = nvals;
      int offset = static_cast<int>(frac * (newSpan - 1) + 0.5);
      int newStart = wrapIndex(pivot - offset, nvals);
      int newEnd = wrapIndex(newStart + newSpan - 1, nvals);
      zoomAreaPtr->xStart = newStart;
      zoomAreaPtr->xEnd = newEnd;
      updateZoomCenter();
      updateZoomInterval();
      update();
    } else {
      QWidget::wheelEvent(event);
    }
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (!rightDown || this != zoomPlot || !zoomAreaPtr || arrayPtrs.isEmpty()) {
      QWidget::mouseMoveEvent(event);
      return;
    }
    QRect plotRect = makePlotRect(width(), height());
    if (!plotRect.contains(event->pos()) || plotRect.width() < 1) {
      QWidget::mouseMoveEvent(event);
      return;
    }
    int dx = event->pos().x() - lastRightX;
    if (dx == 0) {
      QWidget::mouseMoveEvent(event);
      return;
    }
    int nvals = arrayPtrs[0]->nvals;
    if (nvals < 1) {
      QWidget::mouseMoveEvent(event);
      return;
    }
    int start = wrapIndex(zoomAreaPtr->xStart, nvals);
    int end = wrapIndex(zoomAreaPtr->xEnd, nvals);
    int span = (end >= start) ? (end - start + 1) : (nvals - start + end + 1);
    if (span < 1)
      span = 1;
    double delta = -dx * (span / static_cast<double>(plotRect.width()));
    dragRemainder += delta;
    int shift = static_cast<int>(dragRemainder);
    if (shift != 0) {
      dragRemainder -= shift;
      start = wrapIndex(start + shift, nvals);
      end = wrapIndex(start + span - 1, nvals);
      zoomAreaPtr->xStart = start;
      zoomAreaPtr->xEnd = end;
      updateZoomCenter();
      updateZoomInterval();
      update();
    }
    lastRightX = event->pos().x();
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton && infoBox) {
      if (makePlotRect(width(), height()).contains(event->pos()))
        infoBox->close();
      else
        QWidget::mouseReleaseEvent(event);
    } else if (event->button() == Qt::RightButton) {
      rightDown = false;
      QWidget::mouseReleaseEvent(event);
    } else {
      QWidget::mouseReleaseEvent(event);
    }
  }

private:
  AreaData *area;
  QVector<ArrayData *> arrayPtrs;
  QPointer<QMessageBox> infoBox;
  QPixmap pixmap;
  QVector<QPointF> tmpPts;
  bool rightDown = false;
  int lastRightX = 0;
  double dragRemainder = 0.0;
};

/**
 * @brief Combines scale and center controls with a plot widget.
 */
class AreaWidget : public QWidget
{
public:
  AreaWidget(AreaData *adata, const QVector<ArrayData *> &arrays,
    QWidget *parent = nullptr, bool showTitleBox = true,
    bool isZoomPlot = false)
    : QWidget(parent), area(adata), arrayPtrs(arrays)
  {
    auto vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout *titleBox = nullptr;
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QFontMetrics fm(fixedFont);
    int statWidth = fm.horizontalAdvance(" 000.000");
    if (showTitleBox) {
      titleBox = new QVBoxLayout;
      vbox->addLayout(titleBox);
      for (auto arr : arrayPtrs) {
        auto row = new QHBoxLayout;
        titleBox->addLayout(row);
        auto colorLabel = new QLabel;
        colorLabel->setFixedSize(12, 12);
        colorLabel->setStyleSheet(QString("background-color:%1;")
          .arg(arr->color.name()));
        row->addWidget(colorLabel);
        auto headLabel = new QLabel;
        QString head = arr->heading;
        if (!arr->units.isEmpty())
          head += QString(" (%1)").arg(arr->units);
        headLabel->setText(head);
        headLabel->setFont(fixedFont);
        row->addWidget(headLabel);
        row->addStretch();
        auto sdevText = new QLabel("SDEV:");
        sdevText->setFont(fixedFont);
        row->addWidget(sdevText);
        auto sdevLabel = new QLabel;
        sdevLabel->setFont(fixedFont);
        sdevLabel->setAlignment(Qt::AlignRight);
        sdevLabel->setFixedWidth(statWidth);
        row->addWidget(sdevLabel);
        auto avgText = new QLabel("   AVG:");
        avgText->setFont(fixedFont);
        row->addWidget(avgText);
        auto avgLabel = new QLabel;
        avgLabel->setFont(fixedFont);
        avgLabel->setAlignment(Qt::AlignRight);
        avgLabel->setFixedWidth(statWidth);
        row->addWidget(avgLabel);
        auto maxText = new QLabel("   MAX:");
        maxText->setFont(fixedFont);
        row->addWidget(maxText);
        auto maxLabel = new QLabel;
        maxLabel->setFont(fixedFont);
        maxLabel->setAlignment(Qt::AlignRight);
        maxLabel->setFixedWidth(statWidth);
        row->addWidget(maxLabel);
        StatLabels sl{ sdevLabel, avgLabel, maxLabel };
        stats.append(sl);
      }
    }

    auto controls = new QHBoxLayout;
    vbox->addLayout(controls);

    auto scaleLabel = new QLabel("Scale Interval:");
    controls->addWidget(scaleLabel);
    scaleSpin = new QDoubleSpinBox;
    scaleSpin->setDecimals(3);
    scaleSpin->setRange(scale[0], scale[NSCALES - 1]);
    scaleSpin->setValue(scale[area->currScale]);
    double step = (area->currScale > 0) ?
      scale[area->currScale] - scale[area->currScale - 1] :
      (NSCALES > 1 ? scale[1] - scale[0] : scale[0]);
    scaleSpin->setSingleStep(step);
    controls->addWidget(scaleSpin);

    auto centerLabel = new QLabel("Scale Center:");
    controls->addWidget(centerLabel);
    centerSpin = new QDoubleSpinBox;
    centerSpin->setDecimals(3);
    centerSpin->setRange(-LARGEVAL, LARGEVAL);
    centerSpin->setValue(area->centerVal);
    centerSpin->setSingleStep(scaleSpin->value());
    controls->addWidget(centerSpin);
    if (isZoomPlot) {
      auto intervalLabel = new QLabel("Visible Sectors:");
      controls->addWidget(intervalLabel);
      intervalSpin = new QSpinBox;
      int max = nsect > 0 ? nsect : 1;
      intervalSpin->setRange(1, max);
      int def = std::min(3, max);
      intervalSpin->setValue(def);
      controls->addWidget(intervalSpin);
      auto centerSectLabel = new QLabel("Center Sector:");
      controls->addWidget(centerSectLabel);
      centerSectSpin = new QSpinBox;
      centerSectSpin->setRange(1, max);
      centerSectSpin->setValue(1);
      centerSectSpin->setWrapping(true);
      controls->addWidget(centerSectSpin);
    } else {
      intervalSpin = nullptr;
      centerSectSpin = nullptr;
    }
    controls->addStretch();

    plot = new PlotWidget(adata, arrays, this);
    vbox->addWidget(plot, 1);
    int plotHeight = 148 + 1;
    if (isZoomPlot)
      plotHeight += 50;
    plot->setMinimumHeight(plotHeight);
    int topHeight = controls->sizeHint().height();
    if (titleBox)
      topHeight += titleBox->sizeHint().height();
    setMinimumHeight(plotHeight + topHeight + 15);

    connect(scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
      this, [this](double val)
    {
      int iscale = 0;
      while (iscale < NSCALES && val > scale[iscale])
        ++iscale;
      if (iscale >= NSCALES)
        iscale = NSCALES - 1;
      area->currScale = iscale;
      area->xmax = area->centerVal + scale[iscale] * GRIDDIVISIONS;
      area->xmin = area->centerVal - scale[iscale] * GRIDDIVISIONS;
      scaleSpin->blockSignals(true);
      scaleSpin->setValue(scale[iscale]);
      double step = (iscale > 0) ?
        scale[iscale] - scale[iscale - 1] :
        (NSCALES > 1 ? scale[1] - scale[0] : scale[0]);
      scaleSpin->setSingleStep(step);
      centerSpin->setSingleStep(scale[iscale]);
      scaleSpin->blockSignals(false);
      schedulePlotUpdate();
    });

    connect(centerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
      this, [this](double val)
    {
      area->centerVal = val;
      area->xmax = area->centerVal + scale[area->currScale] * GRIDDIVISIONS;
      area->xmin = area->centerVal - scale[area->currScale] * GRIDDIVISIONS;
      schedulePlotUpdate();
    });

    if (intervalSpin) {
      connect(intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int val)
      {
        if (arrayPtrs.isEmpty())
          return;
        int nvals = arrayPtrs[0]->nvals;
        if (nvals < 1)
          return;
        int perSect = nsect > 0 ? std::max(1, nvals / nsect) : nvals;
        int span = perSect * val;
        if (span > nvals)
          span = nvals;
        int oldSpan = (area->xEnd >= area->xStart) ?
          (area->xEnd - area->xStart + 1) :
          (nvals - area->xStart + area->xEnd + 1);
        int center = wrapIndex(area->xStart + oldSpan / 2, nvals);
        int start = wrapIndex(center - span / 2, nvals);
        int end = wrapIndex(start + span - 1, nvals);
        area->xStart = start;
        area->xEnd = end;
        schedulePlotUpdate();
        updateCenterSectSpin();
      });
      connect(centerSectSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int val)
      {
        if (arrayPtrs.isEmpty() || nsect <= 0 || stotal <= 0.0)
          return;
        int nvals = arrayPtrs[0]->nvals;
        if (nvals < 1)
          return;
        double sectLen = stotal / nsect;
        double target = (val - 0.5) * sectLen;
        int center = 0;
        double best = std::numeric_limits<double>::max();
        for (int i = 0; i < nvals; ++i) {
          double s = std::fmod(arrayPtrs[0]->s[i], stotal);
          if (s < 0)
            s += stotal;
          double diff = std::fabs(s - target);
          if (diff < best) {
            best = diff;
            center = i;
          }
        }
        int span = (area->xEnd >= area->xStart) ?
          (area->xEnd - area->xStart + 1) :
          (nvals - area->xStart + area->xEnd + 1);
        int start = wrapIndex(center - span / 2, nvals);
        int end = wrapIndex(start + span - 1, nvals);
        area->xStart = start;
        area->xEnd = end;
        schedulePlotUpdate();
        updateCenterSectSpin();
      });
      updateCenterSectSpin();
    }

    updateStats();
    setMinimumWidth(800);
  }

  PlotWidget *plotWidget() const
  {
    return plot;
  }

  void refresh()
  {
    updateStats();
    updateCenterSectSpin();
    updateIntervalSpin();
    schedulePlotUpdate();
  }

  void updateCenterSectSpin()
  {
    if (!centerSectSpin || arrayPtrs.isEmpty() || nsect <= 0 || stotal <= 0.0)
      return;
    int nvals = arrayPtrs[0]->nvals;
    if (nvals < 1)
      return;
    int span = (area->xEnd >= area->xStart) ?
      (area->xEnd - area->xStart + 1) :
      (nvals - area->xStart + area->xEnd + 1);
    int center = (area->xStart + span / 2) % nvals;
    double s = std::fmod(arrayPtrs[0]->s[center], stotal);
    if (s < 0)
      s += stotal;
    int sect = static_cast<int>((s * nsect) / stotal) + 1;
    centerSectSpin->blockSignals(true);
    centerSectSpin->setValue(sect);
    centerSectSpin->blockSignals(false);
  }

  void updateIntervalSpin()
  {
    if (!intervalSpin || arrayPtrs.isEmpty())
      return;
    int nvals = arrayPtrs[0]->nvals;
    if (nvals < 1)
      return;
    int span = (area->xEnd >= area->xStart) ?
      (area->xEnd - area->xStart + 1) :
      (nvals - area->xStart + area->xEnd + 1);
    int perSect = nsect > 0 ? std::max(1, nvals / nsect) : nvals;
    int val = (span + perSect - 1) / perSect;
    intervalSpin->blockSignals(true);
    intervalSpin->setValue(val);
    intervalSpin->blockSignals(false);
  }

  /**
   * @brief Set initial zoom parameters.
   *
   * @param centerSect Sector to center the zoom on (1-based).
   * @param interval Number of sectors to display.
   */
  void setZoom(int centerSect, int interval)
  {
    if (intervalSpin && interval > 0)
      intervalSpin->setValue(interval);
    if (centerSectSpin && centerSect > 0)
      centerSectSpin->setValue(centerSect);
  }

private:
  /**
   * @brief Queue a plot redraw on the next event loop cycle.
   */
  void schedulePlotUpdate()
  {
    if (plotUpdatePending)
      return;
    plotUpdatePending = true;
    QTimer::singleShot(0, this, [this]()
    {
      plotUpdatePending = false;
      plot->update();
    });
  }

  /**
   * @brief Update statistics labels for displayed arrays.
   */
  void updateStats()
  {
    for (int i = 0; i < arrayPtrs.size() && i < stats.size(); ++i) {
      auto arr = arrayPtrs[i];
      auto sl = stats[i];
      double sdevVal = (statMode && nstat > 0)
        ? arr->runSdev / nstat : arr->sdev;
      double avgVal = (statMode && nstat > 0)
        ? arr->runAvg / nstat : arr->avg;
      double maxVal = statMode ? arr->runMax : arr->maxVal;
      sl.sdev->setText(QString("%1").arg(
        sdevVal * arr->scaleFactor, 0, 'f', 3));
      sl.avg->setText(QString("%1").arg(
        avgVal * arr->scaleFactor, 0, 'f', 3));
      sl.max->setText(QString("%1").arg(
        maxVal * arr->scaleFactor, 0, 'f', 3));
    }
  }

  struct StatLabels
  {
    QLabel *sdev;
    QLabel *avg;
    QLabel *max;
  };

  AreaData *area;
  QVector<ArrayData *> arrayPtrs;
  PlotWidget *plot;
  QDoubleSpinBox *scaleSpin;
  QDoubleSpinBox *centerSpin;
  QSpinBox *intervalSpin;
  QSpinBox *centerSectSpin;
  QVector<StatLabels> stats;
  bool plotUpdatePending = false;
};

/**
 * @brief Synchronize the zoom plot center widget.
 */
static void updateZoomCenter()
{
  if (zoomAreaWidget)
    zoomAreaWidget->updateCenterSectSpin();
}

/**
 * @brief Synchronize the zoom plot interval widget.
 */
static void updateZoomInterval()
{
  if (zoomAreaWidget)
    zoomAreaWidget->updateIntervalSpin();
}

/**
 * @brief CA get callback for PV updates.
 *
 * @param args Channel Access event arguments.
 */
static void getCallback(struct event_handler_args args)
{
  GetCallbackData *cb = static_cast<GetCallbackData *>(args.usr);
  if (!cb || !cb->arr)
    return;
  if (args.status == ECA_NORMAL && args.dbr) {
    cb->arr->vals[cb->index] = *static_cast<const double *>(args.dbr);
    cb->arr->conn[cb->index] = true;
  } else {
    cb->arr->vals[cb->index] = 0.0;
    cb->arr->conn[cb->index] = false;
  }
}

/**
 * @brief Main application window for ADT.
 */
class MainWindow : public QMainWindow
{
public:
  explicit MainWindow(const QString &homeOverride, bool homeSpecified,
    const QString &startupPv, int zoomSect, int zoomInt,
    QWidget *parent = nullptr)
    : QMainWindow(parent), initZoomSector(zoomSect), initZoomInterval(zoomInt)
  {
    auto logo = new LogoWidget(this);
    setCentralWidget(logo);
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [this]() { pollPvUpdate(); });

    QMenu *fileMenu = menuBar()->addMenu("File");
    adtHome = homeOverride.isEmpty() ?
      qEnvironmentVariable("ADTHOME", ".") : homeOverride;
    QString initFile;
    if (homeSpecified) {
      initFile = QDir(adtHome).filePath(INITFILENAME);
    } else {
      initFile = QDir::home().filePath("." INITFILENAME);
      if (!QFile::exists(initFile))
        initFile = QDir(adtHome).filePath(INITFILENAME);
    }
    QString pvDir;
    QList<LoadMenuInfo> menus = readInitFile(initFile, adtHome, pvDir,
      customDirectory);
    pvDirectory = pvDir;
    QMenu *loadMenu = fileMenu->addMenu("Load");
    for (const auto &menu : menus) {
      QMenu *sub = loadMenu->addMenu(menu.title);
      for (const auto &item : menu.items) {
        QAction *act = sub->addAction(item.label);
        connect(act, &QAction::triggered, this, [this, item]() {
          loadPvFile(item.file);
        });
      }
    }
    loadMenu->addSeparator();
    QAction *customAct = loadMenu->addAction("Custom...");
    connect(customAct, &QAction::triggered, this, [this]() {
      QString fn = QFileDialog::getOpenFileName(this, "Choose PV File",
        customDirectory, "PV Files (*.pv)");
      if (!fn.isEmpty()) {
        customDirectory = QFileInfo(fn).absolutePath();
        loadPvFile(fn);
      }
    });
    if (!startupPv.isEmpty()) {
      QString file = startupPv;
      //if (QFileInfo(file).isRelative())
      //file = QDir(pvDir).filePath(file);
      loadPvFile(file);
    }
    QAction *readRefAct = fileMenu->addAction("Read Reference");
    connect(readRefAct, &QAction::triggered, this, [this]() { readReference(); });
    QMenu *readSnapMenu = fileMenu->addMenu("Read Snapshot");
    for (int i = 1; i <= NSAVE; ++i) {
      QAction *act = readSnapMenu->addAction(QString::number(i));
      connect(act, &QAction::triggered, this, [this, i]() { readSnapshot(i); });
    }
    QMenu *writeMenu = fileMenu->addMenu("Write");
    QAction *writeCurAct = writeMenu->addAction("Current");
    connect(writeCurAct, &QAction::triggered, this, [this]() { writeCurrent(); });
    for (int i = 1; i <= NSAVE; ++i) {
      QAction *act = writeMenu->addAction(QString::number(i));
      connect(act, &QAction::triggered, this, [this, i]() { writeSaved(i); });
    }
    QMenu *plotMenu = fileMenu->addMenu("Plot");
    QAction *plotCurAct = plotMenu->addAction("Current");
    connect(plotCurAct, &QAction::triggered, this, [this]() { plotCurrent(); });
    for (int i = 1; i <= NSAVE; ++i) {
      QAction *act = plotMenu->addAction(QString::number(i));
      connect(act, &QAction::triggered, this, [this, i]() { plotSaved(i); });
    }
    QAction *statusAct = fileMenu->addAction("Status...");
    connect(statusAct, &QAction::triggered, this, [this]() { showStatus(); });
    fileMenu->addSeparator();
    QAction *quitAct = fileMenu->addAction("Quit");
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    QMenu *optionsMenu = menuBar()->addMenu("Options");
    QMenu *storeMenu = optionsMenu->addMenu("Store");
    for (int i = 1; i <= NSAVE; ++i) {
      QAction *act = storeMenu->addAction(QString::number(i));
      connect(act, &QAction::triggered, this, [this, i]() { storeSet(i); });
    }
    QMenu *displayMenu = optionsMenu->addMenu("Display");
    QAction *displayOffAct = displayMenu->addAction("Off");
    connect(displayOffAct, &QAction::triggered, this, [this]() { displaySet(0); });
    for (int i = 1; i <= NSAVE; ++i) {
      QAction *act = displayMenu->addAction(QString::number(i));
      connect(act, &QAction::triggered, this, [this, i]() { displaySet(i); });
    }
    QMenu *diffMenu = optionsMenu->addMenu("Difference");
    QAction *diffOffAct = diffMenu->addAction("Off");
    connect(diffOffAct, &QAction::triggered, this, [this]() { diffSet(0); });
    for (int i = 1; i <= NSAVE; ++i) {
      QAction *act = diffMenu->addAction(QString::number(i));
      connect(act, &QAction::triggered, this, [this, i]() { diffSet(i); });
    }
    refAct = optionsMenu->addAction("Reference Enabled");
    refAct->setCheckable(true);
    refAct->setChecked(refOn);
    connect(refAct, &QAction::toggled, this, [this](bool checked)
    {
      refOn = checked;
      resetGraph();
    });
    zoomAct = optionsMenu->addAction("Zoom Enabled");
    zoomAct->setCheckable(true);
    zoomAct->setChecked(zoomOn);
    connect(zoomAct, &QAction::toggled, this, [this](bool checked)
    {
      zoomOn = checked;
      if (zoomWidget)
        zoomWidget->setVisible(zoomOn);
    });
    QAction *statAct = optionsMenu->addAction("Accumulated Statistics");
    statAct->setCheckable(true);
    statAct->setChecked(statMode);
    connect(statAct, &QAction::toggled, this, [this](bool checked)
    {
      statMode = checked;
      nstat = 0.0;
      nstatTime = 0.0;
      for (ArrayData &arr : arrays) {
        arr.runSdev = 0.0;
        arr.runAvg = 0.0;
        arr.runMax = 0.0;
      }
      for (AreaData &area : areas)
        area.tempclear = true;
      if (zoomWidget)
        zoomArea.tempclear = true;
      resetGraph();
    });
    QAction *resetAct = optionsMenu->addAction("Reset Max/Min");
    connect(resetAct, &QAction::triggered, this, [this]()
    {
      nstat = 0.0;
      nstatTime = 0.0;
      for (ArrayData &arr : arrays) {
        arr.minVals.fill(LARGEVAL);
        arr.maxVals.fill(-LARGEVAL);
        arr.runSdev = 0.0;
        arr.runAvg = 0.0;
        arr.runMax = 0.0;
      }
      for (AreaData &area : areas)
        area.tempclear = true;
      resetGraph();
    });

    QMenu *viewMenu = menuBar()->addMenu("View");
    QAction *timingAct = viewMenu->addAction("Timing...");
    connect(timingAct, &QAction::triggered, this, [this]()
    {
      bool ok = false;
      QString text = QInputDialog::getText(this, "Update Interval",
        "Enter interval between updates in ms:", QLineEdit::Normal,
        QString::number(timeInterval), &ok);
      if (ok) {
        bool okVal = false;
        int newVal = text.toInt(&okVal);
        if (okVal && newVal > 0) {
          timeInterval = newVal;
          if (pollTimer)
            pollTimer->start(timeInterval);
        } else {
          QMessageBox::warning(this, "ADT",
            QString("Invalid time value: %1").arg(text));
        }
      }
    });
    QAction *markersAct = viewMenu->addAction("Markers");
    markersAct->setCheckable(true);
    markersAct->setChecked(markers);
    connect(markersAct, &QAction::toggled, this, [this](bool checked)
    {
      markers = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    QAction *linesAct = viewMenu->addAction("Lines");
    linesAct->setCheckable(true);
    linesAct->setChecked(lines);
    connect(linesAct, &QAction::toggled, this, [this](bool checked)
    {
      lines = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    QAction *barsAct = viewMenu->addAction("Bars");
    barsAct->setCheckable(true);
    barsAct->setChecked(bars);
    connect(barsAct, &QAction::toggled, this, [this](bool checked)
    {
      bars = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    QAction *gridAct = viewMenu->addAction("Grid");
    gridAct->setCheckable(true);
    gridAct->setChecked(grid);
    connect(gridAct, &QAction::toggled, this, [this](bool checked)
    {
      grid = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    QAction *maxminAct = viewMenu->addAction("Max/Min");
    maxminAct->setCheckable(true);
    maxminAct->setChecked(showmaxmin);
    connect(maxminAct, &QAction::toggled, this, [this](bool checked)
    {
      showmaxmin = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    QAction *fillAct = viewMenu->addAction("Filled Max/Min");
    fillAct->setCheckable(true);
    fillAct->setChecked(fillmaxmin);
    connect(fillAct, &QAction::toggled, this, [this](bool checked)
    {
      fillmaxmin = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });

    QMenu *clearMenu = menuBar()->addMenu("Clear");
    QAction *clearAct = clearMenu->addAction("Clear");
    connect(clearAct, &QAction::triggered, this, [this]()
    {
      for (AreaData &area : areas) {
        area.tempnodraw = true;
        area.tempclear = true;
      }
      resetGraph();
    });
    QAction *redrawAct = clearMenu->addAction("Redraw");
    connect(redrawAct, &QAction::triggered, this, [this]()
    {
      for (AreaData &area : areas)
        area.tempclear = true;
      resetGraph();
    });
    QAction *updateAct = clearMenu->addAction("Update");
    connect(updateAct, &QAction::triggered, this, [this]()
    {
      resetGraph();
    });
    QAction *autoAct = clearMenu->addAction("Autoclear");
    autoAct->setCheckable(true);
    autoAct->setChecked(autoclear);
    connect(autoAct, &QAction::toggled, this, [](bool checked)
    {
      autoclear = checked;
    });

    QMenu *helpMenu = menuBar()->addMenu("Help");
    QAction *versionAct = helpMenu->addAction("Version");
    connect(versionAct, &QAction::triggered, this, [this]()
    {
      QString unameStr = QString("Running %1 %2 on %3")
        .arg(QSysInfo::kernelType())
        .arg(QSysInfo::kernelVersion())
        .arg(QSysInfo::machineHostName());
      QString msg = QString("%1 %2\n\nWritten by Dwarfs in the Waterfall Glen\n\n%3")
        .arg(ADT_VERSION_STRING)
        .arg(EPICS_VERSION_STRING)
        .arg(unameStr);
      QMessageBox::information(this, "ADT Version", msg);
    });

    QMenu *topMenus[] = { fileMenu, optionsMenu, viewMenu, clearMenu, helpMenu };
    for (QMenu *m : topMenus) {
      connect(m, &QMenu::aboutToShow, this, [this]()
      {
        if (pollTimer)
          pollTimer->stop();
      });
      connect(m, &QMenu::aboutToHide, this, [this]()
      {
        if (pollTimer)
          pollTimer->start(timeInterval);
      });
    }
  }

  ~MainWindow() override
  {
    for (chid ch : channels)
      ca_clear_channel(ch);
    if (caStarted)
      ca_context_destroy();
    if (pollTimer)
      pollTimer->stop();
    zoomAreaWidget = nullptr;
  }

  void storeSet(int n)
  {
    if (n < 1 || n > NSAVE || arrays.isEmpty())
      return;
    int idx = n - 1;
    for (ArrayData &arr : arrays)
      arr.saveVals[idx] = arr.vals;
    time_t now = std::time(nullptr);
    char tbuf[26];
    std::strncpy(tbuf, std::ctime(&now), 24);
    tbuf[24] = '\0';
    saveTime[idx] = QString::fromUtf8(tbuf);
    saveFilename[idx].clear();
    resetGraph();
  }

  void displaySet(int n)
  {
    if (n >= 1 && n <= NSAVE) {
      if (arrays.isEmpty()) {
        QMessageBox::warning(this, "ADT", "There are no PV's defined");
        return;
      }
      int idx = n - 1;
      for (const ArrayData &arr : arrays) {
        if (arr.saveVals[idx].size() != arr.nvals) {
          QMessageBox::warning(this, "ADT", "Data is not defined");
          return;
        }
      }
      ::displaySet = idx;
    } else {
      ::displaySet = -1;
    }
    resetGraph();
  }

  void diffSet(int n)
  {
    if (n >= 1 && n <= NSAVE) {
      if (::diffSet < 0) {
        for (AreaData &area : areas) {
          area.oldCenterVal = area.centerVal;
          area.centerVal = 0.0;
          area.tempclear = true;
        }
        if (zoomWidget) {
          zoomArea.oldCenterVal = zoomArea.centerVal;
          zoomArea.centerVal = 0.0;
          zoomArea.tempclear = true;
        }
      }
      ::diffSet = n - 1;
    } else {
      ::diffSet = -1;
      for (AreaData &area : areas) {
        area.centerVal = area.oldCenterVal;
        area.tempclear = true;
      }
      if (zoomWidget) {
        zoomArea.centerVal = zoomArea.oldCenterVal;
        zoomArea.tempclear = true;
      }
    }
    resetGraph();
  }

private:
  QString adtHome;
  QString customDirectory;
  QString pvDirectory;
  QString pvFilename;
  QString latFilename;
  QString refFilename;
  QString saveTime[NSAVE];
  QString saveFilename[NSAVE];
  QVector<ArrayData> arrays;
  QVector<AreaData> areas;
  QVector<AreaWidget *> areaWidgets;
  AreaWidget *zoomWidget = nullptr;
  QAction *refAct = nullptr;
  QAction *zoomAct = nullptr;
  AreaData zoomArea;
  bool zoomOn = true;
  int initZoomSector = 0;
  int initZoomInterval = 0;
  bool zoomSectorUsed = false;
  bool zoomIntervalUsed = false;
  QVector<chid> channels;
  QTimer *pollTimer = nullptr;
  int timeInterval = 2000;
  bool caStarted = false;
  int nsymbols = 0;

  void resetGraph()
  {
    for (auto aw : areaWidgets)
      aw->refresh();
  }

  void pollPvUpdate()
  {
    if (!caStarted)
      return;
    ca_poll();
    for (ArrayData &arr : arrays) {
      for (int i = 0; i < arr.nvals; ++i) {
        if (!arr.chids[i] || ca_state(arr.chids[i]) != cs_conn) {
          arr.conn[i] = false;
          arr.vals[i] = 0.0;
        }
      }
    }
    for (ArrayData &arr : arrays) {
      double sum = 0.0;
      double sumsq = 0.0;
      double maxv = 0.0;
      int nconn = 0;
      bool haveMax = false;
      for (int i = 0; i < arr.nvals; ++i) {
        if (!arr.conn[i])
          continue;
        double v = arr.vals[i];
        sum += v;
        sumsq += v * v;
        if (!haveMax || std::fabs(v) > std::fabs(maxv)) {
          maxv = v;
          haveMax = true;
        }
        if (v < arr.minVals[i])
          arr.minVals[i] = v;
        if (v > arr.maxVals[i])
          arr.maxVals[i] = v;
        ++nconn;
      }
      if (nconn > 0) {
        arr.avg = sum / nconn;
        arr.sdev = std::sqrt(sumsq / nconn - arr.avg * arr.avg);
        arr.maxVal = maxv;
      } else {
        arr.avg = arr.sdev = arr.maxVal = 0.0;
      }
    }
    nstat += 1.0;
    nstatTime += timeInterval;
    for (ArrayData &arr : arrays) {
      arr.runSdev += arr.sdev;
      arr.runAvg += arr.avg;
      if (std::fabs(arr.maxVal) > std::fabs(arr.runMax))
        arr.runMax = arr.maxVal;
    }
    for (auto aw : areaWidgets)
      aw->refresh();
    for (ArrayData &arr : arrays) {
      for (int i = 0; i < arr.nvals; ++i) {
        if (arr.chids[i] && ca_state(arr.chids[i]) == cs_conn)
          ca_array_get_callback(DBR_DOUBLE, 1, arr.chids[i], getCallback,
            &arr.cbData[i]);
      }
    }
    ca_flush_io();
  }

  void showStatus()
  {
    time_t now = std::time(nullptr);
    char tbuf[26];
    std::strncpy(tbuf, std::ctime(&now), 24);
    tbuf[24] = '\0';

    QString msg;
    msg += QString::asprintf("Time:         %s\n", tbuf);
    msg += QString::asprintf("PV file:      %s\n", pvFilename.toUtf8().constData());
    msg += QString::asprintf("Lattice file: %s\n", latFilename.toUtf8().constData());
    msg += QString::asprintf("Reference file: %s\n\n", refFilename.toUtf8().constData());
    msg += QString::asprintf(
      "Accumulated History Time (min): %.1f    Nsymbols=%d\n",
      nstatTime / 60000.0, nsymbols);
    msg += "Array Nvals    SDEV     AVG     MAX (Accumulated)\n";
    for (const ArrayData &arr : arrays) {
      msg += QString::asprintf("%5d %5d % 7.3f % 7.3f %7.3f\n",
        arr.index + 1, arr.nvals,
        nstat > 0 ? arr.runSdev / nstat : 0.0,
        nstat > 0 ? arr.runAvg / nstat : 0.0,
        arr.runMax);
    }
    msg += "\nSlot  Time                      File\n";
    for (int i = 0; i < NSAVE; ++i) {
      QByteArray st = saveTime[i].toUtf8();
      QByteArray sf = saveFilename[i].toUtf8();
      msg += QString::asprintf("%3d   %.24s  %s\n", i + 1,
        st.constData(), sf.constData());
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Status");
    QVBoxLayout layout(&dlg);
    QLabel label(msg);
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    label.setFont(font);
    label.setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout.addWidget(&label);
    QDialogButtonBox buttons(QDialogButtonBox::Ok);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    layout.addWidget(&buttons);
    dlg.exec();
  }

  bool writePlotFile(const QString &filename, int nsave = -1)
  {
    FILE *file = fopen(filename.toUtf8().constData(), "w");
    if (!file)
      return false;

    char tbuf[26];
    if (nsave < 0) {
      time_t now = std::time(nullptr);
      std::strncpy(tbuf, std::ctime(&now), 24);
      tbuf[24] = '\0';
    } else {
      if (nsave >= NSAVE || saveTime[nsave].isEmpty()) {
        fclose(file);
        QMessageBox::warning(this, "ADT",
          QString("Data is not defined for set %1").arg(nsave + 1));
        return false;
      }
      QByteArray st = saveTime[nsave].toUtf8();
      std::strncpy(tbuf, st.constData(), 24);
      tbuf[24] = '\0';
    }

    fprintf(file, "%s\n", SDDSID);
    fprintf(file, "&description text=\"ADT Plot from %s\" &end\n",
      pvFilename.toUtf8().constData());
    fprintf(file,
      "&parameter name=TimeStamp fixed_value=\"%s\" type=string &end\n",
      tbuf);
    fprintf(file,
      "&parameter name=ADTNArrays type=short fixed_value=%d &end\n",
      arrays.size() + 1);
    fprintf(file,
      "&parameter name=ADTNAreas type=short fixed_value=%d &end\n",
      areas.size() + 1);
    fprintf(file, "&parameter name=ADTHeading type=string &end\n");
    fprintf(file, "&parameter name=ADTUnits type=string &end\n");
    fprintf(file, "&parameter name=ADTScreenTitle type=string &end\n");
    fprintf(file, "&parameter name=ADTDisplayArea type=short &end\n");
    fprintf(file, "&column name=Index type=short &end\n");
    fprintf(file, "&column name=ControlName type=string &end\n");
    fprintf(file, "&column name=Value type=double &end\n");
    fprintf(file,
      "&data mode=ascii no_row_counts=1 additional_header_lines=1 &end\n");
    for (const ArrayData &arr : arrays) {
      const QVector<double> *vals = nullptr;
      if (nsave < 0) {
        vals = &arr.vals;
      } else {
        if (arr.saveVals[nsave].size() != arr.nvals) {
          fclose(file);
          QMessageBox::warning(this, "ADT",
            QString("Data is not defined for set %1").arg(nsave + 1));
          return false;
        }
        vals = &arr.saveVals[nsave];
      }
      fprintf(file, "\n");
      fprintf(file, "%s\n", arr.heading.toUtf8().constData());
      fprintf(file, "%s\n", arr.units.toUtf8().constData());
      fprintf(file, "%s (%s)\n", arr.heading.toUtf8().constData(),
        arr.units.toUtf8().constData());
      fprintf(file, "%d     !ADTDisplayArea\n", arr.area->index + 1);
      for (int i = 0; i < arr.nvals; ++i) {
        fprintf(file, "%d %s % f\n", i + 1,
          arr.names[i].toUtf8().constData(), (*vals)[i]);
      }
    }
    fclose(file);
    return true;
  }

  bool readRefFile(const QString &filename)
  {
    SDDS_TABLE table;
    QByteArray fname = filename.toUtf8();
    SDDS_ClearErrors();
    if (!SDDS_InitializeInput(&table, fname.data())) {
      QMessageBox::warning(this, "ADT",
        QString("Unable to read %1").arg(filename));
      if (SDDS_NumberOfErrors())
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return false;
    }
    if (SDDS_CheckColumn(&table, const_cast<char *>("ControlName"), NULL,
        SDDS_STRING, NULL) != SDDS_CHECK_OKAY ||
        SDDS_CheckColumn(&table, const_cast<char *>("ValueString"), NULL,
        SDDS_STRING, NULL) != SDDS_CHECK_OKAY) {
      QMessageBox::warning(this, "ADT",
        "Missing required columns in reference file");
      SDDS_Terminate(&table);
      return false;
    }
    for (int ia = 0; ia < arrays.size(); ++ia) {
      if (!SDDS_ReadTable(&table)) {
        QMessageBox::warning(this, "ADT",
          QString("Could not get Data for array %1 in %2")
            .arg(ia + 1).arg(filename));
        SDDS_Terminate(&table);
        return false;
      }
      if (ia == 0) {
        char *type = nullptr;
        if (!SDDS_GetParameter(&table, const_cast<char *>("ADTFileType"),
            &type) || strcmp(type, SNAPID)) {
          QMessageBox::warning(this, "ADT",
            QString("Not a valid ADT Reference/Snapshot file: %1")
              .arg(filename));
          SDDS_Free(type);
          SDDS_Terminate(&table);
          return false;
        }
        SDDS_Free(type);
      }
      int nvals = SDDS_CountRowsOfInterest(&table);
      if (nvals != arrays[ia].nvals) {
        QMessageBox::warning(this, "ADT",
          QString("Wrong number of items [%1] for array %2 in %3")
            .arg(nvals).arg(ia + 1).arg(filename));
        SDDS_Terminate(&table);
        return false;
      }
      char **rawnames = (char **)SDDS_GetColumn(&table,
        const_cast<char *>("ControlName"));
      if (!rawnames) {
        QMessageBox::warning(this, "ADT",
          QString("Could not get ControlName's for array %1 in %2")
            .arg(ia + 1).arg(filename));
        SDDS_Terminate(&table);
        return false;
      }
      char **rawvalues = (char **)SDDS_GetColumn(&table,
        const_cast<char *>("ValueString"));
      if (!rawvalues) {
        QMessageBox::warning(this, "ADT",
          QString("Could not get ValueString's for array %1 in %2")
            .arg(ia + 1).arg(filename));
        freeSddsStrings(nvals, rawnames);
        SDDS_Terminate(&table);
        return false;
      }
      arrays[ia].refVals.resize(arrays[ia].nvals);
      for (int i = 0; i < nvals; ++i) {
        if (QString(rawnames[i]) != arrays[ia].names[i]) {
          QMessageBox::warning(this, "ADT",
            QString("PV does not match: %1 from %2")
              .arg(rawnames[i]).arg(filename));
          freeSddsStrings(nvals, rawnames);
          freeSddsStrings(nvals, rawvalues);
          SDDS_Terminate(&table);
          return false;
        }
        arrays[ia].refVals[i] = atof(rawvalues[i]);
      }
      freeSddsStrings(nvals, rawnames);
      freeSddsStrings(nvals, rawvalues);
    }
    SDDS_Terminate(&table);
    if (SDDS_NumberOfErrors())
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return true;
  }

  bool readSnapFile(const QString &filename, int nsave)
  {
    SDDS_TABLE table;
    QByteArray fname = filename.toUtf8();
    SDDS_ClearErrors();
    if (!SDDS_InitializeInput(&table, fname.data())) {
      QMessageBox::warning(this, "ADT",
        QString("Unable to read %1").arg(filename));
      if (SDDS_NumberOfErrors())
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return false;
    }
    if (SDDS_CheckColumn(&table, const_cast<char *>("ControlName"), NULL,
        SDDS_STRING, NULL) != SDDS_CHECK_OKAY ||
        SDDS_CheckColumn(&table, const_cast<char *>("ValueString"), NULL,
        SDDS_STRING, NULL) != SDDS_CHECK_OKAY) {
      QMessageBox::warning(this, "ADT",
        "Missing required columns in snapshot file");
      SDDS_Terminate(&table);
      return false;
    }
    for (int ia = 0; ia < arrays.size(); ++ia) {
      if (!SDDS_ReadTable(&table)) {
        QMessageBox::warning(this, "ADT",
          QString("Could not get Data for array %1 in %2")
            .arg(ia + 1).arg(filename));
        SDDS_Terminate(&table);
        return false;
      }
      if (ia == 0) {
        char *type = nullptr;
        if (!SDDS_GetParameter(&table, const_cast<char *>("ADTFileType"),
            &type) || strcmp(type, SNAPID)) {
          QMessageBox::warning(this, "ADT",
            QString("Not a valid ADT Reference/Snapshot file: %1")
              .arg(filename));
          SDDS_Free(type);
          SDDS_Terminate(&table);
          return false;
        }
        SDDS_Free(type);
        char *time = nullptr;
        if (SDDS_GetParameter(&table, const_cast<char *>("TimeStamp"), &time)) {
          saveTime[nsave] = QString::fromUtf8(time);
          SDDS_Free(time);
        } else {
          saveTime[nsave].clear();
        }
      }
      int nvals = SDDS_CountRowsOfInterest(&table);
      if (nvals != arrays[ia].nvals) {
        QMessageBox::warning(this, "ADT",
          QString("Wrong number of items [%1] for array %2 in %3")
            .arg(nvals).arg(ia + 1).arg(filename));
        SDDS_Terminate(&table);
        return false;
      }
      char **rawnames = (char **)SDDS_GetColumn(&table,
        const_cast<char *>("ControlName"));
      if (!rawnames) {
        QMessageBox::warning(this, "ADT",
          QString("Could not get ControlName's for array %1 in %2")
            .arg(ia + 1).arg(filename));
        SDDS_Terminate(&table);
        return false;
      }
      char **rawvalues = (char **)SDDS_GetColumn(&table,
        const_cast<char *>("ValueString"));
      if (!rawvalues) {
        QMessageBox::warning(this, "ADT",
          QString("Could not get ValueString's for array %1 in %2")
            .arg(ia + 1).arg(filename));
        freeSddsStrings(nvals, rawnames);
        SDDS_Terminate(&table);
        return false;
      }
      arrays[ia].saveVals[nsave].resize(arrays[ia].nvals);
      for (int i = 0; i < nvals; ++i) {
        if (QString(rawnames[i]) != arrays[ia].names[i]) {
          QMessageBox::warning(this, "ADT",
            QString("PV does not match: %1 from %2")
              .arg(rawnames[i]).arg(filename));
          freeSddsStrings(nvals, rawnames);
          freeSddsStrings(nvals, rawvalues);
          SDDS_Terminate(&table);
          return false;
        }
        arrays[ia].saveVals[nsave][i] = atof(rawvalues[i]);
      }
      freeSddsStrings(nvals, rawnames);
      freeSddsStrings(nvals, rawvalues);
    }
    SDDS_Terminate(&table);
    if (SDDS_NumberOfErrors())
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    saveFilename[nsave] = filename;
    return true;
  }

  void readSnapshot(int n)
  {
    if (arrays.isEmpty()) {
      QMessageBox::warning(this, "ADT", "There are no PV's defined");
      return;
    }
    if (n < 1 || n > NSAVE)
      return;
    QString dir = QDir::currentPath();
    QString fn = QFileDialog::getOpenFileName(this, "Read Snapshot File", dir,
      "Snapshot Files (*.snap)");
    if (fn.isEmpty())
      return;
    if (!readSnapFile(fn, n - 1))
      return;
    resetGraph();
  }

  bool writeSnapFile(const QString &filename, int nsave = -1)
  {
    FILE *file = fopen(filename.toUtf8().constData(), "w");
    if (!file) {
      QMessageBox::warning(this, "ADT",
        QString("Unable to open %1").arg(filename));
      return false;
    }

    char tbuf[26];
    if (nsave < 0) {
      time_t clock = std::time(nullptr);
      std::strncpy(tbuf, std::ctime(&clock), 24);
      tbuf[24] = '\0';
    } else {
      if (nsave >= NSAVE || saveTime[nsave].isEmpty()) {
        fclose(file);
        QMessageBox::warning(this, "ADT",
          QString("Data is not defined for set %1").arg(nsave + 1));
        return false;
      }
      QByteArray st = saveTime[nsave].toUtf8();
      std::strncpy(tbuf, st.constData(), 24);
      tbuf[24] = '\0';
    }

    fprintf(file, "%s\n", SDDSID);
    fprintf(file,
      "&description text=\"ADT Snapshot from %s\" contents=\"BURT\" &end\n",
      pvFilename.toUtf8().constData());
    fprintf(file,
      "&parameter name=ADTFileType fixed_value=\"%s\" type=string &end\n",
      SNAPID);
    fprintf(file,
      "&parameter name=TimeStamp fixed_value=\"%s\" type=string &end\n",
      tbuf);
    fprintf(file,
      "&parameter name=SnapType fixed_value=\"Absolute\" type=string &end\n");
    fprintf(file, "&parameter name=Heading type=string &end\n");
    fprintf(file, "&column name=ControlName type=string &end\n");
    fprintf(file, "&column name=ControlType type=string &end\n");
    fprintf(file, "&column name=Lineage type=string &end\n");
    fprintf(file, "&column name=Count type=long &end\n");
    fprintf(file, "&column name=ValueString type=string &end\n");
    fprintf(file,
      "&data mode=ascii no_row_counts=1 additional_header_lines=1 &end\n");
    for (const ArrayData &arr : arrays) {
      const QVector<double> *vals = nullptr;
      if (nsave < 0) {
        vals = &arr.vals;
      } else {
        if (arr.saveVals[nsave].size() != arr.nvals) {
          fclose(file);
          QMessageBox::warning(this, "ADT",
            QString("Data is not defined for set %1").arg(nsave + 1));
          return false;
        }
        vals = &arr.saveVals[nsave];
      }
      fprintf(file, "\n");
      fprintf(file, "%s (%s)\n", arr.heading.toUtf8().constData(),
        arr.units.toUtf8().constData());
      for (int i = 0; i < arr.nvals; ++i) {
        fprintf(file, "%s pv - 1 %f\n",
          arr.names[i].toUtf8().constData(), (*vals)[i]);
      }
    }
    fclose(file);
    return true;
  }

  void readReference()
  {
    if (arrays.isEmpty()) {
      QMessageBox::warning(this, "ADT", "There are no PV's defined");
      return;
    }
    QString dir = QDir::currentPath();
    QString fn = QFileDialog::getOpenFileName(this,
      "Read Reference File", dir, "Snapshot Files (*.snap)");
    if (fn.isEmpty())
      return;
    if (!readRefFile(fn)) {
      QMessageBox::warning(this, "ADT",
        "Could not read reference data\nReferencing is inactive");
      referenceLoaded = false;
      return;
    }
    referenceLoaded = true;
    refFilename = fn;
    resetGraph();
  }

  /**
   * @brief Write current array data to a snapshot file.
   */
  void writeCurrent()
  {
    if (arrays.isEmpty()) {
      QMessageBox::warning(this, "ADT", "There are no PV's defined");
      return;
    }
    QString dir = QDir::currentPath();
    QString fn = QFileDialog::getSaveFileName(this, "Write Snapshot File",
      dir, "Snapshot Files (*.snap)");
    if (fn.isEmpty())
      return;
    writeSnapFile(fn);
  }

  void writeSaved(int n)
  {
    if (arrays.isEmpty()) {
      QMessageBox::warning(this, "ADT", "There are no PV's defined");
      return;
    }
    if (n < 1 || n > NSAVE)
      return;
    int idx = n - 1;
    QString dir = QDir::currentPath();
    QString fn = QFileDialog::getSaveFileName(this, "Write Snapshot File",
      dir, "Snapshot Files (*.snap)");
    if (fn.isEmpty())
      return;
    writeSnapFile(fn, idx);
  }

  void plotCurrent()
  {
    if (arrays.isEmpty()) {
      QMessageBox::warning(this, "ADT", "There are no PV's defined");
      return;
    }
    QTemporaryFile tmp(QDir::tempPath() + "/ADTPlotXXXXXX.sdds");
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
      QMessageBox::warning(this, "ADT", "Unable to create temp file");
      return;
    }
    QString fname = tmp.fileName();
    tmp.close();
    if (!writePlotFile(fname)) {
      QFile::remove(fname);
      return;
    }
    QString cmd = QString(
      "sddsplot -graph=sym,connect -layout=1,2 -split=page -sep "
      "-ylabel=@TimeStamp -xlabel=Index -column=Index,Value \"%1\" "
      "-title=@ADTScreenTitle -end ; rm \"%1\"").arg(fname);
    QProcess::startDetached("/bin/sh", QStringList() << "-c" << cmd);
  }

  void plotSaved(int n)
  {
    if (arrays.isEmpty()) {
      QMessageBox::warning(this, "ADT", "There are no PV's defined");
      return;
    }
    if (n < 1 || n > NSAVE)
      return;
    int idx = n - 1;
    for (const ArrayData &arr : arrays) {
      if (arr.saveVals[idx].size() != arr.nvals) {
        QMessageBox::warning(this, "ADT",
          QString("Data is not defined for set %1").arg(n));
        return;
      }
    }
    QTemporaryFile tmp(QDir::tempPath() + "/ADTPlotXXXXXX.sdds");
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
      QMessageBox::warning(this, "ADT", "Unable to create temp file");
      return;
    }
    QString fname = tmp.fileName();
    tmp.close();
    if (!writePlotFile(fname, idx)) {
      QFile::remove(fname);
      return;
    }
    QString cmd = QString(
      "sddsplot -graph=sym,connect -layout=1,2 -split=page -sep "
      "-ylabel=@TimeStamp -xlabel=Index -column=Index,Value \"%1\" "
      "-title=@ADTScreenTitle -end ; rm \"%1\"").arg(fname);
    QProcess::startDetached("/bin/sh", QStringList() << "-c" << cmd);
  }

  void loadPvFile(const QString &file)
  {
    pvFilename = file;
    if (pollTimer)
      pollTimer->stop();
    timeInterval = 2000;

    if (caStarted) {
      for (chid ch : channels)
        ca_clear_channel(ch);
      channels.clear();
      ca_context_destroy();
      caStarted = false;
    }

    arrays.clear();
    areas.clear();
    areaWidgets.clear();
    zoomWidget = nullptr;
    zoomAreaWidget = nullptr;
    nstat = 0.0;
    nstatTime = 0.0;
    for (int i = 0; i < NSAVE; ++i) {
      saveTime[i].clear();
      saveFilename[i].clear();
    }
    refFilename.clear();
    referenceLoaded = false;

    if (ca_context_create(ca_disable_preemptive_callback) != ECA_NORMAL) {
      QMessageBox::warning(this, "ADT", "Unable to start Channel Access");
      return;
    }
    caStarted = true;

    SDDS_TABLE table;
    QByteArray fname = file.toUtf8();
    if (!SDDS_InitializeInput(&table, fname.data())) {
      QMessageBox::warning(this, "ADT", "Unable to read PV file:\n" + file);
      ca_context_destroy();
      caStarted = false;
      return;
    }

    bool first = true;
    bool oneAreaPerArray = false;
    int iarray;
    int narrays = 0;
    int nareas = 0;
    int32_t templong = 0;

    while ((iarray = SDDS_ReadTable(&table)) > 0) {
      iarray--;
      if (first) {
        char *type = NULL;
        if (!SDDS_GetParameter(&table, const_cast<char *>("ADTFileType"), &type) ||
            strcmp(type, PVID)) {
          QMessageBox::warning(this, "ADT", "Not a valid ADT PV file");
          SDDS_Free(type);
          SDDS_Terminate(&table);
          ca_context_destroy();
          caStarted = false;
          return;
        }
        SDDS_Free(type);
        if (!SDDS_GetParameterAsLong(&table, const_cast<char *>("ADTNArrays"),
            &templong)) {
          QMessageBox::warning(this, "ADT", "Missing ADTNArrays parameter");
          SDDS_Terminate(&table);
          ca_context_destroy();
          caStarted = false;
          return;
        }
        narrays = templong;
        if (!SDDS_GetParameterAsLong(&table, const_cast<char *>("ADTNAreas"),
            &templong)) {
          oneAreaPerArray = true;
          templong = narrays;
        }
        nareas = templong;
        arrays.resize(narrays);
        areas.resize(nareas);
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTTimeInterval"), &templong))
          timeInterval = static_cast<int>(templong);
        char *latfile = NULL;
        if (SDDS_GetParameter(&table, const_cast<char *>("ADTLatticeFile"),
            &latfile) && latfile) {
          QString lf = latfile;
          if (!lf.contains('/'))
            lf = QDir(pvDirectory).filePath(lf);
          latFilename = lf;
          if (!readLatticeFile(lf, nsect, stotal)) {
            nsect = 0;
            stotal = 0.0;
          }
          SDDS_Free(latfile);
        } else {
          nsect = 0;
          stotal = 0.0;
          latNames.clear();
          latS.clear();
          latLen.clear();
          latHeight.clear();
        }
        first = false;
      }

      if (iarray >= narrays)
        continue;

      ArrayData &arr = arrays[iarray];
      arr.index = iarray;

      char *strptr = NULL;
      if (!SDDS_GetParameter(&table, const_cast<char *>("ADTColor"), &strptr)) {
        arr.color = QColor(defaultColors[iarray % NCOLORS]);
      } else {
        arr.color = QColor(strptr);
        SDDS_Free(strptr);
      }
      if (!arr.color.isValid())
        arr.color = Qt::black;

      char *heading = NULL;
      if (SDDS_GetParameter(&table, const_cast<char *>("ADTHeading"), &heading) && heading) {
        arr.heading = heading;
        SDDS_Free(heading);
      } else {
        arr.heading = QString("Array %1").arg(iarray + 1);
      }

      char *units = NULL;
      if (SDDS_GetParameter(&table, const_cast<char *>("ADTUnits"), &units) && units) {
        arr.units = units;
        SDDS_Free(units);
      } else {
        arr.units.clear();
      }

      double sf;
      if (SDDS_GetParameterAsDouble(&table, const_cast<char *>("ADTScaleFactor"), &sf))
        arr.scaleFactor = sf;
      else
        arr.scaleFactor = 1.0;

      if (SDDS_GetParameterAsLong(&table,
          const_cast<char *>("ADTZoomArea"), &templong))
        arr.zoom = templong != 0;
      else
        arr.zoom = false;

      int iarea;
      if (oneAreaPerArray)
        iarea = iarray;
      else {
        if (SDDS_GetParameterAsLong(&table, const_cast<char *>("ADTDisplayArea"), &templong))
          iarea = static_cast<int>(templong) - 1;
        else
          iarea = iarray;
      }
      if (iarea < 0 || iarea >= nareas)
        iarea = 0;
      arr.area = &areas[iarea];
      AreaData &area = areas[iarea];
      if (!area.initialized) {
        area.index = iarea;
        double center;
        if (!SDDS_GetParameterAsDouble(&table, const_cast<char *>("ADTCenterVal"), &center))
          center = 0.0;
        area.centerVal = center;
        area.oldCenterVal = area.centerVal;
        double upd;
        if (!SDDS_GetParameterAsDouble(&table, const_cast<char *>("ADTUnitsPerDiv"), &upd))
          upd = 1.0;
        int iscale = 0;
        while (iscale < NSCALES && upd > scale[iscale])
          ++iscale;
        if (iscale >= NSCALES)
          iscale = NSCALES - 1;
        area.currScale = iscale;
        area.xmax = area.centerVal + scale[iscale] * GRIDDIVISIONS;
        area.xmin = area.centerVal - scale[iscale] * GRIDDIVISIONS;
        area.initialized = true;
      }

      int rows = SDDS_CountRowsOfInterest(&table);
      arr.nvals = rows;
      arr.names.clear();
      arr.vals.fill(0.0, rows);
      arr.s.fill(0.0, rows);
      arr.minVals.fill(LARGEVAL, rows);
      arr.maxVals.fill(-LARGEVAL, rows);
      arr.conn.fill(false, rows);
      arr.chids.clear();
      arr.cbData.resize(rows);

      char **names = (char **)SDDS_GetColumn(&table, const_cast<char *>("ControlName"));
      if (!names) {
        QMessageBox::warning(this, "ADT", "PV file missing ControlName column");
        SDDS_Terminate(&table);
        ca_context_destroy();
        caStarted = false;
        return;
      }
      for (int i = 0; i < rows; ++i) {
        arr.names.append(names[i]);
        chid ch;
        int status = ca_create_channel(names[i], NULL, NULL, 0, &ch);
        if (status == ECA_NORMAL) {
          channels.append(ch);
          arr.chids.append(ch);
        } else {
          arr.chids.append(0);
        }
        arr.cbData[i].arr = &arr;
        arr.cbData[i].index = i;
        SDDS_Free(names[i]);
      }
      SDDS_Free(names);
      if (!latS.isEmpty()) {
        int jstart = 0;
        for (int i = 0; i < rows; ++i) {
          bool found = false;
          for (int j = 0; j < latNames.size(); ++j) {
            int jj = (jstart + j) % latNames.size();
            if (arr.names[i].contains(latNames[jj])) {
              arr.s[i] = latS[jj];
              jstart = jj + 1;
              found = true;
              break;
            }
          }
          if (!found)
            QMessageBox::warning(this, "ADT",
              QString("Did not find an s value for %1 in array %2")
              .arg(arr.names[i]).arg(iarray + 1));
        }
      }
    }

    SDDS_Terminate(&table);

    if (ca_pend_io(1.0) == ECA_TIMEOUT) {
      QMessageBox *timeoutBox = new QMessageBox(QMessageBox::Warning, "ADT",
        "Timeout connecting to PVs.", QMessageBox::NoButton, this);
      timeoutBox->setAttribute(Qt::WA_DeleteOnClose);
      timeoutBox->show();
      QTimer::singleShot(3000, timeoutBox, &QMessageBox::accept);
    }

    for (ArrayData &arr : arrays) {
      for (int i = 0; i < arr.nvals; ++i) {
        if (arr.chids[i] && ca_state(arr.chids[i]) == cs_conn) {
          ca_array_get(DBR_DOUBLE, 1, arr.chids[i], &arr.vals[i]);
          arr.conn[i] = true;
        } else {
          arr.vals[i] = 0.0;
          arr.conn[i] = false;
        }
      }
    }
    ca_pend_io(1.0);

    for (ArrayData &arr : arrays) {
      double sum = 0.0;
      double sumsq = 0.0;
      double maxv = 0.0;
      int nconn = 0;
      bool haveMax = false;
      for (int i = 0; i < arr.nvals; ++i) {
        if (!arr.conn[i])
          continue;
        double v = arr.vals[i];
        sum += v;
        sumsq += v * v;
        if (!haveMax || std::fabs(v) > std::fabs(maxv)) {
          maxv = v;
          haveMax = true;
        }
        arr.minVals[i] = v;
        arr.maxVals[i] = v;
        ++nconn;
      }
      if (nconn > 0) {
        arr.avg = sum / nconn;
        arr.sdev = std::sqrt(sumsq / nconn - arr.avg * arr.avg);
        arr.maxVal = maxv;
      } else {
        arr.avg = arr.sdev = arr.maxVal = 0.0;
      }
      arr.runSdev = 0.0;
      arr.runAvg = 0.0;
      arr.runMax = 0.0;
    }

    nsymbols = latNames.size();

    QWidget *central = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(central);
    for (AreaData &area : areas) {
      QVector<ArrayData *> arrs;
      for (ArrayData &arr : arrays) {
        if (arr.area == &area)
          arrs.append(&arr);
      }
      if (!arrs.isEmpty()) {
        auto aw = new AreaWidget(&area, arrs, central);
        layout->addWidget(aw);
        areaWidgets.append(aw);
      }
    }
    QVector<ArrayData *> zarrs;
    int maxScale = 0;
    double avgCenter = 0.0;
    for (ArrayData &arr : arrays) {
      if (arr.zoom) {
        zarrs.append(&arr);
        avgCenter += arr.area->centerVal;
        if (arr.area->currScale > maxScale)
          maxScale = arr.area->currScale;
      }
    }
    if (!zarrs.isEmpty()) {
      zoomArea.currScale = maxScale;
      zoomArea.centerVal = avgCenter / zarrs.size();
      zoomArea.oldCenterVal = zoomArea.centerVal;
      zoomArea.xmax = zoomArea.centerVal +
        scale[zoomArea.currScale] * GRIDDIVISIONS;
      zoomArea.xmin = zoomArea.centerVal -
        scale[zoomArea.currScale] * GRIDDIVISIONS;
      zoomArea.initialized = true;
      if (zarrs[0]->nvals > 0) {
        int maxSect = nsect > 0 ? nsect : 1;
        int interval = std::min(3, maxSect);
        int span = std::max(1,
          (nsect > 0) ? (zarrs[0]->nvals * interval) / nsect :
          zarrs[0]->nvals / 10);
        zoomArea.xStart = 0;
        zoomArea.xEnd = span - 1;
      }
      zoomWidget = new AreaWidget(&zoomArea, zarrs, central, false, true);
      layout->addWidget(zoomWidget);
      areaWidgets.append(zoomWidget);
      zoomWidget->setVisible(zoomOn);
      zoomPlot = zoomWidget->plotWidget();
      zoomAreaPtr = &zoomArea;
      zoomAreaWidget = zoomWidget;
      if (!zoomSectorUsed || !zoomIntervalUsed) {
        int center = 0;
        int interval = 0;
        if (!zoomSectorUsed && initZoomSector > 0) {
          center = initZoomSector;
          zoomSectorUsed = true;
        }
        if (!zoomIntervalUsed && initZoomInterval > 0) {
          interval = initZoomInterval;
          zoomIntervalUsed = true;
        }
        if (center > 0 || interval > 0)
          zoomWidget->setZoom(center, interval);
      }
    }
    setCentralWidget(central);

    setWindowTitle("ADT - " + QFileInfo(file).fileName());
    pollTimer->start(timeInterval);
  }
};

static void usage(const char *prog) {
  fprintf(stderr,
    "Usage: %s [-a directory] [-f pvfile] [-s center_sector] [-z number_of_sectors] [-d] [-h|-?]\n"
    "  -a <directory>          specify ADT home directory\n"
    "  -f <file>               open PV file at startup\n"
    "  -s <center_sector>      set initial zoomed on sector\n"
    "  -z <number_of_sectors>  set initial zoom range\n"
    "  -d                      enable diff mode\n"
    "  -h, -?                  show this help message and exit\n",
    prog);
}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  QString startPv;
  QString homeOverride;
  bool homeSpecified = false;
  int zoomSect = 0;
  int zoomInt = 0;
  bool diffMode = false;
  bool showHelp = false;
  QStringList args = app.arguments();
  for (int i = 1; i < args.size(); ++i) {
    const QString &arg = args.at(i);
    if ((arg == "-a" || arg == "/a") && i + 1 < args.size()) {
      homeOverride = args.at(++i);
      homeSpecified = true;
    } else if ((arg == "-f" || arg == "/f") && i + 1 < args.size()) {
      startPv = args.at(++i);
    } else if ((arg == "-s" || arg == "/s") && i + 1 < args.size()) {
      zoomSect = args.at(++i).toInt();
    } else if ((arg == "-z" || arg == "/z") && i + 1 < args.size()) {
      zoomInt = args.at(++i).toInt();
    } else if (arg == "-d" || arg == "/d") {
      diffMode = true;
    } else if (arg == "-h" || arg == "/h" || arg == "-?" || arg == "/?") {
      showHelp = true;
    }
  }
  if (showHelp) {
    usage(argv[0]);
    return 0;
  }
  MainWindow win(homeOverride, homeSpecified, startPv, zoomSect, zoomInt);
  win.show();
  if (diffMode)
    QTimer::singleShot(1000, &win, [&win]() { win.storeSet(1); win.diffSet(1); });
  return app.exec();
}
