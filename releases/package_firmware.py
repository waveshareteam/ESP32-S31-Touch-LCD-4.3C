#!/usr/bin/env python3
"""Create a flashable archive from an ESP-IDF build directory."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import stat
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

DEFAULT_BAUD = "460800"
TARGET_RE = re.compile(r"^[a-z0-9][a-z0-9_-]*$")


def slugify(value: str) -> str:
    value = value.strip().replace("\\", "/")
    value = re.sub(r"[^A-Za-z0-9._-]+", "-", value)
    value = re.sub(r"-+", "-", value).strip("-.")
    return value or "firmware"


def validate_target(value: str) -> str:
    target = value.strip().lower()
    if not TARGET_RE.fullmatch(target):
        raise ValueError(
            "target must contain only lowercase letters, digits, underscores, or hyphens"
        )
    return target


def parse_offset(value: str) -> int:
    offset = int(value, 0)
    if offset < 0:
        raise ValueError(f"flash offset must not be negative: {value}")
    return offset


def format_offset(value: str) -> str:
    return f"0x{parse_offset(value):x}"


def safe_project_path(project: Path, repo: Path) -> str:
    try:
        return project.resolve().relative_to(repo.resolve()).as_posix()
    except ValueError:
        return project.name


def quote_shell(value: str) -> str:
    return "'" + value.replace("'", "'\"'\"'") + "'"


def quote_batch(value: str) -> str:
    return '"' + value.replace('"', '""') + '"'


def write_text(path: Path, content: str, executable: bool = False) -> None:
    path.write_text(content, encoding="utf-8", newline="\n")
    if executable:
        path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def resolve_flash_file(build_dir: Path, value: str) -> Path:
    source = Path(value)
    if not source.is_absolute():
        source = build_dir / source
    if not source.resolve().is_relative_to(build_dir.resolve()):
        raise ValueError("firmware segment must be inside the build directory")
    if not source.is_file():
        raise FileNotFoundError(f"missing firmware file: {source}")
    return source


def copy_file(source: Path, firmware_dir: Path, offset: str) -> str:
    destination_name = slugify(f"{offset.lower()}_{source.name}")
    destination = firmware_dir / destination_name
    shutil.copy2(source, destination)
    return f"bin/{destination_name}"


def load_esp_idf_entries(
    build_dir: Path, firmware_dir: Path
) -> tuple[list[dict[str, str]], dict[str, object]]:
    flasher_args_path = build_dir / "flasher_args.json"
    if not flasher_args_path.is_file():
        raise FileNotFoundError(f"missing ESP-IDF flasher args: {flasher_args_path}")

    data = json.loads(flasher_args_path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError(f"invalid ESP-IDF flasher args object: {flasher_args_path}")

    flash_files = data.get("flash_files")
    if not isinstance(flash_files, dict) or not flash_files:
        raise ValueError(f"no flash_files found in {flasher_args_path}")

    entries: list[dict[str, str]] = []
    for raw_offset, raw_source in sorted(
        flash_files.items(), key=lambda item: parse_offset(str(item[0]))
    ):
        if not isinstance(raw_source, str) or not raw_source:
            raise ValueError(f"invalid flash file for offset {raw_offset!r}")
        offset = format_offset(str(raw_offset))
        source = resolve_flash_file(build_dir, raw_source)
        copied = copy_file(source, firmware_dir, offset)
        source_label = Path(raw_source).name if Path(raw_source).is_absolute() else Path(raw_source).as_posix()
        entries.append({"offset": offset, "file": copied, "source": source_label})

    return entries, data


def write_padding(output, size: int) -> None:
    chunk = b"\xff" * 4096
    while size:
        count = min(size, len(chunk))
        output.write(chunk[:count])
        size -= count


def create_combined_bin(
    package_dir: Path,
    firmware_dir: Path,
    artifact_name: str,
    entries: list[dict[str, str]],
) -> str:
    segments: list[tuple[int, Path, dict[str, str]]] = []
    for entry in entries:
        source = package_dir / entry["file"]
        segments.append((parse_offset(entry["offset"]), source, entry))

    combined_name = slugify(f"{artifact_name}.combined.bin")
    combined_path = firmware_dir / combined_name
    position = 0
    with combined_path.open("wb") as output:
        for offset, source, entry in sorted(segments, key=lambda item: item[0]):
            if offset < position:
                raise ValueError(
                    f"firmware segment {entry['file']} at {entry['offset']} overlaps a previous segment"
                )
            if offset + source.stat().st_size > 32 * 1024 * 1024:
                raise ValueError("firmware exceeds the supported flash size")
            write_padding(output, offset - position)
            with source.open("rb") as segment:
                shutil.copyfileobj(segment, output)
            position = offset + source.stat().st_size
    return f"bin/{combined_name}"


def build_esptool_command(
    target: str,
    baud: str,
    before: str,
    after: str,
    write_flash_args: list[str],
    combined_file: str,
    stub: bool = True,
) -> list[str]:
    return [
        "python",
        "-m",
        "esptool",
        "--chip",
        target,
        "--port",
        "$PORT",
        "--baud",
        baud,
        "--before",
        before,
        "--after",
        after,
        *([] if stub else ["--no-stub"]),
        "write-flash",
        *write_flash_args,
        "0x0",
        combined_file,
    ]


def shell_command(parts: Iterable[str]) -> str:
    return " ".join('"$PORT"' if part == "$PORT" else quote_shell(part) for part in parts)


def batch_command(parts: Iterable[str]) -> str:
    return " ".join('"%PORT%"' if part == "$PORT" else quote_batch(part) for part in parts)


def write_flash_helpers(
    package_dir: Path, command: list[str], artifact_name: str, target: str
) -> None:
    shell = f"""#!/usr/bin/env sh
