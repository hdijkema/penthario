#!/usr/bin/env python3
"""Check a Jekyll build for complete Markdown navigation and leaked theme assets."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlparse
import argparse


class Page(HTMLParser):
    """Collect the homepage guide list and the documentation sidebar separately."""

    def __init__(self, file):
        super().__init__()
        self.home_guides = []
        self.sidebar_guides = []
        self.in_home_guides = False
        self.in_sidebar = False
        self.feed(file.read_text(encoding="utf-8"))

    def handle_starttag(self, tag, attributes):
        attributes = dict(attributes)
        if tag == "section" and attributes.get("id") == "documentation":
            self.in_home_guides = True
        if tag == "aside" and "doc-sidebar" in attributes.get("class", "").split():
            self.in_sidebar = True
        if tag == "a" and "href" in attributes:
            link = attributes["href"]
            if self.in_home_guides and not urlparse(link).scheme:
                self.home_guides.append(link)
            if self.in_sidebar:
                self.sidebar_guides.append(link)

    def handle_endtag(self, tag):
        if tag == "section":
            self.in_home_guides = False
        if tag == "aside":
            self.in_sidebar = False


def check(site):
    """Compare rendered guide links with source Markdown, including any theme CSS."""
    source = Path(__file__).resolve().parents[1] / "docs"
    guides = [file.relative_to(source) for file in source.rglob("*")
              if file.suffix in (".md", ".markdown")
              and file.relative_to(source).as_posix() != "assets/README.md"]
    expected = {"/pgadmin-jr/" + file.with_suffix(".html").as_posix() for file in guides}
    home = Page(site / "index.html")
    assert len(home.home_guides) == len(expected), "Unexpected homepage guide count"
    assert set(home.home_guides) == expected, "Homepage contains missing guides or non-Markdown assets"
    for file in guides:
        page = Page(site / file.with_suffix(".html"))
        assert len(page.sidebar_guides) == len(expected), f"Unexpected sidebar count: {file}"
        assert set(page.sidebar_guides) == expected, f"Invalid sidebar entries: {file}"
    theme_css = site / "assets/css/style.css"
    if theme_css.exists():
        css = theme_css.read_text(encoding="utf-8").lower()
        assert "<html" not in css and "<!doctype" not in css, "Theme CSS received an HTML layout"
    print(f"{len(guides)} Markdown guides: homepage/sidebar navigation and theme asset isolation passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path, help="Generated site directory containing index.html")
    check(parser.parse_args().site)
