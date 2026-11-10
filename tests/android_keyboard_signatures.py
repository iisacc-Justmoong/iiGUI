"""Check Android keyboard JNI signatures and accent bits against the SDK."""
from pathlib import Path
import argparse
import re
import subprocess
from android_text_signatures import declaration_matches


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--javap", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = (root / "src/Platform/AndroidKeyboardMap.h").read_text()
    output = subprocess.run([str(args.javap), "-s", "-constants", "-classpath", str(args.jar),
                             "android.view.KeyCharacterMap"], check=True, text=True, capture_output=True).stdout
    for name, descriptor in (("load", "(I)Landroid/view/KeyCharacterMap;"),
                             ("get", "(II)I"), ("getDeadChar", "(II)I")):
        if not declaration_matches(output, name, descriptor, "android.view.KeyCharacterMap"):
            raise RuntimeError("Android SDK method is missing: " + name)
        if f'"{name}", "{descriptor}"' not in source:
            raise RuntimeError("Native key mapper does not call the checked descriptor: " + name)
    model = (root / "src/Platform/AndroidKeyboard.h").read_text()
    for sdk_name, source_name in (("COMBINING_ACCENT", "accent_flag"), ("COMBINING_ACCENT_MASK", "accent_mask")):
        sdk = re.search(r"\b" + sdk_name + r"\s*=\s*(-?\d+)\s*;", output)
        native = re.search(r"\b" + source_name + r"\s*=\s*(0x[0-9a-fA-F]+|\d+)u", model)
        if not sdk or not native or int(sdk.group(1)) & 0xffffffff != int(native.group(1), 0):
            raise RuntimeError("Android accent bit differs from the SDK: " + sdk_name)
    key_output = subprocess.run([str(args.javap), "-constants", "-classpath", str(args.jar),
                                 "android.view.KeyEvent"], check=True, text=True, capture_output=True).stdout
    device_output = subprocess.run([str(args.javap), "-constants", "-classpath", str(args.jar),
                                    "android.view.InputDevice"], check=True, text=True, capture_output=True).stdout
    fixture = root / "resources/fixtures/android-keyboard/android"
    values = "\n".join((fixture / name).read_text() for name in ("input.h", "keycodes.h"))
    for native_name, number in re.findall(r"\b(AKEYCODE_\w+|AKEY_EVENT_ACTION_\w+|AKEY_EVENT_FLAG_\w+|AMETA_\w+|AINPUT_SOURCE_\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+)", values):
        if native_name.startswith("AKEYCODE_"):
            sdk_name, declarations = native_name[1:], key_output
        elif native_name.startswith("AKEY_EVENT_ACTION_"):
            sdk_name, declarations = native_name.removeprefix("AKEY_EVENT_"), key_output
        elif native_name.startswith("AKEY_EVENT_FLAG_"):
            sdk_name, declarations = native_name.removeprefix("AKEY_EVENT_"), key_output
        elif native_name.startswith("AMETA_"):
            sdk_name, declarations = native_name[1:], key_output
        else:
            sdk_name, declarations = native_name.removeprefix("AINPUT_"), device_output
        sdk = re.search(r"\b" + sdk_name + r"\s*=\s*(-?\d+)\s*;", declarations)
        if not sdk or int(sdk.group(1)) & 0xffffffff != int(number, 0):
            raise RuntimeError("Keyboard test constant differs from Android SDK: " + native_name)
    print("Android keyboard JNI descriptors, key codes and dead-key constants match the SDK")


if __name__ == "__main__":
    main()
