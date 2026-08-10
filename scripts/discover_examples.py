#!/usr/bin/env python3
"""Discover first-party ESP-IDF projects without choosing an IDF release."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


IDF_ROOT = Path("examples/esp-idf")
TARGET_RE = re.compile(r"^[a-z0-9][a-z0-9_-]*$")
DECLARED_TARGET_RE = re.compile(
    r"^\s*set\s*\(\s*IDF_TARGET\s+[\"']?([a-z0-9_-]+)[\"']?",
    re.IGNORECASE | re.MULTILINE,
)


class DiscoveryError(ValueError):
    """Raised when repository discovery inputs are inconsistent."""


def normalize(value: str) -> str:
    return value.replace("\\", "/").strip("/")


def validate_target(value: str) -> str:
    target = value.strip().lower()
    if not TARGET_RE.fullmatch(target):
        raise DiscoveryError(
            "target must contain only lowercase letters, digits, underscores, or hyphens"
        )
    return target


def selector_matches(entry: dict[str, str], selector: str) -> bool:
    if not selector or selector == "all":
        return True

    selector = normalize(selector)
    path = normalize(entry["path"])
    return (
        selector == entry["name"]
        or selector == path
        or path.startswith(selector + "/")
        or selector in path.split("/")
    )


def declared_targets(project: Path) -> set[str]:
    cmake = project / "CMakeLists.txt"
    text = cmake.read_text(encoding="utf-8", errors="replace")
    return {match.lower() for match in DECLARED_TARGET_RE.findall(text)}


def discover_esp_idf(repo: Path, target: str) -> list[dict[str, str]]:
    root = repo / IDF_ROOT
    if not root.exists():
        return []
    if not root.is_dir():
        raise DiscoveryError(f"ESP-IDF example root is not a directory: {IDF_ROOT.as_posix()}")

    entries: list[dict[str, str]] = []
    for project in sorted(root.iterdir(), key=lambda item: item.name.casefold()):
        if not project.is_dir() or not (project / "CMakeLists.txt").is_file():
            continue

        declared = declared_targets(project)
        mismatches = sorted(item for item in declared if item != target)
        if mismatches:
            relative = project.relative_to(repo).as_posix()
            raise DiscoveryError(
                f"{relative}/CMakeLists.txt declares {', '.join(mismatches)}, "
                f"but --target is {target}"
            )

        entries.append(
            {
                "name": project.name,
                "path": project.relative_to(repo).as_posix(),
                "target": target,
            }
        )
    return entries


def write_github_output(path: Path, projects: list[dict[str, str]], target: str) -> None:
    projects_json = json.dumps(projects, separators=(",", ":"))
    with path.open("a", encoding="utf-8", newline="\n") as output:
        output.write(f"projects={projects_json}\n")
        output.write(f"count={len(projects)}\n")
        output.write(f"target={target}\n")


def run(args: argparse.Namespace) -> dict[str, object]:
    repo = Path(args.repo).resolve()
    if not repo.is_dir():
        raise DiscoveryError(f"repository directory does not exist: {args.repo}")

    target = validate_target(args.target)
    selector = normalize(args.selector)
    projects = [
        entry
        for entry in discover_esp_idf(repo, target)
        if selector_matches(entry, selector)
    ]
    if args.require_match and not projects:
        raise DiscoveryError(f"selector matched no ESP-IDF projects: {args.selector}")

    if args.github_output:
        write_github_output(Path(args.github_output), projects, target)
    return {"target": target, "projects": projects}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=".", help="Repository root. Defaults to the current directory.")
    parser.add_argument(
        "--target",
        required=True,
        help="Expected ESP-IDF target, for example esp32s31.",
    )
    parser.add_argument(
        "--selector",
        default="all",
        help="all, an example directory name, or a repo-relative example path.",
    )
    parser.add_argument(
        "--require-match",
        action="store_true",
        help="Fail when the selector matches no projects.",
    )
    parser.add_argument("--github-output", help="Append projects, count, and target outputs to this file.")
    args = parser.parse_args()

    try:
        payload = run(args)
    except (DiscoveryError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if not args.github_output:
        print(json.dumps(payload, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