set -eu
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 /dev/ttyUSB0"
    exit 2
fi
PORT="$1"
cd "$(dirname "$0")"
if command -v python >/dev/null 2>&1 && python -c 'import esptool' >/dev/null 2>&1; then
    PYTHON=python
elif command -v python3 >/dev/null 2>&1 && python3 -c 'import esptool' >/dev/null 2>&1; then
    PYTHON=python3
else
    echo "Install an ESP32-S31-capable esptool: python3 -m pip install esptool" >&2
    exit 1
fi
exec "$PYTHON" {shell_command(command[1:])}
"""
    batch = f"""@echo off
setlocal
set "PORT=%~1"
if not defined PORT (
  echo Usage: flash.bat COMx
  exit /b 2
)
if not "%~2"=="" exit /b 2
cd /d "%~dp0"
python -c "import esptool" >nul 2>&1
if not errorlevel 1 goto use_python
py -3 -c "import esptool" >nul 2>&1
if not errorlevel 1 goto use_py
echo Install an ESP32-S31-capable esptool: py -3 -m pip install esptool
exit /b 1
:use_python
python {batch_command(command[1:])}
exit /b %errorlevel%
:use_py
py -3 {batch_command(command[1:])}
exit /b %errorlevel%
"""
    args_text = " ".join("<PORT>" if part == "$PORT" else part for part in command) + "\n"
    readme = f"""# {artifact_name}

Target: `{target}`

Install esptool if needed:

```bash
python -m pip install esptool
```

Flash from this directory:

```bash
./flash.sh /dev/ttyUSB0
```

On Windows:

