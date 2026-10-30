#!/usr/bin/env python3
"""Validate a tagged build, upload a draft, verify assets, then publish it."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from releases.release_contract import ROOT, config, validate_tag, SHA_RE
from releases.validate_release import inspect_archive, validate_set


class GitHub:
    def __init__(self, repository: str):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
            raise ValueError("Expected OWNER/REPOSITORY")
        self.base = f"/repos/{repository}"

    def call(self, route: str, method: str = "GET", payload=None, binary: bytes | None = None, public=False):
        host = "https://uploads.github.com" if binary is not None else "https://api.github.com"
        headers = {"Accept": "application/vnd.github+json", "User-Agent": "waveshare-firmware-release",
                   "X-GitHub-Api-Version": "2022-11-28"}
        if not public:
            headers["Authorization"] = "Bearer " + os.environ["GH_TOKEN"]
        data = binary if binary is not None else (json.dumps(payload).encode() if payload is not None else None)
        headers["Content-Type"] = "application/zip" if binary is not None else "application/json"
        request = urllib.request.Request(host + self.base + route, data=data, headers=headers, method=method)
        with urllib.request.urlopen(request, timeout=60) as response:
            content = response.read()
            return json.loads(content) if content else None

    def assets(self, release_id: int, public=False) -> list[dict]:
        result = []
        for page in range(1, 100):
            batch = self.call(f"/releases/{release_id}/assets?per_page=100&page={page}", public=public)
            result.extend(batch)
            if len(batch) < 100:
                return result
        raise ValueError("Too many release asset pages")


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def verify_tag(api: GitHub, tag: str, commit: str) -> None:
    validate_tag(tag)
    if not SHA_RE.fullmatch(commit) or git("rev-parse", "HEAD") != commit:
        raise ValueError("Event source commit differs from checkout")
    ref = f"refs/tags/{tag}"
    if git("cat-file", "-t", ref) != "tag" or git("rev-parse", ref + "^{commit}") != commit:
        raise ValueError("Release requires an annotated tag pointing to the checkout")
    remote = api.call("/git/ref/tags/" + urllib.parse.quote(tag, safe=""))
    if remote["object"]["type"] != "tag" or remote["object"]["sha"] != git("rev-parse", ref):
        raise ValueError("Remote tag changed")
    tagged = api.call("/git/tags/" + remote["object"]["sha"])
    if tagged["object"]["type"] != "commit" or tagged["object"]["sha"] != commit:
        raise ValueError("Remote tag commit differs from the build")
    metadata = api.call("")
    branch = urllib.parse.quote(metadata["default_branch"], safe="")
    comparison = api.call(f"/compare/{commit}...{branch}")
    if comparison["status"] not in {"ahead", "identical"}:
        raise ValueError("Release commit is not on the default branch")


def verify_assets(assets: list[dict], expected: dict[str, tuple[int, str]]) -> None:
    if len(assets) != len(expected) or {a["name"] for a in assets} != set(expected):
        raise ValueError("Release assets differ from the exact validated matrix")
    for asset in assets:
        if (asset["state"] != "uploaded"
                or (asset["size"], asset.get("digest")) != expected[asset["name"]]):
            raise ValueError("Remote asset size, upload state or SHA256 mismatch")


def publish(api: GitHub, directory: Path, tag: str, commit: str) -> dict:
    verify_tag(api, tag, commit)
    files = validate_set(directory, commit, tag)
    cfg = config()
    board = inspect_archive(files[0])["resolved_dependencies"]["waveshare/waveshare_boards"]["version"]
    body = (f"Source: `{commit}`\n\n"
            f"{len(files)} ESP-IDF examples for ESP32-S31-Touch-LCD-4.3C. "
            "Each ZIP includes firmware segments, a combined image at offset 0x0, "
            "a manifest, checksums and Bash/Windows flash helpers.\n\n"
            f"ESP-IDF: `{cfg['framework_version']}` (`{cfg['framework_commit']}`).\n"
            f"Managed board package: `waveshare/waveshare_boards {board}`.\n\n"
            "ESP32-S31 support uses a pinned preview toolchain. "
            "Compilation and archive checks do not establish physical-board functionality.\n\n"
            "Download the ZIP named for your example; GitHub's Source code archives contain source only.")
    try:
        release = api.call(f"/releases/tags/{tag}")
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise
        release = api.call("/releases", "POST", {
            "tag_name": tag, "target_commitish": commit, "name": tag, "body": body,
            "draft": True, "prerelease": cfg["experimental"]})
    else:
        if not release["draft"]:
            raise ValueError("Public Release already exists; issue a new patch version instead")
        # Only an unpublished draft may be retried. A public version is immutable.
        for asset in api.assets(release["id"]):
            api.call(f"/releases/assets/{asset['id']}", "DELETE")
        api.call(f"/releases/{release['id']}", "PATCH", {
            "body": body, "name": tag, "prerelease": cfg["experimental"]})
    release_id = release["id"]
    payloads = {p.name: p.read_bytes() for p in files}
    payloads["SHA256SUMS"] = "".join(
        f"{hashlib.sha256(payloads[name]).hexdigest()}  {name}\n" for name in sorted(payloads)).encode()
    expected = {name: (len(data), "sha256:" + hashlib.sha256(data).hexdigest())
                for name, data in payloads.items()}
    for name, payload in payloads.items():
        api.call(f"/releases/{release_id}/assets?name=" + urllib.parse.quote(name, safe=""),
                 "POST", binary=payload)
    verify_assets(api.assets(release_id), expected)
    verify_tag(api, tag, commit)
    if not api.call(f"/releases/{release_id}")["draft"]:
        raise ValueError("Release was published outside the workflow")
    published = api.call(f"/releases/{release_id}", "PATCH", {
        "draft": False, "prerelease": cfg["experimental"],
        "make_latest": "false" if cfg["experimental"] else "true"})
    if published["draft"] or published["tag_name"] != tag or published["prerelease"] != cfg["experimental"]:
        raise ValueError("Published Release metadata mismatch")
    # Verify public metadata and a fresh asset download independently of upload.
    public = api.call(f"/releases/tags/{tag}", public=True)
    if public["draft"] or public["tag_name"] != tag or public["prerelease"] != cfg["experimental"]:
        raise ValueError("Public Release metadata mismatch")
    assets = api.assets(release_id, public=True)
    verify_assets(assets, expected)
    representative = next(a for a in sorted(assets, key=lambda a: a["name"]) if a["name"].endswith(".zip"))
    url = representative["browser_download_url"]
    if not url.startswith("https://github.com/"):
        raise ValueError("Unexpected public asset URL")
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read(128 * 1024 * 1024 + 1)
    if (len(data), "sha256:" + hashlib.sha256(data).hexdigest()) != expected[representative["name"]]:
        raise ValueError("Fresh public download differs from uploaded firmware")
    with tempfile.TemporaryDirectory() as temp:
        downloaded = Path(temp) / representative["name"]
        downloaded.write_bytes(data)
        inspect_archive(downloaded)
    return public


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--directory", type=Path, default=Path("release-artifacts"))
    args = parser.parse_args()
    try:
        if os.environ["GITHUB_REF_TYPE"] != "tag":
            raise ValueError("Release must run on a version tag")
        api = GitHub(os.environ["GITHUB_REPOSITORY"])
        tag, commit = os.environ["GITHUB_REF_NAME"], os.environ["GITHUB_SHA"]
        if args.verify_only:
            verify_tag(api, tag, commit)
            print(f"Verified annotated tag {tag} at {commit}")
        else:
            result = publish(api, args.directory, tag, commit)
            print(f"Published and verified: {result['html_url']}")
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"error: {exc}\n")
