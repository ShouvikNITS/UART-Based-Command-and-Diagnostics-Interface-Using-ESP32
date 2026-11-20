
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_err.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"

#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// =====================================================
// CONFIGURATION
// =====================================================

#define CONSOLE_UART       UART_NUM_0
#define UART_BAUD_RATE     115200

#define LED_GPIO           GPIO_NUM_2

#define RX_BUFFER_SIZE     2048
#define TX_BUFFER_SIZE     2048

#define COMMAND_SIZE       64

#define HISTORY_DEPTH      5
#define EVENT_LOG_DEPTH    10
#define EVENT_TEXT_SIZE    96

#define APP_VERSION        "2.0.0"


// =====================================================
// GLOBAL VARIABLES
// =====================================================

static bool led_state = false;

static uint32_t rx_bytes = 0;
static uint32_t valid_commands = 0;
static uint32_t invalid_commands = 0;

static int64_t start_time_us = 0;


// Command history circular buffer
static char command_history[HISTORY_DEPTH][COMMAND_SIZE];

static uint8_t history_head = 0;
static uint8_t history_count = 0;


// Event log circular buffer
static char event_log[EVENT_LOG_DEPTH][EVENT_TEXT_SIZE];

static uint8_t event_head = 0;
static uint8_t event_count = 0;


// =====================================================
// SYSTEM TIME
// =====================================================

static uint32_t get_uptime_seconds(void)
{
    return (uint32_t)(
        (esp_timer_get_time() - start_time_us) / 1000000
    );
}


// =====================================================
// EVENT LOGGER
// =====================================================

static void record_event(const char *message)
{
    snprintf(
        event_log[event_head],
        EVENT_TEXT_SIZE,
        "[%lu sec] %s",
        (unsigned long)get_uptime_seconds(),
        message
    );

    event_head = (event_head + 1) % EVENT_LOG_DEPTH;

    if (event_count < EVENT_LOG_DEPTH)
    {
        event_count++;
    }
}


static void print_event_log(void)
{
    printf("\r\n========== EVENT LOG ==========\r\n");

    if (event_count == 0)
    {
        printf("No events recorded.\r\n");
        return;
    }

    uint8_t start =
        (event_head + EVENT_LOG_DEPTH - event_count)
        % EVENT_LOG_DEPTH;

    for (uint8_t i = 0; i < event_count; i++)
    {
        uint8_t index =
            (start + i) % EVENT_LOG_DEPTH;

        printf("%u. %s\r\n",
               (unsigned)(i + 1),
               event_log[index]);
    }

    printf("================================\r\n");
}


// =====================================================
// COMMAND HISTORY
// =====================================================

static void save_command_history(const char *command)
{
    snprintf(
        command_history[history_head],
        COMMAND_SIZE,
        "%s",
        command
    );

    history_head = (history_head + 1) % HISTORY_DEPTH;

    if (history_count < HISTORY_DEPTH)
    {
        history_count++;
    }
}


static void print_command_history(void)
{
    printf("\r\n======== COMMAND HISTORY ========\r\n");

    if (history_count == 0)
    {
        printf("No commands recorded.\r\n");
        return;
    }

    uint8_t start =
        (history_head + HISTORY_DEPTH - history_count)
        % HISTORY_DEPTH;

    for (uint8_t i = 0; i < history_count; i++)
    {
        uint8_t index =
            (start + i) % HISTORY_DEPTH;

        printf("%u. %s\r\n",
               (unsigned)(i + 1),
               command_history[index]);
    }

    printf("=================================\r\n");
}


// =====================================================
// UART INITIALIZATION
// =====================================================

static esp_err_t uart_console_init(void)
{
    uart_config_t config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    esp_err_t err = uart_driver_install(
        CONSOLE_UART,
        RX_BUFFER_SIZE,
        TX_BUFFER_SIZE,
        0,
        NULL,
        0
    );

    if (err != ESP_OK)
        return err;

    err = uart_param_config(CONSOLE_UART, &config);

    if (err != ESP_OK)
        return err;

    // Use interrupt-driven, blocking UART VFS operations.
    uart_vfs_dev_use_driver(CONSOLE_UART);

    // Disable standard I/O buffering.
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    return ESP_OK;
}


// =====================================================
// GPIO INITIALIZATION
// =====================================================

static esp_err_t gpio_init(void)
{
    esp_err_t err = gpio_reset_pin(LED_GPIO);

    if (err != ESP_OK)
        return err;

    err = gpio_set_direction(
        LED_GPIO,
        GPIO_MODE_OUTPUT
    );

    if (err != ESP_OK)
        return err;

    err = gpio_set_level(LED_GPIO, 0);

    led_state = false;

    return err;
}


