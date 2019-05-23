#ifndef THE_INTERCEPT_SHELL_H
#define THE_INTERCEPT_SHELL_H

#define HOST "http://192.168.1.215:8080"

// command console registration functions
static void register_system();
static void register_factory_reset();
static void register_restart();
static int register_tuna_jokes();
static void register_hidden_cmd();
static void register_register_team();
static void register_unread();
static void register_contacts();
static void register_read_message();
static void register_compose();
static void register_admin_login();
static void register_admin_read();
static void register_admin_jump();

// command functions
static int register_team(int argc, char **argv);
static int unread(int argc, char **argv);
static int read_message(int argc, char **argv);
static int factory_reset(int argc, char **argv);
static int contacts(int argc, char **argv);
static int restart(int argc, char** argv);
static int tuna_jokes(int argc, char **argv);
static int hidden_cmd(int argc, char **argv); //flag
static __attribute__((used)) int unregistered_cmd(); //flag
static int admin_login(int argc, char **argv);
static int admin_read(int argc, char **argv);
static int admin_jump(int argc, char **argv);

#endif //THE_INTERCEPT_SHELL_H
