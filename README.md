# Monitoramento de Temperatura de Vacinas com IoT

Projeto acadêmico de **Luana Martins Arimitsu**, do curso de Análise e Desenvolvimento de Sistemas da Universidade Presbiteriana Mackenzie, relacionado à **ODS 3 — Saúde e Bem-Estar**.

O protótipo lê a temperatura de um sensor DS18B20, aciona um buzzer quando ela fica fora da faixa definida e troca mensagens pela internet usando MQTT. A montagem e os testes desta entrega foram realizados **exclusivamente no simulador Wokwi**, sem montagem física. A proposta de hardware utiliza componentes físicos representados na simulação.

## Demonstração

- [Executar o projeto no Wokwi](https://wokwi.com/projects/481607136168156161)
- [Assistir ao vídeo de demonstração](https://youtu.be/5unmxdcgXqU)
- [Repositório](https://github.com/LuanaArimitsu/monitoramento-vacinas-iot)

## Componentes e ligações

| Componente | Quantidade | Função |
|---|---:|---|
| ESP32 DevKitC V4 | 1 | Processamento e conexão Wi-Fi |
| DS18B20 | 1 | Medição de temperatura por barramento OneWire |
| Resistor de 4,7 kΩ | 1 | Pull-up na linha de dados do sensor |
| Buzzer piezoelétrico | 1 | Alerta sonoro e teste remoto |
| Fios de conexão | Conforme o circuito | Interligação dos componentes |

| Terminal | Conexão |
|---|---|
| DS18B20 VCC | ESP32 3V3 |
| DS18B20 GND | ESP32 GND |
| DS18B20 DQ | ESP32 GPIO 15 |
| Resistor de 4,7 kΩ | Entre DQ e 3V3 |
| Buzzer positivo (terminal 2 no Wokwi) | ESP32 GPIO 25 |
| Buzzer negativo (terminal 1 no Wokwi) | ESP32 GND |

O esquema completo está em `diagram.json` e no link do Wokwi. O ESP32 é a plataforma embarcada; o Wokwi é o simulador utilizado para testes.

## Arquivos e bibliotecas

- `sketch.ino`: firmware em C++ para o ambiente Arduino no ESP32.
- `diagram.json`: componentes e conexões da simulação.
- `libraries.txt`: DallasTemperature, OneWire e PubSubClient.

`WiFi.h` pertence ao suporte Arduino para ESP32. `math.h` é utilizada para verificar valores numéricos.

## Funcionamento

1. O ESP32 inicia o sensor e a conexão Wi-Fi.
2. Solicita uma conversão de temperatura aproximadamente a cada dois segundos e aguarda a conversão de forma assíncrona.
3. Classifica a leitura como `normal`, `alerta` ou `erro_sensor`.
4. Entre **2 °C e 8 °C, inclusive**, mantém o buzzer desligado. Fora da faixa, produz um alerta intermitente. Há também sinalização para erro de leitura.
5. Com MQTT conectado, tenta publicar a leitura mais recente aproximadamente a cada cinco segundos.
6. Ao receber `TESTAR` no tópico de comando, aciona o buzzer por dois segundos e depois retoma o comportamento automático.

A faixa de 2 °C a 8 °C é uma referência de demonstração. Uma aplicação real deve seguir as especificações de conservação de cada vacina e validar o dispositivo. Este protótipo acadêmico não substitui equipamento certificado e não controla refrigeração.

## Executar no Wokwi

1. Abra o projeto pelo link acima e inicie a simulação.
2. Aguarde o console indicar `[MQTT] Conectado!`.
3. Clique no DS18B20 e altere a temperatura no controle do simulador.
4. Verifique os estados no console e o som do buzzer. Permita áudio no navegador se necessário.
5. Para reproduzir em outro projeto ESP32 no Wokwi, copie os três arquivos deste repositório para as abas correspondentes.

A rede configurada é `Wokwi-GUEST`, sem senha, exclusiva do ambiente simulado. O uso em uma placa física exige configurar a rede local, além da montagem e validação elétrica.

## Comunicação MQTT

O ESP32 utiliza Wi-Fi e MQTT sobre TCP com a biblioteca PubSubClient. O navegador utiliza MQTT sobre WebSocket seguro para acessar o mesmo broker.

| Parâmetro | Valor |
|---|---|
| Broker | `broker.hivemq.com` |
| Porta TCP do ESP32 | `1883` |
| Porta WebSocket com SSL do navegador | `8884` |
| Cliente no navegador | [HiveMQ WebSocket Client](https://www.hivemq.com/demos/websocket-client/) |
| Usuário e senha | Vazios no broker público de demonstração |
| ClientID do navegador | Curto e exclusivo, por exemplo `luaVacinas26` |

| Tópico | Uso |
|---|---|
| `mackenzie/luana/vacinas/prototipo01/telemetria` | ESP32 publica temperatura e estado |
| `mackenzie/luana/vacinas/prototipo01/comando` | ESP32 recebe `TESTAR` |
| `mackenzie/luana/vacinas/prototipo01/evento` | ESP32 publica evento de início do teste |

No cliente web, conecte com SSL marcado e assine o tópico de telemetria. Para testar remotamente, publique `TESTAR` no tópico de comando com QoS 0 e Retain desmarcado. Mantenha a temperatura normal para distinguir esse teste do alerta automático.

Exemplo ilustrativo do formato da mensagem:

```json
{"temperatura":5.19,"estado":"normal","seq":1,"origem":"simulacao","leitura_ms":4605,"envio_ms":6094}
```

`leitura_ms` e `envio_ms` são tempos desde a inicialização do ESP32 simulado. A diferença entre eles indica a idade da leitura no envio; não representa, sozinha, a latência completa até o navegador. As publicações usam QoS 0 e não são retidas. A confirmação local de publicação não garante recebimento por outro cliente.

## Testes e limitações

O vídeo demonstra alterações de temperatura, alertas, retorno ao estado normal, recebimento da telemetria e acionamento remoto. A autora realizou quatro repetições de cada tipo de medição. Os tempos de resposta devem ser obtidos dos registros da gravação e apresentados no artigo, sem confundir os dois segundos de duração do teste com o tempo de resposta.

A comunicação remota depende da internet e da disponibilidade do broker público. Os testes simulados não validam precisão física, posicionamento do sensor, comportamento térmico ou confiabilidade de uso real. As chamadas de conexão MQTT podem bloquear temporariamente o processamento em falhas de rede. Uma evolução prevê autenticação, conexão protegida no dispositivo, histórico e validação física.

## Documentação técnica

- [Documentação do Wokwi](https://docs.wokwi.com/)
- [DS18B20 no Wokwi](https://docs.wokwi.com/parts/wokwi-ds18b20)
- [Biblioteca PubSubClient](https://pubsubclient.knolleary.net/)
- [Biblioteca DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [Biblioteca OneWire](https://www.pjrc.com/teensy/td_libs_OneWire.html)
- [MQTT](https://mqtt.org/)
