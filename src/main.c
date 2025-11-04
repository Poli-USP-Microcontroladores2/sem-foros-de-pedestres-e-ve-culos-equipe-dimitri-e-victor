#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

// ===========================================================
// TEMPOS DOS LEDS - MODO DIA
// ===========================================================
// Define quanto tempo cada cor do semáforo fica acesa
#define TEMPO_VERDE_MS    3000   // 3 segundos - carros podem passar
#define TEMPO_AMARELO_MS  1000   // 1 segundo - atenção, vai fechar
#define TEMPO_VERMELHO_MS 4000   // 4 segundos - parado, pedestre atravessa

// ===========================================================
// TEMPOS DO MODO NOTURNO - AMARELO PISCANTE
// ===========================================================
// No modo noturno, o amarelo fica piscando para sinalização
#define TEMPO_PISCA_ON_MS  1000  // 1 segundo aceso
#define TEMPO_PISCA_OFF_MS 1000  // 1 segundo apagado

// ===========================================================
// LEDS ONBOARD (FRDM-KL25Z)
// ===========================================================
// A placa FRDM-KL25Z tem um LED RGB integrado
// led0 = LED Verde, led2 = LED Vermelho
// Amarelo = Verde + Vermelho acesos juntos
#define LED_VERDE_NODE    DT_ALIAS(led0)
#define LED_VERMELHO_NODE DT_ALIAS(led2)

// Verifica se o LED Verde está disponível no devicetree
#if DT_NODE_HAS_STATUS(LED_VERDE_NODE, okay)
static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(LED_VERDE_NODE, gpios);
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

// Verifica se o LED Vermelho está disponível no devicetree
#if DT_NODE_HAS_STATUS(LED_VERMELHO_NODE, okay)
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(LED_VERMELHO_NODE, gpios);
#else
#error "Unsupported board: led2 devicetree alias is not defined"
#endif

// ===========================================================
// BOTÃO PTA16 COM PULL-UP E INTERRUPÇÃO
// ===========================================================
// O botão simula um pedestre solicitando atravessia
// PTA16 = Pino 16 da Porta A
#define PORTA_BOTAO  DT_NODELABEL(gpioa)  // Porta A do microcontrolador
#define PINO_BOTAO   16                    // Pino 16 (PTA16)

static const struct device *gpio_port_a;   // Ponteiro para o dispositivo GPIO Porta A
static struct gpio_callback button_cb_data; // Estrutura para callback da interrupção
volatile bool pedestre_solicitado = false;  // Flag global: pedestre apertou o botão?
                                            // volatile = pode mudar na ISR

// ===========================================================
// ISR - BOTÃO PRESSIONADO (Interrupt Service Routine)
// ===========================================================
// Esta função é chamada AUTOMATICAMENTE quando o botão é pressionado
// Ela roda em contexto de interrupção (muito rápido, não pode travar)
void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Se o pedestre ainda não solicitou, marca a flag
    if (!pedestre_solicitado) {
        pedestre_solicitado = true;  // Avisa as threads que pedestre quer atravessar
        printk("\n=== BOTÃO PRESSIONADO: PEDESTRE SOLICITOU TRAVESSIA ===\n");
    }
    // IMPORTANTE: ISR deve ser rápida! Só marca a flag e sai
    // As threads vão lidar com a mudança do semáforo
}

// ===========================================================
// SINCRONIZAÇÃO ENTRE THREADS
// ===========================================================
// Mutex = garante que apenas 1 thread controla os LEDs por vez
// Evita que verde e vermelho acendam juntos acidentalmente
K_MUTEX_DEFINE(led_mutex);

// Semáforos (não confundir com semáforo de trânsito!)
// São mecanismos de sincronização entre threads
// Funcionam como "fichas": quem tem a ficha pode executar
K_SEM_DEFINE(sem_verde, 1, 1);      // Verde começa com 1 ficha (executa primeiro)
K_SEM_DEFINE(sem_amarelo, 0, 1);    // Amarelo começa com 0 fichas (espera)
K_SEM_DEFINE(sem_vermelho, 0, 1);   // Vermelho começa com 0 fichas (espera)

// ===========================================================
// CONFIGURAÇÃO DO MODO
// 0 = Modo Dia (sequência normal: verde → amarelo → vermelho)
// 1 = Modo Noite (amarelo piscante contínuo)
// ===========================================================
#define MODO_OPERACAO 0  // Mude para 1 para testar modo noturno

