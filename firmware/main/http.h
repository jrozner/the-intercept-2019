#ifndef THE_INTERCEPT_HTTP_H
#define THE_INTERCEPT_HTTP_H

extern char serial[12];
extern char *access_key;
extern uint8_t *secret_key;

int lookup_serial();
int lookup_access_key();
int set_access_key(char *);
int lookup_secret_key();
int set_secret_key(uint8_t *, size_t);
int generate_signature(unsigned char *, char *, char *);
char *generate_auth_header(char *method, char *body);


#endif //THE_INTERCEPT_HTTP_H
