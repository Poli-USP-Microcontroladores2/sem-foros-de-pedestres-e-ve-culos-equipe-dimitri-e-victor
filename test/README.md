# Plano de Testes: Semáforo Veicular (MCU 1)

Este documento descreve o plano de validação e verificação (V&V) para o firmware do **Semáforo Veicular (MCU 1)**, conforme os princípios do **Modelo V**.  
O firmware foi desenvolvido para a **plataforma FRDM-KL25Z** utilizando o **Zephyr RTOS**.

---

## Ambiente de Teste

A validação do firmware será executada em **três fases**:

1. **Validação de Compilação**  
   Garantir que cada modo de operação (`MODO_OPERACAO = 0` ou `1`) compile com sucesso para o alvo FRDM-KL25Z.

2. **Validação de Lógica (Emulação)**  
   Testar a cooperação entre threads (`thread_led_verde`, `thread_led_amarelo`, `thread_led_vermelho`, `thread_botao`), semáforos e mutex usando **Ztest** em ambiente simulado (`native_sim` ou `qemu`).

3. **Validação Funcional (Hardware)**  
   Testar os requisitos funcionais e temporizações diretamente no **hardware real**.

---

## 1. Testes Unitários (TU)

**Objetivo:**  
Validar individualmente os componentes internos e a lógica RTOS do semáforo veicular.  
**Foco:** sincronismo de threads, uso correto de `k_mutex`, `k_sem`, e resposta ao evento de botão (simulado via jumper PTA1 → GND).

| ID do Teste | Módulo Testado         | Cenário de Teste                          | Passos de Execução (Ztest / Emulação)                                                                                                                                   | Resultado Esperado |
|--------------|-----------------------|-------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------|
| **TU-01**    | `k_mutex`             | Exclusão Mútua dos LEDs                   | 1. Compilar em `MODO_OPERACAO = 0`.<br>2. Executar por 3 ciclos completos.<br>3. Verificar logs `printk`.                                                              | Nenhum par de LEDs (Verde + Vermelho) deve acender simultaneamente. |
| **TU-02**    | `thread_led_verde`    | Temporização do Verde                     | 1. Compilar em `MODO_OPERACAO = 0`.<br>2. Medir tempo entre `LED VERDE aceso` e `LED VERDE apagado`.                                                                  | Deve durar **≈ 3000 ms ± tolerância**. |
| **TU-03**    | `thread_led_amarelo`  | Temporização do Amarelo                   | 1. Compilar em `MODO_OPERACAO = 0`.<br>2. Medir tempo entre `LED AMARELO aceso` e `LED AMARELO apagado`.                                                              | Deve durar **≈ 1000 ms ± tolerância**. |
| **TU-04**    | `thread_led_vermelho` | Temporização do Vermelho                  | 1. Compilar em `MODO_OPERACAO = 0`.<br>2. Medir tempo entre `LED VERMELHO aceso` e `LED VERMELHO apagado`.                                                            | Deve durar **≈ 4000 ms ± tolerância**. |
| **TU-05**    | `thread_botao`        | Detecção de Pedestre (ISR simulada)       | 1. Compilar em `MODO_OPERACAO = 0`.<br>2. Forçar `gpio_pin_get()` = 0 via mock / jumper.<br>3. Observar mensagem `"PEDESTRE SOLICITOU TRAVESSIA"`.                    | Flag `pedestre_solicitado` deve tornar-se `true` e iniciar o ciclo `AMARELO → VERMELHO`. |

---

## 2. Testes de Sistema (TS)

**Objetivo:**  
Validar que o sistema cumpre os **requisitos funcionais de operação** e responde corretamente ao ambiente físico (botão, LEDs).

| ID do Teste | Requisito Testado         | Cenário de Teste                  | Passos de Execução (Hardware)                                                                                                                                  | Resultado Esperado |
|--------------|--------------------------|-----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------|
| **TS-01**    | Compilação do Modo Dia   | MODO_OPERACAO = 0                 | 1. Definir `#define MODO_OPERACAO 0`.<br>2. Executar `west build` ou `platformio run`.                                                                          | Compilação concluída com **[SUCCESS]**. |
| **TS-02**    | Ciclo Automático (Dia)   | LEDs alternando corretamente      | 1. Fazer upload no hardware.<br>2. Observar LEDs.<br>3. Medir sequência e tempos.                                                                              | Verde → Amarelo → Vermelho → Verde (3s, 1s, 4s). Nenhum conflito simultâneo. |
| **TS-03**    | Compilação do Modo Noite | MODO_OPERACAO = 1                 | 1. Definir `#define MODO_OPERACAO 1`.<br>2. Compilar.                                                                    | Build **[SUCCESS]**. |
| **TS-04**    | Pisca Noturno (Amarelo)  | Sequência Noturna                 | 1. Fazer upload do firmware (MODO 1).<br>2. Observar LEDs.                                                              | LEDs piscando juntos (amarelo simulado) com período de **2 s** (1 s aceso, 1 s apagado). |
| **TS-05**    | Interação com Botão      | Travessia de Pedestre (Dia)       | 1. Firmware com `MODO_OPERACAO = 0`.<br>2. Conectar jumper PTA1 → GND.<br>3. Observar transição automática para ciclo pedestre. | Ao detectar o botão, o sistema força transição para **amarelo → vermelho**, desativando o verde até concluir o ciclo. |

---

## 3. Testes de Integração (TI)

**Objetivo:**  
Validar o sincronismo entre **MCU 1 (Veículos)** e **MCU 2 (Pedestres)**.  
Garantir que **nunca** ocorra o estado simultâneo **Veículo Verde** e **Pedestre Verde**.

| ID do Teste | Requisito Testado | Cenário de Teste | Passos de Execução (Hardware) | Resultado Esperado |
|--------------|------------------|------------------|--------------------------------|--------------------|
| **TI-01** | Sinalização de Pedido | Travessia por Botão | 1. MCU 1 (Veículo) em "Verde".<br>2. MCU 2 (Pedestre) pressiona botão.<br>3. MCU 2 envia sinal (Pino X) para MCU 1.<br>4. MCU 1 inicia sequência de fechamento (V→A→R). | MCU 1 só retorna sinal "OK" (Pino Y) quando estiver em Vermelho. |
| **TI-02** | Sincronismo Total | Ciclo Coordenado | 1. MCU 1 e MCU 2 executando.<br>2. Pedestre solicita travessia. | **Nunca** ocorre Verde (Veículo) + Verde (Pedestre). |
| **TI-03** | Falha de Comunicação | Pino Y desconectado | 1. Repetir TI-01.<br>2. Desconectar linha “OK” (Y). | MCU 2 mantém Vermelho permanentemente — travessia não liberada. |

---

## 4. Critérios de Aceitação

- Todos os testes TU, TS e TI devem resultar em **[PASS]**.  
- Nenhum conflito de LEDs (Verde + Vermelho simultâneo).  
- A temporização deve estar dentro de ±10 % dos tempos especificados.  
- Em modo noturno, o sistema deve permanecer em loop piscante contínuo.  
- O sistema deve operar de forma determinística, sem deadlocks ou starvation entre threads.

---

## 5. Referências

- **Plataforma:** NXP FRDM-KL25Z  
- **RTOS:** Zephyr 3.x  
- **Ferramentas:** `west`, `platformio`, `Ztest`, `QEMU`  


---

**Autor:** Dimitri Garcia  
 
**Versão:** 1.0
