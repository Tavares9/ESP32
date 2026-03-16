#include <WiFi.h>
#include "ThingSpeak.h"
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = -3600*3;

//Pin out
# define Hall_sensor 26  //Anemômetro
const int guvaPin = 36; //Sensor UV
const int REED = 25; //Pluv
const int correntSol = 35;

//***Parâmetros sensor de corrente*******************
int mVperAmp = 66;
int Watt = 0;
double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;
float result;
float vetCorrente[1000];

//***Parâmetros Anemômetro********************************************
// Constants definitions
const float pi = 3.14159265;           // Numero pi
int period = 5000;               // Tempo de medida(miliseconds)
int delaytime = 2000;             // Time between samples (miliseconds)
int radius = 147;      // Aqui ajusta o raio do anemometro em milimetros
// Variable definitions
unsigned int Sample = 0;   
unsigned int counter = 0; // contador anemo
unsigned int RPM = 0;  
float speedwind = 0;      // velocidade do vento (m/s)
float windspeed = 0;           // velocidade do vento (km/h)
short dia_ant = 0;
short dia_at = 0;
//dir
int ar = 0;
int wd = 0;
int wds = 0;

//***Parâmetros Pluviômetro********************************************
int val = 1;                    //valor atual
int old_val = 0;                //valor anterior
int REEDCOUNT = 0;              //contador pluv

const char* ssid = "IJK";   // network SSID (name) 
const char* password = "10203010";   // senha da rede

WiFiClient  client;

unsigned long myChannelNumber = 1;
const char * myWriteAPIKey = "WGS6I3OB2AOT67MH";

unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

//Variaveis das grandezas
float temperatureC;
float humidity;
float pressure;
float guvaVoltage; 
float guvaValue;
int UVindex;
float precipitacao;
float rad;
float wattSol;
int wdir;
float wattSolAnt = 1;

// Create a sensor object
Adafruit_BME280 bme; //BME280 connect to ESP32 I2C (GPIO 21 = SDA, GPIO 22 = SCL)

void setup() {
  Serial.begin(115200); 
  initBME();
 
  pinMode (REED, INPUT_PULLUP);
  digitalWrite(REED,HIGH);

  pinMode(26, INPUT);
  digitalWrite(26, HIGH);     //internall pull-up active
  
  WiFi.mode(WIFI_STA);   
  
  ThingSpeak.begin(client);  // inicia ThingSpeak

  Serial.printf("\nsetup() em core: %d", xPortGetCoreID()); //Mostra no monitor em qual core o setup() foi chamado
  
  xTaskCreatePinnedToCore(loop2, "loop2", 8192, NULL, 1, NULL, 0); //Cria a tarefa "loop2()" com prioridade 1, atribuída ao core 0
  delay(1);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

void loop2(void*z) //Atribuímos o loop2 ao core 0, com prioridade 1
{
  Serial.printf("\nloop2() em core: %d", xPortGetCoreID()); //Mostra no monitor em qual core o loop2() foi chamado
  while (1)
  {
    val = digitalRead(REED);      //leitura do Reed swtich

    if ((val == LOW) && (old_val == HIGH)){    //checa se há mudança de estado

    delay(10);                  

    REEDCOUNT = REEDCOUNT + 1;

    old_val = val;              //atribui o valor atual para a variavel de valor anterior
    Serial.print("Medida de chuva (contagem): ");
    Serial.print(REEDCOUNT);//*0.2794); 
    Serial.println(" pulso");
    Serial.print("Medida de chuva (calculado): ");
    Serial.print(REEDCOUNT*0.25); 
    Serial.println(" mm");
  } 
 
  else {
    old_val = val;
  }
  precipitacao = REEDCOUNT*0.25;
  delay(100);
  }
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    printLocalTime();
    lerAnemometro();
    lerGUVA();
    lerSenCorrente();
    upload();
  }
  if(dia_ant != dia_at){
    REEDCOUNT=0;
    dia_ant=dia_at;
  }
  delay(100);
}

void initBME(){
  if (!bme.begin(0x76)) {
    Serial.println("Sensor BME280 não encontrado!");
  }
}

void upload() {
    
    // Connect or reconnect to WiFi
    if(WiFi.status() != WL_CONNECTED){
      Serial.print("Attempting to connect");
      while(WiFi.status() != WL_CONNECTED){
        WiFi.begin(ssid, password); 
        delay(5000);     
      } 
      Serial.println("\nConnected.");
    }

    //uncomment if you want to get temperature in Fahrenheit
    /*temperatureF = 1.8 * bme.readTemperature() + 32;
    Serial.print("Temperature (ºC): ");
    Serial.println(temperatureF);*/

    Serial.print("Medida de chuva (contagem): ");
    Serial.print(REEDCOUNT);//*0.2794); 
    Serial.println(" pulso");
    Serial.print("Medida de chuva (calculado): ");
    Serial.print(REEDCOUNT*0.25); 
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
    
    ThingSpeak.setField(1, temperatureC);
    ThingSpeak.setField(2, humidity);
    ThingSpeak.setField(3, pressure);
    ThingSpeak.setField(4, UVindex);
    ThingSpeak.setField(5, result);
    ThingSpeak.setField(6, precipitacao);
    ThingSpeak.setField(7, speedwind);
    ThingSpeak.setField(8, wdir);
    
    // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
    // pieces of information in a channel.
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    if(x == 200){
      Serial.println("Channel update successful.");
    }
    else{
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }
    lastTime = millis();
}

