#include <stdio.h>
#include <string.h>
//#include "esp_system.h"
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

#define PROD false
// sensor pins
#define SENSOR_PHOTO 33 //9
#define SENSOR_SW1 4 //6
#define SENSOR_SW2 34 //26

static const char* TAG = "intercept";

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem()
{
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

static void initialize_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void initialize_console()
{
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

static xQueueHandle gpio_evt_queue = NULL;

portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR gpio_isr_handler_sw1(void* arg);
static void IRAM_ATTR gpio_isr_handler_sw2(void* arg);
static void sensor_handler_digi(void *arg);
static void sensor_handler_ana(void *arg);

static void IRAM_ATTR gpio_isr_handler_sw1(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void IRAM_ATTR gpio_isr_handler_sw2(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// debounce vars for digital switches
bool old_state_sw1=0, old_state_sw2=0;

static void sensor_handler_digi(void *arg) {
    uint32_t io_num;
    bool state=0;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            state = gpio_get_level(io_num);
            switch(io_num) {
                case SENSOR_SW1:
                    if (state && state != old_state_sw1) {
                        printf("TAMPERING DETECTED? sw %d\n", io_num);
                    }
                    old_state_sw1 = state;
                    break;
                case SENSOR_SW2:
                    if (state && state != old_state_sw2) {
                        printf("TAMPERING DETECTED? sw %d\n", io_num);
                    }
                    old_state_sw2 = state;
                    break;
               //default:
               //     break;
            }
            /*if (state != old_state_sw1) {
                //printf("TAMPERING DETECTED? sw %d\n", io_num);
                printf("TAMPERING DETECTED? sw\n");
            }*/

        }
    }
}

static void sensor_handler_ana(void *arg) {
    const TickType_t delay_ms = 50/portTICK_PERIOD_MS;

    for( ;; )
    {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_5,ADC_ATTEN_DB_0);
        int val = adc1_get_raw(ADC1_CHANNEL_5);
        if (val > 0) { 
            printf("TAMPERING DETECTED\t%d\n", val);
        }
        
        vTaskDelay( delay_ms );
    }
}

void app_main()
{

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
    initialize_nvs();
#if CONFIG_STORE_HISTORY
    initialize_filesystem();
#endif
    initialize_console();

    /* Register commands */
    esp_console_register_help_command(); // register master 'help' command
    register_system(); // register main command handlers
    //register_wifi(); // register wifi command handlers TODO not in prod

    // TODO prod values
    // join game WIFI network
    //wifi_join("NSL","1qaz2wsx3edc", 10);

    /////////////////////////////////////////////////
    // Tamper setupio_conf.pin_bit_mask


    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = ((1ULL<<SENSOR_SW1)|(1ULL<<SENSOR_SW2));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
    //gpio_set_intr_type(SENSOR_SW1, GPIO_INTR_POSEDGE);
    //gpio_set_intr_type(SENSOR_SW2, GPIO_INTR_HIGH_LEVEL);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //start digital sensor task
    xTaskCreate(sensor_handler_digi, "sensor_handler_digi", 2048, NULL, 10, NULL);
    //start analog sensor task
    xTaskCreate(sensor_handler_ana, "sensor_handler_ana", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(SENSOR_SW1, gpio_isr_handler_sw1, (void*) SENSOR_SW1);
    gpio_isr_handler_add(SENSOR_SW2, gpio_isr_handler_sw2, (void*) SENSOR_SW2);
   

    /////////////////////////////////////////////////
    //	Console setup
    const char* prompt = LOG_COLOR_I "cooltuna> " LOG_RESET_COLOR; // TODO prompt device name/info ?
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

    /* Main console loop */
    while(true) {
        char* line = linenoise(prompt);
        printf("TIME\t%d\n", xTaskGetTickCount());

        if (line == NULL) { /* Ignore empty lines */
            continue;
        }
        linenoiseHistoryAdd(line); //Add the command to the history
/*#if CONFIG_STORE_HISTORY
        // Save command history to filesystem
        linenoiseHistorySave(HISTORY_PATH);
#endif*/

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
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
}
