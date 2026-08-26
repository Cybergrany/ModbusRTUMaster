#!/usr/bin/env python3
"""Fail closed when package metadata loses its OGM prerelease identity."""

import json
import re
from pathlib import Path


REPO_DIR = Path(__file__).resolve().parent.parent


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
fork_lock = json.loads(
    (REPO_DIR / "ogm-fork-lock.json").read_text(encoding="utf-8")
)
properties = read_properties(REPO_DIR / "library.properties")

expected_version = "2.0.0-ogm.1"
versions = {
    "library.json": library_json.get("version"),
    "library.properties": properties.get("version"),
    "ogm-fork-lock.json": fork_lock.get("package_version"),
}
if any(version != expected_version for version in versions.values()):
    raise AssertionError(
        f"OGM package identity must be {expected_version} everywhere: {versions}"
    )

expected_url = "https://github.com/Cybergrany/ModbusRTUMaster"
json_url = library_json.get("repository", {}).get("url", "")
if json_url.endswith(".git"):
    json_url = json_url[:-4]
if json_url != expected_url or properties.get("url") != expected_url:
    raise AssertionError("package manifests must identify the Cybergrany fork")

if "depends" in properties:
    raise AssertionError(
        "library.properties cannot express immutable dependency revisions"
    )

manifest_dependencies = library_json.get("dependencies", {})
locked_dependencies = fork_lock.get("dependencies", {})
for dependency in ("ModbusADU", "ModbusRTUComm"):
    manifest_pin = manifest_dependencies.get(dependency, "")
    match = re.fullmatch(r"(https://[^#]+\.git)#([0-9a-f]{40})", manifest_pin)
    if match is None:
        raise AssertionError(
            f"library.json {dependency} must use an exact URL#40hex pin"
        )
    manifest_url, manifest_commit = match.groups()
    locked = locked_dependencies.get(dependency, {})
    if manifest_url != locked.get("url") or manifest_commit != locked.get("commit"):
        raise AssertionError(
            f"library.json and ogm-fork-lock.json disagree for {dependency}"
        )

print(f"Package contract: {expected_version}, Cybergrany fork, exact dependency pins")
