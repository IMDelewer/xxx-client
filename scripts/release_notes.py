#!/usr/bin/env python3
"""Print the changelog entry matching the client version."""
from pathlib import Path
import re


def notes_for_version(changelog, version):
    match = re.search(r"^## " + re.escape(version) + r"\s*\n(.*?)(?=^## |\Z)", changelog, re.MULTILINE | re.DOTALL)
    if not match or not match.group(1).strip():
        raise ValueError(f"Missing changelog entry for {version}")
    return match.group(1).strip()


def main():
    root = Path(__file__).resolve().parents[1]
    version = re.search(r'#define XXX_VERSION "([^"]+)"', (root / "src/game/version.h").read_text(encoding="utf-8")).group(1)
    print(notes_for_version((root / "CHANGELOG.md").read_text(encoding="utf-8"), version))


if __name__ == "__main__":
    main()
