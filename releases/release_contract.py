#!/usr/bin/env python3
"""Shared configuration, matrix and version identity for firmware releases."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts.discover_examples import discover_esp_idf, selector_matches

TAG_RE = re.compile(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)")
SHA_RE = re.compile(r"[0-9a-f]{40}")


def config() -> dict:
    value = json.loads((ROOT / "releases/config.json").read_text())
    if (value["target"] != "esp32s31"
            or not SHA_RE.fullmatch(value["framework_commit"])):
        raise ValueError("Invalid release configuration")
    return value


def validate_tag(tag: str) -> str:
    if not TAG_RE.fullmatch(tag):
        raise ValueError("Use an annotated vMAJOR.MINOR.PATCH tag, e.g. v1.0.0")
    return tag


def projects(selector: str = "all") -> list[dict]:
    cfg = config()
    rows = discover_esp_idf(ROOT, cfg["target"])
    if len(rows) != cfg["expected_projects"]:
        raise ValueError("Project count differs from releases/config.json")
    selected = [r for r in rows if selector_matches(r, selector)]
    if not selected:
        raise ValueError(f"No matching example: {selector}")
    return selected


def artifact_name(project: str, commit: str, version: str = "") -> str:
    if not re.fullmatch(r"[0-9]{2}_[a-z0-9_]+", project) or not SHA_RE.fullmatch(commit):
        raise ValueError("Invalid project or source commit")
    if version:
        validate_tag(version)
    cfg = config()
    return (f"{project}-esp-idf-{cfg['framework_version']}-{cfg['target']}-"
            f"{version or 'dev'}-{commit[:12]}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selector", default="all")
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()
    cfg = config()
    values = {"matrix": json.dumps({"include": projects(args.selector)}, separators=(",", ":")),
              **{k: str(cfg[k]) for k in ("framework_commit", "framework_version", "component_manager_version")}}
    if args.github_output:
        with args.github_output.open("a") as out:
            for key, value in values.items():
                out.write(f"{key}={value}\n")
    else:
        print(values["matrix"])


if __name__ == "__main__":
    main()
