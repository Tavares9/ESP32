// Bibliotecas
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Firebase_ESP_Client.h>
#include <ThingSpeak.h>
#include <time.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Fornece as informações do processo de geração de token.
#include "addons/TokenHelper.h"
// Fornece informações de impressão de carga útil RTDB e outras funções auxiliares.
#include "addons/RTDBHelper.h"

// Pinos utilizados
#define ANEMOMETER_PIN 26
#define PLUVIOMETER_PIN 25
#define SOLAR_CURRENT_PIN 39
#define WIND_VANE_PIN 35

// Configuração do ThingSpeak
unsigned long myChannelNumber = 1;

// Define Firebase objetos
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

// Cliente WiFi
WiFiClient client;

// Variável para salvar identificação
String uid;

// Criar um objeto para o sensor BME
Adafruit_BME280 bme; // BME280 conectar no I2C do ESP32 (GPIO 21 = SDA, GPIO 22 = SCL)

// Variáveis para armazenar dados do sensor
float temperatureC = 0;
float humidity = 0;
float pressure = 0;
float precipitation = 0;
float current = 0;

// Path principal da base de dados
String databasePath;

// Vetores armazenamento data
char timeSecond[3];
char timeMinute[3];
char timeHour[3];
char timeDay[3];
char timeMon[3];
char timeYear[5];

// Paths Firebase
String secPath = "/Segundos";
String minPath = "/Minutos";
String hourPath = "/Horas";
String dayPath = "/Dia";
String monPath = "/Mês";
String yearPath = "/Ano";

String tempPath = "/Temperature";
String humPath = "/Humidity";
String presPath = "/Pressure";
String timePath = "/Timestamp";
String dirVentoPath = "/Direção do vento";
String pluviometroPath = "/Pulso pluviomentro";
String chuvaPath = "/mm de chuva";
String velVentoPath = "/Velocidade do vento";

// Atualização Firebase
String parentPath;

// Variável data
int timestamp;

// Variáveis ciclo
unsigned long sendDataPrevMillis = 0;
unsigned long sendDataPrevMillisTh = 0;
unsigned long timerDelayFire = 590000; // ciclo Firebase a cada 10min
unsigned long timerDelay = 290000;     // ciclo ThingSpeak a cada 5min

// Configuração fuso horário
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = -3600 * 3;

// Parâmetros Pluviômetro
int val = 1;        // valor atual
int old_val = 0;    // valor anterior
int pluviCount = 0; // contador pluviometro

// Parâmetros Anemômetro
// Constants definitions
const float pi = 3.14159265; // Numero pi
int period = 5000;           // Tempo de medida 5s(miliseconds)
int delaytime = 2000;        // Tempo entre os samples 2s(miliseconds)
int radius = 147;            // Aqui ajusta o raio do anemometro em milimetros

// Variable definitions
unsigned int Sample = 0;
unsigned int counter = 0; // contador anemo
unsigned int RPM = 0;     // rotações por min
float speedwind = 0;      // velocidade do vento (m/s)
float windspeed = 0;      // velocidade do vento (km/h)

// Direção do vento
int ar = 0;
int wd = 0;
int wds = 0;
int wdir;

int contador = 0;

float temperatureCFire;
float humidityFire;
float pressureFire;
float precipitationFire;
float speedwindFire;
int wdirFire;
float currentFire;

// monitorar data
short dia_ant = 0;
short dia_at = 0;

// Monitorar dia
short previousDay = 0;
short currentDay = 0;

// Parâmetros sensor de corrente
int mVperAmp = 66;
int Watt = 0;
double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;
float vetCorrente[1000];

// Função para iniciar BME
void initBME()
{
  if (!bme.begin(0x76))
  {
    Serial.println("Sensor BME280 não encontrado!");
  }
}

// Função para ler data
/*unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
  }
  currentDay = timeinfo.tm_mday;

  time(&now);
  return now;
}*/

void IRAM_ATTR readPluviometer()
{
  val = digitalRead(PLUVIOMETER_PIN);

  if ((val == LOW) && (old_val == HIGH))
  {
    pluviCount++;
    precipitation = pluviCount * 0.25;
    old_val = val;
    Serial.print("Rain measurement (count): ");
    Serial.print(pluviCount);
    Serial.println(" pulses");
    Serial.print("Rain measurement (calculated): ");
    Serial.print(precipitation);
    Serial.println(" mm");
  }
  else
  {
    old_val = val;
  }
}

void conectWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Conectando WiFi ..");
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
}

void arduinoOTA()
{
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("SKYNET");

  // No authentication by default
  ArduinoOTA.setPassword("39456");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
}

