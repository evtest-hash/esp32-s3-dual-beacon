#include "status_led.h"
#include "beacon_config.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "status_led";

static void toggle(void *arg)
{
    static bool on = false;
    (void)arg;
    on = !on;
    gpio_set_level(LED_GPIO, on);
}

void status_led_start(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    const esp_timer_create_args_t args = {
        .callback = toggle,
        .name     = "status_led",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, LED_TOGGLE_MS * 1000));

    ESP_LOGI(TAG, "status LED on GPIO%d, toggling every %d ms", LED_GPIO, LED_TOGGLE_MS);
}
