#include "pcb_management.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(a) ((a) >= 0 ? (a) : -(a))

static esp_err_t create_i2c_master_bus(i2c_master_bus_handle_t* bus_handle);
static esp_err_t add_device_to_bus(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t address);
static float read_temperature_LM75(i2c_master_dev_handle_t *dev_handle);
static float read_voltage_CYPD(i2c_master_dev_handle_t *dev_handle);
static void init_pins();
static esp_err_t start_electronics();

static temperature_sensor_handle_t temp_handle;

static i2c_master_dev_handle_t i2c_temp_sensor;
static i2c_master_dev_handle_t i2c_power_delivery;
static i2c_master_dev_handle_t i2c_ssd1306;

esp_err_t init_pcb_management()
{
    i2c_master_bus_handle_t i2c_bus_handle;
    //init i2c and add devices to bus
    if (create_i2c_master_bus(&i2c_bus_handle) != ESP_OK || 
        add_device_to_bus(&i2c_bus_handle, &i2c_temp_sensor, LM75_ADDRESS) != ESP_OK || 
        add_device_to_bus(&i2c_bus_handle, &i2c_power_delivery, CYPD_ADDRESS) != ESP_OK ||
        add_device_to_bus(&i2c_bus_handle, &i2c_ssd1306, SSD1306_ADDRESS) != ESP_OK)
        {
            log_error("Failed to initialize I2C bus");
            return ESP_FAIL;
        }

    init_pins();

    adc1_config_channel_atten(ADC1_CHANNEL_8, ADC_ATTEN_DB_11);
    adc1_config_width(ADC_WIDTH_BIT_12);

    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 50);
    temperature_sensor_install(&temp_sensor_config, &temp_handle);
    temperature_sensor_enable(temp_handle);

    if (init_ssd1306(&i2c_ssd1306) != ESP_OK)
    {
        log_error("Failed to initialize OLED display");
        return ESP_FAIL;
    }

    if (show_splash_screen(&i2c_ssd1306) != ESP_OK)
    {
        log_error("Failed to show splash screen");
        return ESP_FAIL;
    }

    if (start_electronics() != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static uint64_t last_temp_time = 0;
// non blocking function
void temperature_protection(pcb_data_t *pcb_data)
{
    if (MAX(pcb_data->pcb_temp, pcb_data->esp_temp) > 50)
    {
        last_temp_time = esp_timer_get_time() / 1000;
        gpio_set_level(FAN_ENABLE_PIN, 1);
    }
    // if the temperature is low, after 10 seconds turn off the fan
    else if ((esp_timer_get_time() / 1000) - last_temp_time > 10000)
    {
        gpio_set_level(FAN_ENABLE_PIN, 0);
    }
}

static void init_pins()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << FAN_ENABLE_PIN) | (1ULL << BUCK_REGULATOR_5V_ENABLE_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << CURRENT_FAULT_PIN) | (1ULL << POWER_DELIVERY_STATUS_PIN);
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << POWER_GOOD_5V_BUCK) | (1ULL << POWER_GOOD_STM32_LDO);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

static esp_err_t create_i2c_master_bus(i2c_master_bus_handle_t* bus_handle)
{
    i2c_master_bus_config_t conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = 0,
        .scl_io_num = I2C_MASTER_SCL_PIN,
        .sda_io_num = I2C_MASTER_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    return i2c_new_master_bus(&conf, bus_handle);
}

static esp_err_t add_device_to_bus(i2c_master_bus_handle_t* bus_handle, i2c_master_dev_handle_t* dev_handle, uint8_t address)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };

    return i2c_master_bus_add_device(*bus_handle, &dev_cfg, dev_handle);
}

static float read_temperature_LM75(i2c_master_dev_handle_t *dev_handle)
{
    uint8_t buf_send[1] = {0}; // send address of temperature register
    uint8_t buf_rcv[2];

    if (i2c_master_transmit_receive(*dev_handle, buf_send, 1, buf_rcv, 2, 1000) == ESP_OK)
    {
        int16_t temp = (buf_rcv[0] << 8) | (buf_rcv[1]);
        temp >>= 7;
        return temp * 0.5;
    }
    else
    {
        log_error("Failed to read temperature from LM75");
        return -1;
    }
}

static float read_voltage_CYPD(i2c_master_dev_handle_t *dev_handle)
{
    uint16_t voltage_reg = 0x100D;
    uint8_t buf_send[2] = {voltage_reg, voltage_reg >> 8};
    uint8_t buf_rcv[1];

    if (i2c_master_transmit_receive(*dev_handle, buf_send, 2, buf_rcv, 1, 1000) == ESP_OK)
    {
        float temp = buf_rcv[0];
        return temp / 10;
    }
    else
    {
        log_error("Failed to read voltage from CYPD");
        return -1;
    }
}

