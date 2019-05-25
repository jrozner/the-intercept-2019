#ifndef THE_INTERCEPT_TAMPER_H
#define THE_INTERCEPT_TAMPER_H

extern bool tamper_notified;

uint8_t get_tamper_nvs();
void set_tamper_nvs(uint8_t val);

static const char tamper_msg[] =
"Tampering has been detected. COOLTUNA security protocols enforced.\n"
"All commands have been disabled except: factory_reset\n\n"
"Please restore product packaging to its original condition and\n"
"use this command to re-authenticate with the server!\n";

#endif

