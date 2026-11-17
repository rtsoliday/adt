#!/usr/bin/env python3
"""Build and validate the ADT (Qt) website and standalone manuals."""

import argparse
from html.parser import HTMLParser
import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "docs/site"
OUTPUT = SITE / "dist"
SOURCE = runpy.run_path(str(ROOT / "scripts/sync-docs.py"))


class Page(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.ids = set()
        self.links = []
        self.title = False

    def handle_starttag(self, tag, attrs):
        attributes = dict(attrs)
        for key in ("id", "name"):
            if attributes.get(key):
                self.ids.add(attributes[key])
        if tag == "title":
            self.title = True
        for key in ("href", "src"):
            if attributes.get(key):
                self.links.append(attributes[key])


def verify(base):
    pages = {}
    for path in OUTPUT.rglob("*.html"):
        page = Page()
        page.feed(path.read_text(encoding="utf-8"))
        pages[path.resolve()] = page
    if not pages:
        raise RuntimeError("No generated HTML pages found")
    issues = []
    count = 0
    for path, page in pages.items():
        if not page.title:
            issues.append(f"{path.relative_to(OUTPUT)}: missing title")
        for url in page.links:
            split = urlsplit(url)
            if split.scheme or split.netloc:
                continue
            name = unquote(split.path)
            if name.startswith("/"):
                if not name.startswith(base):
                    issues.append(f"{path.relative_to(OUTPUT)}: URL escapes base: {url}")
                    continue
                destination = (OUTPUT / name[len(base):]).resolve()
            else:
                destination = (path.parent / name).resolve() if name else path
            if not destination.is_relative_to(OUTPUT.resolve()):
                issues.append(f"{path.relative_to(OUTPUT)}: URL escapes site: {url}")
                continue
            if destination.is_dir():
                destination /= "index.html"
            if not destination.exists() and not destination.suffix:
                destination = destination.with_suffix(".html")
            count += 1
            if not destination.exists():
                issues.append(f"{path.relative_to(OUTPUT)}: missing {url}")
            elif split.fragment and destination in pages and unquote(split.fragment) not in pages[destination].ids:
                issues.append(f"{path.relative_to(OUTPUT)}: missing anchor {url}")
    manual = (OUTPUT / "downloads/adt-manual.html").resolve()
    sections = SOURCE["guide_sections"]()
    for title, _ in sections:
        route = SOURCE["GUIDE_ROUTES"][title]
        path = (OUTPUT / (route.lstrip("/") + ".html")).resolve()
        fragment = SOURCE["anchor"](title)
        if path not in pages or fragment not in pages[path].ids:
            issues.append(f"Missing guide topic: {title}")
        if manual not in pages or fragment not in pages[manual].ids:
            issues.append(f"Missing standalone manual topic: {title}")
    guide = OUTPUT / "downloads/adt-qt-guide.md"
    if not guide.is_file() or guide.read_text(encoding="utf-8") != SOURCE["guide_markdown"](sections):
        issues.append("Markdown guide differs from the embedded topics or application version")
    readme = OUTPUT / "downloads/README.md"
    if not readme.is_file() or readme.read_bytes() != (ROOT / "README.md").read_bytes():
        issues.append("README download differs from README.md")
    if issues:
        raise RuntimeError("\n".join(sorted(set(issues))))
    print(f"Validated {len(pages)} HTML pages, {count} local links/assets, and all {len(sections)} guide topics.")


def main():
    if sys.version_info < (3, 9):
        raise RuntimeError("Python 3.9 or newer is required")
    os.umask(0o022)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=os.environ.get("DOCS_BASE", "/"))
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    if (not re.fullmatch(r"/(?:[A-Za-z0-9_.-]+/)*", args.base)
            or any(part in (".", "..") for part in args.base.split("/"))):
        parser.error("--base must have leading/trailing slashes and no dot segments")
    if not args.check_only:
        SOURCE["synchronize"]()
        node = shutil.which("node")
        npm = shutil.which("npm") or shutil.which("npm.cmd")
        if not node or not npm:
            raise RuntimeError("Node.js 22 or newer and npm are required")
        version = subprocess.check_output([node, "--version"], text=True).strip()
        if int(version.lstrip("v").split(".")[0]) < 22:
            raise RuntimeError(f"Node.js 22 or newer is required; found {version}")
        npm_env = {**os.environ, "npm_config_update_notifier": "false"}
        if not (SITE / "node_modules/vitepress").is_dir() or not (SITE / "node_modules/markdown-it").is_dir():
            subprocess.run([npm, "ci", "--no-audit", "--no-fund"], cwd=SITE, env=npm_env, check=True)
        subprocess.run(
            [npm, "run", "build"], cwd=SITE, check=True,
            env={**npm_env, "DOCS_BASE": args.base},
        )
    if not (OUTPUT / "index.html").is_file():
        raise RuntimeError("No built website found; run make docs first")
    verify(args.base)
    if not args.check_only:
        destination = ROOT / "docs/html"
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(OUTPUT, destination)
        for path in [destination, *destination.rglob("*")]:
            path.chmod(0o755 if path.is_dir() else 0o644)
        print(f"Built {destination / 'index.html'} for {args.base}")


if __name__ == "__main__":
    main()
