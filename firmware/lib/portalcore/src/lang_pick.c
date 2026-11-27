#include "lang_pick.h"

#include <string.h>

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

/* Where the piece starting at `s` ends: at the next `delimiter`, or at `end`. */
static const char *piece_end(const char *s, const char *end, char delimiter)
{
    const char *found = memchr(s, delimiter, (size_t)(end - s));
    return found != NULL ? found : end;
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
        const char *param_end = piece_end(s, end, ';');
        s = param_end < end ? param_end + 1 : end;

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
    /* The highest weight seen for Dutch or English, and which of the two it was.
     * Only a higher weight replaces it, so of two equal ones the first listed wins. */
    int best = WEIGHT_NONE;
    bool best_is_dutch = false;

    const char *s = accept_language;
    const char *header_end = accept_language + len;
    while (s < header_end) {
        const char *tag = s;
        const char *element_end = piece_end(s, header_end, ',');
        s = element_end < header_end ? element_end + 1 : header_end;

        const char *tag_end = piece_end(tag, element_end, ';');
        int weight = tag_end < element_end ? element_weight(tag_end + 1, element_end) : WEIGHT_MAX;
        trim(&tag, &tag_end);

        bool dutch = is_language(tag, tag_end, "nl");
        if (weight > best && (dutch || is_language(tag, tag_end, "en"))) {
            best = weight;
            best_is_dutch = dutch;
        }
    }
    return best_is_dutch && best > 0;
}

lang_choice_t lang_from_query(const char *query, size_t len)
{
    static const char KEY[] = "lang=";
    const size_t key_len = sizeof(KEY) - 1;
    if (query == NULL) {
        return LANG_UNSET;
    }
    const char *end = query + len;
    for (const char *param = query; param < end;) {
        const char *param_end = piece_end(param, end, '&');
        size_t param_len = (size_t)(param_end - param);
        if (param_len >= key_len && memcmp(param, KEY, key_len) == 0) {
            /* The first "lang=" decides, also when it names another language. */
            if (param_len == key_len + 2 && memcmp(param + key_len, "nl", 2) == 0) {
                return LANG_DUTCH;
            }
            if (param_len == key_len + 2 && memcmp(param + key_len, "en", 2) == 0) {
                return LANG_ENGLISH;
            }
            return LANG_UNSET;
        }
        param = param_end < end ? param_end + 1 : end;
    }
    return LANG_UNSET;
}
