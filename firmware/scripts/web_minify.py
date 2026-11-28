# Makes a page smaller without changing what it does: comments and spare
# whitespace go, in the HTML and in the CSS and JavaScript inside it. Nothing is
# renamed and no JavaScript lines are joined, so the result reads like the source
# with the air let out. Plain Python, used by web_pages.py, tested in
# test_web_minify.py.
#
# What it doesn't know: a regular expression written directly after ")", "]", "}",
# "++" or "--" is read as a division. Put such an expression in a variable.
import re

# --- CSS -------------------------------------------------------------------

_STRING = re.compile(r'"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'', re.S)
_CSS_PIECE = re.compile(r"/\*.*?\*/|" + _STRING.pattern, re.S)


def _css_code(code):
    code = re.sub(r"\s+", " ", code)
    code = re.sub(r" ?([{};,]) ?", r"\1", code)
    # "color: red" inside a rule; never a selector such as "a :hover".
    code = re.sub(r"([{;][\w-]+): ", r"\1:", code)
    return code.replace(";}", "}")


def minify_css(text):
    # A comment becomes a space; then every piece that isn't a string is code.
    text = _CSS_PIECE.sub(lambda piece: " " if piece.group().startswith("/*") else piece.group(), text)
    pieces = re.split("(%s)" % _STRING.pattern, text, flags=re.S)
    return "".join(piece if i % 2 else _css_code(piece) for i, piece in enumerate(pieces)).strip()


# --- JavaScript ------------------------------------------------------------

_WORD = re.compile(r"[A-Za-z0-9_$\u0080-￿]")
_SPACES = " \t\r\f\v"
# After one of these words a "/" starts a regular expression, not a division.
_BEFORE_A_REGEX = {
    "return", "typeof", "instanceof", "in", "of", "new", "delete", "void", "throw", "case", "do",
    "else", "yield", "await",
}  # fmt: skip
_MERGING = "+-/"  # "+ +" is not "++", "/ /" is not a comment


def _end_of_string(text, start):
    string = _STRING.match(text, start)
    if not string:
        raise ValueError("JavaScript: a string that never ends: %s" % text[start : start + 40])
    return string.end()


def _end_of_template(text, start):
    i = start + 1
    while i < len(text):
        if text[i] == "\\":
            i += 2
        elif text[i] == "`":
            return i + 1
        elif text.startswith("${", i):
            i = _end_of_placeholder(text, i + 2)
        else:
            i += 1
    raise ValueError("JavaScript: a template string that never ends: %s" % text[start : start + 40])


def _end_of_placeholder(text, start):
    depth = 1
    i = start
    while i < len(text):
        c = text[i]
        if c in "\"'":
            i = _end_of_string(text, i)
        elif c == "`":
            i = _end_of_template(text, i)
        else:
            depth += (c == "{") - (c == "}")
            i += 1
            if depth == 0:
                return i
    raise ValueError("JavaScript: a ${...} that never ends")


def _end_of_regex(text, start):
    in_class = False
    i = start + 1
    while i < len(text) and text[i] != "\n":
        c = text[i]
        if c == "\\":
            i += 2
            continue
        if c == "[":
            in_class = True
        elif c == "]":
            in_class = False
        elif c == "/" and not in_class:
            return i + 1
        i += 1
    raise ValueError("JavaScript: a regular expression that never ends: %s" % text[start : start + 40])


_CODE_BEFORE = 16  # how much code before a "/" says what it is


def _regex_may_start(pieces):
    """Whether a "/" after these pieces starts a regular expression, not a division."""
    before = []  # the code just before, backwards: an atom as "x", any whitespace as one space
    for kind, piece in reversed(pieces):
        if kind != "space":
            before.append("x" if kind == "atom" else piece)
        elif before and before[-1] != " ":
            before.append(" ")
        if len(before) >= _CODE_BEFORE:
            break
    code_before = "".join(reversed(before)).strip()
    if not code_before:
        return True
    word = re.search(r"[A-Za-z0-9_$]+$", code_before)
    if word:
        return word.group() in _BEFORE_A_REGEX
    # After a value (")", "]", "}", "i++") it divides.
    return code_before[-1] not in ")]}" and not code_before.endswith(("++", "--"))


def _js_pieces(text):
    """`text` as (kind, piece): "code", "space" (also where a comment was) or "atom"
    (a string, template string or regular expression, never to be touched)."""
    pieces = []
    i = 0
    while i < len(text):
        c = text[i]
        if text.startswith("//", i):
            end = text.find("\n", i)
            i = len(text) if end < 0 else end
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            if end < 0:
                raise ValueError("JavaScript: a comment that never ends: %s" % text[i : i + 40])
            pieces.append(("space", "\n" if "\n" in text[i:end] else " "))
            i = end + 2
        elif c in "\"'`" or (c == "/" and _regex_may_start(pieces)):
            end = (
                _end_of_template(text, i) if c == "`"
                else _end_of_regex(text, i) if c == "/"
                else _end_of_string(text, i)
            )  # fmt: skip
            pieces.append(("atom", text[i:end]))
            i = end
        elif c in _SPACES or c == "\n":
            pieces.append(("space", c))
            i += 1
        else:
            pieces.append(("code", c))
            i += 1
    return pieces


def _space_needed(before, after):
    if not before or not after or before == "\n" or after == "\n":
        return False
    if _WORD.match(before) and _WORD.match(after):
        return True
    if before == after and before in _MERGING:
        return True
    return "." in (before, after)


def minify_js(text):
    out = []  # the result, piece by piece
    gap = ""  # whitespace seen since the last piece: "", " " or "\n"
    for kind, piece in _js_pieces(text):
        if kind == "space":
            gap = "\n" if "\n" in (gap, piece) else " "
            continue
        if gap == "\n":
            if out:
                out.append("\n")
        elif gap and _space_needed(out[-1][-1] if out else "", piece[0]):
            out.append(" ")
        gap = ""
        out.append(piece)
    return "".join(out)


# --- HTML ------------------------------------------------------------------

_BLOCK = re.compile(r"(<(script|style)\b[^>]*>)(.*?)(</\2>)", re.S | re.I)
_COMMENT = re.compile(r"<!--.*?-->", re.S)
_HELD = "\x00%d\x00"  # stands in for a block while the markup around it is done
_HELD_BLOCK = re.compile(_HELD.replace("%d", r"(\d+)"))


def minify_html(text):
    """`text` with its comments, indentation and blank lines gone, and what is
    inside <style> and <script> minified. The page may not contain <pre>."""
    blocks = []

    def hold(block):
        body = minify_css if block.group(2).lower() == "style" else minify_js
        blocks.append(block.group(1) + body(block.group(3)) + block.group(4))
        return _HELD % (len(blocks) - 1)

    lines = (line.strip() for line in _COMMENT.sub("", _BLOCK.sub(hold, text)).splitlines())
    markup = "\n".join(line for line in lines if line)
    return _HELD_BLOCK.sub(lambda held: blocks[int(held.group(1))], markup)