static bool set_led(bool state)
{
    esp_err_t err = gpio_set_level(
        LED_GPIO,
        state ? 1 : 0
    );

    if (err != ESP_OK)
        return false;

    led_state = state;

    return true;
}


// =====================================================
// COMMAND NORMALIZATION
// =====================================================

static void normalize_command(char *command)
{
    char *start = command;

    // Remove leading spaces
    while (*start == ' ' || *start == '\t')
    {
        start++;
    }

    if (start != command)
    {
        memmove(
            command,
            start,
            strlen(start) + 1
        );
    }

    // Remove trailing spaces
    size_t length = strlen(command);

    while (length > 0 &&
           isspace((unsigned char)command[length - 1]))
    {
        command[--length] = '\0';
    }

    // Convert to uppercase
    for (size_t i = 0; command[i] != '\0'; i++)
    {
        command[i] = (char)toupper(
            (unsigned char)command[i]
        );
    }
}


// =====================================================
// DIAGNOSTICS
// =====================================================

static void print_uptime(void)
{
    printf("System uptime: %lu seconds\r\n",
           (unsigned long)get_uptime_seconds());
}


static void print_heap(void)
{
    size_t free_heap =
        heap_caps_get_free_size(MALLOC_CAP_8BIT);

    size_t minimum_heap =
        heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    size_t largest_block =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    printf("\r\n========== MEMORY DIAGNOSTICS ==========\r\n");

    printf("Total free heap       : %lu bytes\r\n",
           (unsigned long)free_heap);

    printf("Minimum free heap     : %lu bytes\r\n",
           (unsigned long)minimum_heap);

    printf("Largest free block    : %lu bytes\r\n",
           (unsigned long)largest_block);

    printf("========================================\r\n");
}


static void print_status(void)
{
    printf("\r\n========== SYSTEM STATUS ==========\r\n");

    printf("Device          : ESP32 DevKit V1\r\n");
    printf("Application     : UART Diagnostics\r\n");
    printf("Firmware        : %s\r\n", APP_VERSION);

    printf("ESP-IDF         : %s\r\n",
           esp_get_idf_version());

    printf("System          : ACTIVE\r\n");
    printf("UART            : UART0\r\n");
    printf("Baud rate       : %d\r\n", UART_BAUD_RATE);
    printf("Format          : 8-N-1\r\n");

    printf("Uptime          : %lu seconds\r\n",
           (unsigned long)get_uptime_seconds());

    printf("LED state       : %s\r\n",
           led_state ? "ON" : "OFF");

    printf("Valid commands  : %lu\r\n",
           (unsigned long)valid_commands);

    printf("Invalid commands: %lu\r\n",
           (unsigned long)invalid_commands);

    printf("Received bytes  : %lu\r\n",
           (unsigned long)rx_bytes);

    printf("===================================\r\n");
}


static void print_statistics(void)
{
    printf("\r\n======= COMMUNICATION STATS =======\r\n");

    printf("UART RX bytes   : %lu\r\n",
           (unsigned long)rx_bytes);

    printf("Valid commands  : %lu\r\n",
           (unsigned long)valid_commands);

    printf("Invalid commands: %lu\r\n",
           (unsigned long)invalid_commands);

    printf("History entries : %u\r\n",
           (unsigned)history_count);

    printf("Event log entries: %u\r\n",
           (unsigned)event_count);

    printf("==================================\r\n");
}


// =====================================================
// SELF TEST
// =====================================================

static void run_self_test(void)
{
    printf("\r\n========== EMBEDDED SELF TEST ==========\r\n");

    // Receiving this command proves that the UART
    // command input path is operational.
    printf("UART command input : PASS\r\n");

    // Test GPIO driver operations and restore LED state.
    esp_err_t led_on_result =
        gpio_set_level(LED_GPIO, 1);

    esp_err_t led_off_result =
        gpio_set_level(LED_GPIO, 0);

    esp_err_t restore_result =
        gpio_set_level(LED_GPIO, led_state ? 1 : 0);

    bool gpio_ok =
        (led_on_result == ESP_OK) &&
        (led_off_result == ESP_OK) &&
        (restore_result == ESP_OK);

    printf("GPIO driver        : %s\r\n",
           gpio_ok ? "PASS" : "FAIL");

    // Check that memory is available.
    size_t free_heap =
        heap_caps_get_free_size(MALLOC_CAP_8BIT);

    bool memory_ok = (free_heap > 0);

    printf("Heap availability  : %s\r\n",
           memory_ok ? "PASS" : "FAIL");

    if (gpio_ok && memory_ok)
    {
        printf("OVERALL RESULT     : PASS\r\n");
    }
    else
    {
        printf("OVERALL RESULT     : FAIL\r\n");
    }

    printf("========================================\r\n");
}


// =====================================================
// COMMAND ERROR HANDLER
// =====================================================

