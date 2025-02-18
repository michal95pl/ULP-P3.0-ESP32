#ifndef LOG_H
#define LOG_H

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <esp_err.h>

esp_err_t log_init();

/*
* @brief Log an info message to UART 0. Thread safe.
*/
void log_info(const char *message);

/*
* @brief Log an error message to UART 0. Thread safe.
*/
void log_error(const char *message);

#endif