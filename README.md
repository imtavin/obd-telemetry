# Sistema de Telemetria OBD2 (ESP32 + BLE)

Este projeto implementa um **firmware de telemetria veicular** utilizando **ESP32**, comunicação **OBD2 via CAN** e **Bluetooth Low Energy (BLE)** para interação com uma aplicação móvel.

A arquitetura segue princípios de **Clean Architecture**, **separação de responsabilidades**, **baixo acoplamento** e **alto desempenho**, considerando as limitações de memória e processamento de sistemas embarcados.

---

## Objetivos do Projeto

- Ler dados OBD2 (PIDs) via CAN Bus
- Processar e interpretar dados no firmware
- Expor dados via protocolo customizado sobre BLE
- Manter sistema robusto com watchdog, timeout e máquina de estados
- Facilitar manutenção e evolução do código

---

## Arquitetura Geral

O firmware é dividido em módulos bem definidos:

- **Protocol**: comunicação APP ↔ ESP (parser/encoder)
- **Control**: máquina de estados, fluxo principal e timeouts
- **OBD**: comunicação CAN e interpretação de PIDs
- **BLE**: servidor BLE e callbacks
- **Device**: abstrações de hardware (LED, erros)
- **Utils**: utilitários genéricos (timer, buffers)

Não há dependência circular entre módulos críticos.

---

## Estrutura de Pastas

```text
src/
├── main.cpp
│
├── protocol/
│   ├── ProtocolParser.h / .cpp
│   ├── ProtocolEncoder.h / .cpp
│   └── ProtocolTypes.h
│
├── control/
│   ├── StateMachine.h / .cpp
│   ├── DeviceState.h
│   ├── LoopController.h / .cpp
│   ├── TimeoutManager.h / .cpp
│
├── obd/
│   ├── CanDriver.h / .cpp
│   ├── PidRegistry.h / .cpp
│   ├── IPidInterpreter.h
│   ├── SpeedInterpreter.cpp
│   ├── RpmInterpreter.cpp
│
├── device/
│   ├── LedController.h / .cpp
│   ├── ErrorCodes.h
│   └── ErrorMapper.cpp
│
├── ble/
│   ├── BleServer.h / .cpp
│   └── BleCallbacks.cpp
│
└── utils/
    ├── Timer.h
    └── RingBuffer.h
```

---

## Fluxo de Comunicação (Resumo)

```text
APP  → START
ESP  → OK-START

APP  → SET-<PID>
ESP  → OK-SET

ESP  → DATA-<PID>:<valor> (stream)

APP  → STOP
ESP  → OK-STOP
```

---

## Concorrência e Threads (ESP32)

- **Core 0**
  - BLE Stack
  - Callbacks BLE

- **Core 1**
  - Loop principal
  - Máquina de estados
  - Leitura CAN / OBD

Comunicação entre tarefas feita via **RingBuffer / Queue**.
Sem lógica pesada dentro de callbacks.

---

## Boas Práticas Aplicadas

- Nenhuma lógica de negócio dentro de callbacks BLE
- Máquina de estados explícita (sem ifs espalhados)
- PIDs interpretados via Strategy Pattern
- Baixo uso de heap
- Watchdog e timeouts isolados
- Código testável e previsível

---

## Requisitos

- ESP32
- Interface CAN (ex: MCP2515)
- VSCode + PlatformIO ou Arduino Framework
- Aplicativo BLE compatível

---

## Próximos Passos

- Persistência de configuração
- Suporte a múltiplos PIDs simultâneos
- Filtro de taxa de envio
- Logs condicionais
