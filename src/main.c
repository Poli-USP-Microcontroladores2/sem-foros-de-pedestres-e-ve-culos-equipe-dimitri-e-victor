#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* --- Definições do Projeto --- */

// ####################################################################
// ### CONTROLE DE MODO ESTÁTICO ###
// Defina 0 para Modo Normal (Dia - ciclo automático V/R)
// Defina 1 para Modo Noturno (pisca vermelho)
// Defina 2 para Modo Botão (vermelho até o botão ser pressionado)
#define MODO_OPERACAO 0
// ####################################################################

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5

// Tempos do Modo Normal (Dia) / Botão
#define TEMPO_VERDE_PEDESTRE_MS  4000
#define TEMPO_VERMELHO_PEDESTRE_MS 2000

// Tempos do Modo Noturno
#define TEMPO_PISCA_NOTURNO_MS 1000 // 1s aceso, 1s apagado

// 1. Definição dos Nós do Device Tree
#define LED_VERDE_NODE    DT_ALIAS(led0) // led0 = Verde
#define LED_VERMELHO_NODE DT_ALIAS(led2) // led2 = Vermelho
#define BOTAO_NODE        DT_ALIAS(sw0)  // sw0 = Botão (usado apenas no MODO 2)

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

// A verificação do botão só é necessária se estivermos no MODO 2
#if MODO_OPERACAO == 2
    #if DT_NODE_HAS_STATUS(BOTAO_NODE, okay)
    static const struct gpio_dt_spec botao = GPIO_DT_SPEC_GET(BOTAO_NODE, gpios);
    #else
    #error "Unsupported board: sw0 (botao) devicetree alias is not defined"
    #endif
#endif
// --- Fim das Verificações ---

// 3. Primitivas do RTOS
K_MUTEX_DEFINE(pedestre_mutex); // Usado nos modos 0 e 2

// O semáforo e a ISR só são definidos/usados se estivermos no MODO 2
#if MODO_OPERACAO == 2
    K_SEM_DEFINE(sem_iniciar_travessia, 0, 1);
    static struct gpio_callback botao_cb_data;

    void botao_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
    {
        k_sem_give(&sem_iniciar_travessia); 
        printk("ISR: Botão pressionado!\n");
    }
#endif

/* --- Lógica das Threads --- */

// Thread 1: Controla o LED VERMELHO (e o Pisca Noturno)
void thread_pedestre_vermelho(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) 
    {
#if MODO_OPERACAO == 0
        // --- MODO 0: NORMAL (DIA - CICLO AUTOMÁTICO) ---
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        printk("PEDESTRE: VERMELHO (Modo Dia)\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0);
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
        gpio_pin_set_dt(&led_vermelho, 0);
        k_mutex_unlock(&pedestre_mutex);
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);

#elif MODO_OPERACAO == 1
        // --- MODO 1: NOTURNO (PISCA VERMELHO) ---
        // (Não precisa de mutex, pois a thread verde está dormindo)
        printk("PEDESTRE: NOTURNO (Pisca Vermelho)\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0); 
        k_msleep(TEMPO_PISCA_NOTURNO_MS);
        gpio_pin_set_dt(&led_vermelho, 0);
        k_msleep(TEMPO_PISCA_NOTURNO_MS);

#elif MODO_OPERACAO == 2
        // --- MODO 2: BOTÃO DE TRAVESSIA ---
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0);
        printk("PEDESTRE: VERMELHO (Aguardando botão...)\n");
        
        // Dorme indefinidamente, esperando o botão
        k_sem_take(&sem_iniciar_travessia, K_FOREVER);
        
        // Botão pressionado!
        printk("PEDESTRE: Botão recebido. Cumprindo %dms de vermelho...\n", TEMPO_VERMELHO_PEDESTRE_MS);
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
        
        gpio_pin_set_dt(&led_vermelho, 0);
        k_mutex_unlock(&pedestre_mutex);
        
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
#endif
    }
}

// Thread 2: Controla o LED VERDE
void thread_pedestre_verde(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) 
    {
#if MODO_OPERACAO == 0
        // --- MODO 0: NORMAL (DIA - CICLO AUTOMÁTICO) ---
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        printk("PEDESTRE: VERDE (Modo Dia)\n");
        gpio_pin_set_dt(&led_verde, 1);
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
        gpio_pin_set_dt(&led_verde, 0);
        k_mutex_unlock(&pedestre_mutex);

#elif MODO_OPERACAO == 1
        // --- MODO 1: NOTURNO ---
        k_sleep(K_FOREVER); // Dorme para sempre

#elif MODO_OPERACAO == 2
        // --- MODO 2: BOTÃO DE TRAVESSIA ---
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        printk("PEDESTRE: VERDE (Iniciando travessia %dms...)\n", TEMPO_VERDE_PEDESTRE_MS);
        gpio_pin_set_dt(&led_verde, 1);
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

    // 5. Verificações de "pronto" (LEDs)
    if (!gpio_is_ready_dt(&led_verde)) { printk("Erro: LED Verde (led0) não pronto\n"); return; }
    if (!gpio_is_ready_dt(&led_vermelho)) { printk("Erro: LED Vermelho (led2) não pronto\n"); return; }

    // 6. Configuração dos pinos (LEDs)
    ret = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) { printk("Erro: falha ao configurar LED Verde\n"); return; }
    ret = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_ACTIVE); // Começa ligado
    if (ret < 0) { printk("Erro: falha ao configurar LED Vermelho\n"); return; }
    
    // 7. Configurações específicas do MODO
#if MODO_OPERACAO == 0
    printk("Semáforo de Pedestre (MODO NORMAL/DIA - Automático) iniciado.\n");
#elif MODO_OPERACAO == 1
    printk("Semáforo de Pedestre (MODO NOTURNO) iniciado.\n");
#elif MODO_OPERACAO == 2
    printk("Semáforo de Pedestre (MODO BOTÃO) iniciado.\n");
    
    // Configura o Botão (apenas neste modo)
    if (!gpio_is_ready_dt(&botao)) { printk("Erro: Botão não pronto\n"); return; }
    
    ret = gpio_pin_configure_dt(&botao, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) { printk("Erro: falha ao configurar Botão\n"); return; }
    
    ret = gpio_pin_interrupt_configure_dt(&botao, GPIO_INT_EDGE_FALLING);
    if (ret < 0) { printk("Erro: falha ao configurar interrupção do Botão\n"); return; }

    gpio_init_callback(&botao_cb_data, botao_callback, BIT(botao.pin));
    gpio_add_callback(botao.port, &botao_cb_data);
#endif
    
    // As threads estáticas já estão rodando.
}