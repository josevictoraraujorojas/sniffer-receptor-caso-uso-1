# Procedimento experimental — receptor UART e MQTT

## Objetivo
Verificar o formato das mensagens publicadas no tópico wifi depois da recepção serial e da inclusão do MAC do receptor.

## Execução
Com o sniffer e o receptor conectados por UART e com ambos os firmwares em execução, capture os dados no broker MQTT:

    mosquitto_sub -h HOST_DO_BROKER -p 1883 -t wifi > captura.jsonl

Encerre com Ctrl+C e execute:

    py tests\validar_experimento.py --autoteste
    py tests\validar_experimento.py --arquivo captura.jsonl --saida resultado.json

O teste espera os campos gerados pelo sniffer e macAddress adicionado pelo receptor. Verifica formato dos três endereços MAC, campos numéricos, canal, RSSI e direção. Registros inválidos ou mensagens de erro fazem o comando retornar código diferente de zero.

## Protocolo sugerido
Faça cinco repetições com o mesmo arranjo físico. Registre quantas mensagens o sniffer emitiu pela UART e quantas chegaram ao tópico wifi. Para comparar as contagens, use os logs dos dois dispositivos e a captura MQTT.

## Limitação
O script valida mensagens observadas no MQTT, mas não mede sozinho perdas na UART. Para isso, compare os contadores do sniffer e do receptor no mesmo intervalo, caso esses contadores estejam disponíveis nos logs.
