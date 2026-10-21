
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

static const char *TAG = "SNIFFER_RECEPTOR";

/* Configuracoes da UART */
#define UART_PORT          UART_NUM_2
#define UART_TX_PIN        17
#define UART_RX_PIN        16
#define UART_DRIVER_BUFFER 2048
#define UART_READ_BUFFER   256

/* Configuracoes Wi-Fi */
#define WIFI_SSID "Rojas_wgo_fibra"
#define WIFI_PASS "jr131415"

/* Configuracoes MQTT */
#define MQTT_BROKER_URI     "mqtt://192.168.137.31:1883"
#define MQTT_TOPIC          "wifi"
#define MQTT_REGISTER_TOPIC "sniffer"

/* Tamanhos dos buffers */
#define JSON_BUFFER_SIZE  1024
#define MQTT_MESSAGE_SIZE 1200
#define TOTAL_MENSAGENS_TESTE 100

/* Variaveis globais */
static char mac_dispositivo[18];
static esp_mqtt_client_handle_t cliente_mqtt = NULL;
static bool tarefa_uart_iniciada = false;
static int mensagens_mqtt_publicadas = 0;

/* Registra a tentativa e o resultado local da publicação MQTT. */
static int publicar_mensagem_experimental(
    esp_mqtt_client_handle_t cliente,
    const char *topico,
    const char *payload,
    int qos
)
{
    if (cliente == NULL || topico == NULL || payload == NULL)
    {
        ESP_LOGE(TAG, "[EXPERIMENTO][MQTT_ENVIO_FALHA] cliente, topico ou payload nulo");
        return -1;
    }

    if (strcmp(topico, MQTT_TOPIC) == 0 && mensagens_mqtt_publicadas >= TOTAL_MENSAGENS_TESTE)
        return -2;

    int mensagem_id = esp_mqtt_client_publish(
        cliente, topico, payload, 0, qos, 0
    );

    if (mensagem_id < 0)
    {
        ESP_LOGE(TAG, "[EXPERIMENTO][MQTT_ENVIO_FALHA] topico=%s payload=%s",
                 topico, payload);
    }
    else
    {
        ESP_LOGI(TAG, "[EXPERIMENTO][MQTT_ENVIO] topico=%s mensagem_id=%d qos=%d payload=%s",
                 topico, mensagem_id, qos, payload);
    }

    if (mensagem_id >= 0 && strcmp(topico, MQTT_TOPIC) == 0)
    {
        mensagens_mqtt_publicadas++;
        ESP_LOGI(TAG, "[EXPERIMENTO][CONTAGEM] mensagens_mqtt=%d/%d",
                 mensagens_mqtt_publicadas, TOTAL_MENSAGENS_TESTE);
        if (mensagens_mqtt_publicadas == TOTAL_MENSAGENS_TESTE)
            ESP_LOGI(TAG, "[EXPERIMENTO][FIM] total_mensagens_mqtt=%d", mensagens_mqtt_publicadas);
    }

    return mensagem_id;
}



/* =========================================================
 * INICIALIZACAO DA UART
 * ========================================================= */

static void inicializar_uart(void)
{
    const uart_config_t configuracao = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(
        uart_driver_install(
            UART_PORT,
            UART_DRIVER_BUFFER,
            0,
            0,
            NULL,
            0
        )
    );

    ESP_ERROR_CHECK(
        uart_param_config(UART_PORT, &configuracao)
    );

    ESP_ERROR_CHECK(
        uart_set_pin(
            UART_PORT,
            UART_TX_PIN,
            UART_RX_PIN,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE
        )
    );
}


/* =========================================================
 * IDENTIFICACAO DO DISPOSITIVO
 * ========================================================= */

static void obter_mac_dispositivo(void)
{
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    snprintf(
        mac_dispositivo,
        sizeof(mac_dispositivo),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    );
}


/* =========================================================
 * VALIDACAO DE ENDERECO MAC
 * ========================================================= */

static bool mac_valido(const char *mac)
{
    if (mac == NULL || strlen(mac) != 17)
    {
        return false;
    }

    for (int i = 0; i < 17; i++)
    {
        if ((i + 1) % 3 == 0)
        {
            if (mac[i] != ':')
            {
                return false;
            }

            continue;
        }

        bool digito = mac[i] >= '0' && mac[i] <= '9';
        bool maiuscula = mac[i] >= 'A' && mac[i] <= 'F';
        bool minuscula = mac[i] >= 'a' && mac[i] <= 'f';

        if (!digito && !maiuscula && !minuscula)
        {
            return false;
        }
    }

    return strcmp(mac, "00:00:00:00:00:00") != 0 &&
           strcmp(mac, "FF:FF:FF:FF:FF:FF") != 0;
}


