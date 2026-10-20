# Procedimento experimental — receptor UART e publicador MQTT

O objetivo é acompanhar as mensagens capturadas pelo sniffer desde a recepção serial até a publicação no broker MQTT.

## 1. Abrir o monitor serial do receptor

Com o receptor conectado ao computador, execute no terminal ESP-IDF:

```powershell
idf.py -p COM6 monitor
```

Substitua `COM6` pela porta do receptor. Quando um JSON completo chega pela UART, o firmware registra:

```text
[EXPERIMENTO][UART_RECEBIDO] bytes=... payload={...}
```

Em seguida, se o JSON passar na validação, procure:

```text
[EXPERIMENTO][MQTT_ENVIO] topico=wifi mensagem_id=... payload={...}
```

Esse registro mostra a mensagem encaminhada e o identificador retornado pela chamada local ao cliente MQTT. Se houver falha, procure `[EXPERIMENTO][MQTT_ENVIO_FALHA]`; mensagens que não passaram pela validação geram `[EXPERIMENTO][JSON_REJEITADO]`.

## 2. Conferir o que chegou ao broker

Em outro terminal, execute:

```powershell
mosquitto_sub -h HOST_DO_BROKER -p 1883 -t wifi -v
```

Troque o host pelo broker do experimento. Compare o JSON recebido no broker com o payload mostrado no log do receptor. Isso separa a recepção pela UART da publicação observada no broker.

## 3. Conferir o backend e a persistência

No console do backend do Caso de Uso 1, procure `[EXPERIMENTO][MQTT_RECEBIDO]`, `[EXPERIMENTO][PROCESSAMENTO_OK]`, `[EXPERIMENTO][INFLUX_PERSISTIDO]` e possíveis linhas de falha. O log de persistência informa o measurement, o bucket, o horário e os identificadores das entidades associadas.

## Dados a registrar

Em cada repetição, registre quadros enviados pelo sniffer, mensagens recebidas na UART do receptor, publicações no MQTT, mensagens recebidas pelo backend, registros persistidos e falhas. Não confunda o retorno local da função de publicação MQTT com a confirmação da gravação no banco. A persistência é verificada separadamente nos logs do backend e na consulta ao InfluxDB.
