#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define VERSION "1.0.0"

#define GAME_NAME "MermaidsTale"
#define PROP_NAME "CaptainsCuffs"

#define MQTT_TOPIC_COMMAND  "MermaidsTale/CaptainsCuffs/command"
#define MQTT_TOPIC_STATUS   "MermaidsTale/CaptainsCuffs/status"
#define MQTT_TOPIC_LOG      "MermaidsTale/CaptainsCuffs/log"
//************ GLOBAL VARIABLES **********
WiFiClient espClient;
PubSubClient mqttClient(espClient);



// WiFi credentials
const char* WIFI_SSID = "AlchemyGuest";
const char* WIFI_PASS = "VoodooVacation5601";

// MQTT broker
const char* MQTT_SERVER = "10.1.10.115";
const int MQTT_PORT = 1883;

bool puzzleSolved = false;

String incoming = "";

// *************** FUNCTIONS  *******************
//WIFI NETWORK
void setup_wifi() {
  delay(1000);
  Serial.println("*********** WIFI ***********");
  Serial.print("Connecting to SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID,WIFI_PASS);

  while(WiFi.status() != WL_CONNECTED){
    delay(100);
    Serial.print("-");
  }
  Serial.println("\nConnected.");
}

//MQTT SERVER
void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");

        String clientId = PROP_NAME;
        clientId += "_";
        clientId += String(random(0xffff), HEX);

        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("connected!");

            // Subscribe to command topic
            mqttClient.subscribe(MQTT_TOPIC_COMMAND);

            // Announce we're online
            mqttClient.publish(MQTT_TOPIC_STATUS, "ONLINE");
            mqttLogf("%s v%s online", PROP_NAME, VERSION);

        } else {
            Serial.printf("failed (rc=%d), retrying in 5s\n", mqttClient.state());
            delay(5000);
        }
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char topicBuf[128];
  strncpy(topicBuf,topic,sizeof(topicBuf)-1);
  topicBuf[sizeof(topicBuf)-1] = '\0';

  char message[128];
  if(length >= sizeof(message)){
    length = sizeof(message) - 1;
  }

  memcpy(message,payload,length);
  message[length] = '\0';

  char * msg = message;
  while(*msg == ' ' || *msg == '\r' || *msg == '\n')
    msg++;
  char * end = msg + strlen(msg) -1;
  while(end > msg && (*end == ' ' || *end == 't' || *end == '\r' || *end == '\n')){
    *end = '\0';
    end--;
  }

  Serial.printf("[MQTT] Received on %s: %s\n", topicBuf,msg);

  if(strcmp(topicBuf,MQTT_TOPIC_COMMAND) != 0){
    return;
  }

  if(strcmp(msg,"PING") == 0){
    mqttClient.publish(MQTT_TOPIC_COMMAND,"PONG");
    Serial.println("[MQTT] PING -> PONG");
    return;
  }
  if(strcmp(msg,"STATUS") == 0){
    const char* state = puzzleSolved ? "SOLVED" : "READY";
    mqttClient.publish(MQTT_TOPIC_COMMAND,state);
    Serial.printf("[MQTT] STATUS -> %s\n",state);
    return;
  }
  if(strcmp(msg,"RESET") == 0){
    mqttClient.publish(MQTT_TOPIC_COMMAND,"OK");
    Serial.println("[MQTT] RESET -> Rebooting...");
    delay(100);
    ESP.restart();
    return;
  }
  if (strcmp(msg, "PUZZLE_RESET") == 0) {
    puzzleSolved = false;
    // Add your puzzle reset logic here
    mqttClient.publish(MQTT_TOPIC_COMMAND, "OK");
    Serial.println("[MQTT] PUZZLE_RESET -> OK");
    return;
  }
  if (strcmp(msg, "SOLVE") == 0) {
        puzzleSolved = true;
        mqttClient.publish(MQTT_TOPIC_COMMAND, "SOLVED");
        return;
  }
   Serial.printf("[MQTT] Unknown command: %s\n", msg);
}

void setupMQTT() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);  // Increase if needed
}

//GENERAL FUNCTIONS
void mqttLogf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    mqttClient.publish(MQTT_TOPIC_LOG, buffer);
    Serial.println(buffer);
}

void receiveMegaCommand(String cmd){
  if(cmd == "PONG"){
    mqttClient.publish(MQTT_TOPIC_COMMAND, "MEGA_PONG");
  } else if (cmd == "READY") {
    mqttClient.publish(MQTT_TOPIC_COMMAND, "READY_OK");
  } else if (cmd == "RESET"){
    puzzleSolved = false;
    mqttClient.publish(MQTT_TOPIC_COMMAND, "PUZZLE_HAS_RESET");
  } else if (cmd == "SOLVED"){
    puzzleSolved = true;
    mqttClient.publish(MQTT_TOPIC_COMMAND, "PUZZLE_SOLVED");
  }else if (cmd == "MANUALLY_SOLVED"){
    puzzleSolved = true;
    mqttClient.publish(MQTT_TOPIC_COMMAND, "MANUALLY_SOLVED");
  }

}

void sendMegaCommand(String cmd){
  Serial.println(cmd);
}

void program(){
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  while(Serial.available()){
    char c = Serial.read();

    if(c == '\n') {
      incoming.trim();
      receiveMegaCommand(incoming);
      incoming = "";
    } else {
      incoming += c;
    }
  }
}

void _init(){
  //network setup
  setup_wifi();
  //mqtt setup
  setupMQTT();
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