void conectFirebase()
{
  // API key do projeto Firebase
  config.api_key = API_KEY;

  // Credenciais da rede de internet
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // URL do projeto Firebase
  config.database_url = DATABASE_URL;

  // Conexão com Firebase
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  config.token_status_callback = tokenStatusCallback; // addons/TokenHelper.h

  // Número de tentativas pra gerar token
  config.max_token_generation_retry = 5;

  // Iniciar biblioteca Firebase
  Firebase.begin(&config, &auth);

  // Autenticar identificação Firebase
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "")
  {
    Serial.print(".");
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/readings";
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Booting");

  pinMode(ANEMOMETER_PIN, INPUT);
  digitalWrite(ANEMOMETER_PIN, HIGH);

  pinMode(PLUVIOMETER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PLUVIOMETER_PIN), readPluviometer, RISING);

  // esp_bluedroid_disable()
  conectWifi();
  arduinoOTA();
  initBME();

  ThingSpeak.begin(client); // inicia ThingSpeak

  // Configuração para ler data
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  conectFirebase();
}

void upThingspeak()
{
  if (millis() - sendDataPrevMillisTh > timerDelay || sendDataPrevMillis == 0)
  {

    readAnemometer();
    lerSenCorrente();
    Serial.println("Fazendo upload para o ThingSpeak");

    // Get current timestamp
    timestamp = getTime();
    Serial.print("time: ");
    Serial.println(timestamp);

    // Conectar ou reconectar WiFi
    conectWifi();
    /*if (WiFi.status() != WL_CONNECTED) {
      Serial.print("Attempting to connect");
      while (WiFi.status() != WL_CONNECTED) {
         WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
      Serial.println("\nConnected.");
    }*/

    Serial.print("Medida de chuva (contagem): ");
    Serial.print(pluviCount); //*0.2794);
    Serial.println(" pulso");
    Serial.print("Medida de chuva (calculado): ");
    Serial.print(pluviCount * 0.25);
    Serial.println(" mm");

    // Leitura BME
    temperatureC = bme.readTemperature();
    Serial.print("Temperature (ºC): ");
    Serial.println(temperatureC);
    humidity = bme.readHumidity();
    Serial.print("Humidity (%): ");
    Serial.println(humidity);
    pressure = bme.readPressure() / 100.0F;
    Serial.print("Pressure (hPa): ");
    Serial.println(pressure);

    // Configuração dos gráficos do ThingSpeak
    ThingSpeak.setField(1, temperatureC);
    ThingSpeak.setField(2, humidity);
    ThingSpeak.setField(3, pressure);
    ThingSpeak.setField(4, precipitation);
    ThingSpeak.setField(5, speedwind);
    ThingSpeak.setField(6, wdir);
    ThingSpeak.setField(7, current);
    // ThingSpeak.setField(8, dia_ant);

    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    if (x == 200)
    {
      Serial.println("Canal atualizado com sucesso.");
    }
    else
    {
      Serial.println("Problema ao atualizar canal. HTTP erro código " + String(x));
    }

    temperatureCFire += temperatureC;
    humidityFire += humidity;
    pressureFire += pressure;
    currentFire += current;
    precipitationFire += precipitation;
    speedwindFire += speedwind;
    wdirFire += wdir;
    contador++;

    sendDataPrevMillisTh = millis();
  }
}

void upFirebase()
{
  // Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelayFire || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();

    Serial.print("Fazendo upload para o Firebase");

    // Get current timestamp
    timestamp = getTime();
    Serial.print("time: ");
    Serial.println(timestamp);

    parentPath = databasePath + "/" + String(timestamp);

    json.set(secPath.c_str(), String(timeSecond));
    json.set(minPath.c_str(), String(timeMinute));
    json.set(hourPath.c_str(), String(timeHour));
    json.set(dayPath.c_str(), String(timeDay));
    json.set(monPath.c_str(), String(timeMon));
    json.set(yearPath.c_str(), String(timeYear));

    json.set(tempPath.c_str(), String(temperatureCFire / contador));
    json.set(humPath.c_str(), String(humidityFire / contador));
    json.set(presPath.c_str(), String(pressureFire / contador));
    json.set(dirVentoPath.c_str(), String(wdirFire / contador));
    json.set(velVentoPath.c_str(), String(speedwindFire / contador));
    json.set(pluviometroPath.c_str(), String(precipitationFire / contador));
    json.set(chuvaPath.c_str(), String(precipitation));
    json.set(timePath, String(timestamp));
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());

    temperatureCFire = humidityFire = pressureFire = currentFire = precipitationFire = speedwindFire = wdirFire = contador = 0;
  }
}

void loop() // loop principal
{
  ArduinoOTA.handle();

  upThingspeak();
  upFirebase();

  if (previousDay != currentDay)
  {
    pluviCount = precipitation = 0;
    previousDay = currentDay;
  }
}

