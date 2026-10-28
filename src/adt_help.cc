/**
 * @file adt_help.cc
 * @brief Help menu, guide window, and About dialog for the Qt ADT.
 *
 * The guide window, search, and About layout are adapted from the companion
 * Qt StripTool and QtProbe ports so that the Qt applications share one Help
 * menu design. The guide is compiled into the program, so it is available
 * without a network connection or an installed manual.
 *
 * @author Robert Soliday
 * @copyright
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 * Distributed subject to a Software License Agreement found in the file LICENSE that is included with this distribution.
 */

#include "adt_help.h"
#include "adtVersion.h"

#include <cmath>

#include <epicsVersion.h>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopedPointer>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace adt {
namespace {

struct GuideSection
{
  const char *title;
  const char *markdown;
};

/* Each section is a separate literal to stay within MSVC's string limits. */
const GuideSection guideSections[] = {
  {"Getting Started", R"md(
ADT, the Array Display Tool, plots arrays of EPICS process variables, such
as beam position monitor readbacks or corrector settings, in machine order.
It reads the PVs over Channel Access and redraws them at a fixed update
interval.

### Open a display

- Choose a file from **File > Load**. Its submenus come from the `adtrc`
  configuration file; see [Configuration File](#configuration-file).
- Choose **File > Load > Custom...** to open any ADT PV file (`*.pv`).
- Or name a PV file when starting ADT: `adt -f pv/sr.bpm.pv`.

Loading a PV file replaces the startup logo with one or more display areas
and, when the file asks for one, a zoom area at the bottom. The window title
shows the name of the PV file.

### Read the display

- Each array has a title row with its color, heading and units, followed by
  the population standard deviation (**SDEV**), average (**AVG**) and
  largest-magnitude value (**MAX**) of what is plotted, over the available
  points that Check Status does not exclude. A statistic that cannot be
  represented shows **n/a**.
- **Unavailable: n/N** appears when channels are disconnected or return no
  valid value, or when a server has not answered a read for more than twice
  the update interval (at least 5 s). A point without a value is drawn as a
  black square on the center line, with no line or bar to it.
- Hold the left mouse button on a plot to see the nearest element and its
  neighbors, with their PV names and values. Release the button to close the
  details, or move off the plot before releasing to keep them open.

ADT only reads PVs. The **Write** menu writes snapshot files, never PVs.
)md"},
  {"Display Areas", R"md(
A PV file divides its arrays among one or more display areas. Every array
in an area shares the area's vertical scale, and each array is drawn in its
own color.

### Scale

- **Scale Interval** is the number of units per grid division. It steps
  through a 1, 2, 5 sequence from 0.001 to 5000.
- **Scale Center** is the value at the center line. The area shows five
  divisions above and below the center.

The PV file sets the initial values with `ADTCenterVal` and
`ADTUnitsPerDiv`, taken from the first array assigned to each area.

### View menu

- **Markers**, **Lines** and **Bars** select how each array is drawn.
- **Grid** shows the horizontal division lines.
- **Max/Min** draws the lowest and highest value seen at each element since
  the last reset. **Filled Max/Min** shades the band between them instead.
- **Timing...** sets the update interval in milliseconds. The default is
  2000 ms unless the PV file sets `ADTTimeInterval`.

### Clear menu

- **Autoclear** (on by default) erases the plots before each update. Turn it
  off to let successive traces accumulate.
- **Clear** erases the plots, **Redraw** erases and draws the current values,
  and **Update** draws the current values now.

### Statistics

- **Options > Accumulated Statistics** changes the title rows to show SDEV
  and AVG averaged over the updates since it was turned on, and MAX as the
  largest-magnitude value seen. Updates whose statistic is n/a are left out
  of the averages.
- **Options > Reset Max/Min**, or a right click on a display area, restarts
  the Max/Min envelopes and the accumulated statistics.
- **File > Status...** lists the files in use, the accumulated statistics and
  history time, and the time and file of each stored set.
)md"},
  {"Log Scale", R"md(
A PV file page with a nonzero `ADTLogScale` plots its array in decades, as
log10 of the scale factor times the value. The area's grid, scale center and
scale interval are then in decades; for example `pv/sr.ionpump.pv` centers
on -8, which is 1e-8 Torr.

- A value at or below zero has no logarithm. It is drawn as unavailable, and
  point details show **&le; 0 (log undefined)**. Point details show the
  logarithm of the other values.
- In difference and reference modes the array shows the change in decades,
  log10(value) - log10(stored or reference value). A point where either value
  is not positive, or is unavailable, is drawn as unavailable.
- **SDEV**, **AVG** and **MAX** are computed over the linear values of the
  valid, positive points and shown as log10 of each, or **n/a** when a
  statistic is not positive. In difference and reference modes they describe
  the plotted changes in decades.
- Stored sets, snapshots, references and plot files keep the linear values.
)md"},
  {"Zoom Area", R"md(
Arrays marked with `ADTZoomArea` in the PV file are also drawn in a zoom
area at the bottom of the window. When the PV file names a lattice file,
the zoom area positions elements by their location along the machine and
draws the lattice beneath the plot.

- **Options > Zoom Enabled** shows or hides the zoom area.
- **Visible Sectors** sets how many sectors are shown, and **Center Sector**
  picks the sector in the middle. Sectors come from the lattice file.
- Turn the **mouse wheel** over the zoom area to zoom in or out. The element
  under the pointer stays in place.
- **Drag with the right button** in the zoom area to pan along the machine.
- **Middle click** a display area to center the zoom area on that element.

The zoom area uses the largest scale interval and the average scale center
of the areas whose arrays it shows. The `-s` and `-z` command-line options,
or `ADTZoomInterval` in the PV file, set the initial zoom.
)md"},
  {"Status Checking", R"md(
When the PV file has a `StatusName` column, ADT reads each point's status PV
in the same update as its value. Status 0 means invalid data, status 2 means
old data, and any other value is valid.

- **Options > Check Status** selects which points to exclude: **None**,
  **Invalid** (status 0) or **All** (status 0 and 2). All is the default.
  The menu is enabled only when the loaded file has status checking.
- An excluded point is still plotted at its value, with a gray dot instead
  of the normal marker: dark gray for invalid data, lighter gray for old
  data. It does not count in the title-row statistics, and the title row
  shows **Excluded: n/N**.
- A point whose status PV cannot be read is unavailable, in every mode.
- Starting ADT with `-x` turns status checking off.
)md"},
  {"Stored Sets, References and Differences", R"md(
ADT keeps five stored sets of values, numbered 1 to 5, along with an
optional reference.

- **Options > Store** copies the current values into a set and records the
  time.
- **File > Read Snapshot** reads a snapshot file into a set. The file must
  list the same PV names, in the same order, as the loaded PV file.
- **Options > Display** overlays a stored set in gray (green for an array
  that is already gray). Choose **Off** to remove it.
- **Options > Difference** plots the current values minus a stored set. The
  scale centers move to zero and return when you choose **Off**.
- **File > Read Reference** reads a snapshot file as the reference. A PV
  file can also name one with `ADTReferenceFile`. While **Options >
  Reference Enabled** is checked, the reference is subtracted from the plots.
  A Difference set takes precedence over the reference.

The title-row statistics describe what is plotted: in difference mode the
current values minus the difference set, with the reference displayed the
current values minus the reference, and otherwise the current values. Start
ADT with `-d` to store set 1 shortly after startup and display differences
from it.
)md"},
  {"Snapshots and Plots", R"md(
- **File > Write > Current** writes the current values to a snapshot file.
  **File > Write > 1** to **5** writes a stored set.
- **File > Plot > Current**, or a stored set number, plots the values with
  the external `sddsplot` program, one panel per array. Plotting needs
  `sddsplot` on your `PATH` and a Unix shell; ADT warns when either is
  missing. The plot runs on its own and stays open after ADT exits. Errors
  from a running `sddsplot` go to its standard error, not to ADT.

Snapshot files use the SDDS `ADTSNAP` format with `ControlName` and
`ValueString` columns, one page per array. References use the same format.
Values are written with full double precision.

A failed or incomplete snapshot or reference read leaves the previous set
unchanged. Reads reject files with the wrong number of values, different
PV names, or nonnumeric or nonfinite values. Pages after the last array of
the loaded PV file are not read, so a snapshot of a larger PV file that
begins with the same arrays can be used.

Stored sets, snapshots and references keep each point's availability.
Snapshot files add an `ADTAvailable` column (1 available, 0 unavailable) and
still write 0 as the value of an unavailable point, so older ADT releases can
read them; a file without the column is read as fully available. When a
stored set or the reference is displayed or subtracted, a point that is
unavailable in either one has no value and does not count in the
statistics. Plot files leave out unavailable points, so `sddsplot` shows
gaps.
)md"},
  {"PV Files", R"md(
A PV file is an SDDS file with one page per array. Each page lists the
array's PVs in a string `ControlName` column, in display order.

### File parameters

These are read from the first page.

| Parameter | Meaning |
| --- | --- |
| `ADTFileType` | Must be `ADTPV` |
| `ADTNArrays` | Number of arrays (pages) |
| `ADTNAreas` | Number of display areas; one per array when absent |
| `ADTTimeInterval` | Update interval in ms |
| `ADTZoomInterval` | Initial number of visible zoom sectors |
| `ADTMarkers`, `ADTLines`, `ADTBars`, `ADTGrid`, `ADTMaxMin`, `ADTFillMaxMin` | Initial View settings |
| `ADTLatticeFile` | Lattice file; a name without `/` is found in the PV directory |
| `ADTReferenceFile` | Reference snapshot; a relative name is found in the snapshot directory |

### Array parameters

These are read from each page.

| Parameter | Meaning |
| --- | --- |
| `ADTHeading`, `ADTUnits` | Title-row text |
| `ADTColor` | Color name, such as `Red` or `#ff0000` |
| `ADTScaleFactor` | Multiplier applied to displayed values |
| `ADTDisplayArea` | Display area number, starting at 1 |
| `ADTZoomArea` | Nonzero to show the array in the zoom area |
| `ADTLogScale` | Nonzero to plot the array in decades; see [Log Scale](#log-scale) |
| `ADTCenterVal`, `ADTUnitsPerDiv` | Initial scale center and interval for the area |

### Status column

An optional string `StatusName` column names a status PV for each point;
`-` means the point is not checked. Status checking is on whenever the
column is present, unless ADT was started with `-x`. See
[Status Checking](#status-checking).
)md"},
  {"Configuration File", R"md(
ADT reads a configuration file named `adtrc` to find the PV and snapshot
directories and to build the **File > Load** menu.

1. With `-a <directory>`, ADT uses `<directory>/adtrc` only.
2. Otherwise it uses `$HOME/.adtrc` if that exists.
3. Otherwise it uses `$ADTHOME/adtrc`, or `./adtrc` when `ADTHOME` is not
   set.

The ADT home is the `-a` directory, otherwise `ADTHOME`, otherwise the
current directory. PV files and snapshots default to its `pv` and `snap`
subdirectories, even when the configuration file is missing. In that case
the Load menu contains only **Custom...**.

- `ADTPVDirectory` and `ADTSnapDirectory` replace the default directories.
  `ADTPVSubDirectory` selects a PV subdirectory of the ADT home and takes
  precedence over `ADTPVDirectory`.
- With `-x`, `ADTXPVDirectory` and `ADTXSnapDirectory` replace their
  ordinary counterparts when they are present.
- Each page of the file defines one Load submenu, titled by `ADTMenuTitle`,
  with items from its `ADTMenuLabel` and `ADTPVFile` columns.

A file given with `-f` is opened relative to the current directory, not the
PV directory.
)md"},
  {"Command-Line Options", R"md(
```
adt [-a directory] [-f pvfile] [-s center_sector] [-z number_of_sectors]
    [-d] [-x] [-h|-?]
```

| Option | Meaning |
| --- | --- |
| `-a <directory>` | Use this ADT home directory and its `adtrc` |
| `-f <pvfile>` | Open a PV file at startup |
| `-s <center_sector>` | Center the zoom area on this sector |
| `-z <number_of_sectors>` | Show this many sectors in the zoom area |
| `-d` | Store set 1 after startup and display differences from it |
| `-x` | Use the Xorbit directories from the configuration file and turn off status checking |
| `-h`, `-?` | Print usage and exit |

Each option may also be written with `/` instead of `-`, as on Windows.
Qt's own options, such as `-display` and `-style`, are accepted.

ADT exits with status 0 after printing help, which needs no display. An
unknown option, a missing operand for `-a`, `-f`, `-s` or `-z`, a value of
`-s` or `-z` that is not an integer, or an unexpected argument prints
`Invalid option` and the usage text to standard error, and ADT exits with
status 8.

`-x` only selects directories and turns off status checking. It does not
change PV names or isolate Channel Access, so name simulation PVs explicitly
in the PV file.
)md"},
  {"Differences from Motif ADT", R"md(
The historical ADT reference manual describes the Motif application. The Qt
application differs in these ways:

- There are no EPICS start, stop or rescan controls. ADT reads connected
  channels at each update and reports unavailable channels in the title rows
  and point details.
- A nonpositive value of a log-scale array is drawn as unavailable, and
  point details show **&le; 0 (log undefined)**, where the Motif version showed
  0. In difference and reference modes a log-scale array plots the change in
  decades.
- Threshold PVs (`ThresholdName`) are not checked.
- Snapshot and plot files do not include the Motif version's `StatusName`
  and `Status` columns. Snapshots add an `ADTAvailable` column, and plot
  files leave out unavailable points.
- The Filled Max/Min setting is `ADTFillMaxMin`, rather than the manual's
  `ADTFilledMaxMin`.
- `-x` does not add or remove an `Xorbit:` prefix.
- X resources, the ControlApp application-defaults file, `ADTHelpFile` and
  the Mosaic help settings apply only to the Motif version. Help is built
  into the Qt application.
- Mouse-wheel zooming of the zoom area is new. Dragging the zoom area with
  the right button pans it, where the Motif version scrolled while the button
  was held. A middle click recenters the zoom area only from a display area,
  not within the zoom area itself.
)md"},
  {"Keyboard Shortcuts", R"md(
Use Command instead of Ctrl on macOS.

| Where | Keys | Action |
| --- | --- | --- |
| ADT window | F1 | Open Help Topics |
| Help | Ctrl+F | Search the guide |
| Help | Enter or F3 | Next match |
| Help | Shift+Enter or Shift+F3 | Previous match |
| Help | Esc | Close Help |

Help search ignores case, highlights every match, and wraps around.

### Mouse controls

- **Point details:** hold the left button on a display area or the zoom
  area; release it to close the details, or move off the plot first to keep
  them open.
- **Reset Max/Min:** right click a display area.
- **Zoom:** turn the mouse wheel over the zoom area.
- **Pan:** drag the zoom area with the right button.
- **Center the zoom area:** middle click an element in a display area.
)md"},
};

struct Topic
{
  QString anchor, title, markdown;
};

QVector<Topic> helpTopics()
{
  QVector<Topic> topics;
  for (const GuideSection &section : guideSections) {
    const QString title = QString::fromUtf8(section.title);
    QString anchor = title.toLower();
    anchor.remove(QRegularExpression(QStringLiteral("[^\\w\\s-]")));
    anchor.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("-"));
    topics.append({anchor, title, QString::fromUtf8(section.markdown).trimmed()});
  }
  return topics;
}

QString topicHtml(const Topic &topic)
{
  QTextDocument document;
  document.setMarkdown(topic.markdown);
  // Qt's Markdown import gives list items after a heading the heading's bold
  // block format, which toHtml() then applies to the whole item.
  for (auto block = document.begin(); block.isValid(); block = block.next())
    if (block.textList())
      QTextCursor(block).setBlockCharFormat(QTextCharFormat());
  QString html = document.toHtml();
  const auto body = QRegularExpression(QStringLiteral("<body[^>]*>")).match(html);
  html = html.mid(body.capturedEnd());
  html.truncate(html.lastIndexOf(QStringLiteral("</body>")));
  return QStringLiteral("<h2><a name=\"%1\">%2</a></h2>%3")
    .arg(topic.anchor.toHtmlEscaped(), topic.title.toHtmlEscaped(), html);
}

class HelpWindow final : public QDialog
{
public:
  explicit HelpWindow(QWidget *parent) : QDialog(parent), topics_(helpTopics())
  {
    setObjectName(QStringLiteral("helpWindow"));
    setWindowTitle(tr("ADT Help"));
    setModal(false);
    setWindowFlags((windowFlags() | Qt::WindowMinMaxButtonsHint)
      & ~Qt::WindowContextHelpButtonHint);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(10);
    auto findRow = new QHBoxLayout;
    find_ = new QLineEdit(this);
    find_->setObjectName(QStringLiteral("helpFind"));
    find_->setPlaceholderText(tr("Find in help"));
    find_->setAccessibleName(tr("Find in help"));
    find_->setClearButtonEnabled(true);
    previous_ = new QToolButton(this);
    next_ = new QToolButton(this);
    previous_->setObjectName(QStringLiteral("helpPrevious"));
    next_->setObjectName(QStringLiteral("helpNext"));
    previous_->setArrowType(Qt::UpArrow);
    next_->setArrowType(Qt::DownArrow);
    previous_->setToolTip(tr("Previous match (Shift+Enter / Shift+F3)"));
    next_->setToolTip(tr("Next match (Enter / F3)"));
    previous_->setAccessibleName(tr("Previous match"));
    next_->setAccessibleName(tr("Next match"));
    previous_->setAutoRaise(true);
    next_->setAutoRaise(true);
    matchesLabel_ = new QLabel(this);
    matchesLabel_->setObjectName(QStringLiteral("helpMatches"));
    matchesLabel_->setMinimumWidth(
      matchesLabel_->fontMetrics().horizontalAdvance(tr("No matches")));
    findRow->addWidget(find_, 1);
    findRow->addWidget(previous_);
    findRow->addWidget(next_);
    findRow->addWidget(matchesLabel_);
    layout->addLayout(findRow);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("helpTopics"));
    list_->setAccessibleName(tr("Help topics"));
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setWordWrap(true);
    browser_ = new QTextBrowser(this);
    browser_->setObjectName(QStringLiteral("helpBrowser"));
    browser_->setAccessibleName(tr("Help text"));
    browser_->setOpenLinks(false);
    browser_->document()->setDocumentMargin(14);
    browser_->document()->setDefaultStyleSheet(QStringLiteral(
      "h2 { color: %1; margin-top: 22px; margin-bottom: 6px; } "
      "h3 { margin-top: 14px; margin-bottom: 4px; } "
      "a { color: %1; } th { background-color: %2; text-align: left; } "
      "td, th { padding: 5px; }")
      .arg(palette().color(QPalette::Link).name(),
        palette().color(QPalette::AlternateBase).name()));
    auto splitter = new QSplitter(this);
    splitter->setObjectName(QStringLiteral("helpSplitter"));
    splitter->addWidget(list_);
    splitter->addWidget(browser_);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);
    QString html;
    for (const auto &topic : topics_) {
      auto item = new QListWidgetItem(topic.title, list_);
      item->setData(Qt::UserRole, topic.anchor);
      html += topicHtml(topic);
    }
    browser_->setHtml(html);
    // Explicit anchors keep topic selection, scrolling, and search in sync.
    for (const auto &topic : topics_) {
      int position = -1;
      for (auto block = browser_->document()->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it)
          if (it.fragment().charFormat().anchorNames().contains(topic.anchor))
            position = block.position();
        if (position >= 0)
          break;
      }
      positions_.append(position);
    }
    splitter->setSizes({260, 680});
    setStyleSheet(QStringLiteral(
      "QListWidget#helpTopics { border: 1px solid %1; border-radius: 6px; padding: 4px; } "
      "QListWidget#helpTopics::item { padding: 7px 8px; border-radius: 4px; } "
      "QListWidget#helpTopics::item:selected { background: %2; color: %3; } "
      "QTextBrowser#helpBrowser { border: 1px solid %1; border-radius: 6px; }")
      .arg(palette().color(QPalette::Mid).name(),
        palette().color(QPalette::Highlight).name(),
        palette().color(QPalette::HighlightedText).name()));

    auto buttons = new QHBoxLayout;
    auto manual = new QPushButton(tr("User manual"), this);
    manual->setObjectName(QStringLiteral("helpManual"));
    manual->setAutoDefault(false);
    auto close = new QPushButton(tr("Close"), this);
    close->setObjectName(QStringLiteral("helpClose"));
    close->setAutoDefault(false);
    buttons->addWidget(manual);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    connect(manual, &QPushButton::clicked, this, [this] { openHelpManual(this); });
    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
      if (row >= 0)
        showTopic(topics_[row].anchor);
    });
    connect(browser_->verticalScrollBar(), &QScrollBar::valueChanged, this,
      [this] { followScroll(); });
    connect(browser_, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
      if (url.isRelative() && url.path().isEmpty())
        showTopic(url.fragment());
      else
        QDesktopServices::openUrl(url);
    });
    connect(find_, &QLineEdit::textChanged, this, [this] { updateMatches(); });
    connect(find_, &QLineEdit::returnPressed, this, [this] {
      findNext(QApplication::keyboardModifiers() & Qt::ShiftModifier ? -1 : 1);
    });
    connect(previous_, &QToolButton::clicked, this, [this] { findNext(-1); });
    connect(next_, &QToolButton::clicked, this, [this] { findNext(1); });
    auto shortcut = [this](const QList<QKeySequence> &keys, auto callback) {
      auto action = new QAction(this);
      action->setShortcuts(keys);
      addAction(action);
      connect(action, &QAction::triggered, this, callback);
    };
    shortcut(QKeySequence::keyBindings(QKeySequence::Find), [this] {
      find_->setFocus();
      find_->selectAll();
    });
    auto nextKeys = QKeySequence::keyBindings(QKeySequence::FindNext);
    if (!nextKeys.contains(QKeySequence(Qt::Key_F3)))
      nextKeys.append(QKeySequence(Qt::Key_F3));
    shortcut(nextKeys, [this] { findNext(1); });
    auto previousKeys = QKeySequence::keyBindings(QKeySequence::FindPrevious);
    const QKeySequence shiftF3(Qt::SHIFT | Qt::Key_F3);
    if (!previousKeys.contains(shiftF3))
      previousKeys.append(shiftF3);
    shortcut(previousKeys, [this] { findNext(-1); });
    updateMatches();
    resize(980, 680);
    if (auto screen = this->screen())
      resize(size().boundedTo(screen->availableGeometry().size() - QSize(40, 60)));
    showTopic(QStringLiteral("getting-started"));
  }

  void showTopic(const QString &anchor)
  {
    for (int i = 0; i < topics_.size(); ++i) {
      if (topics_[i].anchor != anchor)
        continue;
      QScopedValueRollback<bool> guard(navigating_, true);
      selectTopic(i);
      browser_->scrollToAnchor(anchor);
      return;
    }
  }

