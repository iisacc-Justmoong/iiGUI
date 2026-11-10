"""Disposable project resources for build-host contract tests."""
from contextlib import contextmanager
import os
from pathlib import Path
import shutil
import sys
import uuid

provider_dir = os.environ.get("IIFILEPROVIDER_PYTHON_DIRECTORY") or str(
    Path.home() / ".local/SDK/iiFileProvider/share/iiFileProvider/python")
sys.path.insert(0, provider_dir)
import ii_file_provider as provider


@contextmanager
def temporary(project, prefix="test-"):
    project = Path(project).resolve(strict=True)
    if not prefix or "/" in prefix or "\\" in prefix or prefix in (".", ".."):
        raise ValueError("Test workspace prefix must be a single name")
    resources = project / "resources"
    state = resources / ".state"
    if resources.is_symlink() or state.is_symlink():
        raise ValueError("Test resource directories cannot be symlinks")
    provider.mkdir(state, parents=True, exist_ok=True)
    owner = state / (prefix + uuid.uuid4().hex)
    provider.mkdir(owner, exist_ok=False)
    try:
        yield owner
    finally:
        shutil.rmtree(owner)
