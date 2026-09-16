#pragma once

#include "../../intercom.h"

#include "esp_err.h"

/* Brings up a PPP data session to a SIM7600 over UART (esp_modem) and waits for the
 * netif to get an IP before returning. */
esp_err_t sim_modem_init(void);
esp_err_t sim_modem_deinit(void);

/* Sends `buf` as the body of a plain-text email to CONFIG_MODEM_MAIL_TO, delivered
 * directly to CONFIG_MODEM_SMTP_HOST:PORT via a hand-rolled SMTP dialog over a raw
 * socket (no relay, no auth, no TLS). Redials the PPP link first if it dropped. */
esp_err_t sim_modem_send_msg(char *buf, size_t buflen);

extern struct intercom sim_modem;
