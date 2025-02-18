#ifndef STM32_SPI_COM_H
    #define STM32_SPI_COM_H

    #include "stm32_spi_com.h"

    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"

    #include "driver/spi_master.h"
    #include "driver/gpio.h"

    esp_err_t init_spi_stm32();
    esp_err_t send_spi_stm32_data(uint8_t data[], uint16_t length);
    void receive_spi_stm32_data(uint8_t command[], uint16_t command_length);

#endif