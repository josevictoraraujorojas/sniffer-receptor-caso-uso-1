# Receptor Serial e Publicador MQTT — Caso de Uso 1

Firmware ESP-IDF que recebe do ESP32 sniffer as mensagens JSON de captura Wi-Fi por UART, valida e complementa os dados e os publica por MQTT. É a etapa de integração entre o dispositivo de captura e a aplicação que recebe os dados do primeiro caso de uso do TCC.

## Objetivo

Separar a captura de quadros Wi-Fi da comunicação com o broker MQTT. O ESP32 sniffer fica dedicado à observação dos quadros; este receptor recebe os registros pela serial e encaminha as mensagens para a infraestrutura de software.

## Fluxo de funcionamento

1. Inicializa a UART para receber mensagens do dispositivo sniffer.
2. Recompõe as mensagens JSON recebidas pelo canal serial.
3. Valida a presença dos campos necessários, incluindo `cliente` e `bssid`, conforme o formato usado pelo firmware.
4. Acrescenta o endereço MAC do próprio receptor no campo `macAddress`.
5. Conecta-se à rede Wi-Fi e publica as mensagens no tópico MQTT `wifi`.
6. Também pode realizar o registro do dispositivo no tópico `sniffer`, conforme a configuração do firmware.

## Comunicação serial

| Parâmetro | Valor documentado |
|---|---|
| Interface | UART2 |
| GPIO TX | GPIO 17 |
| GPIO RX | GPIO 16 |
| Baud rate | 921600 |
| Formato | JSON delimitado por mensagem |

A configuração da UART deve ser idêntica à do firmware de captura. Conecte TX ao RX do outro ESP32 e compartilhe o GND. Confirme a pinagem nos dois projetos antes de alimentar os dispositivos.

## MQTT

O receptor utiliza Wi-Fi para se conectar ao broker MQTT e encaminhar os dados. Os tópicos documentados são:

- `wifi`: mensagens com metadados de quadros capturados;
- `sniffer`: registro/identificação do dispositivo, de acordo com a implementação.

O endereço do broker e as credenciais de rede devem ser configurados para o ambiente de execução. Evite armazenar senhas reais no repositório.

## Requisitos

- ESP32 receptor
- Firmware de captura do repositório [sniffer-caso-uso-1](https://github.com/josevictoraraujorojas/sniffer-caso-uso-1)
- Conexão UART entre os dois dispositivos
- ESP-IDF instalado e configurado
- Rede Wi-Fi e broker MQTT acessíveis

## Compilação e gravação

Com o ambiente ESP-IDF configurado, execute na raiz do projeto:

```bash
idf.py set-target esp32
idf.py build
idf.py -p PORTA flash monitor
```

Substitua `PORTA` pela porta serial correspondente ao receptor. Se o alvo já estiver definido, pule o comando `set-target`.

## Integração com o TCC

Este projeto faz a ponte entre a aquisição no ESP32 sniffer e o fluxo MQTT consumido pelo backend. A aplicação pode associar as mensagens a dispositivos e locais e persistir os dados nos bancos utilizados pelo trabalho.

## Diagnóstico

- Se não houver mensagens, confira TX/RX cruzados, GND comum e baud rate.
- Se a UART receber JSON incompleto, verifique o delimitador e o tamanho máximo do buffer nos dois firmwares.
- Se o MQTT não publicar, confirme a conexão Wi-Fi, o endereço do broker e o nome do tópico.
