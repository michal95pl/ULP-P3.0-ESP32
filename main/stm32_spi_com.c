#include "stm32_spi_com.h"
#include <string.h>
#include "driver/gpio.h"

#define GPIO_MOSI 13
#define GPIO_MISO 12
#define GPIO_SCLK 14

#define GPIO_BUSY_STM 17

spi_device_handle_t spi_handle;

esp_err_t init_spi_stm32()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_BUSY_STM),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    spi_bus_config_t bus_config = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        return ret;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = NULL,
        .queue_size = 3,
        .pre_cb = NULL,
        .post_cb = NULL
    };

    return spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
}

esp_err_t send_spi_data(uint8_t data[], uint16_t length)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    WORD_ALIGNED_ATTR uint8_t sendbuf[length];
    memcpy(sendbuf, data, length);

    t.length = length * 8;
    t.tx_buffer = sendbuf;
    
    while (gpio_get_level(GPIO_BUSY_STM) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return spi_device_transmit(spi_handle, &t);
}

uint8_t dma_data_cmd[] = {'S', 'D', 'M', 0, 0};
esp_err_t send_spi_stm32_data(uint8_t data[], uint16_t length)
{
    dma_data_cmd[3] = (length >> 8) & 0xFF;
    dma_data_cmd[4] = length & 0xFF;

    esp_err_t ret;

    ret = send_spi_data(dma_data_cmd, 5); // set dma in stm32
    if (ret != ESP_OK)
        return ret;
    return send_spi_data(data, length);
}

void receive_spi_data(uint8_t data[], uint16_t length)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    WORD_ALIGNED_ATTR uint8_t rx_buf[length];

    t.length = length * 8;
    t.rxlength = length * 8;
    t.rx_buffer = rx_buf;
    t.tx_buffer = NULL;

    while (gpio_get_level(GPIO_BUSY_STM) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    spi_device_transmit(spi_handle, &t);

    printf("Received data: ");
    for (uint16_t i = 0; i < length; i++)
    {
        printf("%d", rx_buf[i]);
    }
    printf("\n");

    memcpy(data, rx_buf, length);
}

void receive_spi_stm32_data(uint8_t command[], uint16_t command_length)
{
    send_spi_stm32_data(command, command_length); // send command to stm32
    
    uint8_t length[2];
    receive_spi_data(length, 2); // receive length of data
    uint16_t data_length = (length[0] << 8) | length[1];

    uint8_t command_get[] = {'S', 'G', 'D'};
    send_spi_stm32_data(command_get, 3); // send command in order to get data

    uint8_t data[data_length];
    receive_spi_data(data, data_length); // receive data
}