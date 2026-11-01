# Protocolo BLE de Telemetria

## Status

**Versão:** Telemetry Protocol v1.0 - baseline congelada para o MVP.

**Implementação:** contrato binário implementado em `src/protocol/`, com testes host independentes do ESP-IDF.

Este protocolo substitui a proposta textual anterior. A implementação deve usar frames binários nas characteristics BLE.

## 1. Transporte

O protocolo é transportado por BLE/GATT:

~~~text
Control      APP -> ESP32
Events       ESP32 -> APP
Device Info  ESP32 -> APP
~~~

Cada valor GATT contém exatamente um frame completo. A versão inicial não utiliza fragmentação BLE.

O `ProtocolParser` aceita fragmentos para testes e para transporte interno, mas a camada BLE deve rejeitar ou descartar uma escrita que não contenha o frame completo. Como a v1 não possui `MAGIC`, a recuperação de sincronismo é definida por mensagem: após erro de CRC ou tamanho, o valor atual é descartado e o parser é reiniciado. Não há garantia de recuperação em um stream arbitrário concatenado.

Limite máximo de frame:

~~~text
20 bytes
~~~

## 2. Frame

~~~text
+---------+------+----------+--------+---------+-------+
| VERSION | TYPE | SEQUENCE | LENGTH | PAYLOAD | CRC16 |
| 1 byte  | 1    | 2 bytes  | 1 byte | 0..13   | 2     |
+---------+------+----------+--------+---------+-------+
~~~

Total máximo:

~~~text
1 + 1 + 2 + 1 + 13 + 2 = 20 bytes
~~~

Não existem MAGIC ou FLAGS na v1. O campo LENGTH valida a estrutura.

Todos os campos numéricos com mais de um byte usam little-endian.

## 3. CRC

Algoritmo congelado:

~~~text
CRC-16/CCITT-FALSE
Polynomial: 0x1021
Initial: 0xFFFF
RefIn: false
RefOut: false
XorOut: 0x0000
~~~

O CRC é calculado sobre:

~~~text
VERSION + TYPE + SEQUENCE + LENGTH + PAYLOAD
~~~

O campo CRC16 não participa do próprio cálculo.

Frame com CRC inválido deve ser descartado e contabilizado. Não deve executar comando nem alterar estado.

## 4. Tipos de mensagem

~~~text
0x01 HELLO
0x02 GET_CAPABILITIES
0x03 SET_SUBSCRIPTION
0x04 START
0x05 STOP
0x06 PING
0x07 GET_STATUS

0x80 ACK
0x81 ERROR
0x82 PONG
0x83 CAPABILITIES
0x84 STATUS
0x85 TELEMETRY
~~~

Tipos desconhecidos devem ser rejeitados.

## 5. Sequence

SEQUENCE é um contador de 16 bits por direção e por sessão.

- comandos enviados pelo aplicativo possuem sequência própria;
- eventos enviados pelo ESP32 possuem sequência própria;
- respostas carregam a sequência de referência no payload;
- overflow retorna a zero;
- a comparação deve considerar wrap-around.

O sequence da telemetria permite detectar perda, duplicação ou reordenação.

A validação não pertence ao parser de bytes: o `SessionManager` mantém um `SequenceTracker` por direção e por sessão, reiniciando-o em nova conexão ou novo handshake. Distância `1` significa sequência esperada; distância positiva menor que `0x8000` indica gap; distância zero indica duplicação; os demais casos indicam reordenação. O contador possui wrap-around de 16 bits.

## 6. Session ID

O aplicativo gera um SESSION_ID aleatório de 32 bits.

Ele é utilizado em HELLO e na confirmação da sessão. O firmware associa o ID à conexão BLE atual e invalida a sessão na desconexão.

O ID não aparece em cada frame de telemetria para preservar o limite de 20 bytes. A telemetria só é aceita quando existe uma sessão ativa e válida.

## 7. Payloads de comandos

### HELLO - 0x01

~~~text
SESSION_ID       4 bytes
MIN_VERSION      1 byte
MAX_VERSION      1 byte
~~~

### GET_CAPABILITIES - 0x02

Payload vazio.

### SET_SUBSCRIPTION - 0x03

~~~text
SIGNAL_MASK      2 bytes
~~~

Máscara inicial:

~~~text
bit 0 = RPM
bit 1 = SPEED
~~~

O formato reserva oito bits e o limite estrutural continua sendo oito sinais. No MVP, somente os bits 0 e 1 estão implementados:

- bit 0 = RPM;
- bit 1 = SPEED.

Portanto, a máscara válida do MVP é `0x0003`. Bits 2 a 7 são reservados e devem ser rejeitados até que seus sinais sejam definidos e implementados.

### START - 0x04

Payload vazio. Exige sessão válida e assinatura válida.

### STOP - 0x05

Payload vazio. Deve ser idempotente.

### PING - 0x06

Payload vazio. Não deve consultar a ECU.

### GET_STATUS - 0x07

Payload vazio.

## 8. Payloads de resposta

### ACK - 0x80

~~~text
REFERENCE_SEQUENCE  2 bytes
COMMAND_TYPE        1 byte
RESULT              1 byte
SESSION_ID          4 bytes
~~~

RESULT = 0 indica aceitação; valores diferentes de zero são reservados para resultados de rejeição ou processamento futuro.

O ACK significa que o comando foi recebido, validado e aceito pela lógica de aplicação. Não significa que uma consulta futura à ECU já retornou dados.

