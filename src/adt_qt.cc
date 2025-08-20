/**
 * @file adt_qt.cc
 * @brief Minimal Qt-based entry for ADT.
 */

#include "aps.icon"
#include "adtVersion.h"

#include <sys/utsname.h>

#include <epicsVersion.h>
#include <QApplication>
#include <QBitmap>
#include <QImage>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFont>
#include <QByteArray>
#include <QFontMetrics>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainterPath>
#include <QMessageBox>
#include <QDoubleSpinBox>
#include <QString>
#include <QWidget>
#include <QList>
#include <QVector>
#include <QColor>
#include <limits>
#include <cmath>
#include <cstring>
#include <cstdint>

#include <SDDS.h>
#include <QDir>
#include <cadef.h>

#define INITFILENAME "adtrc"

static constexpr int GRIDDIVISIONS = 5;
static const char *PVID = "ADTPV";
static constexpr double LARGEVAL = 1e40;

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

struct AreaData
{
  int index = 0;
  int currScale = 0;
  double centerVal = 0.0;
  double xmin = 0.0;
  double xmax = 0.0;
  bool initialized = false;
};

struct ArrayData
{
  int index = 0;
  int nvals = 0;
  QVector<QString> names;
  QVector<double> vals;
  QVector<double> minVals;
  QVector<double> maxVals;
  QVector<bool> conn;
  QVector<chid> chids;
  QString heading;
  QString units;
  double scaleFactor = 1.0;
  double sdev = 0.0;
  double avg = 0.0;
  double maxVal = 0.0;
  AreaData *area = nullptr;
  QColor color;
};

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
    if (files) {
      for (int i = 0; i < rows; ++i)
        SDDS_Free(files[i]);
      SDDS_Free(files);
    }
    if (labels) {
      for (int i = 0; i < rows; ++i)
        SDDS_Free(labels[i]);
      SDDS_Free(labels);
    }
    menus.append(menu);
  }
  SDDS_Terminate(&table);
  return menus;
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

    QFont font("Monospace", 9);
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

    struct utsname name;
    if (uname(&name) != -1) {
      drawCentered(QString("Running %1 %2 on %3")
        .arg(name.sysname)
        .arg(name.release)
        .arg(name.nodename), ystart + 2 * linespacing);
    }

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
    QPainter p(this);
    p.fillRect(rect(), Qt::white);

    int w = width();
    int h = height();
    int headerHeight = 20;
    QRect plotRect(0, headerHeight, w - 1, h - headerHeight - 1);

    p.setPen(Qt::black);
    p.drawRect(plotRect);

    for (int i = 1; i < GRIDDIVISIONS; ++i) {
      int x = plotRect.left() + i * plotRect.width() / GRIDDIVISIONS;
      int y = plotRect.top() + i * plotRect.height() / GRIDDIVISIONS;
      p.drawLine(plotRect.left(), y, plotRect.right(), y);
      p.drawLine(x, plotRect.top(), x, plotRect.bottom());
    }

    double upd = scale[area->currScale];
    double ymin = area->centerVal - upd * GRIDDIVISIONS;
    double ymax = area->centerVal + upd * GRIDDIVISIONS;

    auto mapY = [&](double v) {
      if (v > ymax)
        v = ymax;
      if (v < ymin)
        v = ymin;
      double frac = (v - ymin) / (ymax - ymin);
      return plotRect.bottom() - static_cast<int>(frac * plotRect.height());
    };

    for (auto arr : arrayPtrs) {
      if (arr->nvals < 1)
        continue;
      p.setPen(arr->color);
      double xstep = arr->nvals > 1 ? plotRect.width() /
        static_cast<double>(arr->nvals - 1) : 0.0;
      QPainterPath path;
      path.moveTo(plotRect.left(), mapY(arr->vals[0]));
      for (int i = 1; i < arr->nvals; ++i)
        path.lineTo(plotRect.left() + static_cast<int>(i * xstep),
          mapY(arr->vals[i]));
      p.drawPath(path);
    }

    QString head = arrayPtrs[0]->heading;
    if (!arrayPtrs[0]->units.isEmpty())
      head += QString(" (%1)").arg(arrayPtrs[0]->units);
    p.drawText(5, 15, head);
  }

private:
  AreaData *area;
  QVector<ArrayData *> arrayPtrs;
};

/**
 * @brief Combines scale and center controls with a plot widget.
 */
