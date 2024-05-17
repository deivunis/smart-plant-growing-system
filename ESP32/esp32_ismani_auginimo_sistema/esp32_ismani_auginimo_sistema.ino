#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <time.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <sunset.h>

//marsutizatoriaus prisijungimai
#define wifi_ssid "REDACTED"
#define wifi_password "REDACTED"

//mqtt serverio prisijungimai
#define mqtt_server "REDACTED"
#define mqtt_user "REDACTED"
#define mqtt_password "REDACTED"

//mqtt topics
#define humidity_topic "sensor/ias/humidity"
#define temperature_topic "sensor/ias/temperature"
#define tvoc_topic "sensor/ias/tvoc"
#define eco2_topic "sensor/ias/eco2"
#define pressure_topic "sensor/ias/pressure"
#define altitude_topic "sensor/ias/altitude"
#define light_topic "sensor/ias/light"
#define tds_topic "sensor/ias/tds"
#define water_tank_topic "sensor/ias/watertank"
#define soil_moisture_topic "sensor/ias/soilmoisture"
#define battery_level_topic "sensor/ias/batterylevel"
#define test_topic "sensor/ias/test"
#define plant_type_topic "module/ias/planttype"
#define water_pump_topic "module/ias/waterpump"
#define water_pump_availability "module/ias/waterpump/availability"
#define water_pump_state "module/ias/waterpump/state"
#define min_moisture_topic "info/ias/minmoisture"
#define max_moisture_topic "info/ias/maxmoisture"
#define min_lux_topic "info/ias/minlux"
#define max_lux_topic "info/ias/maxlux"
#define max_tds_topic "info/ias/maxtds"
#define min_light_hours_topic "info/ias/minlighthours"
#define min_temp_topic "info/ias/mintemp"
#define max_temp_topic "info/ias/maxtemp"
#define min_air_hum_topic "info/ias/minairhum"
#define max_air_hum_topic "info/ias/maxairhum"
#define min_co2_topic "info/ias/minco2"
#define max_co2_topic "info/ias/maxco2"
#define sleep_topic "ias/sleep"
#define led_topic "module/ias/led"
#define led_availability "module/ias/led/availability"
#define led_state "module/ias/led/state"
#define sunrise_topic "info/ias/sunrise"
#define sunset_topic "info/ias/sunset"
#define current_time_topic "info/ias/currenttime"
#define plant_light_time "info/ias/plantlighttime"
#define do_plant_need_light "info/ias/do_plant_need_light"
#define led_start_time_in_minutes "info/ias/ledstarttimeinminutes"
#define lux_above_threshold_in_seconds "info/ias/luxabovethresholdinseconds"
#define remaining_time_in_minutes "info/ias/remainingtimeinminutes"
#define last_watering_time_in_minutes "info/ias/lastwateringtimeinminutes"
#define update_interval_in_seconds "info/ias/updateintervalinseconds"


//dht22
#define DHTTYPE DHT22
#define DHTPIN 32

//TDS
#define TDSPIN 33
#define VREF 3.3 //esp32 veikia 3.3 itampoje
#define SCOUNT 30 //samples kiekis naudojamas medianos filtro algoritmui

//Water Level Sensor
#define WTRLVLPIN 34

//Soil Moisture Sensor
#define SMSPIN 35

//Batery level
#define BTRLVLPIN 39

//Water pump
#define PUMPPIN 4

//Time things
int TIMEZONE = 3;
#define LATITUDE    54.6872 // Vilnius latitude
#define LONGITUDE   25.2797 // Vilnius longitude

WiFiClient espClient;
PubSubClient client(espClient);
SunSet sun;

//dht22 inicializacija
DHT dht(DHTPIN, DHTTYPE, 22);

//sgp30 inicializacija
Adafruit_SGP30 sgp;

//bme280 inicializacija
#define SEALEVELPRESSURE_HPA (1013.25) //juros lygio slegis
#define pressure_offset 3.3
Adafruit_BME280 bme; //IC2

//bh1750 inicializacija
BH1750 bh;