// get current from adc1 pin
float get_current_connector(adc1_channel_t channel)
{
    float current_real = 0;

    // oversampling
    for (uint8_t i=0; i < 5; i++)
    {
        float voltage = adc1_get_raw(channel) * (3.3/4095.0);
        float current = 9.226 * voltage - 14.946;
        current_real += current;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return current_real / 5;
}

void update_pcb_data(pcb_data_t *pcb_data)
{
    pcb_data->pcb_temp = read_temperature_LM75(&i2c_temp_sensor);

    float esp_temp;
    temperature_sensor_get_celsius(temp_handle, &esp_temp);
    pcb_data->esp_temp = esp_temp;

    pcb_data->stm32_temp = 0;
   
    pcb_data->voltage = read_voltage_CYPD(&i2c_power_delivery);
    pcb_data->current_connector_0 = get_current_connector(ADC_CHANNEL_7);
    pcb_data->current_connector_1 = get_current_connector(ADC_CHANNEL_8);
    pcb_data->current_connector_2 = get_current_connector(ADC_CHANNEL_9);

    if (pcb_data->current_connector_0 < 0)
        pcb_data->current_connector_0 = 0;

    if (pcb_data->current_connector_1 < 0)
        pcb_data->current_connector_1 = 0;

    if (pcb_data->current_connector_2 < 0)
        pcb_data->current_connector_2 = 0;
}

void show_stat_screen(pcb_data_t *pcb_data, uint8_t is_connected)
{
    clear_ssd1306(&i2c_ssd1306);

    show_text(&i2c_ssd1306, 0, 0, is_connected? "connected" : "disconnected");
    //show_text(&i2c_ssd1306, 0, 1, "gradient");

    char temp_str[13];
    snprintf(temp_str, sizeof(temp_str), "Temp: %.1f",MAX(pcb_data->pcb_temp, pcb_data->esp_temp));
    show_text(&i2c_ssd1306, 0, 3, temp_str);

    char voltage_str[11];
    snprintf(voltage_str, sizeof(voltage_str), "USB:  %.1fV", pcb_data->voltage);
    show_text(&i2c_ssd1306, 0, 4, voltage_str);

    char current_str[13];
    snprintf(current_str, sizeof(current_str), "Out0: %.2fA", pcb_data->current_connector_0);
    show_text(&i2c_ssd1306, 0, 5, current_str);

    snprintf(current_str, sizeof(current_str), "Out1: %.2fA", pcb_data->current_connector_1);
    show_text(&i2c_ssd1306, 0, 6, current_str);

    snprintf(current_str, sizeof(current_str), "Out2: %.2fA", pcb_data->current_connector_2);
    show_text(&i2c_ssd1306, 0, 7, current_str);
}

uint8_t buck_5v_regulator_run = 0;

static esp_err_t start_electronics()
{
    vTaskDelay(pdMS_TO_TICKS(100));

    if (read_voltage_CYPD(&i2c_power_delivery) > 6)
    {
        gpio_set_level(BUCK_REGULATOR_5V_ENABLE_PIN, 1);

        uint8_t power_good_counter_check = 0;
        while (gpio_get_level(POWER_GOOD_5V_BUCK) == 0 && power_good_counter_check < 10)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            power_good_counter_check++;
        }

        if (power_good_counter_check == 10)
        {
            log_error("5V buck regulator failed to start");
            return ESP_FAIL;
        }

        buck_5v_regulator_run = 1;
    }

    uint8_t STM32_LDO_counter_check = 0;
    while (gpio_get_level(POWER_GOOD_STM32_LDO) == 0 && STM32_LDO_counter_check < 10)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        STM32_LDO_counter_check++;
    }

    if (STM32_LDO_counter_check == 10)
    {
        log_error("STM32 LDO failed to start");
        return ESP_FAIL;
    }

    return ESP_OK;  
}

esp_err_t check_electronics()
{
    if (gpio_get_level(POWER_GOOD_STM32_LDO) == 0 || (gpio_get_level(POWER_GOOD_5V_BUCK) == 0 && buck_5v_regulator_run) || gpio_get_level(CURRENT_FAULT_PIN) == 0)
    {
        if (buck_5v_regulator_run)
        {
            gpio_set_level(BUCK_REGULATOR_5V_ENABLE_PIN, 0);
            buck_5v_regulator_run = 0;
        }
        log_error("Electronics failure detected");
        return ESP_FAIL;
    }
    return ESP_OK;
}