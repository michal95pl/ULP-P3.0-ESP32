#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

    #include <stdint.h>
    
    /*
    * @brief Initialize the wifi connection and create tcp server in thread
    */
    void wifi_init();
    uint8_t is_client_connected();

#endif