private:
  QVector<Topic> topics_;
  QVector<int> positions_;
  QListWidget *list_;
  QTextBrowser *browser_;
  QLineEdit *find_;
  QToolButton *previous_, *next_;
  QLabel *matchesLabel_;
  QVector<QTextCursor> matches_;
  int currentMatch_ = -1;
  bool navigating_ = false;

  void selectTopic(int index)
  {
    QSignalBlocker blocker(list_);
    list_->setCurrentRow(index);
    list_->scrollToItem(list_->currentItem());
  }

  int topicAt(int position) const
  {
    int current = 0;
    for (int i = 0; i < positions_.size(); ++i)
      if (positions_[i] >= 0 && positions_[i] <= position)
        current = i;
    return current;
  }

  void followScroll()
  {
    if (navigating_)
      return;
    int current = 0;
    auto document = browser_->document();
    const int top = browser_->verticalScrollBar()->value() + 24;
    for (int i = 0; i < positions_.size(); ++i)
      if (positions_[i] >= 0 && document->documentLayout()->blockBoundingRect(
          document->findBlock(positions_[i])).top() <= top)
        current = i;
    selectTopic(current);
  }

  void updateMatches()
  {
    const int start = currentMatch_ >= 0 ? matches_[currentMatch_].selectionStart()
      : browser_->cursorForPosition(QPoint(0, 0)).position();
    matches_.clear();
    currentMatch_ = -1;
    if (!find_->text().isEmpty()) {
      QTextCursor cursor(browser_->document());
      while (!(cursor = browser_->document()->find(find_->text(), cursor)).isNull()) {
        if (currentMatch_ < 0 && cursor.selectionStart() >= start)
          currentMatch_ = matches_.size();
        matches_.append(cursor);
      }
      if (currentMatch_ < 0 && !matches_.isEmpty())
        currentMatch_ = 0;
    }
    showMatches();
  }

  void findNext(int direction)
  {
    if (matches_.isEmpty())
      return;
    currentMatch_ = (currentMatch_ + direction + matches_.size()) % matches_.size();
    showMatches();
  }

  void showMatches()
  {
    QList<QTextEdit::ExtraSelection> selections;
    QColor highlight = browser_->palette().color(QPalette::Highlight);
    highlight.setAlpha(55);
    for (int i = 0; i < matches_.size(); ++i) {
      QTextEdit::ExtraSelection selection;
      selection.cursor = matches_[i];
      selection.format.setBackground(i == currentMatch_
        ? browser_->palette().color(QPalette::Highlight) : highlight);
      if (i == currentMatch_)
        selection.format.setForeground(browser_->palette().color(QPalette::HighlightedText));
      selections.append(selection);
    }
    browser_->setExtraSelections(selections);
    previous_->setEnabled(!matches_.isEmpty());
    next_->setEnabled(!matches_.isEmpty());
    matchesLabel_->setText(find_->text().isEmpty() ? QString()
      : matches_.isEmpty() ? tr("No matches")
      : tr("%1 of %2").arg(currentMatch_ + 1).arg(matches_.size()));
    if (currentMatch_ < 0)
      return;
    QScopedValueRollback<bool> guard(navigating_, true);
    // Extra selections own the highlights; keep the normal cursor collapsed
    // so clearing Find also removes the current match's visible selection.
    auto cursor = matches_[currentMatch_];
    cursor.setPosition(cursor.selectionStart());
    browser_->setTextCursor(cursor);
    browser_->ensureCursorVisible();
    selectTopic(topicAt(matches_[currentMatch_].selectionStart()));
  }
};

