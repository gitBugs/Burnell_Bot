#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>

#define DHTPIN 14          // DHT22 connected to gpio14
#define DHTTYPE DHT22     // Calls are to DHT22  type sensor not DHT11
// Initialize DHT sensor with threshold parameter of 15 as advised in https://github.com/esp8266/Arduino
DHT dht(DHTPIN, DHTTYPE, 15);

BH1750 lightMeter;
uint16_t lastReportedLux = 0;
uint16_t lux = 0;

#undef MQTT_KEEPALIVE     // redefine mqtt keepalive period in seconds, about 3 loops
#define MQTT_KEEPALIVE 60
#define Interval 1000UL  // Number of milli that each loop should take to complete
#define motionInterval 30000 // Millis between reports whether motion detector was triggered
#define motionPin 12 // Pin to which motion sensor is attached.

   //Keep track of when the next report of motion activity is due
   int nextMotionReport = millis();
   int motionDetected = 1; 

   //Switches which action comes next.
   int nextAction = 0;
   
   
// Wifi and MQTT specifics
char wifissid[] = "*****";   // Wifi noetwork name or ESSID/SSID
char wifipwd[] = "*****";   // Wifi password

char nodeprefix[] = "BurnellBot";   // Prefix for node name rest is mac address
char mqttsrvr[] = "10.3.11.253";    // Ip address of mqtt broker
char mqttuid[] = "";   // mqtt broker username
char mqttpwd[] = ""; // mqtt broker password

char topic_node[]   = "SHHNoT/commons/node/burnellBot";   // topic name for LWT node up/down notifications
char topic_temperature[] = "SHHNoT/commons/sensor/burnellBot/temperature";   // topic name for temperature readings
char topic_humidity[]    = "SHHNoT/commons/sensor/burnellBot/humidity";
char topic_light[]       = "SHHNoT/commons/sensor/burnellBot/light";
char topic_motion[]      = "SHHNoT/commons/sensor/burnellBot/motion";
String NodeName="";

// tx pacing counter
int pacing = 0;

// variable that holds the terminal millis count for a loop cycle
unsigned long nextturn; 


//Initialize mqtt and wifi
WiFiClient wifiClient;
PubSubClient client(mqttsrvr, 1883, callback, wifiClient);

// arrived message callback subroutine
void callback(char* topic, byte* payload, unsigned int length) {
   // handle message arrived
   return;
} 

// Convert 6 byte mac address to text string subroutine
String macToStr(const uint8_t* mac)
{
   String result;
   
   for (int i = 0; i < 6; ++i) {
      result += String(mac[i], 16);
   }
   
   return result;
}

// Connecto to Wifi subroutine
void WifiConnect() {
  int timout = 30;
  
  Serial.print("Wifi connecting to ");
  Serial.println(wifissid);
  WiFi.begin(wifissid, wifipwd);
  while (WiFi.status() != WL_CONNECTED) {
     delay(1000);
     Serial.print(".");
     if(--timout < 0){
        Serial.println("WIFI connect failed reset and try again...");
        abort();
     }       
  }
  Serial.println("");
  Serial.print("WiFi connected with IP ");
  Serial.println(WiFi.localIP()); 
  return;
}

