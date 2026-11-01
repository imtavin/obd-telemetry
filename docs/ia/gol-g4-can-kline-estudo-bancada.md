# Estudo de bancada - VW Gol G4 Ecomotion 2013: CAN, K-Line e OBD

## Classificacao e objetivo

Este e um material interno de levantamento. Ele nao altera o contrato do MVP
nem transforma informacao encontrada na internet em uma caracteristica
confirmada do veiculo. A fonte de verdade para o perfil ativo continuara sendo
a captura de bancada registrada para este carro.

Objetivo: decidir, com evidencias, se o MVP CAN/OBD e viavel no veiculo de
referencia e produzir amostras anonimizadas para os testes host.

## Decisoes ja aprovadas no projeto

- MVP: ESP32 + MCP2515/TJA1050 + CAN OBD; comunicacao somente leitura.
- Perfil inicial: CAN de 11 bits, 500 kbit/s, ISO 15765-4, OBD Mode 01.
- Requisicao funcional prevista: `0x7DF`; respostas aceitas: `0x7E8` a
  `0x7EF`.
- Sinais do MVP: RPM (PID `0x0C`) e velocidade (PID `0x0D`).
- K-Line e extensao futura, com transceptor e backend proprio. Ela nao deve
  ser conectada ao MCP2515 nem ser implementada como atalho no adapter CAN.

## Fatos tecnicos externos, nao especificos deste carro

No conector diagnostico SAE J1962 de 16 pinos, as linhas relevantes sao:

| Pino | Funcao |
| --- | --- |
| 4 | terra de chassi |
| 5 | terra de sinal |
| 6 | CAN High para ISO 15765-4 |
| 7 | K-Line para ISO 9141-2 / ISO 14230-4 |
| 14 | CAN Low para ISO 15765-4 |
| 15 | L-Line opcional |
| 16 | alimentacao da bateria |

Para OBD sobre CAN com identificadores padrao, `0x7DF` e o endereco funcional
de requisicao externa; as respostas OBD legisladas ocupam `0x7E8` a `0x7EF`.
Uma resposta Mode 01 bem-sucedida usa servico `0x41` e repete o PID solicitado.

Referencias abertas utilizadas neste levantamento:

