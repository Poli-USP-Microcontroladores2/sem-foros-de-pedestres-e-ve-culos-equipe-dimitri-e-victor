#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* --- Definições do Semáforo de Pedestres --- */
#define TEMPO_VERDE_PEDESTRE_MS  4000
#define TEMPO_VERMELHO_PEDESTRE_MS 2000

// 1. Definição do Mutex (Recurso Compartilhado)
// O "recurso" aqui é o "direito de acender um LED". 
// Apenas uma thread pode ter esse direito por vez.
K_MUTEX_DEFINE(pedestre_mutex);

// Variáveis globais para simular o estado dos LEDs
// (Usaremos isso para os testes de asserção)
static bool led_verde_estado = false;
static bool led_vermelho_estado = false;

/* --- Lógica das Threads --- */

// 2. Thread 1: Controla o LED Verde do Pedestre
void thread_pedestre_verde(void *a, void *b, void *c)
{
    while (1) {
        // Tenta pegar o controle (mutex)
        k_mutex_lock(&pedestre_mutex, K_FOREVER);

        // --- Seção Crítica (Controlando o LED) ---
        led_verde_estado = true;
        led_vermelho_estado = false; // Garante que o outro esteja apagado
        printk("PEDESTRE: [VERDE] ON\n");
        
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);

        printk("PEDESTRE: [VERDE] OFF\n");
        led_verde_estado = false;
        // --- Fim da Seção Crítica ---

        // Libera o controle para a próxima thread (a vermelha)
        k_mutex_unlock(&pedestre_mutex);

        // Espera o tempo do vermelho passar antes de tentar de novo
        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);
    }
}

// 3. Thread 2: Controla o LED Vermelho do Pedestre
void thread_pedestre_vermelho(void *a, void *b, void *c)
{
    while (1) {
        // Espera o tempo do verde passar (só para sincronizar o início)
        k_msleep(TEMPO_VERDE_PEDESTRE_MS);

        // Tenta pegar o controle (mutex)
        k_mutex_lock(&pedestre_mutex, K_FOREVER);

        // --- Seção Crítica (Controlando o LED) ---
        led_vermelho_estado = true;
        led_verde_estado = false; // Garante que o outro esteja apagado
        printk("PEDESTRE: [VERMELHO] ON\n");

        k_msleep(TEMPO_VERMELHO_PEDESTRE_MS);

        printk("PEDESTRE: [VERMELHO] OFF\n");
        led_vermelho_estado = false;
        // --- Fim da Seção Crítica ---

        // Libera o controle para a próxima thread (a verde)
        k_mutex_unlock(&pedestre_mutex);
    }
}

/* --- Pilhas (Stacks) para as Threads --- */
// Cada thread precisa da sua própria área de memória (pilha)
#define STACK_SIZE 512
K_THREAD_STACK_DEFINE(verde_stack_area, STACK_SIZE);
K_THREAD_STACK_DEFINE(vermelho_stack_area, STACK_SIZE);

// Estruturas de dados das threads
static struct k_thread verde_thread_data;
static struct k_thread vermelho_thread_data;

/* --- Suíte de Testes (Ztest) --- */

// Esta função é o nosso "teste unitário"
// Ela vai iniciar as threads e verificar se elas funcionam
static void test_ciclo_pedestre_threads(void)
{
    printk("--- Iniciando Teste Unitário: Ciclo de Pedestre ---\n");
    
    // Inicia as duas threads
    k_thread_create(&verde_thread_data, verde_stack_area,
                    K_THREAD_STACK_SIZEOF(verde_stack_area),
                    thread_pedestre_verde, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(7), 0, K_NO_WAIT); // Prioridade 7

    k_thread_create(&vermelho_thread_data, vermelho_stack_area,
                    K_THREAD_STACK_SIZEOF(vermelho_stack_area),
                    thread_pedestre_vermelho, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(7), 0, K_NO_WAIT); // Prioridade 7

    // --- Validação (O "V" do Modelo) ---
    // O teste vai rodar por 7 segundos para observar 1 ciclo completo (4s + 2s = 6s)

    // Ponto 1: Imediatamente (t=0s), o verde deve ligar
    k_msleep(100); // Dá tempo para o scheduler rodar
    zassert_true(led_verde_estado, "Falha: LED Verde não ligou no t=0");
    zassert_false(led_vermelho_estado, "Falha: LED Vermelho está ligado com o Verde");
    printk("TEST: t=0.1s - OK\n");

    // Ponto 2: Após 4 segundos (t=4s), o verde deve desligar e o vermelho ligar
    k_msleep(TEMPO_VERDE_PEDESTRE_MS); // Dorme 4s
    zassert_false(led_verde_estado, "Falha: LED Verde não desligou no t=4s");
    zassert_true(led_vermelho_estado, "Falha: LED Vermelho não ligou no t=4s");
    printk("TEST: t=4.1s - OK\n");

    // Ponto 3: Após mais 2 segundos (t=6s), o vermelho desliga e o verde liga de novo
    k_msleep(TEMPO_VERMELHO_PEDESTRE_MS); // Dorme 2s
    zassert_true(led_verde_estado, "Falha: LED Verde não ligou no t=6s");
    zassert_false(led_vermelho_estado, "Falha: LED Vermelho não desligou no t=6s");
    printk("TEST: t=6.1s - OK\n");

    printk("--- Teste Unitário Concluído com Sucesso ---\n");

    // Aborta as threads para o teste terminar (não é necessário em `native_posix`, mas é boa prática)
    k_thread_abort(&verde_thread_data);
    k_thread_abort(&vermelho_thread_data);
}

// 1. Define a suíte de testes
ZTEST_SUITE(pedestre_suite, NULL, NULL, NULL, NULL, NULL);

// 2. Adiciona a função de teste à suíte
ZTEST(pedestre_suite, test_ciclo_pedestre_threads)