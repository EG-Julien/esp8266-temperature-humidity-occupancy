#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include <toggle.h>
//#include <wifi_config.h>

#include "wifi.h"

#include <dht/dht.h>

#define SENSOR_PIN 5
#define OCCUPANCY_PIN 4

void occupancy_identify(homekit_value_t _value) {
    printf("Occupancy identify\n");
}


homekit_characteristic_t occupancy_detected = HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);


void sensor_callback(bool high, void *context) {
    occupancy_detected.value = HOMEKIT_UINT8(high ? 1 : 0);
    homekit_characteristic_notify(&occupancy_detected, occupancy_detected.value);
}


static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

void temperature_sensor_identify(homekit_value_t _value) {
    printf("Temperature sensor identify\n");
}

homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);


void temperature_sensor_task(void *_args) {
    gpio_set_pullup(SENSOR_PIN, false, false);

    float humidity_value, temperature_value;
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE_DHT22, SENSOR_PIN,
            &humidity_value, &temperature_value
        );
        if (success) {
            temperature.value.float_value = temperature_value;
            humidity.value.float_value = humidity_value;

            homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature_value));
            homekit_characteristic_notify(&humidity, HOMEKIT_FLOAT(humidity_value));
        } else {
            printf("Couldnt read data from sensor\n");
        }

        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

void temperature_sensor_init() {
    xTaskCreate(temperature_sensor_task, "Temperature Sensor", 256, NULL, 2, NULL);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Kariboo Corp"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0012345"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Kariboo iTemp"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, temperature_sensor_identify),
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
            &temperature,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Humidity Sensor"),
            &humidity,
            NULL
        }),
        NULL
    }),
    HOMEKIT_ACCESSORY(.id=2, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Sensor"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Kariboo Corp"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "1"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Kariboo iOccupancy"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, occupancy_identify),
            NULL
        }),
        HOMEKIT_SERVICE(OCCUPANCY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Sensor"),
            &occupancy_detected,
            NULL
        }),
        NULL
    }),
    NULL
};

static bool homekit_initialized = false;
homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

// void on_wifi_config_event(wifi_config_event_t event) {
//     if (event == WIFI_CONFIG_CONNECTED) {
//         if (!homekit_initialized) {
//             homekit_server_init(&config);
//             homekit_initialized = true;
//         }
//     }
// }


void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_init();
    temperature_sensor_init();
    homekit_server_init(&config);

    //wifi_config_init2("occupancy-sensor", NULL, on_wifi_config_event);

    if (toggle_create(OCCUPANCY_PIN, sensor_callback, NULL)) {
        printf("Failed to initialize sensor\n");
    }
}

