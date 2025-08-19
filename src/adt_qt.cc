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
#include <QMessageBox>
#include <QString>
#include <QWidget>
#include <QList>
#include <QVector>

#include <SDDS.h>
#include <QDir>
#include <cadef.h>

#define INITFILENAME "adtrc"

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

    if (ca_context_create(ca_disable_preemptive_callback) != ECA_NORMAL) {
      QMessageBox::warning(this, "ADT", "Unable to start Channel Access");
      return;
    }
    caStarted = true;

    SDDS_TABLE table;
    QByteArray fname = file.toUtf8();
    if (!SDDS_InitializeInput(&table, fname.data()) ||
        SDDS_ReadTable(&table) <= 0) {
      QMessageBox::warning(this, "ADT", "Unable to read PV file:\n" + file);
      SDDS_Terminate(&table);
      ca_context_destroy();
      caStarted = false;
      return;
    }

    int rows = SDDS_CountRowsOfInterest(&table);
    char **names = (char **)SDDS_GetColumn(&table,
      const_cast<char *>("ControlName"));
    if (!names) {
      QMessageBox::warning(this, "ADT",
        "PV file missing ControlName column");
      SDDS_Terminate(&table);
      ca_context_destroy();
      caStarted = false;
      return;
    }

    for (int i = 0; i < rows; ++i) {
      chid ch;
      int status = ca_create_channel(names[i], NULL, NULL, 0, &ch);
      if (status == ECA_NORMAL)
        channels.append(ch);
    }

    if (ca_pend_io(5.0) == ECA_TIMEOUT)
      QMessageBox::warning(this, "ADT", "Timeout connecting to PVs");

    for (int i = 0; i < rows; ++i)
      SDDS_Free(names[i]);
    SDDS_Free(names);
    SDDS_Terminate(&table);

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