// ===========================================================
// MODO NOTURNO - AMARELO PISCANTE
// ===========================================================
// Esta função é chamada se MODO_OPERACAO = 1
// Fica em loop infinito piscando o amarelo
void modo_noite(void)
{
    printk("\n*** MODO NOTURNO ATIVADO ***\n");
    printk("Amarelo piscando: %d ms aceso, %d ms apagado\n\n",
           TEMPO_PISCA_ON_MS, TEMPO_PISCA_OFF_MS);

    while (1) {  // Loop infinito
        // Amarelo = Verde + Vermelho acesos juntos
        gpio_pin_set_dt(&led_verde, 1);
        gpio_pin_set_dt(&led_vermelho, 1);
        printk("Amarelo ACESO\n");
        k_msleep(TEMPO_PISCA_ON_MS);  // Aguarda 1 segundo

        // Apaga o amarelo
        gpio_pin_set_dt(&led_verde, 0);
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("Amarelo APAGADO\n");
        k_msleep(TEMPO_PISCA_OFF_MS);  // Aguarda 1 segundo
    }
}

// ===========================================================
// THREAD LED VERDE
// ===========================================================
// Esta thread controla o LED verde do semáforo
// Fica em loop: acende verde → espera → passa para amarelo
void thread_led_verde(void)
{
    // Configura o pino do LED verde como saída, inicialmente desligado
    int ret = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Verde\n", ret);
        return;  // Se falhar, encerra a thread
    }

    printk("Thread LED Verde iniciada\n");

    while (1) {  // Loop infinito da thread
        // Espera receber a "ficha" (semáforo de sincronização)
        // Bloqueia aqui até alguém dar k_sem_give(&sem_verde)
        k_sem_take(&sem_verde, K_FOREVER);

        // Pega o mutex para ter acesso exclusivo aos LEDs
        k_mutex_lock(&led_mutex, K_FOREVER);

        printk("LED VERDE aceso\n");
        gpio_pin_set_dt(&led_verde, 1);  // Liga o LED verde

        // Fica verde por TEMPO_VERDE_MS, mas verifica o botão a cada 100ms
        int tempo_passado = 0;
        while (tempo_passado < TEMPO_VERDE_MS) {
            // Se o pedestre apertou o botão durante o verde
            if (pedestre_solicitado) {
                printk("Pedestre solicitou travessia durante VERDE — indo para AMARELO\n");
                gpio_pin_set_dt(&led_verde, 0);  // Apaga o verde
                k_mutex_unlock(&led_mutex);      // Libera o mutex
                k_sem_give(&sem_amarelo);        // Passa a "ficha" para o amarelo
                goto fim_verde;  // Pula para o fim (não completa os 3 segundos)
            }
            k_msleep(100);      // Dorme 100ms
            tempo_passado += 100;  // Soma ao tempo total
        }

        // Se chegou aqui, completou os 3 segundos normalmente
        gpio_pin_set_dt(&led_verde, 0);  // Apaga o verde
        printk("LED VERDE apagado\n");

        k_mutex_unlock(&led_mutex);  // Libera o mutex

        // Passa a "ficha" para a thread do amarelo
        k_sem_give(&sem_amarelo);
    fim_verde:
        ;  // Label para o goto (quando pedestre interrompe)
    }
}

// ===========================================================
// THREAD LED AMARELO
// ===========================================================
// Esta thread controla o LED amarelo (transição)
// Sempre fica 1 segundo e depois passa para o vermelho
void thread_led_amarelo(void)
{
    // Configura ambos os LEDs (amarelo = verde + vermelho)
    int ret_verde = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    int ret_vermelho = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    if (ret_verde < 0 || ret_vermelho < 0) {
        printk("Erro: falha ao configurar LEDs para Amarelo\n");
        return;
    }

    printk("Thread LED Amarelo iniciada\n");

    while (1) {  // Loop infinito
        // Espera receber a "ficha" do verde
        k_sem_take(&sem_amarelo, K_FOREVER);

        // Pega o mutex (controle exclusivo dos LEDs)
        k_mutex_lock(&led_mutex, K_FOREVER);

        // Liga verde + vermelho = amarelo
        printk("LED AMARELO aceso (Verde + Vermelho)\n");
        gpio_pin_set_dt(&led_verde, 1);
        gpio_pin_set_dt(&led_vermelho, 1);
        k_msleep(TEMPO_AMARELO_MS);  // Aguarda 1 segundo
        
        // Apaga o amarelo
        gpio_pin_set_dt(&led_verde, 0);
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("LED AMARELO apagado\n");

        k_mutex_unlock(&led_mutex);  // Libera o mutex

        // Sempre passa para o vermelho (com ou sem pedestre)
        k_sem_give(&sem_vermelho);
    }
}

