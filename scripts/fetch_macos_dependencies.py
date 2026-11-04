#!/usr/bin/env python3
"""Fetch pinned upstream runtime libraries independently of Homebrew bottles."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import platform
import shutil
import tarfile
import urllib.request


REVISION = "c0e6703fbcdbe03df2f26875427ec3951ce4ec21"
MOLTENVK_SHA256 = "f95765a6229cb7b915990a2890ce12ebe36a730b021545d3d52ae69ce4c4024e"
LIBRARIES = {"curl", "freetype", "opus", "png", "sdl", "wavpack", "zlib"}


def download(url):
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--arch", choices=("arm64", "x86_64"), default=platform.machine())
    args = parser.parse_args()
    output = args.output.resolve()
    root = output / "ddnet-libs"
    architecture = "libarm64" if args.arch == "arm64" else "lib64"
    tree = json.loads(download(f"https://api.github.com/repos/ddnet/ddnet-libs/git/trees/{REVISION}?recursive=1"))
    if tree.get("truncated"):
        raise SystemExit("The dependency manifest is truncated")
    entries = [entry for entry in tree["tree"] if entry["type"] == "blob" and entry["path"].split("/")[0] in LIBRARIES and ("/include/" in entry["path"] or f"/mac/{architecture}/" in entry["path"] or any(word in Path(entry["path"]).name.lower() for word in ("license", "licence", "copying", "copyright")))]

    def fetch(entry):
        relative = entry["path"]
        target = root / relative
        if target.is_file() and not target.is_symlink():
            data = target.read_bytes()
            digest = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
            if digest == entry["sha"]:
                return
        data = download(f"https://raw.githubusercontent.com/ddnet/ddnet-libs/{REVISION}/{relative}")
        digest = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if digest != entry["sha"]:
            raise RuntimeError(f"Dependency checksum mismatch: {relative}")
        target.parent.mkdir(parents=True, exist_ok=True)
        if entry["mode"] == "120000":
            link = data.decode()
            if not (target.parent / link).resolve().is_relative_to(root):
                raise RuntimeError(f"Invalid dependency symlink: {relative}")
            if not target.is_symlink():
                target.symlink_to(link)
        else:
            temporary = target.with_name(target.name + ".download")
            temporary.write_bytes(data)
            temporary.replace(target)

    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(fetch, entries))
    archive = output / "MoltenVK-macos-1.4.2.tar"
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != MOLTENVK_SHA256:
        data = download("https://github.com/KhronosGroup/MoltenVK/releases/download/v1.4.2/MoltenVK-macos.tar")
        if hashlib.sha256(data).hexdigest() != MOLTENVK_SHA256:
            raise SystemExit("MoltenVK checksum mismatch")
        archive.write_bytes(data)
    destination = output / "MoltenVK"
    with tarfile.open(archive) as bundle:
        candidates = [member for member in bundle.getmembers() if member.isfile() and member.name.endswith("/dylib/macOS/libMoltenVK.dylib")]
        if len(candidates) != 1:
            raise SystemExit("Could not locate the macOS MoltenVK runtime")
        destination.mkdir(exist_ok=True)
        with bundle.extractfile(candidates[0]) as source, (destination / "libMoltenVK.dylib").open("wb") as target:
            shutil.copyfileobj(source, target)
        for member in bundle.getmembers():
            if member.isfile() and Path(member.name).name in ("LICENSE", "LICENSE.txt"):
                with bundle.extractfile(member) as source:
                    (destination / "LICENSE").write_bytes(source.read())
                break
    print(root)
    print(destination / "libMoltenVK.dylib")


if __name__ == "__main__":
    main()
