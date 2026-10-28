/**
 * @file qt_ui_regressions.cc
 * @brief Regression checks for ADT statistic presentation and display modes.
 * @author ADT contributors
 * Distributed subject to the Software License Agreement in LICENSE.
 */
#define main adtApplicationMain
#include "../src/adt_qt.cc"
#undef main

#include <QTemporaryDir>
#include <QThread>
#include <mdb.h>
#include <memory>
#include <stdexcept>

/** @brief Fail a regression check with a diagnostic. */
static void require(bool condition, const char *message)
{
  if (!condition)
    throw std::runtime_error(message);
}

/** @brief Find a submenu by its displayed name. */
static QMenu *findMenu(QMenu *parent, const QString &text)
{
  for (QAction *action : parent->actions()) {
    if (action->text() == text)
      return action->menu();
  }
  throw std::runtime_error("Menu not found");
}

/** @brief Read the persistent display mode description. */
static QString modeText(MainWindow &window)
{
  for (QLabel *label : window.findChildren<QLabel *>()) {
    if (label->text().startsWith("Plot:"))
      return label->text();
  }
  throw std::runtime_error("Display mode label not found");
}

/** @brief Write a test script or synchronization marker. */
static void writeTestFile(const QString &path, const QByteArray &contents)
{
  QFile file(path);
  require(file.open(QIODevice::WriteOnly), "Cannot create test file");
  require(file.write(contents) == contents.size(), "Cannot write test file");
}

/** @brief Process events until a detached plot tool finishes a test phase. */
static bool waitFor(const std::function<bool()> &condition)
{
  QElapsedTimer timer;
  timer.start();
  while (!condition() && timer.elapsed() < 10000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QThread::msleep(10);
  }
  return condition();
}

