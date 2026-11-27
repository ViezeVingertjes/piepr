# PlatformIO pre-build script: secrets.ini -> src/secrets_gen.h
#   setup_pass: password of the setup hotspot, 8-63 bytes (WiFi itself is set up
#               from a phone and saved on the device)
# Read directly (no SCons flag parsing), so any characters are allowed.
import os
import sys

Import("env")  # noqa: F821  (provided by PlatformIO)
sys.path.insert(0, env.subst("$PROJECT_DIR/scripts"))  # noqa: F821
from gen_common import c_string, write_if_changed  # noqa: E402

PROJECT = env.subst("$PROJECT_DIR")  # noqa: F821
EXAMPLE_SETUP_PASS = "CHOOSE_A_SETUP_PASSWORD"
SECRETS = os.path.join(PROJECT, "secrets.ini")


def read_secrets(path):
    if not os.path.exists(path):
        raise SystemExit("secrets.ini missing: copy secrets.ini.example and fill it in")
    values = {}
    with open(path, encoding="utf-8") as f:
        for line in f.read().splitlines():
            if "=" not in line or line.lstrip().startswith(("#", ";", "[")):
                continue
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    setup_pass = values.get("setup_pass", "")
    if setup_pass == EXAMPLE_SETUP_PASS:
        raise SystemExit("secrets.ini: choose your own setup_pass (the example value is public)")
    if not 8 <= len(setup_pass.encode("utf-8")) <= 63:
        raise SystemExit("secrets.ini: setup_pass must be 8-63 bytes (WPA2 requirement)")
    return values


v = read_secrets(SECRETS)
write_if_changed(
    os.path.join(PROJECT, "src", "secrets_gen.h"),
    "/* Generated from secrets.ini by scripts/gen_secrets.py - do not edit or commit. */\n"
    "#pragma once\n"
    "#define SETUP_PASS " + c_string(v["setup_pass"]) + "\n",
    mode=0o600,
)