/**
 * @brief Decorative About illustration; it never connects to a PV.
 *
 * Shows an orbit-like array with markers, lines, and a filled max/min band.
 * Clicking it, or pressing Space or Enter, adds an orbit bump that is then
 * corrected away.
 */
class AboutBanner final : public QWidget
{
public:
  static constexpr int NBPMS = 40;

  explicit AboutBanner(QWidget *parent = nullptr) : QWidget(parent), timer_(this)
  {
    setObjectName(QStringLiteral("aboutBanner"));
    setAccessibleName(tr("Illustrative ADT array display"));
    setAccessibleDescription(tr("Illustrative orbit readings that are not connected to any PV. "
      "Click or press Space to add an orbit bump."));
    setToolTip(tr("Click to add an orbit bump"));
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
    resetEnvelope();
    timer_.setObjectName(QStringLiteral("aboutAnimationTimer"));
    timer_.setInterval(40);
    connect(&timer_, &QTimer::timeout, this, [this] { advance(clock_.restart() / 1000.0); });
  }

  QSize sizeHint() const override { return QSize(460, 136); }

protected:
  void showEvent(QShowEvent *event) override
  {
    QWidget::showEvent(event);
    clock_.start();
    timer_.start();
  }

  void hideEvent(QHideEvent *event) override
  {
    timer_.stop();
    clock_.invalidate();
    bumpTime_ = -1;
    resetEnvelope();
    QWidget::hideEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      startBump();
      event->accept();
    } else {
      QWidget::mousePressEvent(event);
    }
  }

  void keyPressEvent(QKeyEvent *event) override
  {
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Enter) {
      if (!event->isAutoRepeat())
        startBump();
      event->accept();
    } else {
      QWidget::keyPressEvent(event);
    }
  }

  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal scale = qMin(width() / 460.0, height() / 136.0);
    p.translate((width() - 460 * scale) / 2, (height() - 136 * scale) / 2);
    p.scale(scale, scale);
    const QColor base = palette().color(QPalette::Base);
    const QColor ink = palette().color(QPalette::Text);
    const QColor accent = palette().color(QPalette::Link);
    const bool dark = base.lightness() < 128;
    const QColor trace = dark ? QColor("#ff7b72") : QColor("#c8372d");
    QColor band = accent;
    band.setAlpha(dark ? 70 : 45);
    QColor tint = accent;
    tint.setAlpha(18);

    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.setBrush(base);
    p.drawRoundedRect(QRectF(0.5, 0.5, 459, 135), 10, 10);
    const QRectF plot(18, 36, 424, 72);
    p.setPen(Qt::NoPen);
    p.setBrush(tint);
    p.drawRoundedRect(plot.adjusted(-6, -4, 6, 4), 6, 6);

    // Sector boundaries and grid divisions, as in an ADT display area.
    p.setPen(QPen(palette().color(QPalette::Mid), 0.7, Qt::DotLine));
    for (int i = 1; i < 8; ++i) {
      const qreal x = plot.left() + plot.width() * i / 8;
      p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    for (int i = 1; i < 4; ++i) {
      if (i == 2)
        continue;
      const qreal y = plot.top() + plot.height() * i / 4;
      p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    QColor centerLine = ink;
    centerLine.setAlpha(110);
    p.setPen(QPen(centerLine, 1));
    p.drawLine(QPointF(plot.left(), plot.center().y()), QPointF(plot.right(), plot.center().y()));

    auto xAt = [&plot](int i) { return plot.left() + plot.width() * (i + 0.5) / NBPMS; };
    auto yAt = [&plot](qreal v) { return plot.center().y() - v / 1.6 * plot.height() / 2; };

    QPainterPath envelope;
    for (int i = 0; i < NBPMS; ++i) {
      if (i == 0)
        envelope.moveTo(xAt(i), yAt(maxVals_[i]));
      else
        envelope.lineTo(xAt(i), yAt(maxVals_[i]));
    }
    for (int i = NBPMS - 1; i >= 0; --i)
      envelope.lineTo(xAt(i), yAt(minVals_[i]));
    envelope.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(band);
    p.drawPath(envelope);

    QPolygonF points;
    for (int i = 0; i < NBPMS; ++i)
      points.append(QPointF(xAt(i), yAt(value(i))));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(trace, 1.4));
    p.drawPolyline(points);
    p.setPen(QPen(trace, 3.6, Qt::SolidLine, Qt::SquareCap));
    p.drawPoints(points);

    // A lattice strip below the plot, like the one under the zoom area.
    QColor magnet = ink;
    magnet.setAlpha(150);
    p.setPen(Qt::NoPen);
    p.setBrush(magnet);
    for (int i = 0; i < NBPMS; ++i) {
      const qreal x = xAt(i) - plot.width() / NBPMS / 2;
      if (i % 2)
        p.drawRect(QRectF(x + 1, 117, 3.5, 6));
      else
        p.drawRect(QRectF(x + 2, 120, 6.5, 3));
    }

    qreal sum = 0, sumsq = 0;
    for (int i = 0; i < NBPMS; ++i) {
      const qreal v = value(i);
      sum += v;
      sumsq += v * v;
    }
    const qreal avg = sum / NBPMS;
    const qreal sdev = std::sqrt(qMax<qreal>(0, sumsq / NBPMS - avg * avg));
    QFont font = this->font();
    font.setPixelSize(12);
    font.setBold(true);
    p.setFont(font);
    p.setPen(ink);
    QString headline = tr("ARRAY DISPLAY TOOL");
    if (bumping())
      headline = bumpTime_ < 1.0 ? tr("ORBIT BUMP!") : tr("CORRECTING ORBIT");
    p.drawText(QRectF(24, 9, 220, 22), Qt::AlignVCenter, headline);
    font.setBold(false);
    font.setPixelSize(11);
    font.setFamily(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    p.setFont(font);
    p.setPen(accent);
    p.drawText(QRectF(236, 9, 200, 22), Qt::AlignRight | Qt::AlignVCenter,
      QStringLiteral("SDEV %1  AVG %2").arg(sdev, 0, 'f', 3).arg(avg, 0, 'f', 3));
    if (bumping()) {
      p.setPen(Qt::NoPen);
      p.setBrush(accent);
      p.drawRoundedRect(QRectF(24, 128, 412 * (1 - bumpTime_ / BUMPSECONDS), 3), 1.5, 1.5);
    }
  }

private:
  static constexpr qreal BUMPSECONDS = 3.2;
  QTimer timer_;
  QElapsedTimer clock_;
  qreal time_ = 0, bumpTime_ = -1;
  int bumpCenter_ = 9, nextBump_ = 0;
  qreal minVals_[NBPMS], maxVals_[NBPMS];

  bool bumping() const { return bumpTime_ >= 0; }

  bool startBump()
  {
    if (bumping())
      return false;
    const int centers[] = {9, 27, 17, 34};
    bumpCenter_ = centers[nextBump_];
    nextBump_ = (nextBump_ + 1) % 4;
    bumpTime_ = 0;
    update();
    return true;
  }

  qreal value(int i) const
  {
    const qreal phase = 2 * M_PI * i / NBPMS;
    qreal v = 0.5 * std::sin(phase * 4.3 + time_ * 0.9)
      + 0.17 * std::sin(phase * 11.2 - time_ * 0.55);
    if (bumping()) {
      // Grow the bump quickly, then let the correction remove it.
      const qreal amplitude = bumpTime_ < 0.6 ? bumpTime_ / 0.6
        : std::exp(-3.0 * (bumpTime_ - 0.6) / (BUMPSECONDS - 0.6)) *
          (1 - (bumpTime_ - 0.6) / (BUMPSECONDS - 0.6));
      const qreal d = i - bumpCenter_;
      v += 0.95 * amplitude * std::exp(-d * d / 4.5);
    }
    return v;
  }

  void resetEnvelope()
  {
    for (int i = 0; i < NBPMS; ++i)
      minVals_[i] = maxVals_[i] = value(i);
  }

  void advance(qreal seconds)
  {
    if (!std::isfinite(seconds) || seconds <= 0)
      return;
    time_ += seconds;
    if (bumping()) {
      bumpTime_ += seconds;
      if (bumpTime_ >= BUMPSECONDS)
        bumpTime_ = -1;
    }
    // Let the max/min band relax so the illustration keeps moving.
    for (int i = 0; i < NBPMS; ++i) {
      const qreal v = value(i);
      maxVals_[i] = qMax(v, maxVals_[i] - 0.18 * seconds);
      minVals_[i] = qMin(v, minVals_[i] + 0.18 * seconds);
    }
    update();
  }
};

