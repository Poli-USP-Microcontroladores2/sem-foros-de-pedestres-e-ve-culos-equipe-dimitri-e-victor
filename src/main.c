#include <zephyr/kernel.h>

void minha_thread(void)
{
    while (1) {
        printk("Thread executando!\n");
        k_msleep(1000);
    }
}

// Criar a thread
K_THREAD_DEFINE(minha_thread_id, 512, minha_thread, NULL, NULL, NULL, 7, 0, 0);

void main(void)
{
    printk("Main iniciou\n");
    
    while (1) {
        printk("Main executando\n");
        k_msleep(2000);
    }
}