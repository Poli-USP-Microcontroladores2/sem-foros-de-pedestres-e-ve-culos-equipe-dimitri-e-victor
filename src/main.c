#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define SLEEP_LED0_MS 1000   // LED0: 1 segundo
#define SLEEP_LED1_MS 500    // LED1: 0,5 segundo

// LEDs onboard da KL25Z (RGB)
#define LED0_NODE DT_ALIAS(led0)   // geralmente verde
#define LED1_NODE DT_ALIAS(led1)   // geralmente vermelho

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if DT_NODE_HAS_STATUS(LED1_NODE, okay)
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
#else
#error "Unsupported board: led1 devicetree alias is not defined"
#endif

/* =====================================================================
 * Thread 1 - LED0 (1 segundo)
 * ===================================================================== */
void led0_blink_thread(void)
{
    int ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error %d: failed to configure LED0 pin\n", ret);
        return;
    }

    printk("LED0 blinking on %s pin %d every %d ms\n",
           led0.port->name, led0.pin, SLEEP_LED0_MS);

    while (1) {
        gpio_pin_toggle_dt(&led0);
        k_msleep(SLEEP_LED0_MS);
    }
}

/* =====================================================================
 * Thread 2 - LED1 (0,5 segundo)
 * ===================================================================== */
void led1_blink_thread(void)
{
    int ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error %d: failed to configure LED1 pin\n", ret);
        return;
    }

    printk("LED1 blinking on %s pin %d every %d ms\n",
           led1.port->name, led1.pin, SLEEP_LED1_MS);

    while (1) {
        gpio_pin_toggle_dt(&led1);
        k_msleep(SLEEP_LED1_MS);
    }
}

/* =====================================================================
 * Criação das threads
 * ===================================================================== */
K_THREAD_DEFINE(led0_thread_id, 1024, led0_blink_thread, NULL, NULL, NULL,
                5, 0, 0);

K_THREAD_DEFINE(led1_thread_id, 1024, led1_blink_thread, NULL, NULL, NULL,
                5, 0, 0);

void main(void)
{
    printk("Sistema iniciado na FRDM-KL25Z. Duas threads de LED criadas.\n");
}
