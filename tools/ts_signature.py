import os
import re
import subprocess
import sys
import zlib

PREFIX = "speeduino mazduino-iox"
SIG_LINE = re.compile(r'^(\s*signature\s*=\s*)"[^"]*"', re.M)


def version(root):
    ref = os.environ.get("GITHUB_REF_NAME", "")
    if ref.startswith("v"):
        return ref[1:]
    try:
        tag = subprocess.check_output(
            ["git", "describe", "--tags", "--abbrev=0"], cwd=root, stderr=subprocess.DEVNULL
        ).decode().strip()
        return tag[1:] if tag.startswith("v") else tag
    except Exception:
        return "dev"


def ini_hash(text):
    normalized = SIG_LINE.sub(r'\1"@@SIGNATURE@@"', text)
    return zlib.crc32(normalized.encode("utf-8")) & 0xFFFFFFFF


def signature(root):
    ini = os.path.join(root, "tunerstudio", "mazduino-iox.ini")
    header_path = os.path.join(root, "include", "Signature.h")
    with open(ini, encoding="utf-8") as f:
        text = f.read()
    sig = "%s %s.%d" % (PREFIX, version(root), ini_hash(text))
    updated = SIG_LINE.sub(r'\1"%s"' % sig, text, count=1)
    if updated != text:
        with open(ini, "w", encoding="utf-8") as f:
            f.write(updated)
    header = '#pragma once\n#define TS_SIGNATURE "%s"\n' % sig
    if not os.path.exists(header_path) or open(header_path).read() != header:
        os.makedirs(os.path.dirname(header_path), exist_ok=True)
        with open(header_path, "w") as f:
            f.write(header)
    return sig


def ini_path(sig):
    return sig.lower().replace(" ", "/").replace(".", "/") + ".ini"


if __name__ == "__main__":
    sig = signature(os.path.dirname(os.path.dirname(os.path.abspath(sys.argv[0]))))
    if len(sys.argv) > 1 and sys.argv[1] == "--path":
        print(ini_path(sig))
    else:
        print(sig)
else:
    try:
        Import("env")
        print("TS signature: " + signature(env.subst("$PROJECT_DIR")))
    except NameError:
        pass
