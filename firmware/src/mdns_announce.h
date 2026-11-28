#pragma once

/* Announces HOSTNAME.local and service _dsmr._tcp on TCP_PORT. */
void mdns_announce_start(void);

/* Also announces the web page on HTTP_PORT (_http._tcp); after mdns_announce_start. */
void mdns_announce_web(void);
