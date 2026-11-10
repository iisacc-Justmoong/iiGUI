"""A static generic host must not pull in the LVRS compatibility entry points."""
import subprocess
import sys

symbols = subprocess.check_output([sys.argv[2], sys.argv[1]], text=True)
for required in ("default_widget_appearance", "default_text_tokens", "require_text_weight_rules_abi_v1"):
    if required not in symbols:
        raise AssertionError(f"Generic host path is not linked: {required}")
for legacy in ("lvrs_appearance", "lvrs_text_tokens"):
    if legacy in symbols:
        raise AssertionError(f"Generic host pulled in a legacy compatibility symbol: {legacy}")