void lerSenCorrente(){

  int readValue;                // value read from the sensor
  int maxValue = 0;             // store max value here
  int minValue = 4096;          // store min value here ESP32 ADC resolution
  int somaTotal = 0;
   
       // see if you have a new maxValue
       //record the maximum sensor value
           
   for(int i = 0; i < 1000; i++){
       vetCorrente[i] = analogRead(correntSol);
       delayMicroseconds(600);
   } 
   uint32_t start_time = millis();
   while((millis()-start_time) < 1000) //sample for 1 Sec
   {
       readValue = analogRead(correntSol);
       if (readValue < minValue) 
       {
           //record the minimum sensor value
           minValue = readValue;
       }
   }
    for(int i = 0; i < 1000; i++){
          somaTotal += vetCorrente[i];
    } 

   maxValue = somaTotal/1000;
   
   // Subtract min from max
   result = ((maxValue - minValue) * 5)/4096.0; //ESP32 ADC resolution 4096
   Serial.print("Leitura sensor corrente: ");
   Serial.println(result);
}


void lerGUVA(){
  for(int i=0; i < 5; i++){
      guvaValue += analogRead(guvaPin);
    }
    guvaValue = 2*(guvaValue/5);
    guvaVoltage = guvaValue/4095*3.3;
    Serial.print("Guva reading = ");
    Serial.print(guvaValue);
    Serial.println("");
    Serial.print("Guva voltage = ");
    Serial.print(guvaVoltage);
    Serial.println(" V");
    if (guvaValue < 10){
      UVindex = 0;
    }else if(guvaValue < 46){
      UVindex = 1;
    }else if(guvaValue < 65){
      UVindex = 2;
    }else if(guvaValue < 83){
      UVindex = 3;
    }else if(guvaValue < 103){
      UVindex = 4;
    }else if(guvaValue < 124){
      UVindex = 5;
    }else if(guvaValue < 142){
      UVindex = 6;
    }else if(guvaValue < 162){
      UVindex = 7;
    }else if(guvaValue < 180){
      UVindex = 8;
    }else if(guvaValue < 200){
      UVindex = 9;
    }else if(guvaValue < 221){
      UVindex = 10;
    }else{
      UVindex = 11;
    }
    
}

void lerAnemometro() {
  Sample++;
  Serial.print(Sample);
  Serial.print(": Start measurement...");
  windvelocity();
  Serial.println("   finished.");
  Serial.print("Counter: ");
  Serial.print(counter);
  Serial.print(";  RPM: ");
  RPMcalc();
  Serial.print(RPM);
  Serial.print(";  Wind speed: ");
  
//*****************************************************************
//print m/s  
  WindSpeed();
  Serial.print(windspeed);
  Serial.print(" [m/s] ");              
  
//*****************************************************************
//print km/h  
  SpeedWind();
  Serial.print(speedwind);
  Serial.print(" [km/h] ");  
  Serial.println();

  delay(delaytime);                        //delay between prints

  //******************************
  //Direção
  winddir();
}

void addcount(){
  counter++;
}

void windvelocity(){
  speedwind = 0;
  windspeed = 0;
  
  counter = 0;  
  attachInterrupt(digitalPinToInterrupt(26), addcount, RISING);
  unsigned long millis();       
  long startTime = millis();
  while(millis() < startTime + period) {
    //Serial.print(counter);
  }
}

void RPMcalc(){
  RPM=((counter)*60)/(period/1000);  // Calculate revolutions per minute (RPM)
}

void WindSpeed(){
  windspeed = ((4 * pi * radius * RPM)/60) / 1000;  // Calculate wind speed on m/s
 
}

void SpeedWind(){
  speedwind = (((4 * pi * radius * RPM)/60) / 1000)*3.6;  // Calculate wind speed on km/h
 
}

void winddir(){
 for (int i = 0; i < 20; i++) {
    wd = analogRead(34); //34 esp ou A0 arduino
    wd=wd/5.143;
    Serial.println(wd);
    wds=wds+wd; 
    delay(50); 
    }

    ar = wds / 20;

    if(ar >= 0 && ar <=100 )  { 
      wdir = 270;
      }
    if (ar >= 101  && ar <= 200) {
      wdir = 315;
      }
    if (ar >= 201  && ar <=300) {
      wdir = 0;//
      }
    if (ar >= 301 && ar <= 400) {
      wdir = 45; //      
      }
    if (ar >= 401 && ar <= 500){ 
      wdir = 90;                                                                                                                                                                     
      }
    if (ar >= 501 && ar <= 700) {                                                                                                                                                                                                                                       
      wdir = 135;//
     }
    if (ar >= 701 && ar <= 769) {
      wdir = 180;
     }
    if (ar >= 770 && ar <= 800 ) {                                  
      wdir= 225;//
     }
     
  Serial.print("Leitura Analog Media : ");
  Serial.print(ar);
  Serial.print(" - Direção : ");
  Serial.println(wdir);

  delay(1000);

  ar=0;
  wd=0;
  wds=0;
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
 // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
 dia_at=timeinfo.tm_mday;
 //Serial.println(timeinfo.tm_mday);
}