// Connect to MQTT Broker subroutine
void BrokerConnect() {
   uint8_t mac[6];
   String lwtUp;
   String lwtDn; 
  
   if(NodeName.length() <= 0){
      // Generate client name and LWT messages based on MAC address and last 8 bits of microsecond counter
      WiFi.macAddress(mac);   
      NodeName = nodeprefix + macToStr(mac);
      lwtUp = NodeName + " is alive";
      lwtDn = NodeName + " is dead";
   } 
  
   Serial.print(NodeName);
   Serial.print(" connecting to ");
   Serial.print(mqttsrvr);
   Serial.print(" as ");
//   Serial.println(mqttuid);

   if (client.connect((char*)NodeName.c_str(), mqttuid, mqttpwd, topic_node, 0, 0,  (char*)lwtDn.c_str())) {
      Serial.println("Connected to MQTT broker");
      Serial.print("LWT topic is: ");
      Serial.println(topic_node);
      if (client.publish(topic_node, (char*)lwtUp.c_str())) {
         Serial.println("Publish lwt up ok");
      } else {
         Serial.println("Publish lwt up failed");
      }
   } else {
      Serial.println("MQTT connect failed, reset and try again...");
      abort();
   }
  
   return; 
}  

  
// Setuo routine runs once after reset  
void setup() {
   int startdelay;
   
   Serial.begin(115200);
   
   Serial.println("\n\nInitializing node");
  
   WifiConnect();
 
   BrokerConnect();   
   
   //seed the random number generator from micros
   randomSeed(micros());
   
   // put random delay in here between 0 and 20 seconds to lower probability
   // of all nodes talking at the same time after a power cut 
   startdelay = (int) random(0,21);
   
   Serial.print("Data collection starting in ");
   Serial.print(startdelay);
   Serial.println(" seconds");
   
   delay(startdelay * 1000);
   
   // Calculate when the first loops interval should finish ie
   // interval milliseconds from now
   nextturn = millis() + Interval;
   
   
   //initialise Wire library for I2C
   Wire.begin(4, 5);
   lightMeter.begin();
   

      pinMode(motionPin, INPUT);
   return;
   
   
  
}

void do_humidity(){
   float result;
   String humidity;   
  
  // humidity as %rh
   result = dht.readHumidity();
   if(!isnan(result)){
      humidity = result;
      if (client.publish(topic_humidity, (char*)humidity.c_str())) {
         Serial.print("Humidity: ");
         Serial.print(humidity);
         Serial.println("%");
      } else {
         Serial.println("Failed to publish humidity!");
      }
   } else {
     Serial.println("Failed to read humidity from DHT sensor!");
   }
   
   return;
}

// Temperature read and report subroutine
void do_temperature(){
   float result;
   String temperature;
   
   // temperature as centigrade
   result = dht.readTemperature();
   if(!isnan(result)){ 
      temperature = result;
      if (client.publish(topic_temperature, (char*)temperature.c_str())) {
         Serial.print("Temperature: ");
         Serial.print(temperature);
         Serial.println("*C");        
      } else {
         Serial.println("Failed to publish temperature!");
      }
   } else {
      Serial.println("Failed to read temperature from DHT sensor!");
   }

   return;  
}


// main loop runs forever
void loop() {

   // Reconnectr to wifi if connection is lost
   if(WiFi.status() != WL_CONNECTED){
      WifiConnect();
   }
  
   // Reconnect to broker if conenction is lost
   if(!client.connected()) {
      BrokerConnect();
   }   
   if (client.connected()) {
     Serial.println("connected!");
     //client.publish("test", "hello");
     Serial.println("sent!");
   }
   
   
   switch(nextAction){
   
     case 0:
       //Read motion detector pin...
       motionDetected += digitalRead(motionPin);
       Serial.print(digitalRead(motionPin));
       Serial.print("motionDetected = ");
       Serial.println(motionDetected);
       //If motion was seen and it's time for another motion report:
       if ((motionDetected > 0) && (nextMotionReport < millis())){
         String motion;
         motion = motionDetected;
         client.publish(topic_motion, (char*)motion.c_str());
         motionDetected = 0;
         nextMotionReport = millis() + motionInterval;
       }
       nextAction = 1;
       break;
     
     case 1:
       //Read and maybe publish light level
       
       lux = lightMeter.readLightLevel();
       Serial.print("Light: ");
       Serial.print(lux);
       Serial.println(" lx"); 
    
       if((lux-lastReportedLux > 50) or (lastReportedLux - lux > 50)){
         {String light;
         light = lux;
         client.publish(topic_light, (char*)light.c_str());}
         lastReportedLux = lux;
       }
       nextAction = 2;
       break;
       
     case 2:
       //DHT stuff
       do_humidity();
       nextAction = 3;
       break;
       
     case 3:
       //Temp stuff
       do_temperature();
       nextAction = 0;
       break;
   }
   
  while( nextturn - millis() < Interval ) {
     delay(10);
  }
  
  // Calculate end of next cycle time  
  nextturn += Interval;

  return; 
}
