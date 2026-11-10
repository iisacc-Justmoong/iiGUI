"""A new application driver must not link against an older iiGUI library."""
import os
import subprocess
import sys

cxx, library = sys.argv[1:3]
source = "namespace ii::gui { void require_application_driver_abi_v1() noexcept; }\nint main(){ii::gui::require_application_driver_abi_v1();}\n"
base = [cxx, "-std=c++23", "-x", "c++", "-", "-x", "none"]


def link(extra):
    return subprocess.run(base + extra + ["-o", os.devnull], input=source, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)


missing = link([])
if missing.returncode == 0 or "require_application_driver_abi_v1" not in missing.stderr:
    raise AssertionError(f"Missing driver ABI anchor was not diagnosed: {missing.stderr}")
current = link([library])
if current.returncode != 0:
    raise AssertionError(f"The current iiGUI library did not supply the driver ABI anchor: {current.stderr}")
