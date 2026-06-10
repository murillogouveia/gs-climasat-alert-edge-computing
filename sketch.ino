#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ClimaSat Alert
// Sistema de alerta climático

// Sensor DHT22
#define DHT_PIN 6
#define DHT_TYPE DHT22

// Sensor ultrassônico
#define TRIG_PIN 13
#define ECHO_PIN 12 

// Botão de emergência
#define SOS_BUTTON 2

// LEDs
#define LED_GREEN 5
#define LED_YELLOW 4
#define LED_RED 3

// Buzzer no pino D7
#define BUZZER 7

// Potenciômetro simulando dado do satélite
#define SATELLITE_PIN A0

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);

// Leituras dos sensores
float temperatura = 0;
float umidade = 0;
float distanciaAgua = 0;

int dadoSatelite = 0;
int satelitePercentual = 0;

// Estados do sistema
bool alertaSatelite = false;
bool atencaoSatelite = false;
bool riscoEnchente = false;
bool riscoClimatico = false;
bool sosAtivado = false;

String statusSistema = "NORMAL";
String tipoAlerta = "SEM RISCO";

// Controle de tempo com millis()
unsigned long tempoAtual = 0;
unsigned long ultimoSensor = 0;
unsigned long ultimoLCD = 0;
unsigned long ultimoJSON = 0;
unsigned long ultimoBuzzer = 0;

const unsigned long intervaloSensor = 1000;
const unsigned long intervaloLCD = 1000;
const unsigned long intervaloJSON = 2000;
const unsigned long intervaloBuzzer = 400;

bool buzzerLigado = false;

void setup() {
  Serial.begin(9600);

  dht.begin();

  lcd.init();
  lcd.backlight();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(SOS_BUTTON, INPUT_PULLUP);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  pinMode(BUZZER, OUTPUT);

  telaInicial();
}

void loop() {
  tempoAtual = millis();

  // O botão SOS é lido sempre, sem travar o sistema
  sosAtivado = digitalRead(SOS_BUTTON) == LOW;

  if (tempoAtual - ultimoSensor >= intervaloSensor) {
    ultimoSensor = tempoAtual;

    lerSensores();
    analisarDados();
    atualizarAlertas();
    controlarLEDs();
  }

  if (tempoAtual - ultimoLCD >= intervaloLCD) {
    ultimoLCD = tempoAtual;
    mostrarLCD();
  }

  if (tempoAtual - ultimoJSON >= intervaloJSON) {
    ultimoJSON = tempoAtual;
    enviarJSON();
  }

  controlarBuzzer();
}

// ===============================
// Tela inicial
// ===============================
void telaInicial() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ClimaSat Alert");
  lcd.setCursor(0, 1);
  lcd.print("Sistema IoT");
}

// ===============================
// Leitura dos sensores
// ===============================
void lerSensores() {
  temperatura = dht.readTemperature();
  umidade = dht.readHumidity();

  if (isnan(temperatura) || isnan(umidade)) {
    temperatura = 0;
    umidade = 0;
  }

  distanciaAgua = medirDistancia();

  dadoSatelite = analogRead(SATELLITE_PIN);

  // Converte a leitura do potenciômetro de 0-1023 para 0-100%
  satelitePercentual = map(dadoSatelite, 0, 1023, 0, 100);
}

// ===============================
// Medição do HC-SR04
// ===============================
float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracao = pulseIn(ECHO_PIN, HIGH, 30000);

  float distancia = duracao * 0.034 / 2;

  if (distancia <= 0) {
    distancia = 999;
  }

  return distancia;
}

// ===============================
// Análise dos dados
// ===============================
void analisarDados() {
  alertaSatelite = false;
  atencaoSatelite = false;
  riscoEnchente = false;
  riscoClimatico = false;

  // Satélite simulado em porcentagem:
  // 0% a 39% = normal
  // 40% a 69% = atenção
  // 70% a 100% = risco detectado
  if (satelitePercentual >= 70) {
    alertaSatelite = true;
  } else if (satelitePercentual >= 40) {
    atencaoSatelite = true;
  }

  // Quanto menor a distância, maior o nível da água
  if (distanciaAgua < 15) {
    riscoEnchente = true;
  }

  // Temperatura alta + umidade baixa = risco climático
  if (temperatura >= 38 && umidade <= 35) {
    riscoClimatico = true;
  }
}

