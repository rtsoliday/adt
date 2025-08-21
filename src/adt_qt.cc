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
#include <QPainterPath>
#include <QMessageBox>
#include <QMouseEvent>
#include <QDoubleSpinBox>
#include <QString>
#include <QStringList>
#include <QLineEdit>
#include <QWidget>
#include <QList>
#include <QVector>
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
#include <cstdint>
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

static bool markers = true;
static bool lines = true;
static bool bars = false;
static bool grid = true;
static bool autoclear = true;
static bool showmaxmin = true;
static bool fillmaxmin = true;

struct AreaData
{
  int index = 0;
  int currScale = 0;
  double centerVal = 0.0;
  double xmin = 0.0;
  double xmax = 0.0;
  bool initialized = false;
  bool tempclear = false;
  bool tempnodraw = false;
  int xStart = 0;
  int xEnd = -1;
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
};

class PlotWidget;

static PlotWidget *zoomPlot = nullptr;
static AreaData *zoomAreaPtr = nullptr;

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
    if (pixmap.size() != size()) {
      pixmap = QPixmap(size());
      pixmap.fill(Qt::white);
    }
    QPainter pmap(&pixmap);
    if (autoclear || area->tempclear)
      pmap.fillRect(pixmap.rect(), Qt::white);
    area->tempclear = false;

    int w = pixmap.width();
    int h = pixmap.height();
    int headerHeight = 0;
    QRect plotRect(0, headerHeight, w - 1, h - headerHeight - 1);

    pmap.setPen(Qt::black);
    pmap.drawRect(plotRect);

    double upd = scale[area->currScale];
    double ymin = area->centerVal - upd * GRIDDIVISIONS;
    double ymax = area->centerVal + upd * GRIDDIVISIONS;

    auto mapY = [&](double v) {
      // Allow values to map outside of the visible plotting area so that
      // data points and connecting lines can extend beyond the edges when
      // necessary. This avoids clamping out-of-range values to the plot
      // boundaries, which previously caused points to appear on the edge of
      // the graph.
      double frac = (v - ymin) / (ymax - ymin);
      return plotRect.bottom() - static_cast<int>(frac * plotRect.height());
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
          QPainterPath path;
          path.moveTo(xstart, mapY(arr->minVals[start]));
          for (int i = start + 1; i < end; ++i)
            path.lineTo(xstart + (i - start) * xstep, mapY(arr->minVals[i]));
          for (int i = end - 1; i >= start; --i)
            path.lineTo(xstart + (i - start) * xstep, mapY(arr->maxVals[i]));
          path.closeSubpath();
          pmap.fillPath(path, Qt::lightGray);
        } else {
          pmap.setPen(arr->color);
          QPainterPath pathMin;
          pathMin.moveTo(xstart, mapY(arr->minVals[start]));
          for (int i = start + 1; i < end; ++i)
            pathMin.lineTo(xstart + (i - start) * xstep,
              mapY(arr->minVals[i]));
          pmap.drawPath(pathMin);
          QPainterPath pathMax;
          pathMax.moveTo(xstart, mapY(arr->maxVals[start]));
          for (int i = start + 1; i < end; ++i)
            pathMax.lineTo(xstart + (i - start) * xstep,
              mapY(arr->maxVals[i]));
          pmap.drawPath(pathMax);
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

    for (auto arr : arrayPtrs) {
      if (arr->nvals < 1)
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
      double xstart = plotRect.left() + xstep / 2.0; // add half-bin margin
      if (bars) {
        pmap.setPen(arr->color);
        for (int i = start; i < end; ++i) {
          int x = static_cast<int>(xstart + (i - start) * xstep);
          int y = mapY(arr->vals[i]);
          pmap.drawLine(x, y0, x, y);
        }
      } else if (lines) {
        pmap.setPen(arr->color);
        QPainterPath path;
        path.moveTo(xstart, mapY(arr->vals[start]));
        for (int i = start + 1; i < end; ++i)
          path.lineTo(xstart + (i - start) * xstep, mapY(arr->vals[i]));
        pmap.drawPath(path);
      }
      if (markers) {
        pmap.setPen(arr->color);
        pmap.setBrush(arr->color);
        for (int i = start; i < end; ++i) {
          int x = static_cast<int>(xstart + (i - start) * xstep);
          int y = mapY(arr->vals[i]);
          pmap.drawRect(x - 1, y - 1, 3, 3);
        }
        pmap.setBrush(Qt::NoBrush);
      }
    }

    QPainter pw(this);
    pw.drawPixmap(0, 0, pixmap);
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      int w = width();
      int h = height();
      int headerHeight = 0;
      QRect plotRect(0, headerHeight, w - 1, h - headerHeight - 1);
      if (!plotRect.contains(event->pos()) || arrayPtrs.isEmpty()) {
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
      int nmid = static_cast<int>((event->pos().x() - plotRect.left()) /
        xstep + 0.5);
      QString info;
      for (auto arr : arrayPtrs) {
        info += arr->heading + "\n";
        for (int idx = nmid - 1; idx <= nmid + 1; ++idx) {
          if (idx >= 0 && idx < arr->nvals) {
            double val = arr->scaleFactor * arr->vals[idx];
            QString line = QString("%1%2 %3  %4\n")
              .arg(idx == nmid ? "->" : "  ")
              .arg(idx + 1)
              .arg(arr->names[idx])
              .arg(val, 7, 'f', 3);
            info += line;
          }
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
    } else if (event->button() == Qt::MiddleButton) {
      if (zoomPlot && zoomAreaPtr && this != zoomPlot && !arrayPtrs.isEmpty()) {
        int w = width();
        int h = height();
        int headerHeight = 0;
        QRect plotRect(0, headerHeight, w - 1, h - headerHeight - 1);
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
        int span = zoomAreaPtr->xEnd >= zoomAreaPtr->xStart ?
          (zoomAreaPtr->xEnd - zoomAreaPtr->xStart + 1) : nvals / 10;
        if (span < 1)
          span = 1;
        int half = span / 2;
        int start = idx - half;
        int end = start + span - 1;
        if (start < 0) {
          end -= start;
          start = 0;
        }
        if (end >= nvals) {
          start -= end - (nvals - 1);
          end = nvals - 1;
          if (start < 0)
            start = 0;
        }
        zoomAreaPtr->xStart = start;
        zoomAreaPtr->xEnd = end;
        zoomPlot->update();
      } else {
        QWidget::mousePressEvent(event);
      }
    } else {
      QWidget::mousePressEvent(event);
    }
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton && infoBox) {
      int w = width();
      int h = height();
      int headerHeight = 0;
      QRect plotRect(0, headerHeight, w - 1, h - headerHeight - 1);
      if (plotRect.contains(event->pos()))
        infoBox->close();
      else
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
};

/**
 * @brief Combines scale and center controls with a plot widget.
 */
class AreaWidget : public QWidget
{
public:
  AreaWidget(AreaData *adata, const QVector<ArrayData *> &arrays,
    QWidget *parent = nullptr)
    : QWidget(parent), area(adata), arrayPtrs(arrays)
  {
    auto vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);

    auto titleBox = new QVBoxLayout;
    vbox->addLayout(titleBox);
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QFontMetrics fm(fixedFont);
    int statWidth = fm.horizontalAdvance(" 000.000");
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

    auto controls = new QHBoxLayout;
    vbox->addLayout(controls);

    auto scaleLabel = new QLabel("Scale:");
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

    auto centerLabel = new QLabel("Center:");
    controls->addWidget(centerLabel);
    centerSpin = new QDoubleSpinBox;
    centerSpin->setDecimals(3);
    centerSpin->setRange(-LARGEVAL, LARGEVAL);
    centerSpin->setValue(area->centerVal);
    controls->addWidget(centerSpin);
    controls->addStretch();

    plot = new PlotWidget(adata, arrays, this);
    vbox->addWidget(plot, 1);
    int plotHeight = 148 + 1;
    plot->setMinimumHeight(plotHeight);
    int topHeight = controls->sizeHint().height() + titleBox->sizeHint().height();
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
    setMinimumWidth(800);
  }

  PlotWidget *plotWidget() const
  {
    return plot;
  }

  void refresh()
  {
    updateStats();
    plot->update();
  }

private:
  void updateStats()
  {
    for (int i = 0; i < arrayPtrs.size() && i < stats.size(); ++i) {
      auto arr = arrayPtrs[i];
      auto sl = stats[i];
      sl.sdev->setText(QString("%1").arg(
        arr->sdev * arr->scaleFactor, 0, 'f', 3));
      sl.avg->setText(QString("%1").arg(
        arr->avg * arr->scaleFactor, 0, 'f', 3));
      sl.max->setText(QString("%1").arg(
        arr->maxVal * arr->scaleFactor, 0, 'f', 3));
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
  QVector<StatLabels> stats;
};

class MainWindow : public QMainWindow
{
public:
  explicit MainWindow(const QString &homeOverride, bool homeSpecified,
    const QString &startupPv, QWidget *parent = nullptr)
    : QMainWindow(parent)
  {
    auto logo = new LogoWidget(this);
    setCentralWidget(logo);
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [this]() { pollPvUpdate(); });

    QMenu *fileMenu = menuBar()->addMenu("File");
    QString adtHome = homeOverride.isEmpty() ?
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
      if (QFileInfo(file).isRelative())
        file = QDir(pvDir).filePath(file);
      loadPvFile(file);
    }
    fileMenu->addAction("Read Reference");
    QMenu *readSnapMenu = fileMenu->addMenu("Read Snapshot");
    for (int i = 1; i <= NSAVE; ++i)
      readSnapMenu->addAction(QString::number(i));
    QMenu *writeMenu = fileMenu->addMenu("Write");
    QAction *writeCurAct = writeMenu->addAction("Current");
    connect(writeCurAct, &QAction::triggered, this, [this]() { writeCurrent(); });
    for (int i = 1; i <= NSAVE; ++i)
      writeMenu->addAction(QString::number(i));
    QMenu *plotMenu = fileMenu->addMenu("Plot");
    QAction *plotCurAct = plotMenu->addAction("Current");
    connect(plotCurAct, &QAction::triggered, this, [this]() { plotCurrent(); });
    for (int i = 1; i <= NSAVE; ++i)
      plotMenu->addAction(QString::number(i));
    QAction *statusAct = fileMenu->addAction("Status...");
    connect(statusAct, &QAction::triggered, this, [this]() { showStatus(); });
    fileMenu->addSeparator();
    QAction *quitAct = fileMenu->addAction("Quit");
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

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
    optionsMenu->addAction("Reset Max/Min");

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
    if (pollTimer)
      pollTimer->stop();
  }

private:
  QString customDirectory;
  QString pvFilename;
  QString latFilename;
  QString refFilename;
  QString saveTime[NSAVE];
  QString saveFilename[NSAVE];
  QVector<ArrayData> arrays;
  QVector<AreaData> areas;
  QVector<AreaWidget *> areaWidgets;
  AreaWidget *zoomWidget = nullptr;
  QAction *zoomAct = nullptr;
  AreaData zoomArea;
  bool zoomOn = true;
  QVector<chid> channels;
  QTimer *pollTimer = nullptr;
  int timeInterval = 2000;
  bool caStarted = false;
  double nstat = 0.0;
  double nstatTime = 0.0;
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
      for (int i = 0; i < arr.nvals; ++i)
        ca_array_get(DBR_DOUBLE, 1, arr.chids[i], &arr.vals[i]);
    }
    ca_pend_io(1.0);
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
        if (v < arr.minVals[i])
          arr.minVals[i] = v;
        if (v > arr.maxVals[i])
          arr.maxVals[i] = v;
      }
      if (arr.nvals > 0) {
        arr.avg = sum / arr.nvals;
        arr.sdev = std::sqrt(sumsq / arr.nvals - arr.avg * arr.avg);
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

  bool writePlotFile(const QString &filename)
  {
    FILE *file = fopen(filename.toUtf8().constData(), "w");
    if (!file)
      return false;

    time_t now = std::time(nullptr);
    char tbuf[26];
    std::strncpy(tbuf, std::ctime(&now), 24);
    tbuf[24] = '\0';

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
      fprintf(file, "\n");
      fprintf(file, "%s\n", arr.heading.toUtf8().constData());
      fprintf(file, "%s\n", arr.units.toUtf8().constData());
      fprintf(file, "%s (%s)\n", arr.heading.toUtf8().constData(),
        arr.units.toUtf8().constData());
      fprintf(file, "%d     !ADTDisplayArea\n", arr.area->index + 1);
      for (int i = 0; i < arr.nvals; ++i) {
        fprintf(file, "%d %s % f\n", i + 1,
          arr.names[i].toUtf8().constData(), arr.vals[i]);
      }
    }
    fclose(file);
    return true;
  }

  bool writeSnapFile(const QString &filename)
  {
    FILE *file = fopen(filename.toUtf8().constData(), "w");
    if (!file) {
      QMessageBox::warning(this, "ADT",
        QString("Unable to open %1").arg(filename));
      return false;
    }

    time_t clock = std::time(nullptr);
    char tbuf[26];
    std::strncpy(tbuf, std::ctime(&clock), 24);
    tbuf[24] = '\0';

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
      fprintf(file, "\n");
      fprintf(file, "%s (%s)\n", arr.heading.toUtf8().constData(),
        arr.units.toUtf8().constData());
      for (int i = 0; i < arr.nvals; ++i) {
        fprintf(file, "%s pv - 1 %f\n",
          arr.names[i].toUtf8().constData(), arr.vals[i]);
      }
    }
    fclose(file);
    return true;
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
    nstat = 0.0;
    nstatTime = 0.0;
    for (int i = 0; i < NSAVE; ++i) {
      saveTime[i].clear();
      saveFilename[i].clear();
    }

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
        if (SDDS_GetParameterAsLong(&table,
            const_cast<char *>("ADTTimeInterval"), &templong))
          timeInterval = static_cast<int>(templong);
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

    if (ca_pend_io(1.0) == ECA_TIMEOUT)
      QMessageBox::warning(this, "ADT", "Timeout connecting to PVs. ADT may be sluggish.");

    for (ArrayData &arr : arrays) {
      for (int i = 0; i < arr.nvals; ++i)
        ca_array_get(DBR_DOUBLE, 1, arr.chids[i], &arr.vals[i]);
    }
    ca_pend_io(1.0);

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
        arr.minVals[i] = v;
        arr.maxVals[i] = v;
      }
      if (arr.nvals > 0) {
        arr.avg = sum / arr.nvals;
        arr.sdev = std::sqrt(sumsq / arr.nvals - arr.avg * arr.avg);
        arr.maxVal = maxv;
      } else {
        arr.avg = arr.sdev = arr.maxVal = 0.0;
      }
      arr.runSdev = 0.0;
      arr.runAvg = 0.0;
      arr.runMax = 0.0;
    }

    nsymbols = channels.size();

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
      zoomArea.xmax = zoomArea.centerVal +
        scale[zoomArea.currScale] * GRIDDIVISIONS;
      zoomArea.xmin = zoomArea.centerVal -
        scale[zoomArea.currScale] * GRIDDIVISIONS;
      zoomArea.initialized = true;
      if (zarrs[0]->nvals > 0) {
        int span = std::max(1, zarrs[0]->nvals / 10);
        zoomArea.xStart = 0;
        zoomArea.xEnd = span - 1;
      }
      zoomWidget = new AreaWidget(&zoomArea, zarrs, central);
      layout->addWidget(zoomWidget);
      areaWidgets.append(zoomWidget);
      zoomWidget->setVisible(zoomOn);
      zoomPlot = zoomWidget->plotWidget();
      zoomAreaPtr = &zoomArea;
    }
    setCentralWidget(central);

    setWindowTitle("ADT - " + QFileInfo(file).fileName());
    pollTimer->start(timeInterval);
  }
};

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  QString startPv;
  QString homeOverride;
  bool homeSpecified = false;
  QStringList args = app.arguments();
  for (int i = 1; i < args.size(); ++i) {
    const QString &arg = args.at(i);
    if ((arg == "-a" || arg == "/a") && i + 1 < args.size()) {
      homeOverride = args.at(++i);
      homeSpecified = true;
    } else if ((arg == "-f" || arg == "/f") && i + 1 < args.size()) {
      startPv = args.at(++i);
    }
  }
  MainWindow win(homeOverride, homeSpecified, startPv);
  win.show();
  return app.exec();
}