void lerSenCorrente()
{

  int readValue;       // Valor lido pelo sensor
  int maxValue = 0;    // Guarda maior valor
  int minValue = 4096; // Guarda menor valor
  int somaTotal = 0;

  // see if you have a new maxValue
  // record the maximum sensor value

  uint32_t start_time = millis();
  while ((millis() - start_time) < 1000) // sample for 1 Sec
  {
    readValue = analogRead(SOLAR_CURRENT_PIN);
    if (readValue < minValue)
    {
      // Guarda o menor valor
      minValue = readValue;
    }
    if (readValue > maxValue)
    {
      // Guarda o maior valor
      maxValue = readValue;
    }
  }

  // Subtract min from max
  current = readValue;
  // current = ((maxValue - minValue) * 5)/4096.0; //ESP32 ADC resolution 4096
  Serial.print("Leitura sensor corrente: ");
  Serial.println(current);
}

void readAnemometer()
{
  Sample++;
  Serial.print(Sample);
  Serial.print(": Inicio monitoramento anemômetro...");
  windvelocity();
  Serial.println("Monitoramento finalizado.");
  Serial.print("Contador: ");
  Serial.print(counter);
  Serial.print(";  RPM: ");
  RPMcalc();
  Serial.print(RPM);
  Serial.print(";  Velocidade do vento: ");

  //*****************************************************************
  // print m/s
  WindSpeed();
  Serial.print(windspeed);
  Serial.print(" [m/s] ");

  //*****************************************************************
  // print km/h
  SpeedWind();
  Serial.print(speedwind);
  Serial.print(" [km/h] ");
  Serial.println();

  // Direção
  winddir();
}

void addcount()
{
  counter++;
}

void windvelocity()
{
  speedwind = 0;
  windspeed = 0;

  counter = 0;
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), addcount, RISING);
  unsigned long millis();
  long startTime = millis();
  while (millis() < startTime + period)
  {
    // Serial.print(counter);
  }
}

// Calculate rotações por minuto (RPM)
void RPMcalc()
{
  RPM = ((counter) * 60) / (period / 1000);
}

// Calcula velocidade do vento em m/s
void WindSpeed()
{
  windspeed = ((4 * pi * radius * RPM) / 60) / 1000;
}

// Calcula velocidade do vento em km/h
void SpeedWind()
{
  speedwind = (((4 * pi * radius * RPM) / 60) / 1000) * 3.6;
}

void winddir()
{
  for (int i = 0; i < 20; i++)
  {
    wd = analogRead(WIND_VANE_PIN);
    wd = wd / 5.143;
    Serial.print(wd);
    Serial.print(" ");
    wds = wds + wd;
  }

  ar = wds / 20;

  if (ar >= 0 && ar <= 100)
  {
    wdir = 270;
  }
  if (ar >= 101 && ar <= 200)
  {
    wdir = 315;
  }
  if (ar >= 201 && ar <= 300)
  {
    wdir = 0; //
  }
  if (ar >= 301 && ar <= 400)
  {
    wdir = 45; //
  }
  if (ar >= 401 && ar <= 500)
  {
    wdir = 90;
  }
  if (ar >= 501 && ar <= 700)
  {
    wdir = 135; //
  }
  if (ar >= 701 && ar <= 769)
  {
    wdir = 180;
  }
  if (ar >= 770 && ar <= 800)
  {
    wdir = 225; //
  }

  Serial.print("Leitura Analog Media : ");
  Serial.print(ar);
  Serial.print(" - Direção : ");
  Serial.println(wdir);

  ar = 0;
  wd = 0;
  wds = 0;
}

unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return (0);
  }
  dia_at = timeinfo.tm_mday;

  Serial.print(timeinfo.tm_mday);
  Serial.println(timeinfo.tm_mon);
  Serial.println(timeinfo.tm_year);
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  strftime(timeSecond, 3, "%S", &timeinfo);
  Serial.println(timeSecond);

  strftime(timeMinute, 3, "%M", &timeinfo);
  Serial.println(timeMinute);

  strftime(timeHour, 3, "%H", &timeinfo);
  Serial.println(timeHour);

  strftime(timeDay, 3, "%d", &timeinfo);
  Serial.println(timeDay);

  strftime(timeMon, 3, "%B", &timeinfo);
  Serial.println(timeMon);

  strftime(timeYear, 5, "%Y", &timeinfo);
  Serial.println(timeYear);

  /*char date[20]={timeHour[0], timeHour[1], ':', timeMinute[0], timeMinute[1], ':', timeSecond[0], timeSecond[1], ' ', timeDay[0], timeDay[1], ' ', timeMon[0], timeMon[1], timeMon[2], ' ', timeYear[0], timeYear[1], timeYear[2], timeYear[3]};
  Serial.print("Data: ");
  for(int i = 0; i == 19; i++){
    Serial.println(date[i]);
  }
  Serial.println(".");*/
  time(&now);
  return now;
}
