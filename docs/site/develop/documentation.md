# Maintain these docs

The website uses VitePress with the shared presentation of QtProbe,
Qt StripTool, QtEDM, and QtALH. Python 3.9 or newer, Node.js 22 or newer,
and npm build the site.

## Build and preview

Run from the ADT repository root.

```sh
make docs
python3 -m http.server 8000 --bind 127.0.0.1 --directory docs/html
```

Open `http://127.0.0.1:8000/`. The build imports the source documentation,
installs locked npm dependencies when needed, renders the standalone manual,
builds local search, checks links and topic coverage, and writes the
validated static site to `docs/html/`.

For a preview that updates while editing, first run `make docs`, then
`npm run dev --prefix docs/site`. Run `make docs` again after changing an
imported source.

## Source documents

| Content | Authoritative source |
| --- | --- |
| Twelve operator-guide topics | `src/adt_help.cc`, in `guideSections` |
| Build, startup, configuration, and detailed Qt behavior | `README.md` |
| Application version | `src/adtVersion.h` |
| License | `LICENSE` |
| Homepage, downloads, authors, and this workflow | Authored Markdown under `docs/site/` |
| Navigation and shared presentation | `docs/site/.vitepress/` |

`scripts/sync-docs.py` extracts the C++ raw Markdown literals and rewrites
links between guide topics. Generated pages identify their source in a
comment. Edit the authoritative source and rebuild to update the website
and downloads together.

When guide headings change, update `GUIDE_ROUTES` in the import script,
navigation, and `docs/site/.gitignore`. The build checks that every embedded
topic appears in its mapped page and standalone manual.

## Prepare for APS hosting

```sh
make docs DOCS_BASE=/manuals/QtADT/
```

The output is prepared for `https://ops.aps.anl.gov/manuals/QtADT/`.
Publish the complete `docs/html/` contents to the corresponding web
directory when publication is authorized. The build checks links against
the selected prefix.

To validate an existing build, use its original prefix.

```sh
python3 scripts/build-docs.py --check-only --base /manuals/QtADT/
```

## Cleanup

```sh
make docs-clean
make docs-distclean
```

`docs-clean` removes the generated pages, downloads, caches, and HTML.
`docs-distclean` also removes the documentation's npm dependencies.