/** @brief Exercise statistics, label geometry, palettes and stored-set menus. */
int main(int argc, char **argv)
{
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv);
  try {
    require(argc == 2, "Supply the regression PV fixture");
    Statistics empty = computeStatistics({});
    require(!empty.sdevValid && !empty.avgValid && !empty.maxValid,
      "An empty sample must have no valid statistics");
    Statistics zero = computeStatistics({0.0, 0.0});
    require(zero.sdevValid && zero.avgValid && zero.maxValid && zero.max == 0.0,
      "Valid zero readings must remain valid");
    Statistics sample = computeStatistics({1.0, 2.0, 3.0, 4.0});
    require(sample.avg == 2.5 && sample.max == 4.0 &&
      std::fabs(sample.sdev - std::sqrt(1.25)) < 1e-12,
      "Ordinary statistics changed");

    // Reproduce the dark-system palette on the fixed light plot background.
    QPalette dark = app.palette();
    dark.setColor(QPalette::Window, QColor("#222222"));
    dark.setColor(QPalette::WindowText, Qt::white);
    app.setPalette(dark);
    LogoWidget logo;
    logo.setPalette(dark);
    logo.show();
    app.processEvents();
    QImage splash = logo.grab().toImage();
    int darkTextPixels = 0;
    for (int y = 0; y < splash.height(); ++y)
      for (int x = 200; x < splash.width() - 200; ++x)
        darkTextPixels += splash.pixelColor(x, y).lightness() < 100;
    require(darkTextPixels > 20, "Splash text must contrast with its light background");
    logo.hide();
    AreaData area;
    ArrayData array;
    array.heading = "Regression values";
    array.area = &area;
    AreaWidget widget(&area, {&array});
    widget.resize(800, 260);
    widget.show();
    for (double value : {123456789.0, -123456789.0, 1e-12, -1e-12,
        std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()}) {
      array.sdev = array.avg = array.maxVal = value;
      widget.refresh();
      app.processEvents();
      QString expected = formatStatistic(value, true, 1.0);
      require(expected.contains('e') && expected.toDouble() != 0.0,
        "Extreme statistics must use meaningful scientific notation");
      int matches = 0;
      for (QLabel *label : widget.findChildren<QLabel *>()) {
        if (!label->text().isEmpty())
          require(label->palette().color(QPalette::WindowText) == Qt::black,
            "Plot labels must contrast with their light background");
        if (label->text() == expected) {
          ++matches;
          require(label->contentsRect().width() >=
            label->fontMetrics().horizontalAdvance(expected),
            "Statistic digits are clipped");
        }
      }
      require(matches == 3, "All three statistics must be visible");
    }
    widget.hide();

    // The fixture deliberately has no server. Dismiss the expected connection
    // warning and the undefined-set warning so both states can be checked.
    qputenv("EPICS_CA_AUTO_ADDR_LIST", "NO");
    qputenv("EPICS_CA_ADDR_LIST", "127.0.0.1");
    qputenv("EPICS_CA_SERVER_PORT", "17664");
    qputenv("EPICS_CA_REPEATER_PORT", "17665");
    QTimer dismissWarnings;
    QObject::connect(&dismissWarnings, &QTimer::timeout, []() {
      if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        box->accept();
    });
    dismissWarnings.start(25);
    auto windowOwner = std::make_unique<MainWindow>(".", true,
      QString::fromLocal8Bit(argv[1]), 0, 0, false);
    MainWindow &window = *windowOwner;
    QMenu *options = nullptr;
    for (QAction *action : window.menuBar()->actions()) {
      if (action->text() == "Options")
        options = action->menu();
    }
    require(options, "Options menu not found");
    QMenu *display = findMenu(options, "Display");
    QMenu *difference = findMenu(options, "Difference");
    require(display->actions()[0]->isChecked() && difference->actions()[0]->isChecked(),
      "Stored-set menus must start at Off");
    require(modeText(window) == "Plot: Current values", "Initial mode is ambiguous");
    int unavailableStats = 0;
    for (QLabel *label : window.findChildren<QLabel *>())
      unavailableStats += label->text() == "n/a";
    require(unavailableStats == 3, "Unavailable readings must show n/a");
    const auto centers = window.findChildren<QDoubleSpinBox *>("scaleCenter");
    require(centers.size() == 2, "Check the display and zoom center controls");
    centers[0]->setValue(2.0);
    centers[1]->setValue(3.0);
    difference->actions()[0]->trigger();
    require(centers[0]->value() == 2.0 && centers[1]->value() == 3.0,
      "Selecting Difference Off while already off must retain both centers");
    window.storeSet(1);
    difference->actions()[1]->trigger();
    for (QDoubleSpinBox *center : centers)
      require(center->value() == 0.0, "Difference mode must display center zero");
    display->actions()[1]->trigger();
    require(difference->actions()[1]->isChecked() && display->actions()[1]->isChecked(),
      "Active stored sets must be checked");
    require(modeText(window) == "Plot: Current minus set 1 | Overlay: Set 1",
      "Difference and overlay must be identified");
    difference->actions()[2]->trigger();
    require(difference->actions()[1]->isChecked() && !difference->actions()[2]->isChecked(),
      "A failed selection must retain the actual checked set");
    require(modeText(window).contains("minus set 1"),
      "A failed selection must retain the actual displayed mode");
    difference->actions()[0]->trigger();
    display->actions()[0]->trigger();
    require(modeText(window) == "Plot: Current values", "Off must restore the live mode label");
    require(centers[0]->value() == 2.0 && centers[1]->value() == 3.0,
      "Off must display the restored centers");

    QMenu *fileMenu = nullptr;
    for (QAction *action : window.menuBar()->actions()) {
      if (action->text() == "File")
        fileMenu = action->menu();
    }
    require(fileMenu, "File menu not found");
    QAction *readReference = nullptr;
    for (QAction *action : fileMenu->actions()) {
      if (action->text() == "Read Reference")
        readReference = action;
    }
    require(readReference, "Read Reference action not found");
    QString referencePath;
    QTimer selectReference;
    QObject::connect(&selectReference, &QTimer::timeout, [&]() {
      if (auto picker = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
        picker->setDirectory(QFileInfo(referencePath).absolutePath());
        auto filename = picker->findChild<QLineEdit *>("fileNameEdit");
        require(filename, "Non-native file chooser filename field not found");
        filename->setText(QFileInfo(referencePath).fileName());
        static_cast<QDialog *>(picker)->accept();
      }
    });
    selectReference.start(25);
    QDir fixtures(QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath());
    referencePath = fixtures.filePath("reference-valid.snap");
    readReference->trigger();
    require(modeText(window) == "Plot: Current minus reference", "Valid reference must activate");
    referencePath = fixtures.filePath("reference-invalid.snap");
    readReference->trigger();
    require(modeText(window) == "Plot: Current minus reference",
      "A failed reference read must preserve the active reference and mode");
    QAction *customLoad = findMenu(fileMenu, "Load")->actions().last();
    const QString oldTitle = window.windowTitle();
    for (const QString &fixture : {QString("pv-invalid-header.pv"), QString("pv-invalid-late.pv")}) {
      referencePath = fixtures.filePath(fixture);
      customLoad->trigger();
      require(window.windowTitle() == oldTitle && centers[0]->value() == 2.0 &&
        centers[1]->value() == 3.0 && modeText(window) == "Plot: Current minus reference",
        "A rejected PV file must preserve the active display, centers and reference");
      difference->actions()[1]->trigger();
      require(modeText(window).contains("minus set 1"),
        "A rejected PV file must preserve stored sets");
      difference->actions()[0]->trigger();
      require(modeText(window) == "Plot: Current minus reference",
        "A rejected PV file must preserve the active reference");
    }
    selectReference.stop();
    for (QAction *action : options->actions()) {
      if (action->text() == "Reference Enabled") {
        action->trigger();
        require(modeText(window) == "Plot: Current values", "Reference Off must show current values");
        action->trigger();
        require(modeText(window) == "Plot: Current minus reference",
          "The reference must still be usable after a failed replacement");
      }
    }

    selectReference.start(25);
    referencePath = fixtures.filePath("ui-regression.pv");
    customLoad->trigger();
    selectReference.stop();
    require(window.windowTitle() == "ADT - ui-regression.pv" &&
      modeText(window) == "Plot: Current values",
      "A valid PV replacement must install the new layout");
    const auto reloadedCenters = window.findChildren<QDoubleSpinBox *>("scaleCenter");
    require(reloadedCenters.size() == 2 && reloadedCenters[0]->value() == -8.0 &&
      reloadedCenters[1]->value() == -8.0,
      "A valid PV replacement must install the new display and zoom centers");
    window.storeSet(1);
    difference->actions()[1]->trigger();
    require(modeText(window) == "Plot: Current minus set 1",
      "A successfully replaced layout must support storing and differencing");
    difference->actions()[0]->trigger();

    // Stub only the external tools. Exercise ADT's real detached launch and
    // cleanup with shell metacharacters in executable and input file paths.
    QTemporaryDir runtime(QDir::tempPath() + "/ADT ' $() `test` XXXXXX");
    require(runtime.isValid(), "Cannot create plot lifecycle test directory");
    QString state = runtime.path();
    QFile pageFixture(fixtures.filePath("ui-regression.pv"));
    require(pageFixture.open(QIODevice::ReadOnly), "Cannot read plot page fixture");
    QByteArray pageContents = pageFixture.readAll();
    pageContents.replace("name=ADTNArrays type=short fixed_value=1", "name=ADTNArrays type=short fixed_value=2");
    pageContents.replace("name=ADTNAreas type=short fixed_value=1", "name=ADTNAreas type=short fixed_value=2");
    pageContents += "\nADT:UI:REGRESSION:UNAVAILABLE pv RO\n";
    referencePath = state + "/pages.pv";
    writeTestFile(referencePath, pageContents);
    selectReference.start(25);
    customLoad->trigger();
    selectReference.stop();
    writeTestFile(state + "/sddsplot", R"sh(#!/bin/sh
data=
output=
for arg do
  case "$arg" in
    ADTPlot*.sdds|*/ADTPlot*.sdds) data=$arg ;;
    -output=*) output=${arg#-output=} ;;
  esac
done
[ -f "$data" ] || exit 21
if [ -n "$output" ]; then
  printf '%s' "$data" > "$ADT_UI_TEST_STATE/input"
  printf 'graphics\n' > "$output"
  printf '%s' "$output" > "$ADT_UI_TEST_STATE/graphics"
fi
)sh");
    writeTestFile(state + "/mpl_qt", R"sh(#!/bin/sh
[ "$1" = -command ] || exit 22
printf '%s' "$2" > "$ADT_UI_TEST_STATE/command"
/bin/sh -c "$2" || exit 23
: > "$ADT_UI_TEST_STATE/ready"
i=0
while [ ! -f "$ADT_UI_TEST_STATE/release" ]; do
  i=$((i+1))
  [ "$i" -lt 100 ] || exit 24
  sleep 0.05
done
/bin/sh -c "$2" || exit 25
: > "$ADT_UI_TEST_STATE/after-close"
)sh");
    for (const QString &name : {QString("sddsplot"), QString("mpl_qt")})
      require(QFile::setPermissions(state + "/" + name,
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner), "Cannot make plot test tool executable");
    QByteArray oldPath = qgetenv("PATH");
    QByteArray oldTmp = qgetenv("TMPDIR");
    qputenv("PATH", state.toLocal8Bit() + ":" + oldPath);
    qputenv("TMPDIR", state.toLocal8Bit());
    qputenv("ADT_UI_TEST_STATE", state.toLocal8Bit());
    findMenu(fileMenu, "Plot")->actions()[0]->trigger();
    require(waitFor([&]() { return QFile::exists(state + "/ready"); }),
      "Plot viewer must be able to replot its input");
    auto readPath = [&](const QString &name) {
      QFile file(state + "/" + name);
      require(file.open(QIODevice::ReadOnly), "Plot tool did not record its file path");
      return QString::fromLocal8Bit(file.readAll());
    };
    QString input = readPath("input");
    QString graphics = readPath("graphics");
    QByteArray command = readPath("command").toLocal8Bit();
    bool columnArgument = false, deviceArgument = false;
    while (char *token = get_token_t(command.data(), const_cast<char *>("'"))) {
      QString argument = QString::fromLocal8Bit(token).trimmed();
      free(token);
      columnArgument |= argument == "-column=Index,Value";
      deviceArgument |= argument == "-device=qt";
    }
    require(columnArgument && deviceArgument,
      "The mpl_qt tokenizer must recognize individual replot options");
    require(QFile::exists(input) && QFile::exists(graphics), "Viewer files were removed too soon");
    SDDS_DATASET plotData = {};
    QByteArray plotPath = QFile::encodeName(input);
    require(SDDS_InitializeInput(&plotData, plotPath.data()), "Cannot read plot export");
    require(SDDS_ReadPage(&plotData) > 0 && SDDS_RowCount(&plotData) == 0,
      "The first empty array must survive plot export");
    require(SDDS_ReadPage(&plotData) > 0 && SDDS_RowCount(&plotData) == 0,
      "A final empty array must survive plot export as an empty page");
    require(SDDS_ReadPage(&plotData) == -1, "Unexpected extra plot page");
    require(SDDS_Terminate(&plotData), "Cannot close plot export");
    windowOwner.reset();
    require(QFile::exists(input), "Closing ADT must preserve the viewer input");
    writeTestFile(state + "/release", "");
    require(waitFor([&]() { return QFile::exists(state + "/after-close") &&
      !QFile::exists(input) && !QFile::exists(graphics); }),
      "Plot files must survive ADT and be cleaned up after the viewer exits");
    qputenv("PATH", oldPath);
    qputenv("TMPDIR", oldTmp);
    qunsetenv("ADT_UI_TEST_STATE");
    std::puts("ADT UI regression checks passed");
  } catch (const std::exception &error) {
    std::fprintf(stderr, "ADT UI regression failure: %s\n", error.what());
    return 1;
  }
  return 0;
}
