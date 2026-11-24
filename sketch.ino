#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// Conexoes do circuito
const int PINO_SENSOR = 15;
const int PINO_BUZZER = 25;

// Faixa adotada para a demonstracao
const float TEMPERATURA_MINIMA = 2.0;
const float TEMPERATURA_MAXIMA = 8.0;

// Rede exclusiva do simulador Wokwi.
// Na placa fisica, configurar a rede Wi-Fi utilizada.
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_SENHA = "";

// Servidor MQTT publico para demonstracao
const char* MQTT_SERVIDOR = "broker.hivemq.com";
const int MQTT_PORTA = 1883;

// Topicos MQTT
const char* TOPICO_TEMPERATURA =
  "mackenzie/luana/vacinas/prototipo01/telemetria";

const char* TOPICO_COMANDO =
  "mackenzie/luana/vacinas/prototipo01/comando";

const char* TOPICO_EVENTO =
  "mackenzie/luana/vacinas/prototipo01/evento";

WiFiClient conexaoWiFi;
PubSubClient mqtt(conexaoWiFi);

OneWire oneWire(PINO_SENSOR);
DallasTemperature sensor(&oneWire);

String identificadorMQTT;

float temperatura = NAN;
const char* estado = "inicializando";

bool primeiraLeitura = false;
bool conversaoPendente = false;
bool testeBuzzerAtivo = false;

unsigned long inicioConversao = 0;
unsigned long ultimaSolicitacao = 0;
unsigned long ultimaPublicacao = 0;
unsigned long ultimaTentativaMQTT = 0;
unsigned long ultimaTentativaWiFi = 0;
unsigned long inicioTesteBuzzer = 0;
unsigned long instanteLeitura = 0;
unsigned long sequencia = 0;

int frequenciaAtual = 0;

// Altera o som somente quando necessario.
void definirSom(int frequencia) {
  if (frequencia == frequenciaAtual) {
    return;
  }

  frequenciaAtual = frequencia;

  if (frequencia == 0) {
    noTone(PINO_BUZZER);
  } else {
    tone(PINO_BUZZER, frequencia);
  }
}

// Executada quando chega uma mensagem no topico de comando.
void receberComando(char* topico, byte* payload,
                    unsigned int tamanho) {
  String comando;

  for (unsigned int i = 0; i < tamanho; i++) {
    comando += (char)payload[i];
  }

  comando.trim();
  comando.toUpperCase();

  Serial.print("[MQTT] Comando recebido: ");
  Serial.println(comando);

  if (comando == "TESTAR") {
    inicioTesteBuzzer = millis();
    testeBuzzerAtivo = true;

    definirSom(2000);

    Serial.printf(
      "[BUZZER] Teste acionado em %lu ms\n",
      millis()
    );

    mqtt.publish(
      TOPICO_EVENTO,
      "{\"evento\":\"teste_buzzer_iniciado\"}",
      false
    );
  }
}

// Mantem a rede e tenta restabelecer o MQTT.
// As tentativas sao espacadas; conectar pode consumir tempo.
void manterConexao() {
  unsigned long agora = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (agora - ultimaTentativaWiFi >= 15000) {
      ultimaTentativaWiFi = agora;
      Serial.println("[WiFi] Tentando reconectar...");
      WiFi.reconnect();
    }
    return;
  }

  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }

  if (agora - ultimaTentativaMQTT < 5000) {
    return;
  }

  ultimaTentativaMQTT = agora;

  Serial.println("[MQTT] Conectando ao broker...");

  if (mqtt.connect(identificadorMQTT.c_str())) {
    Serial.println("[MQTT] Conectado!");

    if (mqtt.subscribe(TOPICO_COMANDO)) {
      Serial.print("[MQTT] Inscrito em: ");
      Serial.println(TOPICO_COMANDO);
    } else {
      Serial.println("[MQTT] Falha ao assinar comandos.");
    }
  } else {
    Serial.print("[MQTT] Falha. Codigo: ");
    Serial.println(mqtt.state());
  }
}

// Solicita uma conversao sem aguardar dentro da biblioteca.
void solicitarTemperatura() {
  sensor.requestTemperatures();

  inicioConversao = millis();
  ultimaSolicitacao = inicioConversao;
  conversaoPendente = true;
}