//globalus kintamieji
unsigned long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 0.01;
float pressure = 0.0;
float altitude = 0.0;
float lux = 0.0;
//tds kintamieji
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
float averageVoltage = 0;
float tdsValue = 0;
int waterValue = 0;
int smsValue = 0;
float moisturePercentage = 0.0;
String waterTank = "UNDEFINED";
int waterLevelAbove = 0;
int waterLevelBelow = 0;
bool waterLevelStable = false;
float batteryLevel = 0.0;
//laistymo sistemos kintamieji
int lastWateringTime = 0;
int wateringInterval = 30; 
String plantType = "default";
bool waterPump = false;
bool manualWatering = false;
//augalu aplinkos parametru ribos (inicializacija su defaultinem reiksmem)
int minMoisture = 30, maxMoisture = 60, maxTDS = 500, minLux = 200, maxLux = 10000, minLightHours = 6, minTemp = 15, maxTemp = 30, minAirHum = 20, maxAirHum = 60, minCO2 = 200, maxCO2 = 1000;
int luxAboveThresholdDuration = 0;
bool plantNeedsLight = false;
bool isLEDOn = false;
int remainingTime = 0;
int currentTimeInMinutes = 0;
unsigned long sleepDuration = 0;
unsigned long sleepStartTime = 0;
unsigned long ledStartTime = 0;
unsigned long updateInterval = 2;
int hour, minutes, second, day, month, year;
String time_str, sunriseStr, sunsetStr, plantLightTimeStr;

