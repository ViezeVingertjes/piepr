/* libFuzzer harness: arbitrary form bodies into wifi_form_parse. Run: fuzz/run.sh wifi_form */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "wifi_form.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    wifi_credentials_t c;
    memset(&c, 0, sizeof(c));
    if (wifi_form_parse((const char *)data, size, &c) == WIFI_FORM_OK) {
        size_t ssid_len = strnlen(c.ssid, sizeof(c.ssid));
        size_t pass_len = strnlen(c.pass, sizeof(c.pass));
        assert(ssid_len >= 1 && ssid_len <= WIFI_SSID_MAX_LEN);
        assert(pass_len == 0 || (pass_len >= WIFI_PASS_MIN_LEN && pass_len <= WIFI_PASS_MAX_LEN));
    }
    return 0;
}
