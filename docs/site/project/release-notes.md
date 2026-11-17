# Release notes

## ADT 2.1.1.0

Released November 17, 2026. Changes since ADT 2.1.0.0:

### Build fixes

- Use the discovered EPICS Base library directory for Linux and macOS
  application links, including an explicit `EPICS_BASE_LIB_DIR` override.
- Link Readline when the selected EPICS Base static `libCom.a` requires it.
  Apply the same dependency selection to the Qt UI and export regression
  executables.

### Documentation

- Add the ADT (Qt) documentation website, built from the README and all
  twelve topics in the built-in operator guide.
- Provide a standalone HTML manual and downloadable Markdown guide and
  README, generated from the same sources as the website.
- Add `make docs`, configurable `DOCS_BASE` for hosting, local search,
  link and topic validation, and documentation cleanup targets.
- Document isolated native builds, regression checks, source maintenance,
  and preparation for APS hosting.
- Update the application version, About dialog, website, and generated
  manuals to 2.1.1.0.
