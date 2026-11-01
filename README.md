# Firmware de Telemetria Veicular

Firmware embarcado para ESP32-WROOM-32D, desenvolvido com PlatformIO, ESP-IDF e FreeRTOS, destinado à aquisição segura de dados OBD-II e à transmissão de telemetria por BLE para um aplicativo móvel.

## Visão geral

O dispositivo atua como uma ponte somente leitura entre o veículo e o aplicativo. Ele consulta sinais de diagnóstico autorizados, converte os dados para um modelo semântico de telemetria e os disponibiliza por uma comunicação BLE autenticada.

A solução foi projetada para separar o acesso ao veículo, o agendamento de sinais, o controle da sessão, o protocolo binário e o transporte BLE. Essa separação permite evoluir o hardware de diagnóstico sem acoplar os detalhes do veículo ao aplicativo.

## Características do projeto

- ESP32-WROOM-32D-N4 em DevKit;
- MCP2515 com transceptor TJA1050;
- CAN com identificadores padrão de 11 bits e taxa nominal de 500 kbit/s;
- ISO 15765-4 sobre CAN e OBD-II Mode 01;
- aquisição multi-PID, com RPM e velocidade como sinais iniciais;
- BLE/GATT para comunicação com o aplicativo;
- protocolo de aplicação binário com CRC-16/CCITT-FALSE;
- bonding BLE com passkey por dispositivo;
- comunicação veicular exclusivamente somente leitura.

O hardware de protótipo utiliza alimentação de 5 V para o módulo MCP2515/TJA1050, conversão de nível entre o módulo e o ESP32 e um LM2596 precedido por proteção elétrica. A terminação de 120 ohms do módulo deve permanecer desabilitada quando o dispositivo não for um terminador necessário do barramento.

## Arquitetura

```text
Aplicativo BLE
    ↓
BLE/GATT e protocolo binário
    ↓
ControlTask / SessionManager
    ↓
TelemetryScheduler
    ↓
VehicleAccess
    ↓
ObdCanAdapter → MCP2515 → CAN/OBD
```

O contrato `VehicleAccess` mantém o domínio orientado a sinais semânticos, permitindo adicionar futuramente um `ObdKLineAdapter` externo para veículos cujo diagnóstico utilize K-Line. A aplicação e o protocolo não dependem de registradores do MCP2515, frames CAN crus ou detalhes de temporização do veículo.

## Documentação técnica

- [MVP e requisitos](docs/mvp-telemetria.md)
- [Arquitetura e fluxo BLE](docs/arquitetura-fluxo-ble.md)
- [Protocolo BLE binário](docs/protocolo-ble-telemetria.md)
- [Análise de arquitetura e roadmap](docs/ia/analise-codigo-arquitetura-roadmap.md)

Os documentos de análise, decisões de engenharia e roadmap são mantidos em `docs/ia/` como material interno de trabalho.

## Desenvolvimento local

### Build no Windows

O ESP-IDF rejeita caminhos de projeto que contenham espaços. Como o repositório está dentro de uma pasta do Google Drive com espaços, use o wrapper versionado:

```powershell
powershell.exe -ExecutionPolicy Bypass -File .\tools\platformio.ps1 run
```

O wrapper cria temporariamente uma unidade virtual livre, executa o PlatformIO e remove somente o mapeamento criado por ele. Não execute `pio run` diretamente pelo caminho físico original.

### Testes host

```powershell
powershell.exe -ExecutionPolicy Bypass -File .\tools\run-host-tests.ps1
```

Os testes de host não dependem de ESP-IDF, BLE ou CAN.
