#include <WiFi.h>
#include <PubSubClient.h>

#define OPEN_PIN 2
#define CLOSE_PIN 3
#define RESET_PIN 4

//************ GLOBAL VARIABLES **********
WiFiClient espClient;
PubSubClient mqtt(espClient);


const char * ssid = "AlchemyGuest";
const char * password = "VoodooVacation5601";
const char * mqttServer = "10.1.10.115";
const char * topic = "MermaidsTale/CaptainsESP"; 

// *************** FUNCTIONS  *******************
//WIFI NETWORK
void setup_wifi() {
  delay(1000);
  Serial.println("*********** WIFI ***********");
  Serial.print("\n Connecting to ");
  Serial.print(ssid);

  WiFi.begin(ssid,password);

  while(WiFi.status() != WL_CONNECTED){
    delay(100);
    Serial.print("-");
  }
  Serial.println("\nConnected.");
}

//MQTT SERVER
void reconnect() {
  while (!mqtt.connected()) {
    Serial.print("******** MQTT SERVER ********");
    if (mqtt.connect("ESP32 WROOM")) {
      Serial.print("Connection to broker established: ");
      Serial.println(mqttServer);

      mqtt.publish(topic, "Connected!");        //Message sent to broker, identifying the connected shell.
      mqtt.subscribe(topic);                             //shell subscribing to the broker's topic

    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(". Trying again in 5 seconds.");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++)
    message += (char)payload[i];
  Serial.print("MQTT received: ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(message);

  if (strcmp(topic, "OpenCuffs") == 0) {
    openCuffs();
  } else if (strcmp(topic, "CloseCuffs") == 0){
    closeCuffs();
  }else if (strcmp(topic, "Reset Game") == 0){
    resetGame();
  }
}

void setup_server(){
  mqtt.setServer(mqttServer, 1883);
  mqtt.setCallback(callback);
}

//GENERAL FUNCTIONS

void setup_io(){
  pinMode(OPEN_PIN,OUTPUT);
  pinMode(CLOSE_PIN,OUTPUT);
  pinMode(RESET_PIN,OUTPUT);

  digitalWrite(OPEN_PIN,HIGH);
  digitalWrite(CLOSE_PIN,HIGH);
  digitalWrite(RESET_PIN,HIGH);
}

void openCuffs(){
  digitalWrite(OPEN_PIN,LOW);
}

void closeCuffs(){
  digitalWrite(CLOSE_PIN,LOW);
}

void resetGame(){
  digitalWrite(RESET_PIN,LOW);
}

void program(){
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();

}

void resetPins(){
  digitalWrite(OPEN_PIN,HIGH);
  digitalWrite(CLOSE_PIN,HIGH);
  digitalWrite(RESET_PIN,HIGH);
}
void _init(){
  //io setup
  setup_io();
  //mqtt setup
  setup_server();
}

// ***************** SETUP *********************
void setup() {
  Serial.begin(115200);
  _init();
}

// **************** MAIN LOOP ****************
void loop() {
  program();
}



