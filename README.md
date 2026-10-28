# ADT (Qt)

ADT now builds with Qt by default. To build the legacy Motif/X11 version,
invoke `MOTIF=1 make`. Both builds use the executable name `adt`; there is
no separate `qtadt` executable. Version information is available from
**Help > About ADT**.

## Building

The build requires GNU Make, a C++17 compiler, SDDS headers and libraries,
EPICS Base headers and libraries, and Qt development files. The Unix recipe
currently uses `pkg-config Qt5Widgets`. The Windows recipe currently names
Qt 6.8.2 at
`C:\Qt\6.8.2\msvc2022_64`. The link also needs the libraries selected by
the Makefiles, including GSL, zlib, LZMA, and, on Linux, readline and
OpenMP. Keep the sibling `SDDS` checkout available, or explicitly set
`SDDS_REPO`; set `EPICS_BASE` to select a particular EPICS Base installation.

Top-level `make` rebuilds dependencies in the sibling SDDS checkout.
The link and install rules copy executables into `BIN_DIR` and, when set,
`EPICS_BIN_DIR`. A checkout in an EPICS extensions tree can also populate
that tree's installation directories. Check these paths before building.

For an isolated Linux application build against already-built dependencies,
run from the ADT repository root, substituting the EPICS Base path:

```sh
scratch=$(mktemp -d /tmp/adt-build.XXXXXX)
make -C src SDDS_REPO="$PWD/../SDDS" EPICS_BASE=/path/to/epics-base \
  OBJ_DIR="$scratch/obj" BIN_DIR="$scratch/bin" LIB_DIR="$scratch/lib" \
  EPICS_BIN_DIR= EPICS_LIB_DIR= EPICS_INC_DIR= BUNDLE_OPENMP=0
```

This uses existing dependency outputs without rebuilding sibling repositories.
The application and `xintolat` outputs are placed under `$scratch`, which can
be removed when no longer needed.

## Starting ADT

After building, the binary is placed in `bin/<OS>-<ARCH>/adt`. It accepts
several options to control startup behaviour:

```
adt [-a directory] [-f pvfile] [-s center_sector] [-z number_of_sectors] [-d] [-x] [-h|-?]
```

- `-a <directory>` specify the ADT home directory
- `-f <pvfile>` open a PV file at startup
- `-s <center_sector>` set the initial zoomed-on sector
- `-z <number_of_sectors>` set the initial zoom range
- `-d` store set 1 after startup and enable difference mode against it
- `-x` select Xorbit-specific directories from the configuration file and
  turn off status checking
- `-h`, `-?` display usage information and exit

Each option may also begin with `/`. Qt's own options, such as `-display`
and `-style`, are accepted. Help is printed to standard error without
opening a display, and ADT then exits with status 0. An unknown option, a
missing operand for `-a`, `-f`, `-s` or `-z`, a value of `-s` or `-z` that is
not an integer, or an unexpected argument prints `Invalid option` and the
usage text to standard error, and ADT exits with status 8, as the Motif
version did.

Example:

```
make
./bin/Linux-x86_64/adt -f pv/sr.bpm.pv
```

This compiles the Qt version and starts ADT with the sample PV file. The
sample files contain facility PV names. For an offline demonstration, set
Channel Access to an isolated simulated IOC or loopback-only address list
before starting the application. `-x` selects directories and turns off
status checking; it does not isolate Channel Access or add `Xorbit:` to PV
names. ADT reads PV values;
its Write menu writes snapshot files, not PVs.

## Configuration File

ADT looks for a configuration file named `adtrc` to locate PV and snapshot
directories and populate the File/Load menu. Configuration selection is:

1. With `-a <adthome>`, use `<adthome>/adtrc` only.
2. Otherwise, use `$HOME/.adtrc` if it exists.
3. Otherwise, use `$ADTHOME/adtrc` when `ADTHOME` is set, or `./adtrc`
   when it is unset. There is no additional current-directory fallback
   when `ADTHOME` is set.

