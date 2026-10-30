"""Exercise release failure gates without a device or GitHub write access."""
import argparse
from contextlib import redirect_stdout
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import urllib.error
import warnings
import zipfile

from releases.package_firmware import package
from releases.release_contract import ROOT, artifact_name, config, projects, validate_tag
from releases.validate_release import inspect_archive, validate_set
from releases.publish_release import publish, verify_assets, verify_tag

COMMIT = "a" * 40
VERSION = "v1.2.3"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fixture(directory, row):
    cfg = config()
    build = directory / row["name"] / "build"
    (build / "bootloader").mkdir(parents=True)
    (build / "partition_table").mkdir()
    header = bytearray(24)
    header[0] = 0xe9
    header[12:14] = (32).to_bytes(2, "little")
    paths = {"0x2000": "bootloader/bootloader.bin", "0x8000": "partition_table/partition-table.bin",
             "0x10000": row["name"] + ".bin"}
    for name in paths.values():
        (build / name).write_bytes(bytes(header) + name.encode())
    flasher = {"flash_files": paths,
               "extra_esptool_args": {"chip": "esp32s31", "stub": False,
                                      "before": "default_reset", "after": "hard_reset"},
               "write_flash_args": ["--flash_mode", "dio", "--flash_size", "16MB", "--flash_freq", "80m"]}
    for key, offset in (("bootloader", "0x2000"), ("partition-table", "0x8000"), ("app", "0x10000")):
        flasher[key] = {"file": paths[offset], "offset": offset}
    (build / "flasher_args.json").write_text(json.dumps(flasher))
    (build / "project_description.json").write_text(json.dumps({"target": "esp32s31", "project_name": row["name"]}))
    context = {key: cfg[key] for key in ("framework_commit", "component_manager_version", "experimental", "board")}
    context.update(board_files={"board_info.yaml": "b" * 64}, resolved_dependencies={
        "waveshare/waveshare_boards": {"version": "0.2.0", "component_hash": "c" * 64},
        **{name: {"version": version, "component_hash": "d" * 64}
           for name, version in cfg["component_constraints"].items()}})
    (build / "release-context.json").write_text(json.dumps(context))
    args = argparse.Namespace(project=str(ROOT / row["path"]), build_dir=str(build),
                              output_dir=str(directory / "packages"), target="esp32s31",
                              framework_version=cfg["framework_version"], git_sha=COMMIT, baud="460800",
                              name=artifact_name(row["name"], COMMIT, VERSION), release_version=VERSION,
                              build_context=str(build / "release-context.json"))
    return args


def make_package(args):
    with patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "1735689600"}), redirect_stdout(io.StringIO()):
        return package(args)


def rewrite(archive, change, checksums=True):
    with zipfile.ZipFile(archive) as z:
        prefix = archive.stem + "/"
        data = {n[len(prefix):]: z.read(n) for n in z.namelist()}
    change(data)
    if checksums:
        data["SHA256SUMS"] = "".join(f"{digest(data[n])}  {n}\n" for n in sorted(data) if n != "SHA256SUMS").encode()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for name, payload in data.items():
            z.writestr(prefix + name, payload)


class FirmwareFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.args = fixture(self.root, projects()[0])
        self.archive = make_package(self.args)


