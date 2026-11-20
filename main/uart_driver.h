#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t uart_init(void);

int uart_receive(uint8_t *buffer, size_t max_length);

esp_err_t uart_send(const char *data);

#endif