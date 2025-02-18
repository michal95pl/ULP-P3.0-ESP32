#ifndef PCB_MANAGEMENT_H
    #define PCB_MANAGEMENT_H

    #include <esp_err.h>
    #include "esp_timer.h"
    #include "driver\gpio.h"
    #include "driver/i2c_master.h"
    #include "driver/temperature_sensor.h"
    #include "driver/adc.h"

    #include "oled_display.h"
    #include "log.h"

    #define I2C_MASTER_SCL_PIN 7
    #define I2C_MASTER_SDA_PIN 6

    #define LM75_ADDRESS 0x4B
    #define CYPD_ADDRESS 0x08
    #define SSD1306_ADDRESS 0x3C

    #define FAN_ENABLE_PIN 16
    #define BUCK_REGULATOR_5V_ENABLE_PIN 47
    #define CURRENT_FAULT_PIN 38
    #define POWER_DELIVERY_STATUS_PIN 21
    #define POWER_GOOD_5V_BUCK 48
    #define POWER_GOOD_STM32_LDO 5

    #define CURRENT_SENSOR_0 8
    #define CURRENT_SENSOR_1 9
    #define CURRENT_SENSOR_2 10

    typedef struct {
        float pcb_temp;
        float esp_temp;
        float stm32_temp;
        float voltage;
        float current_connector_0;
        float current_connector_1;
        float current_connector_2;
    } pcb_data_t;

    esp_err_t init_pcb_management();
    void temperature_protection(pcb_data_t *pcb_data);
    void update_pcb_data(pcb_data_t *pcb_data);
    void show_stat_screen(pcb_data_t *pcb_data, uint8_t is_connected);
    esp_err_t check_electronics();

#endif