// ===========================================================
// THREAD LED VERMELHO
// ===========================================================
// Esta thread controla o LED vermelho (parado)
// Fica 4 segundos e depois volta para o verde
void thread_led_vermelho(void)
{
    // Configura o LED vermelho como saída
    int ret = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Erro %d: falha ao configurar LED Vermelho\n", ret);
        return;
    }

    printk("Thread LED Vermelho iniciada\n");

    while (1) {  // Loop infinito
        // Espera receber a "ficha" do amarelo
        k_sem_take(&sem_vermelho, K_FOREVER);

        // Pega o mutex
        k_mutex_lock(&led_mutex, K_FOREVER);

        // Liga o LED vermelho
        printk("LED VERMELHO aceso\n");
        gpio_pin_set_dt(&led_vermelho, 1);
        k_msleep(TEMPO_VERMELHO_MS);  // Aguarda 4 segundos (pedestre atravessa)
        
        // Apaga o vermelho
        gpio_pin_set_dt(&led_vermelho, 0);
        printk("LED VERMELHO apagado\n");

        k_mutex_unlock(&led_mutex);  // Libera o mutex

        // Reseta a flag do pedestre (já atravessou)
        pedestre_solicitado = false;
        
        // Volta para o verde (recomeça o ciclo)
        k_sem_give(&sem_verde);
    }
}

// ===========================================================
// DEFINIÇÃO DAS THREADS
// ===========================================================
// K_THREAD_DEFINE cria as threads automaticamente na inicialização
// Parâmetros: ID, tamanho da stack, função, args, prioridade, opções, delay
K_THREAD_DEFINE(thread_verde_id, 1024, thread_led_verde, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(thread_amarelo_id, 1024, thread_led_amarelo, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(thread_vermelho_id, 1024, thread_led_vermelho, NULL, NULL, NULL, 5, 0, 0);

// ===========================================================
// FUNÇÃO PRINCIPAL
// ===========================================================
// Esta função roda UMA VEZ quando o sistema inicia
// Ela configura tudo e depois as threads assumem o controle
void main(void)
{
    printk("===========================================\n");
    printk("Sistema de Semáforo - FRDM-KL25Z\n");
    printk("===========================================\n");

    // ========== CONFIGURAÇÃO DOS LEDS ==========
    // Configura os pinos dos LEDs como saída
    int ret_verde = gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    int ret_vermelho = gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    if (ret_verde < 0 || ret_vermelho < 0) {
        printk("ERRO: Falha ao configurar GPIOs dos LEDs\n");
        return;  // Se falhar, encerra o programa
    }

    // ========== CONFIGURAÇÃO DO BOTÃO PTA16 ==========
    // Pega o ponteiro para a Porta A
    gpio_port_a = DEVICE_DT_GET(PORTA_BOTAO);
    if (!device_is_ready(gpio_port_a)) {
        printk("Erro: Porta A não está pronta!\n");
        return;
    }

    // Configura PTA16 como entrada com pull-up interno
    // GPIO_INPUT = entrada
    // GPIO_PULL_UP = resistor pull-up interno ativo (mantém o pino em HIGH)
    int ret_btn = gpio_pin_configure(gpio_port_a, PINO_BOTAO, 
                                      GPIO_INPUT | GPIO_PULL_UP);
    if (ret_btn < 0) {
        printk("Erro %d ao configurar pino PTA16\n", ret_btn);
        return;
    }

    // Configura a interrupção para detectar quando o botão é pressionado
    // GPIO_INT_EDGE_FALLING = detecta transição de HIGH para LOW (botão pressionado)
    gpio_pin_interrupt_configure(gpio_port_a, PINO_BOTAO, GPIO_INT_EDGE_FALLING);
    
    // Inicializa a estrutura de callback (associa a ISR ao pino)
    // button_isr = função que será chamada quando a interrupção ocorrer
    // BIT(PINO_BOTAO) = máscara indicando qual pino dispara a interrupção
    gpio_init_callback(&button_cb_data, button_isr, BIT(PINO_BOTAO));
    
    // Registra o callback na porta A
    gpio_add_callback(gpio_port_a, &button_cb_data);

    // ========== ESCOLHA DO MODO DE OPERAÇÃO ==========
    if (MODO_OPERACAO == 1) {
        // MODO NOTURNO: suspende todas as threads e entra no amarelo piscante
        k_thread_suspend(thread_verde_id);
        k_thread_suspend(thread_amarelo_id);
        k_thread_suspend(thread_vermelho_id);

        modo_noite();  // Esta função nunca retorna (loop infinito)
    } else {
        // MODO DIURNO: as threads já estão rodando automaticamente
        printk("MODO DIURNO ATIVO\n");
        printk("Verde: %d ms | Amarelo: %d ms | Vermelho: %d ms\n",
               TEMPO_VERDE_MS, TEMPO_AMARELO_MS, TEMPO_VERMELHO_MS);
        printk("Botão PTA16 configurado com pull-up e interrupção\n");
        printk("Pressione o botão para simular pedestre\n");
        printk("===========================================\n\n");
    }
    
    // A função main() termina aqui, mas as threads continuam rodando!
}