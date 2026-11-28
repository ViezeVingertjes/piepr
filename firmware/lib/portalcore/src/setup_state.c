#include "setup_state.h"

#include <string.h>

#include "json_writer.h"

/* Names the page can show and post back unchanged. */
static bool ssid_usable(const char *ssid, size_t len)
{
    return len > 0 && len <= WIFI_SSID_MAX_LEN && utf8_valid(ssid, len);
}

void setup_networks_add(setup_network_t *list, size_t *count, const char *ssid, size_t ssid_len,
                        int8_t rssi, bool secure)
{
    if (rssi < SETUP_SCAN_MIN_RSSI || !ssid_usable(ssid, ssid_len)) {
        return;
    }
    setup_network_t entry = { .rssi = rssi, .secure = secure };
    memcpy(entry.ssid, ssid, ssid_len);

    for (size_t i = 0; i < *count; i++) {
        if (strcmp(list[i].ssid, entry.ssid) == 0) {
            if (list[i].rssi >= rssi) {
                return;
            }
            memmove(&list[i], &list[i + 1], (*count - i - 1) * sizeof(list[0]));
            (*count)--;
            break;
        }
    }
    if (*count == SETUP_SCAN_LIST_MAX) {
        if (list[*count - 1].rssi >= rssi) {
            return;
        }
        (*count)--;
    }

    size_t pos = 0;
    while (pos < *count && list[pos].rssi >= rssi) {
        pos++;
    }
    memmove(&list[pos + 1], &list[pos], (*count - pos) * sizeof(list[0]));
    list[pos] = entry;
    (*count)++;
}

/* Average signal, rounded down: half a dB short of a limit doesn't pass it. */
static int8_t survey_average(const setup_survey_entry_t *e)
{
    int sum = e->rssi_sum;
    int n = e->scans;
    return (int8_t)(sum >= 0 ? sum / n : -((-sum + n - 1) / n));
}

void setup_survey_add(setup_survey_entry_t *survey, size_t *count, size_t max, uint8_t scan,
                      const char *ssid, size_t ssid_len, int8_t rssi, bool secure)
{
    if (!ssid_usable(ssid, ssid_len)) {
        return;
    }
    for (size_t i = 0; i < *count; i++) {
        setup_survey_entry_t *e = &survey[i];
        if (strlen(e->ssid) != ssid_len || memcmp(e->ssid, ssid, ssid_len) != 0) {
            continue;
        }
        e->secure = e->secure || secure; /* then a password is asked for */
        if (e->last_scan != scan) {
            e->last_scan = scan;
            e->scan_rssi = rssi;
            e->rssi_sum += rssi;
            e->scans++;
        } else if (rssi > e->scan_rssi) {
            e->rssi_sum += rssi - e->scan_rssi;
            e->scan_rssi = rssi;
        }
        return;
    }
    setup_survey_entry_t *e;
    if (*count < max) {
        e = &survey[(*count)++];
    } else {
        e = &survey[0];
        for (size_t i = 1; i < max; i++) {
            if (survey_average(&survey[i]) < survey_average(e)) {
                e = &survey[i];
            }
        }
        if (rssi <= survey_average(e)) {
            return;
        }
    }
    *e = (setup_survey_entry_t){
        .rssi_sum = rssi, .scans = 1, .last_scan = scan, .scan_rssi = rssi, .secure = secure
    };
    memcpy(e->ssid, ssid, ssid_len);
    e->ssid[ssid_len] = '\0';
}

void setup_survey_result(const setup_survey_entry_t *survey, size_t count, setup_network_t *list,
                         size_t *list_count)
{
    for (size_t i = 0; i < count; i++) {
        const setup_survey_entry_t *e = &survey[i];
        setup_networks_add(list, list_count, e->ssid, strlen(e->ssid), survey_average(e),
                           e->secure);
    }
}

const char *setup_reason_name(setup_reason_t reason)
{
    switch (reason) {
    case SETUP_REASON_NO_CONNECT:
        return "no_connect";
    case SETUP_REASON_AUTH:
        return "auth";
    case SETUP_REASON_NONE:
    default:
        return "none";
    }
}

size_t setup_state_json(setup_reason_t reason, const char *reason_ssid, const setup_network_t *nets,
                        size_t count, bool scanning, char *out, size_t out_size)
{
    json_writer_t w;
    json_writer_init(&w, out, out_size);
    json_put(&w, "{\"reason\":\"%s\",\"reason_ssid\":", setup_reason_name(reason));
    json_put_string(&w, reason_ssid != NULL ? reason_ssid : "", WIFI_SSID_MAX_LEN);
    json_put(&w, ",\"scanning\":%s,\"networks\":[", scanning ? "true" : "false");
    for (size_t i = 0; i < count; i++) {
        json_put(&w, "%s{\"ssid\":", i > 0 ? "," : "");
        json_put_string(&w, nets[i].ssid, WIFI_SSID_MAX_LEN);
        json_put(&w, ",\"rssi\":%d,\"secure\":%s}", nets[i].rssi,
                 nets[i].secure ? "true" : "false");
    }
    json_put(&w, "]}");
    return json_writer_finish(&w);
}