class ArchiveTests(FirmwareFixture):
    def test_valid_and_reproducible(self):
        first = self.archive.read_bytes()
        manifest = inspect_archive(self.archive)
        self.assertEqual(manifest["git_sha"], COMMIT)
        self.assertIn("--no-stub", manifest["flash_command"])
        self.assertEqual(first, make_package(self.args).read_bytes())

    def test_wrong_declared_target(self):
        path = Path(self.args.build_dir) / "flasher_args.json"
        data = json.loads(path.read_text())
        data["extra_esptool_args"]["chip"] = "esp32s3"
        path.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            make_package(self.args)

    def test_overlapping_segments(self):
        path = Path(self.args.build_dir) / "flasher_args.json"
        data = json.loads(path.read_text())
        data["flash_files"]["0x2001"] = data["flash_files"].pop("0x10000")
        path.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            make_package(self.args)

    def test_segment_outside_build(self):
        path = Path(self.args.build_dir) / "flasher_args.json"
        data = json.loads(path.read_text())
        outside = self.root / "outside.bin"
        outside.write_bytes(b"outside")
        data["flash_files"]["0x10000"] = str(outside)
        path.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            make_package(self.args)

    def test_padding_cannot_be_hidden_by_updated_checksums(self):
        def change(data):
            manifest = json.loads(data["manifest.json"])
            name = manifest["combined_bin"]
            data[name] += b"\xff" * 32
            manifest["files"][0].update(size=len(data[name]), sha256=digest(data[name]))
            data["manifest.json"] = json.dumps(manifest).encode()
        rewrite(self.archive, change)
        with self.assertRaisesRegex(ValueError, "padding"):
            inspect_archive(self.archive)

    def test_wrong_image_chip_even_with_matching_hashes(self):
        def change(data):
            manifest = json.loads(data["manifest.json"])
            segment = next(s for s in manifest["segments"] if s["role"] == "app")
            payload = bytearray(data[segment["file"]]); payload[12:14] = (9).to_bytes(2, "little")
            data[segment["file"]] = bytes(payload)
            segment["sha256"] = digest(payload)
            combined = bytearray(data[manifest["combined_bin"]])
            offset = int(segment["offset"], 0)
            combined[offset:offset+len(payload)] = payload
            data[manifest["combined_bin"]] = bytes(combined)
            manifest["files"][0]["sha256"] = digest(combined)
            data["manifest.json"] = json.dumps(manifest).encode()
        rewrite(self.archive, change)
        with self.assertRaisesRegex(ValueError, "header"):
            inspect_archive(self.archive)

    def test_unexpected_member(self):
        rewrite(self.archive, lambda data: data.update({"extra.txt": b"extra"}))
        with self.assertRaises(ValueError):
            inspect_archive(self.archive)

    def test_duplicate_member(self):
        with warnings.catch_warnings(), zipfile.ZipFile(self.archive, "a") as z:
            warnings.simplefilter("ignore", UserWarning)
            z.writestr(self.archive.stem + "/README.md", b"duplicate")
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            inspect_archive(self.archive)

    def test_flash_helper_tampering(self):
        rewrite(self.archive, lambda data: data.update({"flash.sh": b"#!/bin/sh\nexit 0\n"}))
        with self.assertRaisesRegex(ValueError, "helper"):
            inspect_archive(self.archive)

    def test_shell_fallback_preserves_port_and_exit_code(self):
        with zipfile.ZipFile(self.archive) as z:
            helper = self.root / "flash.sh"
            helper.write_bytes(z.read(self.archive.stem + "/flash.sh"))
        tools = self.root / "tools"; tools.mkdir()
        (tools / "python").write_text("#!/bin/sh\nexit 1\n")
        (tools / "python3").write_text(
            "#!/usr/bin/python3\nimport sys,json,os\n"
            "if sys.argv[1:3]==['-c','import esptool']:sys.exit(0)\n"
            "open(os.environ['ARGS_FILE'],'w').write(json.dumps(sys.argv[1:]))\nsys.exit(7)\n")
        for p in tools.iterdir():p.chmod(0o755)
        output = self.root / "args.json"
        env = dict(os.environ, PATH=str(tools) + ":/bin", ARGS_FILE=str(output))
        result = subprocess.run(["/bin/sh", str(helper), "port with spaces"], env=env)
        self.assertEqual(result.returncode, 7)
        args = json.loads(output.read_text())
        self.assertEqual(args[args.index("--port") + 1], "port with spaces")
        self.assertIn("--no-stub", args)


class MatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temporary.name)
        cls.artifacts = cls.root / "artifacts"; cls.artifacts.mkdir()
        for row in projects():
            archive = make_package(fixture(cls.root, row))
            shutil.copyfile(archive, cls.artifacts / archive.name)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name) / "artifacts"
        shutil.copytree(self.artifacts, self.directory)

    def test_complete_matrix(self):
        self.assertEqual(len(validate_set(self.directory, COMMIT, VERSION)), 16)

    def test_missing_and_unexpected_archives(self):
        next(self.directory.glob("*.zip")).unlink()
        with self.assertRaises(ValueError):validate_set(self.directory, COMMIT, VERSION)
        (self.directory / "unexpected.zip").write_bytes(b"bad")
        with self.assertRaises(ValueError):validate_set(self.directory, COMMIT, VERSION)

    def test_wrong_commit_or_tag(self):
        for commit, tag in [("b" * 40, VERSION), (COMMIT, "v1.2.4")]:
            with self.assertRaises(ValueError):validate_set(self.directory, commit, tag)

    def test_metadata_drift(self):
        archive = next(self.directory.glob("*.zip"))
        def change(data):
            manifest = json.loads(data["manifest.json"])
            manifest["resolved_dependencies"]["waveshare/waveshare_boards"]["version"] = "0.2.1"
            data["manifest.json"] = json.dumps(manifest).encode()
        rewrite(archive, change)
        with self.assertRaisesRegex(ValueError, "Dependency changed"):
            validate_set(self.directory, COMMIT, VERSION)


class FakeGitHub:
    def __init__(self, existing=None, bad_digest=False):
        self.release = existing
        self.bad_digest = bad_digest
        self.uploads = {}
        self.writes = []

    def call(self, route, method="GET", payload=None, binary=None, public=False):
        if method != "GET":self.writes.append((route, method, payload))
        if route.startswith("/releases/tags/"):
            if self.release is None:raise urllib.error.HTTPError(route, 404, "missing", {}, None)
            return dict(self.release)
        if route == "/releases" and method == "POST":
            self.release = dict(payload, id=1, html_url="https://github.com/example/repo/releases/tag/" + VERSION)
        elif "/assets?name=" in route:
            self.uploads[route.split("name=", 1)[1]] = binary
        elif route.startswith("/releases/assets/") and method == "DELETE":
            self.uploads.clear()
        elif method == "PATCH":self.release.update(payload)
        return dict(self.release) if self.release else None

    def assets(self, release_id, public=False):
        return [{"id": i, "name": name, "state": "uploaded", "size": len(data),
                 "digest": "sha256:" + ("0" * 64 if self.bad_digest else digest(data)),
                 "browser_download_url": "https://github.com/example/repo/releases/download/" + name}
                for i, (name, data) in enumerate(self.uploads.items())]