QString versionText()
{
  QString version = QString::fromLatin1(ADT_VERSION_STRING);
  if (version.startsWith(QStringLiteral("ADT ")))
    version.remove(0, 4);
  return version;
}
} // namespace

QUrl helpManualUrl()
{
  const QString configured = qEnvironmentVariable("ADT_HELP_PATH").trimmed();
  if (!configured.isEmpty()) {
    const QUrl supplied(configured);
    return supplied.isValid() && supplied.scheme().size() > 1
      ? supplied : QUrl::fromLocalFile(QFileInfo(configured).absoluteFilePath());
  }
  return QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
    + QStringLiteral("/adt-manual.html"));
}

bool openHelpManual(QWidget *parent)
{
  const QUrl url = helpManualUrl();
  // The default manual is generated from the same embedded topics. A site may
  // instead select its installed manual or website through ADT_HELP_PATH.
  if (qEnvironmentVariable("ADT_HELP_PATH").trimmed().isEmpty()) {
    QString contents, body;
    for (const auto &topic : helpTopics()) {
      contents += QStringLiteral("<li><a href=\"#%1\">%2</a></li>")
        .arg(topic.anchor.toHtmlEscaped(), topic.title.toHtmlEscaped());
      body += topicHtml(topic);
    }
    const QByteArray html = QStringLiteral(
      "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      "<title>ADT User Manual</title><style>"
      "body{font:16px/1.6 system-ui,sans-serif;max-width:960px;margin:40px auto;padding:0 20px;}"
      "h1,h2,h3{line-height:1.3;}h2{margin-top:2em;}a{color:#176c98;}"
      "table{border-collapse:collapse;}td,th{border:1px solid #ccc;padding:6px 10px;text-align:left;}"
      "code{background:#f1f4f6;padding:2px 4px;}pre{overflow:auto;background:#f1f4f6;padding:12px;}"
      "nav{background:#f1f4f6;padding:12px 24px;border-radius:8px;}"
      "</style></head><body><h1>ADT User Manual</h1>"
      "<nav aria-label=\"Contents\"><h2>Contents</h2><ul>%1</ul></nav>%2</body></html>")
      .arg(contents, body).toUtf8();
    QSaveFile file(url.toLocalFile());
    if (!QDir().mkpath(QFileInfo(file.fileName()).absolutePath()) ||
        !file.open(QIODevice::WriteOnly) || file.write(html) != html.size() || !file.commit()) {
      QMessageBox::warning(parent, QObject::tr("ADT manual"),
        QObject::tr("Cannot prepare the user manual: %1").arg(file.errorString()));
      return false;
    }
  }
  if (QDesktopServices::openUrl(url))
    return true;
  QMessageBox::warning(parent, QObject::tr("ADT manual"),
    QObject::tr("Cannot open the user manual at %1").arg(url.toDisplayString()));
  return false;
}