// Le o resultado apos o tempo de conversao do sensor.
void atualizarTemperatura() {
  unsigned long agora = millis();

  if (conversaoPendente &&
      agora - inicioConversao >= 750) {
    temperatura = sensor.getTempCByIndex(0);

    instanteLeitura = millis();
    primeiraLeitura = true;
    conversaoPendente = false;

    if (!isfinite(temperatura) ||
        temperatura == DEVICE_DISCONNECTED_C ||
        temperatura < -55.0 ||
        temperatura > 125.0) {
      temperatura = NAN;
      estado = "erro_sensor";
    } else if (temperatura < TEMPERATURA_MINIMA ||
               temperatura > TEMPERATURA_MAXIMA) {
      estado = "alerta";
    } else {
      estado = "normal";
    }

    Serial.printf("[SENSOR] t=%lu ms | ", instanteLeitura);

    if (isnan(temperatura)) {
      Serial.print("Temperatura indisponivel");
    } else {
      Serial.printf("Temperatura: %.2f C", temperatura);
    }

    Serial.print(" | Estado: ");
    Serial.println(estado);
  }

  if (!conversaoPendente &&
      millis() - ultimaSolicitacao >= 2000) {
    solicitarTemperatura();
  }
}

// Controla o som sem utilizar delay para a cadencia.
void atualizarBuzzer() {
  unsigned long agora = millis();

  // O comando remoto testa o som durante dois segundos.
  if (testeBuzzerAtivo) {
    if (agora - inicioTesteBuzzer < 2000) {
      definirSom(2000);
      return;
    }

    testeBuzzerAtivo = false;
    Serial.println("[BUZZER] Teste finalizado.");
  }

  if (!primeiraLeitura) {
    definirSom(0);
    return;
  }

  if (isnan(temperatura)) {
    // Falha: 100 ms de som a cada segundo.
    definirSom((agora % 1000 < 100) ? 3000 : 0);
  } else if (temperatura < TEMPERATURA_MINIMA ||
             temperatura > TEMPERATURA_MAXIMA) {
    // Alerta: 200 ms de som e 200 ms de silencio.
    definirSom((agora % 400 < 200) ? 2000 : 0);
  } else {
    definirSom(0);
  }
}

void publicarTemperatura() {
  if (!mqtt.connected() || !primeiraLeitura) {
    return;
  }

  unsigned long agora = millis();

  if (agora - ultimaPublicacao < 5000) {
    return;
  }

  ultimaPublicacao = agora;

  char valorTemperatura[20];

  if (isnan(temperatura)) {
    snprintf(
      valorTemperatura,
      sizeof(valorTemperatura),
      "null"
    );
  } else {
    snprintf(
      valorTemperatura,
      sizeof(valorTemperatura),
      "%.2f",
      temperatura
    );
  }

  char mensagem[256];

  snprintf(
    mensagem,
    sizeof(mensagem),
    "{\"temperatura\":%s,\"estado\":\"%s\","
    "\"seq\":%lu,\"origem\":\"simulacao\","
    "\"leitura_ms\":%lu,\"envio_ms\":%lu}",
    valorTemperatura,
    estado,
    ++sequencia,
    instanteLeitura,
    agora
  );

  if (mqtt.publish(TOPICO_TEMPERATURA, mensagem, false)) {
    Serial.print("[MQTT] Publicacao enviada: ");
    Serial.println(mensagem);
  } else {
    Serial.println("[MQTT] Falha ao enviar publicacao.");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PINO_BUZZER, OUTPUT);
  digitalWrite(PINO_BUZZER, LOW);

  sensor.begin();
  sensor.setResolution(12);
  sensor.setWaitForConversion(false);

  // Valor aleatorio evita conflito entre sessoes MQTT.
  identificadorMQTT = "luana-vacinas-";
  identificadorMQTT += String(
    (uint32_t)esp_random(), HEX
  );

  mqtt.setServer(MQTT_SERVIDOR, MQTT_PORTA);
  mqtt.setCallback(receberComando);
  mqtt.setBufferSize(512);
  mqtt.setSocketTimeout(15);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA, 6);

  Serial.println("MONITORAMENTO DE TEMPERATURA DE VACINAS");
  Serial.println("Ambiente de testes: simulacao Wokwi");
  Serial.println("[WiFi] Conectando...");

  solicitarTemperatura();
}

void loop() {
  atualizarTemperatura();
  atualizarBuzzer();

  manterConexao();

  // Atualiza novamente apos possiveis esperas da rede.
  atualizarTemperatura();
  atualizarBuzzer();

  publicarTemperatura();

  delay(1);
}