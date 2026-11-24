"""Inspect the real wrapper output using only Python's standard library."""
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

with tempfile.TemporaryDirectory(prefix="pgjr-xlsx-") as directory:
    file = Path(directory) / "exact.xlsx"
    subprocess.run([sys.argv[1], "--xlsx-fixture", str(file)], check=True, timeout=20)
    with zipfile.ZipFile(file) as book:
        sheet = ET.fromstring(book.read("xl/worksheets/sheet1.xml"))
        ns = {"s": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}
        cells = sheet.findall(".//s:sheetData/s:row/s:c", ns)
        values = ["".join(cell.itertext()) for cell in cells]
        for expected in ["9007199254740993.125", "=1+2", "café ☃", "NULL"]:
            assert expected in values, (expected, values)
        assert not sheet.findall(".//s:f", ns), "Server text must never become an Excel formula"
        for cell in cells:
            if "".join(cell.itertext()) in ["9007199254740993.125", "=1+2"]:
                assert cell.attrib["t"] == "inlineStr", cell.attrib
        assert sheet.find("s:cols/s:col", ns) is not None, "Column widths missing"
print("XLSX exact values, Unicode, string safety and column widths passed")
