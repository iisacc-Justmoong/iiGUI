"""A generated appearance must not link against a library missing the ABI anchor."""
import os
import subprocess
import sys

cxx, include, library = sys.argv[1:4]
source = "#include <iiGUI/Appearance.h>\nint main(){ii::gui::require_text_weight_rules_abi_v1();}\n"
base = [cxx, "-std=c++23", "-DIIGUI_STATIC_DEFINE", "-I", include, "-x", "c++", "-", "-x", "none"]

def link(extra):
    return subprocess.run(base + extra + ["-o", os.devnull], input=source, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)

missing = link([])
if missing.returncode == 0 or "require_text_weight_rules_abi_v1" not in missing.stderr:
    raise AssertionError(f"Missing ABI anchor was not diagnosed: {missing.stderr}")
current = link([library])
if current.returncode != 0:
    raise AssertionError(f"The current iiGUI library did not supply its ABI anchor: {current.stderr}")
