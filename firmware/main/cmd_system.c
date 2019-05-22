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
#include "esp_http_client.h"

#include "esp_http_client.h" // http requests
#include "hwcrypto/aes.h" // aes crypto module

#include "cmd_tamper.h"

#define HOST "http://192.168.1.215:8080"

static const char* TAG = "cmd_system";

static char serial[12];

// command console registration functions
static void register_factory_reset();
static void register_restart();
static int register_tuna_jokes();
static void register_hidden_cmd();
static void register_register_team();
static void register_unread();
static void register_contacts();
static void register_read_message();
static void register_compose();

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
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                printf("%.*s", evt->data_len, (char*)evt->data);
            }

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
    if(register_tuna_jokes() == 10) { // used to ensure a reference to unregistred_cmd because I suck at GCC apparently
        unregistered_cmd();
    }
}

static struct {
    struct arg_str *team_name;
    struct arg_end *end;
} register_arguments;

static void register_register_team() {
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

static int register_team(int argc, char **argv) {
    char *post_data;
    char *url = HOST "/register";
    esp_err_t err;

    asprintf(&post_data, "serial=%s&team_name=%s", serial, argv[1]);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length == ESP_FAIL || content_length == 0) {
        ESP_LOGE(TAG, "no length or chunked");
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
    // TODO: update storage
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs));

    char *access_key = cJSON_GetObjectItem(root,"access_key")->string;
    ESP_ERROR_CHECK(nvs_set_str(nvs, "access_key", access_key));

    char *secret_key = cJSON_GetObjectItem(root,"access_key")->string;
    ESP_ERROR_CHECK(nvs_set_str(nvs, "access_key", secret_key));

    ESP_ERROR_CHECK(nvs_commit(nvs));

    nvs_close(nvs);
    cJSON_free(root);

    free(buffer);
    free(post_data);

    return ESP_OK;
}

static void register_factory_reset() {
	esp_console_cmd_t cmd = {
	    .command = "factory_reset",
	    .help = "Perform a factory reset; this is required when tampering is detected",
	    .hint = NULL,
	    .func = &factory_reset,
	};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int factory_reset(int argc, char **argv) {
    set_tamper_nvs(0);
    return ESP_OK;
}

static void register_contacts() {
	esp_console_cmd_t cmd = {
		.command = "contacts",
		.help = "List contact book",
		.hint = NULL,
		.func = &contacts,
	};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int contacts(int argc, char **argv) {
	//esp_http_client_config_t config = {
	//   .url = "http://10.0.20.114:8080/contacts",
	//   .event_handler = _http_event_handle,
	//};
	//esp_http_client_handle_t client = esp_http_client_init(&config);
	//esp_err_t set_header = esp_http_client_set_header(client, "Authentication", "0"); // TODO HMAC header
	//esp_err_t err = esp_http_client_perform(client);

	//if (err != ESP_OK) {
	//	ESP_LOGE("get_contacts", "Error occured getting contact list!\n");
	//}
	//esp_http_client_cleanup(client);

    // TODO parse data - must handle in event handler? unsure

	return ESP_OK;
}

static int register_tuna_jokes() {
        esp_console_cmd_t cmd = {
        .command = "tuna_jokes",
        .help = "Ask for a joke, get a joke",
        .hint = NULL,
        .func = &tuna_jokes,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    return ESP_OK;
}

static int tuna_jokes(int argc, char **argv) {
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

    printf(jokes[xTaskGetTickCount()&7]);

    return ESP_OK;
}

static void register_hidden_cmd() {
    esp_console_cmd_t cmd = {
        .command = "tuna_jokeZ",
        .help = NULL,
        .hint = NULL,
        .func = &hidden_cmd,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int hidden_cmd(int argc, char **argv) {
    // hidden command flag{findThemFISHJokeZ}
    static char b[] = "g{f";
    static char f[] = "Joke";
    static char d[] = "hem";
    static char a[] = "fla";
    static char c[] = "ind";
    static char e[] = "FIS";

    printf("What did the tuna say when he posted bail?\n\n");
    printf("I'm off the hook.\n\n");
    printf("%s%s%sT%s%sH%sZ}\n",a,b,c,d,e,f);

    return ESP_OK;
}

static int unregistered_cmd() {
    // unregistered command
    // find by reversing or some magical hax to jump to it
    // flag{hiddenAmongTHEReeds}
    volatile const char sea_urchin[] = "hlned{AngadgsdeT}meEoifRH";
    printf("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n", sea_urchin[22],sea_urchin[1],sea_urchin[9],sea_urchin[8],sea_urchin[5],sea_urchin[0],sea_urchin[21],sea_urchin[13],sea_urchin[4],sea_urchin[18],sea_urchin[2],sea_urchin[6],sea_urchin[17],sea_urchin[20],sea_urchin[7],sea_urchin[11],sea_urchin[15],sea_urchin[24],sea_urchin[19],sea_urchin[23],sea_urchin[14],sea_urchin[3],sea_urchin[10],sea_urchin[12],sea_urchin[16]);

    return ESP_OK;
}

static int restart(int argc, char** argv) {
    ESP_LOGI(__func__, "Restarting");
    esp_restart();
}

static void register_restart() {
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Restart the device",
        .hint = NULL,
        .func = &restart,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int unread(int argc, char **argv) {
    return ESP_OK;
}

static void register_unread() {
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


static int read_message(int argc, char **argv) {
    return ESP_OK;
}

static void register_read_message() {
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


static int compose(int argc, char **argv) {
    return ESP_OK;
}

static void register_compose() {
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

// unreferenced function set "used" to store unferenced string data for various challenges
static __attribute__((used)) int placeholder();
static int placeholder() {
    const char hiddentuna[] = "flag{trawlingTHEDepthsISea}";
    printf(hiddentuna);
    const char ver[] = "COOLTUNA v1.34.5.1.2";
    printf(ver);

    return ESP_OK;
}
