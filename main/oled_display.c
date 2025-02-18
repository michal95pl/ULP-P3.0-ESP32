#include "oled_display.h"
#include "font8x8_basic.h"
#include "splash_animation_data.h"

static const uint8_t init_commands[] = {
    0x00,

    0xAE, // Display OFF (sleep mode)

    0xA8, // Set multiplex ratio
    0x3F, // 1/64 duty (64 rows)

    0xD3, // Set display offset
    0x00, // No offset

    0x40, // Set start line address

    0xA1, // Set segment re-map

    0xC8, // Set COM output scan direction

    0xD5, // Set display clock divide ratio/oscillator frequency
    0x80, // Set divide ratio

    0xDA, // Set COM pins hardware configuration
    0x12, // Alternative COM pin configuration

    0x81, // Set contrast control
    0xFF, // Maximum contrast

    0xA4, // Entire display ON (RAM content display)

    0xDB, // Set VCOM deselect level
    0x40, // VCOMH deselect level ~0.77 * Vcc

    0x8D, // Charge pump
    0x14, // Enable charge pump

    0x2E, // Deactivate scroll
    0xA6, // Normal display (not inverted)

    0x20, // Set memory addressing mode
    0x02, // Page addressing mode

    0xAF  // Display ON in normal mode
};

esp_err_t send_command(i2c_master_dev_handle_t *dev_handle, uint8_t *command, uint8_t len)
{
    // command stream
    if (len > 1)
    {
        uint8_t buf[len + 1];
        buf[0] = 0x00;
        memcpy(&buf[1], command, len);
        return i2c_master_transmit(*dev_handle, buf, len + 1, 1000);
    }
    // single command
    else
    {
        uint8_t buf[2];
        buf[0] = 0x80;
        buf[1] = command; // command, to simplify it's not a pointer, but a value.
        return i2c_master_transmit(*dev_handle, buf, 2, 1000);
    }
}

esp_err_t send_data(i2c_master_dev_handle_t *dev_handle, uint8_t *data, size_t len)
{
    if (len > 1)
    {
        uint8_t buf[len + 1];
        buf[0] = 0x40;
        memcpy(&buf[1], data, len);
        return i2c_master_transmit(*dev_handle, buf, len + 1, 1000);
    }
    else
    {
        uint8_t buf[2] = {0xC0, data};
        return i2c_master_transmit(*dev_handle, buf, 2, 1000);
    }
}

esp_err_t init_ssd1306(i2c_master_dev_handle_t *dev_handle)
{
    if (send_command(dev_handle, init_commands, sizeof(init_commands)) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (clear_ssd1306(dev_handle) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t clear_ssd1306(i2c_master_dev_handle_t *dev_handle)
{
    // column address
    if (send_command(dev_handle, 0x00, 1) != ESP_OK) // lower
    {
        return ESP_FAIL;
    }

    if (send_command(dev_handle, 0x10, 1) != ESP_OK) // higher
    {
        return ESP_FAIL;
    }

    uint8_t data[128];

    for (uint8_t i=0; i < 128; i++)
    {
        data[i] = 0x00;
    }

    for (uint8_t i=0; i < 8; i++)
    {
        // page address
        if (send_command(dev_handle, 0xB0+i, 1) != ESP_OK)
        {
            return ESP_FAIL;
        }

        if (send_data(dev_handle, data, 128) != ESP_OK)
        {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t show_text(i2c_master_dev_handle_t *dev_handle, uint8_t x, uint8_t y, char *text)
{
    if (y > 7 || x > 15)
    {
        return ESP_FAIL;
    }

    uint8_t temp_column_addr = x * 8;

    // column address
    if (send_command(dev_handle, temp_column_addr & 0xF, 1) != ESP_OK) // lower
    {
        return ESP_FAIL;
    }

    if (send_command(dev_handle, ((temp_column_addr >> 4) & 0xF) | 0x10, 1) != ESP_OK) // higher
    {
        return ESP_FAIL;
    }

    // page address
    if (send_command(dev_handle, 0xB0 + y, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    for (uint8_t i=0; i < strlen(text); i++)
    {
        if (send_data(dev_handle, font8x8_basic_tr[(uint8_t)text[i]], 8) != ESP_OK)
        {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t show_buffor(i2c_master_dev_handle_t *dev_handle, uint8_t* buffor)
{
    if (send_command(dev_handle, 0x00, 1) != ESP_OK) // lower
    {
        return ESP_FAIL;
    }

    if (send_command(dev_handle, 0x10, 1) != ESP_OK) // higher
    {
        return ESP_FAIL;
    }

    for (uint8_t i=0; i < 8; i++)
    {
        // page address
        if (send_command(dev_handle, 0xB0+i, 1) != ESP_OK)
        {
            return ESP_FAIL;
        }

        if (send_data(dev_handle, &buffor[i*128], 128) != ESP_OK)
        {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t show_splash_screen(i2c_master_dev_handle_t *dev_handle)
{
    if (show_buffor(dev_handle, splash_screen_anim[0]) != ESP_OK)
    {
        return ESP_FAIL;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    for (uint8_t i=1; i < 15; i++)
    {
       if (show_buffor(dev_handle, splash_screen_anim[i]) != ESP_OK)
       {
           return ESP_FAIL;
       }
       vTaskDelay(40 / portTICK_PERIOD_MS);
    }
    return ESP_OK;
}