#include "json_writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "json_escape.h"

void json_writer_init(json_writer_t *w, char *out, size_t out_size)
{
    *w = (json_writer_t){ .buf = out, .size = out_size, .overflow = out_size == 0 };
}

void json_put(json_writer_t *w, const char *fmt, ...)
{
    if (w->overflow) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(&w->buf[w->len], w->size - w->len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= w->size - w->len) {
        w->overflow = true;
    } else {
        w->len += (size_t)n;
    }
}

void json_put_string(json_writer_t *w, const char *s, size_t max_len)
{
    const char *nul = memchr(s, '\0', max_len);
    size_t len = nul != NULL ? (size_t)(nul - s) : max_len;
    json_put(w, "\"");
    if (w->overflow || !json_escape(s, len, &w->buf[w->len], w->size - w->len)) {
        w->overflow = true;
        return;
    }
    w->len += strlen(&w->buf[w->len]);
    json_put(w, "\"");
}

void json_key(json_writer_t *w, const char *key)
{
    bool first = w->len == 0 || w->buf[w->len - 1] == '{';
    json_put(w, "%s\"%s\":", first ? "" : ",", key);
}

size_t json_writer_finish(json_writer_t *w)
{
    if (w->overflow) {
        if (w->size > 0) {
            w->buf[0] = '\0';
        }
        return 0;
    }
    return w->len;
}
