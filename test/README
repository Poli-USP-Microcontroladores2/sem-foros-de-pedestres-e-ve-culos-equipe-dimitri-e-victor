# Plano de Testes: Semáforo de Pedestres (MCU 2)

Este documento descreve o plano de validação e verificação (V&V) para o firmware do Semáforo de Pedestres (MCU 2), conforme os requisitos do Modelo V.

**Ambiente de Teste:**
A validação do firmware será executada em três fases:
1.  **Validação de Compilação:** Garantir que cada modo de operação (`0`, `1`, `2`) compila com sucesso para a plataforma alvo (`frdm_kl25z`).
2.  **Validação de Lógica (Emulação):** Testar a lógica de RTOS (Mutex, Semáforos, Threads) usando o `Ztest` em um ambiente emulado (`native_sim` ou `qemu`).
3.  **Validação Funcional (Hardware):** Testar os requisitos de sistema (Modos de Operação) no hardware alvo.

---

## 1. Testes Unitários (TU)

**Objetivo:** Validar os componentes internos e a lógica de RTOS do MCU 2 (Pedestre) de forma isolada.
**Foco:** A correta cooperação entre as `thread_pedestre_vermelho`, `thread_pedestre_verde`, o `pedestre_mutex` e o `sem_iniciar_travessia`.

| ID do Teste | Módulo Testado | Cenário de Teste | Passos de Execução (via Ztest/Emulação) | Resultado Esperado |
| :--- | :--- | :--- | :--- | :--- |
| **TU-01** | `k_mutex` | Exclusão Mútua (Modo 0 e 2) | 1. Compilar em `MODO_OPERACAO 0` (Dia).<br>2. Observar a saída (`printk`) por 3 ciclos completos. | Em NENHUM momento o "PEDESTRE: VERDE" deve aparecer antes do "PEDESTRE: VERMELHO" terminar. Os dois LEDs (Verde e Vermelho) nunca devem estar ligados ao mesmo tempo. |
| **TU-02** | `thread_pedestre_vermelho` | Temporização (Modo 0) | 1. Compilar em `MODO_OPERACAO 0`.<br>2. Medir o tempo entre o `printk` "VERMELHO" e o `k_mutex_unlock`. | O tempo deve ser igual a `TEMPO_VERMELHO_PEDESTRE_MS` (2000ms ± margem). |
| **TU-03** | `thread_pedestre_verde` | Temporização (Modo 0) | 1. Compilar em `MODO_OPERACAO 0`.<br>2. Medir o tempo entre o `printk` "VERDE" e o `k_mutex_unlock`. | O tempo deve ser igual a `TEMPO_VERDE_PEDESTRE_MS` (4000ms ± margem). |
| **TU-04** | `k_sem` (Modo 2) | Sinalização ISR -> Thread | 1. Compilar em `MODO_OPERACAO 2` (Botão).<br>2. Simular uma chamada de ISR (Botão) via `k_sem_give()`. | A `thread_pedestre_vermelho`, que estava bloqueada em `k_sem_take()`, deve acordar e continuar a execução. |

---

## 2. Testes de Sistema (TS)

**Objetivo:** Validar que os requisitos funcionais (os 3 modos de operação) são atendidos no hardware-alvo.

| ID do Teste | Requisito Testado | Cenário de Teste | Passos de Execução (Hardware) | Resultado Esperado |
| :--- | :--- | :--- | :--- | :--- |
| **TS-01** | Modo Normal (Dia) | Compilação do Modo 0 | 1. Definir `MODO_OPERACAO 0`.<br>2. Compilar (`platformio run`). | O build deve ser concluído com `[SUCCESS]`. |
| **TS-02** | Modo Normal (Dia) | Execução do Ciclo Automático | 1. Fazer upload do firmware TS-01.<br>2. Observar os LEDs. | O LED Vermelho deve ficar aceso por 2s, seguido pelo LED Verde por 4s, em um loop contínuo. |
| **TS-03** | Modo Noturno | Compilação do Modo 1 | 1. Definir `MODO_OPERACAO 1`.<br>2. Compilar (`platformio run`). | O build deve ser concluído com `[SUCCESS]`. |
| **TS-04** | Modo Noturno | Execução do Pisca-Pisca | 1. Fazer upload do firmware TS-03.<br>2. Observar os LEDs. | O LED Vermelho deve piscar (1s aceso, 1s apagado). O LED Verde nunca deve ser ativado. |
| **TS-05** | Modo Botão | Compilação do Modo 2 | 1. Definir `MODO_OPERACAO 2`.<br>2. Compilar (`platformio run`). | O build deve ser concluído com `[SUCCESS]`. |
| **TS-06** | Modo Botão | Estado Padrão (Espera) | 1. Fazer upload do firmware TS-05.<br>2. Aguardar 10s sem tocar no botão `sw0`. | O LED Vermelho deve permanecer aceso. O ciclo de travessia (Verde) NÃO deve iniciar. |
| **TS-07** | Modo Botão | Acionamento Físico | 1. Executar o teste TS-06.<br>2. Pressionar o botão físico `sw0`. | O sistema deve detectar o botão, aguardar 2s (Vermelho), acender o LED Verde por 4s, e então retornar ao estado Vermelho (Padrão). |

---

## 3. Testes de Integração (TI)

**Objetivo:** Validar o sincronismo e a consistência entre o MCU 1 (Veículos) e o MCU 2 (Pedestres).
**Foco:** Garantir que NUNCA haja um estado de (Veículo Verde) e (Pedestre Verde) ao mesmo tempo.

| ID do Teste | Requisito Testado | Cenário de Teste | Passos de Execução (Hardware) | Resultado Esperado |
| :--- | :--- | :--- | :--- | :--- |
| **TI-01** | Sincronismo (Modo Botão) | Pedido de Travessia | 1. O MCU 1 (Veículo) está no estado "Verde".<br>2. O pedestre aperta o botão (`sw0`) na MCU 2.<br>3. O MCU 2 envia um sinal de "pedido" ao MCU 1 (Pino X).<br>4. O MCU 1 inicia seu ciclo para fechar (V -> A -> R).<br>5. O MCU 1 envia um sinal de "OK" (Vermelho) ao MCU 2 (Pino Y). | O LED Verde do Pedestre (MCU 2) **só pode** acender *após* receber o sinal de "OK" (Pino Y) do MCU 1. |
| **TI-02** | Sincronismo (Modo Dia) | Conflito de Ciclo Automático | 1. O MCU 1 (Veículo) e o MCU 2 (Pedestre, `MODO 0`) rodam seus ciclos automáticos. | **Resultado Esperado (Falha):** O teste deve falhar, provando que este modo viola o sincronismo. Deve ser documentado que o "Modo Normal (Dia)" automático **NÃO PODE** ser usado em produção com o MCU 1. |
| **TI-03** | Consistência (Falha) | Falha de Comunicação | 1. Executar o cenário TI-01.<br>2. Desconectar o pino de sinal "OK" (Pino Y) do MCU 1. | O MCU 2 (Pedestre) deve permanecer no estado Vermelho e **NUNCA** entrar no estado Verde, mesmo após o botão ter sido pressionado. |