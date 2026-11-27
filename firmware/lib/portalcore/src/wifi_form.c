#include "wifi_form.h"

#include <stdbool.h>
#include <string.h>

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/* URL-decodes src[0..len) into dst (capacity dst_size incl. NUL).
 * Returns the decoded length, -1 on a malformed escape or NUL byte, or
 * -2 when the result does not fit. */
static int url_decode(const char *src, size_t len, char *dst, size_t dst_size)
{
    size_t out = 0;
    for (size_t i = 0; i < len; i++) {
        char ch = src[i];
        if (ch == '+') {
            ch = ' ';
        } else if (ch == '%') {
            if (i + 2 >= len) { /* needs two more characters */
                return -1;
            }
            int hi = hex_value(src[i + 1]);
            int lo = hex_value(src[i + 2]);
            if (hi < 0 || lo < 0) {
                return -1;
            }
            ch = (char)(hi << 4 | lo);
            i += 2;
        }
        if (ch == '\0') {
            return -1;
        }
        if (out + 1 >= dst_size) {
            return -2;
        }
        dst[out++] = ch;
    }
    dst[out] = '\0';
    return (int)out;
}

typedef struct {
    const char *value;
    size_t len;
    bool found;
} field_t;

wifi_form_result_t wifi_form_parse(const char *body, size_t len, wifi_credentials_t *out)
{
    field_t ssid = { 0 };
    field_t pass = { 0 };

    /* Split "k=v&k=v" into raw (still encoded) fields. */
    size_t pos = 0;
    while (pos < len) {
        const char *pair = body + pos;
        const char *amp = memchr(pair, '&', len - pos);
        size_t pair_len = amp ? (size_t)(amp - pair) : len - pos;
        pos += pair_len + 1;

        const char *eq = memchr(pair, '=', pair_len);
        if (eq == NULL) {
            continue;
        }
        size_t key_len = (size_t)(eq - pair);
        field_t *field = NULL;
        if (key_len == 4 && memcmp(pair, "ssid", 4) == 0) {
            field = &ssid;
        } else if (key_len == 4 && memcmp(pair, "pass", 4) == 0) {
            field = &pass;
        } else {
            continue;
        }
        if (field->found) {
            return WIFI_FORM_MALFORMED;
        }
        field->found = true;
        field->value = eq + 1;
        field->len = pair_len - key_len - 1;
    }
    if (!ssid.found || !pass.found) {
        return WIFI_FORM_MALFORMED;
    }

    /* Decode into temporaries one byte larger than allowed, so "too long"
     * can be told apart from "malformed". */
    char ssid_buf[WIFI_SSID_MAX_LEN + 2];
    char pass_buf[WIFI_PASS_MAX_LEN + 2];
    int ssid_len = url_decode(ssid.value, ssid.len, ssid_buf, sizeof(ssid_buf));
    int pass_len = url_decode(pass.value, pass.len, pass_buf, sizeof(pass_buf));
    if (ssid_len == -1 || pass_len == -1) {
        return WIFI_FORM_MALFORMED;
    }
    if (ssid_len < 1 || ssid_len > WIFI_SSID_MAX_LEN) { /* -2 = too long */
        return WIFI_FORM_BAD_SSID;
    }
    if (pass_len == -2 ||
        (pass_len != 0 && (pass_len < WIFI_PASS_MIN_LEN || pass_len > WIFI_PASS_MAX_LEN))) {
        return WIFI_FORM_BAD_PASS;
    }

    memcpy(out->ssid, ssid_buf, (size_t)ssid_len + 1);
    memcpy(out->pass, pass_buf, (size_t)pass_len + 1);
    return WIFI_FORM_OK;
}
