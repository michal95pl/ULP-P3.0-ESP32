#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

    #include "driver/i2c_master.h"
    #include <string.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"

    esp_err_t init_ssd1306(i2c_master_dev_handle_t *dev_handle);
    esp_err_t clear_ssd1306(i2c_master_dev_handle_t *dev_handle);

    /*
    * Function to show text on OLED display
    * x - column (0-15)
    * y - row (0-7)
    */
    esp_err_t show_text(i2c_master_dev_handle_t *dev_handle, uint8_t x, uint8_t y, char *text);

    /*
    * Function to show buffor on OLED display
    * buffor - 1024 bytes (128x8), ech byte is one column of 8 pixels
    */
    esp_err_t show_buffor(i2c_master_dev_handle_t *dev_handle, uint8_t* buffor);

    /*
    * Function to show splash screen on OLED display. Takes about 1.5 seconds.
    */
    esp_err_t show_splash_screen(i2c_master_dev_handle_t *dev_handle);

#endif