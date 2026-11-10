"""Provide owned, disposable resource state for manual build verification."""
from contextlib import contextmanager
import errno
import os
from pathlib import Path
import shutil
import sys


def provider():
    provider_dir = os.environ.get('IIFILEPROVIDER_PYTHON_DIRECTORY') or str(
        Path.home() / '.local/SDK/iiFileProvider/share/iiFileProvider/python')
    if provider_dir not in sys.path:
        sys.path.insert(0, provider_dir)
    import ii_file_provider
    return ii_file_provider


def remove_owned_tree(path):
    """Remove a fresh verification build, including late provider sidecars."""
    path = Path(path)
    for attempt in range(3):
        if path.is_symlink():
            path.unlink()
            return
        if not path.is_dir():
            return
        try:
            shutil.rmtree(path)
            return
        except OSError as error:
            if error.errno != errno.ENOTEMPTY or attempt == 2:
                raise


@contextmanager
def temporary(project, prefix):
    if not prefix or prefix in ('.', '..') or '/' in prefix or '\\' in prefix:
        raise ValueError('Verification workspace prefix must be a single name')
    files = provider()
    resources = Path(project).resolve(strict=True) / 'resources'
    state = resources / '.state'
    verification = state / 'verification'
    if any(path.is_symlink() for path in (resources, state, verification)):
        raise ValueError('Verification resource directories cannot be symlinks')
    files.mkdir(verification, parents=True, exist_ok=True)
    with files.TemporaryDirectory(prefix=prefix, dir=verification) as directory:
        yield Path(directory)
