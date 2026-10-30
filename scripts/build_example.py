#!/usr/bin/env python3
"""Resolve the managed board, generate Board Manager code and build one example."""
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from releases.release_contract import config, projects


def build(selector: str, jobs: int = 4) -> Path:
    cfg = config()
    rows = projects(selector)
    if len(rows) != 1:
        raise ValueError("Select exactly one example")
    idf = Path(os.environ["IDF_PATH"])
    revision = subprocess.check_output(["git", "-C", str(idf), "rev-parse", "HEAD"], text=True).strip()
    if revision != cfg["framework_commit"]:
        raise ValueError("ESP-IDF commit differs from releases/config.json")
    if importlib.metadata.version("idf-component-manager") != cfg["component_manager_version"]:
        raise ValueError("Component Manager version differs from releases/config.json")
    project = ROOT / rows[0]["path"]
    env = dict(os.environ)
    env["IDF_COMPONENT_CONSTRAINTS"] = ";".join(
        f"{name}=={version}" for name, version in cfg["component_constraints"].items())
    command = [sys.executable, str(idf / "tools/idf.py"), "--preview", "-C", str(project)]
    # A first configure downloads the managed packages before their bmgr CLI
    # extension is available. The only permitted failure is missing board code.
    first = subprocess.run(command + [f"-DIDF_TARGET={cfg['target']}", "reconfigure"],
                           env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(first.stdout, end="", flush=True)
    board = project / "managed_components/waveshare__waveshare_boards/boards" / cfg["board"]
    if not (board / "board_info.yaml").is_file():
        raise ValueError(f"Published waveshare/waveshare_boards must include {cfg['board']}; "
                         "publish that component before running release builds")
    if first.returncode and not any(text in first.stdout for text in (
            "gen_bmgr_codes", "No board selected", "No board is selected")):
        raise subprocess.CalledProcessError(first.returncode, command)
    subprocess.run(command + ["bmgr", "-b", cfg["board"]], env=env, check=True)
    subprocess.run(command + ["-j", str(jobs), "build"], env=env, check=True)
    # The lock parser is supplied by the pinned Component Manager environment.
    import yaml
    lock = yaml.safe_load((project / "dependencies.lock").read_text())
    dependencies = {name: {k: str(data[k]) for k in ("version", "component_hash") if k in data}
                    for name, data in lock["dependencies"].items()
                    if data.get("source", {}).get("type") == "service"}
    if "waveshare/waveshare_boards" not in dependencies:
        raise ValueError("Board must come from the managed component registry")
    context = {
        "framework_commit": revision,
        "component_manager_version": cfg["component_manager_version"],
        "board": cfg["board"],
        "board_files": {p.relative_to(board).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in sorted(board.rglob("*")) if p.is_file()},
        "resolved_dependencies": dependencies,
    }
    output = project / "build/release-context.json"
    output.write_text(json.dumps(context, indent=2, sort_keys=True) + "\n")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("example")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    try:
        if args.jobs < 1:
            raise ValueError("--jobs must be positive")
        print(build(args.example, args.jobs))
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"error: {exc}\n")
