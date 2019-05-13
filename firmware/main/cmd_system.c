#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc_cntl_reg.h"
#include "sdkconfig.h"

#include "esp_http_client.h" // http requests
#include "hwcrypto/aes.h" // aes crypto module

static const char* TAG = "cmd_system";

// command console registration functions
static void register_key_reset();
static void register_submit_flag();
static void register_list_problems();
static void register_get_contacts();
static void register_crypto_test();
static void register_restart();
static void register_tuna_jokes();
static void register_hidden_cmd(); 

// command functions
static int key_reset();
static int submit_flag();
static int list_problems();
static int get_contacts();
static int crypto_test();
static int restart(int argc, char** argv);
static int tuna_jokes();
static int hidden_cmd(); //flag
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
void register_system()
{
	register_key_reset();
	register_submit_flag();
	register_list_problems();
	register_get_contacts();
	//register_get_messages();
	//register_crypto_test();
	register_restart();
    register_hidden_cmd();
    register_tuna_jokes();
}


///////////////////////////////////////////
// Example AES256 encryption
static void register_crypto_test()
{
        esp_console_cmd_t cmd = {
            .command = "crypto",
            .help = "do stuff",
            .hint = NULL,
            .func = &crypto_test,
        };
        ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

char plaintext[8192];
char encrypted[8192];

static int crypto_test() {
	uint8_t key[32];
	uint8_t iv[16];
	memset(iv, 0, sizeof(iv));
	memset(key, 0, sizeof(key));
	memset(plaintext, 0, sizeof(plaintext));
	strcpy(plaintext, "This is my text string!\n");

	printf("Plaintext:\n");
	printf(plaintext);

	esp_aes_context ctx;
	esp_aes_init( &ctx );
	esp_aes_setkey( &ctx, key, 256 );
	esp_aes_crypt_cbc( &ctx, ESP_AES_ENCRYPT, sizeof(plaintext), iv, (uint8_t*)plaintext, (uint8_t*)encrypted );

	//See encrypted payload, and wipe out plaintext.
	memset( plaintext, 0, sizeof( plaintext ) );
	int i;
	for( i = 0; i < 128; i++ )
	{
		//printf( "%02x[%c]%c", encrypted[i], (encrypted[i]>31)?encrypted[i]:' ', ((i&0xf)!=0xf)?' ':'\n' );
		printf( "\\x%c", encrypted[i]);
	}
	printf( "\n" );

	//Must reset IV.
	printf( "IV: %02x %02x\n", iv[0], iv[1] );
	memset( iv, 0, sizeof( iv ) );


	//Use the ESP32 to decrypt the CBC block.
	esp_aes_crypt_cbc( &ctx, ESP_AES_DECRYPT, sizeof(encrypted), iv, (uint8_t*)encrypted, (uint8_t*)plaintext );


	//Verify output
	for( i = 0; i < 128; i++ )
	{
		//printf( "%02x[%c]%c", plaintext[i],  (plaintext[i]>31)?plaintext[i]:' ', ((i&0xf)!=0xf)?' ':'\n' );
		printf( "%02x", plaintext[i]);
        // has the issue with crashing/halting the board like the other printf bullshit
	}
	printf( "\n" );

    esp_aes_free( &ctx );	
	return 0;
}

static void register_key_reset() {
	// key reset
	esp_console_cmd_t cmd = {
	    .command = "key_reset",
	    .help = "Request key reset from game server",
	    .hint = NULL,
	    .func = &key_reset,
	};
	ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

//////////////////////////////////////////////////
// Encryption Things

// static char server_psk[] = ""; // server PSK
//
// static char team_secret[] = "AAAAAAAAAAAAAAAAAAA"; // team "secret ID"

// static char hmac_key1[]=""; // general hmac encryption key
// static char hmac_key2[]=""; // key_reset hmac encryption key

static int key_reset() {
	printf("Initiating key reset with game server . . .\n");
	// request key reset from webserver
	// TODO
	// read key data from webserver
	// TODO
	// key = ""; 
	return 0;
}

// flag submission arg struct
static struct {
	struct arg_int *id;
	struct arg_str *flag;
	struct arg_end *end;
} submit_args;

static void register_submit_flag() {
	submit_args.id = arg_int1(NULL, NULL, "<id>", "ID of challenge");
	submit_args.id->ival[0] = 0;
	submit_args.flag = arg_str1(NULL, NULL, "<flag>", "The flag string");
	submit_args.end = arg_end(2);

	esp_console_cmd_t cmd = {
	    .command = "submit_flag",
	    .help = "Submit a flag",
	    .hint = NULL,
	    .func = &submit_flag,
	    .argtable = &submit_args,
	};
	ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int submit_flag(int argc, char **argv) // TODO arguments
{
	
	// send flag to webserver
	// check response
	// accepted
	// already solved
	// error
	printf("Error submitting flag to game server...please try again.\n");
	return 0;
}

static void register_list_problems() {
	esp_console_cmd_t cmd = {
	    .command = "problems",
	    .help = "List challenges",
	    .hint = NULL,
	    .func = &list_problems,
	};
	ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int list_problems() {
	printf("+----------- CHALLENGES --------------+\n");
	// if (!get_problems_query) {
	// 	printf("Problems list\n");
	// } else { // error if can't contact server
	// 	printf("Error communicating with game server...\n");
	// 	return 1;
	// }
	return 0;
}


static void register_get_contacts() {
	esp_console_cmd_t cmd = {
		.command = "get_contacts",
		.help = "List contact book",
		.hint = NULL,
		.func = &get_contacts,
	};
	ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int get_contacts() {

	esp_http_client_config_t config = {
	   .url = "http://10.0.20.114:8080/contacts",
	   .event_handler = _http_event_handle,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);	
	//esp_err_t set_header = esp_http_client_set_header(client, "x-verification", "0") // TODO HMAC header
	esp_err_t err = esp_http_client_perform(client);

	if (err != ESP_OK) {
		ESP_LOGE("get_contacts", "Error occured getting contact list!\n");
	}
	esp_http_client_cleanup(client);

    // TODO parse data - must handle in event handler? unsure

	return 0;
}

static void register_tuna_jokes() {
        esp_console_cmd_t cmd = {
        .command = "tuna_jokes",
        .help = "Ask for a joke, get a joke",
        .hint = NULL,
        .func = &tuna_jokes,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int tuna_jokes() {
    static const char *jokes[] = {
        "What do you call a fish that needs help with his or her vocals?\n\nAutotuna.\n",
        "What game do fish like playing the most?\n\nName that tuna!\n",
        "Did you hear about the evil tuna?\n\nHe was rotten to the albacore.\n",
        "Who does a fish call when his piano breaks?\n\nThe piano tuna.\n",
        "Why is it so easy to weigh tuna?\n\nBecause they have their own scales!\n",
        "What is a fish’s favorite pick-up line?\n\nTuna round and let me see that bass.\n",
        "What kind of music should you listen to while fishing for tuna?\n\nSomething catchy.\n",
        "Why don’t tuna like basketball?\n\nBecause they’re afraid of the net.\n",
    };

    printf(jokes[xTaskGetTickCount()&7]);

    return 0;
}


static void register_hidden_cmd() {
    esp_console_cmd_t cmd = {
        .command = "tuna_jokeZ",
        .help = NULL,
        .hint = NULL,
        .func = &hidden_cmd,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );

}

static int hidden_cmd() {
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

    return 0;
}

static const char __attribute__((used)) hidden_tuna[] = "flag{iamthehiddentuna}";

static int unregistered_cmd() {
    // unregistered command
    // find by reversing or some magical hax to jump to it
    // flag{}
    printf("this is my secret hidden string in the unregistered command omg");
    return 0;
}


// IP:8080/messages // messages for current user
// /contacts // phone book

/* 
 // TODO commands
 * list problems / solved filtering
 * message a user
 * check your messages
 
 // TODO extra
 * hidden command?
 * user priv levels?

*/

// 'restart' command restarts the program

static int restart(int argc, char** argv) {
    ESP_LOGI(__func__, "Restarting");
    esp_restart();
}

static void register_restart() {
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Restart the program",
        .hint = NULL,
        .func = &restart,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
/*
// 'free' command prints available heap memory

static int free_mem(int argc, char** argv)
{
    printf("%d\n", esp_get_free_heap_size());
    return 0;
}

static void register_free()
{
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "Get the total size of heap memory available",
        .hint = NULL,
        .func = &free_mem,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

// 'tasks' command prints the list of tasks and related information
#if WITH_TASKS_INFO

static int tasks_info(int argc, char** argv)
{
    const size_t bytes_per_task = 40; // see vTaskList description
    char* task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE(__func__, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask Number\n", stdout);    
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}

static void register_tasks()
{
    const esp_console_cmd_t cmd = {
        .command = "tasks",
        .help = "Get information about running tasks",
        .hint = NULL,
        .func = &tasks_info,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

#endif // WITH_TASKS_INFO

// 'deep_sleep' command puts the chip into deep sleep mode

static struct {
    struct arg_int *wakeup_time;
    struct arg_int *wakeup_gpio_num;
    struct arg_int *wakeup_gpio_level;
    struct arg_end *end;
} deep_sleep_args;


static int deep_sleep(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &deep_sleep_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, deep_sleep_args.end, argv[0]);
        return 1;
    }
    if (deep_sleep_args.wakeup_time->count) {
        uint64_t timeout = 1000ULL * deep_sleep_args.wakeup_time->ival[0];
        ESP_LOGI(__func__, "Enabling timer wakeup, timeout=%lluus", timeout);
        ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    }
    if (deep_sleep_args.wakeup_gpio_num->count) {
        int io_num = deep_sleep_args.wakeup_gpio_num->ival[0];
        if (!rtc_gpio_is_valid_gpio(io_num)) {
            ESP_LOGE(__func__, "GPIO %d is not an RTC IO", io_num);
            return 1;
        }
        int level = 0;
        if (deep_sleep_args.wakeup_gpio_level->count) {
            level = deep_sleep_args.wakeup_gpio_level->ival[0];
            if (level != 0 && level != 1) {
                ESP_LOGE(__func__, "Invalid wakeup level: %d", level);
                return 1;
            }
        }
        ESP_LOGI(__func__, "Enabling wakeup on GPIO%d, wakeup on %s level",
                io_num, level ? "HIGH" : "LOW");

        ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup(1ULL << io_num, level) );
    }
    rtc_gpio_isolate(GPIO_NUM_12);
    esp_deep_sleep_start();
}

static void register_deep_sleep()
{
    deep_sleep_args.wakeup_time =
            arg_int0("t", "time", "<t>", "Wake up time, ms");
    deep_sleep_args.wakeup_gpio_num =
            arg_int0(NULL, "io", "<n>",
                     "If specified, wakeup using GPIO with given number");
    deep_sleep_args.wakeup_gpio_level =
            arg_int0(NULL, "io_level", "<0|1>", "GPIO level to trigger wakeup");
    deep_sleep_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "deep_sleep",
        .help = "Enter deep sleep mode. "
                "Two wakeup modes are supported: timer and GPIO. "
                "If no wakeup option is specified, will sleep indefinitely.",
        .hint = NULL,
        .func = &deep_sleep,
        .argtable = &deep_sleep_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

// This command helps maintain sanity when testing console example from a console

static int make(int argc, char** argv)
{
    int count = REG_READ(RTC_CNTL_STORE0_REG);
    if (++count >= 3) {
        printf("This is not the console you are looking for.\n");
        return 0;
    }
    REG_WRITE(RTC_CNTL_STORE0_REG, count);

    const char* make_output =
R"(LD build/console.elf
esptool.py v2.1-beta1
)";

    const char* flash_output[] = {
R"(Flashing binaries to serial port (*) (app at offset 0x10000)...
esptool.py v2.1-beta1
Connecting....
)",
R"(Chip is ESP32D0WDQ6 (revision 0)
Uploading stub...
Running stub...
Stub running...
Changing baud rate to 921600
Changed.
Configuring flash size...
Auto-detected Flash size: 4MB
Flash params set to 0x0220
Compressed 15712 bytesset shiftwidth=4 to 9345...
)",
R"(Wrote 15712 bytes (9345 compressed) at 0x00001000 in 0.1 seconds (effective 1126.9 kbit/s)...
Hash of data verified.
Compressed 333776 bytes to 197830...
)",
R"(Wrote 333776 bytes (197830 compressed) at 0x00010000 in 3.3 seconds (effective 810.3 kbit/s)...
Hash of data verified.
Compressed 3072 bytes to 82...
)",
R"(Wrote 3072 bytes (82 compressed) at 0x00008000 in 0.0 seconds (effective 1588.4 kbit/s)...
Hash of data verified.
Leaving...
Hard resetting...
)"
    };

    const char* monitor_output =
R"(MONITOR
)" LOG_COLOR_W R"(--- idf_monitor on (*) 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H --
)" LOG_RESET_COLOR;

    bool need_make = false;
    bool need_flash = false;
    bool need_monitor = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "all") == 0) {
            need_make = true;
        } else if (strcmp(argv[i], "flash") == 0) {
            need_make = true;
            need_flash = true;
        } else if (strcmp(argv[i], "monitor") == 0) {
            need_monitor = true;
        } else if (argv[i][0] == '-') {
            // probably -j option
        } else if (isdigit((int) argv[i][0])) {
            // might be an argument to -j
        } else {
            printf("make: *** No rule to make target `%s'.  Stop.\n", argv[i]);
            // Technically this is an error, but let's not spoil the output
            return 0;
        }
    }
    if (argc == 1) {
        need_make = true;
    }
    if (need_make) {
        printf("%s", make_output);
    }
    if (need_flash) {
        size_t n_items = sizeof(flash_output) / sizeof(flash_output[0]);
        for (int i = 0; i < n_items; ++i) {
            printf("%s", flash_output[i]);
            vTaskDelay(200/portTICK_PERIOD_MS);
        }
    }
    if (need_monitor) {
        printf("%s", monitor_output);
        esp_restart();
    }
    return 0;
}

static void register_make()
{
    const esp_console_cmd_t cmd = {
        .command = "make",
        .help = NULL, // hide from 'help' output
        .hint = "all | flash | monitor",
        .func = &make,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

*/