// ===============================
// Atualização do status
// ===============================
void atualizarAlertas() {
  statusSistema = "NORMAL";
  tipoAlerta = "SEM RISCO";

  if (sosAtivado) {
    statusSistema = "CRITICO";
    tipoAlerta = "SOS MANUAL";
  }

  else if (alertaSatelite && riscoEnchente) {
    statusSistema = "CRITICO";
    tipoAlerta = "ENCHENTE";
  }

  else if (alertaSatelite && riscoClimatico) {
    statusSistema = "CRITICO";
    tipoAlerta = "CLIMA EXTREMO";
  }

  else if (alertaSatelite && !riscoEnchente && !riscoClimatico) {
    statusSistema = "ATENCAO";
    tipoAlerta = "SAT NAO CONF.";
  }

  else if (
    atencaoSatelite ||
    distanciaAgua < 25 ||
    temperatura >= 32 ||
    umidade <= 45
  ) {
    statusSistema = "ATENCAO";
    tipoAlerta = "MONITORANDO";
  }

  else {
    statusSistema = "NORMAL";
    tipoAlerta = "SEM RISCO";
  }
}

// ===============================
// Controle dos LEDs
// ===============================
void controlarLEDs() {
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  if (statusSistema == "NORMAL") {
    digitalWrite(LED_GREEN, HIGH);
  }

  else if (statusSistema == "ATENCAO") {
    digitalWrite(LED_YELLOW, HIGH);
  }

  else if (statusSistema == "CRITICO") {
    digitalWrite(LED_RED, HIGH);
  }
}

// ===============================
// Controle do buzzer sem delay()
// ===============================
void controlarBuzzer() {
  if (statusSistema == "CRITICO") {
    if (tempoAtual - ultimoBuzzer >= intervaloBuzzer) {
      ultimoBuzzer = tempoAtual;

      buzzerLigado = !buzzerLigado;

      if (buzzerLigado) {
        tone(BUZZER, 1000);
      } else {
        noTone(BUZZER);
      }
    }
  } else {
    noTone(BUZZER);
    buzzerLigado = false;
  }
}

// ===============================
// LCD
// ===============================
void mostrarLCD() {
  lcd.clear();

  if (statusSistema == "NORMAL") {
    lcd.setCursor(0, 0);
    lcd.print("STATUS NORMAL");

    lcd.setCursor(0, 1);
    lcd.print("SAT:");
    lcd.print(satelitePercentual);
    lcd.print("%");
  }

  else if (statusSistema == "ATENCAO") {
    if (tipoAlerta == "SAT NAO CONF.") {
      lcd.setCursor(0, 0);
      lcd.print("RISCO SATELITE");

      lcd.setCursor(0, 1);
      lcd.print("NAO CONFIRMADO");
    } else {
      lcd.setCursor(0, 0);
      lcd.print("STATUS ATENCAO");

      lcd.setCursor(0, 1);
      lcd.print("MONITORANDO");
    }
  }

  else if (statusSistema == "CRITICO") {
    lcd.setCursor(0, 0);
    lcd.print("RISCO CONFIRM.");

    lcd.setCursor(0, 1);

    if (tipoAlerta == "ENCHENTE") {
      lcd.print("ALERTA ENCHENTE");
    }

    else if (tipoAlerta == "CLIMA EXTREMO") {
      lcd.print("ALERTA CLIMA");
    }

    else if (tipoAlerta == "SOS MANUAL") {
      lcd.print("SOS ACIONADO");
    }
  }
}

// ===============================
// Envio em JSON com ArduinoJson
// ===============================
void enviarJSON() {
  StaticJsonDocument<256> doc;

  doc["projeto"] = "ClimaSat Alert";
  doc["temperatura"] = temperatura;
  doc["umidade"] = umidade;
  doc["distanciaAgua"] = distanciaAgua;
  doc["satelitePercentual"] = satelitePercentual;
  doc["status"] = statusSistema;
  doc["alerta"] = tipoAlerta;
  doc["sos"] = sosAtivado;

  serializeJsonPretty(doc, Serial);
  Serial.println();
  Serial.println("------------------------------");
}