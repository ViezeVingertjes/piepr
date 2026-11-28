#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_http_server.h"

/*
 * What the setup portal and the status page share: the pages of web/ and how
 * they are sent. A page is in the firmware gzipped, once in English and once in
 * Dutch.
 */

typedef struct {
    const uint8_t *data;
    size_t len;
} web_blob_t;

typedef struct {
    web_blob_t english;
    web_blob_t dutch;
} web_page_t;

/* One WEB_PAGE_<NAME> per page in web/, written at build time (scripts/gen_web.py). */
#include "web_pages_gen.h"

/* Starts a web server on HTTP_PORT with these handlers; `config` holds what is
 * particular to it. On an error nothing is left running. */
esp_err_t web_start(httpd_config_t *config, const httpd_uri_t *uris, size_t count);

/* Sends `body` with this status and content type, never to be cached. */
esp_err_t web_send(httpd_req_t *req, const char *status, const char *type, const void *body,
                   size_t len);

/* Sends `page` in the language the request asks for: "?lang=nl" or "?lang=en"
 * (the link on the page) wins; without it, Dutch for a browser that prefers it
 * (Accept-Language) and English otherwise. */
esp_err_t web_send_page(httpd_req_t *req, const char *status, const web_page_t *page);
