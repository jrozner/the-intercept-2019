/*
 * The Intercept - LayerOne 2019
 * Hardware CTF
 *
 * Coded (poorly) by datagram & Joe Rozner
 *
 * Many lines of code re-used from various esp-idf component/example files.
 * Base code based on the esp-idf/examples/console
 */

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_fat.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_http_client.h"

#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include <driver/adc.h>

#include "cmd_decl.h"
#include "cmd_wifi.c"
#include "cmd_system.c"
#include "common.h"
#include "cmd_tamper.h"

#define PROD false

// sensor pins
#define SENSOR_SW1 4 //GPIO 4 - phys 6
#define SENSOR_SW2 34 //GPIO 34 - phys 26

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem() {
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY

static void initialize_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void initialize_console() {
    /* Disable buffering on stdin and stdout */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_CONSOLE_UART_NUM,
            256, 0, 0, NULL, 0) );

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
            .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

/*#if CONFIG_STORE_HISTORY
    // Load command history from filesystem
    linenoiseHistoryLoad(HISTORY_PATH);
#endif*/
}

// Tamper sensor ISR notification queue
static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler_sw1(void* arg);
static void IRAM_ATTR gpio_isr_handler_sw2(void* arg);
static void sensor_handler_digi(void *arg);
static void sensor_handler_ana(void *arg);

// switch 1 (lever) ISR
static void IRAM_ATTR gpio_isr_handler_sw1(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// switch 2 (lever) ISR
static void IRAM_ATTR gpio_isr_handler_sw2(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// debounce vars for digital switches
bool old_state_sw1=0, old_state_sw2=0;

bool tamper_notified = 0;

static const char tamper_msg[] =
"Tampering has been detected. COOLTUNA security protocols enforced.\n"
"All commands have been disabled except: factory_reset\n\n"
"Please restore product packaging to its original condition and\n"
"use this command to re-authenticate with the server!\n";

// Task handling digital sensor notifications from ISRs
static void sensor_handler_digi(void *arg) {
    uint32_t io_num;
    bool state=0;
    for (;;) {
        // check queue for events from ISR
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            /* workflow:
             * - grab switch state
             * - verify state is high
             * -- really awful debounce check
             * --- tamper alert
             * - save current state for debounce checking
             */
            state = gpio_get_level(io_num);
            switch(io_num) {
                case SENSOR_SW1:
                    if (state && state != old_state_sw1) {
#if !PROD
                        ESP_LOGW(TAG, "[DEV] TAMPERING DETECTED? SW1 (%d)", io_num);
#endif
                        //tamper_detected=1;
                        set_tamper_nvs(1);
                    }
                    old_state_sw1 = state;
                    break;
                case SENSOR_SW2:
                    if (state && state != old_state_sw2) {
#if !PROD
                        //printf("[DEV] TAMPERING DETECTED? sw %d\n", io_num);
                        ESP_LOGW(TAG, "[DEV] TAMPERING DETECTED? SW2 (%d)", io_num);
#endif
                        //tamper_detected=1;
                        set_tamper_nvs(1);
                    }
                    old_state_sw2 = state;
                    break;
            }
            //if (tamper_detected && !tamper_notified) { // prevent spam
            if (get_tamper_nvs() && !tamper_notified) { // prevent spam
                //printf(tamper_msg);
                ESP_LOGE(TAG, "%s", tamper_msg);
                tamper_notified=1;
            }
        }
    }
}

// Task handling analog sensor
// Polling via task delay and analog reads
static void sensor_handler_ana(void *arg) {
    const TickType_t delay_ms = 50/portTICK_PERIOD_MS;

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_5,ADC_ATTEN_DB_0);
    for( ;; ) {
        int val = adc1_get_raw(ADC1_CHANNEL_5);
        if (val > 0) { 
#if !PROD
            ESP_LOGW(TAG, "[DEV] TAMPERING DETECTED? ANALOG (%d)", val);
#endif
            //tamper_detected=1;
            set_tamper_nvs(1);
            if (!tamper_notified) { // prevent spam
                //printf(tamper_msg);
                ESP_LOGE(TAG, "%s", tamper_msg);
                tamper_notified=1; // possible race condition with the other task?
                                   // modifying the check to this format seems better
                                   // (previously double alerting tamper_msg on console)
            }
        }
        
        vTaskDelay( delay_ms );
    }
}

