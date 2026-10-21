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


## Limite do ensaio e salvamento dos logs

O receptor encerra o fluxo experimental após 100 publicações aceitas localmente pelo cliente MQTT no tópico `wifi`. O contador não comprova entrega ao broker nem persistência no backend. O sniffer transmissor tem seu próprio limite de 100 mensagens enviadas pela UART; compare as contagens para identificar perdas entre as etapas.

Para gravar a saída serial em um arquivo no computador, abra o PowerShell na pasta do projeto e execute (ajuste a porta serial conforme necessário):

```powershell
idf.py -p COM6 flash monitor 2>&1 | Tee-Object -FilePath experimento-receptor.log
```

O arquivo `experimento-receptor.log` será criado na pasta atual. Se o firmware já estiver gravado, use `idf.py -p COM6 monitor 2>&1 | Tee-Object -FilePath experimento-receptor.log` para não regravar a placa. Guarde o arquivo de cada execução separadamente e não reutilize o mesmo nome se quiser preservar o ensaio anterior.

Para salvar também as mensagens observadas no broker, em outro PowerShell execute:

```powershell
mosquitto_sub -h HOST_DO_BROKER -p 1883 -t wifi -v 2>&1 | Tee-Object -FilePath experimento-broker.log
```

Substitua `HOST_DO_BROKER` pelo host configurado no firmware. O arquivo do broker deve ser separado do log serial.
