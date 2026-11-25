#!/usr/bin/env python3
"""Check a Jekyll build for complete Markdown navigation and leaked theme assets."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlparse
import argparse
import json
import re
import xml.etree.ElementTree as ET


class Page(HTMLParser):
    """Collect the homepage guide list and the documentation sidebar separately."""

    def __init__(self, file):
        super().__init__()
        self.home_guides = []
        self.sidebar_guides = []
        self.metadata = {}
        self.canonical = []
        self.in_home_guides = False
        self.in_sidebar = False
        self.feed(file.read_text(encoding="utf-8"))

    def handle_starttag(self, tag, attributes):
        attributes = dict(attributes)
        if tag == "meta":
            key = attributes.get("name") or attributes.get("property")
            if key:
                self.metadata[key] = attributes.get("content", "")
        if tag == "link" and attributes.get("rel") == "canonical":
            self.canonical.append(attributes.get("href"))
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
    origin = "https://hdijkema.github.io"
    homepage_url = origin + "/pgadmin-jr/"
    check_metadata(home, homepage_url)
    for file in guides:
        page = Page(site / file.with_suffix(".html"))
        assert len(page.sidebar_guides) == len(expected), f"Unexpected sidebar count: {file}"
        assert set(page.sidebar_guides) == expected, f"Invalid sidebar entries: {file}"
        check_metadata(page, origin + "/pgadmin-jr/" + file.with_suffix(".html").as_posix())
    theme_css = site / "assets/css/style.css"
    if theme_css.exists():
        css = theme_css.read_text(encoding="utf-8").lower()
        assert "<html" not in css and "<!doctype" not in css, "Theme CSS received an HTML layout"
    locations = [node.text for node in ET.parse(site / "sitemap.xml").findall(
        ".//{http://www.sitemaps.org/schemas/sitemap/0.9}loc")]
    expected_locations = {origin + link for link in expected} | {homepage_url}
    assert len(locations) == len(expected_locations) and set(locations) == expected_locations, \
        "Sitemap must contain only the homepage and documentation URLs"
    for verification in source.glob("google*.html"):
        assert (site / verification.name).read_bytes() == verification.read_bytes(), \
            "Google verification file must remain byte-for-byte unchanged"
    html = (site / "index.html").read_text(encoding="utf-8")
    structured = re.search(r'<script type="application/ld\+json">\s*(.*?)\s*</script>', html, re.S)
    assert structured, "Missing application structured data"
    application = json.loads(structured.group(1))
    assert application["@type"] == "SoftwareApplication" and application["url"] == homepage_url
    print(f"{len(guides)} guides: navigation, metadata, sitemap, verification and theme isolation passed")


def check_metadata(page, canonical):
    """Require one canonical URL and usable search/social metadata for each page."""
    assert page.canonical == [canonical], f"Incorrect or duplicate canonical URL: {canonical}"
    for key in ("description", "og:title", "og:description", "twitter:title", "twitter:description"):
        assert page.metadata.get(key), f"Missing {key}: {canonical}"
    assert page.metadata["og:url"] == canonical
    assert page.metadata["twitter:card"] == "summary_large_image"
    assert page.metadata["og:image"].startswith("https://hdijkema.github.io/pgadmin-jr/")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path, help="Generated site directory containing index.html")
    check(parser.parse_args().site)
