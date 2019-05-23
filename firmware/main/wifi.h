#ifndef THE_INTERCEPT_WIFI_H
#define THE_INTERCEPT_WIFI_H

void initialise_wifi(void);
bool wifi_join(const char* ssid, const char* pass, int timeout_ms);

#endif //THE_INTERCEPT_WIFI_H
