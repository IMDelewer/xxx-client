#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


def copy_notices(root, destination, libraries=None):
    shutil.copy2(root / "docs/PACKAGE.md", destination / "README.md")
    shutil.copy2(root / "LICENSE", destination / "LICENSE")
    shutil.copytree(root / "licenses", destination / "licenses")
    external = destination / "licenses" / "external"
    for base in (root / "src/engine/external", libraries or root / "ddnet-libs"):
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


def copy_runtime_data(build, destination):
    manifest = build / "runtime-data.txt"
    if not manifest.is_file():
        raise SystemExit("Reconfigure the build to generate runtime-data.txt")
    fonts = json.loads((build / "data/fonts/index.json").read_text(encoding="utf-8"))
    optional = {"data/fonts/" + entry["file"] for entry in fonts.get("optional font files", [])}
    for entry in sorted(set(manifest.read_text(encoding="utf-8").splitlines())):
        relative = Path(entry)
        if relative.is_absolute() or ".." in relative.parts or not entry.startswith("data/"):
            raise SystemExit(f"Invalid runtime resource: {entry}")
        if entry in optional or entry.startswith("data/spoofclients/") or relative.suffix in (".vert", ".frag"):
            raise SystemExit(f"Unexpected release resource: {entry}")
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(build / relative, target)


def macho_files(folder):
    seen = set()
    for path in folder.rglob("*"):
        if not path.is_file() or path.resolve() in seen:
            continue
        with path.open("rb") as stream:
            magic = stream.read(4)
        if magic in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"):
            resolved = path.resolve()
            if not resolved.is_relative_to(folder.resolve()):
                raise SystemExit(f"Runtime symlink leaves the application: {path}")
            seen.add(resolved)
            yield resolved


def expand_macos_path(value, loader, executable):
    for prefix, parent in (("@executable_path/", executable.parent), ("@loader_path/", loader.parent)):
        if value.startswith(prefix):
            return parent / value[len(prefix):]
    return Path(value) if value.startswith("/") else None


def macos_dependency_exists(dependency, loader, executable, runpaths):
    if dependency.startswith("@rpath/"):
        candidates = [path / dependency[len("@rpath/"):] for path in runpaths]
    else:
        path = expand_macos_path(dependency, loader, executable)
        candidates = [path] if path is not None else []
    app = executable.parent.parent.parent.resolve()
    return any(path.is_file() and path.resolve().is_relative_to(app) for path in candidates)


def macos_rpaths(binary):
    commands = subprocess.run(["otool", "-l", str(binary)], check=True, capture_output=True, text=True).stdout
    return re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset \d+\)", commands)


def prepare_macos_binaries(app, minimum_version):
    architecture = platform.machine().lower()
    required = tuple(map(int, minimum_version.split(".")))
    required = (required + (0, 0, 0))[:3]
    binaries = list(macho_files(app))
    for binary in binaries:
        architectures = subprocess.run(["lipo", "-archs", str(binary)], check=True, capture_output=True, text=True).stdout.split()
        if architecture not in architectures:
            raise SystemExit(f"Wrong architecture: {binary.name}")
        if len(architectures) > 1:
            temporary = binary.with_name(binary.name + ".thin")
            subprocess.run(["lipo", str(binary), "-thin", architecture, "-output", str(temporary)], check=True)
            temporary.replace(binary)
        binary.chmod(0o755)
        subprocess.run(["strip", "-S", "-x", str(binary)], check=True)
        commands = subprocess.run(["otool", "-l", str(binary)], check=True, capture_output=True, text=True).stdout
        versions = re.findall(r"\bminos (\d+(?:\.\d+)*)", commands)
        versions += re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+(?:\.\d+)*)", commands)
        for version in versions:
            parts = tuple(map(int, version.split(".")))
            if (parts + (0, 0, 0))[:3] > required:
                raise SystemExit(f"{binary.name} requires macOS {version}, above the package minimum {minimum_version}")
    executable = (app / "Contents/MacOS/xxx").resolve()
    for binary in binaries:
        paths = macos_rpaths(binary)
        while paths:
            subprocess.run(["install_name_tool", "-delete_rpath", paths[0], str(binary)], check=True)
            paths = macos_rpaths(binary)
        if binary == executable:
            subprocess.run(["install_name_tool", "-add_rpath", "@executable_path/../Frameworks", str(binary)], check=True)
    root_paths = [expand_macos_path(path, executable, executable) for path in macos_rpaths(executable)]
    for binary in binaries:
        runpaths = root_paths + [expand_macos_path(path, binary, executable) for path in macos_rpaths(binary)]
        runpaths = [path for path in runpaths if path is not None]
        listing = subprocess.run(["otool", "-L", str(binary)], check=True, capture_output=True, text=True).stdout
        for line in listing.splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            if dependency.startswith(("/usr/lib/", "/System/Library/")):
                continue
            if not macos_dependency_exists(dependency, binary, executable, runpaths):
                raise SystemExit(f"Unresolved dependency in {binary.name}: {dependency}")
    return binaries


def package_report(stage, archive):
    files = [{"path": str(path.relative_to(stage)), "bytes": path.stat().st_size} for path in stage.rglob("*") if path.is_file() and not path.is_symlink()]
    files.sort(key=lambda item: item["bytes"], reverse=True)
    report = {"archive": archive.name, "archive_bytes": archive.stat().st_size, "installed_bytes": sum(item["bytes"] for item in files), "files": files}
    archive.with_name(archive.name + ".json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{archive.name}: {report['archive_bytes'] / 1000000:.1f} MB download, {report['installed_bytes'] / 1000000:.1f} MB installed")


