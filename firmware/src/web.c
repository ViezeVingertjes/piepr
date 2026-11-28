#include "web.h"

#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "esp_log.h"
#include "lang_pick.h"

#include "web_pages_gen.inc"

#define ACCEPT_LANGUAGE_MAX 96 /* the start of the header is what decides */
#define LANG_QUERY_MAX      16 /* "lang=nl"; a longer query is none of ours */

esp_err_t web_send(httpd_req_t *req, const char *status, const char *type, const void *body,
                   size_t len)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, (ssize_t)len);
}

static bool wants_dutch(httpd_req_t *req)
{
    char query[LANG_QUERY_MAX] = { 0 };
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        lang_choice_t chosen = lang_from_query(query, strnlen(query, sizeof(query)));
        if (chosen != LANG_UNSET) {
            return chosen == LANG_DUTCH;
        }
    }
    char header[ACCEPT_LANGUAGE_MAX] = { 0 };
    esp_err_t err = httpd_req_get_hdr_value_str(req, "Accept-Language", header, sizeof(header));
    if (err != ESP_OK && err != ESP_ERR_HTTPD_RESULT_TRUNC) {
        return false;
    }
    return lang_prefers_dutch(header, strnlen(header, sizeof(header)));
}

esp_err_t web_send_page(httpd_req_t *req, const char *status, const web_page_t *page)
{
    const web_blob_t *blob = wants_dutch(req) ? &page->dutch : &page->english;
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return web_send(req, status, "text/html; charset=utf-8", blob->data, blob->len);
}

esp_err_t web_start(httpd_config_t *config, const httpd_uri_t *uris, size_t count)
{
    /* Browsers and phones drop connections all the time: the web server's
     * warnings about that are not worth a line in the log. */
    static const char *const SERVER_TAGS[] = { "httpd_txrx", "httpd_parse", "httpd_sess",
                                               "httpd_uri" };
    for (size_t i = 0; i < sizeof(SERVER_TAGS) / sizeof(SERVER_TAGS[0]); i++) {
        esp_log_level_set(SERVER_TAGS[i], ESP_LOG_ERROR);
    }

    config->server_port = HTTP_PORT;
    httpd_handle_t server;
    esp_err_t err = httpd_start(&server, config);
    for (size_t i = 0; err == ESP_OK && i < count; i++) {
        err = httpd_register_uri_handler(server, &uris[i]);
        if (err != ESP_OK) {
            httpd_stop(server); /* not half a server */
        }
    }
    return err;
}
