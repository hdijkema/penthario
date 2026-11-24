#!/usr/bin/env python3
"""Validate the editable native JSON catalog, including duplicate source messages."""
from collections import Counter
from pathlib import Path
import json
import re


def unique_entries(pairs):
    """Reject duplicate keys before JSON decoding can silently overwrite them."""
    result = {}
    for key, value in pairs:
        assert key not in result, f"Duplicate catalog key: {key}"
        result[key] = value
    return result


root = Path(__file__).resolve().parents[1]
catalog = json.loads((root / "translations/fltk-ui.json").read_text(encoding="utf-8"),
                     object_pairs_hook=unique_entries)
assert catalog, "Empty translation catalog"
for source, translations in catalog.items():
    assert source and isinstance(translations, dict), source
    assert set(translations) == {"nl", "de"}, source
    for language, text in translations.items():
        assert isinstance(text, str) and text, f"{language}: {source}"
        assert Counter(re.findall(r"%L?\d+|%n", source)) == Counter(re.findall(r"%L?\d+|%n", text)), source
        assert source.count("\n") == text.count("\n"), source
print(f"{len(catalog)} messages: Dutch and German translations complete")