QDialog *showHelp(QWidget *parent, const QString &topic)
{
  auto help = parent ? static_cast<HelpWindow *>(parent->findChild<QDialog *>(
    QStringLiteral("helpWindow"), Qt::FindDirectChildrenOnly)) : nullptr;
  if (!help)
    help = new HelpWindow(parent);
  help->showNormal();
  if (!topic.isEmpty())
    help->showTopic(topic);
  help->raise();
  help->activateWindow();
  return help;
}

void showAbout(QWidget *parent)
{
  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("aboutDialog"));
  dialog.setWindowTitle(QObject::tr("About ADT"));
  dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  auto layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(18, 18, 18, 14);
  layout->setSpacing(6);
  layout->addWidget(new AboutBanner(&dialog));
  auto title = new QLabel(QStringLiteral("ADT"), &dialog);
  title->setObjectName(QStringLiteral("aboutTitle"));
  QFont font = title->font();
  font.setPointSizeF(font.pointSizeF() > 0 ? font.pointSizeF() * 1.7 : 18);
  font.setBold(true);
  title->setFont(font);
  layout->addWidget(title);
  // ADT's Windows build does not compile with /utf-8, so spell non-ASCII
  // text as UTF-8 bytes.
  auto subtitle = new QLabel(QString::fromUtf8("Version %1 \xC2\xB7 EPICS Array Display Tool")
    .arg(versionText()), &dialog);
  subtitle->setObjectName(QStringLiteral("aboutSubtitle"));
  subtitle->setWordWrap(true);
  layout->addWidget(subtitle);
  auto credits = new QTextBrowser(&dialog);
  credits->setObjectName(QStringLiteral("aboutCredits"));
  credits->setAccessibleName(QObject::tr("Author credits"));
  credits->setFrameShape(QFrame::NoFrame);
  QPalette creditsPalette = credits->palette();
  creditsPalette.setColor(QPalette::Base, dialog.palette().color(QPalette::Window));
  credits->setPalette(creditsPalette);
  credits->document()->setDocumentMargin(0);
  credits->setHtml(QString::fromUtf8(
    "<p>Live displays of EPICS process-variable arrays, with stored sets, "
    "references, differences and snapshots.<br>"
    "Developed at Argonne National Laboratory</p>"
    "<p><b>Qt port development and maintenance</b><br>"
    "Robert Soliday (APS) \xE2\x80\x94 <a href=\"mailto:soliday@anl.gov\">soliday@anl.gov</a></p>"
    "<p><b>Original ADT</b><br>"
    "Kenneth Evans, Jr. (APS) \xE2\x80\x94 Motif application and reference manual</p>"
    "<p>Written by Dwarfs in the Waterfall Glen. Distributed under the "
    "repository's Software License Agreement.</p>"));
  credits->setOpenExternalLinks(true);
  layout->addWidget(credits, 1);
  auto footer = new QLabel(QString::fromUtf8("Qt %1 \xC2\xB7 %2\nRunning %3 %4 on %5")
    .arg(QString::fromLatin1(qVersion()), QString::fromLatin1(EPICS_VERSION_STRING),
      QSysInfo::kernelType(), QSysInfo::kernelVersion(), QSysInfo::machineHostName()),
    &dialog);
  footer->setObjectName(QStringLiteral("aboutFooter"));
  footer->setForegroundRole(QPalette::PlaceholderText);
  footer->setWordWrap(true);
  footer->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(footer);
  auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
  auto help = buttons->addButton(QObject::tr("Open help"), QDialogButtonBox::HelpRole);
  help->setObjectName(QStringLiteral("aboutOpenHelp"));
  bool openHelp = false;
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(help, &QPushButton::clicked, &dialog, [&] {
    openHelp = true;
    dialog.accept();
  });
  layout->addWidget(buttons);
  buttons->button(QDialogButtonBox::Ok)->setDefault(true);
  buttons->button(QDialogButtonBox::Ok)->setFocus();
  // Show all credits when space permits, and keep scrolling on small screens
  // or with large desktop fonts instead of clipping the last lines. Measure a
  // detached copy: a hidden QTextBrowser lays out its own document for its
  // unshown viewport, which greatly overstates the height.
  QScopedPointer<QTextDocument> measure(credits->document()->clone());
  measure->setTextWidth(464);
  const int chromeHeight = dialog.sizeHint().height() - credits->sizeHint().height();
  QSize desired(500, chromeHeight + qCeil(measure->size().height()) + 12);
  if (auto screen = dialog.screen())
    desired = desired.boundedTo(screen->availableGeometry().size() - QSize(40, 60));
  dialog.resize(desired);
  dialog.exec();
  if (openHelp)
    showHelp(parent, QStringLiteral("getting-started"));
}

