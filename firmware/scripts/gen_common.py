# Shared helpers for the PlatformIO pre-build scripts in this folder.
import os


def c_string(text):
    """C string literal; every byte that isn't plain printable ASCII becomes an
    octal escape (an octal escape never swallows the next character)."""
    out = []
    for b in text.encode("utf-8"):
        c = chr(b)
        out.append(c if 0x20 <= b < 0x7F and c not in '\\"?' else "\\%03o" % b)
    return '"' + "".join(out) + '"'


def write_if_changed(path, content, mode=None):
    """Rewrites `path` (text or bytes) only when its content changes, so builds
    stay incremental."""
    binary = isinstance(content, bytes)
    if os.path.exists(path):
        with open(path, "rb" if binary else "r", encoding=None if binary else "utf-8") as f:
            if f.read() == content:
                return
    with open(path, "wb" if binary else "w", encoding=None if binary else "utf-8") as f:
        f.write(content)
    if mode is not None:
        os.chmod(path, mode)
