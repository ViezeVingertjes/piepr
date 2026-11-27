#include "lang_pick.h"

#define WEIGHT_MAX  1000 /* q=1, in thousandths */
#define WEIGHT_NONE (-1)
#define Q_DECIMALS  3

static bool is_space(char c)
{
    return c == ' ' || c == '\t';
}

static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static void trim(const char **s, const char **end)
{
    while (*s < *end && is_space(**s)) {
        (*s)++;
    }
    while (*end > *s && is_space((*end)[-1])) {
        (*end)--;
    }
}

/* A weight "0", "0.5", "1.000" as thousandths, or WEIGHT_NONE when it isn't one. */
static int parse_weight(const char *s, const char *end)
{
    trim(&s, &end);
    if (s == end || (*s != '0' && *s != '1')) {
        return WEIGHT_NONE;
    }
    int weight = (*s++ - '0') * WEIGHT_MAX;
    if (s == end) {
        return weight;
    }
    if (*s++ != '.' || end - s > Q_DECIMALS) {
        return WEIGHT_NONE;
    }
    for (int place = WEIGHT_MAX / 10; s < end; s++, place /= 10) {
        if (*s < '0' || *s > '9') {
            return WEIGHT_NONE;
        }
        weight += (*s - '0') * place;
    }
    return weight <= WEIGHT_MAX ? weight : WEIGHT_NONE;
}

/* The weight of one list element's parameters (";q=0.8"); 1 when it has none. */
static int element_weight(const char *s, const char *end)
{
    int weight = WEIGHT_MAX;
    while (s < end) {
        const char *param = s;
        while (s < end && *s != ';') {
            s++;
        }
        const char *param_end = s;
        if (s < end) {
            s++; /* past the ';' */
        }
        trim(&param, &param_end);
        if (param < param_end && lower(*param) == 'q') {
            const char *value = param + 1;
            while (value < param_end && is_space(*value)) {
                value++;
            }
            if (value < param_end && *value == '=') {
                weight = parse_weight(value + 1, param_end);
            }
        }
    }
    return weight;
}

/* True when the tag in [s, end) is `lang` itself or `lang` plus subtags ("nl-BE"). */
static bool is_language(const char *s, const char *end, const char *lang)
{
    while (*lang != '\0') {
        if (s == end || lower(*s++) != *lang++) {
            return false;
        }
    }
    return s == end || *s == '-';
}

bool lang_prefers_dutch(const char *accept_language, size_t len)
{
    if (accept_language == NULL || len == 0) {
        return false;
    }
    int dutch = WEIGHT_NONE, english = WEIGHT_NONE;
    int dutch_at = 0, english_at = 0; /* which list element set that weight */

    const char *s = accept_language;
    const char *header_end = accept_language + len;
    for (int at = 0; s < header_end; at++) {
        const char *element = s;
        while (s < header_end && *s != ',') {
            s++;
        }
        const char *element_end = s;
        if (s < header_end) {
            s++; /* past the ',' */
        }

        const char *tag_end = element;
        while (tag_end < element_end && *tag_end != ';') {
            tag_end++;
        }
        int weight = tag_end < element_end ? element_weight(tag_end + 1, element_end) : WEIGHT_MAX;
        const char *tag = element;
        trim(&tag, &tag_end);

        if (weight > dutch && is_language(tag, tag_end, "nl")) {
            dutch = weight;
            dutch_at = at;
        } else if (weight > english && is_language(tag, tag_end, "en")) {
            english = weight;
            english_at = at;
        }
    }
    if (dutch <= 0) {
        return false;
    }
    return dutch > english || (dutch == english && dutch_at < english_at);
}
