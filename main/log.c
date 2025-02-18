#include "log.h"

static SemaphoreHandle_t log_mutex;

esp_err_t log_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    if (uart_param_config(UART_NUM_0, &uart_config) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (uart_set_pin(UART_NUM_0, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (uart_driver_install(UART_NUM_0, 1024, 1024, 0, NULL, 0) != ESP_OK)
    {
        return ESP_FAIL;
    }

    log_mutex = xSemaphoreCreateMutex();

    return ESP_OK;
}

void log_info(const char *message)
{
    if (xSemaphoreTake(log_mutex, portMAX_DELAY) == pdTRUE)
    {
        uart_write_bytes(UART_NUM_0, "[INFO] ", 7);
        uart_write_bytes(UART_NUM_0, message, strlen(message));
        uart_write_bytes(UART_NUM_0, "\n", 1);

        xSemaphoreGive(log_mutex);
    }
}

void log_error(const char *message)
{
    if (xSemaphoreTake(log_mutex, portMAX_DELAY) == pdTRUE)
    {
        uart_write_bytes(UART_NUM_0, "[ERROR] ", 8);
        uart_write_bytes(UART_NUM_0, message, strlen(message));
        uart_write_bytes(UART_NUM_0, "\n", 1);

        xSemaphoreGive(log_mutex);
    }
    
}