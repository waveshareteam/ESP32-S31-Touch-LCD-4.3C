#!/usr/bin/env python3
"""Package one CI build with the source and framework identities verified."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from releases.package_firmware import package
from releases.release_contract import ROOT, artifact_name, config, projects
from releases.validate_release import inspect_archive


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("example")
    parser.add_argument("--release-version", default="")
    args = parser.parse_args()
    rows = projects(args.example)
    if len(rows) != 1:
        raise ValueError("Select one example")
    if subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=all"], cwd=ROOT, text=True):
        raise ValueError("Refuse to package uncommitted source as a commit build")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    os.environ["SOURCE_DATE_EPOCH"] = subprocess.check_output(
        ["git", "show", "-s", "--format=%ct", "HEAD"], cwd=ROOT, text=True).strip()
    cfg = config()
    project = ROOT / rows[0]["path"]
    archive = package(argparse.Namespace(
        project=str(project), build_dir=str(project / "build"), output_dir=str(ROOT / "releases/dist"),
        target=cfg["target"], framework_version=cfg["framework_version"], git_sha=commit,
        baud="460800", name=artifact_name(project.name, commit, args.release_version),
        release_version=args.release_version, build_context=str(project / "build/release-context.json")))
    inspect_archive(archive)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as exc:
        sys.exit(f"error: {exc}")