/* =========================================================
 * EXTRACAO DE CAMPO STRING DO JSON
 * ========================================================= */

static bool extrair_campo_json(
    const char *json,
    const char *chave,
    char *destino,
    size_t tamanho_destino
)
{
    if (json == NULL || chave == NULL ||
        destino == NULL || tamanho_destino == 0)
    {
        return false;
    }

    char busca[64];

    int tamanho = snprintf(
        busca,
        sizeof(busca),
        "\"%s\":\"",
        chave
    );

    if (tamanho < 0 || (size_t)tamanho >= sizeof(busca))
    {
        return false;
    }

    const char *inicio = strstr(json, busca);

    if (inicio == NULL)
    {
        return false;
    }

    inicio += strlen(busca);

    const char *fim = strchr(inicio, '"');

    if (fim == NULL)
    {
        return false;
    }

    size_t tamanho_campo = (size_t)(fim - inicio);

    if (tamanho_campo >= tamanho_destino)
    {
        return false;
    }

    memcpy(destino, inicio, tamanho_campo);
    destino[tamanho_campo] = '\0';

    return true;
}


/* =========================================================
 * VALIDACAO DOS DADOS RECEBIDOS
 * ========================================================= */

static bool validar_json_wifi(const char *json)
{
    char mac_cliente[32];
    char mac_bssid[32];

    if (!extrair_campo_json(
            json, "cliente", mac_cliente, sizeof(mac_cliente)
        ))
    {
        return false;
    }

    if (!extrair_campo_json(
            json, "bssid", mac_bssid, sizeof(mac_bssid)
        ))
    {
        return false;
    }

    return mac_valido(mac_cliente) &&
           mac_valido(mac_bssid);
}


/* =========================================================
 * PUBLICACAO DOS DADOS VIA MQTT
 * ========================================================= */

static void publicar_dados_wifi(char *json)
{
    if (cliente_mqtt == NULL)
    {
        ESP_LOGE(TAG, "[EXPERIMENTO][MQTT_NAO_CONECTADO] mensagem não publicada");
        return;
    }

    if (!validar_json_wifi(json))
    {
        ESP_LOGW(TAG, "[EXPERIMENTO][JSON_REJEITADO] mensagem recebida pela UART não passou na validação");
        return;
    }

    size_t tamanho = strlen(json);

    if (tamanho < 2 || json[tamanho - 1] != '}')
    {
        return;
    }

    /* Remove a chave final para acrescentar o MAC do gateway. */
    json[tamanho - 1] = '\0';

    char mensagem[MQTT_MESSAGE_SIZE];

    int tamanho_escrito = snprintf(
        mensagem,
        sizeof(mensagem),
        "%s,\"macAddress\":\"%s\"}",
        json,
        mac_dispositivo
    );

    if (tamanho_escrito < 0 ||
        (size_t)tamanho_escrito >= sizeof(mensagem))
    {
        return;
    }

    publicar_mensagem_experimental(cliente_mqtt, MQTT_TOPIC, mensagem, 0);
}


/* =========================================================
 * PROCESSAMENTO DE CADA CARACTERE DA UART
 * ========================================================= */

static void processar_caractere_uart(
    char caractere,
    char *buffer_json,
    size_t *indice_json,
    int *contagem_chaves,
    bool *recebendo_json
)
{
    if (mensagens_mqtt_publicadas >= TOTAL_MENSAGENS_TESTE)
        return;

    if (caractere == '\r')
    {
        return;
    }

    /* Inicia a captura ao encontrar a primeira chave. */
    if (caractere == '{' && !(*recebendo_json))
    {
        *indice_json = 0;
        *contagem_chaves = 0;
        *recebendo_json = true;
    }

    if (!(*recebendo_json))
    {
        return;
    }

    /* Descarta mensagens maiores que o buffer. */
    if (*indice_json >= JSON_BUFFER_SIZE - 1)
    {
        *indice_json = 0;
        *contagem_chaves = 0;
        *recebendo_json = false;
        return;
    }

    buffer_json[(*indice_json)++] = caractere;

    if (caractere == '{')
    {
        (*contagem_chaves)++;
    }
    else if (caractere == '}')
    {
        (*contagem_chaves)--;

        if (*contagem_chaves != 0)
        {
            return;
        }

        /* JSON completo: finaliza e publica. */
        buffer_json[*indice_json] = '\0';
        ESP_LOGI(TAG, "[EXPERIMENTO][UART_RECEBIDO] bytes=%u payload=%s",
                 (unsigned int)*indice_json, buffer_json);
        publicar_dados_wifi(buffer_json);

        *indice_json = 0;
        *contagem_chaves = 0;
        *recebendo_json = false;
    }
}


/* =========================================================
 * RECEPCAO DOS JSONS PELA UART
 * ========================================================= */

