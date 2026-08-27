#!/usr/bin/env python3
"""Validate public package identity and immutable dependency pins."""

import json
import re
from pathlib import Path


REPO_DIR = Path(__file__).resolve().parent.parent
EXPECTED_VERSION = "2.1.0"
EXPECTED_URL = "https://github.com/Cybergrany/ModbusRTUMaster"
EXPECTED_DEPENDENCIES = {
    "ModbusADU": (
        "https://github.com/CMB27/ModbusADU.git"
        "#7cb0e24f0abe86bc83e114325d75fe7a7d878562"
    ),
    "ModbusRTUComm": (
        "https://github.com/Cybergrany/ModbusRTUComm.git"
        "#e5997f8a6f825976c8f0c9212cec09f653743cf2"
    ),
}


def read_properties(path: Path):
    properties = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise AssertionError(f"invalid library.properties line: {raw_line}")
        key, value = line.split("=", 1)
        properties[key.strip()] = value.strip()
    return properties


library_json = json.loads((REPO_DIR / "library.json").read_text(encoding="utf-8"))
properties = read_properties(REPO_DIR / "library.properties")

versions = {
    "library.json": library_json.get("version"),
    "library.properties": properties.get("version"),
}
if any(version != EXPECTED_VERSION for version in versions.values()):
    raise AssertionError(
        f"package version must be {EXPECTED_VERSION} everywhere: {versions}"
    )

json_url = library_json.get("repository", {}).get("url", "")
if json_url.endswith(".git"):
    json_url = json_url[:-4]
if json_url != EXPECTED_URL or properties.get("url") != EXPECTED_URL:
    raise AssertionError("package manifests must identify the public repository")

if "depends" in properties:
    raise AssertionError(
        "library.properties cannot express immutable dependency revisions"
    )

dependencies = library_json.get("dependencies", {})
if dependencies != EXPECTED_DEPENDENCIES:
    raise AssertionError(f"dependency tuple changed: {dependencies}")
for dependency, pin in dependencies.items():
    if re.fullmatch(r"https://[^#]+\.git#[0-9a-f]{40}", pin) is None:
        raise AssertionError(
            f"library.json {dependency} must use an exact URL#40hex pin"
        )

print(
    f"Package contract: {EXPECTED_VERSION}, public repository, exact dependency pins"
)
