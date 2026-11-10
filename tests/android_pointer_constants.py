"""Compare the unavailable native canceled-gesture flag to the Android SDK."""
import argparse
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--javap", type=Path, required=True)
    args = parser.parse_args()
    output = subprocess.run([str(args.javap), "-constants", "-classpath", str(args.jar),
                             "android.view.MotionEvent"], check=True, text=True, capture_output=True).stdout
    sdk = re.search(r"\bFLAG_CANCELED\s*=\s*(\d+)\s*;", output)
    header = (Path(__file__).resolve().parents[1] / "src/Platform/AndroidPointerNative.h").read_text()
    native = re.search(r"\bandroid_motion_flag_canceled\s*=\s*(0x[\da-fA-F]+|\d+)\s*;", header)
    if not sdk or not native or int(sdk.group(1)) != int(native.group(1), 0):
        raise RuntimeError("Android canceled-pointer flag differs from the installed SDK")
    print("Android canceled-pointer flag matches the installed SDK")


if __name__ == "__main__":
    main()
