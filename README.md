# adt

ADT now builds with Qt by default. To build the legacy Motif/X11 version,
invoke `MOTIF=1 make`.

After building, the binary is placed in `bin/<OS>-<ARCH>/adt`. It accepts
several options to control startup behaviour:

```
adt [-a directory] [-f pvfile] [-s center_sector] [-z number_of_sectors] [-d]
```

- `-a <directory>` specify the ADT home directory
- `-f <pvfile>` open a PV file at startup
- `-s <center_sector>` set the initial zoomed-on sector
- `-z <number_of_sectors>` set the initial zoom range
- `-d` enable diff mode
- `-h` display usage information

Example:

```
make
./bin/Linux-x86_64/adt -f pv/sr.bpm.pv
```

This compiles the Qt version and starts ADT with the sample PV file.

## Configuration File

ADT looks for a configuration file named `adtrc` to locate PV and snapshot
directories and populate the File/Load menu. The file is searched for in the
following locations, in order:

1. the directory specified with `-a <adthome>`
2. `$HOME/.adtrc`
3. `$ADTHOME/adtrc`
4. the current working directory

If no configuration file is found, ADT defaults to using `./pv` and `./snap`
and shows only the Custom option in the File/Load menu. An example
configuration file is provided at `src/adtrc`.
