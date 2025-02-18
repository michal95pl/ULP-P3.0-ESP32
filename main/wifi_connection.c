#include "wifi_connection.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"

#include "log.h"
#include "status_json_app.h"
#include "stm32_spi_com.h"

static uint16_t convert_2byte_utf_to_1byte(uint8_t buffor[], uint8_t converted_buffor[], uint16_t length_buffor);
static void tcp_server_task(void * pvParameters);
static void debug_print_buffor(uint8_t buffor[], uint16_t length_buffor);

void wifi_init()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ULP",
            .ssid_len = 3,
            .channel = 1,
            .password = "2462123456",
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .pmf_cfg = {
                .required = true,
            },
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    
    log_info("WiFi initialized");

    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}

uint8_t rx_buffer[3100];
uint8_t rx_buffer_converted[3100];


uint8_t keepAlive = 1;
uint8_t keepIdle = 5;
uint8_t keepInterval = 5;
uint8_t keepCount = 3;

uint8_t client_connected = 0;
uint8_t status_json[250];

static void tcp_server_task(void * pvParameters)
{
    uint8_t ip_protocol = 0;

    // ipv4
    struct sockaddr_storage dest_addr;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(1234);
    ip_protocol = IPPROTO_IP;


    int sock = socket(AF_INET, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
        log_error("Unable to create socket");
        vTaskDelete(NULL);
        return;
    }

    uint8_t opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int bind_err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (bind_err != 0) {
        log_error("Socket unable to bind");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    log_info("Socket created");

    int listen_err = listen(sock, 1);
    if (listen_err != 0) {
        log_error("Error occurred during listen");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        client_connected = 0;

        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int client_sock = accept(sock, (struct sockaddr *)&source_addr, &addr_len);

        if (client_sock < 0) {
            log_error("Unable to accept connection");
        } else {
            client_connected = 1;
            log_info("Client connected");

            setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(uint8_t));
            setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(uint8_t));
            setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(uint8_t));
            setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(uint8_t));

            while (1)
            {
                int16_t len_rcv = recv(client_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                if (len_rcv < 0) {
                    log_error("Error occurred during receiving");
                    break;
                } else if (len_rcv == 0) {
                    log_info("Connection closed");
                    break;
                }
                else
                {
                    len_rcv = convert_2byte_utf_to_1byte(rx_buffer, rx_buffer_converted, len_rcv);

                    debug_print_buffor(rx_buffer_converted, len_rcv);
                    
                    if (rx_buffer_converted[0] == 'W')
                    {
                        if (send_spi_stm32_data(rx_buffer_converted, len_rcv) != ESP_OK)
                        {
                            log_error("Error occurred during sending data to STM32");
                        }
                    }

                    // ack
                    uint8_t test_send[] = {'W', 'C', 'A', 0x03};
                    int16_t err_send = send(client_sock, test_send, sizeof(test_send), 0);
                    if (err_send < 0) {
                        log_error("Error occurred during sending");
                        break;
                    }
                    
                    if (rx_buffer_converted[0] == 'R' && rx_buffer_converted[1] == 'T' && rx_buffer_converted[2] == 'S')
                    {
                        uint16_t len_status = get_json_status(status_json);
                        debug_print_buffor(status_json, len_status);
                        send(client_sock, status_json, len_status, 0);
                    }
                }
            }
        }
    }
}

/*
* Function convert 2 byte utf8 to 1 byte utf8 if present
* @param buffor - pointer to buffor contains 2 byte utf8
* @param converted_buffor - pointer to buffor where will be saved converted utf8
* @param length_buffor - length of buffor
* @return converted_length - length of converted buffor
*/
static uint16_t convert_2byte_utf_to_1byte(uint8_t buffor[], uint8_t converted_buffor[], uint16_t length_buffor)
{
    uint16_t converted_length = 0;
    for (uint16_t i = 0; i < length_buffor; i++)
    {
        if (buffor[i] > 127)
        {
            uint8_t temp = (buffor[i++] & 0x3F) << 6;
            temp |= buffor[i] & 0x3F;
            converted_buffor[converted_length++] = temp;
        }
        else
        {
            converted_buffor[converted_length++] = buffor[i];
        }
    }
    return converted_length;
}

uint8_t is_client_connected()
{
    return client_connected;
}

static void debug_print_buffor(uint8_t buffor[], uint16_t length_buffor)
{
    for (uint16_t i = 0; i < length_buffor; i++)
    {
        if (buffor[i] >= 32 && buffor[i] <= 126)
        {
            printf("%c", buffor[i]);
        }
        else
        {
            printf("[%d]", buffor[i]);
        }
    }
    printf("\n");
}