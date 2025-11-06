#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

// ===========================================================
// TEMPOS DOS LEDS - MODO DIA (SEMÁFORO PEDESTRE)
// ===========================================================
// Define quanto tempo cada cor do semáforo de pedestre fica acesa
#define TEMPO_VERMELHO_MS 4000   // 4 segundos - pedestre NÃO pode atravessar
#define TEMPO_VERDE_MS 4000      // 4 segundos - pedestre PODE atravessar

// Quando botão é pressionado, reduz o tempo vermelho para liberar rápido
#define TEMPO_VERMELHO_RAPIDO_MS 1000  // 1 segundo antes de liberar

// ===========================================================
// TEMPOS DO MODO NOTURNO - VERMELHO PISCANTE
// ===========================================================
#define TEMPO_PISCA_ON_MS 1000   // 1 segundo aceso
#define TEMPO_PISCA_OFF_MS 1000  // 1 segundo apagado

// ===========================================================
// LEDS ONBOARD (FRDM-KL25Z)
// ===========================================================
// led0 = LED Verde (pedestre pode atravessar)
// led2 = LED Vermelho (pedestre não pode atravessar)
#define LED_VERDE_NODE DT_ALIAS(led0)
#define LED_VERMELHO_NODE DT_ALIAS(led2)

#if DT_NODE_HAS_STATUS(LED_VERDE_NODE, okay)
static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if DT_NODE_HAS_STATUS(LED_VERMELHO_NODE, okay)
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);
#else
#error "Unsupported board: led2 devicetree alias is not defined"
#endif

// ===========================================================
// BOTÃO PTA16 COM PULL-UP E INTERRUPÇÃO
// ===========================================================
#define PORTA_BOTAO DT_NODELABEL(gpioa)
#define PINO_BOTAO 16

static const struct device *gpio_port_a;
static struct gpio_callback button_cb_data;
volatile bool pedestre_solicitado = false;

// ===========================================================
// ISR - BOTÃO PRESSIONADO
// ===========================================================
void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (!pedestre_solicitado) {
        pedestre_solicitado = true;
        printk("\n=== BOTÃO PRESSIONADO: PEDESTRE QUER ATRAVESSAR ===\n");
    }
}

// ===========================================================
// SINCRONIZAÇÃO ENTRE THREADS
// ===========================================================
K_MUTEX_DEFINE(led_mutex);
K_SEM_DEFINE(sem_vermelho, 1, 1);  // Vermelho começa primeiro (pedestre espera)
K_SEM_DEFINE(sem_verde, 0, 1);     // Verde espera

// ===========================================================
// CONFIGURAÇÃO DO MODO
// 0 = Modo Dia (vermelho → verde alternando)
// 1 = Modo Noite (vermelho piscante)
// ===========================================================
#define MODO_OPERACAO 0

// ===========================================================
// MODO NOTURNO - VERMELHO PISCANTE
// ===========================================================
void modo_noite(void)
{
    printk("\n** MODO NOTURNO ATIVADO **\n");
    printk("Vermelho piscando: %d ms aceso, %d ms apagado\n\n",
           TEMPO_PISCA_ON_MS, TEMPO_PISCA_OFF_MS);

    while (1) {
        // Liga vermelho
        gpio_pin_set_dt(&led_vermelho, 1);
        printk("Vermelho ACESO\n");
        k_msleep(TEMPO_PISCA_ON_MS);

        // Apaga vermelho
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("Vermelho APAGADO\n");
        k_msleep(TEMPO_PISCA_OFF_MS);
    }
}

