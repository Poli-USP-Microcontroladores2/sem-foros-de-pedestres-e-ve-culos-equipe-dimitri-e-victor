#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

// ===========================================================
// TEMPOS DOS LEDS - MODO DIA
// ===========================================================
#define TEMPO_VERDE_MS    3000   // 3 segundos
#define TEMPO_AMARELO_MS  1000   // 1 segundo
#define TEMPO_VERMELHO_MS 4000   // 4 segundos

// ===========================================================
// TEMPOS DO MODO NOTURNO - AMARELO PISCANTE
// ===========================================================
#define TEMPO_PISCA_ON_MS  1000  // 1 segundo aceso
#define TEMPO_PISCA_OFF_MS 1000  // 1 segundo apagado

// ===========================================================
// LEDS ONBOARD (FRDM-KL25Z)
// ===========================================================
#define LED_VERDE_NODE    DT_ALIAS(led0)
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
// BOTÃO SIMULADO COM JUMPER (PTA1 -> GND)
// ===========================================================
#define PORTA_BOTAO  DT_NODELABEL(gpioa)
#define PINO_BOTAO   1  // PTA1

static const struct device *gpio_port_a;
volatile bool pedestre_solicitado = false;

// ===========================================================
// SINCRONIZAÇÃO ENTRE THREADS
// ===========================================================
K_MUTEX_DEFINE(led_mutex);

K_SEM_DEFINE(sem_verde, 1, 1);      // Verde começa primeiro
K_SEM_DEFINE(sem_amarelo, 0, 1);    // Amarelo espera
K_SEM_DEFINE(sem_vermelho, 0, 1);   // Vermelho espera

// ===========================================================
// CONFIGURAÇÃO DO MODO
// 0 = Modo Dia (sequência normal)
// 1 = Modo Noite (amarelo piscante)
// ===========================================================
#define MODO_OPERACAO 0

// ===========================================================
// THREAD DO BOTÃO (JUMPER PTA1 -> GND)
// ===========================================================
void thread_botao(void)
{
    gpio_port_a = DEVICE_DT_GET(PORTA_BOTAO);
    if (!device_is_ready(gpio_port_a)) {
        printk("Erro: Porta A não está pronta!\n");
        return;
    }

    int ret = gpio_pin_configure(gpio_port_a, PINO_BOTAO,
                                 GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) {
        printk("Erro %d ao configurar pino do BOTAO\n", ret);
        return;
    }

    printk("Thread BOTAO iniciada - use jumper entre PTA1 e GND\n");

    while (1) {
        int estado = gpio_pin_get(gpio_port_a, PINO_BOTAO);

        if (estado == 0 && !pedestre_solicitado) {
            pedestre_solicitado = true;
            printk("\n=== JUMPER DETECTADO: PEDESTRE SOLICITOU TRAVESSIA ===\n");

            while (gpio_pin_get(gpio_port_a, PINO_BOTAO) == 0) {
                k_msleep(50);
            }
        }

        k_msleep(100);
    }
}

// ===========================================================
// MODO NOTURNO - AMARELO PISCANTE
// ===========================================================
void modo_noite(void)
{
    printk("\n*** MODO NOTURNO ATIVADO ***\n");
    printk("Amarelo piscando: %d ms aceso, %d ms apagado\n\n",
           TEMPO_PISCA_ON_MS, TEMPO_PISCA_OFF_MS);

    while (1) {
        gpio_pin_set_dt(&led_verde, 1);
        gpio_pin_set_dt(&led_vermelho, 1);
        printk("Amarelo ACESO\n");
        k_msleep(TEMPO_PISCA_ON_MS);

        gpio_pin_set_dt(&led_verde, 0);
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("Amarelo APAGADO\n");
        k_msleep(TEMPO_PISCA_OFF_MS);
    }
}

