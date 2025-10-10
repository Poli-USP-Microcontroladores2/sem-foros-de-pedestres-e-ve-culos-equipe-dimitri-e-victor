# PSI-Microcontroladores2-Aula01

Atividade prática inicial da aula 1 da disciplina Microcontroladores 2 (PSI).

Registre sua evolução utilizando os checklists abaixo por meio de commits com mensagens descritivas:

### Setup (individual)
- [ X] Possuo VSCode instalado
- [ X] Possuo MCUXpresso instalado
- [ X] Possuo PlatformIO instalado
- [ X] Possuo cabo USB compatível com Placa Freedom
- [ X] Possuo Placa Freedom operacional
- [ X] Possuo Zephyr atualizado para versão 4.2

Materiais para consulta: https://frdm-kl25z-com-zephyr-rtos.super.site/instalao, https://frdm-kl25z-com-zephyr-rtos.super.site/tutoriais/1-exemplo-pisca-led-rtos/atualizao-para-o-zephyr-42

### Atividade Baremetal (implementação individual, validação em dupla)
- [X ] Crie uma branch chamada baremetal
- [X ] Compile e execute o exemplo de piscar led baremetal na placa Freedom com o Zephyr versão 4.2 (vide [https://frdm-kl25z-com-zephyr-rtos.super.site/tutoriais/2-pisca-led-simples](https://frdm-kl25z-com-zephyr-rtos.super.site/tutoriais/1-exemplo-pisca-led-rtos/atualizao-para-o-zephyr-42)). Dica: Utilize por enquanto somente o led da placa
- [ X] Faça alterações no código para controlar dois leds: um led deve piscar a cada segundo, e o segundo led deve piscar a cada meio segundo
- [ X] Desafio: tente fazer com que o primeiro led pisque a cada segundo, e o segundo led pisque a cada 350 ms
- [ ] Crie um planejamento de testes para verificação do correto funcionamento, detalhando quais são os resultados esperados
- [ X] Abra uma Pull Request
- [ ] Seu par irá avaliar a Pull Request: adicionar sugestões em issues ou na discussão do PR (ATENÇÃO: o par deve tentar executar os testes propostos, e mesmo em caso de tudo funcionar, registrar em comentários como a validação foi realizada)
- [ ] Após as pendências serem resolvidas, seu par irá validar as melhorias e aceitar a PR

### Atividade Threads (implementação individual, validação em dupla)
- [ X] Crie uma branch chamada threads
- [ ] Compile e execute o exemplo de piscar led com threads na placa Freedom (vide [https://github.com/platformio/platform-freescalekinetis/blob/develop/examples/zephyr-blink/src/main.c e https://frdm-kl25z-com-zephyr-rtos.super.site/tutoriais/1-exemplo-pisca-led-rtos](https://frdm-kl25z-com-zephyr-rtos.super.site/tutoriais/threads-zephyr-42))
- [ ] Faça alterações no código para controlar dois leds: um led deve piscar a cada segundo, e o segundo led deve piscar a cada meio segundo
- [ ] Desafio: tente fazer com que o primeiro led pisque a cada segundo, e o segundo led pisque a cada 350 ms
- [ ] Crie um planejamento de testes para verificação do correto funcionamento, detalhando quais são os resultados esperados
- [ ] Abra uma Pull Request
- [ ] Seu par irá avaliar a Pull Request: adicionar sugestões em issues ou na discussão do PR (ATENÇÃO: o par deve tentar executar os testes propostos, e mesmo em caso de tudo funcionar, registrar em comentários como a validação foi realizada)
- [ ] Após as pendências serem resolvidas, seu par irá validar as melhorias e aceitar a PR

### Desafio (individual)
- [ ] Explore recursos de debug e faça um arquivo markdown mostrando como estes recursos podem ser úteis na identificação e correção de problemas
- [ ] Reflita e responda em seu arquivo markdown: qual a diferença entre os recursos de debug e testes? São abordagens complementares?
