#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h> // Necessário para a API legado (device_get_binding)

/* --- Definições do Projeto --- */
#define MODO_OPERACAO 2 // Travado no Modo 2

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5

#define TEMPO_VERDE_PEDESTRE_MS  4000
#define TEMPO_VERMELHO_PEDESTRE_MS 2000
#define POLLING_ESPERA_OK_MS 50

/* --- Nós do Device Tree (API Moderna) --- */
#define LED_VERDE_NODE    DT_ALIAS(led0)
#define LED_VERMELHO_NODE DT_ALIAS(led2)
#define BOTAO_NODE        DT_ALIAS(sw0) 

/* --- Pinos (API Legado) --- */
// Nossos pinos de comunicação (PTD4 e PTC3)
#define SINAL_PEDIDO_PIN 4
#define SINAL_OK_VEICULO_PIN 3
// Ponteiros globais para os dispositivos GPIO
static const struct device *gpio_d; // Para PTD4
static const struct device *gpio_c; // For PTC3

/* --- Verificações de Compilação (API Moderna) --- */
#if DT_NODE_HAS_STATUS(LED_VERDE_NODE, okay)
static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
#else
#error "led0 (verde) alias não definido"
#endif

#if DT_NODE_HAS_STATUS(LED_VERMELHO_NODE, okay)
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);
#else
#error "led2 (vermelho) alias não definido"
#endif

#if MODO_OPERACAO == 2
    #if DT_NODE_HAS_STATUS(BOTAO_NODE, okay)
    static const struct gpio_dt_spec botao = GPIO_DT_SPEC_GET(BOTAO_NODE, gpios);
    #else
    #error "sw0 (botao) alias não definido"
    #endif
    
    // As verificações de #error para os pinos de overlay foram REMOVIDAS
    // porque o build não as estava encontrando.
#endif

/* --- Primitivas do RTOS --- */
K_MUTEX_DEFINE(pedestre_mutex); 

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

// Thread 1: Controla o LED VERMELHO (A "Gerente")
void thread_pedestre_vermelho(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) 
    {
#if MODO_OPERACAO == 2
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0);
        printk("PEDESTRE: VERMELHO (Aguardando botão...)\n");
        
        k_sem_take(&sem_iniciar_travessia, K_FOREVER);
        
        printk("PEDESTRE: Botão recebido. Enviando pedido (ALTO) para MCU Veículos.\n");
        // API LEGADO: gpio_pin_set(dispositivo, pino, valor)
        gpio_pin_set(gpio_d, SINAL_PEDIDO_PIN, 1);
        
        printk("PEDESTRE: Aguardando OK (ALTO) do MCU Veículos...\n");
        // API LEGADO: gpio_pin_get(dispositivo, pino)
        while (gpio_pin_get(gpio_c, SINAL_OK_VEICULO_PIN) == 0) {
            k_msleep(POLLING_ESPERA_OK_MS);
        }
        
        printk("PEDESTRE: OK Recebido! Liberando travessia.\n");
        gpio_pin_set_dt(&led_vermelho, 0);
        k_mutex_unlock(&pedestre_mutex);
        
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
        
#elif MODO_OPERACAO == 0
        // (Modo 0 inalterado)
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        printk("PEDESTRE: VERMELHO (Modo Dia)\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0);
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
        gpio_pin_set_dt(&led_vermelho, 0);
        k_mutex_unlock(&pedestre_mutex);
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
#elif MODO_OPERACAO == 1
        // (Modo 1 inalterado)
        printk("PEDESTRE: NOTURNO (Pisca Vermelho)\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        gpio_pin_set_dt(&led_verde, 0); 
        k_msleep(TEMPO_PISCA_NOTURNO_MS);
        gpio_pin_set_dt(&led_vermelho, 0);
        k_msleep(TEMPO_PISCA_NOTURNO_MS);
#endif
    }
}