The ADT home is the `-a` directory, otherwise `ADTHOME`, otherwise `.`.
The default PV and snapshot directories are `<ADT home>/pv` and
`<ADT home>/snap`, including when the selected configuration file is absent.
In that case the Load menu contains only Custom. An example configuration
file is provided at `src/adtrc`; its menu entries name the PV files in `pv/`.

`ADTPVDirectory` and `ADTSnapDirectory` override the default directories.
`ADTPVSubDirectory`, when present, selects a PV subdirectory of the ADT home
and takes precedence over `ADTPVDirectory`. With `-x`, `ADTXPVDirectory`
and `ADTXSnapDirectory` override their ordinary counterparts when supplied.
Each configuration page defines a Load submenu using `ADTMenuTitle` and the
`ADTPVFile` and `ADTMenuLabel` columns. A startup `-f` path is interpreted
relative to the current working directory, independently of the configured
PV directory. A relative `ADTReferenceFile` in a PV file is resolved against
the snapshot directory.

`pv/sr.bpm.short.pv` is an OrbitScreen file (`OSFileType`, with the
`ORBITSCREENLATTICE` lattice `pv/sr.short.lat`), not an ADT PV file. Neither
the Qt nor the Motif ADT can load it, and it is not in the example
configuration.

## Qt behavior and the historical manual

This README is the user manual for the Qt application.
[doc/adt.html](doc/adt.html) and `doc/adt.ps` are historical manuals for the
legacy Motif application, and their screenshot shows the Motif interface. The
HTML was generated from RTF, but the RTF source is not available; it is now
maintained by hand as a historical document, and a note at its top points
here. The HTML is not embedded in the Qt executable.

The Qt Help menu matches the other Qt ports (Qt StripTool, QtProbe, QtEDM
and QtALH). **Help Topics** (F1) opens a searchable guide for the Qt
application that is compiled into the executable from `src/adt_help.cc`.
**Keyboard Shortcuts** opens the guide at its shortcut and mouse reference.
**User Manual** writes the same guide as an HTML file in the user's cache
directory and opens it in the desktop browser; set `ADT_HELP_PATH` to a file
or URL to open a site manual instead. **About ADT** shows the version,
credits, and the Qt, EPICS and host details. There is no context Help.

For the Qt application, account for these differences when using the manual:

- There are no Options > EPICS start/stop/rescan controls. Connected
  channels are polled at the update interval, initially
  2000 ms unless a PV file specifies a positive `ADTTimeInterval`. Invalid
  nonpositive file intervals use 2000 ms. Replies are processed as they
  arrive, as in the Motif version, so a large layout does not fall behind the
  update interval. ADT keeps at most one read outstanding per channel; a
  reading whose read has been outstanding for more than twice the update
  interval, and at least 5 s, is unavailable until the server answers. Each array header reports an
  unavailable count, and point details label unavailable readings. A point
  without a value is drawn as a black square on the area's center line, with
  no line or bar to it. Options > Store, Read Snapshot, and Read Reference
  keep each point's availability with its value. When a stored set or the
  reference is displayed or subtracted, a point that is unavailable in either
  one has no value and is left out of the statistics.
- Snapshot files keep every historical column and value, including the `0`
  written for an unavailable point, and add a short `ADTAvailable` column
  (1 available, 0 unavailable). Reading a snapshot or reference marks points
  with `ADTAvailable` 0 unavailable; a file without the column is fully
  available, as before. Older ADT releases read these files and ignore the
  added column. Plot files leave out the rows of unavailable points; their
  `Index` column keeps the true positions, so `sddsplot` shows gaps.
