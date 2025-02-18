#ifndef STATUS_JSON_APP_H
#define STATUS_JSON_APP_H

    #include <stdint.h>
    #include "pcb_management.h"

    /*
    * @brief This function get address of pcb_data to create json status in get_json_status function
    */
    void get_json_status_data(pcb_data_t *pcb_data_);

    /*
    * @brief This function create json status
    * @param status_json - pointer to buffer where will be saved json status
    * @return length of json status
    */
    uint16_t get_json_status(uint8_t status_json[]);
#endif