// ===========================================================
// THREAD LED VERDE
// ===========================================================
void thread_led_verde(void)
{
    int ret = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Verde\n", ret);
        return;
    }

    printk("Thread LED Verde iniciada\n");

    while (1) {
        k_sem_take(&sem_verde, K_FOREVER);

        k_mutex_lock(&led_mutex, K_FOREVER);

        printk("LED VERDE aceso\n");
        gpio_pin_set_dt(&led_verde, 1);

        int tempo_passado = 0;
        while (tempo_passado < TEMPO_VERDE_MS) {
            if (pedestre_solicitado) {
                printk("Pedestre solicitou travessia durante VERDE — indo para AMARELO\n");
                gpio_pin_set_dt(&led_verde, 0);
                k_mutex_unlock(&led_mutex);
                k_sem_give(&sem_amarelo);
                goto fim_verde;
            }
            k_msleep(100);
            tempo_passado += 100;
        }

        gpio_pin_set_dt(&led_verde, 0);
        printk("LED VERDE apagado\n");

        k_mutex_unlock(&led_mutex);

        k_sem_give(&sem_amarelo);
    fim_verde:
        ;
    }
}

// ===========================================================
// THREAD LED AMARELO
// ===========================================================
void thread_led_amarelo(void)
{
    int ret_verde = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    int ret_vermelho = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    if (ret_verde < 0 || ret_vermelho < 0) {
        printk("Erro: falha ao configurar LEDs para Amarelo\n");
        return;
    }

    printk("Thread LED Amarelo iniciada\n");

    while (1) {
        k_sem_take(&sem_amarelo, K_FOREVER);

        k_mutex_lock(&led_mutex, K_FOREVER);

        printk("LED AMARELO aceso (Verde + Vermelho)\n");
        gpio_pin_set_dt(&led_verde, 1);
        gpio_pin_set_dt(&led_vermelho, 1);
        k_msleep(TEMPO_AMARELO_MS);
        gpio_pin_set_dt(&led_verde, 0);
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("LED AMARELO apagado\n");

        k_mutex_unlock(&led_mutex);

        // se veio de pedestre, ir direto ao vermelho
        k_sem_give(&sem_vermelho);
    }
}

// ===========================================================
// THREAD LED VERMELHO
// ===========================================================
void thread_led_vermelho(void)
{
    int ret = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Vermelho\n", ret);
        return;
    }

    printk("Thread LED Vermelho iniciada\n");

    while (1) {
        k_sem_take(&sem_vermelho, K_FOREVER);

        k_mutex_lock(&led_mutex, K_FOREVER);

        printk("LED VERMELHO aceso\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        k_msleep(TEMPO_VERMELHO_MS);
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("LED VERMELHO apagado\n");

        k_mutex_unlock(&led_mutex);

        // após o vermelho (pedestre ou não), volta sempre para VERDE
        pedestre_solicitado = false;
        k_sem_give(&sem_verde);
    }
}

// ===========================================================
// DEFINIÇÃO DAS THREADS
// ===========================================================
K_THREAD_DEFINE(thread_verde_id, 1024, thread_led_verde, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(thread_amarelo_id, 1024, thread_led_amarelo, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(thread_vermelho_id, 1024, thread_led_vermelho, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(thread_botao_id, 1024, thread_botao, NULL, NULL, NULL, 5, 0, 0);

// ===========================================================
// FUNÇÃO PRINCIPAL
// ===========================================================
void main(void)
{
    printk("===========================================\n");
    printk("Sistema de Semáforo - FRDM-KL25Z\n");
    printk("===========================================\n");

    int ret_verde = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    int ret_vermelho = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    if (ret_verde < 0 || ret_vermelho < 0) {
        printk("ERRO: Falha ao configurar GPIOs dos LEDs\n");
        return;
    }

    if (MODO_OPERACAO == 1) {
        k_thread_suspend(thread_verde_id);
        k_thread_suspend(thread_amarelo_id);
        k_thread_suspend(thread_vermelho_id);
        k_thread_suspend(thread_botao_id);

        modo_noite();
    } else {
        printk("MODO DIURNO ATIVO\n");
        printk("Verde: %d ms | Amarelo: %d ms | Vermelho: %d ms\n",
               TEMPO_VERDE_MS, TEMPO_AMARELO_MS, TEMPO_VERMELHO_MS);
        printk("Use jumper entre PTA1 e GND para simular pedestre\n");
        printk("===========================================\n\n");
    }
}