void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    //inicializuojamas prisijungimas, nurodant irenginio pavadinimas, useri ir pass
    if (client.connect("ESP32_IAS", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(plant_type_topic);
      client.subscribe(water_pump_topic);
      client.subscribe(sleep_topic);
      client.subscribe(led_topic);
      client.subscribe(led_start_time_in_minutes);
      client.subscribe(lux_above_threshold_in_seconds);
      client.subscribe(remaining_time_in_minutes);
      client.subscribe(do_plant_need_light);
      client.subscribe(last_watering_time_in_minutes);
      client.subscribe(min_moisture_topic);
      client.subscribe(max_moisture_topic);
      client.subscribe(min_lux_topic);
      client.subscribe(max_lux_topic);
      client.subscribe(max_tds_topic);
      client.subscribe(min_light_hours_topic);
      client.subscribe(min_temp_topic);
      client.subscribe(max_temp_topic);
      client.subscribe(min_air_hum_topic);
      client.subscribe(max_air_hum_topic);
      client.subscribe(min_co2_topic);
      client.subscribe(max_co2_topic);
      client.subscribe(update_interval_in_seconds);
      //apsisaugom, kad po restarto nepradetu pumpinti vandens
      client.publish(water_pump_topic, "OFF", true);
      client.publish(water_pump_state, "OFF", true);
      digitalWrite(PUMPPIN, LOW);

      client.publish(water_pump_availability, "ONLINE", true);
      client.publish(led_availability, "ONLINE", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//MQTT temu prenumeravimas (gavimas)
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();

  //atnaujinam kintamuosius is vartotojo ivesties
  if(String(topic) == min_moisture_topic) { minMoisture = message.toInt(); }
  if(String(topic) == max_moisture_topic) { maxMoisture = message.toInt(); }
  if(String(topic) == min_lux_topic) { minLux = message.toInt(); }
  if(String(topic) == max_lux_topic) { maxLux = message.toInt(); }
  if(String(topic) == max_tds_topic) { maxTDS = message.toInt(); }
  if(String(topic) == min_light_hours_topic) { minLightHours = message.toInt(); }
  if(String(topic) == min_temp_topic) { minTemp = message.toInt(); }
  if(String(topic) == max_temp_topic) { maxTemp = message.toInt(); }
  if(String(topic) == min_air_hum_topic) { minAirHum = message.toInt(); }
  if(String(topic) == max_air_hum_topic) { maxAirHum = message.toInt(); }
  if(String(topic) == min_co2_topic) { minCO2 = message.toInt(); }
  if(String(topic) == max_co2_topic) { maxCO2 = message.toInt(); }

  //apsisaugom, kad po restartu isliktu tokie patys laikai (pasiims paskutine zinoma reiksme is mqtt)
  if(String(topic) == led_start_time_in_minutes) { ledStartTime = message.toInt(); }
  if(String(topic) == lux_above_threshold_in_seconds) { luxAboveThresholdDuration = message.toInt(); }
  if(String(topic) == remaining_time_in_minutes) { remainingTime = message.toInt(); }
  if(String(topic) == do_plant_need_light) {
    if(message == "true") {
      plantNeedsLight = true;
    } else {
      plantNeedsLight = false;
    }
  }
  if(String(topic) == update_interval_in_seconds) { updateInterval = message.toInt(); }
  
  
  //tikrinama zinute is home assistant
  if (String(topic) == plant_type_topic) {
    plantType = message;
    Serial.print("Received plant type: ");
    Serial.println(plantType);
    setPlantLevels(plantType);

    client.publish(min_moisture_topic, String(minMoisture).c_str(), true);
    client.publish(max_moisture_topic, String(maxMoisture).c_str(), true);
    client.publish(min_lux_topic, String(minLux).c_str(), true);
    client.publish(max_lux_topic, String(maxLux).c_str(), true);
    client.publish(max_tds_topic, String(maxTDS).c_str(), true);
    client.publish(min_light_hours_topic, String(minLightHours).c_str(), true);
    client.publish(min_temp_topic, String(minTemp).c_str(), true);
    client.publish(max_temp_topic, String(maxTemp).c_str(), true);
    client.publish(min_air_hum_topic, String(minAirHum).c_str(), true);
    client.publish(max_air_hum_topic, String(maxAirHum).c_str(), true);
    client.publish(min_co2_topic, String(minCO2).c_str(), true);
    client.publish(max_co2_topic, String(maxCO2).c_str(), true);
  }
  //tikrinama zinute is home assistant
  if (String(topic) == water_pump_topic) {
    if (message == "ON") {
      manualWatering = true;
      digitalWrite(PUMPPIN, HIGH);
      client.publish(water_pump_state, "ON", true);
      Serial.println("Watering plant....");
    } else if (message == "OFF") {
      manualWatering = false;
      digitalWrite(PUMPPIN, LOW);
      client.publish(water_pump_state, "OFF", true);
      Serial.println("Not watering plant....");
    }
  }
  if (String(topic) == led_topic) {
    if (message == "ON") {
      client.publish(led_state, "ON", true);
      isLEDOn = true;
      Serial.println("LED is on....");
    } else if (message == "OFF") {
      client.publish(led_state, "OFF", true);
      isLEDOn = false;
      Serial.println("LED is off....");
    }
  }
  if (String(topic) == sleep_topic) {
    String duration_str = message.substring(message.indexOf(',') + 1);
    sleepDuration = duration_str.toInt();
    message = message.substring(0, message.indexOf(','));
    if (message == "deep") {
      Serial.print("Entering deep sleep for: ");
      Serial.println(sleepDuration);
      esp_sleep_enable_timer_wakeup(sleepDuration * 1000000);
      esp_deep_sleep_start();
    } else if (message == "light") {
      Serial.print("Entering light sleep for: ");
      Serial.println(sleepDuration);
      esp_sleep_enable_timer_wakeup(sleepDuration * 1000000);
      esp_light_sleep_start();
    } else if (message == "modem") {
      Serial.print("Entering modem sleep for: ");
      Serial.println(sleepDuration);
      WiFi.mode(WIFI_OFF);
      btStop();
      sleepStartTime = millis();
    } else {
      Serial.println("Invalid sleep mode!");
    }
  }
}

//pokycio tikrinimo funkcija
bool checkBound(float newValue, float prevValue, float maxDiff) {
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}

//absoliucos dregmes apskaiciavimas
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

//slegio skaitymo funkcija
float read_pressure(){
  int reading = (bme.readPressure()/100.0F+pressure_offset)*10;
  return (float)reading/10;
}

//medianos filtro algoritmas
int getMedianNum(int bArray[], int iFilterLen){
  int bTab[iFilterLen];
  for (byte i = 0; i<iFilterLen; i++)
  bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0){
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

//dirvozemio dregmes skaiciavimas procentine israiska
float calculateMoisturePercentage(int sensorValue) {
  //kalibravimas
  int dryValue = 4095;
  int wetValue = 0;

  //mapinimas
  float moisture = map(sensorValue, wetValue, dryValue, 100, 0);

  //uztikrinama, kad reiksmes butu 0-100 ribose
  if (moisture < 0) {
    moisture = 0;
  } else if (moisture > 100) {
    moisture = 100;
  }
  return moisture;
}

//vandens talpykloje esancio vandens lygio sensoriaus stabilizuotos reiksmes gavimas
String checkWaterLevelStability(int sensorValue) {
  //skaiciuojama kiek is eiles kartu buvo viena ar kita reiksme
  if (waterValue > 500) {
    waterLevelAbove++;
    waterLevelBelow = 0;
  } else if (waterValue < 500) {
    waterLevelBelow++;
    waterLevelAbove = 0;
  }
  //jeigu buvo ilgiau kaip minute (30 nes kas 2 sekundes programa vykdo darba) stabilizuojama reiksme
  if (waterLevelAbove >= 30 || waterLevelBelow >= 30) {
    waterLevelStable = true;
  }
  //nustatoma vandens talpykla pilna ar tuscia
  if (waterLevelStable) {
    if (waterLevelAbove >= 30) {
      waterTank = "FULL";
    } else if (waterLevelBelow >= 30) {
      waterTank = "EMPTY";
    }
      
    //resetinama ir skaiciuojama stabili reiksme is naujo
    waterLevelAbove = 0;
    waterLevelBelow = 0;
    waterLevelStable = false;
    }
  return waterTank;
}
//Augalu parametru nustatytmas
void setPlantLevels(String plant) {
  if (plant == "flowers") {
    minMoisture = 30;
    maxMoisture = 60;
    maxTDS = 500;
    minLux = 1000;
    maxLux = 20000;
    minLightHours = 8;
    minTemp = 10;
    maxTemp = 30;
    minAirHum = 40;
    maxAirHum = 70;
    minCO2 = 250;
    maxCO2 = 800;
  } else if (plant == "trees_and_shrubs") {
    minMoisture = 20;
    maxMoisture = 60;
    maxTDS = 600;
    minLux = 500;
    maxLux = 40000;
    minLightHours = 6;
    minTemp = -10;
    maxTemp = 35;
    minAirHum = 30;
    maxAirHum = 60;
    minCO2 = 250;
    maxCO2 = 1200;
  } else if (plant == "succulents_and_cacti") {
    minMoisture = 10;
    maxMoisture = 30;
    maxTDS = 1000;
    minLux = 2000;
    maxLux = 100000;
    minLightHours = 8;
    minTemp = 5;
    maxTemp = 35;
    minAirHum = 20;
    maxAirHum = 50;
    minCO2 = 200;
    maxCO2 = 800;
  } else if (plant == "vegetables_and_fruits") {
    minMoisture = 41;
    maxMoisture = 80;
    maxTDS = 800;
    minLux = 3000;
    maxLux = 80000;
    minLightHours = 8;
    minTemp = 10;
    maxTemp = 35;
    minAirHum = 40;
    maxAirHum = 70;
    minCO2 = 250;
    maxCO2 = 1200;
  } else if (plant == "herbs") {
    minMoisture = 40;
    maxMoisture = 70;
    maxTDS = 700;
    minLux = 2000;
    maxLux = 60000;
    minLightHours = 6;
    minTemp = 10;
    maxTemp = 30;
    minAirHum = 40;
    maxAirHum = 60;
    minCO2 = 250;
    maxCO2 = 1000;
  } else if (plant == "vines_and_climbers") {
    minMoisture = 40;
    maxMoisture = 70;
    maxTDS = 600;
    minLux = 1000;
    maxLux = 40000;
    minLightHours = 8;
    minTemp = 5;
    maxTemp = 35;
    minAirHum = 40;
    maxAirHum = 70;
    minCO2 = 350;
    maxCO2 = 1000;
  } else if (plant == "aquatics") {
    minMoisture = 90; //galima sakyti, kad visada turetu buti 100 nes vandenyje, bet jeigu kempineje augina, tuomet 100 nebus
    maxMoisture = 100;
    maxTDS = 1000;
    minLux = 500;
    maxLux = 10000;
    minLightHours = 10;
    minTemp = 15;
    maxTemp = 30;
    minAirHum = 0; //irgi neturi itakos taciau, galim istatyti i jutikliaus ribas, kad netrigerintu sistemos
    maxAirHum = 100; //irgi neturi itakos taciau, galim istatyti i jutikliaus ribas, kad netrigerintu sistemos
    minCO2 = 0; //oro co2 kiekis neturi itakos, tai istatom jutiklio ribas
    maxCO2 = 60000; //oro co2 kiekis neturi itakos
  } else if (plant == "tropicals") {
    minMoisture = 50;
    maxMoisture = 80;
    maxTDS = 800;
    minLux = 2000;
    maxLux = 50000;
    minLightHours = 10;
    minTemp = 20;
    maxTemp = 40;
    minAirHum = 50;
    maxAirHum = 80;
    minCO2 = 300;
    maxCO2 = 1200;
  } else if (plant == "bulbous") {
    minMoisture = 30;
    maxMoisture = 60;
    maxTDS = 600;
    minLux = 1000;
    maxLux = 40000;
    minLightHours = 6;
    minTemp = 5;
    maxTemp = 25;
    minAirHum = 30;
    maxAirHum = 60;
    minCO2 = 250;
    maxCO2 = 1000;
  } else {
    //default
    minMoisture = 35;
    maxMoisture = 70;
    maxTDS = 750;
    minLux = 200;
    maxLux = 15000;
    minLightHours = 6;
    minTemp = 15;
    maxTemp = 28;
    minAirHum = 35;
    maxAirHum = 75;
    minCO2 = 350;
    maxCO2 = 1000;
  }
}
//funkcija nustatyti ar reikia laistyti augala
bool shouldWaterPlant() {
  int moistureLevel = moisturePercentage;
  if(moistureLevel < minMoisture && moistureLevel > 0) {
    return true;
  } else {
    return false;
  }
}
void update_time(){
  time_t now = time(nullptr); 
  struct tm *now_tm;
  now = time(NULL);
  now_tm = localtime(&now);
  hour = now_tm->tm_hour;
  minutes = now_tm->tm_min;
  int weekday = now_tm->tm_wday;
  String week_days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  String wday = week_days[weekday];
  second = now_tm->tm_sec; 
  day    = now_tm->tm_mday;
  month  = (now_tm->tm_mon)+1;
  year   = now_tm->tm_year+1900;
  time_str = String(year)+"-"+(month<10?"0":"")+String(month)+"-"+String(day)
              +" "+wday+(day<10?" 0":" ")+(hour<10?"0":"")+String(hour)+":"+(minutes<10?"0":"")+String(minutes)+":"+(second<10?"0":"")+String(second);
  client.publish(current_time_topic, String(time_str).c_str(), true);
  currentTimeInMinutes = hour * 60 + minutes;
  //Serial.println(time_str);
  if(month >= 03 && month <= 10) {TIMEZONE = 3;} else {TIMEZONE = 2;};
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  //isjungiamas BT taupyti energijai
  btStop();
  //wifi
  setup_wifi();
  //mqtt
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  //ntp
  configTime(2*3600, 3600, "pool.ntp.org");
  delay(5000); // Wait for time to start
  time_t now = time(nullptr);
  //lokacija
  sun.setPosition(LATITUDE, LONGITUDE, TIMEZONE);
  //dht22
  dht.begin();
  //sgp30
  if (!sgp.begin()) { //sgp hex adresas 0x58
    Serial.println("SGP30 sensor not found!");
    while (1);
  } else {
    Serial.print("Found SGP30 serial #");
    Serial.print(sgp.serialnumber[0], HEX);
    Serial.print(sgp.serialnumber[1], HEX);
    Serial.println(sgp.serialnumber[2], HEX);
  }
  //bme280
  bool bme_280_status;
  bme_280_status = bme.begin(0x76); //bme280 HEX adresas yra 0x76, reikalingas, kad atskirtu tarp keliu veikianciu jutikliu ant to pacio I2C
  if (!bme_280_status) {
    Serial.println("BME280 sensor not found!");
    while (1);
  }
  //bh1750
  // bool bh_1750_status;
  // bh_1750_status = bh.begin(0x23); //bh1750 hex address 0x23
    if (!bh.begin()) {
    Serial.println("BH1750 sensor not found!");
    while (1);
  }
  //TDS
  pinMode(TDSPIN, INPUT);
  //Water level sensors
  pinMode(WTRLVLPIN, INPUT_PULLUP);
  //Soil moisture sensor
  pinMode(SMSPIN, INPUT_PULLUP);
  //Batery level
  pinMode(BTRLVLPIN, INPUT_PULLUP);
  //Water pump
  pinMode(PUMPPIN, OUTPUT);
  digitalWrite(PUMPPIN, LOW);

  client.publish(plant_type_topic, String(plantType).c_str(), true);

}

void loop() {
  //mqtt bandoma prisijungti kol pavyks
  if (!client.connected() && sleepDuration == 0) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > updateInterval*1000) {
    lastMsg = now;
    static bool firstIteration = true;
    if(firstIteration) {firstIteration = false; } else{
    //time
    update_time();
    sun.setCurrentDate(year, month, day);
    sun.setTZOffset(TIMEZONE);
    double sunrise = sun.calcSunrise();
    double sunset = sun.calcSunset();
    sunriseStr = String(static_cast<int>(sunrise/60)) + ":" + String(static_cast<int>(sunrise) % 60);
    sunsetStr = String(static_cast<int>(sunset/60)) + ":" + String(static_cast<int>(sunset) % 60);
    client.publish(sunrise_topic, String(sunriseStr).c_str(), true);
    client.publish(sunset_topic, String(sunsetStr).c_str(), true);
    
    //dht22 nuskaitomos temp ir dregmes reiksmes
    float newTemp = dht.readTemperature();
    float newHum = dht.readHumidity();
    //isvedama temperatura ir dregme tik jeigu yra aptiktas diff kintamojo pokytis
    if (checkBound(newTemp, temp, diff)) {
      temp = newTemp;
      Serial.print("New temperature:");
      Serial.println(String(temp).c_str());
      client.publish(temperature_topic, String(temp).c_str(), true);
    }
    if (checkBound(newHum, hum, diff)) {
      hum = newHum;
      Serial.print("New humidity:");
      Serial.println(String(hum).c_str());
      client.publish(humidity_topic, String(hum).c_str(), true);
    }

    //sgp30 nustatoma absoliuti oro dregme is dht22
    sgp.setHumidity(getAbsoluteHumidity(newTemp, newHum));
   //nuskaitomos TVOC ir eCO2 reiksmes
    if (! sgp.IAQmeasure()) {
      Serial.println("Measurement failed");
      return;
    } else {
      Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
      Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");
      client.publish(tvoc_topic, String(sgp.TVOC).c_str(), true);
      client.publish(eco2_topic, String(sgp.eCO2).c_str(), true);
    }
    //nuskaitomos raw h2 ir etanolio reiksmes (nelabai naudojamos, daugiau del bendros informacijos)
    if (! sgp.IAQmeasureRaw()) {
     Serial.println("Raw Measurement failed");
      return;
    } else {
      Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
      Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");
    }

    //bme280
    //slegio nuskaitymas
    pressure = bme.readPressure() / 100.0F;
    Serial.print("Pressure = ");
    Serial.print(pressure);
    Serial.println(" hPa");
    client.publish(pressure_topic, String(pressure).c_str(), true);

    //apytikslis aukstis virs juros lygio
    altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
    Serial.print("Approx. Altitude = ");
    Serial.print(altitude);
    Serial.println(" m");
    client.publish(altitude_topic, String(altitude).c_str(), true);
    // Serial.print("BME280 Humidity = ");
    // Serial.print(bme.readHumidity());
    // Serial.println(" %");
    // Serial.print("BME280 Temperature = ");
    // Serial.print(bme.readTemperature());
    // Serial.println(" *C");

    lux = 1.5 * (bh.readLightLevel());
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");
    client.publish(light_topic, String(lux).c_str(), true);

    //TDS matavimas.
    analogBuffer[analogBufferIndex] = analogRead(TDSPIN);
    analogBufferIndex++;
    if(analogBufferIndex == SCOUNT) {
      //pradedam deti is naujo i buferi
      analogBufferIndex = 0;
    }
    //leidziamas ciklas per buferi, kuris susideda is 30 elementu
    for(copyIndex=0; copyIndex<SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
      //nuskaitoma analogo reiksme atlikus medianos filtra ir tuomet konvertuojama i itampa
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 4096.0;
      //temperaturos kompensavimo forumule: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
      float compensationCoefficient = 1.0+0.02*(newTemp-25.0); //newTemp reiksme is DHT22 jutiklio, skaitoma, kad oro temperatura lygi vandens;
      //temperaturos kompensavimas pagal koeficienta
      float compensationVoltage=averageVoltage/compensationCoefficient;
      //itampos konvertas i tds reiksme pagal formule
      tdsValue=(133.42*compensationVoltage*compensationVoltage*compensationVoltage - 255.86*compensationVoltage*compensationVoltage + 857.39*compensationVoltage)*0.5;
        
      //Serial.print("voltage:");
      //Serial.print(averageVoltage,2);
      //Serial.print("V   ");
      }
    Serial.print("TDS Value:");
    Serial.print(tdsValue,0);
    Serial.println("ppm");
    client.publish(tds_topic, String(tdsValue).c_str(), true);

    //Water level sensor
    waterValue = analogRead(WTRLVLPIN);
    Serial.print("Water level sensor raw value = ");
    Serial.println(waterValue);
    checkWaterLevelStability(waterValue);
    Serial.print("Water tank status: "); Serial.println(waterTank);
    client.publish(water_tank_topic, String(waterTank).c_str(), true);

    //Soil moisture sensor
    smsValue = analogRead(SMSPIN);
    Serial.print("Soil moisture sensor raw value = ");
    Serial.println(smsValue);
    moisturePercentage = calculateMoisturePercentage(smsValue);
    Serial.print("Soil Moisture Percentage: ");
    Serial.print(moisturePercentage);
    Serial.println("%");
    client.publish(soil_moisture_topic, String(moisturePercentage).c_str(), true);

    //Baterry status
    batteryLevel = map(analogRead(BTRLVLPIN), 3200, 4095, 0, 100);
    Serial.print("ESP32 Battery level: ");
    Serial.print(batteryLevel);
    Serial.println("%");
    Serial.print("Battery sensor raw value = ");
    Serial.println(analogRead(BTRLVLPIN));
    client.publish(battery_level_topic, String(batteryLevel).c_str(), true);
    //jeigu zemas baterijos lygis, ijungiamas miego rezimas iki kol vartotojas rankiniu budu perjungs valdikli
    if(batteryLevel < 5) {
      Serial.println("Low battery level! Shutting down...");
      ESP.deepSleep(0);
    }
    //atliekami skaiciavimai nusileidus saulei iki kol patekes
  //if (hour == static_cast<int>(sunset/60) && minutes == (static_cast<int>(sunset) % 60)) {
  if ((currentTimeInMinutes >= static_cast<int>(sunset) || currentTimeInMinutes <= static_cast<int>(sunrise)) && luxAboveThresholdDuration > 0) {
    if (luxAboveThresholdDuration < minLightHours * 3600) {
      plantNeedsLight = true;
      client.publish(do_plant_need_light, "true", true);
      remainingTime = (minLightHours * 60) - (luxAboveThresholdDuration/60);
      client.publish(remaining_time_in_minutes, String(remainingTime).c_str(), true);
      //if (plantNeedsLight && (1440 - (hour * 60) + (static_cast<int>(sunrise)) > remainingTime)) {
      if (remainingTime < currentTimeInMinutes + static_cast<int>(sunrise) && !isLEDOn) {
        client.publish(led_topic, "ON", true);
        client.publish(led_state, "ON", true);
        isLEDOn = true;
        ledStartTime = currentTimeInMinutes;
        client.publish(led_start_time_in_minutes, String(ledStartTime).c_str(), true);
      } else {
        plantNeedsLight = false;
        luxAboveThresholdDuration = 0;
        client.publish(do_plant_need_light, "false", true);
        client.publish(lux_above_threshold_in_seconds, String(luxAboveThresholdDuration).c_str(), true);
        Serial.println("Not enough remaining time until sunrise. Not turning on LED.");
      }
    } else if (luxAboveThresholdDuration >= minLightHours * 3600) {
      plantNeedsLight = false;
      luxAboveThresholdDuration = 0;
      isLEDOn = false;
      client.publish(do_plant_need_light, "false", true);
      client.publish(led_start_time_in_minutes, String(ledStartTime).c_str(), true);
      client.publish(lux_above_threshold_in_seconds, String(luxAboveThresholdDuration).c_str(), true);
      client.publish(led_topic, "OFF", true);
      client.publish(led_state, "OFF", true);
      Serial.println("The plant received enough light today.");
    } else {
      Serial.println("Some unknown error..");
    }
  }
  
  //praejus trukstamam laikui isjungiam sviesa
  if (ledStartTime != 0 && (currentTimeInMinutes - ledStartTime) >= remainingTime) {
    ledStartTime = 0;
    plantNeedsLight = false;
    isLEDOn = false;
    luxAboveThresholdDuration = 0;
    client.publish(led_start_time_in_minutes , String(ledStartTime).c_str(), true);
    client.publish(lux_above_threshold_in_seconds, String(luxAboveThresholdDuration).c_str(), true);
    client.publish(do_plant_need_light, "false", true);
    client.publish(led_topic, "OFF", true);
    client.publish(led_state, "OFF", true);
    Serial.println("Remaining light amount was compensated with LED. Turning off.");
  }
  //skaiciuojamas laikas kuomet yra pakankamas sviesos kiekis
  if(lux >= minLux && !plantNeedsLight) {
    luxAboveThresholdDuration += updateInterval;
    client.publish(lux_above_threshold_in_seconds, String(luxAboveThresholdDuration).c_str(), true);
  }
  plantLightTimeStr = String(luxAboveThresholdDuration/3600) + "/" + String(minLightHours) + " val.";
  client.publish(plant_light_time, String(plantLightTimeStr).c_str(), true);

  //Serial.println(sunrise);
  //Serial.println(sunset);
  }
  }


  //shouldWaterPlant();
  //vandens pramogos
  if (shouldWaterPlant()) {
    if (waterTank == "FULL" && tdsValue < maxTDS && !manualWatering && (currentTimeInMinutes - lastWateringTime >= wateringInterval)) {
      //Ijungiama vandens pompa
      Serial.println("Watering plant...");
      client.publish(water_pump_topic, "ON", true);
      client.publish(water_pump_state, "ON", true);
      digitalWrite(PUMPPIN, HIGH);
      delay(60000);
      //Isjungiama vandens pompa
      client.publish(water_pump_topic, "OFF", true);
      client.publish(water_pump_state, "OFF", true);
      digitalWrite(PUMPPIN, LOW);
      Serial.println("Watering finished.");
      
      lastWateringTime = currentTimeInMinutes;
      client.publish(last_watering_time_in_minutes, String(lastWateringTime).c_str(), true);
      manualWatering = false;
    }
  }

  //modem miego rezimas
  if (sleepDuration > 0 && (millis() - sleepStartTime) >= (sleepDuration * 1000)) {
  Serial.println("Wake up from sleep mode");
  //ijungiamas belaidis rysys (isskyrus bt, nes jo ir taip nereikia)
  setup_wifi();
  sleepDuration = 0;
  }
  
}
