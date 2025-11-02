#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* --- Definições do Projeto --- */
#define STACK_SIZE 1024
#define THREAD_PRIORITY 5
#define TEMPO_VERDE_PEDESTRE_MS  4000
#define TEMPO_VERMELHO_PEDESTRE_MS 2000 // Tempo "mínimo" que fica vermelho

// 1. Define os "nós" do Device Tree 
// led0 = Verde
// led2 = Vermelho
#define LED_VERDE_NODE    DT_ALIAS(led0)
#define LED_VERMELHO_NODE DT_ALIAS(led2)

// 2. Verificação de Tempo de Compilação (Verde)
#if DT_NODE_HAS_STATUS(LED_VERDE_NODE, okay)
static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
#else
#error "Unsupported board: led0 (verde) devicetree alias is not defined"
#endif

// 2. Verificação de Tempo de Compilação (Vermelho)
#if DT_NODE_HAS_STATUS(LED_VERMELHO_NODE, okay)
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);
#else
#error "Unsupported board: led2 (vermelho) devicetree alias is not defined"
#endif

// 3. Definição do Mutex
K_MUTEX_DEFINE(pedestre_mutex);

/* --- Lógica das Threads --- */

// Thread 1: Controla o LED VERMELHO (Começa primeiro)
void thread_pedestre_vermelho(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) {
        // Pega o mutex primeiro - Estado inicial é VERMELHO
        k_mutex_lock(&pedestre_mutex, K_FOREVER);

        printk("PEDESTRE: VERMELHO\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0);
        
        // Fica vermelho pelo seu tempo
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
        
        gpio_pin_set_dt(&led_vermelho, 0);

        // Libera o mutex para o verde (se ele for acionado)
        k_mutex_unlock(&pedestre_mutex);
        
        // Espera o verde terminar antes de tentar pegar o mutex de novo
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
    }
}

// Thread 2: Controla o LED VERDE (Espera o vermelho)
void thread_pedestre_verde(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) {
        // Espera o tempo do vermelho passar (sincronização inicial)
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);

        // Tenta pegar o mutex (que foi liberado pelo vermelho)
        k_mutex_lock(&pedestre_mutex, K_FOREVER);

        printk("PEDESTRE: VERDE\n");
        gpio_pin_set_dt(&led_verde, 1);
        gpio_pin_set_dt(&led_vermelho, 0);

        k_msleep(TEMPO_VERDE_PEDESTRE_MS);

        gpio_pin_set_dt(&led_verde, 0);
        
        // Libera o mutex para o vermelho
        k_mutex_unlock(&pedestre_mutex);
    }
}


// 4. Definição Estática das Threads
K_THREAD_DEFINE(vermelho_thread_id, STACK_SIZE,
                thread_pedestre_vermelho, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, 0); // Vermelho inicia imediatamente

K_THREAD_DEFINE(verde_thread_id, STACK_SIZE,
                thread_pedestre_verde, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, 0); // Verde também inicia, mas dorme primeiro

/* --- Função Principal (main) --- */
void main(void)
{
    int ret; 

    // 5. Verificações de "pronto"
    if (!gpio_is_ready_dt(&led_verde)) {
        printk("Erro: LED Verde (led0) device %s is not ready\n", led_verde.port->name);
        return; 
    }
    if (!gpio_is_ready_dt(&led_vermelho)) {
        printk("Erro: LED Vermelho (led2) device %s is not ready\n", led_vermelho.port->name);
        return; 
    }

    // 6. Configuração dos pinos
    ret = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Verde (led0)\n", ret);
        return; 
    }
    ret = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Vermelho (led2)\n", ret);
        return; 
    }

    // Garante que o estado inicial é VERMELHO ACESO
    // (A thread vermelha vai fazer isso, mas é bom garantir)
    gpio_pin_set_dt(&led_verde, 0);
    gpio_pin_set_dt(&led_vermelho, 1);
    
    printk("Semáforo de Pedestre (2 Threads) iniciado. Estado inicial: VERMELHO.\n");
    // As threads estáticas já estão rodando.
}