import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from package_client import copy_runtime_data, macos_dependency_exists, prepare_macos_binaries
from release_notes import notes_for_version


class RuntimeManifestTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.destination = self.root / "package"
        (self.build / "data/fonts").mkdir(parents=True)
        (self.build / "data/fonts/index.json").write_text(json.dumps({"optional font files": [{"file": "optional.ttf"}]}))
        (self.build / "runtime-data.txt").write_text("data/fonts/index.json\n")

    def test_stale_files_are_not_packaged(self):
        (self.build / "data/fonts/optional.ttf").write_bytes(b"cached font")
        (self.build / "data/spoofclients").mkdir()
        (self.build / "data/spoofclients/private.json").write_text("{}")
        copy_runtime_data(self.build, self.destination)
        self.assertEqual([str(path.relative_to(self.destination)) for path in self.destination.rglob("*") if path.is_file()], ["data/fonts/index.json"])

    def test_rejects_private_fonts_shaders_and_path_traversal(self):
        for entry in ("data/fonts/optional.ttf", "data/spoofclients/private.json", "data/shader/code.frag", "../secret", "/data/secret"):
            with self.subTest(entry=entry):
                (self.build / "runtime-data.txt").write_text(entry + "\n")
                with self.assertRaises(SystemExit):
                    copy_runtime_data(self.build, self.destination)

    def test_missing_resource_fails_packaging(self):
        (self.build / "runtime-data.txt").write_text("data/missing.png\n")
        with self.assertRaises(FileNotFoundError):
            copy_runtime_data(self.build, self.destination)

    def test_missing_manifest_requires_reconfiguration(self):
        (self.build / "runtime-data.txt").unlink()
        with self.assertRaises(SystemExit):
            copy_runtime_data(self.build, self.destination)


class ReleaseNotesTests(unittest.TestCase):
    def test_only_requested_version_is_published(self):
        changelog = "# Changelog\n\n## 0.2.0\n\n- Smaller packages.\n\n## 0.1.0\n\n- Initial release.\n"
        self.assertEqual(notes_for_version(changelog, "0.2.0"), "- Smaller packages.")
        self.assertEqual(notes_for_version(changelog, "0.1.0"), "- Initial release.")

    def test_missing_and_empty_entries_fail(self):
        for changelog in ("## 0.1.0\n- Initial release.\n", "## 0.2.0\n\n## 0.1.0\n- Initial release.\n"):
            with self.assertRaises(ValueError):
                notes_for_version(changelog, "0.2.0")


class MacosCompatibilityTests(unittest.TestCase):
    def test_missing_framework_cannot_pass_dependency_check(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / "xxx.app"
            executable = app / "Contents/MacOS/xxx"
            frameworks = app / "Contents/Frameworks"
            dependency = "@rpath/SDL2.framework/Versions/A/SDL2"
            self.assertFalse(macos_dependency_exists(dependency, executable, executable, [frameworks]))
            sdl = frameworks / "SDL2.framework/Versions/A/SDL2"
            sdl.parent.mkdir(parents=True)
            sdl.write_bytes(b"framework fixture")
            self.assertTrue(macos_dependency_exists(dependency, executable, executable, [frameworks]))

    @unittest.skipIf(os.name == "nt", "macOS bundle symlinks are checked on macOS")
    def test_external_runtime_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            executable = root / "xxx.app/Contents/MacOS/xxx"
            frameworks = root / "xxx.app/Contents/Frameworks"
            frameworks.mkdir(parents=True)
            external = root / "external.dylib"
            external.write_bytes(b"external fixture")
            (frameworks / "external.dylib").symlink_to(external)
            self.assertFalse(macos_dependency_exists("@rpath/external.dylib", executable, executable, [frameworks]))

    def test_newer_runtime_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary)
            (app / "runtime.dylib").write_bytes(b"\xcf\xfa\xed\xfe")

            def command(args, **kwargs):
                result = type("Result", (), {})()
                result.stdout = "arm64\n" if args[0] == "lipo" else "cmd LC_BUILD_VERSION\nminos 26.0\n"
                return result

            with patch("package_client.platform.machine", return_value="arm64"), patch("package_client.subprocess.run", side_effect=command):
                with self.assertRaisesRegex(SystemExit, "requires macOS 26.0"):
                    prepare_macos_binaries(app, "12.0")


if __name__ == "__main__":
    unittest.main()
