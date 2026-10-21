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

## Testes experimentais do TCC

### Objetivo e fluxo avaliado

Verificar o recebimento de mensagens JSON pela UART2, a validação dos campos necessários, a complementação com o MAC do receptor e a publicação no tópico MQTT `wifi`. O teste integra-se ao firmware [sniffer-caso-uso-1](https://github.com/josevictoraraujorojas/sniffer-caso-uso-1) e ao backend do [Caso de Uso 1](https://github.com/josevictoraraujorojas/caso-de-uso-1).

### Preparação

1. Conecte os dois ESP32 com TX/RX cruzados conforme a configuração dos firmwares e GND comum; confira a velocidade UART de 921600 baud.
2. Configure Wi-Fi e endereço/porta do broker MQTT no receptor.
3. Inicie broker, InfluxDB, Neo4j e backend do Caso de Uso 1.
4. Identifique a porta serial do receptor e do sniffer.

### Execução e coleta dos logs

Crie a pasta de resultados se necessário: `New-Item -ItemType Directory -Force -Path "$HOME\TCC-Experimentos"`. No terminal ESP-IDF, na raiz deste repositório:

```powershell
idf.py build
idf.py -p COM6 flash
idf.py -p COM6 monitor 2>&1 | Tee-Object -FilePath "C:\Users\Jose\TCC-Experimentos\01-pzem-serial.log"
```

Substitua `COM6` pela porta do receptor. Encerre o monitor com `Ctrl+]` e execute `Stop-Transcript` quando retornar ao prompt. Em outro terminal, inicie a observação MQTT antes de executar os dispositivos:

```powershell
mosquitto_sub -h HOST_DO_BROKER -p 1883 -t wifi -v |
  Tee-Object -FilePath "$HOME\TCC-Experimentos\06-mqtt-wifi.log"
```

Substitua o host pelo endereço real do broker. Inicie o receptor antes do sniffer e aguarde o marcador `[EXPERIMENTO][FIM]` ou a contagem limite configurada.

### Como o teste foi instrumentado

Os logs usam marcadores `[EXPERIMENTO][UART_RECEBIDO]`, `[EXPERIMENTO][MQTT_ENVIO]`, `[EXPERIMENTO][CONTAGEM]` e `[EXPERIMENTO][FIM]`, conforme os pontos instrumentados no firmware. O limite configurado é de até 100 publicações locais MQTT no tópico `wifi`. O retorno do cliente MQTT não garante, por si só, recebimento pelo broker.

### Verificação e interpretação

- Conte mensagens recebidas pela UART, publicações MQTT do receptor e mensagens observadas pelo assinante.
- Confira no backend do Caso de Uso 1 os marcadores `[EXPERIMENTO][MQTT_RECEBIDO]` e de persistência/processamento.
- Consulte no InfluxDB o bucket `caso_uso_1`, medição `trafego_wifi`, no intervalo da execução.
- Registre cada etapa separadamente e anote erros de JSON, UART, Wi-Fi ou MQTT.

Não considere as contagens equivalentes automaticamente: mensagens incompletas ou inválidas podem não ser publicadas, e o broker/assinante/backend podem observar quantidades diferentes.