// ===========================================================
// THREAD LED VERMELHO (Pedestre NÃO pode atravessar)
// ===========================================================
void thread_led_vermelho(void)
{
    int ret = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Vermelho\n", ret);
        return;
    }

    printk("Thread LED Vermelho (Pedestre) iniciada\n");

    while (1) {
        k_sem_take(&sem_vermelho, K_FOREVER);
        k_mutex_lock(&led_mutex, K_FOREVER);

        printk("LED VERMELHO aceso - Pedestre NÃO pode atravessar\n");
        gpio_pin_set_dt(&led_vermelho, 1);

        // Verifica se o botão foi pressionado durante o tempo vermelho
        int tempo_passado = 0;
        int tempo_total = TEMPO_VERMELHO_MS;

        while (tempo_passado < tempo_total) {
            if (pedestre_solicitado && tempo_total == TEMPO_VERMELHO_MS) {
                // Se o botão foi pressionado, reduz o tempo restante para 1 segundo
                printk("Pedestre solicitou! Acelerando para VERDE em 1 segundo...\n");
                tempo_total = tempo_passado + TEMPO_VERMELHO_RAPIDO_MS;
            }
            k_msleep(100);
            tempo_passado += 100;
        }

        gpio_pin_set_dt(&led_vermelho, 0);
        printk("LED VERMELHO apagado\n");
        k_mutex_unlock(&led_mutex);

        k_sem_give(&sem_verde);
    }
}

// ===========================================================
// THREAD LED VERDE (Pedestre PODE atravessar)
// ===========================================================
void thread_led_verde(void)
{
    int ret = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Verde\n", ret);
        return;
    }

    printk("Thread LED Verde (Pedestre) iniciada\n");

    while (1) {
        k_sem_take(&sem_verde, K_FOREVER);
        k_mutex_lock(&led_mutex, K_FOREVER);

        printk("LED VERDE aceso - Pedestre PODE atravessar\n");
        gpio_pin_set_dt(&led_verde, 1);

        k_msleep(TEMPO_VERDE_MS);  // 4 segundos para atravessar

        gpio_pin_set_dt(&led_verde, 0);
        printk("LED VERDE apagado\n");
        k_mutex_unlock(&led_mutex);

        // Reseta a flag do pedestre (já atravessou)
        pedestre_solicitado = false;

        // Volta para vermelho
        k_sem_give(&sem_vermelho);
    }
}

// ===========================================================
// DEFINIÇÃO DAS THREADS
// ===========================================================
K_THREAD_DEFINE(thread_vermelho_id, 1024, thread_led_vermelho, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(thread_verde_id, 1024, thread_led_verde, NULL, NULL, NULL, 5, 0, 0);

// ===========================================================
// FUNÇÃO PRINCIPAL
// ===========================================================
void main(void)
{
    printk("===========================================\n");
    printk("Semáforo de PEDESTRES - FRDM-KL25Z\n");
    printk("===========================================\n");

    // Configura LEDs
    int ret_verde = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    int ret_vermelho = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    if (ret_verde < 0 || ret_vermelho < 0) {
        printk("ERRO: Falha ao configurar GPIOs dos LEDs\n");
        return;
    }

    // Configura botão PTA16
    gpio_port_a = DEVICE_DT_GET(PORTA_BOTAO);
    if (!device_is_ready(gpio_port_a)) {
        printk("Erro: Porta A não está pronta!\n");
        return;
    }

    int ret_btn = gpio_pin_configure(gpio_port_a, PINO_BOTAO, GPIO_INPUT | GPIO_PULL_UP);
    if (ret_btn < 0) {
        printk("Erro %d ao configurar pino PTA16\n", ret_btn);
        return;
    }

    gpio_pin_interrupt_configure(gpio_port_a, PINO_BOTAO, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&button_cb_data, button_isr, BIT(PINO_BOTAO));
    gpio_add_callback(gpio_port_a, &button_cb_data);

    // Escolhe modo de operação
    if (MODO_OPERACAO == 1) {
        // Modo noturno
        k_thread_suspend(thread_vermelho_id);
        k_thread_suspend(thread_verde_id);
        modo_noite();
    } else {
        // Modo diurno
        printk("MODO DIURNO ATIVO\n");
        printk("Vermelho: %d ms | Verde: %d ms\n", TEMPO_VERMELHO_MS, TEMPO_VERDE_MS);
        printk("Botão PTA16: Pressione para acelerar travessia\n");
        printk("===========================================\n\n");
    }
}
