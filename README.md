# adt

ADT now builds with Qt by default. To build the legacy Motif/X11
version, invoke `MOTIF=1 make`.

The Qt version accepts `-f <pvfile>` to load a PV file on startup and
`-a <directory>` to set the ADT home directory, matching the legacy
options.