void populateHelpMenu(QMenu *menu, QWidget *owner)
{
  auto help = menu->addAction(QObject::tr("Help &Topics"));
  help->setObjectName(QStringLiteral("helpAction"));
  auto keys = QKeySequence::keyBindings(QKeySequence::HelpContents);
  if (!keys.contains(QKeySequence(Qt::Key_F1)))
    keys.append(QKeySequence(Qt::Key_F1));
  help->setShortcuts(keys);
  QObject::connect(help, &QAction::triggered, owner, [owner] {
    showHelp(owner, QStringLiteral("getting-started"));
  });
  auto shortcuts = menu->addAction(QObject::tr("&Keyboard Shortcuts"));
  shortcuts->setObjectName(QStringLiteral("keyboardShortcutsAction"));
  QObject::connect(shortcuts, &QAction::triggered, owner, [owner] {
    showHelp(owner, QStringLiteral("keyboard-shortcuts"));
  });
  auto manual = menu->addAction(QObject::tr("&User Manual"));
  manual->setObjectName(QStringLiteral("manualAction"));
  manual->setToolTip(QObject::tr("Open the bundled manual or the site manual selected by ADT_HELP_PATH"));
  QObject::connect(manual, &QAction::triggered, owner, [owner] { openHelpManual(owner); });
  menu->addSeparator();
  auto about = menu->addAction(QObject::tr("&About ADT"));
  about->setObjectName(QStringLiteral("aboutAction"));
  QObject::connect(about, &QAction::triggered, owner, [owner] { showAbout(owner); });
}
} // namespace adt
