#pragma once

/* Normal mode: starts the web server on HTTP_PORT with the status page (/) and
 * the same facts as JSON (/api/status). Read-only. Its task runs below the
 * telegram stream's, so a browser never delays a reader. If the server can't
 * start, that is logged and the rest of the device carries on. */
void status_page_start(void);