class AreaWidget : public QWidget
{
public:
  AreaWidget(AreaData *adata, const QVector<ArrayData *> &arrays,
    QWidget *parent = nullptr)
    : QWidget(parent), area(adata),
      primaryArray(arrays.isEmpty() ? nullptr : arrays[0])
  {
    auto vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    auto top = new QHBoxLayout;
    vbox->addLayout(top);

    auto scaleLabel = new QLabel("Scale:");
    top->addWidget(scaleLabel);
    scaleSpin = new QDoubleSpinBox;
    scaleSpin->setDecimals(3);
    scaleSpin->setRange(scale[0], scale[NSCALES - 1]);
    scaleSpin->setValue(scale[area->currScale]);
    double step = (area->currScale > 0) ?
      scale[area->currScale] - scale[area->currScale - 1] :
      (NSCALES > 1 ? scale[1] - scale[0] : scale[0]);
    scaleSpin->setSingleStep(step);
    top->addWidget(scaleSpin);

    auto centerLabel = new QLabel("Center:");
    top->addWidget(centerLabel);
    centerSpin = new QDoubleSpinBox;
    centerSpin->setDecimals(3);
    centerSpin->setRange(-LARGEVAL, LARGEVAL);
    centerSpin->setValue(area->centerVal);
    top->addWidget(centerSpin);
    top->addStretch();

    auto sdevText = new QLabel("SDEV:");
    top->addWidget(sdevText);
    sdevLabel = new QLabel;
    top->addWidget(sdevLabel);

    auto avgText = new QLabel("    AVG:");
    top->addWidget(avgText);
    avgLabel = new QLabel;
    top->addWidget(avgLabel);

    auto maxText = new QLabel("    MAX:");
    top->addWidget(maxText);
    maxLabel = new QLabel;
    top->addWidget(maxLabel);

    plot = new PlotWidget(adata, arrays, this);
    vbox->addWidget(plot, 1);

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
      scaleSpin->blockSignals(false);
      plot->update();
    });

    connect(centerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
      this, [this](double val)
    {
      area->centerVal = val;
      area->xmax = area->centerVal + scale[area->currScale] * GRIDDIVISIONS;
      area->xmin = area->centerVal - scale[area->currScale] * GRIDDIVISIONS;
      plot->update();
    });

    updateStats();
    setMinimumSize(800, 150);
  }

private:
  void updateStats()
  {
    if (!primaryArray)
      return;
    sdevLabel->setText(QString("%1").arg(
      primaryArray->sdev * primaryArray->scaleFactor, 0, 'f', 3));
    avgLabel->setText(QString("%1").arg(
      primaryArray->avg * primaryArray->scaleFactor, 0, 'f', 3));
    maxLabel->setText(QString("%1").arg(
      primaryArray->maxVal * primaryArray->scaleFactor, 0, 'f', 3));
  }

  AreaData *area;
  ArrayData *primaryArray;
  PlotWidget *plot;
  QDoubleSpinBox *scaleSpin;
  QDoubleSpinBox *centerSpin;
  QLabel *sdevLabel;
  QLabel *avgLabel;
  QLabel *maxLabel;
};

