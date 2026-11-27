# PlatformIO pre-build script: web/ -> src/web_pages_gen.inc, the setup pages as
# C arrays (gzipped, in English and Dutch) for src/setup_portal.c. The work is
# done by web_pages.py; a problem in web/ stops the build with its message.
import os
import sys

Import("env")  # noqa: F821  (provided by PlatformIO)
sys.path.insert(0, env.subst("$PROJECT_DIR/scripts"))  # noqa: F821
from gen_common import write_if_changed  # noqa: E402
from web_pages import PageError, build  # noqa: E402

PROJECT = env.subst("$PROJECT_DIR")  # noqa: F821
BYTES_PER_LINE = 16


def c_array(name, data):
    rows = [
        "    " + " ".join("0x%02x," % byte for byte in data[i : i + BYTES_PER_LINE])
        for i in range(0, len(data), BYTES_PER_LINE)
    ]
    return "static const uint8_t %s[] = {\n%s\n};\n" % (name, "\n".join(rows))


try:
    pages = build(os.path.join(PROJECT, "web"))
except PageError as error:
    raise SystemExit(str(error))

out = ["/* Generated from web/ by scripts/gen_web.py - do not edit or commit. */\n"]
for name, versions in pages.items():
    out.append(c_array("PAGE_%s_en" % name, versions["en"]))
    out.append(c_array("PAGE_%s_nl" % name, versions["nl"]))
    out.append(
        "static const localized_page_t PAGE_%s = {\n"
        "    { PAGE_%s_en, sizeof(PAGE_%s_en) },\n"
        "    { PAGE_%s_nl, sizeof(PAGE_%s_nl) },\n"
        "};\n" % ((name,) * 5)
    )
write_if_changed(os.path.join(PROJECT, "src", "web_pages_gen.inc"), "\n".join(out))
