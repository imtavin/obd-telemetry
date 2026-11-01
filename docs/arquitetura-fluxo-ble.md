# Arquitetura e Fluxo BLE - Sistema de Telemetria

## Status

**Baseline:** arquitetura v1.0 - aprovada para implementação do MVP.

## 1. Princípios

- BLE é transporte mobile, não camada de negócio.
- A aplicação trabalha com sinais semânticos.
- O firmware traduz sinais para OBD/CAN.
- CAN e K-Line são backends substituíveis.
- Uma única task é dona da máquina de estados.
- Não existe uma task por PID.
- Callbacks BLE não executam lógica pesada.
- O firmware não encaminha comandos arbitrários para a ECU.

## 2. Arquitetura em camadas

~~~text
Application
|-- SessionManager
|-- SubscriptionManager
|-- TelemetryScheduler
|-- StateMachine
'-- ErrorManager

Protocol
|-- FrameParser
|-- FrameEncoder
|-- MessageTypes
'-- CRC16

Transport
'-- BLE GATT

Vehicle
|-- VehicleAccess
|-- ObdCanAdapter
|   |-- MCP2515/SPI
|   |-- CAN
|   |-- ISO-TP
|   '-- OBD-II
'-- ObdKLineAdapter
    |-- UART
    |-- K-Line
    |-- ISO14230/KWP2000
    '-- OBD-II

Platform
|-- FreeRTOS
|-- GPIO/SPI/UART
|-- Watchdog
|-- NVS
'-- Diagnostics
~~~

## 3. Backend veicular

O contrato interno deve ser independente do meio físico:

~~~text
VehicleAccess
|-- initialize()
|-- getCapabilities()
|-- readSignal(signal_id)
|-- stop()
'-- getHealth()
~~~

No MVP:

~~~text
VehicleBackend = CAN_OBD
~~~

O backend K-Line será adicionado sem alterar o protocolo BLE ou o modelo de sinais.

Autodetecção não será implementada na primeira versão. O backend será selecionado por perfil/configuração para evitar sequências de inicialização indevidas no veículo.

## 4. Tasks FreeRTOS

### ControlTask

Responsável por máquina de estados, comandos, sessão BLE, assinatura, início/parada do scheduler, recuperação e erros.

### TelemetryTask

Responsável por calcular vencimentos, selecionar o próximo sinal, respeitar frequência/prioridade, enviar requisições e publicar amostras agrupadas.

### VehicleTask

No MVP, responsável por SPI, MCP2515, frames CAN, ISO-TP, OBD-II Mode 01, decodificação, timeout e retry. No futuro, executará o backend K-Line quando selecionado.

### BleTxTask

Retira eventos da fila de transmissão e emite notificações BLE.

O stack BLE do ESP-IDF mantém suas próprias tasks. Os callbacks da aplicação apenas validam o tamanho mínimo, copiam o frame e publicam evento em fila.

## 5. Filas e propriedade dos dados

~~~text
BLE callback
   ↓ BleRxQueue
ControlTask
   ↓ VehicleRequestQueue
VehicleTask
   ↓ VehicleResultQueue
TelemetryTask / ControlTask
   ↓ BleTxQueue
BleTxTask
~~~

Regras:

- cada fila possui capacidade definida;
- filas cheias geram métrica de descarte;
- nenhuma chamada bloqueia indefinidamente;
- apenas ControlTask altera estado da sessão;
- apenas VehicleTask acessa MCP2515 e CAN;
- apenas BleTxTask envia notificações.

## 6. Estados

Estados independentes evitam uma enumeração combinatória.

### DeviceHealth

~~~text
BOOTING
READY
FAULT
~~~

### BleLink

~~~text
DISCONNECTED
CONNECTED
~~~

### TelemetrySession

~~~text
IDLE
CONFIGURED
RUNNING
DEGRADED
~~~

Combinações válidas típicas:

~~~text
READY + DISCONNECTED + IDLE
READY + CONNECTED    + IDLE
READY + CONNECTED    + CONFIGURED
READY + CONNECTED    + RUNNING
READY + CONNECTED    + DEGRADED
FAULT + DISCONNECTED + IDLE
~~~

## 7. Fluxo normal

~~~text
BOOT
  ↓
Inicializa hardware, CAN e BLE
  ↓
READY / DISCONNECTED / IDLE
  ↓ BLE connected
READY / CONNECTED / IDLE
  ↓ HELLO
