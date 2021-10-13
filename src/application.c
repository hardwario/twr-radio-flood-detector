#include <application.h>

#define SERVICE_INTERVAL_INTERVAL (60 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)
#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL (10 * 1000)

#define FLOOD_DETECTOR_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define FLOOD_DETECTOR_UPDATE_SERVICE_INTERVAL (1 * 1000)
#define FLOOD_DETECTOR_UPDATE_NORMAL_INTERVAL  (5 * 1000)

#define RADIO_FLOOD_DETECTOR        0x0d

// LED instance
twr_led_t led;

// Button instance
twr_button_t button;

// Thermometer instance
twr_tmp112_t tmp112;
static event_param_t temperature_event_param = { .next_pub = 0 };

// Flood detector instance
twr_flood_detector_t flood_detector;
event_param_t flood_detector_event_param = { .next_pub = 0 };

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        twr_led_pulse(&led, 100);
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_radio_pub_battery(&voltage);
        }
    }
}

void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        if (twr_tmp112_get_temperature_celsius(self, &value))
        {
            if ((fabsf(value - param->value) >= TEMPERATURE_PUB_VALUE_CHANGE) || (param->next_pub < twr_scheduler_get_spin_tick()))
            {
                twr_radio_pub_temperature(param->channel, &value);

                param->value = value;
                param->next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void flood_detector_event_handler(twr_flood_detector_t *self, twr_flood_detector_event_t event, void *event_param)
{
    bool is_alarm;
    event_param_t *param = (event_param_t *)event_param;


    if (event == TWR_FLOOD_DETECTOR_EVENT_UPDATE)
    {
       is_alarm = twr_flood_detector_is_alarm(self);

       if ((is_alarm != param->value) || (param->next_pub < twr_scheduler_get_spin_tick()))
       {
           twr_radio_pub_bool("flood-detector/a/alarm", &is_alarm);

           param->value = is_alarm;
           param->next_pub = twr_scheduler_get_spin_tick() + FLOOD_DETECTOR_NO_CHANGE_INTEVAL;
       }
    }
}

void switch_to_normal_mode_task(void *param)
{
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_NORMAL_INTERVAL);

    twr_flood_detector_set_update_interval(&flood_detector, FLOOD_DETECTOR_UPDATE_NORMAL_INTERVAL);

    twr_scheduler_unregister(twr_scheduler_get_current_task_id());
}

void application_init(void)
{
    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_scan_interval(&button, 20);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize thermometer sensor on core module
    temperature_event_param.channel = TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, &temperature_event_param);
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_SERVICE_INTERVAL);

    // Initialize flood detector
    twr_flood_detector_init(&flood_detector, TWR_FLOOD_DETECTOR_TYPE_LD_81_SENSOR_MODULE_CHANNEL_A);
    twr_flood_detector_set_event_handler(&flood_detector, flood_detector_event_handler, &flood_detector_event_param);
    twr_flood_detector_set_update_interval(&flood_detector, FLOOD_DETECTOR_UPDATE_SERVICE_INTERVAL);

    twr_radio_pairing_request("flood-detector", VERSION);

    twr_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);

    twr_led_pulse(&led, 2000);
}
