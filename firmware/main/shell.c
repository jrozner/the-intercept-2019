#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <cJSON.h>
#include <nvs.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc_cntl_reg.h"
#include "sdkconfig.h"

#include "esp_http_client.h" // http requests
#include "hwcrypto/aes.h" // aes crypto module

#include "shell.h"
#include "tamper.h"
#include "util.h"
#include "common.h"
#include "http.h"

static const char* TAG = "shell";

// HTTP request event handler
esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            break;
        case HTTP_EVENT_ON_FINISH:
            break;
        case HTTP_EVENT_DISCONNECTED:
            break;
	    default:
	        break;
    }
    return ESP_OK;
}

// Register all command handlers (besides wifi)
void register_system() {
    esp_console_register_help_command();
    register_register_team();
	register_factory_reset();
	register_unread();
	register_contacts();
	register_read_message();
	register_compose();
	register_restart();
    register_hidden_cmd();
    register_admin_login();
    if(register_tuna_jokes() == 10) { // used to ensure a reference to unregistred_cmd because I suck at GCC apparently
        unregistered_cmd();
    }
}

static struct {
    struct arg_str *team_name;
    struct arg_end *end;
} register_arguments;

void register_register_team() {
    register_arguments.team_name = arg_str1(NULL, NULL, "<team name>", "name of team");
    register_arguments.end = arg_end(1);

    esp_console_cmd_t cmd = {
        .command = "register",
        .help = "Register a new device",
        .hint = NULL,
        .func = &register_team,
        .argtable = &register_arguments,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int register_team(int argc, char **argv) {
    char *post_data;
    char *url = HOST "/register";
    size_t body_len;

    asprintf(&post_data, "serial=%s&team_name=%s", serial, argv[1]);
    body_len = strlen(post_data);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    if (esp_http_client_open(client, body_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        free(post_data);
        return ESP_FAIL;
    }

    if (esp_http_client_write(client, post_data, body_len) <= 0) {
        ESP_LOGE(TAG, "Failed to write post body");
        free(post_data);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length == ESP_FAIL || content_length == 0) {
        if (esp_http_client_is_chunked_response(client)) {
            printf("response is chunked\n");
        }
        ESP_LOGE(TAG, "no length or chunked");
        esp_http_client_cleanup(client);
        free(post_data);
        return ESP_FAIL;
    }

    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "got unexpected status");
        esp_http_client_cleanup(client);
        free(post_data);
        return ESP_FAIL;
    }

    char *buffer = malloc(content_length+1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "unable to allocate space for buffer");
        esp_http_client_cleanup(client);
        free(post_data);
        return ESP_FAIL;
    }

    int total_read = 0, read_len = 0;
    read_len = esp_http_client_read(client, buffer, content_length);

    if (read_len < total_read) {
        esp_http_client_cleanup(client);
        free(buffer);
        free(post_data);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buffer);

    nvs_handle nvs;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    char *ak = cJSON_GetObjectItem(root,"access_key")->valuestring;
    ESP_ERROR_CHECK(set_access_key(ak));

    char *sk = cJSON_GetObjectItem(root,"secret_key")->valuestring;
    uint8_t *decoded;
    if ((decoded = hex_decode(sk)) == NULL) {
        ESP_LOGE(TAG, "Failed to decode secret key");
        nvs_close(nvs);
        cJSON_free(root);
        esp_http_client_cleanup(client);
        free(post_data);
        return ESP_FAIL;
    }

    size_t secret_length = strlen(sk) / 2;

    ESP_ERROR_CHECK(set_secret_key(decoded, secret_length));

    nvs_close(nvs);
    cJSON_free(root);

    esp_http_client_cleanup(client);
    free(decoded);
    free(buffer);
    free(post_data);

    return ESP_OK;
}