class PublicationTests(FirmwareFixture):
    # Publication tests reuse the valid fixture, while archive tests remain independent.
    def run_publish(self, api, tag_side_effect=None):
        def download(url, timeout):return io.BytesIO(api.uploads[url.rsplit("/", 1)[1]])
        with patch("releases.publish_release.verify_tag", side_effect=tag_side_effect), \
                patch("releases.publish_release.validate_set", return_value=[self.archive]), \
                patch("urllib.request.urlopen", side_effect=download):
            return publish(api, self.root, VERSION, COMMIT)

    def test_success_uploads_checksums_and_verifies_download(self):
        api = FakeGitHub()
        result = self.run_publish(api)
        self.assertFalse(result["draft"])
        self.assertTrue(result["prerelease"])
        self.assertEqual(set(api.uploads), {self.archive.name, "SHA256SUMS"})

    def test_digest_failure_leaves_draft(self):
        api = FakeGitHub(bad_digest=True)
        with self.assertRaises(ValueError):self.run_publish(api)
        self.assertTrue(api.release["draft"])

    def test_existing_public_release_is_never_changed(self):
        api = FakeGitHub({"id": 1, "draft": False, "tag_name": VERSION})
        with self.assertRaises(ValueError):self.run_publish(api)
        self.assertFalse(api.writes)

    def test_draft_can_be_retried(self):
        api = FakeGitHub({"id": 1, "draft": True, "tag_name": VERSION,
                          "html_url": "https://github.com/example/repo/releases/tag/" + VERSION})
        api.uploads["stale.zip"] = b"stale"
        self.run_publish(api)
        self.assertNotIn("stale.zip", api.uploads)
        self.assertTrue(any(method == "DELETE" for _, method, _ in api.writes))

    def test_tag_move_aborts_publication(self):
        api = FakeGitHub()
        with self.assertRaises(ValueError):self.run_publish(api, [None, ValueError("moved")])
        self.assertTrue(api.release["draft"])


class ContractTests(unittest.TestCase):
    def test_version_format_and_selectors(self):
        self.assertEqual(validate_tag("v0.1.0"), "v0.1.0")
        for tag in ["1.0.0", "v1", "v01.0.0", "v1.0.0-rc1", "v1.0.0\n"]:
            with self.assertRaises(ValueError):validate_tag(tag)
        self.assertEqual(projects("01_i2c_scan"), projects("examples/esp-idf/01_i2c_scan"))
        self.assertEqual(len(projects()), 16)

    def test_workflow_has_success_gates_and_pinned_actions(self):
        text = (ROOT / ".github/workflows/firmware.yml").read_text()
        self.assertIn("needs: [gate, build, validate]", text)
        self.assertNotIn("always()", text)
        import re
        for action in re.findall(r"uses: (\S+)", text):
            self.assertRegex(action, r"^[^@]+@[0-9a-f]{40}$")

    def test_wrong_asset_set(self):
        with self.assertRaises(ValueError):verify_assets([], {"example.zip": (1, "sha256:" + "a" * 64)})

    def test_reject_lightweight_or_moved_tag(self):
        api = FakeGitHub()
        with patch("releases.publish_release.git", side_effect=[COMMIT, "commit"]):
            with self.assertRaises(ValueError):verify_tag(api, VERSION, COMMIT)

    def test_annotated_tag_uses_matching_remote_object_and_default_branch(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            def run(*args):
                return subprocess.check_output(["git", "-C", str(directory), *args],
                                               text=True, stderr=subprocess.DEVNULL).strip()
            run("init", "-b", "main")
            (directory / "source.txt").write_text("fixture\n")
            run("add", "source.txt")
            run("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-m", "fixture")
            commit = run("rev-parse", "HEAD")
            run("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "tag", "-a", VERSION, "-m", "fixture")
            tag_object = run("rev-parse", "refs/tags/" + VERSION)
            from unittest.mock import Mock
            api = Mock()
            values = [{"object": {"type": "tag", "sha": tag_object}},
                      {"object": {"type": "commit", "sha": commit}},
                      {"default_branch": "main"}, {"status": "identical"}]
            with patch("releases.publish_release.ROOT", directory):
                api.call.side_effect = values
                verify_tag(api, VERSION, commit)
                api.call.side_effect = [{"object": {"type": "tag", "sha": "f" * 40}}]
                with self.assertRaisesRegex(ValueError, "Remote tag changed"):
                    verify_tag(api, VERSION, commit)
                api.call.side_effect = values[:-1] + [{"status": "diverged"}]
                with self.assertRaisesRegex(ValueError, "default branch"):
                    verify_tag(api, VERSION, commit)


if __name__ == "__main__":
    unittest.main()
