"""Tests for web_minify.py. Run: python3 -m unittest discover -s firmware/scripts"""
import unittest

from web_minify import minify_css, minify_html, minify_js


class Css(unittest.TestCase):
    def test_comments_and_spare_whitespace_go(self):
        self.assertEqual(minify_css("/* tokens */\na {\n  color: red;\n  margin: 0 auto;\n}\n"),
                         "a{color:red;margin:0 auto}")

    def test_a_list_of_selectors_is_joined(self):
        self.assertEqual(minify_css("h1,\nh2 { margin: 0 }"), "h1,h2{margin:0}")

    def test_strings_are_left_alone(self):
        css = 'a{font:1rem "Segoe UI", x;background:url("data:a; b , { c } /* d */")}'
        self.assertEqual(minify_css(css), 'a{font:1rem "Segoe UI",x;background:url("data:a; b , { c } /* d */")}')

    def test_a_space_that_means_something_stays(self):
        # A descendant selector, the operators in calc() and a media query.
        self.assertEqual(minify_css("a :hover { width: calc(1rem + 1px) }"), "a :hover{width:calc(1rem + 1px)}")
        self.assertEqual(minify_css("@media (prefers-color-scheme: dark) {\n  :root { --a: 1 }\n}"),
                         "@media (prefers-color-scheme: dark){:root{--a:1}}")

    def test_a_selector_inside_a_media_query_keeps_its_space(self):
        self.assertEqual(minify_css("@media print { a :hover { color: red } }"), "@media print{a :hover{color:red}}")

    def test_a_pseudo_class_keeps_its_colon(self):
        self.assertEqual(minify_css(".a:not(.b) + .c > i:nth-child(2) { x: y }"), ".a:not(.b) + .c > i:nth-child(2){x:y}")


class Js(unittest.TestCase):
    def test_comments_go(self):
        self.assertEqual(minify_js("/* one */\nvar a = 1; // two\n/* three\n   lines */\nvar b = 2;\n"),
                         "var a=1;\nvar b=2;")

    def test_a_comment_between_two_words_leaves_a_space(self):
        self.assertEqual(minify_js("return/* x */a;"), "return a;")

    def test_strings_are_left_alone(self):
        for text in ('"http://piepr.local"', "'/* no */ // no'", '"a  \\" b"', "'it\\'s  x'", '"[[Too long: max ]]"'):
            self.assertEqual(minify_js("var s = " + text + ";"), "var s=" + text + ";")

    def test_a_template_string_is_left_alone(self):
        text = "`a  ${ b + `c  ${d}` }  // e`"
        self.assertEqual(minify_js("var s = " + text + ";"), "var s=" + text + ";")

    def test_a_regular_expression_is_left_alone(self):
        self.assertEqual(minify_js("var r = /a\\/b [\"'/] /g;"), "var r=/a\\/b [\"'/] /g;")
        self.assertEqual(minify_js("return /  /.test(s);"), "return/  /.test(s);")

    def test_a_regular_expression_after_a_word_on_the_line_is_left_alone(self):
        self.assertEqual(minify_js("if (a) return 1; else return /a  b/.test(s);"), "if(a)return 1;else return/a  b/.test(s);")
        self.assertEqual(minify_js("x = 1\nreturn /a  b/"), "x=1\nreturn/a  b/")

    def test_a_division_after_a_count_is_not_a_regular_expression(self):
        self.assertEqual(minify_js('x = i++ / 2; y = a / b; s = "c  d";'), 'x=i++/2;y=a/b;s="c  d";')
        self.assertEqual(minify_js("x = i-- / 2 + 'a / b';"), "x=i--/2+'a / b';")

    def test_a_division_after_a_long_indent_is_not_a_regular_expression(self):
        self.assertEqual(minify_js("var ratio = width\n" + " " * 40 + "/ height / 2;"), "var ratio=width\n/height/2;")
        self.assertEqual(minify_js("x = a" + " " * 40 + "/ b / c;"), "x=a/b/c;")

    def test_division_is_not_a_regular_expression(self):
        self.assertEqual(minify_js('var x = a / b / c; var s = "d";'), 'var x=a/b/c;var s="d";')
        self.assertEqual(minify_js("var x = (a + b) / 2 + f(c) / 3; // d"), "var x=(a+b)/2+f(c)/3;")

    def test_signs_that_would_merge_keep_their_space(self):
        self.assertEqual(minify_js("x = a + +b;"), "x=a+ +b;")
        self.assertEqual(minify_js("x = a - -b;"), "x=a- -b;")
        self.assertEqual(minify_js("x = a++ + b;"), "x=a++ +b;")
        self.assertEqual(minify_js("x = a + ++b;"), "x=a+ ++b;")
        self.assertEqual(minify_js("x = a / /b/.source;"), "x=a/ /b/.source;")

    def test_words_keep_the_space_between_them(self):
        code = "var a = typeof b; if (a in c) { return new D(); } else if (e instanceof F) { throw e; }"
        self.assertEqual(minify_js(code), "var a=typeof b;if(a in c){return new D();}else if(e instanceof F){throw e;}")

    def test_a_number_keeps_its_space_before_a_dot(self):
        self.assertEqual(minify_js("var s = 1 .toFixed(2) + a . b;"), "var s=1 .toFixed(2)+a . b;")

    def test_operators_lose_their_spaces(self):
        self.assertEqual(minify_js("a = b ? c : d || e && !f; g = h <= 2 * i % 3;"), "a=b?c:d||e&&!f;g=h<=2*i%3;")

    def test_line_breaks_stay_and_lines_are_trimmed(self):
        self.assertEqual(minify_js("function f() {\n    var a = 1\n\n    return a\n  }\n"),
                         "function f(){\nvar a=1\nreturn a\n}")

    def test_non_ascii_text_in_a_string_is_left_alone(self):
        self.assertEqual(minify_js('s = "Kon “" + a + "” niet";'), 's="Kon “"+a+"” niet";')

    def test_a_string_that_never_ends_stops_the_build(self):
        with self.assertRaises(ValueError):
            minify_js('var s = "abc;\nvar t = 1;')


class Html(unittest.TestCase):
    def test_lines_are_trimmed_and_blank_lines_dropped(self):
        self.assertEqual(minify_html("<main>\n\n   <p>a  b</p>\n</main>\n"), "<main>\n<p>a  b</p>\n</main>")

    def test_comments_go(self):
        self.assertEqual(minify_html("<p>a</p>\n<!-- the\n  rest -->\n<p>b</p> <!-- c -->"), "<p>a</p>\n<p>b</p>")

    def test_style_and_script_are_minified(self):
        page = "<style>\n  a { color: red }\n</style>\n<p>x</p>\n<script>\n  var a = 1; // one\n</script>"
        self.assertEqual(minify_html(page), "<style>a{color:red}</style>\n<p>x</p>\n<script>var a=1;</script>")

    def test_a_comment_inside_a_script_string_stays(self):
        self.assertEqual(minify_html('<script>var s = "<!-- x -->";</script>'), '<script>var s="<!-- x -->";</script>')

    def test_marked_text_is_left_alone(self):
        self.assertEqual(minify_html("<p>[[Find it at]] <b>x</b>[[, port]]</p>"),
                         "<p>[[Find it at]] <b>x</b>[[, port]]</p>")


if __name__ == "__main__":
    unittest.main()