- When a PV file has a `StatusName` column, and `-x` is not given, ADT reads
  each status PV in the same update as its value, as the Motif version did.
  A status name of `-` is not checked. Status 0 means invalid data and status
  2 means old data; other values are valid. Options > Check Status selects
  None, Invalid (exclude status 0) or All (also exclude status 2); All is the
  default, and the menu is enabled only for files with status checking. An
  excluded point stays plotted at its value with a gray dot (dark gray for
  invalid, lighter gray for old data), is left out of the statistics, and is
  counted as `Excluded: n/N` in the array header. A point whose status PV
  cannot be read is unavailable. Threshold PVs (`ThresholdName`) are not
  implemented. Unlike the Motif version, snapshot and plot files do not add
  `StatusName` and `Status` columns.
- An array whose PV file page sets a nonzero `ADTLogScale` is plotted in
  decades: log10 of the scale factor times the value, for the current
  values, a displayed stored set, and the max/min envelope. Its grid, center,
  and scale are in decades (`pv/sr.ionpump.pv` uses center -8, "log Torr").
  A nonpositive value has no logarithm: it is drawn as unavailable, and point
  details show `≤ 0 (log undefined)`, where the Motif version showed 0. In
  difference and reference modes the array plots the change in decades,
  log10(value) - log10(stored or reference value); a point where either value
  is nonpositive or unavailable is drawn as unavailable. SDEV, AVG, and MAX
  are computed over the linear scaled values of the valid, positive points
  and shown as log10 of each, or `n/a` when a statistic is not positive; in
  difference and reference modes they are computed on the plotted decade
  changes. Accumulated statistics accumulate the shown values, as the Motif
  version did. Stored sets and snapshot, reference, and plot files keep raw
  linear values. `pv/sr.logionpump.pv` sets `ADTLogScale` to 0 because its PVs
  already hold logarithms.
- `-x` selects configuration directories and turns off status checking, as
  in the Motif version. Supply simulation PV names explicitly in the PV file. The historical automatic prefix/removal of
  `Xorbit:` is not implemented in Qt.
- X resources, the ControlApp application-defaults file, `ADTHelpFile`, and
  Mosaic Help settings are for the legacy interface.
- The Qt View toggle parameter is `ADTFillMaxMin`, rather than the historical
  manual's `ADTFilledMaxMin`. Area center and scale settings are taken from
  the first array assigned to an area.
- Title-row SDEV, AVG, and MAX describe what is plotted: the current values
  minus the difference set in difference mode, minus the reference when the
  reference is displayed, and otherwise the current values. SDEV is the
  population standard deviation, computed from the mean in a second pass;
  only available points that Check Status does not exclude count. A statistic that cannot be represented shows
  `n/a`, and accumulated statistics leave such updates out.
- Qt supports the initial zoom sector/range options above, mouse-wheel
  zooming, and middle-click centering from display areas onto the zoom data.
  Point details use significant digits, preserving small readings.
- Snapshot and plot exports preserve double precision. Failed or incomplete snapshot/reference reads leave the prior set intact.
  Snapshot/reference inputs require one complete page per array, in the order
  of the loaded PV file. Pages after the last array are not read, so a
  snapshot of a PV file that begins with the same arrays can be used. SDDS can
  treat an incomplete trailing row in an uncounted ASCII page as the end of the
  file, so ADT cannot detect every truncated file.
  Snapshot reads reject nonnumeric and nonfinite values. Snapshot reads and
  writes use the existing SDDS `ADTSNAP`/`ValueString` format, and references
  use the same snapshot format. Snapshot and plot exports escape control names
  using SDDS string rules. Plot requires the external `sddsplot`
  program and a Unix shell. ADT looks for `sddsplot` on the `PATH` before it
  writes the temporary plot file, and warns when it is missing or when
  `/bin/sh` cannot be started; the file name reaches the shell as an argument,
  not as command text. The plot runs detached, survives ADT, and removes the
  file when it closes. Errors from a running `sddsplot` go to its own
  standard error, not to ADT.

Use the current menus and `adt -h` output together with this README for Qt
operation. Keep the historical manual as a reference for the Motif interface.
