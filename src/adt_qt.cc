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
#include "adt_help.h"

#include <epicsVersion.h>
#include <QSysInfo>
#include <QApplication>
#include <QBitmap>
#include <QImage>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QFont>
#include <QFontDatabase>
#include <QByteArray>
#include <QFontMetrics>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QActionGroup>
#include <QPainter>
#include <QPalette>
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
#include <QElapsedTimer>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPointer>
#include <QPixmap>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QSocketNotifier>
#include <QHash>
#include <limits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <functional>

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

// Legacy X11 color names; Qt does not know X11 Green3 and Grey75, so they are
// given by their X11 RGB values.
static const char *defaultColors[] = {
  "Red", "Blue", "#00cd00", "Gold", "Magenta", "Cyan", "Orange",
  "Purple", "SpringGreen", "Chartreuse", "DeepSkyBlue",
  "MediumSpringGreen", "Tomato", "Tan", "#bfbfbf", "Black"
};
static constexpr int NCOLORS = sizeof(defaultColors) / sizeof(defaultColors[0]);
static constexpr int NSAVE = 5;
// Keep a malformed PV file from requesting enormous vectors before its pages
// can be checked. Bundled layouts use at most eight arrays and four areas.
static constexpr int MAX_LAYOUT_ARRAYS = 1024;
static constexpr int MAX_LAYOUT_AREAS = 1024;
// Each PV opens Channel Access channels; the largest bundled layout has 2,640.
static constexpr int MAX_LAYOUT_PVS = 100000;
// Zoom plots draw one label per sector on every repaint. Bundled lattices
// have at most 40 sectors.
static constexpr int MAX_LATTICE_SECTORS = 1024;

static bool markers = true, lines = true, bars = false, grid = true,
  autoclear = true, showmaxmin = true, fillmaxmin = true, statMode = false,
  refOn = true, referenceLoaded = false;
static int diffSet = -1, displaySet = -1, nsect = 0;
// Status checking, as in legacy ADT: 0 None, 1 Invalid, 2 All (the default).
static int checkStatusMode = 2;
static constexpr short STATUS_NOTCONN = 100;  // status PV not readable
static constexpr short STATUS_UNUSED = 200;   // StatusName "-"
static QColor displayColor(0x66, 0x66, 0x66);  // X11 Grey40, as in legacy ADT
static const QColor backgroundColor("#CCCCCC");
static const QColor filledMinMaxColor(211, 211, 211, 127);
static double nstat = 0.0, nstatTime = 0.0, stotal = 0.0;
static QVector<QString> latNames;
static QVector<double> latS, latLen;
static QVector<short> latHeight;
static bool latRing = false;
static std::function<void()> resetFilledExtremaCallback;

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
  QVector<qint64> getIssued;  // time of the outstanding get, or -1
  // Status PVs from the StatusName column: 0 invalid, 2 old data, other
  // values valid; STATUS_NOTCONN or STATUS_UNUSED otherwise.
  bool statusCheck = false;
  QVector<chid> statusChids;
  QVector<short> statusVals;
  QVector<GetCallbackData> statusCbData;
  QVector<qint64> statusIssued;
  bool zoom = false;
  QString heading;
  QString units;
  double scaleFactor = 1.0;
  bool logScale = false;  // ADTLogScale: plot and statistics in decades
  double sdev = 0.0;
  double avg = 0.0;
  double maxVal = 0.0;
  bool sdevValid = true;
  bool avgValid = true;
  bool maxValid = true;
  double runSdev = 0.0;
  double runAvg = 0.0;
  double runMax = 0.0;
  bool runMaxSet = false;
  int runSdevCount = 0;
  int runAvgCount = 0;
  AreaData *area = nullptr;
  QColor color;
  QVector<double> saveVals[NSAVE];
  QVector<bool> saveAvail[NSAVE];
  QVector<double> refVals;
  QVector<bool> refAvail;

  /**
   * @brief Whether the current reading of a point is available.
   *
   * A point whose status PV cannot be read is unavailable, as in legacy ADT.
   */
  bool available(int i) const
  {
    return conn[i] && !(statusCheck && statusVals[i] == STATUS_NOTCONN);
  }

  /**
   * @brief Status exclusion of an available point under the Check Status mode.
   *
   * @return 0 when the point counts, 1 for invalid data, 2 for old data.
   */
  int excluded(int i) const
  {
    if (!statusCheck || statusVals[i] == STATUS_UNUSED || !available(i))
      return 0;
    if (checkStatusMode >= 1 && statusVals[i] == 0)
      return 1;
    if (checkStatusMode == 2 && statusVals[i] == 2)
      return 2;
    return 0;
  }
};

/**
 * @brief Availability of a point in a stored set or the reference.
 *
 * Sets read from files without an ADTAvailable column are fully available.
 */
static bool setAvailable(const QVector<bool> &avail, int i)
{
  return i >= avail.size() || avail[i];
}

class PlotWidget;
class AreaWidget;

static PlotWidget *zoomPlot = nullptr;
static AreaData *zoomAreaPtr = nullptr;
static AreaWidget *zoomAreaWidget = nullptr;
static void updateZoomCenter();
static void updateZoomInterval();
static void getCallback(struct event_handler_args args);
static void caFdRegistration(void *arg, int fd, int opened);

/**
 * @brief Milliseconds on a monotonic clock, for get deadlines.
 */
static qint64 monotonicMs()
{
  static QElapsedTimer clock;
  if (!clock.isValid())
    clock.start();
  return clock.elapsed();
}

/**
 * @brief Format a PV value for point detail popups.
 *
 * Significant digits preserve very small values such as vacuum pressures that
 * fixed decimal formatting would round to zero.
 *
 * @param value Value to format.
 * @return Compact display string.
 */
static QString formatPvValue(double value)
{
  return QString::number(value, 'g', 4);
}

/**
 * @brief Population statistics of one update's plotted values.
 */
struct Statistics
{
  double sdev = 0.0;
  double avg = 0.0;
  double max = 0.0;
  bool sdevValid = true;
  bool avgValid = true;
  bool maxValid = true;
};

/**
 * @brief Compute SDEV, AVG, and largest-magnitude MAX in two passes.
 *
 * The mean is computed first, as the sum divided by the count, and the
 * variance is the mean squared deviation from it (population definition),
 * taken about the mean refined by one correction pass. Deviations are
 * scaled by the largest one so that finite values cannot overflow the
 * squares; a statistic that itself cannot be represented is marked invalid.
 * An empty sample gives zeros.
 *
 * @param values Finite values of the points that count.
 * @return Statistics and their validity.
 */
static Statistics computeStatistics(const QVector<double> &values)
{
  Statistics st;
  int n = values.size();
  if (n == 0)
    return st;
  double sum = 0.0;
  for (double v : values)
    sum += v;
  double mean = sum / n;
  if (!std::isfinite(mean)) {
    mean = 0.0;
    for (double v : values)
      mean += v / n;
  }
  st.avg = mean;
  st.avgValid = std::isfinite(mean);
  st.max = values[0];
  for (double v : values) {
    if (std::fabs(v) > std::fabs(st.max))
      st.max = v;
  }
  if (!st.avgValid) {
    st.sdevValid = false;
    return st;
  }
  // Take deviations from the mean refined by one correction pass, which
  // absorbs the rounding of the first sum, so that constant data gives an
  // SDEV of exactly zero. AVG keeps the plain mean.
  double center = mean;
  double residual = 0.0;
  for (double v : values)
    residual += v - mean;
  if (std::isfinite(residual))
    center += residual / n;
  // Halved deviations are exact and cannot overflow for finite values.
  double largest = 0.0;
  for (double v : values)
    largest = std::max(largest, std::fabs(v / 2 - center / 2));
  if (largest == 0.0) {
    st.sdev = 0.0;
  } else {
    double sumsq = 0.0;
    for (double v : values) {
      double d = (v / 2 - center / 2) / largest;
      sumsq += d * d;
    }
    st.sdev = largest * std::sqrt(sumsq / n) * 2;
    st.sdevValid = std::isfinite(st.sdev);
  }
  return st;
}

/**
 * @brief Format a statistic for a title row, or "n/a" if it is undefined.
 */
static QString formatStatistic(double value, bool valid, double multiplier)
{
  double shown = value * multiplier;
  if (!valid || !std::isfinite(shown))
    return QString("n/a");
  return QString("%1").arg(shown, 0, 'f', 3);
}

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
 * @brief Locate the index at or above a requested s value.
 */
static int indexAbove(double sval, const QVector<double> &array)
{
  int nmax = array.size();
  if (nmax < 1 || stotal <= 0.0)
    return -1;
  double s = sval;
  if (s < 0.0)
    s += stotal;
  if (s > stotal)
    s -= stotal;
  int first = 0;
  int last = nmax - 1;
  int istart = static_cast<int>(nmax * (s / stotal) - 1);
  if (istart < first)
    istart = first;
  else if (istart > last)
    istart = last;
  if (s > array[istart]) {
    for (int i = istart + 1; i <= last; ++i) {
      if (s <= array[i])
        return i;
    }
    return -1;
  } else if (s < array[istart]) {
    int ii = istart;
    for (int i = istart - 1; i >= first; --i) {
      if (s > array[i])
        return ii;
      ii = i;
    }
    return ii;
  }
  return istart;
}

/**
 * @brief Locate the index at or below a requested s value.
 */
static int indexBelow(double sval, const QVector<double> &array)
{
  int nmax = array.size();
  if (nmax < 1 || stotal <= 0.0)
    return -1;
  double s = sval;
  if (s < 0.0)
    s += stotal;
  if (s > stotal)
    s -= stotal;
  int first = 0;
  int last = nmax - 1;
  int istart = static_cast<int>(nmax * (s / stotal) + 1);
  if (istart < first)
    istart = first;
  else if (istart > last)
    istart = last;
  if (s < array[istart]) {
    for (int i = istart - 1; i >= first; --i) {
      if (s >= array[i])
        return i;
    }
    return -1;
  } else if (s > array[istart]) {
    int ii = istart;
    for (int i = istart + 1; i <= last; ++i) {
      if (s < array[i])
        return ii;
      ii = i;
    }
    return ii;
  }
  return istart;
}

/**
 * @brief Determine the index range intersecting a span of s values.
 */
