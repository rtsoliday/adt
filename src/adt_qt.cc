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
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QString>
#include <QWidget>

class LogoWidget : public QWidget
{
public:
  explicit LogoWidget(QWidget *parent = nullptr) : QWidget(parent)
  {
    setMinimumSize(400, 200);
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
    QMenu *loadMenu = fileMenu->addMenu("Load");
    for (int i = 1; i <= NSAVE; ++i)
      loadMenu->addAction(QString::number(i));
    loadMenu->addSeparator();
    loadMenu->addAction("Custom...");
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
    helpMenu->addAction("Version");
  }
};

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  MainWindow win;
  win.show();
  return app.exec();
}