void app_main() {
    // PRAISE THE TUNA
    const char* tunalogo = "\n"
    "                          7777\n"
    "                         77=+~7\n"
    "                        7+77?77\n"
    "                      7.I==?I777         77=+77                        7 77\n"
    "                     77+~~?+=??+~77    7=+7?777                       7~:77\n"
    "              7 777+.=~,,..........,.:7=.~+I77                       77?=7\n"
    "          7777.?::.....,,.,.................~,7777 7               77I=+7\n"
    "         7:,,,,,,..,??????I+????,,,,,,,......,:.7+77              7+=7.7\n"
    "\x1b[1;93m+----=+--------+\x1b[0m?+??7?+I??II??II??I??I??I?+,:,,:,,.,~,7777        7=~==7\n"
    "\x1b[1;93m|   /\x1b[0m,\x1b[1;93m|        |\x1b[0mII=IIII=::?~+7+7+I???+II???I??+I?++?~,,,:+:7777777:~=I7\n"
    " \x1b[1;93m\\ /\x1b[0m=:?\x1b[1;93m\\       /\x1b[0m77?=?I7I?I?II7IIII?IIII7IIIIII?I???????+++++?+:.,+:II=7\n"
    "  \x1b[1;93mV\x1b[0m777+I\x1b[1;93m\\_____/\x1b[0m77?II77??I?+IIIIII7III7II77I7IIIII7IIII7?I???+=???++=7\n"
    " 77=+II7,777II?+III+I7I???7III77777I777777I7IIIIII?I+=?+I+7~77777=?I7~7\n"
    "   -_    /+??++=II=I?IIIIIII77II7IIIII7II7II????++???,~777      77~=I=77\n"
    "     7777I+??+~=?+=?I?I??I?IIIIII?I??III?++?==??I++7  77         7 =??+7\n"
    "         77.?=:=???++=+?++?+?+++??I????+=+=+?I=I 77               77+I?7\n"
    "            7777777:77IIII???????III77III~,??I?777                 77=?.7\n"
    "                    77,~.I~=7777777777777777777I77                  77?~7\n"
    "                     777:~=77               777,=77                  77:,7\n"
    "                        I7=77                  7777                    777\n"
    "                        777+7\n"
    "                           777\n\n";


    // Reset logging for prod post-boot
    // Allow seeing the boot informational messages about memory setup/entry point/etc
    // Disable errors with
    #if PROD
        //ESP_LOGI(TAG, "+ + + Logging DISABLED + + +!\n");
        esp_log_level_set("*", ESP_LOG_ERROR); 
    #else // logging enabled (debug build only)
        ESP_LOGW(TAG, "[DEV] Info Logging Enabled!");
    #endif

    // init system components for console
    initialize_nvs();
    #if CONFIG_STORE_HISTORY
      initialize_filesystem();
    #endif
    initialize_console();

    // register console command handlers
    register_system();
    #if !PROD
        register_wifi(); // register wifi command handlers (debug build only)
    #endif

    // TODO prod values
    // Join game WIFI network
    //if (!wifi_join("interceptctfnet2","9w38ruaowfuaw86sty3", 10)) {
    wifi_join("interceptctfnet2","9w38ruaowfuaw86sty3", 10);
    /*    ESP_LOGE(TAG, "Can't connect to game network - restarting in 10 seconds!");
#if PROD
        uint8_t i = 0;
        for (i=0; i<10; i++) {
            printf("%d...",10-i);
            vTaskDelay((TickType_t) (1000/portTICK_PERIOD_MS));
        }
        esp_restart();
#else
        ESP_LOGW(TAG, "[DEV] Restart aborted due to dev mode.");
#endif
    }*/

    nvs_handle nvs;
    esp_err_t err;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    size_t serial_length;
    err = nvs_get_str(nvs, "serial", NULL, &serial_length);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_ERROR_CHECK(err);
    }

    err = nvs_get_str(nvs, "serial", serial, &serial_length);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            for (int i = 0; i < 11; i++) {
                char c = (esp_random() % 10) + '0';
                serial[i] = c;
            }
            serial[11] = 0;
            ESP_ERROR_CHECK(nvs_set_str(nvs, "serial", serial));
            ESP_ERROR_CHECK(nvs_commit(nvs));
        } else {
            ESP_ERROR_CHECK(err)
        }
    }

    /////////////////////////////////////////////////
    // Tamper sensor pin setup

    //tamper_detected = get_tamper_nvs();

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = ((1ULL<<SENSOR_SW1)|(1ULL<<SENSOR_SW2));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //start digital sensor task
    xTaskCreate(sensor_handler_digi, "sensor_handler_digi", 4096, NULL, 10, NULL);
    //start analog sensor task
    xTaskCreate(sensor_handler_ana, "sensor_handler_ana", 4096, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(SENSOR_SW1, gpio_isr_handler_sw1, (void*) SENSOR_SW1);
    gpio_isr_handler_add(SENSOR_SW2, gpio_isr_handler_sw2, (void*) SENSOR_SW2);

    // Board started with switches "out" (triggered) bypasses ISR
    // Manually check to verify they are not high
    if (gpio_get_level(SENSOR_SW1) | gpio_get_level(SENSOR_SW2) | get_tamper_nvs()) {
#if !PROD
        ESP_LOGW(TAG, "[DEV] On-Boot Tamper Detected");
#endif
        //tamper_detected=1;
        set_tamper_nvs(1);
        tamper_notified=1;
        ESP_LOGE(TAG, "%s", tamper_msg);
    } else {
        char sa[] = "/%(.2\x04\x0f\x1d\x1c\x07\x08<9x\x07=! :4\n";
        int i;
        for (i=0; i<20; i++) {
            sa[i] ^= 'I';
        }
        printf(sa);
    }

    /////////////////////////////////////////////////
    //	Console setup
    const char* prompt = LOG_COLOR_I "cooltuna> " LOG_RESET_COLOR;
    int probe_status = linenoiseProbe();
    if (probe_status) { 
        printf("\n [!] Your terminal application does not support escape sequences.\n"
               "[!] Line editing and history features are disabled.\n"
               "[!] On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
        #if CONFIG_LOG_COLORS
          prompt = "cooltuna> ";
        #endif
    }
    printf(tunalogo);
    printf("+ Welcome to COOLTUNA Messaging Service +\n");

    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
    // MAIN CONSOLE LOOP	
    while(true) {
        char* line = linenoise(prompt);
        if (line == NULL) { /* Ignore empty lines */
            continue;
        }
        linenoiseHistoryAdd(line); //Add the command to the history

        // Try to run the command
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) { // TODO remove me?
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(err));
        } else if (err != ESP_OK) { // TODO remove me?
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        linenoiseFree(line);
    }
}
