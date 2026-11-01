# MVP - Sistema de Telemetria Veicular

## Status

**Baseline:** MVP v1.0 - escopo e arquitetura aprovados para implementação.

Este documento é normativo para o escopo do MVP. Itens descritos como extensão ou validação não fazem parte dos critérios de aceite da primeira versão.

## 1. Objetivo

Entregar um módulo baseado em ESP32 capaz de adquirir RPM e velocidade do veículo por OBD-II sobre CAN, processar os dados e transmiti-los por BLE para um aplicativo Android.

~~~text
Veículo -> OBD-II/CAN -> MCP2515 -> ESP32 -> BLE -> Aplicativo Android
~~~

## 2. Decisões congeladas

- MCU: ESP32-WROOM-32D-N4 montado em DevKit.
- Toolchain: PlatformIO + ESP-IDF.
- Concorrência: FreeRTOS através do ESP-IDF.
- Interface veicular do MVP: MCP2515 + transceptor TJA1050.
- MCP2515: cristal de 8 MHz.
- Barramento inicial: CAN 11-bit, 500 kbit/s.
- Diagnóstico: ISO 15765-4, OBD-II Mode 01.
- Sinais obrigatórios: RPM e velocidade.
- Aquisição: multi-PID desde o início, com scheduler único.
- Limite estrutural do protocolo: até 8 sinais; no MVP, RPM e SPEED são os únicos sinais implementados e a máscara válida é `0x0003`.
- Transporte mobile: BLE/GATT.
- Plataforma mobile do MVP: Android.
- Protocolo de aplicação: binário, com CRC-16/CCITT-FALSE.
- Frame máximo: 20 bytes.
- Segurança BLE: bonding e passkey estática por dispositivo.
- Comunicação veicular: somente leitura; nenhuma escrita arbitrária na ECU.
- Alimentação do protótipo: tomada OBD, placa de proteção e LM2596.

## 3. Backend veicular escalável

O MVP utiliza CAN, mas a arquitetura deve permitir K-Line sem acoplar a aplicação ao meio físico.

~~~text
VehicleAccess
|-- ObdCanAdapter       <- MVP
|   |-- Mcp2515Driver
|   |-- IsoTp
|   '-- ObdService
'-- ObdKLineAdapter     <- extensão futura
    |-- KLineDriver
    |-- ISO14230/KWP2000
    '-- ObdService
~~~

O aplicativo solicita sinais semânticos (RPM, SPEED). O backend ativo resolve transporte, protocolo e decodificador.

O suporte K-Line exige transceptor e interface física próprios. Ele está previsto arquiteturalmente, mas não é requisito funcional do MVP.

## 4. Escopo funcional

### RF-01 - Inicialização

O firmware deve inicializar ESP-IDF/FreeRTOS, GPIOs, SPI, MCP2515, BLE/GATT, watchdog e diagnóstico. Falha de inicialização deve gerar estado de falha e log estruturado.

### RF-02 - Comunicação BLE

O módulo deve anunciar o serviço GATT, aceitar conexão autenticada, receber comandos em Control, enviar respostas e telemetria em Events, fornecer Device Info e detectar desconexão.

### RF-03 - Assinatura multi-PID

RPM e velocidade devem permanecer ativos simultaneamente. O firmware deve validar a máscara `0x0003`, rejeitar bits reservados ou assinaturas inválidas, manter uma única assinatura ativa e impedir tasks ou loops duplicados.

### RF-04 - Aquisição veicular

O firmware deve consultar a ECU usando OBD-II Mode 01 e interpretar, no mínimo:

- PID 0C: RPM;
- PID 0D: velocidade.

Timeout, resposta negativa, PID indisponível e resposta inválida devem ser tratados.

### RF-05 - Telemetria

RPM e velocidade devem ser transmitidos em uma mensagem agrupada com sequence, timestamp relativo, máscara de validade, valores decodificados e CRC válido.

### RF-06 - Controle de sessão

O firmware deve suportar:

~~~text
HELLO
GET_CAPABILITIES
SET_SUBSCRIPTION
START
STOP
PING
GET_STATUS
~~~

Comandos repetidos devem ser idempotentes quando aplicável.

### RF-07 - Falhas e dados desatualizados

O aplicativo deve distinguir BLE desconectado, firmware responsivo sem telemetria, ECU sem resposta, dados stale e sessão encerrada. Não deve afirmar que o veículo foi desligado apenas por ausência de dados.

## 5.1 - Fundação de protocolo implementada

