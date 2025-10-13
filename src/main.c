#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define SLEEP_TIME_MS 500

// LED Verde onboard da KL25Z (LED0 no device tree)
#define LED0_NODE DT_ALIAS(led1)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

/* Thread responsável por piscar o LED */
void led_blink_thread(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led)) {
        printk("Error: LED device %s is not ready\n", led.port->name);
        return;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error %d: failed to configure LED pin\n", ret);
        return;
    }

    printk("LED blinking on %s pin %d (thread)\n", led.port->name, led.pin);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_msleep(SLEEP_TIME_MS);
    }
}

/* Criação da thread para piscar LED */
K_THREAD_DEFINE(led_thread_id, 1024, led_blink_thread, NULL, NULL, NULL,
                5, 0, 0);

void main(void)
{
    printk("Sistema iniciado na FRDM-KL25Z. Thread de LED criada.\n");
}