class MainWindow : public QMainWindow
{
public:
  explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent)
  {
    constexpr int NSAVE = 5;
    auto logo = new LogoWidget(this);
    setCentralWidget(logo);

    QMenu *fileMenu = menuBar()->addMenu("File");
    QString adtHome = qEnvironmentVariable("ADTHOME", ".");
    QString initFile = QDir::home().filePath("." INITFILENAME);
    if (!QFile::exists(initFile))
      initFile = QDir(adtHome).filePath(INITFILENAME);
    QString pvDir;
    QList<LoadMenuInfo> menus = readInitFile(initFile, adtHome, pvDir,
      customDirectory);
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
    fileMenu->addAction("Read Reference");
    QMenu *readSnapMenu = fileMenu->addMenu("Read Snapshot");
    for (int i = 1; i <= NSAVE; ++i)
      readSnapMenu->addAction(QString::number(i));
    QMenu *writeMenu = fileMenu->addMenu("Write");
    writeMenu->addAction("Current");
    for (int i = 1; i <= NSAVE; ++i)
      writeMenu->addAction(QString::number(i));
    QMenu *plotMenu = fileMenu->addMenu("Plot");
    plotMenu->addAction("Current");
    for (int i = 1; i <= NSAVE; ++i)
      plotMenu->addAction(QString::number(i));
    fileMenu->addAction("Status...");
    fileMenu->addSeparator();
    fileMenu->addAction("Quit");

    QMenu *optionsMenu = menuBar()->addMenu("Options");
    QMenu *epicsMenu = optionsMenu->addMenu("EPICS");
    epicsMenu->addAction("Start");
    epicsMenu->addAction("Reinitialize");
    epicsMenu->addAction("Rescan");
    epicsMenu->addAction("Exit");
    QMenu *storeMenu = optionsMenu->addMenu("Store");
    for (int i = 1; i <= NSAVE; ++i)
      storeMenu->addAction(QString::number(i));
    QMenu *displayMenu = optionsMenu->addMenu("Display");
    displayMenu->addAction("Off");
    for (int i = 1; i <= NSAVE; ++i)
      displayMenu->addAction(QString::number(i));
    QMenu *diffMenu = optionsMenu->addMenu("Difference");
    diffMenu->addAction("Off");
    for (int i = 1; i <= NSAVE; ++i)
      diffMenu->addAction(QString::number(i));
    QMenu *checkMenu = optionsMenu->addMenu("Check Status");
    checkMenu->addAction("Off");
    checkMenu->addAction("Check InValid");
    checkMenu->addAction("Check All");
    QAction *refAct = optionsMenu->addAction("Reference Enabled");
    refAct->setCheckable(true);
    QAction *zoomAct = optionsMenu->addAction("Zoom Enabled");
    zoomAct->setCheckable(true);
    QAction *statAct = optionsMenu->addAction("Accumulated Statistics");
    statAct->setCheckable(true);
    optionsMenu->addAction("Reset Max/Min");

    QMenu *viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction("Timing...");
    QAction *markersAct = viewMenu->addAction("Markers");
    markersAct->setCheckable(true);
    QAction *linesAct = viewMenu->addAction("Lines");
    linesAct->setCheckable(true);
    QAction *barsAct = viewMenu->addAction("Bars");
    barsAct->setCheckable(true);
    QAction *gridAct = viewMenu->addAction("Grid");
    gridAct->setCheckable(true);
    QAction *maxminAct = viewMenu->addAction("Max/Min");
    maxminAct->setCheckable(true);
    QAction *fillAct = viewMenu->addAction("Filled Max/Min");
    fillAct->setCheckable(true);

    QMenu *clearMenu = menuBar()->addMenu("Clear");
    clearMenu->addAction("Clear");
    clearMenu->addAction("Redraw");
    clearMenu->addAction("Update");
    QAction *autoAct = clearMenu->addAction("Autoclear");
    autoAct->setCheckable(true);

    QMenu *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("Contents");
    helpMenu->addAction("New Features");
    helpMenu->addAction("Overview");
    helpMenu->addAction("Mouse Operations");
    helpMenu->addAction("Color Code");
    QAction *versionAct = helpMenu->addAction("Version");
    connect(versionAct, &QAction::triggered, this, [this]()
    {
      struct utsname info;
      QString unameStr;
      if (uname(&info) != -1) {
        unameStr = QString("Running %1 %2 on %3")
          .arg(info.sysname)
          .arg(info.release)
          .arg(info.nodename);
      }
      QString msg = QString("%1 %2\n\nWritten by Dwarfs in the Waterfall Glen\n\n%3")
        .arg(ADT_VERSION_STRING)
        .arg(EPICS_VERSION_STRING)
        .arg(unameStr);
      QMessageBox::information(this, "ADT Version", msg);
    });
  }

  ~MainWindow() override
  {
    for (chid ch : channels)
      ca_clear_channel(ch);
    if (caStarted)
      ca_context_destroy();
  }

private:
  QString customDirectory;
  QString pvFilename;
  QVector<ArrayData> arrays;
  QVector<AreaData> areas;
  QVector<chid> channels;
  bool caStarted = false;

  void loadPvFile(const QString &file)
  {
    pvFilename = file;

    if (caStarted) {
      for (chid ch : channels)
        ca_clear_channel(ch);
      channels.clear();
      ca_context_destroy();
      caStarted = false;
    }

    arrays.clear();
    areas.clear();

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
          if (type)
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
      arr.minVals.fill(LARGEVAL, rows);
      arr.maxVals.fill(-LARGEVAL, rows);
      arr.conn.fill(false, rows);
      arr.chids.clear();

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
        SDDS_Free(names[i]);
      }
      SDDS_Free(names);
    }

    SDDS_Terminate(&table);

    if (ca_pend_io(5.0) == ECA_TIMEOUT)
      QMessageBox::warning(this, "ADT", "Timeout connecting to PVs");

    for (ArrayData &arr : arrays) {
      for (int i = 0; i < arr.nvals; ++i)
        ca_array_get(DBR_DOUBLE, 1, arr.chids[i], &arr.vals[i]);
    }
    ca_pend_io(5.0);

    for (ArrayData &arr : arrays) {
      double sum = 0.0;
      double sumsq = 0.0;
      double maxv = 0.0;
      for (int i = 0; i < arr.nvals; ++i) {
        double v = arr.vals[i];
        sum += v;
        sumsq += v * v;
        if (i == 0 || std::fabs(v) > std::fabs(maxv))
          maxv = v;
      }
      if (arr.nvals > 0) {
        arr.avg = sum / arr.nvals;
        arr.sdev = std::sqrt(sumsq / arr.nvals - arr.avg * arr.avg);
        arr.maxVal = maxv;
      } else {
        arr.avg = arr.sdev = arr.maxVal = 0.0;
      }
    }

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
      }
    }
    setCentralWidget(central);

    setWindowTitle("ADT - " + QFileInfo(file).fileName());
    QMessageBox::information(this, "ADT",
      QString("Loaded %1 PVs from:\n%2").arg(channels.size()).arg(file));
  }
};

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  MainWindow win;
  win.show();
  return app.exec();
}
