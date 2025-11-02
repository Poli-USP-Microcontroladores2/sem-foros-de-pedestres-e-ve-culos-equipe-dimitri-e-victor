#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* --- Definições do Projeto --- */

// ####################################################################
// ### CONTROLE DE MODO ESTÁTICO ###
// Defina 0 para Modo Dia (ciclo V/R)
// Defina 1 para Modo Noturno (pisca vermelho)
#define MODO_NOTURNO 0
// ####################################################################

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5

// Tempos do Modo Normal (Dia)
#define TEMPO_VERDE_PEDESTRE_MS  4000
#define TEMPO_VERMELHO_PEDESTRE_MS 2000

// Tempos do Modo Noturno
#define TEMPO_PISCA_NOTURNO_MS 1000 // 1s aceso, 1s apagado

// 1. Definição dos Nós do Device Tree (led0=Verde, led2=Vermelho)
#define LED_VERDE_NODE    DT_ALIAS(led0)
#define LED_VERMELHO_NODE DT_ALIAS(led2)

// --- Verificações de Compilação (Obrigatórias) ---
#if DT_NODE_HAS_STATUS(LED_VERDE_NODE, okay)
static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
#else
#error "Unsupported board: led0 (verde) devicetree alias is not defined"
#endif

#if DT_NODE_HAS_STATUS(LED_VERMELHO_NODE, okay)
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);
#else
#error "Unsupported board: led2 (vermelho) devicetree alias is not defined"
#endif
// --- Fim das Verificações ---

// 3. Definição do Mutex (compartilhado por ambas as lógicas)
K_MUTEX_DEFINE(pedestre_mutex);

/* --- Lógica das Threads (com compilação condicional) --- */

// Thread 1: Controla o LED VERMELHO (e o Pisca Noturno)
void thread_pedestre_vermelho(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) 
    {
        k_mutex_lock(&pedestre_mutex, K_FOREVER);

#if MODO_NOTURNO == 1
        // --- LÓGICA MODO NOTURNO ---
        printk("PEDESTRE: NOTURNO (Pisca Vermelho)\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0); // Garante que o verde está apagado
        
        k_msleep(TEMPO_PISCA_NOTURNO_MS);
        
        gpio_pin_set_dt(&led_vermelho, 0);
        
        k_mutex_unlock(&pedestre_mutex);
        k_msleep(TEMPO_PISCA_NOTURNO_MS);

#else
        // --- LÓGICA MODO DIA (Ciclo Automático) ---
        printk("PEDESTRE: VERMELHO (Modo Dia)\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0);
        
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
        
        gpio_pin_set_dt(&led_vermelho, 0);
        k_mutex_unlock(&pedestre_mutex);
        
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
#endif
    }
}

// Thread 2: Controla o LED VERDE (Só funciona no Modo Dia)
void thread_pedestre_verde(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) 
    {
#if MODO_NOTURNO == 1
        // --- LÓGICA MODO NOTURNO ---
        // Não faz NADA. Apenas dorme para sempre para economizar CPU.
        k_sleep(K_FOREVER);

#else
        // --- LÓGICA MODO DIA ---
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);

        k_mutex_lock(&pedestre_mutex, K_FOREVER);

        printk("PEDESTRE: VERDE (Modo Dia)\n");
        gpio_pin_set_dt(&led_verde, 1);
        gpio_pin_set_dt(&led_vermelho, 0);

        k_msleep(TEMPO_VERDE_PEDESTRE_MS);

        gpio_pin_set_dt(&led_verde, 0);
        k_mutex_unlock(&pedestre_mutex);
#endif
    }
}


// 4. Definição Estática das Threads
K_THREAD_DEFINE(vermelho_thread_id, STACK_SIZE,
                thread_pedestre_vermelho, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, 0); 

K_THREAD_DEFINE(verde_thread_id, STACK_SIZE,
                thread_pedestre_verde, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, 0); 

/* --- Função Principal (main) --- */
void main(void)
{
    int ret; 

    // 5. Verificações de "pronto"
    if (!gpio_is_ready_dt(&led_verde) || !gpio_is_ready_dt(&led_vermelho)) {
        printk("Erro: Dispositivos de LED não estão prontos.\n");
        return; 
    }

    // 6. Configuração dos pinos
    ret = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE); // Começa desligado
    if (ret < 0) { printk("Erro: falha ao configurar LED Verde\n"); return; }
    
    ret = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_ACTIVE); // Começa ligado
    if (ret < 0) { printk("Erro: falha ao configurar LED Vermelho\n"); return; }
    
#if MODO_NOTURNO == 1
    printk("Semáforo de Pedestre (MODO NOTURNO) iniciado.\n");
#else
    printk("Semáforo de Pedestre (MODO DIA) iniciado.\n");
#endif
    
    // As threads estáticas já estão rodando.
    // A main não precisa fazer mais nada.
}