static void indexLimits(double smin, double smax, const QVector<double> &array,
  int &imin, int &imax)
{
  int nmax = array.size();
  if (nmax < 1) {
    imin = imax = 0;
    return;
  }
  if (!latRing || stotal <= 0.0 || nmax == 1) {
    imin = 0;
    imax = nmax - 1;
    return;
  }
  int imin0 = indexBelow(smin, array);
  double soff = 0.0;
  imin = imin0;
  if (imin0 < 0) {
    imin0 = nmax - 1;
    imin = -1;
    soff += stotal;
  }
  while (array[imin0] > smin + soff) {
    imin -= nmax;
    soff += stotal;
  }
  int imax0 = indexAbove(smax, array);
  imax = imax0;
  soff = 0.0;
  if (imax0 < 0) {
    imax0 = (nmax > 1) ? 1 : 0;
    imax = nmax + 1;
    soff -= stotal;
  }
  while (array[imax0] < smax + soff) {
    imax += nmax;
    soff -= stotal;
  }
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
 * @brief Initialize SDDS input and release a partially read layout on failure.
 *
 * Failed initialization can retain an open file and layout allocations before
 * SDDS saves its cleanup layout. Use the public layout APIs to transfer that
 * ownership to SDDS_Terminate without changing successful reads.
 */
static bool initializeSddsInput(SDDS_TABLE *table, char *filename)
{
  *table = {};
  if (SDDS_InitializeInput(table, filename))
    return true;
  SDDS_DeferSavingLayout(table, 0);
  if (SDDS_SaveLayout(table))
    SDDS_Terminate(table);
  return false;
}

/**
 * @brief Read an allocated SDDS string without interpreting numeric storage as a pointer.
 *
 * The caller releases a successful result with SDDS_Free.
 */
static char *readStringParameter(SDDS_TABLE *table, const char *name)
{
  int index = SDDS_GetParameterIndex(table, const_cast<char *>(name));
  if (index < 0 || SDDS_GetParameterType(table, index) != SDDS_STRING)
    return nullptr;
  char *value = nullptr;
  if (!SDDS_GetParameter(table, const_cast<char *>(name), &value))
    return nullptr;
  return value;
}

/**
 * @brief Parse a complete finite snapshot value, preserving strtod precision.
 */
static bool parseSnapshotValue(const char *text, double &value)
{
  if (!text)
    return false;
  char *end = nullptr;
  value = std::strtod(text, &end);
  if (end == text || !std::isfinite(value))
    return false;
  while (*end && std::isspace(static_cast<unsigned char>(*end)))
    ++end;
  return !*end;
}

/**
 * @brief Write a string column with SDDS whitespace and punctuation escaping.
 */
static bool writeSddsString(FILE *file, const QString &value)
{
  QByteArray bytes = value.toUtf8();
  char *text = bytes.data();
  return SDDS_WriteTypedValue(&text, 0, SDDS_STRING, nullptr, file) != 0;
}

/**
 * @brief Commit a completed export without truncating an existing destination.
 *
 * Closes the temporary stream on every path. QSaveFile discards incomplete
 * output if a read, write, or commit fails.
 */
static bool commitExport(FILE *file, const QString &filename)
{
  bool ok = !ferror(file) && fflush(file) == 0 && fseek(file, 0, SEEK_SET) == 0;
  QSaveFile output(filename);
  if (ok)
    ok = output.open(QIODevice::WriteOnly);
  char buffer[16384];
  while (ok) {
    size_t count = fread(buffer, 1, sizeof(buffer), file);
    if (count && output.write(buffer, static_cast<qint64>(count)) !=
        static_cast<qint64>(count)) {
      ok = false;
      break;
    }
    if (count < sizeof(buffer)) {
      ok = !ferror(file);
      break;
    }
  }
  if (fclose(file) != 0)
    ok = false;
  if (!ok) {
    output.cancelWriting();
    return false;
  }
  return output.commit();
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
 * @param snapDirectory Output path for snapshot directory.
 * @param isXorbit True if Xorbit-specific directories should be used when
 *   provided.
 * @return List of menu items discovered in the file.
 */
static QList<LoadMenuInfo> readInitFile(const QString &filename,
  const QString &adtHome, QString &pvDirectory, QString &customDirectory,
  QString &snapDirectory, bool isXorbit)
{
  QList<LoadMenuInfo> menus;
  pvDirectory = adtHome + "/pv";
  customDirectory = pvDirectory;
  snapDirectory = adtHome + "/snap";
  if (filename.isEmpty())
    return menus;
  SDDS_ClearErrors();
  SDDS_TABLE table;
  QByteArray fname = filename.toUtf8();
  if (!initializeSddsInput(&table, fname.data()))
    return menus;
  const char *stringParameters[] = {"ADTPVDirectory", "ADTPVSubDirectory",
    "ADTSnapDirectory", "ADTXPVDirectory", "ADTXSnapDirectory", "ADTMenuTitle"};
  for (const char *name : stringParameters) {
    int index = SDDS_GetParameterIndex(&table, const_cast<char *>(name));
    if (index >= 0 && SDDS_GetParameterType(&table, index) != SDDS_STRING) {
      SDDS_Terminate(&table);
      return menus;
    }
  }
  if (SDDS_CheckColumn(&table, const_cast<char *>("ADTPVFile"), NULL,
      SDDS_STRING, NULL) != SDDS_CHECK_OKAY) {
    SDDS_Terminate(&table);
    return menus;
  }
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
      char *snapdir = NULL;
      if (SDDS_GetParameter(&table, const_cast<char *>("ADTSnapDirectory"),
          &snapdir)) {
        snapDirectory = snapdir;
        SDDS_Free(snapdir);
      }
      if (isXorbit) {
        char *xpv = NULL;
        if (SDDS_GetParameter(&table,
            const_cast<char *>("ADTXPVDirectory"), &xpv)) {
          pvDirectory = xpv;
          customDirectory = pvDirectory;
          SDDS_Free(xpv);
        }
        char *xsnap = NULL;
        if (SDDS_GetParameter(&table,
            const_cast<char *>("ADTXSnapDirectory"), &xsnap)) {
          snapDirectory = xsnap;
          SDDS_Free(xsnap);
        }
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
    if (!files) {
      freeSddsStrings(rows, labels);
      menus.clear();
      break;
    }
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
  latRing = false;
  SDDS_TABLE table;
  QByteArray fname = filename.toUtf8();
  if (!initializeSddsInput(&table, fname.data()))
    return false;
  if (SDDS_ReadTable(&table) != 1) {
    SDDS_Terminate(&table);
    return false;
  }
  if (SDDS_CheckColumn(&table, const_cast<char *>("Name"), NULL,
      SDDS_STRING, NULL) != SDDS_CHECK_OKAY ||
      SDDS_CheckColumn(&table, const_cast<char *>("S"), NULL,
      SDDS_DOUBLE, NULL) != SDDS_CHECK_OKAY ||
      SDDS_CheckColumn(&table, const_cast<char *>("Length"), NULL,
      SDDS_DOUBLE, NULL) != SDDS_CHECK_OKAY ||
      SDDS_CheckColumn(&table, const_cast<char *>("SymbolHeight"), NULL,
      SDDS_SHORT, NULL) != SDDS_CHECK_OKAY) {
    SDDS_Terminate(&table);
    return false;
  }
  bool ok = false;
  char *type = NULL;
  type = readStringParameter(&table, "ADTFileType");
  if (type && !strcmp(type, "ADTLATTICE")) {
    int32_t ns;
    double st;
    int32_t ringVal = 1;
    SDDS_GetParameterAsLong(&table, const_cast<char *>("Ring"), &ringVal);
    if (SDDS_GetParameterAsLong(&table, const_cast<char *>("Nsectors"), &ns) &&
        SDDS_GetParameterAsDouble(&table, const_cast<char *>("Stotal"), &st) &&
        ns > 0 && ns <= MAX_LATTICE_SECTORS && std::isfinite(st) && st > 0.0) {
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
        bool valid = true;
        for (int i = 0; i < rows; ++i) {
          if (!std::isfinite(scol[i]) || !std::isfinite(len[i]) ||
              scol[i] < 0.0 || scol[i] > st || len[i] < 0.0 ||
              (i > 0 && scol[i] < scol[i - 1])) {
            valid = false;
            break;
          }
        }
        if (valid) {
          for (int i = 0; i < rows; ++i) {
            latNames.append(names[i]);
            latS.append(scol[i]);
            latLen.append(len[i]);
            latHeight.append(height[i]);
          }
          latRing = ringVal != 0;
          ok = true;
        }
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
    p.fillRect(rect(), backgroundColor);
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
    drawCentered("Written by Dwarfs in the Waterfall Glen", ystart);
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
      double offset = (v - ymin) * yscale;
      // Keep clipping outside the visible plot, but within integer/raster bounds.
      if (std::isnan(offset))
        offset = 0.0;
      offset = std::max(-1000000.0, std::min(1000000.0, offset));
      return bottom - static_cast<int>(offset);
    };

    bool refOnLoaded = referenceLoaded && refOn;
    bool diffOn = diffSet >= 0;
    // The plotted quantity of vec[idx], or false when it is undefined. A log
    // array plots log10(scale factor x value), and in difference or reference
    // mode the change in decades; a nonpositive operand has no logarithm.
    auto transform = [&](ArrayData *arr, const QVector<double> &vec, int idx,
      double &y) {
      const QVector<double> *subtract = nullptr;
      if (diffOn && arr->saveVals[diffSet].size() == arr->nvals)
        subtract = &arr->saveVals[diffSet];
      else if (refOnLoaded && arr->refVals.size() == arr->nvals)
        subtract = &arr->refVals;
      if (!arr->logScale) {
        y = (vec[idx] - (subtract ? (*subtract)[idx] : 0.0)) * arr->scaleFactor;
        return true;
      }
      double v = arr->scaleFactor * vec[idx];
      if (!(v > 0.0))
        return false;
      y = std::log10(v);
      if (subtract) {
        double s = arr->scaleFactor * (*subtract)[idx];
        if (!(s > 0.0))
          return false;
        y -= std::log10(s);
      }
      return true;
    };
    // Max/min envelope values; an undefined logarithm maps to the plot bottom.
    auto diffVal = [&](ArrayData *arr, const QVector<double> &vec, int idx) {
      double y;
      return transform(arr, vec, idx, y) ? y : std::numeric_limits<double>::quiet_NaN();
    };
    // The plotted value of a point of the current values (set < 0) or of a
    // stored set. A point that is unavailable in either operand has none.
    auto plotValue = [&](ArrayData *arr, const QVector<double> &vec, int set,
      int idx, double &y) {
      bool ok = set < 0 ? arr->available(idx) : setAvailable(arr->saveAvail[set], idx);
      if (diffOn && arr->saveVals[diffSet].size() == arr->nvals)
        ok = ok && setAvailable(arr->saveAvail[diffSet], idx);
      else if (refOnLoaded && arr->refVals.size() == arr->nvals)
        ok = ok && setAvailable(arr->refAvail, idx);
      return transform(arr, vec, idx, y) && ok;
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

    struct ZoomLimit
    {
      int min = 0;
      int max = -1;
    };
    QVector<ZoomLimit> zoomLimits;
    double zoomSmin = 0.0;
    double zoomSmax = 0.0;
    double zoomRange = 0.0;
    bool useZoomLimits = false;
    bool zoomWrap = latRing && stotal > 0.0;
    bool zoomDrawWrap = zoomWrap && nsect > 1;

    if (area == zoomAreaPtr && !arrayPtrs.isEmpty()) {
      ArrayData *baseArr = arrayPtrs[0];
      int baseN = baseArr->nvals;
      if (baseN > 0 && baseArr->s.size() == baseN) {
        int baseStart = wrapIndex(area->xStart, baseN);
        int baseEnd = wrapIndex(area->xEnd, baseN);
        int span = (baseEnd >= baseStart) ?
          (baseEnd - baseStart + 1) : (baseN - baseStart + baseEnd + 1);
        if (span >= baseN && stotal > 0.0) {
          zoomSmin = 0.0;
          zoomSmax = stotal;
        } else {
          zoomSmin = baseArr->s[baseStart];
          zoomSmax = baseArr->s[baseEnd] + (baseEnd < baseStart ? stotal : 0.0);
        }
        zoomRange = zoomSmax - zoomSmin;
        if (zoomRange <= 0.0 && zoomWrap)
          zoomRange = stotal;
        if (zoomRange > 0.0) {
          useZoomLimits = true;
          zoomLimits.resize(arrayPtrs.size());
          for (int i = 0; i < arrayPtrs.size(); ++i) {
            auto arr = arrayPtrs[i];
            ZoomLimit limit;
            if (arr->nvals > 0 && arr->s.size() == arr->nvals) {
              if (zoomWrap)
                indexLimits(zoomSmin, zoomSmax, arr->s, limit.min, limit.max);
              else {
                limit.min = 0;
                limit.max = arr->nvals - 1;
              }
            }
            zoomLimits[i] = limit;
          }
        }
      }
    }

    bool applyZoomLimits = useZoomLimits && zoomRange > 0.0 &&
      zoomLimits.size() == arrayPtrs.size();
    double zoomXScale = applyZoomLimits ? plotRect.width() / zoomRange : 0.0;

    if (showmaxmin && area != zoomAreaPtr) {
      for (int arrIndex = 0; arrIndex < arrayPtrs.size(); ++arrIndex) {
        auto arr = arrayPtrs[arrIndex];
        if (arr->nvals < 1 || arr->minVals.size() != arr->nvals ||
            arr->maxVals.size() != arr->nvals)
          continue;
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
          pmap.setBrush(filledMinMaxColor);
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

    pmap.setPen(Qt::black);
    pmap.drawLine(plotRect.left(), y0, plotRect.right(), y0);

    if (area->tempnodraw) {
      area->tempnodraw = false;
      QPainter pw(this);
      pw.drawPixmap(0, 0, pixmap);
      return;
    }

    auto drawPolylineWrapped = [&](const QVector<QPointF> &points,
      bool allowWrap) {
      if (points.size() < 2) {
        return;
      }
      if (!allowWrap) {
        QPointF prev = points[0];
        for (int i = 1; i < points.size(); ++i) {
          QPointF curr = points[i];
          if (curr.x() >= prev.x())
            pmap.drawLine(prev, curr);
          prev = curr;
        }
        return;
      }
      double width = static_cast<double>(plotRect.width());
      if (width <= 0.0) {
        pmap.drawPolyline(points.constData(), points.size());
        return;
      }
      double left = static_cast<double>(plotRect.left());
      double right = static_cast<double>(plotRect.right());
      double threshold = width / 2.0;
      QPointF prev = points[0];
      for (int i = 1; i < points.size(); ++i) {
        QPointF curr = points[i];
        double dx = curr.x() - prev.x();
        if (dx < -threshold) {
          double spanToRight = right - prev.x();
          double spanFromLeft = curr.x() - left;
          double totalSpan = spanToRight + spanFromLeft;
          if (totalSpan <= 0.0) {
            pmap.drawLine(prev, curr);
          } else {
            double frac = spanToRight / totalSpan;
            double yEdge = prev.y() + frac * (curr.y() - prev.y());
            QPointF rightPoint(right, yEdge);
            QPointF leftPoint(left, yEdge);
            pmap.drawLine(prev, rightPoint);
            pmap.drawLine(leftPoint, curr);
          }
        } else {
          pmap.drawLine(prev, curr);
        }
        prev = curr;
        if (curr.x() == width) {
          break;
        }
      }
    };

    // Draw lines between consecutive points that have values, normal markers,
    // and the legacy not-connected marker (a black 5x5 square) on the center
    // line for points without a value.
    auto finishArray = [&](int count, const QColor &clr, bool wrapped,
      bool allowWrap) {
      bool allValid = !tmpState.contains(0);
      bool allNormal = allValid && !tmpState.contains(2) && !tmpState.contains(3);
      if (lines) {
        int first = 0;
        while (first < count) {
          while (first < count && !tmpState[first])
            ++first;
          int last = first;
          while (last < count && tmpState[last])
            ++last;
          if (last - first >= 2) {
            QVector<QPointF> run = allValid ? tmpPts : tmpPts.mid(first, last - first);
            if (wrapped)
              drawPolylineWrapped(run, allowWrap);
            else
              pmap.drawPolyline(run.constData(), run.size());
          }
          first = last;
        }
      }
      if (markers) {
        QPen oldPen = pmap.pen();
        QPen markerPen(clr, 3);
        markerPen.setCapStyle(Qt::SquareCap);
        pmap.setPen(markerPen);
        if (allNormal) {
          pmap.drawPoints(tmpPts.constData(), count);
        } else {
          for (int i = 0; i < count; ++i)
            if (tmpState[i] == 1)
              pmap.drawPoint(tmpPts[i]);
        }
        pmap.setPen(oldPen);
      }
      if (allNormal)
        return;
      for (int i = 0; i < count; ++i) {
        int px = static_cast<int>(tmpPts[i].x());
        int py = static_cast<int>(tmpPts[i].y());
        if (!tmpState[i]) {
          pmap.fillRect(px - 2, y0 - 2, 5, 5, Qt::black);
        } else if (tmpState[i] > 1) {
          // Legacy status markers: a rounded 5x5 dot, X11 Grey30 for invalid
          // data and Grey50 for old data (Qt does not know X11 color names).
          QColor statusColor = tmpState[i] == 2 ? QColor(0x4d, 0x4d, 0x4d) :
            QColor(0x7f, 0x7f, 0x7f);
          pmap.fillRect(px - 2, py - 1, 5, 3, statusColor);
          pmap.fillRect(px - 1, py - 2, 3, 1, statusColor);
          pmap.fillRect(px - 1, py + 2, 3, 1, statusColor);
        }
      }
    };
    // Drawing state of a point: 0 without a value, 1 normal, 2 excluded as
    // invalid, 3 excluded as old data (status markers apply to current values).
    auto pointState = [&](ArrayData *arr, const QVector<double> &vec, int set,
      int idx, double &y) {
      if (!plotValue(arr, vec, set, idx, y))
        return 0;
      return set < 0 ? 1 + arr->excluded(idx) : 1;
    };

    auto drawArray = [&](int arrIndex, ArrayData *arr,
      const QVector<double> &vec, int set, const QColor &clr) {
      if (arr->nvals < 1 || vec.size() != arr->nvals)
        return;
      pmap.setPen(clr);
      if (area == zoomAreaPtr) {
        bool drewZoom = false;
        if (applyZoomLimits && arrIndex >= 0 && arrIndex < zoomLimits.size() &&
            arr->s.size() == arr->nvals) {
          ZoomLimit limit = zoomLimits[arrIndex];
          int count = limit.max - limit.min + 1;
          if (count > 0) {
            tmpPts.resize(count);
            tmpState.fill(1, count);
            for (int i = 0; i < count; ++i) {
              int idx = limit.min + i;
              int wrapped = idx;
              double soff = 0.0;
              while (wrapped >= arr->nvals) {
                wrapped -= arr->nvals;
                soff += stotal;
              }
              while (wrapped < 0) {
                wrapped += arr->nvals;
                soff -= stotal;
              }
              double sval = arr->s[wrapped] + soff;
              if (zoomWrap) {
                while (sval < zoomSmin)
                  sval += stotal;
                while (sval > zoomSmax)
                  sval -= stotal;
              }
              double x = plotRect.left() + (sval - zoomSmin) * zoomXScale;
              double yv;
              tmpState[i] = pointState(arr, vec, set, wrapped, yv);
              int y = tmpState[i] ? mapY(yv) : y0;
              int xi = static_cast<int>(x);
              if (bars && tmpState[i])
                pmap.drawLine(xi, y0, xi, y);
              tmpPts[i] = QPointF(x, y);
            }
            finishArray(count, clr, true, zoomDrawWrap);
            drewZoom = true;
          }
        }
        if (drewZoom)
          return;
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
        bool haveSRange = std::isfinite(range) && range > 0.0;
        double xscale = haveSRange ? plotRect.width() / range : 0.0;
        tmpPts.resize(count);
        tmpState.fill(1, count);
        for (int i = 0; i < count; ++i) {
          int idx = (start + i) % nvals;
          double sval = arr->s[idx];
          if (count < nvals && idx < start)
            sval += stotal;
          double x = haveSRange ?
            plotRect.left() + (sval - smin) * xscale :
            plotRect.left() + (i + 0.5) * plotRect.width() / count;
          double yv;
          tmpState[i] = pointState(arr, vec, set, idx, yv);
          int y = tmpState[i] ? mapY(yv) : y0;
          int xi = static_cast<int>(x);
          if (bars && tmpState[i])
            pmap.drawLine(xi, y0, xi, y);
          tmpPts[i] = QPointF(x, y);
        }
        finishArray(count, clr, true, zoomDrawWrap);
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
        tmpPts.resize(count);
        tmpState.fill(1, count);
        for (int i = start; i < end; ++i, x += xstep) {
          double yv;
          int state = pointState(arr, vec, set, i, yv);
          bool ok = state != 0;
          tmpState[i - start] = state;
          int y = ok ? mapY(yv) : y0;
          int xi = static_cast<int>(x);
          if (bars && ok)
            pmap.drawLine(xi, y0, xi, y);
          tmpPts[i - start] = QPointF(x, y);
        }
        finishArray(count, clr, false, false);
      }
    };

    if (displaySet >= 0) {
      for (int i = 0; i < arrayPtrs.size(); ++i) {
        auto arr = arrayPtrs[i];
        QColor clr = displayColor;
        if (clr == arr->color)
          clr = Qt::green;
        drawArray(i, arr, arr->saveVals[displaySet], displaySet, clr);
      }
    }

    for (int i = 0; i < arrayPtrs.size(); ++i) {
      auto arr = arrayPtrs[i];
      drawArray(i, arr, arr->vals, -1, arr->color);
    }

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
      if (!latS.isEmpty() && range > 0.0) {
        int latCount = static_cast<int>(latS.size());
        int latMin = 0;
        int latMax = latCount - 1;
        if (latRing)
          indexLimits(smin, smax, latS, latMin, latMax);
        for (int iis = latMin; iis <= latMax; ++iis) {
          int idx = iis;
          double soff = 0.0;
          while (idx >= latCount) {
            idx -= latCount;
            soff += stotal;
          }
          while (idx < 0) {
            idx += latCount;
            soff -= stotal;
          }
          double sdraw = latS[idx] + soff;
          double sadd = sdraw;
          if (sadd < smin)
            sadd += stotal;
          if (sadd > smax)
            sadd -= stotal;
          if (sadd < smin || sadd > smax)
            continue;
          int x1 = static_cast<int>(std::lround(xfroms(sdraw)));
          int unit = latHeight[idx] * bunit;
          pmap.drawLine(x1, y0, x1, y0 + unit);
          if (latLen[idx] > 0.0) {
            double s2 = latS[idx] + latLen[idx] + soff;
            int x2 = static_cast<int>(std::lround(xfroms(s2)));
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
        if (plotRect.width() > 0) {
          nmid = static_cast<int>((event->pos().x() - plotRect.left()) *
            static_cast<double>(nvals) / plotRect.width());
          nmid = std::max(0, std::min(nvals - 1, nmid));
        }
      }
      QString info;
      for (auto arr : arrayPtrs) {
        if (arr->nvals < 1)
          continue;
        info += arr->heading + "\n";
        for (int off = -1; off <= 1; ++off) {
          int idx = wrapIndex(nmid + off, arr->nvals);
          double val = arr->scaleFactor * arr->vals[idx];
          QString text;
          if (!arr->available(idx))
            text = "Unavailable";
          else if (!arr->logScale)
            text = formatPvValue(val);
          else if (val > 0.0)
            text = formatPvValue(std::log10(val));
          else
            text = QString::fromUtf8("\xE2\x89\xA4 0 (log undefined)");
          QString line = QString("%1%2 %3  %4\n")
            .arg(idx == nmid ? "->" : "  ")
            .arg(idx + 1)
            .arg(arr->names[idx])
            .arg(text, 11);
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
        QRect plotRect = makePlotRect(width(), height());
        if (plotRect.contains(event->pos()) && resetFilledExtremaCallback) {
          resetFilledExtremaCallback();
        } else {
          QWidget::mousePressEvent(event);
        }
      }
    } else if (event->button() == Qt::MiddleButton) {
      if (zoomPlot && zoomAreaPtr && this != zoomPlot && !arrayPtrs.isEmpty()) {
        QRect plotRect = makePlotRect(width(), height());
        if (!plotRect.contains(event->pos())) {
          QWidget::mousePressEvent(event);
          return;
        }
        auto sourceArr = arrayPtrs[0];
        if (!sourceArr || sourceArr->nvals < 1) {
          QWidget::mousePressEvent(event);
          return;
        }
        if (!zoomPlot || zoomPlot->arrayPtrs.isEmpty()) {
          QWidget::mousePressEvent(event);
          return;
        }
        auto zoomBase = zoomPlot->arrayPtrs[0];
        if (!zoomBase || zoomBase->nvals < 1) {
          QWidget::mousePressEvent(event);
          return;
        }
        int nvals = sourceArr->nvals;
        int startIdx = area->xStart;
        int endIdx = area->xEnd >= area->xStart ? area->xEnd + 1 : nvals;
        if (startIdx < 0)
          startIdx = 0;
        if (endIdx > nvals)
          endIdx = nvals;
        int count = endIdx - startIdx;
        if (count < 1) {
          QWidget::mousePressEvent(event);
          return;
        }
        double xstep = count > 0 ? plotRect.width() /
          static_cast<double>(count) : 0.0;
        double xpos = event->pos().x();
        double xstart = plotRect.left() + xstep / 2.0;
        int offset = 0;
        if (xstep > 0.0)
          offset = static_cast<int>(std::round((xpos - xstart) / xstep));
        int idx = startIdx + offset;
        if (idx < startIdx)
          idx = startIdx;
        if (idx >= endIdx)
          idx = endIdx - 1;
        if (idx < 0 || idx >= nvals) {
          QWidget::mousePressEvent(event);
          return;
        }

        int zoomNvals = zoomBase->nvals;
        int span = (zoomAreaPtr->xEnd >= zoomAreaPtr->xStart) ?
          (zoomAreaPtr->xEnd - zoomAreaPtr->xStart + 1) :
          (zoomNvals - zoomAreaPtr->xStart + zoomAreaPtr->xEnd + 1);
        if (span < 1)
          span = 1;
        if (span > zoomNvals)
          span = zoomNvals;

        auto normalizeS = [](double s) {
          if (stotal <= 0.0)
            return s;
          double val = std::fmod(s, stotal);
          if (val < 0.0)
            val += stotal;
          return val;
        };

        int target = 0;
        bool haveSourceS = sourceArr->s.size() == sourceArr->nvals;
        bool haveZoomS = zoomBase->s.size() == zoomBase->nvals;
        if (haveSourceS && haveZoomS) {
          double targetS = normalizeS(sourceArr->s[idx]);
          double best = std::numeric_limits<double>::max();
          for (int i = 0; i < zoomNvals; ++i) {
            double zs = normalizeS(zoomBase->s[i]);
            double diff = std::fabs(zs - targetS);
            if (stotal > 0.0)
              diff = std::min(diff, stotal - diff);
            if (diff < best) {
              best = diff;
              target = i;
            }
          }
        } else {
          double frac = (count > 1) ?
            static_cast<double>(idx - startIdx) /
            static_cast<double>(count - 1) : 0.0;
          if (frac < 0.0)
            frac = 0.0;
          if (frac > 1.0)
            frac = 1.0;
          target = static_cast<int>(std::round(frac * (zoomNvals - 1)));
        }

        if (target < 0)
          target = 0;
        if (target >= zoomNvals)
          target = zoomNvals - 1;

        int half = span / 2;
        int start = wrapIndex(target - half, zoomNvals);
        int end = wrapIndex(start + span - 1, zoomNvals);
        zoomAreaPtr->xStart = start;
        zoomAreaPtr->xEnd = end;
        zoomPlot->update();
        updateZoomCenter();
        updateZoomInterval();
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
      // Bound the floating-point result before converting, including when
      // a large wheel delta makes factor underflow to zero.
      double requestedSpan = span / factor + 0.5;
      int newSpan = static_cast<int>(std::max(1.0,
        std::min(static_cast<double>(nvals), requestedSpan)));
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
  QVector<int> tmpState;
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
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, backgroundColor);
    setPalette(pal);
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
        auto availabilityLabel = new QLabel;
        availabilityLabel->setFont(fixedFont);
        row->addWidget(availabilityLabel);
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
        StatLabels sl{ sdevLabel, avgLabel, maxLabel, availabilityLabel };
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
      intervalSpin->setSingleStep(1);
      int def = std::min(3, max);
      intervalSpin->setValue(def);
      intervalSpin->setEnabled(max > 1);
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
        if (hasSectorControls()) {
          int center = centerSectSpin ? centerSectSpin->value() : 1;
          applySectorSelection(center, val);
          schedulePlotUpdate();
          updateCenterSectSpin();
          updateIntervalSpin();
          return;
        }
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
        if (arrayPtrs.isEmpty())
          return;
        if (hasSectorControls()) {
          int interval = intervalSpin ? intervalSpin->value() : 1;
          applySectorSelection(val, interval);
          schedulePlotUpdate();
          updateIntervalSpin();
          return;
        }
        if (nsect <= 0 || stotal <= 0.0)
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
      });
      updateCenterSectSpin();
      updateIntervalSpin();
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
    if (!centerSectSpin || arrayPtrs.isEmpty())
      return;
    if (nsect <= 0 || stotal <= 0.0)
      return;
    auto arr = arrayPtrs[0];
    if (!arr || arr->nvals < 1 || arr->s.size() != arr->nvals)
      return;
    int nvals = arr->nvals;
    double sectLen = stotal / nsect;
    if (sectLen <= 0.0)
      return;
    int span = (area->xEnd >= area->xStart) ?
      (area->xEnd - area->xStart + 1) :
      (nvals - area->xStart + area->xEnd + 1);
    int midIdx = wrapIndex(area->xStart + span / 2, nvals);
    double s = arr->s[midIdx];
    if (s <= 0.0) {
      for (int off = 1; off <= span / 2; ++off) {
        int up = wrapIndex(midIdx + off, nvals);
        if (arr->s[up] > 0.0) { s = arr->s[up]; break; }
        int dn = wrapIndex(midIdx - off, nvals);
        if (arr->s[dn] > 0.0) { s = arr->s[dn]; break; }
      }
    }
    double norm = normalizeS(s);
    int sect = static_cast<int>(norm / sectLen) + 1;
    if (sect < 1)
      sect = 1;
    else if (sect > nsect)
      sect = nsect;
    centerSectSpin->blockSignals(true);
    centerSectSpin->setValue(sect);
    centerSectSpin->blockSignals(false);
  }

  void updateIntervalSpin()
  {
    if (!intervalSpin)
      return;
    intervalSpin->setEnabled(nsect > 1);
    if (arrayPtrs.isEmpty())
      return;
    int nvals = arrayPtrs[0]->nvals;
    if (nvals < 1)
      return;
    int span = (area->xEnd >= area->xStart) ?
      (area->xEnd - area->xStart + 1) :
      (nvals - area->xStart + area->xEnd + 1);
    int perSect = nsect > 0 ? std::max(1, nvals / nsect) : nvals;
    int val = (span + perSect - 1) / perSect;
    if (val < 1)
      val = 1;
    if (val > nsect)
      val = nsect;
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
      int unavailable = 0;
      int excluded = 0;
      for (int k = 0; k < arr->nvals; ++k) {
        unavailable += !arr->available(k);
        excluded += arr->excluded(k) != 0;
      }
      QStringList counts;
      if (unavailable > 0)
        counts << QString("Unavailable: %1/%2").arg(unavailable).arg(arr->nvals);
      if (excluded > 0)
        counts << QString("Excluded: %1/%2").arg(excluded).arg(arr->nvals);
      sl.availability->setText(counts.join("  "));
      // Log arrays already include the scale factor in their decades.
      double factor = arr->logScale ? 1.0 : arr->scaleFactor;
      if (statMode && nstat > 0) {
        sl.sdev->setText(formatStatistic(arr->runSdevCount > 0 ?
          arr->runSdev / arr->runSdevCount : 0.0, arr->runSdevCount > 0, factor));
        sl.avg->setText(formatStatistic(arr->runAvgCount > 0 ?
          arr->runAvg / arr->runAvgCount : 0.0, arr->runAvgCount > 0, factor));
      } else {
        sl.sdev->setText(formatStatistic(arr->sdev, arr->sdevValid, factor));
        sl.avg->setText(formatStatistic(arr->avg, arr->avgValid, factor));
      }
      if (statMode)
        sl.max->setText(formatStatistic(arr->runMax,
          arr->runMaxSet || !arr->logScale, factor));
      else
        sl.max->setText(formatStatistic(arr->maxVal, arr->maxValid, factor));
    }
  }

  bool hasSectorControls() const
  {
    if (!intervalSpin || !centerSectSpin || arrayPtrs.isEmpty())
      return false;
    if (!latRing || nsect <= 0 || stotal <= 0.0)
      return false;
    auto arr = arrayPtrs[0];
    if (!arr || arr->nvals < 1)
      return false;
    return arr->s.size() == arr->nvals;
  }

  double normalizeS(double s) const
  {
    if (stotal <= 0.0)
      return s;
    double val = std::fmod(s, stotal);
    if (val < 0.0)
      val += stotal;
    return val;
  }

  bool computeAbsoluteRange(double &smin, double &smax, int &span) const
  {
    if (!hasSectorControls())
      return false;
    auto arr = arrayPtrs[0];
    int nvals = arr->nvals;
    if (nvals < 1)
      return false;
    int start = wrapIndex(area->xStart, nvals);
    span = (area->xEnd >= area->xStart) ?
      (area->xEnd - area->xStart + 1) :
      (nvals - area->xStart + area->xEnd + 1);
    if (span <= 0)
      return false;
    smin = arr->s[start];
    if (span == 1) {
      smax = smin;
      return true;
    }
    double prev = smin;
    double offset = 0.0;
    for (int i = 1; i < span; ++i) {
      int idx = wrapIndex(area->xStart + i, nvals);
      double current = arr->s[idx];
      if (current < prev)
        offset += stotal;
      prev = current;
      if (i == span - 1)
        smax = current + offset;
    }
    if (span >= nvals && stotal > 0.0 && smax < smin + stotal)
      smax = smin + stotal;
    return true;
  }

  int computeCenterSector(double smin, double smax) const
  {
    if (!hasSectorControls())
      return 1;
    double sectLen = stotal / nsect;
    if (sectLen <= 0.0)
      return 1;
    double center = (smin + smax) / 2.0;
    double norm = normalizeS(center);
    int sect = static_cast<int>(norm / sectLen) + 1;
    if (sect < 1)
      sect = 1;
    else if (sect > nsect)
      sect = nsect;
    return sect;
  }

  int computeVisibleSectors(double smin, double smax) const
  {
    if (!hasSectorControls())
      return 1;
    double sectLen = stotal / nsect;
    if (sectLen <= 0.0)
      return 1;
    double range = smax - smin;
    if (range < 0.0 && stotal > 0.0)
      range += stotal;
    int val = static_cast<int>(std::lround(range / sectLen));
    if (val < 1)
      val = 1;
    if (val > nsect)
      val = nsect;
    return val;
  }

  int findIndexNearS(double targetS) const
  {
    if (!hasSectorControls())
      return 0;
    auto arr = arrayPtrs[0];
    int nvals = arr->nvals;
    if (nvals < 1)
      return 0;
    double bestDiff = std::numeric_limits<double>::max();
    int bestIndex = 0;
    for (int i = 0; i < nvals; ++i) {
      double s = normalizeS(arr->s[i]);
      double diff = std::fabs(s - targetS);
      if (stotal > 0.0)
        diff = std::min(diff, stotal - diff);
      if (diff < bestDiff) {
        bestDiff = diff;
        bestIndex = i;
      }
    }
    return bestIndex;
  }

  void applySectorSelection(int centerSector, int visibleSectors)
  {
    if (!hasSectorControls())
      return;
    auto arr = arrayPtrs[0];
    int nvals = arr->nvals;
    if (nvals < 1)
      return;
    int clampedCenter = centerSector;
    if (clampedCenter < 1)
      clampedCenter = 1;
    else if (clampedCenter > nsect)
      clampedCenter = nsect;
    int clampedInterval = visibleSectors;
    if (clampedInterval < 1)
      clampedInterval = 1;
    else if (clampedInterval > nsect)
      clampedInterval = nsect;
    double sectLen = stotal / nsect;
    if (sectLen <= 0.0) {
      area->xStart = 0;
      area->xEnd = nvals - 1;
      return;
    }
    double centerS = normalizeS((clampedCenter - 0.5) * sectLen);
    if (clampedInterval >= nsect) {
      area->xStart = 0;
      area->xEnd = nvals - 1;
      return;
    }
    int centerIndex = findIndexNearS(centerS);
    int perSect = std::max(1, nvals / nsect);
    int span = perSect * clampedInterval;
    if (span > nvals)
      span = nvals;
    if (span < 1)
      span = 1;
    int startIndex = wrapIndex(centerIndex - span / 2, nvals);
    int endIndex = wrapIndex(startIndex + span - 1, nvals);
    area->xStart = startIndex;
    area->xEnd = endIndex;
  }

  struct StatLabels
  {
    QLabel *sdev;
    QLabel *avg;
    QLabel *max;
    QLabel *availability;
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
  if (!cb || !cb->arr || cb->index < 0 ||
      cb->index >= cb->arr->vals.size() || cb->index >= cb->arr->conn.size() ||
      cb->index >= cb->arr->getIssued.size())
    return;
  cb->arr->getIssued[cb->index] = -1;
  double value = std::numeric_limits<double>::quiet_NaN();
  if (args.status == ECA_NORMAL && args.dbr && args.type == DBR_DOUBLE &&
      args.count >= 1)
    value = *static_cast<const double *>(args.dbr);
  if (std::isfinite(value)) {
    cb->arr->vals[cb->index] = value;
    cb->arr->conn[cb->index] = true;
  } else {
    cb->arr->vals[cb->index] = 0.0;
    cb->arr->conn[cb->index] = false;
  }
}

/**
 * @brief CA get callback for status PV updates.
 *
 * @param args Channel Access event arguments.
 */
static void statusCallback(struct event_handler_args args)
{
  GetCallbackData *cb = static_cast<GetCallbackData *>(args.usr);
  if (!cb || !cb->arr || cb->index < 0 ||
      cb->index >= cb->arr->statusVals.size() ||
      cb->index >= cb->arr->statusIssued.size())
    return;
  cb->arr->statusIssued[cb->index] = -1;
  if (args.status == ECA_NORMAL && args.dbr && args.type == DBR_ENUM &&
      args.count >= 1)
    cb->arr->statusVals[cb->index] = static_cast<short>(
      *static_cast<const dbr_enum_t *>(args.dbr));
  else
    cb->arr->statusVals[cb->index] = STATUS_NOTCONN;
}

/**
 * @brief Main application window for ADT.
 */
class MainWindow : public QMainWindow
{
public:
  explicit MainWindow(const QString &homeOverride, bool homeSpecified,
    const QString &startupPv, int zoomSect, int zoomInt, bool xorbitMode,
    QWidget *parent = nullptr)
    : QMainWindow(parent), initZoomSector(zoomSect), initZoomInterval(zoomInt),
      xorbit(xorbitMode)
  {
    auto logo = new LogoWidget(this);
    setCentralWidget(logo);
    setAutoFillBackground(true);
    QPalette mainPal = palette();
    mainPal.setColor(QPalette::Window, backgroundColor);
    setPalette(mainPal);

    if (QMenuBar *bar = menuBar())
      bar->setStyleSheet(
        "QMenuBar { background-color: #597EFF; color: white; }"
        "QMenuBar::item { background: transparent; color: white; }"
        "QMenuBar::item:hover { background-color: #4767D6; color: white; }"
        "QMenuBar::item:selected { background-color: #4767D6; color: white; }"
        "QMenuBar::item:pressed { background-color: #4767D6; color: white; }");

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
    QString snapDir;
    QList<LoadMenuInfo> menus = readInitFile(initFile, adtHome, pvDir,
      customDirectory, snapDir, xorbitMode);
    pvDirectory = pvDir;
    snapDirectory = snapDir;
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
    checkStatusMenu = optionsMenu->addMenu("Check Status");
    checkStatusMenu->setEnabled(!arrays.isEmpty() && arrays[0].statusCheck);
    auto statusGroup = new QActionGroup(this);
    const char *statusModes[] = { "None", "Invalid", "All" };
    for (int mode = 0; mode < 3; ++mode) {
      QAction *act = checkStatusMenu->addAction(statusModes[mode]);
      act->setCheckable(true);
      act->setChecked(mode == checkStatusMode);
      statusGroup->addAction(act);
      connect(act, &QAction::triggered, this, [this, mode]()
      {
        checkStatusMode = mode;
        resetGraph();
      });
    }
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
        arr.runMaxSet = false;
        arr.runSdevCount = 0;
        arr.runAvgCount = 0;
      }
      for (AreaData &area : areas)
        area.tempclear = true;
      if (zoomWidget)
        zoomArea.tempclear = true;
      resetGraph();
    });
    auto resetFunc = [this]() { resetFilledExtrema(); };
    QAction *resetAct = optionsMenu->addAction("Reset Max/Min");
    connect(resetAct, &QAction::triggered, this, [resetFunc](bool)
    {
      resetFunc();
    });
    resetFilledExtremaCallback = resetFunc;

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
    markersAct = viewMenu->addAction("Markers");
    markersAct->setCheckable(true);
    markersAct->setChecked(markers);
    connect(markersAct, &QAction::toggled, this, [this](bool checked)
    {
      markers = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    linesAct = viewMenu->addAction("Lines");
    linesAct->setCheckable(true);
    linesAct->setChecked(lines);
    connect(linesAct, &QAction::toggled, this, [this](bool checked)
    {
      lines = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    barsAct = viewMenu->addAction("Bars");
    barsAct->setCheckable(true);
    barsAct->setChecked(bars);
    connect(barsAct, &QAction::toggled, this, [this](bool checked)
    {
      bars = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    gridAct = viewMenu->addAction("Grid");
    gridAct->setCheckable(true);
    gridAct->setChecked(grid);
    connect(gridAct, &QAction::toggled, this, [this](bool checked)
    {
      grid = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    maxminAct = viewMenu->addAction("Max/Min");
    maxminAct->setCheckable(true);
    maxminAct->setChecked(showmaxmin);
    connect(maxminAct, &QAction::toggled, this, [this](bool checked)
    {
      showmaxmin = checked;
      for (auto aw : areaWidgets)
        aw->refresh();
    });
    fillAct = viewMenu->addAction("Filled Max/Min");
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

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->setObjectName("helpMenu");
    adt::populateHelpMenu(helpMenu, this);

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
    resetFilledExtremaCallback = {};
  }

  void storeSet(int n)
  {
    if (n < 1 || n > NSAVE || arrays.isEmpty())
      return;
    int idx = n - 1;
    for (ArrayData &arr : arrays) {
      arr.saveVals[idx] = arr.vals;
      arr.saveAvail[idx].resize(arr.nvals);
      for (int i = 0; i < arr.nvals; ++i)
        arr.saveAvail[idx][i] = arr.available(i);
    }
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
      if (arrays.isEmpty()) {
        QMessageBox::warning(this, "ADT", "There are no PV's defined");
        return;
      }
      for (const ArrayData &arr : arrays) {
        if (arr.saveVals[n - 1].size() != arr.nvals) {
          QMessageBox::warning(this, "ADT", "Data is not defined");
          return;
        }
      }
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
  QString snapDirectory;
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
  QAction *markersAct = nullptr;
  QAction *linesAct = nullptr;
  QAction *barsAct = nullptr;
  QAction *gridAct = nullptr;
  QAction *maxminAct = nullptr;
  QAction *fillAct = nullptr;
  QMenu *checkStatusMenu = nullptr;
  AreaData zoomArea;
  bool zoomOn = true;
  int initZoomSector = 0;
  int initZoomInterval = 0;
  bool xorbit = false;
  bool zoomSectorUsed = false;
  bool zoomIntervalUsed = false;
  int fileZoomInterval = 0;
  QVector<chid> channels;
  QTimer *pollTimer = nullptr;
  QHash<int, QSocketNotifier *> caNotifiers;
  int timeInterval = 2000;
  bool caStarted = false;
  int nsymbols = 0;

  void resetFilledExtrema()
  {
    nstat = 0.0;
    nstatTime = 0.0;
    for (ArrayData &arr : arrays) {
      arr.minVals.fill(LARGEVAL);
      arr.maxVals.fill(-LARGEVAL);
      arr.runSdev = 0.0;
      arr.runAvg = 0.0;
      arr.runMax = 0.0;
      arr.runMaxSet = false;
      arr.runSdevCount = 0;
      arr.runAvgCount = 0;
    }
    for (AreaData &area : areas)
      area.tempclear = true;
    resetGraph();
  }

  void resetGraph()
  {
    // Statistics follow the plotted quantity, which a menu choice can change.
    for (ArrayData &arr : arrays)
      updateArrayStatistics(arr);
    for (auto aw : areaWidgets)
      aw->refresh();
  }

  /**
   * @brief Compute one update's statistics of what is plotted for an array.
   *
   * In difference mode the plotted values are the current values minus the
   * difference set; with the reference displayed they are the current values
   * minus the reference; otherwise they are the current values. Only points
   * available in both operands and not excluded by Check Status count.
   */
  void updateArrayStatistics(ArrayData &arr)
  {
    const QVector<double> *subtract = nullptr;
    const QVector<bool> *subtractAvail = nullptr;
    if (::diffSet >= 0 && arr.saveVals[::diffSet].size() == arr.nvals) {
      subtract = &arr.saveVals[::diffSet];
      subtractAvail = &arr.saveAvail[::diffSet];
    } else if (referenceLoaded && refOn && arr.refVals.size() == arr.nvals) {
      subtract = &arr.refVals;
      subtractAvail = &arr.refAvail;
    }
    QVector<double> values;
    values.reserve(arr.nvals);
    for (int i = 0; i < arr.nvals; ++i) {
      if (!arr.available(i) || arr.excluded(i) ||
          (subtractAvail && !setAvailable(*subtractAvail, i)))
        continue;
      if (!arr.logScale) {
        values.append(subtract ? arr.vals[i] - (*subtract)[i] : arr.vals[i]);
        continue;
      }
      // Log arrays: linear scaled values of positive points, or the plotted
      // change in decades when a set or the reference is subtracted.
      double v = arr.scaleFactor * arr.vals[i];
      if (!(v > 0.0))
        continue;
      if (subtract) {
        double s = arr.scaleFactor * (*subtract)[i];
        if (!(s > 0.0))
          continue;
        values.append(std::log10(v) - std::log10(s));
      } else {
        values.append(v);
      }
    }
    Statistics st = computeStatistics(values);
    if (arr.logScale && !subtract) {
      // Display log10 of each linear statistic, as legacy ADT did; a
      // statistic that is not positive has no logarithm.
      st.sdevValid = st.sdevValid && st.sdev > 0.0;
      st.avgValid = st.avgValid && st.avg > 0.0;
      st.maxValid = st.maxValid && st.max > 0.0;
      st.sdev = st.sdevValid ? std::log10(st.sdev) : 0.0;
      st.avg = st.avgValid ? std::log10(st.avg) : 0.0;
      st.max = st.maxValid ? std::log10(st.max) : 0.0;
    }
    arr.sdev = st.sdev;
    arr.avg = st.avg;
    arr.maxVal = st.max;
    arr.sdevValid = st.sdevValid;
    arr.avgValid = st.avgValid;
    arr.maxValid = st.maxValid;
  }

  void updateViewActions()
  {
    if (markersAct)
      markersAct->setChecked(markers);
    if (linesAct)
      linesAct->setChecked(lines);
    if (barsAct)
      barsAct->setChecked(bars);
    if (gridAct)
      gridAct->setChecked(grid);
    if (maxminAct)
      maxminAct->setChecked(showmaxmin);
    if (fillAct)
      fillAct->setChecked(fillmaxmin);
  }

public:
  /**
   * @brief Track the socket that Channel Access signals when replies wait.
   */
  void caFdChanged(int fd, bool opened)
  {
    if (opened) {
      if (caNotifiers.contains(fd))
        return;
      auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
      connect(notifier, &QSocketNotifier::activated, this, [this]()
      {
        if (caStarted)
          ca_poll();
      });
      caNotifiers.insert(fd, notifier);
    } else {
      delete caNotifiers.take(fd);
    }
  }

private:
  void pollPvUpdate()
  {
    if (!caStarted)
      return;
    ca_poll();
    // Keep at most one get outstanding per channel. A reading whose get has
    // been outstanding longer than this is no longer current.
    qint64 now = monotonicMs();
    qint64 staleAfter = std::max<qint64>(2LL * timeInterval, 5000);
    for (ArrayData &arr : arrays) {
      for (int i = 0; i < arr.nvals; ++i) {
        if (!arr.chids[i] || ca_state(arr.chids[i]) != cs_conn) {
          arr.conn[i] = false;
          arr.vals[i] = 0.0;
          arr.getIssued[i] = -1;
        } else if (arr.getIssued[i] >= 0) {
          if (now - arr.getIssued[i] > staleAfter) {
            arr.conn[i] = false;
            arr.vals[i] = 0.0;
          }
        } else if (ca_array_get_callback(DBR_DOUBLE, 1, arr.chids[i],
            getCallback, &arr.cbData[i]) == ECA_NORMAL) {
          arr.getIssued[i] = now;
        } else {
          arr.conn[i] = false;
          arr.vals[i] = 0.0;
        }
      }
      if (!arr.statusCheck)
        continue;
      for (int i = 0; i < arr.nvals; ++i) {
        chid ch = arr.statusChids[i];
        if (arr.statusVals[i] == STATUS_UNUSED)
          continue;
        if (!ch || ca_state(ch) != cs_conn) {
          arr.statusVals[i] = STATUS_NOTCONN;
          arr.statusIssued[i] = -1;
        } else if (arr.statusIssued[i] >= 0) {
          if (now - arr.statusIssued[i] > staleAfter)
            arr.statusVals[i] = STATUS_NOTCONN;
        } else if (ca_array_get_callback(DBR_ENUM, 1, ch, statusCallback,
            &arr.statusCbData[i]) == ECA_NORMAL) {
          arr.statusIssued[i] = now;
        } else {
          arr.statusVals[i] = STATUS_NOTCONN;
        }
      }
    }
    ca_flush_io();
    for (ArrayData &arr : arrays) {
      for (int i = 0; i < arr.nvals; ++i) {
        if (!arr.conn[i])
          continue;
        double v = arr.vals[i];
        if (v < arr.minVals[i])
          arr.minVals[i] = v;
        if (v > arr.maxVals[i])
          arr.maxVals[i] = v;
      }
      updateArrayStatistics(arr);
    }
    nstat += 1.0;
    nstatTime += timeInterval;
    for (ArrayData &arr : arrays) {
      if (arr.sdevValid) {
        arr.runSdev += arr.sdev;
        ++arr.runSdevCount;
      }
      if (arr.avgValid) {
        arr.runAvg += arr.avg;
        ++arr.runAvgCount;
      }
      if (arr.maxValid && (std::fabs(arr.maxVal) > std::fabs(arr.runMax) ||
          (arr.logScale && !arr.runMaxSet))) {
        arr.runMax = arr.maxVal;
        arr.runMaxSet = true;
      }
    }
    for (auto aw : areaWidgets)
      aw->refresh();
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
      auto accumulated = [](double sum, int count) {
        if (count <= 0)
          return QString::asprintf("% 7.3f", 0.0);
        double v = sum / count;
        return std::isfinite(v) ? QString::asprintf("% 7.3f", v) : QString("    n/a");
      };
      msg += QString::asprintf("%5d %5d ", arr.index + 1, arr.nvals) +
        accumulated(arr.runSdev, arr.runSdevCount) + " " +
        accumulated(arr.runAvg, arr.runAvgCount) + " " +
        QString::asprintf("%7.3f\n", arr.runMax);
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

  /**
   * @brief Validate a saved slot before creating any export output.
   */
  bool validateExportSet(int nsave)
  {
    if (nsave < 0)
      return true;
    bool valid = nsave < NSAVE && !saveTime[nsave].isEmpty();
    if (valid) {
      for (const ArrayData &arr : arrays) {
        if (arr.saveVals[nsave].size() != arr.nvals) {
          valid = false;
          break;
        }
      }
    }
    if (!valid)
      QMessageBox::warning(this, "ADT",
        QString("Data is not defined for set %1").arg(nsave + 1));
    return valid;
  }

  bool writePlotFile(const QString &filename, int nsave = -1)
  {
    if (!validateExportSet(nsave))
      return false;
    FILE *file = std::tmpfile();
    if (!file)
      return false;

    char tbuf[26];
    if (nsave < 0) {
      time_t now = std::time(nullptr);
      std::strncpy(tbuf, std::ctime(&now), 24);
      tbuf[24] = '\0';
    } else {
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
    // The legacy plot format writes each count plus one (file.c writeplotfile).
    fprintf(file,
      "&parameter name=ADTNArrays type=short fixed_value=%d &end\n",
      static_cast<int>(arrays.size()) + 1);
    fprintf(file,
      "&parameter name=ADTNAreas type=short fixed_value=%d &end\n",
      static_cast<int>(areas.size()) + 1);
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
        vals = &arr.saveVals[nsave];
      }
      fprintf(file, "\n");
      fprintf(file, "%s\n", arr.heading.toUtf8().constData());
      fprintf(file, "%s\n", arr.units.toUtf8().constData());
      fprintf(file, "%s (%s)\n", arr.heading.toUtf8().constData(),
        arr.units.toUtf8().constData());
      fprintf(file, "%d     !ADTDisplayArea\n", arr.area->index + 1);
      for (int i = 0; i < arr.nvals; ++i) {
        // Unavailable points are left out; Index keeps the true positions.
        if (nsave < 0 ? !arr.available(i) : !setAvailable(arr.saveAvail[nsave], i))
          continue;
        fprintf(file, "%d ", i + 1);
        if (!writeSddsString(file, arr.names[i])) {
          fclose(file);
          return false;
        }
        fprintf(file, " %.*g\n",
          std::numeric_limits<double>::max_digits10, (*vals)[i]);
      }
    }
    return commitExport(file, filename);
  }

  bool readRefFile(const QString &filename)
  {
    QVector<QVector<double>> pendingValues(arrays.size());
    QVector<QVector<bool>> pendingAvail(arrays.size());
    SDDS_TABLE table;
    QByteArray fname = filename.toUtf8();
    SDDS_ClearErrors();
    if (!initializeSddsInput(&table, fname.data())) {
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
    // ADTAvailable marks unavailable points; files without it are fully available.
    int availCheck = SDDS_CheckColumn(&table, const_cast<char *>("ADTAvailable"),
      NULL, SDDS_ANY_INTEGER_TYPE, NULL);
    if (availCheck != SDDS_CHECK_OKAY && availCheck != SDDS_CHECK_NONEXISTENT) {
      QMessageBox::warning(this, "ADT",
        "ADTAvailable must be an integer column in reference file");
      SDDS_Terminate(&table);
      return false;
    }
    bool haveAvail = availCheck == SDDS_CHECK_OKAY;
    for (int ia = 0; ia < arrays.size(); ++ia) {
      if (SDDS_ReadTable(&table) <= 0) {
        QMessageBox::warning(this, "ADT",
          QString("Could not get Data for array %1 in %2")
            .arg(ia + 1).arg(filename));
        SDDS_Terminate(&table);
        return false;
      }
      if (ia == 0) {
        char *type = readStringParameter(&table, "ADTFileType");
        if (!type || strcmp(type, SNAPID)) {
          QMessageBox::warning(this, "ADT",
            QString("Not a valid ADT Reference/Snapshot file: %1")
              .arg(filename));
          SDDS_Free(type);
          SDDS_Terminate(&table);
          return false;
        }
        SDDS_Free(type);
      }
      int64_t nvals = SDDS_CountRowsOfInterest(&table);
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
      int32_t *rawavail = nullptr;
      if (haveAvail) {
        rawavail = SDDS_GetColumnInLong(&table, const_cast<char *>("ADTAvailable"));
        if (!rawavail) {
          QMessageBox::warning(this, "ADT",
            QString("Could not get ADTAvailable for array %1 in %2")
              .arg(ia + 1).arg(filename));
          freeSddsStrings(nvals, rawnames);
          freeSddsStrings(nvals, rawvalues);
          SDDS_Terminate(&table);
          return false;
        }
      }
      pendingValues[ia].resize(arrays[ia].nvals);
      pendingAvail[ia].fill(true, arrays[ia].nvals);
      for (int i = 0; i < nvals; ++i) {
        if (QString(rawnames[i]) != arrays[ia].names[i]) {
          QMessageBox::warning(this, "ADT",
            QString("PV does not match: %1 from %2")
              .arg(rawnames[i]).arg(filename));
          freeSddsStrings(nvals, rawnames);
          freeSddsStrings(nvals, rawvalues);
          SDDS_Free(rawavail);
          SDDS_Terminate(&table);
          return false;
        }
        if (rawavail && !rawavail[i]) {
          pendingValues[ia][i] = 0.0;
          pendingAvail[ia][i] = false;
          continue;
        }
        if (!parseSnapshotValue(rawvalues[i], pendingValues[ia][i])) {
          QMessageBox::warning(this, "ADT",
            QString("Invalid value for %1 in %2").arg(rawnames[i]).arg(filename));
          freeSddsStrings(nvals, rawnames);
          freeSddsStrings(nvals, rawvalues);
          SDDS_Free(rawavail);
          SDDS_Terminate(&table);
          return false;
        }
      }
      freeSddsStrings(nvals, rawnames);
      freeSddsStrings(nvals, rawvalues);
      SDDS_Free(rawavail);
    }
    if (!SDDS_Terminate(&table))
      return false;
    for (int ia = 0; ia < arrays.size(); ++ia) {
      arrays[ia].refVals = pendingValues[ia];
      arrays[ia].refAvail = pendingAvail[ia];
    }
    if (SDDS_NumberOfErrors())
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return true;
  }

  bool readSnapFile(const QString &filename, int nsave)
  {
    if (nsave < 0 || nsave >= NSAVE)
      return false;
    QVector<QVector<double>> pendingValues(arrays.size());
    QVector<QVector<bool>> pendingAvail(arrays.size());
    QString pendingTime;
    SDDS_TABLE table;
    QByteArray fname = filename.toUtf8();
    SDDS_ClearErrors();
    if (!initializeSddsInput(&table, fname.data())) {
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
    // ADTAvailable marks unavailable points; files without it are fully available.
    int availCheck = SDDS_CheckColumn(&table, const_cast<char *>("ADTAvailable"),
      NULL, SDDS_ANY_INTEGER_TYPE, NULL);
    if (availCheck != SDDS_CHECK_OKAY && availCheck != SDDS_CHECK_NONEXISTENT) {
      QMessageBox::warning(this, "ADT",
        "ADTAvailable must be an integer column in snapshot file");
      SDDS_Terminate(&table);
      return false;
    }
    bool haveAvail = availCheck == SDDS_CHECK_OKAY;
    for (int ia = 0; ia < arrays.size(); ++ia) {
      if (SDDS_ReadTable(&table) <= 0) {
        QMessageBox::warning(this, "ADT",
          QString("Could not get Data for array %1 in %2")
            .arg(ia + 1).arg(filename));
        SDDS_Terminate(&table);
        return false;
      }
      if (ia == 0) {
        char *type = readStringParameter(&table, "ADTFileType");
        if (!type || strcmp(type, SNAPID)) {
          QMessageBox::warning(this, "ADT",
            QString("Not a valid ADT Reference/Snapshot file: %1")
              .arg(filename));
          SDDS_Free(type);
          SDDS_Terminate(&table);
          return false;
        }
        SDDS_Free(type);
        char *time = readStringParameter(&table, "TimeStamp");
        if (time) {
          pendingTime = QString::fromUtf8(time);
          SDDS_Free(time);
        }
      }
      int64_t nvals = SDDS_CountRowsOfInterest(&table);
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
      int32_t *rawavail = nullptr;
      if (haveAvail) {
        rawavail = SDDS_GetColumnInLong(&table, const_cast<char *>("ADTAvailable"));
        if (!rawavail) {
          QMessageBox::warning(this, "ADT",
            QString("Could not get ADTAvailable for array %1 in %2")
              .arg(ia + 1).arg(filename));
          freeSddsStrings(nvals, rawnames);
          freeSddsStrings(nvals, rawvalues);
          SDDS_Terminate(&table);
          return false;
        }
      }
      pendingValues[ia].resize(arrays[ia].nvals);
      pendingAvail[ia].fill(true, arrays[ia].nvals);
      for (int i = 0; i < nvals; ++i) {
        if (QString(rawnames[i]) != arrays[ia].names[i]) {
          QMessageBox::warning(this, "ADT",
            QString("PV does not match: %1 from %2")
              .arg(rawnames[i]).arg(filename));
          freeSddsStrings(nvals, rawnames);
          freeSddsStrings(nvals, rawvalues);
          SDDS_Free(rawavail);
          SDDS_Terminate(&table);
          return false;
        }
        if (rawavail && !rawavail[i]) {
          pendingValues[ia][i] = 0.0;
          pendingAvail[ia][i] = false;
          continue;
        }
        if (!parseSnapshotValue(rawvalues[i], pendingValues[ia][i])) {
          QMessageBox::warning(this, "ADT",
            QString("Invalid value for %1 in %2").arg(rawnames[i]).arg(filename));
          freeSddsStrings(nvals, rawnames);
          freeSddsStrings(nvals, rawvalues);
          SDDS_Free(rawavail);
          SDDS_Terminate(&table);
          return false;
        }
      }
      freeSddsStrings(nvals, rawnames);
      freeSddsStrings(nvals, rawvalues);
      SDDS_Free(rawavail);
    }
    if (!SDDS_Terminate(&table))
      return false;
    for (int ia = 0; ia < arrays.size(); ++ia) {
      arrays[ia].saveVals[nsave] = pendingValues[ia];
      arrays[ia].saveAvail[nsave] = pendingAvail[ia];
    }
    saveTime[nsave] = pendingTime;
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
    QString dir = snapDirectory.isEmpty() ? QDir::currentPath() : snapDirectory;
    QString fn = QFileDialog::getOpenFileName(this, "Read Snapshot File", dir,
      "Snapshot Files (*.snap)");
    if (fn.isEmpty())
      return;
    snapDirectory = QFileInfo(fn).absolutePath();
    if (!readSnapFile(fn, n - 1))
      return;
    resetGraph();
  }

  bool writeSnapFile(const QString &filename, int nsave = -1)
  {
    if (!validateExportSet(nsave))
      return false;
    FILE *file = std::tmpfile();
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
    fprintf(file, "&column name=ADTAvailable type=short &end\n");
    fprintf(file,
      "&data mode=ascii no_row_counts=1 additional_header_lines=1 &end\n");
    for (const ArrayData &arr : arrays) {
      const QVector<double> *vals = nullptr;
      if (nsave < 0) {
        vals = &arr.vals;
      } else {
        vals = &arr.saveVals[nsave];
      }
      fprintf(file, "\n");
      fprintf(file, "%s (%s)\n", arr.heading.toUtf8().constData(),
        arr.units.toUtf8().constData());
      for (int i = 0; i < arr.nvals; ++i) {
        if (!writeSddsString(file, arr.names[i])) {
          fclose(file);
          return false;
        }
        // Unavailable points keep the 0 that older readers have always seen.
        bool avail = nsave < 0 ? arr.available(i) : setAvailable(arr.saveAvail[nsave], i);
        fprintf(file, " pv - 1 %.*g %d\n",
          std::numeric_limits<double>::max_digits10, avail ? (*vals)[i] : 0.0,
          avail ? 1 : 0);
      }
    }
    if (!commitExport(file, filename)) {
      QMessageBox::warning(this, "ADT",
        QString("Unable to write %1").arg(filename));
      return false;
    }
    return true;
  }

  void readReference()
  {
    if (arrays.isEmpty()) {
      QMessageBox::warning(this, "ADT", "There are no PV's defined");
      return;
    }
    QString dir = snapDirectory.isEmpty() ? QDir::currentPath() : snapDirectory;
    QString fn = QFileDialog::getOpenFileName(this,
      "Read Reference File", dir, "Snapshot Files (*.snap)");
    if (fn.isEmpty())
      return;
    snapDirectory = QFileInfo(fn).absolutePath();
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
    QString dir = snapDirectory.isEmpty() ? QDir::currentPath() : snapDirectory;
    QString fn = QFileDialog::getSaveFileName(this, "Write Snapshot File",
      dir, "Snapshot Files (*.snap)");
    if (fn.isEmpty())
      return;
    snapDirectory = QFileInfo(fn).absolutePath();
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
    QString dir = snapDirectory.isEmpty() ? QDir::currentPath() : snapDirectory;
    QString fn = QFileDialog::getSaveFileName(this, "Write Snapshot File",
      dir, "Snapshot Files (*.snap)");
    if (fn.isEmpty())
      return;
    snapDirectory = QFileInfo(fn).absolutePath();
    writeSnapFile(fn, idx);
  }

  void plotCurrent()
  {
    if (arrays.isEmpty()) {
      QMessageBox::warning(this, "ADT", "There are no PV's defined");
      return;
    }
    launchPlot(-1);
  }

  /**
   * @brief Write a plot file and show it with sddsplot in a detached shell.
   *
   * The plot outlives ADT and does not block it. The shell receives the file
   * name as a positional parameter, never inside its command text, and
   * removes the file when sddsplot exits. Errors from a running sddsplot go
   * to its own standard error.
   *
   * @param nsave Stored set index, or -1 for the current values.
   */
  void launchPlot(int nsave)
  {
    if (QStandardPaths::findExecutable("sddsplot").isEmpty()) {
      QMessageBox::warning(this, "ADT",
        "Unable to plot: sddsplot was not found on the PATH");
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
    if (!writePlotFile(fname, nsave)) {
      QFile::remove(fname);
      return;
    }
    const QString script =
      "sddsplot -graph=sym,connect -layout=1,2 -split=page -sep "
      "-ylabel=@TimeStamp -xlabel=Index -column=Index,Value \"$1\" "
      "-title=@ADTScreenTitle -end ; rm -f \"$1\"";
    if (!QProcess::startDetached("/bin/sh",
        QStringList() << "-c" << script << "adt-plot" << fname)) {
      QMessageBox::warning(this, "ADT", "Unable to start /bin/sh to run sddsplot");
      QFile::remove(fname);
    }
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
    launchPlot(idx);
  }

  void loadPvFile(const QString &file)
  {
    SDDS_ClearErrors();
    pvFilename = file;
    if (pollTimer)
      pollTimer->stop();
    if (checkStatusMenu)
      checkStatusMenu->setEnabled(false);
    timeInterval = 2000;

    if (caStarted) {
      for (chid ch : channels)
        ca_clear_channel(ch);
      channels.clear();
      ca_context_destroy();
      caStarted = false;
    }

    // Destroy widgets and their queued callbacks before releasing their data.
    delete takeCentralWidget();
    zoomPlot = nullptr;
    zoomAreaPtr = nullptr;
    setCentralWidget(new LogoWidget(this));
    arrays.clear();
    areas.clear();
    areaWidgets.clear();
    zoomWidget = nullptr;
    zoomAreaWidget = nullptr;
    fileZoomInterval = 0;
    nstat = 0.0;
    nstatTime = 0.0;
    for (int i = 0; i < NSAVE; ++i) {
      saveTime[i].clear();
      saveFilename[i].clear();
    }
    refFilename.clear();
    latFilename.clear();
    referenceLoaded = false;
    ::displaySet = -1;
    ::diffSet = -1;
    QString pendingReferenceFile;

    if (ca_context_create(ca_disable_preemptive_callback) != ECA_NORMAL) {
      QMessageBox::warning(this, "ADT", "Unable to start Channel Access");
      return;
    }
    caStarted = true;
    // Service Channel Access whenever its receive threads have replies, as
    // legacy ADT did through its file descriptor manager. Polling only at
    // the update interval lets a large layout's replies queue for several
    // intervals.
    ca_add_fd_registration(caFdRegistration, this);

    SDDS_TABLE table;
    QByteArray fname = file.toUtf8();
    if (!initializeSddsInput(&table, fname.data())) {
      QMessageBox::warning(this, "ADT", "Unable to read PV file:\n" + file);
      ca_context_destroy();
      channels.clear();
      caStarted = false;
      return;
    }

    // A failed load must not leave partially populated arrays for the next
    // timer or menu action to use.
    auto failLoad = [&](const QString &message) {
      SDDS_Terminate(&table);
      // Clear channels before destroying the context, as a reload does;
      // destroying the context alone leaks their names.
      for (chid ch : channels)
        ca_clear_channel(ch);
      ca_context_destroy();
      channels.clear();
      caStarted = false;
      arrays.clear();
      areas.clear();
      latNames.clear(); latS.clear(); latLen.clear(); latHeight.clear();
      nsect = 0;
      stotal = 0.0;
      latRing = false;
      QMessageBox::warning(this, "ADT", message);
    };
    const char *stringParameters[] = {"ADTFileType", "ADTLatticeFile",
      "ADTReferenceFile", "ADTColor", "ADTHeading", "ADTUnits"};
    for (const char *name : stringParameters) {
      int index = SDDS_GetParameterIndex(&table, const_cast<char *>(name));
      if (index >= 0 && SDDS_GetParameterType(&table, index) != SDDS_STRING) {
        failLoad(QString("PV file parameter %1 must be a string").arg(name));
        return;
      }
    }
    if (SDDS_CheckColumn(&table, const_cast<char *>("ControlName"), NULL,
        SDDS_STRING, NULL) != SDDS_CHECK_OKAY) {
      failLoad("PV file requires a string ControlName column");
      return;
    }
    // A StatusName column enables status checking unless -x was given, as in
    // legacy ADT.
    bool statusColumn = false;
    if (!xorbit) {
      int statusCheck = SDDS_CheckColumn(&table, const_cast<char *>("StatusName"),
        NULL, SDDS_STRING, NULL);
      if (statusCheck == SDDS_CHECK_WRONGTYPE) {
        failLoad("PV file StatusName column must be a string column");
        return;
      }
      statusColumn = statusCheck == SDDS_CHECK_OKAY;
    }

    bool first = true;
    bool oneAreaPerArray = false;
    int iarray;
    int narrays = 0;
    int nareas = 0;
    int pages = 0;
    int totalRows = 0;
    int32_t templong = 0;
    QStringList missingS;  // PVs without a lattice s value, reported once

    while ((iarray = SDDS_ReadTable(&table)) > 0) {
      ++pages;
      iarray--;
      if (first) {
        char *type = NULL;
        if (!SDDS_GetParameter(&table, const_cast<char *>("ADTFileType"), &type) ||
            strcmp(type, PVID)) {
          SDDS_Free(type);
          failLoad("Not a valid ADT PV file");
          return;
        }
        SDDS_Free(type);
        if (!SDDS_GetParameterAsLong(&table, const_cast<char *>("ADTNArrays"),
            &templong)) {
          failLoad("Missing ADTNArrays parameter");
          return;
        }
        narrays = templong;
        if (!SDDS_GetParameterAsLong(&table, const_cast<char *>("ADTNAreas"),
            &templong)) {
          oneAreaPerArray = true;
          templong = narrays;
        }
        nareas = templong;
        if (narrays <= 0 || nareas <= 0) {
          failLoad("ADTNArrays and ADTNAreas must be positive");
          return;
        }
        if (narrays > MAX_LAYOUT_ARRAYS || nareas > MAX_LAYOUT_AREAS) {
          failLoad(QString("PV file exceeds the limit of %1 arrays or %2 areas")
            .arg(MAX_LAYOUT_ARRAYS).arg(MAX_LAYOUT_AREAS));
          return;
        }
        arrays.resize(narrays);
        areas.resize(nareas);
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTTimeInterval"), &templong))
          timeInterval = templong > 0 ? static_cast<int>(templong) : 2000;
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTZoomInterval"), &templong)) {
          int interval = static_cast<int>(templong);
          fileZoomInterval = interval > 0 ? interval : 0;
        }
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTMarkers"), &templong))
          markers = templong != 0;
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTLines"), &templong))
          lines = templong != 0;
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTBars"), &templong))
          bars = templong != 0;
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTGrid"), &templong))
          grid = templong != 0;
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTMaxMin"), &templong))
          showmaxmin = templong != 0;
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTFillMaxMin"), &templong))
          fillmaxmin = templong != 0;
        updateViewActions();
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
            latRing = false;
          }
          SDDS_Free(latfile);
        } else {
          nsect = 0;
          stotal = 0.0;
          latNames.clear();
          latS.clear();
          latLen.clear();
          latHeight.clear();
          latRing = false;
        }
        char *reffile = NULL;
        if (SDDS_GetParameter(&table,
            const_cast<char *>("ADTReferenceFile"), &reffile)) {
          QString rf = reffile;
          if (!rf.isEmpty()) {
            if (!QDir::isAbsolutePath(rf)) {
              QString baseDir = snapDirectory.isEmpty() ?
                QDir::currentPath() : snapDirectory;
              rf = QDir(baseDir).filePath(rf);
            }
            pendingReferenceFile = rf;
          } else {
            pendingReferenceFile.clear();
          }
          SDDS_Free(reffile);
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
      if (!std::isfinite(arr.scaleFactor)) {
        failLoad("ADTScaleFactor must be finite");
        return;
      }

      if (SDDS_GetParameterAsLong(&table,
          const_cast<char *>("ADTZoomArea"), &templong))
        arr.zoom = templong != 0;
      else
        arr.zoom = false;
      if (SDDS_GetParameterAsLong(&table,
          const_cast<char *>("ADTLogScale"), &templong))
        arr.logScale = templong != 0;
      else
        arr.logScale = false;

      int iarea;
      if (oneAreaPerArray)
        iarea = iarray;
      else {
        if (SDDS_GetParameterAsLong(&table, const_cast<char *>("ADTDisplayArea"), &templong))
          iarea = templong > 0 ? static_cast<int>(templong) - 1 : -1;
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
        // Any ADTUnitsPerDiv selects the nearest scale, as in legacy ADT; values
        // at or below the smallest scale select it.
        if (!std::isfinite(center)) {
          failLoad("ADTCenterVal must be finite");
          return;
        }
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

      int64_t rowCount = SDDS_CountRowsOfInterest(&table);
      if (rowCount <= 0 || rowCount > std::numeric_limits<int>::max()) {
        failLoad("PV arrays must contain a positive, representable row count");
        return;
      }
      if (rowCount > MAX_LAYOUT_PVS - totalRows) {
        failLoad(QString("PV file exceeds the limit of %1 PVs").arg(MAX_LAYOUT_PVS));
        return;
      }
      int rows = static_cast<int>(rowCount);
      totalRows += rows;
      arr.nvals = rows;
      arr.names.clear();
      arr.vals.fill(0.0, rows);
      arr.s.fill(0.0, rows);
      arr.minVals.fill(LARGEVAL, rows);
      arr.maxVals.fill(-LARGEVAL, rows);
      arr.conn.fill(false, rows);
      arr.chids.clear();
      arr.cbData.resize(rows);
      arr.getIssued.fill(-1, rows);

      char **names = (char **)SDDS_GetColumn(&table, const_cast<char *>("ControlName"));
      if (!names) {
        failLoad("Unable to read PV ControlName column");
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
      arr.statusCheck = statusColumn;
      arr.statusChids.clear();
      arr.statusVals.fill(STATUS_UNUSED, rows);
      arr.statusCbData.resize(rows);
      arr.statusIssued.fill(-1, rows);
      if (statusColumn) {
        char **statusNames = (char **)SDDS_GetColumn(&table,
          const_cast<char *>("StatusName"));
        if (!statusNames) {
          failLoad("Unable to read PV StatusName column");
          return;
        }
        for (int i = 0; i < rows; ++i) {
          chid ch = 0;
          // A status name of "-" marks a point that is not checked.
          if (strcmp(statusNames[i], "-")) {
            arr.statusVals[i] = STATUS_NOTCONN;
            if (ca_create_channel(statusNames[i], NULL, NULL, 0, &ch) == ECA_NORMAL)
              channels.append(ch);
            else
              ch = 0;
          }
          arr.statusChids.append(ch);
          arr.statusCbData[i].arr = &arr;
          arr.statusCbData[i].index = i;
          SDDS_Free(statusNames[i]);
        }
        SDDS_Free(statusNames);
      }
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
            missingS << QString("%1 in array %2").arg(arr.names[i]).arg(iarray + 1);
        }
      }
    }

    if (iarray != -1 || first || pages != narrays) {
      failLoad("PV file is malformed, truncated, or does not match ADTNArrays");
      return;
    }
    SDDS_Terminate(&table);
    if (!missingS.isEmpty()) {
      const int shown = 20;
      QString list = QStringList(missingS.mid(0, shown)).join("\n");
      if (missingS.size() > shown)
        list += QString("\n... and %1 more").arg(missingS.size() - shown);
      QMessageBox::warning(this, "ADT",
        QString("Did not find an s value for %1 PVs in %2:\n%3")
        .arg(missingS.size()).arg(latFilename).arg(list));
    }

    if (ca_pend_io(1.0) == ECA_TIMEOUT) {
      QMessageBox *timeoutBox = new QMessageBox(QMessageBox::Warning, "ADT",
        "Timeout connecting to PVs.", QMessageBox::NoButton, this);
      timeoutBox->setAttribute(Qt::WA_DeleteOnClose);
      timeoutBox->show();
      QTimer::singleShot(3000, timeoutBox, &QMessageBox::accept);
    }

    QVector<QVector<dbr_enum_t>> statusRaw(arrays.size());
    QVector<QVector<bool>> statusRead(arrays.size());
    for (int ia = 0; ia < arrays.size(); ++ia) {
      ArrayData &arr = arrays[ia];
      for (int i = 0; i < arr.nvals; ++i) {
        if (arr.chids[i] && ca_state(arr.chids[i]) == cs_conn) {
          arr.conn[i] = ca_array_get(DBR_DOUBLE, 1, arr.chids[i],
            &arr.vals[i]) == ECA_NORMAL;
        } else {
          arr.vals[i] = 0.0;
          arr.conn[i] = false;
        }
      }
      if (!arr.statusCheck)
        continue;
      statusRaw[ia].fill(0, arr.nvals);
      statusRead[ia].fill(false, arr.nvals);
      for (int i = 0; i < arr.nvals; ++i) {
        chid ch = arr.statusChids[i];
        if (ch && ca_state(ch) == cs_conn)
          statusRead[ia][i] = ca_array_get(DBR_ENUM, 1, ch,
            &statusRaw[ia][i]) == ECA_NORMAL;
      }
    }
    bool initialReadsComplete = ca_pend_io(1.0) == ECA_NORMAL;
    // Synchronous gets do not identify which individual reads timed out.
    // Keep failed or nonfinite readings unavailable until a valid callback.
    for (int ia = 0; ia < arrays.size(); ++ia) {
      ArrayData &arr = arrays[ia];
      for (int i = 0; i < arr.nvals; ++i) {
        if (!initialReadsComplete || !arr.conn[i] || !std::isfinite(arr.vals[i])) {
          arr.conn[i] = false;
          arr.vals[i] = 0.0;
        }
        if (arr.statusCheck && arr.statusVals[i] != STATUS_UNUSED)
          arr.statusVals[i] = initialReadsComplete && statusRead[ia][i] ?
            static_cast<short>(statusRaw[ia][i]) : STATUS_NOTCONN;
      }
    }

    for (ArrayData &arr : arrays) {
      for (int i = 0; i < arr.nvals; ++i) {
        if (!arr.conn[i])
          continue;
        arr.minVals[i] = arr.vals[i];
        arr.maxVals[i] = arr.vals[i];
      }
      updateArrayStatistics(arr);
      arr.runSdev = 0.0;
      arr.runAvg = 0.0;
      arr.runMax = 0.0;
      arr.runMaxSet = false;
      arr.runSdevCount = 0;
      arr.runAvgCount = 0;
    }

    nsymbols = latNames.size();

    QWidget *central = new QWidget;
    central->setAutoFillBackground(true);
    QPalette centralPal = central->palette();
    centralPal.setColor(QPalette::Window, backgroundColor);
    central->setPalette(centralPal);
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
      if (!zoomSectorUsed || !zoomIntervalUsed || fileZoomInterval > 0) {
        int center = 0;
        int interval = 0;
        if (!zoomSectorUsed && initZoomSector > 0) {
          center = initZoomSector;
          zoomSectorUsed = true;
        }
        if (!zoomIntervalUsed && initZoomInterval > 0) {
          interval = initZoomInterval;
          zoomIntervalUsed = true;
        } else if (fileZoomInterval > 0) {
          interval = fileZoomInterval;
        }
        if (center > 0 || interval > 0)
          zoomWidget->setZoom(center, interval);
      }
    }
    setCentralWidget(central);

    if (!pendingReferenceFile.isEmpty()) {
      if (readRefFile(pendingReferenceFile)) {
        referenceLoaded = true;
        refFilename = pendingReferenceFile;
        resetGraph();
      } else {
        referenceLoaded = false;
        refFilename.clear();
      }
    }

    // A startup -f file is loaded before the Options menu exists.
    if (checkStatusMenu)
      checkStatusMenu->setEnabled(statusColumn);
    setWindowTitle("ADT - " + QFileInfo(file).fileName());
    pollTimer->start(timeInterval);
  }
};

/**
 * @brief Channel Access file descriptor registration callback.
 *
 * Called from ca_create_channel and ca_context_destroy on the main thread.
 */
static void caFdRegistration(void *arg, int fd, int opened)
{
  static_cast<MainWindow *>(arg)->caFdChanged(fd, opened != 0);
}

static void usage(const char *prog) {
  fprintf(stderr,
    "Usage: %s [-a directory] [-f pvfile] [-s center_sector] [-z number_of_sectors] [-d] [-x] [-h|-?]\n"
    "  -a <directory>          specify ADT home directory\n"
    "  -f <file>               open PV file at startup\n"
    "  -s <center_sector>      set initial zoomed on sector\n"
    "  -z <number_of_sectors>  set initial zoom range\n"
    "  -d                      enable diff mode\n"
    "  -x                      use Xorbit directories; no status checking\n"
    "  -h, -?                  show this help message and exit\n"
    "Options may also begin with /. Qt options such as -display are accepted.\n"
    "Exit status: 0 after help; 8 for an unknown option, a missing operand,\n"
    "a non-integer -s or -z value, or an unexpected argument.\n",
    prog);
}

/**
 * @brief Match a one-letter option written as -x or /x.
 */
static bool isOption(const char *arg, char letter)
{
  return (arg[0] == '-' || arg[0] == '/') && arg[1] == letter && arg[2] == '\0';
}

/**
 * @brief Whether an option takes an operand.
 */
static bool takesOperand(const char *arg)
{
  return isOption(arg, 'a') || isOption(arg, 'f') || isOption(arg, 's') ||
    isOption(arg, 'z');
}

int main(int argc, char **argv)
{
  // Help needs no display, so look for it before QApplication opens one.
  for (int i = 1; i < argc; ++i) {
    if (takesOperand(argv[i])) {
      ++i;
    } else if (isOption(argv[i], 'h') || isOption(argv[i], '?')) {
      usage(argv[0]);
      return 0;
    }
  }
  // QApplication removes the Qt options, such as -display and -style.
  QApplication app(argc, argv);
  QString startPv;
  QString homeOverride;
  bool homeSpecified = false;
  int zoomSect = 0;
  int zoomInt = 0;
  bool diffMode = false;
  bool xorbitMode = false;
  // Reject what legacy ADT rejected, with its exit status 8.
  auto invalid = [&argv](const QString &option) {
    fprintf(stderr, "Invalid option %s\n", option.toLocal8Bit().constData());
    usage(argv[0]);
    return 8;
  };
  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (takesOperand(arg)) {
      if (i + 1 >= argc)
        return invalid(arg);
      QString value = QString::fromLocal8Bit(argv[++i]);
      if (isOption(arg, 'a')) {
        homeOverride = value;
        homeSpecified = true;
      } else if (isOption(arg, 'f')) {
        startPv = value;
      } else {
        bool ok = false;
        int number = value.toInt(&ok);
        if (!ok)
          return invalid(QString("%1 %2").arg(arg, value));
        if (isOption(arg, 's'))
          zoomSect = number;
        else
          zoomInt = number;
      }
    } else if (isOption(arg, 'd')) {
      diffMode = true;
    } else if (isOption(arg, 'x')) {
      xorbitMode = true;
    } else {
      return invalid(arg);
    }
  }
  MainWindow win(homeOverride, homeSpecified, startPv, zoomSect, zoomInt,
    xorbitMode);
  win.show();
  if (diffMode)
    QTimer::singleShot(1000, &win, [&win]() { win.storeSet(1); win.diffSet(1); });
  return app.exec();
}