void register_factory_reset() {
	esp_console_cmd_t cmd = {
	    .command = "factory_reset",
	    .help = "Perform a factory reset; this is required when tampering is detected",
	    .hint = NULL,
	    .func = &factory_reset,
	};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int factory_reset(int argc, char **argv) {
    char *url = HOST "/rotate_secret";

    esp_http_client_config_t config = {
            .url = url,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char *auth_header;
    if ((auth_header = generate_auth_header("POST", "")) == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        free(auth_header);
        return ESP_FAIL;
    }

    //if (esp_http_client_write(client, "", 0) <= 0) {
    //    ESP_LOGE(TAG, "Failed to write post body");
    //    esp_http_client_cleanup(client);
    //    return ESP_FAIL;
    //}

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length == ESP_FAIL || content_length == 0) {
        if (esp_http_client_is_chunked_response(client)) {
            printf("response is chunked\n");
        }
        ESP_LOGE(TAG, "no length or chunked");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "got unexpected status");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    char *buffer = malloc(content_length+1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "unable to allocate space for buffer");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    int total_read = 0, read_len = 0;
    read_len = esp_http_client_read(client, buffer, content_length);

    if (read_len < total_read) {
        esp_http_client_cleanup(client);
        free(buffer);
        free(auth_header);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buffer);

    nvs_handle nvs;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    char *ak = cJSON_GetObjectItem(root,"access_key")->valuestring;
    ESP_ERROR_CHECK(set_access_key(ak));

    char *sk = cJSON_GetObjectItem(root,"secret_key")->valuestring;
    uint8_t *decoded;
    if ((decoded = hex_decode(sk)) == NULL) {
        ESP_LOGE(TAG, "Failed to decode secret key");
        nvs_close(nvs);
        cJSON_free(root);
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    size_t secret_length = strlen(sk) / 2;

    ESP_ERROR_CHECK(set_secret_key(decoded, secret_length));

    nvs_close(nvs);
    cJSON_free(root);

    set_tamper_nvs(0);
    tamper_notified = false;

    esp_http_client_cleanup(client);
    free(decoded);
    free(buffer);
    free(auth_header);

    return ESP_OK;
}

void register_contacts() {
	esp_console_cmd_t cmd = {
		.command = "contacts",
		.help = "List contact book",
		.hint = NULL,
		.func = &contacts,
	};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int contacts(int argc, char **argv) {
    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    char *url = HOST "/contacts";

    esp_http_client_config_t config = {
            .url = url,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char *auth_header;
    if ((auth_header = generate_auth_header("GET", "")) == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_method(client, HTTP_METHOD_GET);

    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        free(auth_header);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length == ESP_FAIL || content_length == 0) {
        ESP_LOGE(TAG, "no length or chunked");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "got unexpected status");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    char *buffer = malloc(content_length+1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "unable to allocate space for buffer");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    int total_read = 0, read_len = 0;
    read_len = esp_http_client_read(client, buffer, content_length);

    if (read_len < total_read) {
        esp_http_client_cleanup(client);
        free(buffer);
        free(auth_header);
        return ESP_FAIL;
    }

    nvs_handle nvs;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    printf("Contacts\n\n");

    cJSON *messages = cJSON_Parse(buffer), *message;

    cJSON_ArrayForEach(message, messages) {
        char *team_name = cJSON_GetObjectItem(message, "team_name")->valuestring;

        printf("%s\n", team_name);
    }

    cJSON_free(messages);
    free(buffer);
    esp_http_client_cleanup(client);
    free(auth_header);

    return ESP_OK;
}

int register_tuna_jokes() {
        esp_console_cmd_t cmd = {
        .command = "tuna_jokes",
        .help = "Ask for a joke, get a joke",
        .hint = NULL,
        .func = &tuna_jokes,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    return ESP_OK;
}

int tuna_jokes(int argc, char **argv) {
    static const char *jokes[] = {
        "What do you call a fish that needs help with his or her vocals?\n\nAutotuna.\n",
        "What game do fish like playing the most?\n\nName that tuna!\n",
        "Did you hear about the evil tuna?\n\nHe was rotten to the albacore.\n",
        "Who does a fish call when his piano breaks?\n\nThe piano tuna.\n",
        "Why is it so easy to weigh tuna?\n\nBecause they have their own scales!\n",
        "What is a fish’s favorite pick-up line?\n\nTuna round and let me see that bass.\n",
        "What kind of music should you listen to while fishing for tuna?\n\nSomething catchy.\n",
        "Why don’t tuna like basketball?\n\nBecause they’re afraid of the net.\n",
        "What do you call a tuna with a tie?\n\nSoFISHticated\n",
    };

    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    printf(jokes[xTaskGetTickCount()&7]);

    return ESP_OK;
}

void register_hidden_cmd() {
    esp_console_cmd_t cmd = {
        .command = "tuna_jokeZ",
        .help = NULL,
        .hint = NULL,
        .func = &hidden_cmd,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int hidden_cmd(int argc, char **argv) {
    // hidden command flag{findThemFISHJokeZ}
    static char b[] = "g{f";
    static char f[] = "Joke";
    static char d[] = "hem";
    static char a[] = "fla";
    static char c[] = "ind";
    static char e[] = "FIS";


    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    printf("What did the tuna say when he posted bail?\n\n");
    printf("I'm off the hook.\n\n");
    printf("%s%s%sT%s%sH%sZ}\n",a,b,c,d,e,f);

    return ESP_OK;
}

int unregistered_cmd() {
    if (get_tamper_nvs()) {
        printf("unable to execute command; tamper detected. Perform a factory reset to continue use");
        return ESP_FAIL;
    }
    // unregistered command
    // find by reversing or some magical hax to jump to it
    // flag{hiddenAmongTHEReeds}
    volatile const char sea_urchin[] = "hlned{AngadgsdeT}meEoifRH";
    printf("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n", sea_urchin[22],sea_urchin[1],sea_urchin[9],sea_urchin[8],sea_urchin[5],sea_urchin[0],sea_urchin[21],sea_urchin[13],sea_urchin[4],sea_urchin[18],sea_urchin[2],sea_urchin[6],sea_urchin[17],sea_urchin[20],sea_urchin[7],sea_urchin[11],sea_urchin[15],sea_urchin[24],sea_urchin[19],sea_urchin[23],sea_urchin[14],sea_urchin[3],sea_urchin[10],sea_urchin[12],sea_urchin[16]);

    return ESP_OK;
}

int restart(int argc, char** argv) {
    ESP_LOGI(__func__, "Restarting");
    esp_restart();
}

void register_restart() {
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Restart the device",
        .hint = NULL,
        .func = &restart,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int unread(int argc, char **argv) {
    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    char *url = HOST "/messages";

    esp_http_client_config_t config = {
            .url = url,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char *auth_header;
    if ((auth_header = generate_auth_header("GET", "")) == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_method(client, HTTP_METHOD_GET);

    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        free(auth_header);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length == ESP_FAIL || content_length == 0) {
        ESP_LOGE(TAG, "no length or chunked");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "got unexpected status");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    char *buffer = malloc(content_length+1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "unable to allocate space for buffer");
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    int total_read = 0, read_len = 0;
    read_len = esp_http_client_read(client, buffer, content_length);

    if (read_len < total_read) {
        esp_http_client_cleanup(client);
        free(buffer);
        free(auth_header);
        return ESP_FAIL;
    }

    nvs_handle nvs;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    printf("Unread Messages\n\n");

    cJSON *messages = cJSON_Parse(buffer), *message;

    cJSON_ArrayForEach(message, messages) {
        char id = cJSON_GetObjectItem(message, "id")->valueint;
        char *text = cJSON_GetObjectItem(message, "text")->valuestring;
        cJSON *sender = cJSON_GetObjectItem(message, "sender");
        char *sender_name = cJSON_GetObjectItem(sender, "team_name")->valuestring;

        char *short_text;
        if ((short_text = substr(text, 20)) == NULL) {
            continue;
        }

        printf("#%d | %s | %s\n", id, sender_name, short_text);
        free(short_text);
    }

    cJSON_free(messages);
    free(buffer);
    esp_http_client_cleanup(client);
    free(auth_header);

    return ESP_OK;
}

void register_unread() {
    const esp_console_cmd_t cmd = {
        .command = "unread",
        .help = "List unread messages",
        .hint = NULL,
        .func = &unread,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static struct {
    struct arg_int *id;
    struct arg_end *end;
} read_message_arguments;


int read_message(int argc, char **argv) {
    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    char *url;

    if (argc < 2) {
        ESP_LOGE(TAG, "missing argument");
        return ESP_FAIL;
    }

    if (asprintf(&url, HOST "/messages/%s", argv[1]) == -1) {
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
            .url = url,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char *auth_header;
    if ((auth_header = generate_auth_header("GET", "")) == NULL) {
        free(url);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_method(client, HTTP_METHOD_GET);

    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        free(auth_header);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length == ESP_FAIL || content_length == 0) {
        ESP_LOGE(TAG, "no length or chunked");
        free(url);
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "got unexpected status");
        free(url);
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    char *buffer = malloc(content_length+1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "unable to allocate space for buffer");
        free(url);
        esp_http_client_cleanup(client);
        free(auth_header);
        return ESP_FAIL;
    }

    int total_read = 0, read_len = 0;
    read_len = esp_http_client_read(client, buffer, content_length);

    if (read_len < total_read) {
        free(url);
        esp_http_client_cleanup(client);
        free(buffer);
        free(auth_header);
        return ESP_FAIL;
    }

    nvs_handle nvs;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    cJSON *message = cJSON_Parse(buffer);

    char *text = cJSON_GetObjectItem(message, "text")->valuestring;
    cJSON *sender = cJSON_GetObjectItem(message, "sender");
    char *sender_name = cJSON_GetObjectItem(sender, "team_name")->valuestring;

    printf("FROM: %s\n\n%s\n", sender_name, text);

    free(url);
    cJSON_free(message);
    free(buffer);
    esp_http_client_cleanup(client);
    free(auth_header);

    return ESP_OK;
}

void register_read_message() {
    read_message_arguments.id = arg_int1(NULL, NULL, "<id>", "id of message");
    read_message_arguments.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "read",
        .help = "Read a message",
        .hint = NULL,
        .func = &read_message,
        .argtable = &read_message_arguments,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static struct {
    struct arg_str *recipient;
    struct arg_str *message;
    struct arg_end *end;
} compose_arguments;


int compose(int argc, char **argv) {
    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    char *post_data;
    char *url = HOST "/messages";
    size_t body_len;

    if (argc != 3) {
        ESP_LOGE(TAG, "invalid arguments");
        return ESP_FAIL;
    }

    asprintf(&post_data, "receiver=%s&text=%s", argv[1], argv[2]);
    body_len = strlen(post_data);

    esp_http_client_config_t config = {
            .url = url,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char *auth_header;
    if ((auth_header = generate_auth_header("POST", post_data)) == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    if (esp_http_client_open(client, body_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        free(post_data);
        return ESP_FAIL;
    }

    if (esp_http_client_write(client, post_data, body_len) <= 0) {
        ESP_LOGE(TAG, "Failed to write post body");
        free(post_data);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length == ESP_FAIL) {
        ESP_LOGE(TAG, "failed to read headers");
        esp_http_client_cleanup(client);
        free(post_data);
        return ESP_FAIL;
    }

    if (esp_http_client_get_status_code(client) != 201) {
        ESP_LOGE(TAG, "got unexpected status");
        esp_http_client_cleanup(client);
        free(post_data);
        return ESP_FAIL;
    }

    esp_http_client_cleanup(client);
    free(post_data);

    return ESP_OK;
}

void register_compose() {
    compose_arguments.recipient = arg_str1(NULL, NULL, "<recipient>", "recipient of the message");
    compose_arguments.message = arg_str1(NULL, NULL, "<message>", "message to send");
    compose_arguments.end = arg_end(2);
    const esp_console_cmd_t cmd = {
        .command = "compose",
        .help = "Compose a message",
        .hint = NULL,
        .func = &compose,
        .argtable = &compose_arguments,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static bool admin_state = 0; // arg for checking admin mode

static struct {
    struct arg_str *password;
    struct arg_end *end;
} admin_login_arguments;

void register_admin_login() {
    admin_login_arguments.password = arg_str1(NULL, NULL, "<password>", "password string");
    admin_login_arguments.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "admin_login",
        .help = "Unlock administrative mode",
        .hint = NULL,
        .func = &admin_login,
        .argtable = &admin_login_arguments,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int admin_login(int argc, char **argv) {
    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    if (admin_state != 1) { // check if already admin
        if (argv[1] != NULL) {
            // do check
            const char aa[] = "\x60\x6a\x7f\x75\xa3\x4f\x07\xb3\xb2"; // enc 1
            const char bb[] = "\x53\x7c\x25\xed\x5b\x9d\x95\x06\x17"; // enc 2
            const char cc[] = "\x2b\x23\x31\x32\xcc\x29\x73\xfb\xd7"; // key 1
            const char dd[] = "\x1e\x3d\x66\xa6\x1e\xcf\xd0\x4a\x64"; // key 2
            char ee[20]; // output buf
            memset(&ee, 0, 20);
            uint8_t i;
            for (i=0; i<9; i++) {// reassemble plaintext password
                ee[i] = aa[i] ^ cc[i];
                ee[i+9] = bb[i] ^ dd[i];
            }

            const TickType_t delay_ms = 50/portTICK_PERIOD_MS;

            if (strlen(argv[1]) == 18) {
                bool allowed = true;
                for (i=0; i<18; i++) {
                    if (ee[i] != argv[1][i]) {
                        allowed=false;
                        break;
                    }
                    vTaskDelay(delay_ms); // Force delay for timing attack vuln
                }
                if (allowed) {
                    admin_state = 1;
                    register_admin_read();
                    register_admin_jump();
                    printf("Access granted.\nHello, Administrator!\nPlease check 'help' for administrative help.\n");
                    return ESP_OK;
                }
            }
            printf("Access denied.\n");
        } else {
            printf("Please specify a password!\n");
        }
    } else {
        printf("You're already an admin, you silly salmon.\n");
    }

    return ESP_OK;
}

static struct {
    struct arg_str *addr;
    struct arg_end *end;
} admin_read_arguments;

void register_admin_read() {
    admin_read_arguments.addr = arg_str1(NULL, NULL, "<address>", "address to read from (hex)");
    admin_read_arguments.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "admin_read",
        .help = "ADMIN - read a memory address (32 bits)",
        .hint = NULL,
        .func = &admin_read,
        .argtable = &admin_read_arguments,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int admin_read(int argc, char **argv) {
    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    if (admin_state) {
        if (argv[1] != NULL) {
            unsigned int *addr = strtoul(argv[1], NULL, 16);
            printf("Reading from %s: %08x\n", argv[1], (unsigned int)(*addr));
        } else {
            printf("Please specify an address to read from!\n");
        }
    }

    return ESP_OK;
}

static struct {
    struct arg_str *addr;
    struct arg_end *end;
} admin_jump_arguments;

void register_admin_jump() {
    admin_jump_arguments.addr = arg_str1(NULL, NULL, "<address>", "address to jump to (hex)");
    admin_jump_arguments.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "admin_jump",
        .help = "ADMIN - jump to a memory address",
        .hint = NULL,
        .func = &admin_jump,
        .argtable = &admin_jump_arguments,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int admin_jump(int argc, char **argv) {
    if (get_tamper_nvs()) {
        ESP_LOGE(TAG, "%s", tamper_msg);
        return ESP_FAIL;
    }

    if (admin_state) {
        if (argv[1] != NULL) {
            printf("Prepare for dive!\n");
            int addr = strtoul(argv[1], NULL, 16);
            int (*func)();
            func = (int (*)())addr;
            (int)(*func)();
        } else {
            printf("Please specify an address to jump to!\n");
        }
    }

    return ESP_OK;
}

// unreferenced function set "used" to store unferenced string data for various challenges
__attribute__((used)) int placeholder();
int placeholder() {
    const char hiddentuna[] = "flag{trawlingTHEDepthsISea}";
    printf(hiddentuna);
    const char ver[] = "COOLTUNA v1.34.5.1.2";
    printf(ver);

    return ESP_OK;
}