A Fase 1 implementou, em código independente do ESP-IDF:

- frame fixo de 20 bytes com CRC-16/CCITT-FALSE;
- encoder e parser incremental com validação de versão, tipo, tamanho e CRC;
- timeout explícito por relógio monotônico e descarte da mensagem incompleta;
- mensagens tipadas com validação de enum, máscara, versão e booleanos;
- `SequenceGenerator` e `SequenceTracker` por direção/sessão;
- `SessionPolicy` para autenticação, handshake, ordem de comandos e idempotência.

A política de sessão não cria tasks nem acessa BLE, CAN ou FreeRTOS. A integração concorrente pertence à Fase 2.

## 5. Requisitos não funcionais

### RNF-01 - Segurança elétrica e veicular

O sistema deve ser somente leitura e limitar consultas OBD. O módulo MCP2515/TJA1050 será alimentado em 5 V e deverá utilizar conversão de nível entre o módulo e os GPIOs de 3,3 V do ESP32.

O resistor de terminação de 120 ohms do módulo deve permanecer desabilitado.

### RNF-02 - Desempenho inicial

Parâmetros iniciais de teste:

- RPM: 5 Hz;
- velocidade: 5 Hz;
- uma requisição OBD pendente por vez;
- timeout de resposta CAN: 200 ms;
- até 2 retries controlados;
- SPI inicial: 1 MHz.

Esses valores são baseline de teste e podem ser ajustados somente após medição.

### RNF-03 - Confiabilidade

O firmware deve possuir watchdog de tarefas, filas FreeRTOS limitadas, timeout em toda transação veicular, retry limitado, backoff, recuperação de sessão e métricas de timeout, descarte e reconexão.

### RNF-04 - Protocolo

O parser recebe timestamps monotônicos do proprietário do transporte para expirar frames incompletos. A v1 não possui `MAGIC`; após um erro, o valor GATT atual é descartado e a próxima mensagem começa em um novo frame.



O parser deve rejeitar CRC inválido, tamanho inconsistente, tipo desconhecido, payload acima do limite e comando inválido no estado atual. Nenhuma entrada BLE pode causar acesso direto a CAN ou à ECU.

## 6. Fora do MVP

- combustível;
- marca e modelo automáticos;
- K-Line funcional;
- CAN proprietário;
- DTCs;
- escrita na ECU;
- histórico e nuvem;
- OTA;
- iOS como plataforma de validação;
- autodetecção de protocolo veicular.

## 7. Hardware do protótipo

~~~text
OBD pino 16
  -> proteção contra transientes/polaridade
  -> LM2596
  -> alimentação do protótipo

OBD pinos 4/5 -> GND
OBD pino 6    -> CAN-H
OBD pino 14   -> CAN-L
~~~

O LM2596 é aceito somente para protótipo. A solução de produção deverá ser reavaliada para ambiente automotivo.

O módulo MCP2515/TJA1050 deve ser conectado ao ESP32 por level shifting:

~~~text
ESP32 -> módulo: CS, SCK, MOSI
módulo -> ESP32: MISO, INT
~~~

## 8. Critérios de aceite

O MVP será aceito quando:

1. o firmware compilar e gravar via PlatformIO/ESP-IDF;
2. o ESP32 inicializar sem falha;
3. o MCP2515 responder corretamente via SPI;
4. o BLE estabelecer sessão autenticada;
5. RPM e velocidade forem configuradas simultaneamente;
6. os dados forem consultados via CAN/OBD-II;
7. a telemetria binária passar na validação de CRC;
8. CRC inválido, timeout e resposta inválida forem tratados;
9. a desconexão BLE interromper a aquisição;
10. a reconexão exigir novo handshake;
11. comandos repetidos não criarem loops duplicados;
12. a operação for validada repetidamente no veículo-alvo ou em bancada equivalente.

## 9. Validações de hardware

As seguintes atividades são validações, não decisões pendentes:

- confirmar o funcionamento do perfil CAN no Gol G4 Ecomotion;
- validar o cristal de 8 MHz do módulo;
- validar níveis SPI com os conversores;
- confirmar ausência da terminação de 120 ohms;
- validar proteção e aquecimento do LM2596;
- medir taxa de resposta, timeout e carga da rede.

## 10. Evoluções posteriores

- backend K-Line externo;
- combustível;
- identificação por VIN/perfil;
- suporte a outros veículos;
- iOS;
- DTCs somente leitura;
- OTA;
- hardware automotivo dedicado.
