#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pcb_management.h"
#include "log.h"
#include "wifi_connection.h"
#include "stm32_spi_com.h"
#include "status_json_app.h"

pcb_data_t pcb_data;

void screen_task(void * pvParameters);

void app_main(void)
{
    if (log_init() != ESP_OK)
    {
        printf("Failed to initialize log\n");
        return;
    }

    if (init_pcb_management() != ESP_OK)
    {
        log_error("Failed to initialize PCB management");
        return;
    }

    log_info("PCB management initialized!");

    if (init_spi_stm32() != ESP_OK)
    {
        log_error("Failed to initialize SPI communication with STM32");
        return;
    }

    log_info("SPI communication with STM32 initialized!");

    update_pcb_data(&pcb_data);
    get_json_status_data(&pcb_data);

    wifi_init();
    xTaskCreate(screen_task, "screen_task", 4096, NULL, 5, NULL);

    while (1)
    {
        if (check_electronics() != ESP_OK)
        {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void screen_task(void * pvParameters)
{
    while (1)
    {
        update_pcb_data(&pcb_data);
        temperature_protection(&pcb_data);
        show_stat_screen(&pcb_data, is_client_connected());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
