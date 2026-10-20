#!/usr/bin/env python3
"""Validador offline de mensagens capturadas no experimento do TCC.

O script não substitui o teste físico do ESP32. Ele valida a estrutura dos
JSONs coletados e produz contagens reproduzíveis para a avaliação.
"""
import argparse
import json
import math
import re
import sys
from pathlib import Path

SCHEMA = json.loads(r'''{"name":"receptor_wifi","required":["type","subtype","rssi","canal","tamanho","to_ds","from_ds","direcao","cliente","bssid","timestamp","macAddress"],"numeric":["type","subtype","rssi","canal","tamanho","to_ds","from_ds","timestamp"],"mac":["cliente","bssid","macAddress"],"allowed":{"direcao":["CLIENTE_AP","AP_CLIENTE"]},"sample":{"type":2,"subtype":8,"rssi":-48,"canal":6,"tamanho":128,"to_ds":1,"from_ds":0,"direcao":"CLIENTE_AP","cliente":"AA:BB:CC:DD:EE:FF","bssid":"11:22:33:44:55:66","timestamp":123,"macAddress":"22:33:44:55:66:77"}}''')
MAC_RE = re.compile(r"^(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")


def extrair_json(linha):
    """Aceita JSON puro ou uma linha de log que contenha um objeto JSON."""
    inicio = linha.find("{")
    if inicio < 0:
        return None, "sem_json"
    try:
        objeto, _ = json.JSONDecoder().raw_decode(linha[inicio:])
    except json.JSONDecodeError as exc:
        return None, f"json_invalido: {exc.msg}"
    if not isinstance(objeto, dict):
        return None, "o payload JSON não é um objeto"
    return objeto, None


def validar_registro(dado):
    if "erro" in dado:
        return f"erro_dispositivo: {dado.get('erro')}"
    faltantes = [chave for chave in SCHEMA["required"] if chave not in dado]
    if faltantes:
        return "campos_ausentes: " + ", ".join(faltantes)

    for chave in SCHEMA.get("numeric", []):
        valor = dado[chave]
        if isinstance(valor, bool) or not isinstance(valor, (int, float)):
            return f"campo_nao_numerico: {chave}"
        if not math.isfinite(float(valor)):
            return f"campo_nao_finito: {chave}"

    for chave in SCHEMA.get("mac", []):
        valor = dado[chave]
        if not isinstance(valor, str) or not MAC_RE.fullmatch(valor):
            return f"mac_invalido: {chave}"

    for chave, valores in SCHEMA.get("allowed", {}).items():
        if dado[chave] not in valores:
            return f"valor_nao_permitido: {chave}={dado[chave]!r}"

    if SCHEMA.get("name") == "sniffer" or SCHEMA.get("name") == "receptor_wifi":
        if not 1 <= dado["canal"] <= 13:
            return "canal_fora_da_faixa_1_13"
        if dado["tamanho"] <= 0:
            return "tamanho_de_quadro_invalido"
        if not -127 <= dado["rssi"] <= 0:
            return "rssi_fora_da_faixa_esperada"

    if SCHEMA.get("name") == "ambiente":
        sensor_esperado = {
            "temperatura": "dht22",
            "umidade": "dht22",
            "qualidade_ar": "mq135",
        }.get(dado["campo"])
        if sensor_esperado is None:
            return f"campo_ambiental_desconhecido: {dado['campo']}"
        if str(dado["sensor"]).lower() != sensor_esperado:
            return f"sensor_incompativel_com_campo: {dado['campo']}"

    return None


def executar_autoteste():
    exemplo = SCHEMA["sample"]
    assert validar_registro(exemplo) is None, "exemplo válido foi rejeitado"
    incompleto = dict(exemplo)
    incompleto.pop(SCHEMA["required"][0])
    assert validar_registro(incompleto) is not None, "campo ausente não foi detectado"
    assert validar_registro({"erro": "falha simulada"}) is not None
    print("AUTOTESTE OK: regras de validação verificadas.")
    print("Observação: isso testa o validador Python, não o hardware nem o firmware.")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Valida mensagens JSON do experimento.")
    parser.add_argument("--arquivo", type=Path, help="Arquivo JSONL capturado (uma mensagem por linha).")
    parser.add_argument("--saida", type=Path, help="Caminho opcional para salvar o resumo em JSON.")
    parser.add_argument("--autoteste", action="store_true", help="Testa as regras locais sem hardware.")
    args = parser.parse_args()

    if args.autoteste:
        return executar_autoteste()
    if not args.arquivo:
        parser.error("informe --arquivo CAMINHO ou use --autoteste")
    if not args.arquivo.is_file():
        parser.error(f"arquivo não encontrado: {args.arquivo}")

    resumo = {
        "schema": SCHEMA["name"],
        "arquivo": str(args.arquivo),
        "linhas_lidas": 0,
        "linhas_ignoradas_sem_json": 0,
        "mensagens_json": 0,
        "validas": 0,
        "invalidas": 0,
        "erros_dispositivo": 0,
        "detalhes_invalidos": [],
    }

    with args.arquivo.open("r", encoding="utf-8", errors="replace") as arquivo:
        for numero, linha in enumerate(arquivo, start=1):
            resumo["linhas_lidas"] += 1
            dado, erro_parse = extrair_json(linha)
            if erro_parse == "sem_json":
                resumo["linhas_ignoradas_sem_json"] += 1
                continue
            resumo["mensagens_json"] += 1
            if erro_parse:
                resumo["invalidas"] += 1
                resumo["detalhes_invalidos"].append({"linha": numero, "motivo": erro_parse})
                continue
            erro_validacao = validar_registro(dado)
            if erro_validacao is None:
                resumo["validas"] += 1
            elif erro_validacao.startswith("erro_dispositivo:"):
                resumo["erros_dispositivo"] += 1
                resumo["detalhes_invalidos"].append({"linha": numero, "motivo": erro_validacao})
            else:
                resumo["invalidas"] += 1
                resumo["detalhes_invalidos"].append({"linha": numero, "motivo": erro_validacao})

    avaliadas = resumo["mensagens_json"]
    resumo["taxa_validas_percentual"] = round(100 * resumo["validas"] / avaliadas, 2) if avaliadas else 0.0

    texto = json.dumps(resumo, ensure_ascii=False, indent=2)
    print(texto)
    if args.saida:
        args.saida.parent.mkdir(parents=True, exist_ok=True)
        args.saida.write_text(texto + "\n", encoding="utf-8")

    return 1 if resumo["invalidas"] or resumo["erros_dispositivo"] or avaliadas == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
