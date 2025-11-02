#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* --- Definições do Projeto --- */

// ####################################################################
// ### CONTROLE DE MODO ESTÁTICO ###
// Defina 0 para Modo Normal (Dia - ciclo automático V/R)
// Defina 1 para Modo Noturno (pisca vermelho)
// Defina 2 para Modo Botão (vermelho até o botão ser pressionado)
#define MODO_OPERACAO 2
// ####################################################################

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5

// Tempos do Modo Normal (Dia) / Botão
#define TEMPO_VERDE_PEDESTRE_MS  4000
#define TEMPO_VERMELHO_PEDESTRE_MS 2000
#define POLLING_ESPERA_OK_MS 50 // Intervalo para checar o pino de OK

// Tempos do Modo Noturno
#define TEMPO_PISCA_NOTURNO_MS 1000 // 1s aceso, 1s apagado

// 1. Definição dos Nós do Device Tree
#define LED_VERDE_NODE    DT_ALIAS(led0) // led0 = Verde
#define LED_VERMELHO_NODE DT_ALIAS(led2) // led2 = Vermelho
#define BOTAO_NODE        DT_ALIAS(sw0)  // sw0 = Botão (usado apenas no MODO 2)

// --- NOVOS PINOS DE COMUNICAÇÃO ---
// (Estes aliases teriam que ser definidos em um arquivo .overlay)
#define SINAL_PEDIDO_NODE       DT_ALIAS(sinal-pedido)
#define SINAL_OK_VEICULO_NODE   DT_ALIAS(sinal-ok-veiculo)

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

    #if DT_NODE_HAS_STATUS(SINAL_PEDIDO_NODE, okay)
    static const struct gpio_dt_spec sinal_pedido = GPIO_DT_SPEC_GET(SINAL_PEDIDO_NODE, gpios);
    #else
    #error "Cross-MCU Error: 'sinal-pedido' alias is not defined in overlay"
    #endif

    #if DT_NODE_HAS_STATUS(SINAL_OK_VEICULO_NODE, okay)
    static const struct gpio_dt_spec sinal_ok_veiculo = GPIO_DT_SPEC_GET(SINAL_OK_VEICULO_NODE, gpios);
    #else
    #error "Cross-MCU Error: 'sinal-ok-veiculo' alias is not defined in overlay"
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
        
        // 1. Garante o estado padrão (Vermelho) e pega o mutex
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0);
        printk("PEDESTRE: VERMELHO (Aguardando botão...)\n");
        
        // 2. Dorme indefinidamente, esperando o botão
        k_sem_take(&sem_iniciar_travessia, K_FOREVER);
        
        // 3. (Ponto 1) Botão pressionado! Envia SINAL DE PEDIDO (ALTO)
        printk("PEDESTRE: Botão recebido. Enviando pedido (ALTO) para MCU Veículos.\n");
        gpio_pin_set_dt(&sinal_pedido, 1);
        
        // 4. (Ponto 1) Espera (polling) pelo SINAL DE OK (ALTO)
        printk("PEDESTRE: Aguardando OK (ALTO) do MCU Veículos...\n");
        while (gpio_pin_get_dt(&sinal_ok_veiculo) == 0) {
            k_msleep(POLLING_ESPERA_OK_MS); // Dorme 50ms para não usar 100% CPU
        }
        
        // 5. OK Recebido! Libera o mutex para a thread_verde
        printk("PEDESTRE: OK Recebido! Liberando travessia.\n");
        gpio_pin_set_dt(&led_vermelho, 0);
        k_mutex_unlock(&pedestre_mutex);
        
        // 6. Espera APENAS o verde terminar (Ponto 2)
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
        
        // 7. Volta ao loop para pegar o mutex e reiniciar o estado padrão
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
        // 1. Dorme, esperando a thread_vermelho liberar o mutex
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        
        // 2. Acordou! Inicia Fase Verde
        printk("PEDESTRE: VERDE (Iniciando travessia %dms...)\n", TEMPO_VERDE_PEDESTRE_MS);
        gpio_pin_set_dt(&led_verde, 1);
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
        
        // 3. Verde terminado. Apaga o LED
        gpio_pin_set_dt(&led_verde, 0);
        
        // 4. (Ponto 2) Envia SINAL DE FIM (BAIXO)
        printk("PEDESTRE: Travessia concluída. Enviando sinal (BAIXO) para MCU Veículos.\n");
        gpio_pin_set_dt(&sinal_pedido, 0);
        
        // 5. Libera o mutex para a thread_vermelho
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
    printk("Semáforo de Pedestre (MODO BOTÃO com Sincronismo) iniciado.\n");
    
    // Botão (Entrada com ISR)
    if (!gpio_is_ready_dt(&botao)) { printk("Erro: Botão não pronto\n"); return; }
    ret = gpio_pin_configure_dt(&botao, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) { printk("Erro: falha config Botão\n"); return; }
    ret = gpio_pin_interrupt_configure_dt(&botao, GPIO_INT_EDGE_FALLING);
    if (ret < 0) { printk("Erro: falha config ISR Botão\n"); return; }
    gpio_init_callback(&botao_cb_data, botao_callback, BIT(botao.pin));
    gpio_add_callback(botao.port, &botao_cb_data);

    // Sinal de Pedido (Saída)
    if (!gpio_is_ready_dt(&sinal_pedido)) { printk("Erro: Pino SINAL_PEDIDO não pronto\n"); return; }
    ret = gpio_pin_configure_dt(&sinal_pedido, GPIO_OUTPUT_INACTIVE); // Começa em BAIXO
    if (ret < 0) { printk("Erro: falha config SINAL_PEDIDO\n"); return; }
    
    // Sinal de OK (Entrada)
    if (!gpio_is_ready_dt(&sinal_ok_veiculo)) { printk("Erro: Pino SINAL_OK_VEICULO não pronto\n"); return; }
    ret = gpio_pin_configure_dt(&sinal_ok_veiculo, GPIO_INPUT | GPIO_PULL_DOWN); // Pull-down para esperar sinal ALTO
    if (ret < 0) { printk("Erro: falha config SINAL_OK_VEICULO\n"); return; }

#endif
    
    // As threads estáticas já estão rodando.
}