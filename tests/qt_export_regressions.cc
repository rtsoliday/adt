/**
 * @file qt_export_regressions.cc
 * @brief Snapshot round trips and rendering checks for undefined extrema.
 * @author ADT contributors
 * Distributed subject to the Software License Agreement in LICENSE.
 */
#define main adtApplicationMain
#include "../src/adt_qt.cc"
#undef main

#include <QTemporaryDir>
#include <stdexcept>

/** @brief Fail a check with a readable diagnostic. */
static void require(bool ok, const char *message)
{
  if (!ok)
    throw std::runtime_error(message);
}

/** @brief Find a menu action by its label. */
static QAction *actionNamed(QMenu *menu, const QString &name)
{
  for (QAction *action : menu->actions())
    if (action->text() == name)
      return action;
  throw std::runtime_error("Menu action not found");
}

/** @brief Exercise the real snapshot writer and reader through their menus. */
int main(int argc, char **argv)
{
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv);
  try {
    require(argc == 2, "Supply the regression PV fixture");
    QTemporaryDir runtime(QDir::tempPath() + "/ADT-export-XXXXXX");
    require(runtime.isValid(), "Cannot create export test directory");
    QFile fixture(QString::fromLocal8Bit(argv[1]));
    require(fixture.open(QIODevice::ReadOnly), "Cannot read fixture");
    QByteArray contents = fixture.readAll();
    contents.replace("Regression values", "! Heading with \\\"quotes\\\" and punctuation");
    QString pvPath = runtime.filePath("quoted\"name.pv");
    QFile pv(pvPath);
    require(pv.open(QIODevice::WriteOnly) && pv.write(contents) == contents.size(),
      "Cannot write quoted-path fixture");
    pv.close();
    qputenv("EPICS_CA_AUTO_ADDR_LIST", "NO");
    qputenv("EPICS_CA_ADDR_LIST", "127.0.0.1");
    qputenv("EPICS_CA_SERVER_PORT", "17674");
    qputenv("EPICS_CA_REPEATER_PORT", "17675");
    int warnings = 0;
    QString selectedFile;
    QTimer dialogs;
    QObject::connect(&dialogs, &QTimer::timeout, [&]() {
      if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
        ++warnings;
        box->accept();
      }
      if (auto picker = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
        picker->setDirectory(QFileInfo(selectedFile).absolutePath());
        picker->findChild<QLineEdit *>("fileNameEdit")->setText(QFileInfo(selectedFile).fileName());
        static_cast<QDialog *>(picker)->accept();
      }
    });
    dialogs.start(25);
    MainWindow window(runtime.path(), true, pvPath, 0, 0, false);
    // Top-level menus belong to the menu bar rather than another QMenu.
    QMenu *file = nullptr;
    QMenu *options = nullptr;
    for (QAction *action : window.menuBar()->actions()) {
      if (action->text() == "File") file = action->menu();
      if (action->text() == "Options") options = action->menu();
    }
    require(file && options, "Missing top-level menu");
    selectedFile = runtime.filePath("roundtrip.snap");
    int before = warnings;
    actionNamed(actionNamed(file, "Write")->menu(), "Current")->trigger();
    require(warnings == before, "Snapshot write failed");
    SDDS_DATASET data = {};
    QByteArray path = QFile::encodeName(selectedFile);
    require(SDDS_InitializeInput(&data, path.data()), "Snapshot header is invalid");
    // Legacy SDDS metadata retains the escape before an embedded quote.
    require(QString::fromUtf8(data.layout.description).endsWith("quoted\\\"name.pv"),
      "Snapshot description is truncated at the embedded quote");
    require(SDDS_ReadPage(&data) > 0 && SDDS_RowCount(&data) == 1,
      "Snapshot heading consumed a data row");
    char *heading = nullptr;
    require(SDDS_GetParameter(&data, const_cast<char *>("Heading"), &heading), "Missing heading");
    require(QString::fromUtf8(heading).startsWith("! Heading with \"quotes\""),
      "Snapshot heading did not round trip");
    free(heading);
    require(SDDS_Terminate(&data), "Cannot close snapshot");
    actionNamed(actionNamed(file, "Read Snapshot")->menu(), "1")->trigger();
    require(warnings == before, "ADT rejected its own snapshot");
    actionNamed(actionNamed(options, "Display")->menu(), "1")->trigger();
    require(warnings == before, "Reloaded snapshot is unavailable");

    // Exercise the real legacy header writer with a trailing backslash, too.
    QString trailing = runtime.filePath("trailing\\");
    require(QFile::copy(pvPath, trailing), "Cannot create trailing-backslash fixture");
    selectedFile = trailing;
    actionNamed(actionNamed(file, "Load")->menu(), "Custom...")->trigger();
    before = warnings; // A disconnected PV warning is expected during loading.
    selectedFile = runtime.filePath("trailing.snap");
    actionNamed(actionNamed(file, "Write")->menu(), "Current")->trigger();
    actionNamed(actionNamed(file, "Read Snapshot")->menu(), "1")->trigger();
    require(warnings == before, "Trailing-backslash path broke the snapshot round trip");

    AreaData area;
    area.currScale = 9;
    ArrayData array;
    array.area = &area;
    array.color = Qt::red;
    array.nvals = 4;
    array.names = {"a", "b", "c", "d"};
    array.vals.fill(0, 4);
    array.conn.fill(false, 4);
    array.minVals.fill(std::numeric_limits<double>::infinity(), 4);
    array.maxVals.fill(-std::numeric_limits<double>::infinity(), 4);
    diffSet = displaySet = -1;
    referenceLoaded = false;
    array.statusCheck = true;
    array.conn.fill(true);
    array.statusVals = {100, 200, 65535, 1};
    for (int i = 0; i < 4; ++i)
      require(array.available(i) && !array.excluded(i),
        "Valid enum status values collide with internal states");
    array.statusCheck = false;
    array.conn.fill(false);

    require(pointIndex(array, 0.375, false, 0, 0) == 1,
      "Hit testing must use each array's own point count");
    array.s = {10, 35, 60, 85};
    area.xStart = area.xEnd = 1;
    nsect = 4;
    stotal = 100;
    latRing = true;
    double smin, smax;
    plotZoomBounds(area, array, smin, smax);
    require(smax > smin && smin < 35 && smax > 35 &&
      pointIndex(array, 0.5, true, smin, smax) == 1,
      "A one-point zoom must have a finite centered range");
    require(pointIndex(array, 1, true, 50, 75) == 2,
      "Zoom selection must prefer an on-screen PV over a nearer off-screen PV");
    ArrayData endpoints = array;
    endpoints.s[0] = 12.5;
    require(pointIndex(endpoints, 1, true, 12.5, 112.5) == 0,
      "A PV on the ring seam must be selectable at the right boundary");
    area.xStart = 0;
    area.xEnd = -1;
    latRing = false;
    nsect = 0;
    stotal = 0;

    array.conn.fill(true);
    array.vals = {1, 2, 3, 4};
    array.refVals = {1, 1, 1, 1};
    array.refAvail = {true, true, true, false};
    referenceLoaded = refOn = true;
    double plotted;
    require(plottedValue(array, array.vals, -1, 1, plotted) && plotted == 1 &&
      !plottedValue(array, array.vals, -1, 3, plotted),
      "Inspection and availability must honor reference subtraction");
    referenceLoaded = false;
    array.conn.fill(false);
    PlotWidget plot(&area, {&array});
    plot.resize(800, 250);
    plot.show();
    for (bool filled : {false, true}) {
      fillmaxmin = filled;
      showmaxmin = false;
      QImage baseline = plot.grab().toImage();
      showmaxmin = true;
      require(plot.grab().toImage() == baseline,
        "Undefined extrema changed the plot");
      array.minVals = {1, 2, 3, 4};
      array.maxVals = {2, 3, 4, 5};
      require(plot.grab().toImage() != baseline,
        "Real extrema must remain visible after disconnection");
      array.minVals.fill(std::numeric_limits<double>::infinity());
      array.maxVals.fill(-std::numeric_limits<double>::infinity());
    }
    array.heading = "Beam position - a long heading describing diagnostics for the horizontal plane";
    array.units = "mm";
    AreaWidget labels(&area, {&array});
    labels.resize(800, 300);
    labels.show();
    app.processEvents();
    bool found = false;
    for (QLabel *label : labels.findChildren<QLabel *>()) {
      if (label->text().startsWith("Beam position")) {
        found = true;
        require(label->wordWrap() && label->toolTip().endsWith("(mm)"),
          "Full heading and units must be accessible");
        require(label->height() >= label->heightForWidth(label->width()),
          "Wrapped heading is vertically clipped");
      }
    }
    require(found, "Heading label not found");

    array.scaleFactor = -2;
    array.sdev = std::sqrt(1.25);
    labels.refresh();
    bool positiveSdev = false;
    for (QLabel *label : labels.findChildren<QLabel *>())
      positiveSdev |= label->text() == "2.236";
    require(positiveSdev, "A negative scale factor must not negate SDEV");

    // Boundary neighbors must be clipped, not wrapped back across the zoom.
    array.scaleFactor = 1;
    array.vals = {1, 6, 3, 4};
    array.conn.fill(true);
    area.currScale = 10;
    area.xStart = area.xEnd = 1;
    nsect = 4;
    stotal = 100;
    latRing = true;
    showmaxmin = false;
    PlotWidget sparse(&area, {&array});
    zoomPlot = &sparse;
    zoomAreaPtr = &area;
    sparse.resize(800, 250);
    QImage zoomImage = sparse.grab().toImage();
    QRect bounds = makePlotRect(zoomImage.width(), zoomImage.height());
    int firstRed = -1, lastRed = -1;
    int sampleX = bounds.left() + bounds.width() / 4;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
      if (zoomImage.pixelColor(sampleX, y) == QColor(Qt::red)) {
        if (firstRed < 0) firstRed = y;
        lastRed = y;
      }
    }
    require(firstRed >= 0 && lastRed - firstRed <= 2,
      "Zoom boundary neighbors draw a spurious wrapped line");

    // Sector selections must survive even when the entire ring has one PV.
    AreaData singleArea;
    singleArea.currScale = 10;
    ArrayData single;
    single.area = &singleArea;
    single.nvals = 1;
    single.names = {"single"};
    single.heading = "Single PV";
    single.color = Qt::red;
    single.s = {35};
    single.vals = {4};
    single.conn = {true};
    singleArea.xEnd = 0;
    AreaWidget singleWidget(&singleArea, {&single}, nullptr, true, true);
    zoomAreaPtr = &singleArea;
    zoomPlot = singleWidget.plotWidget();
    zoomPlot->resize(800, 250);
    singleWidget.setZoom(2, 1);
    singleWidget.refresh();
    plotZoomBounds(singleArea, single, smin, smax);
    require(smin == 25 && smax == 50,
      "One visible sector must not expand to the full single-PV ring");
    singleWidget.setZoom(3, 1);
    singleWidget.refresh();
    require(singleWidget.findChild<QSpinBox *>("centerSector")->value() == 3,
      "An empty center sector must not snap back to the nearest PV");
    plotZoomBounds(singleArea, single, smin, smax);
    require(smin == 50 && smax == 75, "Empty sector bounds are incorrect");
    require(pointIndex(single, 0.5, true, smin, smax) == -1,
      "An empty zoom sector must not select an off-screen PV");
    QMouseEvent inspectEmpty(QEvent::MouseButtonPress, QPointF(100, 100),
      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(zoomPlot, &inspectEmpty);
    auto emptyInfo = zoomPlot->findChild<QMessageBox *>();
    require(emptyInfo && emptyInfo->text().contains("No PVs in the visible range") &&
      !emptyInfo->text().contains("->1 single"),
      "The empty zoom popup must explain why there is no selected PV");
    emptyInfo->close();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    singleWidget.setZoom(3, 4);
    zoomPlot->resize(800, 250);
    QImage fullRing = zoomPlot->grab().toImage();
    bool edgeLabel = false;
    for (int y = fullRing.height() - 14; y < fullRing.height() - 3; ++y)
      for (int x = 2; x < 20; ++x)
        edgeLabel |= fullRing.pixelColor(x, y) == QColor(Qt::black);
    require(edgeLabel, "The full-ring sector label at the left boundary is missing");
    plotZoomBounds(singleArea, single, smin, smax);
    require(pointIndex(single, 0, true, smin, smax) == 0,
      "Full-ring inspection must consider the visible copy of a wrapped PV");

    singleWidget.setZoom(1, 3);
    singleWidget.refresh();
    plotZoomBounds(singleArea, single, smin, smax);
    require(smin == 75 && smax == 150 &&
      singleWidget.findChild<QSpinBox *>("visibleSectors")->value() == 3,
      "A wrapped three-sector selection must retain its physical range");
    zoomPlot->resize(800, 250);
    QImage wrapped = zoomPlot->grab().toImage();
    bool redPoint = false;
    for (int y = 0; y < wrapped.height(); ++y)
      for (int x = 630; x < 650; ++x)
        redPoint |= wrapped.pixelColor(x, y) == QColor(Qt::red);
    require(redPoint, "The lone PV disappeared from a wrapped sector window");
    require(pointIndex(single, 0.8, true, smin, smax) == 0,
      "A visible PV across the ring boundary must remain selectable");
    QPointF pivot(400, 125);
    QWheelEvent wheel(pivot, pivot, QPoint(), QPoint(0, 120), Qt::NoButton,
      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(zoomPlot, &wheel);
    plotZoomBounds(singleArea, single, smin, smax);
    require(std::fabs(smax - smin - 62.5) < 1e-9,
      "Wheel zoom must change physical width in a single-PV ring");
    double beforePan = smin;
    QMouseEvent press(QEvent::MouseButtonPress, pivot, Qt::RightButton,
      Qt::RightButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, pivot + QPointF(80, 0), Qt::NoButton,
      Qt::RightButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, pivot + QPointF(80, 0),
      Qt::RightButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(zoomPlot, &press);
    QApplication::sendEvent(zoomPlot, &move);
    QApplication::sendEvent(zoomPlot, &release);
    plotZoomBounds(singleArea, single, smin, smax);
    require(std::fabs(smax - smin - 62.5) < 1e-9 && smin < beforePan,
      "Right-button panning must move a sparse ring window without resizing it");
    QMouseEvent recenter(QEvent::MouseButtonPress, QPointF(300, 125),
      Qt::MiddleButton, Qt::MiddleButton, Qt::NoModifier);
    QApplication::sendEvent(&plot, &recenter);
    plotZoomBounds(singleArea, single, smin, smax);
    double center = std::fmod((smin + smax) / 2, stotal);
    require(std::fabs(center - 35) < 1e-9 && std::fabs(smax - smin - 62.5) < 1e-9,
      "Middle-button recentering must retain the physical zoom width");
    zoomPlot = nullptr;
    zoomAreaPtr = nullptr;
    latRing = false;
    nsect = 0;
    stotal = 0;
    std::puts("ADT export and rendering regression checks passed");
  } catch (const std::exception &error) {
    std::fprintf(stderr, "ADT export regression failure: %s\n", error.what());
    return 1;
  }
  return 0;
}
