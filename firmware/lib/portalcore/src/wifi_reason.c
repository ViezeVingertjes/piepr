#include "wifi_reason.h"

#include <stddef.h>

const char *wifi_reason_text(unsigned reason)
{
    switch (reason) {
    case WIFI_REASON_TEXT_NO_AP_FOUND:
        return "network not found";
    case WIFI_REASON_TEXT_AUTH_FAIL:
        return "password rejected";
    case WIFI_REASON_TEXT_4WAY_TIMEOUT:
    case WIFI_REASON_TEXT_HANDSHAKE_TIMEOUT:
        return "password check timed out";
    case WIFI_REASON_TEXT_BEACON_TIMEOUT:
        return "access point stopped answering";
    case WIFI_REASON_TEXT_ASSOC_FAIL:
        return "access point refused the connection";
    case WIFI_REASON_TEXT_AUTH_EXPIRE:
        return "sign-in expired";
    case WIFI_REASON_TEXT_ASSOC_LEAVE:
        return "left the network";
    case WIFI_REASON_TEXT_CONNECTION_FAIL:
        return "could not connect";
    default:
        return NULL;
    }
}
