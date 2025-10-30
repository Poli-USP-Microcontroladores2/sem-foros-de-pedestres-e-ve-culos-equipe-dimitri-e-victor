#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

// Tempos de acendimento de cada LED - Modo Dia
#define TEMPO_VERDE_MS    3000   // 3 segundos
#define TEMPO_AMARELO_MS  1000   // 1 segundo
#define TEMPO_VERMELHO_MS 4000   // 4 segundos

// Tempos do modo noturno - Amarelo piscante
#define TEMPO_PISCA_ON_MS  1000  // 1 segundo aceso
#define TEMPO_PISCA_OFF_MS 1000  // 1 segundo apagado

// LEDs onboard da KL25Z (RGB)
#define LED_VERDE_NODE    DT_ALIAS(led0)   // LED verde
#define LED_VERMELHO_NODE DT_ALIAS(led2)   // LED vermelho
// Amarelo = Verde + Vermelho ligados juntos

// Verificação e configuração dos LEDs
#if DT_NODE_HAS_STATUS(LED_VERDE_NODE, okay)
static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if DT_NODE_HAS_STATUS(LED_VERMELHO_NODE, okay)
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);
#else
#error "Unsupported board: led1 devicetree alias is not defined"
#endif

// Mutex para exclusão mútua entre os LEDs
K_MUTEX_DEFINE(led_mutex);

// Semáforos para sincronização entre threads
K_SEM_DEFINE(sem_verde, 1, 1);      // Verde começa primeiro
K_SEM_DEFINE(sem_amarelo, 0, 1);    // Amarelo espera
K_SEM_DEFINE(sem_vermelho, 0, 1);   // Vermelho espera

// *** CONFIGURAÇÃO DO MODO ***
// 0 = Modo Dia (sequência normal)
// 1 = Modo Noite (amarelo piscante)
#define MODO_OPERACAO 0

/* =====================================================================
 * Modo Noite - Amarelo Piscante
 * ===================================================================== */
void modo_noite(void)
{
    printk("\n*** MODO NOTURNO ATIVADO ***\n");
    printk("Amarelo piscando: %d ms aceso, %d ms apagado\n\n",
           TEMPO_PISCA_ON_MS, TEMPO_PISCA_OFF_MS);

    while (1) {
        // Acende amarelo (verde + vermelho)
        gpio_pin_set_dt(&led_verde, 1);
        gpio_pin_set_dt(&led_vermelho, 1);
        printk("Amarelo ACESO\n");
        k_msleep(TEMPO_PISCA_ON_MS);

        // Apaga amarelo
        gpio_pin_set_dt(&led_verde, 0);
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("Amarelo APAGADO\n");
        k_msleep(TEMPO_PISCA_OFF_MS);
    }
}

/* =====================================================================
 * Thread LED Verde - 3 segundos
 * ===================================================================== */
void thread_led_verde(void)
{
    int ret = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Verde\n", ret);
        return;
    }

    printk("Thread LED Verde iniciada\n");

    while (1) {
        // Espera sua vez
        k_sem_take(&sem_verde, K_FOREVER);
        
        // Adquire o mutex para acesso exclusivo
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        printk("LED VERDE aceso\n");
        gpio_pin_set_dt(&led_verde, 1);
        k_msleep(TEMPO_VERDE_MS);
        gpio_pin_set_dt(&led_verde, 0);
        printk("LED VERDE apagado\n");
        
        // Libera o mutex
        k_mutex_unlock(&led_mutex);
        
        // Sinaliza para o próximo LED (amarelo)
        k_sem_give(&sem_amarelo);
    }
}

/* =====================================================================
 * Thread LED Amarelo - 1 segundo
 * (Amarelo = Verde + Vermelho ligados)
 * ===================================================================== */
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
        // Espera sua vez
        k_sem_take(&sem_amarelo, K_FOREVER);
        
        // Adquire o mutex para acesso exclusivo
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        printk("LED AMARELO aceso (Verde + Vermelho)\n");
        gpio_pin_set_dt(&led_verde, 1);
        gpio_pin_set_dt(&led_vermelho, 1);
        k_msleep(TEMPO_AMARELO_MS);
        gpio_pin_set_dt(&led_verde, 0);
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("LED AMARELO apagado\n");
        
        // Libera o mutex
        k_mutex_unlock(&led_mutex);
        
        // Sinaliza para o próximo LED (vermelho)
        k_sem_give(&sem_vermelho);
    }
}

/* =====================================================================
 * Thread LED Vermelho - 4 segundos
 * ===================================================================== */
void thread_led_vermelho(void)
{
    int ret = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Vermelho\n", ret);
        return;
    }

    printk("Thread LED Vermelho iniciada\n");

    while (1) {
        // Espera sua vez
        k_sem_take(&sem_vermelho, K_FOREVER);
        
        // Adquire o mutex para acesso exclusivo
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        printk("LED VERMELHO aceso\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        k_msleep(TEMPO_VERMELHO_MS);
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("LED VERMELHO apagado\n");
        
        // Libera o mutex
        k_mutex_unlock(&led_mutex);
        
        // Sinaliza para o próximo LED (verde - reinicia o ciclo)
        k_sem_give(&sem_verde);
    }
}

/* =====================================================================
 * Definição das threads - Modo Dia
 * ===================================================================== */
K_THREAD_DEFINE(thread_verde_id, 1024, thread_led_verde, NULL, NULL, NULL,
                5, 0, 0);

K_THREAD_DEFINE(thread_amarelo_id, 1024, thread_led_amarelo, NULL, NULL, NULL,
                5, 0, 0);

K_THREAD_DEFINE(thread_vermelho_id, 1024, thread_led_vermelho, NULL, NULL, NULL,
                5, 0, 0);

void main(void)
{
    printk("===========================================\n");
    printk("Sistema de Semáforo - FRDM-KL25Z\n");
    printk("===========================================\n");

    // Configura os pinos dos LEDs
    int ret_verde = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    int ret_vermelho = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);
    
    if (ret_verde < 0 || ret_vermelho < 0) {
        printk("ERRO: Falha ao configurar GPIOs\n");
        return;
    }

    // Verifica o modo de operação configurado
    if (MODO_OPERACAO == 1) {
        // *** MODO NOTURNO ***
        // Suspende as threads do modo dia
        k_thread_suspend(thread_verde_id);
        k_thread_suspend(thread_amarelo_id);
        k_thread_suspend(thread_vermelho_id);
        
        // Executa modo noturno (amarelo piscante)
        modo_noite();
    } else {
        // *** MODO DIA (padrão) ***
        printk("MODO DIURNO ATIVO\n");
        printk("Verde: %d ms | Amarelo: %d ms | Vermelho: %d ms\n",
               TEMPO_VERDE_MS, TEMPO_AMARELO_MS, TEMPO_VERMELHO_MS);
        printk("Sequência: Verde -> Amarelo -> Vermelho\n");
        printk("===========================================\n\n");
        
        // As threads já estão rodando automaticamente
    }
}