- [Pinagem SAE J1962 - DashLogic](https://dashlogic.com/docs/technical/obdii_connector_pinout)
- [Especificacao de cabo CAN OBD-II - National Instruments](https://www.ni.com/en/support/documentation/supplemental/18/specifications-for-the-can-obd-ii-cable.html)
- [ISO 15765-4, mapeamento de IDs OBD](https://machdienlythu.vn/wp-content/uploads/2020/12/iso_15765-4.pdf)
- [Explicacao de OBD-II/Mode 01 e IDs funcionais](https://leaukojo.github.io/sloppycan/dev/explainers/obd2-explainer.html)

Essas referencias descrevem padroes e interfaces. Elas **nao confirmam** qual
protocolo esta conectado ao DLC deste Gol G4 especifico.

## Estado de conhecimento do veiculo de referencia

| Afirmação | Estado | Como fechar |
| --- | --- | --- |
| O DLC expoe CAN nos pinos 6 e 14 | Nao confirmado | Inspecao de pinagem e captura passiva |
| A ECU responde a OBD CAN em 500 kbit/s | Nao confirmado | Captura e consulta Mode 01 controlada |
| RPM e velocidade estao disponiveis por PIDs 0C e 0D | Nao confirmado | Validar servico, PID, tamanho e valor |
| K-Line existe e e o diagnostico necessario | Nao confirmado | Somente investigar se CAN nao responder |
| ECU Bosch ME7.5.30 | Hipotese | Etiqueta/modulo ou ferramenta diagnostica |

Ausencia de resposta CAN nao prova, por si so, que o carro usa K-Line. Antes
de concluir, registrar alimentacao, terras, pinagem, bitrate, formato de
quadro, identificadores vistos e a configuracao do instrumento.

## Roteiro de bancada seguro

### 1. Antes de conectar o ESP32

1. Confirmar o conector DLC e os pinos 4, 5, 6, 7, 14 e 16 com diagrama ou
   instrumento apropriado.
2. Confirmar que o modulo MCP2515 usa cristal de 8 MHz.
3. Confirmar level shifting para todos os sinais entre ESP32 de 3,3 V e o
   modulo MCP2515/TJA1050 de 5 V.
4. Confirmar que a terminacao de 120 ohms do modulo esta desabilitada.
5. Validar primeiro SPI e loopback CAN em bancada isolada do veiculo.

Nao ligar GPIO de 3,3 V diretamente a sinal de 5 V. Nao usar a alimentacao
OBD sem a protecao definida para o prototipo. Nao enviar escrita, codificacao,
DTC-clear, comandos proprietarios ou atuacao.

### 2. Captura CAN passiva

Com a interface em modo listen-only, registrar ao menos 60 s em cada cenario:

1. ignicao ligada, motor parado;
2. motor em marcha lenta;
3. motor com RPM levemente variada, parado e em condicao segura;
4. percurso curto e seguro, se necessario para correlacionar velocidade.

Registrar bitrate, IDs de 11/29 bits, DLC, dados e timestamp monotônico. A
captura passiva nao substitui a validacao de OBD: pode nao haver trafego
diagnostico espontaneo no DLC.

### 3. Consultas OBD de leitura

Somente se CAN e bitrate forem confirmados, enviar uma requisicao por vez,
esperar a resposta ou timeout e registrar o resultado.

| Sinal | Requisicao CAN esperada | Resposta valida esperada |
| --- | --- | --- |
| RPM | `0x7DF: 02 01 0C 00 00 00 00 00` | `04 41 0C A B ...`; RPM = `(A * 256 + B) / 4` |
| Velocidade | `0x7DF: 02 01 0D 00 00 00 00 00` | `03 41 0D A ...`; velocidade = `A` km/h |

Aceitar somente respostas em `0x7E8` a `0x7EF` que validem PCI, servico,
PID e numero de bytes. Preservar tambem timeout, resposta negativa e resposta
de mais de uma ECU; eles sao cenarios de teste, nao erros a ocultar.

Comecar com a baseline atual: uma requisicao pendente, timeout de 200 ms e no
maximo duas tentativas. Os valores so serao alterados depois da medicao.

### 4. Caminho K-Line, se CAN nao se confirmar

Nao reutilizar MCP2515/TJA1050 para K-Line. Antes de qualquer transmissao,
registrar se o pino 7 esta presente e selecionar um transceptor K-Line
compativel com o nivel eletrico e protecoes automotivas. O suporte exigira um
backend separado (UART + ISO 9141-2/ISO 14230-4), portanto e uma decisao de
escopo posterior, nao uma adaptacao de configuracao CAN.

## Formato de captura para simulacao

Manter dumps crus fora do Git. Para cada evento, registrar localmente:

```text
timestamp_monotonic_us
cenario
direcao                  # RX ou TX
transporte               # CAN ou KLINE
can_id                   # vazio para K-Line
id_format                # standard_11, extended_29 ou n/a
dlc                      # vazio para K-Line
dados_hex
pid_solicitado           # 0C, 0D ou vazio
latencia_ms              # TX ate resposta; vazio para passivo
resultado                # valid, timeout, negative, malformed, duplicate
referencia_externa       # painel, GPS ou instrumento; sem VIN/identificador
```

Das capturas, extrair fixtures anonimizadas para o repositorio com, no minimo:

- resposta RPM valida;
- resposta de velocidade valida;
- timeout;
- PID/servico/ID invalidos;
- resposta negativa;
- snapshot parcial e dado stale.

## Criterio de decisao

O CAN/OBD do MVP fica confirmado somente quando houver resposta repetivel e
validada aos PIDs 0C e 0D, em IDs e bitrate registrados, sem violar as regras
eletricas. Caso contrario, registrar a evidencia e abrir decisao arquitetural
para um backend K-Line separado; nao alterar o protocolo BLE nem anunciar
K-Line em CAPABILITIES antes de implementa-lo.
