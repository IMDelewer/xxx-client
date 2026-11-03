#!/usr/bin/env python3
import argparse
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


def copy_notices(root, destination):
    for name in ("README.md", "LICENSE"):
        shutil.copy2(root / name, destination / name)
    shutil.copytree(root / "licenses", destination / "licenses")
    shutil.copytree(root / "docs", destination / "docs")
    shutil.copy2(root / "PROMT.md", destination / "PROMT.md")
    external = destination / "licenses" / "external"
    for base in (root / "src/engine/external", root / "ddnet-libs"):
        if not base.is_dir():
            continue
        for source in base.rglob("*"):
            if source.is_file() and any(word in source.name.lower() for word in ("license", "licence", "copying", "copyright")):
                target = external / base.name / source.relative_to(base)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)



def copy_macos_notices(executable, destination):
    pending = [executable]
    seen = set()
    formula_roots = set()
    while pending:
        binary = pending.pop()
        if binary in seen:
            continue
        seen.add(binary)
        parts = binary.resolve().parts
        if "Cellar" in parts:
            formula_roots.add(Path(*parts[:parts.index("Cellar") + 3]))
        listing = subprocess.run(["otool", "-L", str(binary)], check=True, capture_output=True, text=True).stdout
        for line in listing.splitlines()[1:]:
            dependency = Path(line.strip().split(" (", 1)[0])
            if not dependency.is_absolute() or not dependency.is_file() or "Cellar" not in dependency.resolve().parts:
                continue
            resolved = dependency.resolve()
            parts = resolved.parts
            cellar = parts.index("Cellar")
            formula_roots.add(Path(*parts[:cellar + 3]))
            pending.append(resolved)
    for base in sorted(formula_roots):
        for source in base.rglob("*"):
            if source.is_file() and any(word in source.name.lower() for word in ("license", "licence", "copying", "copyright")):
                target = destination / "licenses" / "macos-runtime" / base.parent.name / source.relative_to(base)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)


def main():
    parser = argparse.ArgumentParser(description="Package xxx client and its runtime resources")
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("dist"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    version = re.search(r'#define XXX_VERSION "([^"]+)"', (root / "src/game/version.h").read_text()).group(1)
    system = platform.system()
    executable_name = "xxx.exe" if system == "Windows" else "xxx"
    executable = next((p for p in (build / executable_name, build / "Release" / executable_name) if p.is_file()), None)
    if executable is None or not (build / "data").is_dir():
        raise SystemExit("Build game-client before packaging; the executable and build/data are required")
    subprocess.run(["python3" if system != "Windows" else "python", str(root / "scripts/check_public_tree.py"), "--binary", str(executable)], check=True)
    label = f"xxx-client-{version}-{system.lower()}-{platform.machine().lower()}"
    with tempfile.TemporaryDirectory(prefix="xxx-package-", dir=output) as temporary:
        stage = Path(temporary) / label
        stage.mkdir()
        if system == "Darwin":
            if shutil.which("dylibbundler") is None:
                raise SystemExit("Install dylibbundler to produce a self-contained macOS package")
            app = stage / "xxx.app"
            contents = app / "Contents"
            binary_dir = contents / "MacOS"
            resources = contents / "Resources"
            binary_dir.mkdir(parents=True)
            resources.mkdir(parents=True)
            target = binary_dir / "xxx"
            shutil.copy2(executable, target)
            target.chmod(0o755)
            shutil.copytree(build / "data", resources / "data", ignore=shutil.ignore_patterns("spoofclients"))
            copy_notices(root, resources)
            copy_macos_notices(executable, resources)
            shutil.copy2(root / "other/icons/DDNet.icns", resources / "xxx.icns")
            info = {"CFBundleName": "xxx", "CFBundleDisplayName": "xxx client", "CFBundleExecutable": "xxx", "CFBundleIdentifier": "io.github.imdelewer.xxx-client", "CFBundlePackageType": "APPL", "CFBundleShortVersionString": version, "CFBundleVersion": version, "CFBundleIconFile": "xxx.icns", "NSHighResolutionCapable": True}
            (contents / "Info.plist").write_bytes(plistlib.dumps(info))
            frameworks = contents / "Frameworks"
            frameworks.mkdir()
            extra_binaries = []
            listing = subprocess.run(["otool", "-L", str(executable)], check=True, capture_output=True, text=True).stdout
            for line in listing.splitlines()[1:]:
                dependency = Path(line.strip().split(" (", 1)[0])
                if dependency.is_file() and "SDL2" in dependency.name and b"@loader_path/libSDL3.dylib" in dependency.read_bytes():
                    prefix = subprocess.run(["brew", "--prefix", "sdl3"], check=True, capture_output=True, text=True).stdout.strip()
                    library = Path(prefix) / "lib/libSDL3.dylib"
                    bundled = frameworks / "libSDL3.dylib"
                    shutil.copy2(library, bundled)
                    copy_macos_notices(library, resources)
                    extra_binaries.extend(["-x", str(bundled)])
            subprocess.run(["dylibbundler", "-b", "-of", "-x", str(target), *extra_binaries, "-s", str(build), "-d", str(frameworks), "-p", "@executable_path/../Frameworks/"], check=True, stdin=subprocess.DEVNULL, timeout=180)
            minimum = (10, 15, 0)
            for binary in [target, *frameworks.rglob("*.dylib")]:
                commands = subprocess.run(["otool", "-l", str(binary)], check=True, capture_output=True, text=True).stdout
                versions = re.findall(r"\bminos (\d+(?:\.\d+)*)", commands)
                versions += re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+(?:\.\d+)*)", commands)
                for required in versions:
                    parts = tuple(map(int, required.split(".")))
                    minimum = max(minimum, (parts + (0, 0, 0))[:3])
            info["LSMinimumSystemVersion"] = ".".join(map(str, minimum))
            (contents / "Info.plist").write_bytes(plistlib.dumps(info))
            for library in frameworks.rglob("*.dylib"):
                library.chmod(0o755)
                subprocess.run(["install_name_tool", "-id", "@executable_path/../Frameworks/" + library.name, str(library)], check=True)
                subprocess.run(["codesign", "--force", "--sign", "-", str(library)], check=True)
            subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(app)], check=True)
        else:
            target = stage / executable_name
            shutil.copy2(executable, target)
            target.chmod(0o755)
            shutil.copytree(build / "data", stage / "data", ignore=shutil.ignore_patterns("spoofclients"))
            copy_notices(root, stage)
            if system == "Windows":
                for folder in (build, build / "Release"):
                    if folder.is_dir():
                        for library in folder.glob("*.dll"):
                            shutil.copy2(library, stage / library.name)
        if system == "Linux":
            archive = output / f"{label}.tar.xz"
            with tarfile.open(archive, "w:xz") as bundle:
                bundle.add(stage, arcname=stage.name)
        else:
            archive = output / f"{label}.zip"
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
                for source in stage.rglob("*"):
                    if source.is_file():
                        bundle.write(source, source.relative_to(stage.parent))
        print(archive)


if __name__ == "__main__":
    main()
