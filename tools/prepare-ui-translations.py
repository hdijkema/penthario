#!/usr/bin/env python3
"""Build the FLTK catalog from existing translations and the new UI supplement.

This is a maintainer command, not a build or runtime dependency. Unknown text
retains its English source; PostgreSQL messages keep the server's language.
"""
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
english = {}
german = {}
for locale, target in (("en", english), ("de", german)):
    for message in ET.parse(root / f"translations/pgadmin_{locale}.ts").iter("message"):
        source = message.findtext("source")
        translated = message.find("translation")
        if source and translated is not None and translated.get("type") != "unfinished" and translated.text:
            target[source] = translated.text
used = set()
for path in (root / "src/fltk").glob("*.cpp"):
    for raw in re.findall(r'"(?:[^"\\]|\\.)*"', path.read_text(encoding="utf-8")):
        try:
            used.add(json.loads(raw))
        except (SyntaxError, ValueError):
            pass
catalog = {}
for dutch, text in english.items():
    if text in used:
        catalog[text] = {"nl": dutch, "de": german.get(dutch, text)}
catalog.update(json.loads((root / "translations/fltk-extra.json").read_text(encoding="utf-8")))
(root / "translations/fltk-ui.json").write_text(json.dumps(catalog, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"Generated {len(catalog)} FLTK translations")
