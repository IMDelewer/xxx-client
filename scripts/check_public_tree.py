#!/usr/bin/env python3
import argparse
from pathlib import Path
import re
import sys


def main():
    parser = argparse.ArgumentParser(description="Check the public client source and release binary")
    parser.add_argument("--binary", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    failures = []
    for relative in ("src/game/client/cheats", "data/spoofclients", "src/engine/client/input_delay.h", "src/engine/shared/xx_spoof_profile.h", "scripts/xxx_profile_builder.py", "src/tools/xxx-profile-builder.cpp", ".oss-scanner", ".oss_scanner", "scripts/test_xx_proxy.py"):
        if (root / relative).exists():
            failures.append(f"Private module is present: {relative}")
    cmake = (root / "CMakeLists.txt").read_text()
    if not re.search(r'option\(DDEV_XX\s+"[^"]+"\s+OFF\)', cmake):
        failures.append("DDEV_XX must default to OFF")
    for relative in ("src/game/client/gameclient.cpp", "src/game/client/gameclient.h", "src/engine/client/client.cpp"):
        text = (root / relative).read_text()
        if re.search(r"m_Assist|m_Spoofer|cheats/|m_InputDelay|m_aDelayedAck|XxFakePing|XxFakeLag", text):
            failures.append(f"Private module reference remains in {relative}")
    for relative in ("LICENSE", "licenses/DDNet.txt", "licenses/README.md", "licenses/BestClient-MIT.txt", "licenses/TClient-license.txt"):
        if not (root / relative).is_file():
            failures.append(f"Missing license notice: {relative}")
    if args.binary:
        data = args.binary.read_bytes()
        for command in (b"xx_after", b"xx_screenshot", b"xx_dev_screen", b"xx_dev_mouse", b"xx_dev_key", b"xx_dev_wheel", b"xx_dev_click", b"+showhookcoll", b"xx_island_test", b"xx_spoofer", b"xx_avoid", b"xx_fake_ping", b"xx_fake_lag", b"xx_aid_trajectory"):
            if command in data:
                failures.append(f"Release binary contains {command.decode()}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("Public source checks passed" + ("; release binary checks passed" if args.binary else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
