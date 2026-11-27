#pragma once

/* BOOT long-press (SETUP_HOLD_S) requests the setup portal. Sampled from the UI
 * tick: no interrupt, timer or task of its own. */
void setup_button_init(void);

/* Called by the UI tick every UI_TICK_MS. */
void setup_button_poll(void);
