
#include "uart_driver.h"

#include <string.h>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#define UART_PORT       UART_NUM_2
#define UART_TX_PIN     25
#define UART_RX_PIN     4
#define UART_BAUD_RATE  9600

#define UART_RX_BUFFER  2048
#define UART_TX_BUFFER  2048

esp_err_t uart_init(void)
{
    uart_config_t config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    esp_err_t err;

    // Install UART driver
    err = uart_driver_install(
        UART_PORT,
        UART_RX_BUFFER,
        UART_TX_BUFFER,
        0,
        NULL,
        0
    );

    if (err != ESP_OK)
        return err;

    // Configure communication parameters
    err = uart_param_config(UART_PORT, &config);

    if (err != ESP_OK)
        return err;

    // Configure UART GPIO pins
    err = uart_set_pin(
        UART_PORT,
        UART_TX_PIN,
        UART_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );

    if (err != ESP_OK)
        return err;

    return ESP_OK;
}

// Receive UART data
int uart_receive(uint8_t *buffer, size_t max_length)
{
    return uart_read_bytes(
        UART_PORT,
        buffer,
        max_length,
        pdMS_TO_TICKS(100)
    );
}

// Transmit UART data
esp_err_t uart_send(const char *data)
{
    if (data == NULL)
        return ESP_ERR_INVALID_ARG;

    size_t length = strlen(data);

    int written = uart_write_bytes(
        UART_PORT,
        data,
        length
    );

    if (written < 0 || (size_t)written != length)
        return ESP_FAIL;

    return uart_wait_tx_done(
        UART_PORT,
        pdMS_TO_TICKS(1000)
    );
}
