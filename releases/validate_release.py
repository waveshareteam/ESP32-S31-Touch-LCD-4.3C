#!/usr/bin/env python3
"""Independently validate firmware bytes and the exact selected release matrix."""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shlex
import stat
import sys
import tempfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from releases.package_firmware import write_flash_helpers
from releases.release_contract import artifact_name, config, projects, SHA_RE


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def safe_name(name: str) -> bool:
    return bool(re.fullmatch(r"[A-Za-z0-9_.\-/]+", name) and not name.startswith("/")
                and all(p not in ("", ".", "..") for p in name.split("/")))


def inspect_archive(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        names = [p.filename for p in infos]
        if len(names) != len({n.casefold() for n in names}):
            raise ValueError("Duplicate archive member")
        if sum(p.file_size for p in infos) > 128 * 1024 * 1024:
            raise ValueError("Expanded archive exceeds the size limit")
        for info in infos:
            if (not safe_name(info.filename) or info.is_dir() or info.flag_bits & 1
                    or stat.S_IFMT(info.external_attr >> 16) not in (0, stat.S_IFREG)):
                raise ValueError("Unsafe archive member")
        prefix = path.stem + "/"
        if any(not n.startswith(prefix) for n in names):
            raise ValueError("Archive root does not match its filename")
        data = {n[len(prefix):]: archive.read(n) for n in names}
    manifest = json.loads(data["manifest.json"])
    if manifest.get("schema") != 2 or manifest.get("framework") != "esp-idf":
        raise ValueError("Unsupported firmware archive schema")
    if manifest.get("target") != "esp32s31" or manifest.get("image_chip_id") != 32:
        raise ValueError("Wrong firmware target")
    if manifest["name"] != path.stem:
        raise ValueError("Manifest/archive identity mismatch")
    required = {"manifest.json", "SHA256SUMS", "README.md", "flash.sh", "flash.bat", "flash_args.txt"}
    combined_name = f"bin/{path.stem}.combined.bin"
    segments = manifest["segments"]
    segment_names = [entry["file"] for entry in segments]
    if len(segment_names) != len(set(segment_names)) or not segments:
        raise ValueError("Missing or duplicate firmware segments")
    if any(not re.fullmatch(r"bin/[A-Za-z0-9_.-]+\.bin", n) for n in segment_names):
        raise ValueError("Invalid firmware segment filename")
    if set(data) != required | set(segment_names) | {combined_name}:
        raise ValueError("Missing or unexpected archive files")
    sums = "".join(f"{sha(data[n])}  {n}\n" for n in sorted(set(data) - {"SHA256SUMS"}))
    if data["SHA256SUMS"].decode() != sums:
        raise ValueError("Checksum list mismatch")
    combined = data[combined_name]
    if (manifest["combined_bin"] != combined_name or len(manifest["files"]) != 1
            or manifest["files"][0]["file"] != combined_name
            or manifest["files"][0]["offset"] != "0x0"):
        raise ValueError("Invalid combined image metadata")
    for entry in [*segments, *manifest["files"]]:
        payload = data[entry["file"]]
        if not payload or len(payload) != entry["size"] or sha(payload) != entry["sha256"]:
            raise ValueError("Firmware segment size/hash mismatch")
    end = 0
    roles = Counter()
    for segment in segments:
        offset = int(segment["offset"], 0)
        payload = data[segment["file"]]
        if offset < end or offset + len(payload) > 32 * 1024 * 1024:
            raise ValueError("Overlapping or oversized firmware segments")
        if combined[end:offset] != b"\xff" * (offset - end) or combined[offset:offset+len(payload)] != payload:
            raise ValueError("Combined image differs from its segments or erased gaps")
        role = segment["role"]
        roles[role] += 1
        if role in {"app", "bootloader"}:
            if len(payload) < 24 or payload[0] != 0xe9 or int.from_bytes(payload[12:14], "little") != 32:
                raise ValueError("Firmware image header is not ESP32-S31")
        end = offset + len(payload)
    if any(roles[role] != 1 for role in ("app", "bootloader", "partition_table")) or len(combined) != end:
        raise ValueError("Incomplete firmware roles or padding past the last segment")
    # Regenerate helpers from a constrained command and compare them byte for byte.
    command = shlex.split(manifest["flash_command"])
    if command[:9] != ["python", "-m", "esptool", "--chip", "esp32s31", "--port", "<PORT>", "--baud", "460800"]:
        raise ValueError("Invalid flash command identity/transport")
    if command[9] != "--before" or command[10] not in {"default-reset", "no-reset", "usb-reset"}:
        raise ValueError("Invalid before reset mode")
    if command[11] != "--after" or command[12] not in {"hard-reset", "soft-reset", "no-reset", "no-reset-stub"}:
        raise ValueError("Invalid after reset mode")
    rest = command[13:]
    if rest and rest[0] == "--no-stub":
        rest = rest[1:]
    if rest[:1] != ["write-flash"] or rest[-2:] != ["0x0", combined_name]:
        raise ValueError("Invalid flash destination")
    settings = rest[1:-2]
    if len(settings) != 6 or len(set(settings[::2])) != 3:
        raise ValueError("Invalid flash options")
    allowed = {"--flash-mode": {"dio", "qio", "dout", "qout", "keep"},
               "--flash-freq": {"20m", "26m", "40m", "80m", "120m", "keep"},
               "--flash-size": {"1MB", "2MB", "4MB", "8MB", "16MB", "32MB", "keep", "detect"}}
    if any(key not in allowed or value not in allowed[key] for key, value in zip(settings[::2], settings[1::2])):
        raise ValueError("Unsupported flash settings")
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        write_flash_helpers(directory, ["$PORT" if c == "<PORT>" else c for c in command],
                            path.stem, "esp32s31")
        for name in ("flash.sh", "flash.bat", "flash_args.txt"):
            if (directory / name).read_bytes() != data[name]:
                raise ValueError("Flash helper differs from validated arguments")
    return manifest


def validate_set(directory: Path, commit: str, version: str = "", selector: str = "all") -> list[Path]:
    if not SHA_RE.fullmatch(commit):
        raise ValueError("Expected a full source commit SHA")
    rows = projects(selector)
    expected = {artifact_name(r["name"], commit, version) + ".zip": r for r in rows}
    members = [p for p in directory.rglob("*") if p.is_file() or p.is_symlink()]
    if any(p.is_symlink() or p.suffix != ".zip" for p in members):
        raise ValueError("Unexpected file or symlink in the firmware artifact set")
    if len(members) != len(expected) or {p.name for p in members} != set(expected):
        raise ValueError(f"Expected exactly {len(expected)} firmware archives")
    cfg = config()
    dependencies = {}
    board_files = None
    for path in sorted(members):
        manifest = inspect_archive(path)
        row = expected[path.name]
        identity = {"git_sha": commit, "project_path": row["path"], "target": row["target"],
                    "release_version": version, **{k: cfg[k] for k in (
                        "framework_version", "framework_commit", "component_manager_version", "board")}}
        if any(manifest.get(k) != v for k, v in identity.items()):
            raise ValueError("Stale commit, wrong project/version or framework mismatch")
        resolved = manifest["resolved_dependencies"]
        if "waveshare/waveshare_boards" not in resolved or not manifest["board_files"]:
            raise ValueError("Missing managed board provenance")
        for name, metadata in resolved.items():
            if name in dependencies and dependencies[name] != metadata:
                raise ValueError(f"Dependency changed between matrix jobs: {name}")
            dependencies[name] = metadata
        for name, pinned in cfg["component_constraints"].items():
            if name in resolved and resolved[name]["version"] != pinned:
                raise ValueError("Resolved dependency differs from the release pin")
        if board_files is not None and manifest["board_files"] != board_files:
            raise ValueError("Board definition differs between matrix jobs")
        board_files = manifest["board_files"]
    return sorted(members)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--version", default="")
    parser.add_argument("--selector", default="all")
    args = parser.parse_args()
    try:
        print(f"Validated {len(validate_set(args.directory, args.commit, args.version, args.selector))} firmware packages")
    except (ValueError, OSError, KeyError, IndexError, zipfile.BadZipFile) as exc:
        parser.exit(1, f"error: {exc}\n")