static void reject_command(const char *reason)
{
    invalid_commands++;

    record_event(reason);

    printf("ERROR: %s\r\n", reason);
    printf("Type HELP to view available commands.\r\n");
}


// =====================================================
// COMMAND HANDLER
// =====================================================

static void execute_command(char *command)
{
    normalize_command(command);

    if (command[0] == '\0')
        return;

    // Record every non-empty command.
    save_command_history(command);

    char event_message[EVENT_TEXT_SIZE];

    snprintf(
        event_message,
        sizeof(event_message),
        "Command: %s",
        command
    );

    record_event(event_message);


    // ---------------- HELP ----------------

    if (strcmp(command, "HELP") == 0)
    {
        valid_commands++;

        printf("\r\nAvailable Commands:\r\n");

        printf("HELP             - Show commands\r\n");
        printf("PING             - Communication test\r\n");

        printf("LED ON           - Turn LED ON\r\n");
        printf("LED OFF          - Turn LED OFF\r\n");
        printf("LED BLINK <n>    - Blink LED n times\r\n");

        printf("GET STATUS       - System information\r\n");
        printf("GET UPTIME       - Device uptime\r\n");
        printf("GET HEAP         - Memory diagnostics\r\n");
        printf("GET VERSION      - Firmware version\r\n");
        printf("GET STATS        - Communication statistics\r\n");

        printf("HISTORY          - Recent commands\r\n");
        printf("CLEAR HISTORY    - Clear command history\r\n");

        printf("GET LOG          - Display event log\r\n");
        printf("CLEAR LOG        - Clear event log\r\n");

        printf("SELF TEST        - Run embedded self-test\r\n");
        printf("CLEAR STATS      - Clear statistics\r\n");
        printf("RESET            - Restart ESP32\r\n");

        return;
    }


    // ---------------- PING ----------------

    if (strcmp(command, "PING") == 0)
    {
        valid_commands++;

        printf("PONG: UART COMMUNICATION ACTIVE\r\n");

        return;
    }


    // ---------------- LED ON ----------------

    if (strcmp(command, "LED ON") == 0)
    {
        if (set_led(true))
        {
            valid_commands++;

            printf("OK: LED TURNED ON\r\n");

            record_event("LED turned ON");
        }
        else
        {
            reject_command("GPIO operation failed");
        }

        return;
    }


    // ---------------- LED OFF ----------------

    if (strcmp(command, "LED OFF") == 0)
    {
        if (set_led(false))
        {
            valid_commands++;

            printf("OK: LED TURNED OFF\r\n");

            record_event("LED turned OFF");
        }
        else
        {
            reject_command("GPIO operation failed");
        }

        return;
    }


    // ---------------- LED BLINK ----------------

    if (strncmp(command, "LED BLINK", 9) == 0 &&
        (command[9] == '\0' || command[9] == ' '))
    {
        const char *argument = command + 9;

        while (*argument == ' ')
        {
            argument++;
        }

        char *end_pointer = NULL;

        unsigned long count =
            strtoul(argument, &end_pointer, 10);

        if (argument[0] == '\0' ||
            end_pointer == argument ||
            *end_pointer != '\0' ||
            count < 1 ||
            count > 10)
        {
            reject_command(
                "Use LED BLINK followed by a count from 1 to 10"
            );

            return;
        }

        valid_commands++;

        bool previous_state = led_state;

        printf("Blinking LED %lu times...\r\n", count);

        for (unsigned long i = 0; i < count; i++)
        {
            gpio_set_level(LED_GPIO, 1);

            vTaskDelay(pdMS_TO_TICKS(200));

            gpio_set_level(LED_GPIO, 0);

            vTaskDelay(pdMS_TO_TICKS(200));
        }

        gpio_set_level(
            LED_GPIO,
            previous_state ? 1 : 0
        );

        printf("OK: LED BLINK COMPLETED\r\n");

        record_event("LED blink completed");

        return;
    }


    // ---------------- STATUS ----------------

    if (strcmp(command, "GET STATUS") == 0)
    {
        valid_commands++;

        print_status();

        return;
    }


    // ---------------- UPTIME ----------------

    if (strcmp(command, "GET UPTIME") == 0)
    {
        valid_commands++;

        print_uptime();

        return;
    }


    // ---------------- HEAP ----------------

    if (strcmp(command, "GET HEAP") == 0)
    {
        valid_commands++;

        print_heap();

        return;
    }


    // ---------------- VERSION ----------------

    if (strcmp(command, "GET VERSION") == 0)
    {
        valid_commands++;

        printf("Application version: %s\r\n",
               APP_VERSION);

        printf("ESP-IDF version: %s\r\n",
               esp_get_idf_version());

        return;
    }


    // ---------------- STATISTICS ----------------

    if (strcmp(command, "GET STATS") == 0)
    {
        valid_commands++;

        print_statistics();

        return;
    }


    // ---------------- HISTORY ----------------

    if (strcmp(command, "HISTORY") == 0)
    {
        valid_commands++;

        print_command_history();

        return;
    }


    // ---------------- CLEAR HISTORY ----------------

    if (strcmp(command, "CLEAR HISTORY") == 0)
    {
        valid_commands++;

        history_head = 0;
        history_count = 0;

        printf("OK: COMMAND HISTORY CLEARED\r\n");

        record_event("Command history cleared");

        return;
    }


    // ---------------- EVENT LOG ----------------

    if (strcmp(command, "GET LOG") == 0)
    {
        valid_commands++;

        print_event_log();

        return;
    }


    // ---------------- CLEAR EVENT LOG ----------------

    if (strcmp(command, "CLEAR LOG") == 0)
    {
        valid_commands++;

        event_head = 0;
        event_count = 0;

        printf("OK: EVENT LOG CLEARED\r\n");

        record_event("Event log cleared");

        return;
    }


    // ---------------- SELF TEST ----------------

    if (strcmp(command, "SELF TEST") == 0)
    {
        valid_commands++;

        run_self_test();

        record_event("Self-test executed");

        return;
    }


    // ---------------- CLEAR STATISTICS ----------------

    if (strcmp(command, "CLEAR STATS") == 0)
    {
        rx_bytes = 0;
        valid_commands = 0;
        invalid_commands = 0;

        printf("OK: COMMUNICATION STATISTICS CLEARED\r\n");

        record_event("Statistics cleared");

        return;
    }


    // ---------------- RESET ----------------

    if (strcmp(command, "RESET") == 0)
    {
        valid_commands++;

        record_event("Software reset requested");

        printf("ESP32 RESTARTING...\r\n");

        fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(500));

        esp_restart();

        return;
    }


    // ---------------- UNKNOWN COMMAND ----------------

    reject_command("UNKNOWN COMMAND");
}