```bat
flash.bat COMx
```
"""
    write_text(package_dir / "flash.sh", shell, executable=True)
    write_text(package_dir / "flash.bat", batch)
    write_text(package_dir / "flash_args.txt", args_text)
    write_text(package_dir / "README.md", readme)


def create_zip(package_dir: Path, zip_path: Path, timestamp: datetime) -> None:
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(package_dir.rglob("*"), key=lambda item: item.as_posix()):
            if path.is_file():
                stamp = timestamp.replace(year=max(timestamp.year, 1980)).timetuple()[:6]
                info = zipfile.ZipInfo(path.relative_to(package_dir.parent).as_posix(), stamp)
                info.create_system = 3
                info.external_attr = (0o100755 if path.name == "flash.sh" else 0o100644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, path.read_bytes())


def package(args: argparse.Namespace) -> Path:
    repo = Path.cwd().resolve()
    project = Path(args.project).resolve()
    build_dir = Path(args.build_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    target = validate_target(args.target)

    if not project.is_dir():
        raise FileNotFoundError(f"project directory does not exist: {args.project}")
    if not build_dir.is_dir():
        raise FileNotFoundError(f"build directory does not exist: {args.build_dir}")
    if not args.framework_version.strip():
        raise ValueError("framework version must not be empty")
    if not args.baud.isdigit() or int(args.baud) <= 0:
        raise ValueError("baud must be a positive integer")

    git_suffix = f"-{args.git_sha[:7]}" if args.git_sha else ""
    default_name = (
        f"{project.name}-esp-idf-{args.framework_version}-{target}{git_suffix}"
    )
    artifact_name = slugify(args.name or default_name)
    package_dir = (output_dir / artifact_name).resolve()
    if package_dir.parent != output_dir:
        raise ValueError("package directory must be a direct child of output directory")
    firmware_dir = package_dir / "bin"

    output_dir.mkdir(parents=True, exist_ok=True)
    if package_dir.exists():
        shutil.rmtree(package_dir)
    firmware_dir.mkdir(parents=True)

    try:
        segments, flasher_args = load_esp_idf_entries(build_dir, firmware_dir)
        extra_args = flasher_args.get("extra_esptool_args", {})
        if not isinstance(extra_args, dict):
            raise ValueError("extra_esptool_args must be an object")

        declared_chip = extra_args.get("chip")
        if declared_chip and str(declared_chip).lower() != target:
            raise ValueError(
                f"flasher_args.json declares chip {declared_chip}, but --target is {target}"
            )

        before = str(extra_args.get("before", "default_reset")).replace("_", "-")
        after = str(extra_args.get("after", "hard_reset")).replace("_", "-")
        raw_write_args = flasher_args.get("write_flash_args", [])
        if not isinstance(raw_write_args, list):
            raise ValueError("write_flash_args must be a list")
        write_flash_args = [str(item).replace("_", "-") if str(item).startswith("--") else str(item)
                            for item in raw_write_args]

        combined_file = create_combined_bin(
            package_dir, firmware_dir, artifact_name, segments
        )
        command = build_esptool_command(
            target,
            args.baud,
            before,
            after,
            write_flash_args,
            combined_file,
            bool(extra_args.get("stub", True)),
        )
        timestamp = (datetime.fromtimestamp(int(os.environ["SOURCE_DATE_EPOCH"]), timezone.utc)
                     if "SOURCE_DATE_EPOCH" in os.environ else datetime.now(timezone.utc))
        timestamp = timestamp.replace(microsecond=0)
        manifest = {
            "name": artifact_name,
            "framework": "esp-idf",
            "framework_version": args.framework_version,
            "target": target,
            "project_path": safe_project_path(project, repo),
            "git_sha": args.git_sha,
            "timestamp_utc": timestamp.isoformat().replace("+00:00", "Z"),
            "baud": args.baud,
            "combined_bin": combined_file,
            "files": [
                {
                    "offset": "0x0",
                    "file": combined_file,
                    "source": "combined firmware image",
                }
            ],
            "segments": segments,
            "flash_command": " ".join(
                "<PORT>" if item == "$PORT" else item for item in command
            ),
        }
        context_path = getattr(args, "build_context", None)
        if context_path:
            from releases.release_contract import config
            cfg = config()
            context = json.loads(Path(context_path).read_text())
            description = json.loads((build_dir / "project_description.json").read_text())
            if description["target"] != target or description["project_name"] != project.name:
                raise ValueError("Build description target/project mismatch")
            for key in ("framework_commit", "component_manager_version", "board"):
                if context[key] != cfg[key]:
                    raise ValueError(f"Build context mismatch: {key}")
            if args.framework_version != cfg["framework_version"]:
                raise ValueError("Framework version differs from release configuration")
            roles = {str(flasher_args[k]["file"]): role for k, role in (
                ("bootloader", "bootloader"), ("app", "app"), ("partition-table", "partition_table"))}
            for entry in segments:
                entry["role"] = roles.get(entry["source"], "data")
            manifest.update(context)
            manifest.update(schema=2, image_chip_id=32,
                            release_version=getattr(args, "release_version", ""))
        for entry in [*segments, *manifest["files"]]:
            data = (package_dir / entry["file"]).read_bytes()
            entry.update(size=len(data), sha256=hashlib.sha256(data).hexdigest())
        write_text(package_dir / "manifest.json", json.dumps(manifest, indent=2) + "\n")
        write_flash_helpers(package_dir, command, artifact_name, target)
        if context_path:
            readme_path = package_dir / "README.md"
            write_text(readme_path, readme_path.read_text() +
                       f"\nSource: `{args.git_sha}`\n\nFramework: `{args.framework_version}` "
                       f"(`{manifest['framework_commit']}`).\n\n"
                       f"Product version: `{manifest['release_version'] or 'development build'}`.\n\n"
                       "Verify SHA256SUMS before flashing. See manifest.json for exact "
                       "registry dependency versions and board-definition hashes.\n")
        sums = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(package_dir).as_posix()}\n"
                       for p in sorted(package_dir.rglob("*")) if p.is_file())
        write_text(package_dir / "SHA256SUMS", sums)

        zip_path = output_dir / f"{artifact_name}.zip"
        create_zip(package_dir, zip_path, timestamp)
    except Exception:
        if package_dir.exists():
            shutil.rmtree(package_dir)
        raise

    print(zip_path.as_posix())
    return zip_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", required=True, help="Repository-relative ESP-IDF project path.")
    parser.add_argument("--build-dir", required=True, help="ESP-IDF build output directory.")
    parser.add_argument("--output-dir", default="releases/dist")
    parser.add_argument("--name", help="Firmware archive name.")
    parser.add_argument("--framework-version", required=True, help="Exact ESP-IDF tag or commit.")
    parser.add_argument("--target", required=True, help="ESP-IDF target used for the build.")
    parser.add_argument("--git-sha", default="")
    parser.add_argument("--baud", default=DEFAULT_BAUD)
    parser.add_argument("--build-context", help="release-context.json produced by scripts/build_example.py")
    parser.add_argument("--release-version", default="", help="Release tag, e.g. v1.0.0")
    args = parser.parse_args()

    try:
        package(args)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