static void tarefa_recepcao_uart(void *parametro)
{
    (void)parametro;

    uint8_t dados_uart[UART_READ_BUFFER];
    char buffer_json[JSON_BUFFER_SIZE];

    size_t indice_json = 0;
    int contagem_chaves = 0;
    bool recebendo_json = false;

    while (mensagens_mqtt_publicadas < TOTAL_MENSAGENS_TESTE)
    {
        int tamanho = uart_read_bytes(
            UART_PORT,
            dados_uart,
            sizeof(dados_uart),
            pdMS_TO_TICKS(100)
        );

        if (tamanho <= 0)
        {
            continue;
        }

        for (int i = 0; i < tamanho; i++)
        {
            processar_caractere_uart(
                (char)dados_uart[i],
                buffer_json,
                &indice_json,
                &contagem_chaves,
                &recebendo_json
            );
        }
    }

    ESP_LOGI(TAG, "[EXPERIMENTO][TAREFA_ENCERRADA] total_mensagens_mqtt=%d", mensagens_mqtt_publicadas);
    vTaskDelete(NULL);
}


/* =========================================================
 * REGISTRO DO DISPOSITIVO NO MQTT
 * ========================================================= */

static void registrar_dispositivo(void)
{
    char mensagem[128];

    snprintf(
        mensagem,
        sizeof(mensagem),
        "{\"nome\":\"esp_32_sniffer\","
        "\"macAddress\":\"%s\"}",
        mac_dispositivo
    );

    publicar_mensagem_experimental(cliente_mqtt, MQTT_REGISTER_TOPIC, mensagem, 1);
}


/* =========================================================
 * EVENTOS MQTT
 * ========================================================= */

static void tratar_evento_mqtt(
    void *argumentos_manipulador,
    esp_event_base_t base,
    int32_t identificador_evento,
    void *dados_evento
)
{
    esp_mqtt_event_handle_t evento = dados_evento;

    switch (identificador_evento)
    {
        case MQTT_EVENT_CONNECTED:
            cliente_mqtt = evento->client;

            registrar_dispositivo();

            if (!tarefa_uart_iniciada)
            {
                if (xTaskCreate(
                        tarefa_recepcao_uart,
                        "tarefa_recepcao_uart",
                        8192,
                        NULL,
                        5,
                        NULL
                    ) == pdPASS)
                {
                    tarefa_uart_iniciada = true;
                }
            }

            break;

        case MQTT_EVENT_DISCONNECTED:
            cliente_mqtt = NULL;
            break;

        default:
            break;
    }
}


/* =========================================================
 * INICIALIZACAO MQTT
 * ========================================================= */

static void inicializar_mqtt(void)
{
    const esp_mqtt_client_config_t configuracao = {
        .broker.address.uri = MQTT_BROKER_URI
    };

    esp_mqtt_client_handle_t cliente =
        esp_mqtt_client_init(&configuracao);

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            cliente,
            ESP_EVENT_ANY_ID,
            tratar_evento_mqtt,
            NULL
        )
    );

    ESP_ERROR_CHECK(esp_mqtt_client_start(cliente));
}


/* =========================================================
 * EVENTOS WIFI
 * ========================================================= */

static void tratar_evento_wifi(
    void *argumento,
    esp_event_base_t base_evento,
    int32_t identificador_evento,
    void *dados_evento
)
{
    if (base_evento == WIFI_EVENT)
    {
        if (identificador_evento == WIFI_EVENT_STA_START ||
            identificador_evento == WIFI_EVENT_STA_DISCONNECTED)
        {
            esp_wifi_connect();
        }

        return;
    }

    if (base_evento == IP_EVENT &&
        identificador_evento == IP_EVENT_STA_GOT_IP)
    {
        inicializar_mqtt();
    }
}


/* =========================================================
 * INICIALIZACAO WIFI
 * ========================================================= */

static void inicializar_wifi(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t configuracao_inicial =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&configuracao_inicial));

    const wifi_config_t configuracao_wifi = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        }
    };

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            tratar_evento_wifi,
            NULL,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            tratar_evento_wifi,
            NULL,
            NULL
        )
    );

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(WIFI_IF_STA, &configuracao_wifi)
    );

    ESP_ERROR_CHECK(esp_wifi_start());
}


/* =========================================================
 * PONTO DE ENTRADA
 * ========================================================= */

void app_main(void)
{
    ESP_LOGI(TAG, "[EXPERIMENTO][INICIO] inicializando receptor serial");
    obter_mac_dispositivo();
    ESP_LOGI(TAG, "[EXPERIMENTO][DISPOSITIVO] mac=%s", mac_dispositivo);
    inicializar_uart();
    inicializar_wifi();
}