// Thread 2: Controla o LED VERDE (A "Trabalhadora")
void thread_pedestre_verde(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) 
    {
#if MODO_OPERACAO == 2
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        
        printk("PEDESTRE: VERDE (Iniciando travessia %dms...)\n", TEMPO_VERDE_PEDESTRE_MS);
        gpio_pin_set_dt(&led_verde, 1);
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
        
        gpio_pin_set_dt(&led_verde, 0);
        
        printk("PEDESTRE: Travessia concluída. Enviando sinal (BAIXO) para MCU Veículos.\n");
        // API LEGADO: gpio_pin_set(dispositivo, pino, valor)
        gpio_pin_set(gpio_d, SINAL_PEDIDO_PIN, 0);
        
        k_mutex_unlock(&pedestre_mutex);
#elif MODO_OPERACAO == 0
        // (Modo 0 inalterado)
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
        k_mutex_lock(&pedestre_mutex, K_FOREVER);
        printk("PEDESTRE: VERDE (Modo Dia)\n");
        gpio_pin_set_dt(&led_verde, 1);
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);
        gpio_pin_set_dt(&led_verde, 0);
        k_mutex_unlock(&pedestre_mutex);
#elif MODO_OPERACAO == 1
        // (Modo 1 inalterado)
        k_sleep(K_FOREVER);
#endif
    }
}

/* --- Threads Estáticas (Inalteradas) --- */
K_THREAD_DEFINE(vermelho_thread_id, STACK_SIZE,
                thread_pedestre_vermelho, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, 0); 
K_THREAD_DEFINE(verde_thread_id, STACK_SIZE,
                thread_pedestre_verde, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, 0); 

/* --- Função Principal (Refatorada) --- */
void main(void)
{
    int ret; 
    
    /* Configuração da API Moderna (LEDs) */
    if (!gpio_is_ready_dt(&led_verde)) { printk("Erro: LED Verde não pronto\n"); return; }
    if (!gpio_is_ready_dt(&led_vermelho)) { printk("Erro: LED Vermelho não pronto\n"); return; }
    ret = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) { printk("Erro: falha config LED Verde\n"); return; }
    ret = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_ACTIVE); 
    if (ret < 0) { printk("Erro: falha config LED Vermelho\n"); return; }
    
#if MODO_OPERACAO == 2
    printk("Semáforo de Pedestre (MODO BOTÃO com Sincronismo) iniciado.\n");
    
    /* Configuração da API Moderna (Botão) */
    if (!gpio_is_ready_dt(&botao)) { printk("Erro: Botão não pronto\n"); return; }
    ret = gpio_pin_configure_dt(&botao, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) { printk("Erro: falha config Botão\n"); return; }
    ret = gpio_pin_interrupt_configure_dt(&botao, GPIO_INT_EDGE_FALLING);
    if (ret < 0) { printk("Erro: falha config ISR Botão\n"); return; }
    gpio_init_callback(&botao_cb_data, botao_callback, BIT(botao.pin));
    gpio_add_callback(botao.port, &botao_cb_data);

    /* Configuração da API Legado (Pinos de Comunicação) */
    
    // Pega o dispositivo "GPIOD". (O app.overlay NÃO é necessário para este)
    gpio_d = device_get_binding("GPIOD");
    if (gpio_d == NULL) { printk("Erro: Não foi possível encontrar GPIOD\n"); return; }
    
    // Pega o dispositivo "GPIOC". (O app.overlay É necessário para este!)
    gpio_c = device_get_binding("GPIOC");
    if (gpio_c == NULL) { printk("Erro: Não foi possível encontrar GPIOC. (Verifique app.overlay e prj.conf)\n"); return; }
    
    // Configura SINAL_PEDIDO (PTD4, Saída)
    ret = gpio_pin_configure(gpio_d, SINAL_PEDIDO_PIN, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) { printk("Erro: falha config SINAL_PEDIDO (PTD4)\n"); return; }
    
    // Configura SINAL_OK_VEICULO (PTC3, Entrada)
    ret = gpio_pin_configure(gpio_c, SINAL_OK_VEICULO_PIN, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret < 0) { printk("Erro: falha config SINAL_OK (PTC3)\n"); return; }

#elif MODO_OPERACAO == 0
    printk("Semáforo de Pedestre (MODO NORMAL/DIA - Automático) iniciado.\n");
#elif MODO_OPERACAO == 1
    printk("Semáforo de Pedestre (MODO NOTURNO) iniciado.\n");
#endif
}