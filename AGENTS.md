# Agent Instructions

This repository contains the Array Display Tool (ADT) source code and documentation.

## Project Layout

This repository is organized into the following top-level files and directories:

- `AGENTS.md`: agent instructions and guidelines.
- `README.md`: project overview, build prerequisites, and usage examples.
- `LICENSE`: licensing information.
- `Makefile`, `Makefile.build`, `Makefile.rules`: top-level build configuration.
- `doc/`: documentation files.
- `src/`: source code.
- `snap/`: reference snapshots for EPICS PVs.
- `pv/`: PV files for ADT used to configure it's layout.

## Coding Conventions
- **Indentation:** Two spaces per level; tabs are avoided.
- **Brace style:** Opening braces on the same line as function or control statements (K&R style).
- **Comments:** Each source file begins with a Doxygen-style block comment (`/** ... */`) documenting the file, authorship, and license. Functions also include Doxygen headers.
- **Naming:**
  - Internal helper functions and variables use camelCase (e.g., `bigBuffer`, `compute_average`).
  - Macros and constants are uppercase with underscores (e.g., `SDDS_SHORT`, `INITIAL_BIG_BUFFER_SIZE`).
- **File organization:**
  - C source files use `.c` extension; C++ utilities use `.cc`.
- **Header inclusion order:** Project headers (`"..."`) first, followed by standard library headers (`<...>`), then third-party headers.
- **Line length:** No strict maximum; some lines exceed 200 characters.

These guidelines should be followed when contributing new code or documentation to maintain consistency with the existing project.

## Compiling

ADT now builds with Qt by default. To build the legacy Motif/X11
version, invoke `MOTIF=1 make`.

## Machine-managed coding quickstart
This supplement is generated from repository evidence and leaves the handwritten guidance above unchanged.

<!-- BEGIN MACHINE:summary -->
## Quick start
- Repository-local guidance is sufficient: start with `AGENTS.md`, `README.md`, `docs/`, build/test/config files, and the source tree.
- ADT now builds with Qt by default. To build the legacy Motif/X11 version, invoke MOTIF=1 make.
- Primary work areas: `doc`, `pv`, `snap`, `src`.

## Read first
- `README.md`: Primary project overview and workflow notes
- `src/Makefile`: Build system entry point or dependency manifest
- `LICENSE`: Repository configuration that affects local work
- `patch.txt`: Supporting repository evidence
- `src/adt.c`: Source file named after the repository

## Build and test
- Documented setup/build commands: `make` (Qt default), `MOTIF=1 make` (legacy Motif/X11).
- Detected build systems: GNU Make.
- Unknown: no test workflow evidence was found in the inspected files.
- Likely run commands or operator entry points: `./bin/Linux-x86_64/adt -f pv/sr.bpm.pv`.

## Operational warnings
- Local checkout layout appears significant; avoid casual changes to sibling-repo assumptions or relative paths.
- Legacy compatibility paths are still present; confirm which mode is actually in use before changing defaults.
- Platform-specific dependency setup matters; do not assume one platform's build recipe carries over unchanged.

## Compatibility constraints
- Legacy components are still present; prefer the documented default path before switching to older modes.
- Cross-platform support exists, but platform-specific dependency setup matters.
- Build and runtime behavior likely depends on neighboring core toolkit checkouts.

## Related knowledge
- Repository-local documentation should be treated as authoritative.
- If a shared `llm-wiki/` directory is present in this workspace or parent folder, consult [the matching repo page](../llm-wiki/repos/adt.md) for additional architectural context.
- If no shared wiki is present, continue using repository-local evidence only.
- If present in this workspace, [the cross-repo map](../llm-wiki/insights/cross-repo-map.md) helps explain related repositories.
<!-- END MACHINE:summary -->