def main():
    parser = argparse.ArgumentParser(description="Package xxx client and its runtime resources")
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("dist"))
    parser.add_argument("--macos-min-version", default="12.0")
    parser.add_argument("--max-size-mib", type=int, default=45)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    version = re.search(r'#define XXX_VERSION "([^"]+)"', (root / "src/game/version.h").read_text(encoding="utf-8")).group(1)
    cache = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.startswith(("#", "//")):
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    libraries = Path(cache.get("DDNET_LIBS_DIR", str(root / "ddnet-libs")))
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
            copy_runtime_data(build, resources)
            copy_notices(root, resources, libraries)
            copy_macos_notices(executable, resources)
            shutil.copy2(root / "other/icons/DDNet.icns", resources / "xxx.icns")
            info = {"CFBundleName": "xxx", "CFBundleDisplayName": "xxx client", "CFBundleExecutable": "xxx", "CFBundleIdentifier": "io.github.imdelewer.xxx-client", "CFBundlePackageType": "APPL", "CFBundleShortVersionString": version, "CFBundleVersion": version, "CFBundleIconFile": "xxx.icns", "NSHighResolutionCapable": True}
            (contents / "Info.plist").write_bytes(plistlib.dumps(info))
            frameworks = contents / "Frameworks"
            frameworks.mkdir()
            extra_binaries = []
            sdl = Path(cache.get("SDL2_LIBRARY", ""))
            if sdl.is_dir() and sdl.suffix == ".framework":
                bundled_sdl = frameworks / sdl.name
                shutil.copytree(sdl, bundled_sdl, symlinks=True, ignore=shutil.ignore_patterns("Headers", "PrivateHeaders", "Modules", "_CodeSignature"))
                extra_binaries.extend(["-x", str(bundled_sdl / "Versions/A/SDL2")])
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
            searches = ["-s", str(build)]
            for key in ("VULKAN_LIBRARIES", "FREETYPE_LIBRARY", "PNG_LIBRARY", "SDL2_LIBRARY"):
                path = Path(cache.get(key, ""))
                if path.exists():
                    searches.extend(["-s", str(path.parent)])
            subprocess.run(["dylibbundler", "-b", "-of", "-x", str(target), *extra_binaries, *searches, "-d", str(frameworks), "-p", "@executable_path/../Frameworks/"], check=True, input="quit\n", text=True, timeout=180)
            binaries = prepare_macos_binaries(app, args.macos_min_version)
            info["LSMinimumSystemVersion"] = args.macos_min_version
            (contents / "Info.plist").write_bytes(plistlib.dumps(info))
            vulkan = Path(cache.get("VULKAN_LIBRARIES", ""))
            if vulkan.is_file():
                for notice in vulkan.parent.glob("LICENSE*"):
                    target_notice = resources / "licenses/external/MoltenVK" / notice.name
                    target_notice.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(notice, target_notice)
            for binary in binaries:
                if binary.is_relative_to(frameworks):
                    relative = binary.relative_to(frameworks).as_posix()
                    subprocess.run(["install_name_tool", "-id", "@executable_path/../Frameworks/" + relative, str(binary)], check=True)
                    subprocess.run(["codesign", "--force", "--sign", "-", str(binary)], check=True)
            subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(app)], check=True)
            subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
            (stage / "Applications").symlink_to("/Applications", target_is_directory=True)
        else:
            target = stage / executable_name
            shutil.copy2(executable, target)
            target.chmod(0o755)
            copy_runtime_data(build, stage)
            copy_notices(root, stage, libraries)
            if system == "Windows":
                for name in (build / "runtime-libraries.txt").read_text(encoding="utf-8").splitlines():
                    if Path(name).name != name or not name.lower().endswith(".dll"):
                        raise SystemExit(f"Invalid runtime library: {name}")
                    library = next((folder / name for folder in (build, build / "Release") if (folder / name).is_file()), None)
                    if library is None:
                        raise SystemExit(f"Missing runtime library: {name}")
                    shutil.copy2(library, stage / name)
            elif shutil.which("strip"):
                subprocess.run(["strip", "--strip-unneeded", str(target)], check=True)
        resource_root = resources if system == "Darwin" else stage
        subprocess.run(["python3" if system != "Windows" else "python", str(root / "scripts/check_public_tree.py"), "--package-dir", str(resource_root)], check=True)
        if system == "Linux":
            archive = output / f"{label}.tar.xz"
            with tarfile.open(archive, "w:xz") as bundle:
                bundle.add(stage, arcname=stage.name)
        elif system == "Darwin":
            archive = output / f"{label}.dmg"
            subprocess.run(["hdiutil", "create", "-quiet", "-ov", "-fs", "HFS+", "-format", "ULFO", "-volname", "xxx client", "-srcfolder", str(stage), str(archive)], check=True)
            subprocess.run(["hdiutil", "verify", "-quiet", str(archive)], check=True)
        else:
            archive = output / f"{label}.zip"
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
                for source in stage.rglob("*"):
                    if source.is_file():
                        bundle.write(source, source.relative_to(stage.parent))
        package_report(stage, archive)
        if archive.stat().st_size > args.max_size_mib * 1024 * 1024:
            raise SystemExit(f"Package exceeds the {args.max_size_mib} MiB release budget")
        print(archive)


if __name__ == "__main__":
    main()