Sessão autenticada
  ↓ GET_CAPABILITIES
Capacidades enviadas
  ↓ SET_SUBSCRIPTION
READY / CONNECTED / CONFIGURED
  ↓ START
READY / CONNECTED / RUNNING
  ↓
Scheduler -> CAN/OBD -> decoder -> telemetria BLE
~~~

## 8. STOP e desconexão

Ao receber STOP:

- interromper consultas;
- invalidar dados dinâmicos;
- responder ACK;
- manter BLE conectado;
- retornar a CONFIGURED.

Ao detectar desconexão BLE:

- parar o scheduler;
- cancelar requisições pendentes;
- invalidar dados dinâmicos;
- liberar a sessão;
- aguardar novo handshake.

## 9. Recuperação

Parâmetros iniciais:

~~~text
CAN response timeout: 200 ms
Retries: 2
Dados stale: 1 s sem amostra válida
Aplicação suspeita: 1 s
Aplicação indisponível: 3 s
~~~

Um timeout isolado não encerra a sessão. Timeouts consecutivos levam a DEGRADED e geram erro estruturado. A recuperação utiliza PING/GET_STATUS e não cria novos loops de aquisição.

## 10. Scheduler multi-PID

~~~text
Subscription
|-- RPM
'-- SPEED
      ↓
Scheduler único
      ↓
Uma transação veicular por vez
~~~

Baseline:

- RPM: 5 Hz;
- velocidade: 5 Hz;
- limite estrutural de 8 sinais; MVP inicia com RPM e velocidade (máscara `0x0003`);
- uma requisição OBD pendente;
- backoff após falhas.

## 11. BLE GATT

Serviço customizado:

~~~text
Telemetry Service
|-- Control      APP -> ESP32, Write / Write With Response
|-- Events       ESP32 -> APP, Notify
'-- Device Info  ESP32 -> APP, Read
~~~

UUIDs normativos:

~~~text
Service:     9b3d0001-5a5b-4b5c-8d15-2f3e0a7c1000
Control:     9b3d0002-5a5b-4b5c-8d15-2f3e0a7c1000
Events:      9b3d0003-5a5b-4b5c-8d15-2f3e0a7c1000
Device Info: 9b3d0004-5a5b-4b5c-8d15-2f3e0a7c1000
~~~

### Control

Recebe comandos binários:

~~~text
HELLO
GET_CAPABILITIES
SET_SUBSCRIPTION
START
STOP
PING
GET_STATUS
~~~

### Events

Envia:

~~~text
ACK
ERROR
CAPABILITIES
STATUS
PONG
TELEMETRY
~~~

### Device Info

Fornece versão do firmware, versão do protocolo, hardware e capacidades estáticas.

## 12. Segurança BLE

- exigir bonding;
- usar passkey estática por dispositivo;
- armazenar chaves e configuração em NVS;
- bloquear comandos antes do handshake;
- aceitar somente tipos de mensagem allowlisted;
- nunca aceitar CAN bruto ou escrita genérica na ECU;
- rejeitar frames inválidos sem alterar estado.

No protótipo, a passkey pode ser provisionada em configuração de desenvolvimento. Em produto, deve ser individual por dispositivo e disponibilizada por etiqueta/QR ou processo de provisionamento.

## 13. Hardware e segurança elétrica

~~~text
OBD 16 -> proteção -> LM2596 -> alimentação do protótipo
OBD 4/5 -> GND
OBD 6/14 -> CAN-H/CAN-L
~~~

O módulo MCP2515/TJA1050 em 5 V exige level shifting para SPI e INT do ESP32. A terminação de 120 ohms do módulo permanece desabilitada.

K-Line futura utilizará um módulo externo com UART, transceptor apropriado e proteção automotiva própria.

## 14. Diagnóstico e métricas

Registrar boot/reset reason, conexão BLE, início/fim de sessão, comandos aceitos/rejeitados, requisições CAN, timeout de ECU, CRC inválido, frames descartados, heap livre, filas cheias, reconexões e uptime.

## 15. Testes arquiteturais

- conexão e handshake;
- assinatura RPM + velocidade;
- repetição idempotente de comandos;
- CRC inválido;
- payload acima de 20 bytes;
- timeout CAN;
- resposta negativa OBD;
- desconexão BLE durante consulta;
- reconexão com novo session ID;
- ausência de tasks duplicadas;
- fila cheia/backpressure;
- operação contínua.