### ERROR - 0x81

~~~text
REFERENCE_SEQUENCE  2 bytes
DOMAIN              1 byte
CODE                1 byte
SEVERITY            1 byte
RECOVERABLE         1 byte
CONTEXT             0..3 bytes

`RECOVERABLE` deve ser codificado como `0` ou `1`; outros valores tornam o payload inválido.
~~~

Domínios:

~~~text
0x01 SYSTEM
0x02 BLE
0x03 PROTOCOL
0x04 CAN
0x05 OBD
0x06 DATA
~~~

Códigos mínimos:

~~~text
0x01 INVALID_FRAME
0x02 CRC_INVALID
0x03 UNKNOWN_MESSAGE
0x04 INVALID_STATE
0x05 UNSUPPORTED_VERSION
0x06 UNSUPPORTED_SIGNAL
0x07 SUBSCRIPTION_LIMIT
0x08 CAN_INIT_FAILED
0x09 CAN_TIMEOUT
0x0A CAN_BUS_OFF
0x0B ECU_NEGATIVE_RESPONSE
0x0C INVALID_ECU_RESPONSE
0x0D DATA_STALE
0x0E BLE_SESSION_ERROR
~~~

Severidade:

~~~text
0x01 INFO
0x02 WARNING
0x03 ERROR
0x04 FATAL
~~~

### PONG - 0x82

~~~text
REFERENCE_SEQUENCE  2 bytes
~~~

### CAPABILITIES - 0x83

~~~text
SIGNAL_MASK         2 bytes
BACKEND_MASK        1 byte
MAX_SIGNALS         1 byte
~~~

Backend:

~~~text
bit 0 = CAN_OBD
bit 1 = KLINE_OBD
~~~

No MVP, somente `CAN_OBD` (`0x01`) estará ativo. `KLINE_OBD` (`0x02`) fica reservado para extensão e pode ser anunciado apenas quando houver backend implementado.

### STATUS - 0x84

~~~text
DEVICE_HEALTH       1 byte
BLE_STATE           1 byte
SESSION_STATE       1 byte
VEHICLE_STATE       1 byte
SIGNAL_MASK         2 bytes
LAST_ERROR_CODE     1 byte
~~~

### TELEMETRY - 0x85

~~~text
TIMESTAMP_MS        4 bytes
VALID_MASK          2 bytes
RPM                 2 bytes
SPEED               2 bytes
~~~

O timestamp é relativo ao início da sessão e não representa horário civil. `RPM` é transmitido como RPM inteiro em `uint16_t`; `SPEED` é transmitido em km/h inteiro em `uint16_t`. A validade de cada valor é indicada por `VALID_MASK`.

Máscara de validade:

~~~text
bit 0 = RPM válido
bit 1 = SPEED válido
~~~

Quando um bit não estiver ativo, o valor correspondente não deve ser usado pela aplicação. Bits fora de `0x0003` tornam o payload inválido no MVP.

## 9. Máquina de estados do protocolo

~~~text
DISCONNECTED
    ↓ BLE connect
CONNECTED
    ↓ HELLO válido
SESSION_ACTIVE
    ↓ SET_SUBSCRIPTION
CONFIGURED
    ↓ START
STREAMING
    ↓ STOP
CONFIGURED
    ↓ BLE disconnect
DISCONNECTED
~~~

Regras:

- SET_SUBSCRIPTION sem HELLO é rejeitado;
- START sem assinatura é rejeitado;
- DATA não é aceito do aplicativo;
- STOP repetido é aceito de forma idempotente;
- novo BLE connect exige novo handshake;
- erro de sessão não pode iniciar aquisição.

A política pura em `src/control/SessionPolicy.cpp` aplica essas transições sem FreeRTOS. O `ControlTask` será responsável por serializar seus eventos e chamar essa política na Fase 2.

## 10. Estados de erro e recuperação

~~~text
requisição -> timeout
  ↓
retry limitado
  ↓
DEGRADED + ERROR CAN_TIMEOUT
~~~

A aplicação marca dados como stale após 1 segundo sem amostra válida e indisponíveis após 3 segundos sem recuperação.

## 11. BLE e segurança

- serviço GATT customizado;
- characteristic Control com Write With Response;
- characteristic Events com Notify;
- característica Device Info com Read;
- bonding obrigatório;
- passkey estática individual por dispositivo;
- chaves persistidas em NVS;
- comandos somente após autenticação e handshake.

## 12. Testes obrigatórios

- frame válido;
- CRC válido;
- CRC inválido;
- tamanho incorreto;
- payload acima de 13 bytes;
- tipo desconhecido;
- versão incompatível;
- SET_SUBSCRIPTION com RPM + SPEED;
- assinatura acima do limite;
- comandos repetidos;
- comandos fora de ordem;
- perda de sequence;
- timeout CAN;
- resposta negativa da ECU;
- desconexão e nova sessão;
- telemetria stale;
- frame de telemetria com valores inválidos.

A suíte host atual cobre CRC, limites, round-trip, fragmentação, timeout do parser, ressincronização na próxima mensagem, validações tipadas, sequence com wrap-around e política de sessão.

## 13. Compatibilidade futura

Qualquer alteração incompatível deve incrementar VERSION. A futura implementação K-Line não altera os tipos de sinal nem o protocolo BLE; altera somente o backend veicular reportado em CAPABILITIES.
