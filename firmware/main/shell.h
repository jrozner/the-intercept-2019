#ifndef THE_INTERCEPT_SHELL_H
#define THE_INTERCEPT_SHELL_H

#define HOST "http://10.0.0.2:8080"

// command console registration functions
void register_system();
void register_factory_reset();
void register_restart();
int register_tuna_jokes();
void register_hidden_cmd();
void register_register_team();
void register_unread();
void register_contacts();
void register_read_message();
void register_compose();
void register_admin_login();
void register_admin_read();
void register_admin_jump();

// command functions
int register_team(int argc, char **argv);
int unread(int argc, char **argv);
int read_message(int argc, char **argv);
int factory_reset(int argc, char **argv);
int contacts(int argc, char **argv);
int restart(int argc, char** argv);
int tuna_jokes(int argc, char **argv);
int hidden_cmd(int argc, char **argv); //flag
__attribute__((used)) int unregistered_cmd(); //flag
int admin_login(int argc, char **argv);
int admin_read(int argc, char **argv);
int admin_jump(int argc, char **argv);

#endif //THE_INTERCEPT_SHELL_H