// =====================================================
// MAIN APPLICATION
// =====================================================

void app_main(void)
{
    // Configure UART0
    esp_err_t err = uart_console_init();

    if (err != ESP_OK)
    {
        printf("UART initialization failed: %s\r\n",
               esp_err_to_name(err));

        return;
    }

    // Configure onboard LED
    err = gpio_init();

    if (err != ESP_OK)
    {
        printf("GPIO initialization failed: %s\r\n",
               esp_err_to_name(err));

        return;
    }

    // Initialize application timer
    start_time_us = esp_timer_get_time();

    record_event("System booted");

    printf("\r\n");
    printf("============================================\r\n");
    printf(" ESP32 COMMAND AND DIAGNOSTICS INTERFACE\r\n");
    printf("============================================\r\n");

    printf("Device          : ESP32 DevKit V1\r\n");
    printf("Firmware        : %s\r\n", APP_VERSION);
    printf("ESP-IDF         : %s\r\n", esp_get_idf_version());

    printf("UART            : UART0\r\n");
    printf("Baud rate       : %d\r\n", UART_BAUD_RATE);
    printf("Format          : 8-N-1\r\n");

    printf("GPIO2           : LED READY\r\n");

    printf("\r\nSystem initialized successfully!\r\n");

    printf("Type HELP to view commands.\r\n\r\n");

    printf("CMD> ");

    // Command reception buffer
    char command[COMMAND_SIZE];

    size_t index = 0;

    bool overflow = false;


    // Continuous command processing loop
    while (1)
    {
        int received = fgetc(stdin);

        if (received == EOF)
        {
            clearerr(stdin);

            vTaskDelay(pdMS_TO_TICKS(5));

            continue;
        }

        char ch = (char)received;

        rx_bytes++;


        // Ignore carriage return
        if (ch == '\r')
        {
            continue;
        }


        // End of command
        if (ch == '\n')
        {
            printf("\r\n");

            if (overflow)
            {
                invalid_commands++;

                record_event("Oversized command rejected");

                printf("ERROR: COMMAND TOO LONG\r\n");
            }
            else
            {
                command[index] = '\0';

                execute_command(command);
            }

            index = 0;

            overflow = false;

            printf("\r\nCMD> ");

            continue;
        }


        // Backspace handling
        if (ch == '\b' || ch == 127)
        {
            if (index > 0 && !overflow)
            {
                index--;

                printf("\b \b");
            }

            continue;
        }


        // Accept printable characters
        if (isprint((unsigned char)ch))
        {
            if (index < COMMAND_SIZE - 1 && !overflow)
            {
                command[index++] = ch;

                putchar(ch);
            }
            else
            {
                overflow = true;
            }
        }
    }
}
