"""Compile the actual embedded Metal source without opening a GPU window."""
from pathlib import Path
import subprocess
import sys


root, sdk = Path(sys.argv[1]), sys.argv[2]
source = (root / "src/Graphics/CanvasMetal.mm").read_text(encoding="utf-8")
shader = source.split('NSString* const shader = @R"metal(', 1)[1].split(')metal";', 1)[0]
result = subprocess.run(
    ["xcrun", "--sdk", sdk, "metal", "-x", "metal", "-c", "-o", "/dev/null", "-"],
    input=shader, text=True, capture_output=True, check=False,
)
if result.returncode:
    print(result.stdout, result.stderr, file=sys.stderr)
    raise SystemExit(result.returncode)
print(f"METAL_SHADER_OK {sdk}")
