#include "status_json_app.h"

static pcb_data_t* pcb_data_s;

uint16_t get_json_status(uint8_t status_json[])
{
    uint16_t len = sprintf((char*)status_json, "STW{\"usb_pd\":%d,\"communication_module\":%d,\"fan\":%d,\"temp_pcb\":%.1f,\"temp_esp32\":%.1f,\"temp_stm32\":%.1f}x", 
        (pcb_data_s->voltage > 7? 1 : 0), 0, gpio_get_level(FAN_ENABLE_PIN), pcb_data_s->pcb_temp, pcb_data_s->esp_temp, pcb_data_s->stm32_temp);

    status_json[len-1] = 0x03;
    return len;
}

void get_json_status_data(pcb_data_t *pcb_data_)
{
    pcb_data_s = pcb_data_;
}