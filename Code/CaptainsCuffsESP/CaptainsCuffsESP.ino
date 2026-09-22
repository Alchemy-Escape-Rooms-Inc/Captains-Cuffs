#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define VERSION "1.5.1"   // 1.5.1 (2026-09-22): WiFi sleep OFF, non-blocking MQTT reconnect, RSSI + reconnect count in the heartbeat log.
                          // Why: first live game - the board dropped its link 20x in an hour (16-63 s each); a solve landed 30 s late.

#define GAME_NAME "MermaidsTale"
#define PROP_NAME "CaptainsCuffs"

#define MQTT_TOPIC_COMMAND  "MermaidsTale/CaptainsCuffs/command"
#define MQTT_TOPIC_STATUS   "MermaidsTale/CaptainsCuffs/status"
#define MQTT_TOPIC_SOLVED   "MermaidsTale/CaptainsCuffs/solved"
#define MQTT_TOPIC_LOG      "MermaidsTale/CaptainsCuffs/log"
#define MQTT_TOPIC_MESSAGE  "MermaidsTale/CaptainsCuffs/message"
#define MQTT_TOPIC_SYSTEM   "MermaidsTale/CaptainsCuffs/system"
// Game-start announcement: the retained-state sweeper erases all saved
// cuff states around game start/end, so on GameStart we ask the Mega for
// a full status dump (16 msgs, once per game) to repopulate them.
#define MQTT_TOPIC_GAMESTART "MermaidsTale/GameStart"
// SkullVision publishes per-zone occupancy here (retained). Forwarded to the
// Mega for logging/status only since v1.5.0 - it no longer decides the solve.
#define MQTT_TOPIC_SKULL_PREFIX "MermaidsTale/CaptainsCuffs/system/Skull"
#define NUM_SKULL_ZONES 5
// SkullVision's debounced head-count (retained, "0".."9"). Forwarded to the
// Mega as "n<N>". v1.5.0 solve rule: the number of touched skulls must equal
// this head-count - WHICH skulls are touched does not matter.
#define MQTT_TOPIC_PEOPLE "MermaidsTale/CaptainsCuffs/system/People"

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
bool loadedStatus = false;

String incoming = "";

const unsigned long heartBeatPulse = 5 * 1000UL;
unsigned long lastMqttAttempt = 0;      // non-blocking reconnect pacing (v1.5.1)
unsigned long lastWifiKick = 0;
unsigned long mqttLostAt = 0;           // millis() when the link was last seen down (0 = up)
unsigned int  mqttReconnects = 0;

unsigned long lastTime = 0;


// *************** FUNCTIONS  *******************
void setupWiFi() {
  delay(1000);
  Serial.println("*********** WIFI ***********");
  Serial.print("Connecting to SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);   // v1.5.1: modem sleep made the link flap (missed keepalives)
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID,WIFI_PASS);

  unsigned long t0 = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - t0 < 20000UL){   // bounded: the main loop keeps retrying
    delay(100);
    Serial.print("-");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nConnected." : "\nWiFi not up yet - continuing, will retry in loop.");
}
//========== MQTT SERVER ================
void connectMQTT() {
  // v1.5.1: NON-BLOCKING. One attempt every 2 s; never delay() the loop, so the
  // Mega's serial messages keep being read while the link is down.
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiKick > 10000UL) {
      lastWifiKick = millis();
      Serial.println("WiFi down - reconnect kick");
      WiFi.reconnect();
    }
    if (mqttLostAt == 0) mqttLostAt = millis();
    return;
  }
  if (millis() - lastMqttAttempt < 2000UL) return;
  lastMqttAttempt = millis();
  if (mqttLostAt == 0) mqttLostAt = millis();
  {
    Serial.print("Connecting to MQTT...");

    String clientId = PROP_NAME;
    clientId += "_";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected!");

      // Subscribe to command topic
      mqttClient.subscribe(MQTT_TOPIC_COMMAND);
      mqttClient.subscribe(MQTT_TOPIC_GAMESTART);

      // SkullVision occupancy (retained, so a reconnect replays current state)
      for (int i = 0; i < NUM_SKULL_ZONES; i++) {
        char t[64];
        snprintf(t, sizeof(t), "%s%d", MQTT_TOPIC_SKULL_PREFIX, i);
        mqttClient.subscribe(t);
      }
      // SkullVision head-count = the solve target (retained)
      mqttClient.subscribe(MQTT_TOPIC_PEOPLE);

      // Announce we're online
      mqttClient.publish(MQTT_TOPIC_STATUS, "ONLINE");
      mqttClient.publish(MQTT_TOPIC_SOLVED, puzzleSolved ? "true" : "false", true);
      if (mqttReconnects > 0) {
        unsigned long down = mqttLostAt ? (millis() - mqttLostAt) / 1000UL : 0;
        mqttLogf("%s v%s online (reconnect #%u after %lus down, rssi=%d)", PROP_NAME, VERSION, mqttReconnects, down, WiFi.RSSI());
      } else {
        mqttLogf("%s v%s online (boot, rssi=%d)", PROP_NAME, VERSION, WiFi.RSSI());
      }
      mqttReconnects++;
      mqttLostAt = 0;

      // Any (re)connect may have missed sensor changes while offline —
      // request a full status dump from the Mega to re-sync MQTT state.
      loadedStatus = false;

    } else {
      Serial.printf("failed (rc=%d), retry in 2s\n", mqttClient.state());
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
  while(end > msg && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')){
    *end = '\0';
    end--;
  }

  //Serial.printf("[MQTT] Received on %s: %s\n", topicBuf,msg);

  if(strcmp(topicBuf, MQTT_TOPIC_PEOPLE) == 0){
    // Head-count from SkullVision. Same sweeper caveat as the skull topics:
    // an empty payload is a wipe, not "zero people" - ignore it.
    if(msg[0] == '\0')
      return;
    if(msg[0] < '0' || msg[0] > '9')
      return;
    int n = atoi(msg);
    if(n < 0 || n > 9)
      return;
    Serial.print("n");
    Serial.println(n);                        // -> Mega over serial: "n<N>"
    return;
  }

  if(strncmp(topicBuf, MQTT_TOPIC_SKULL_PREFIX, strlen(MQTT_TOPIC_SKULL_PREFIX)) == 0){
    // Occupancy from SkullVision. The retained-state sweeper clears these
    // topics with an EMPTY payload around game start — that is a wipe, not a
    // state change, so ignore it (live changes keep the Mega in sync).
    if(msg[0] == '\0')
      return;
    int zone = atoi(topicBuf + strlen(MQTT_TOPIC_SKULL_PREFIX));
    if(zone < 0 || zone >= NUM_SKULL_ZONES)
      return;
    bool occupied = (strcmp(msg, "Occupied") == 0);
    Serial.print("z");
    Serial.print(zone);
    Serial.println(occupied ? ":o" : ":e");   // -> Mega over serial
    return;
  }

  if(strcmp(topicBuf,MQTT_TOPIC_GAMESTART) == 0){
    // Sweeper wipes retained cuff states at game start — repopulate them.
    // Read-only re-sync: safe even if a retained GameStart replays at boot.
    Serial.println("displayStatus");
    return;
  }

  if(strcmp(topicBuf,MQTT_TOPIC_COMMAND) != 0){
    return;
  }

  if(strcmp(msg,"PING") == 0){
    // NOTE: never publish back to MQTT_TOPIC_COMMAND from this callback --
    // the board subscribes to it, so the broker echoes it back = infinite loop.
    mqttLogf("Command received: %s", msg);
    mqttClient.publish(MQTT_TOPIC_MESSAGE,"PONG");
    //Serial.println("[MQTT] PING -> PONG");
    return;
  }
  if(strcmp(msg,"STATUS") == 0){
    const char* state = puzzleSolved ? "SOLVED" : "READY";
    mqttLogf("Command received: %s", msg);
    mqttClient.publish(MQTT_TOPIC_MESSAGE,state);
    //Serial.printf("[MQTT] STATUS -> %s\n",state);
    return;
  }
  if(strcmp(msg,"RESET") == 0){
    mqttLogf("Command received: %s", msg);
    mqttClient.publish(MQTT_TOPIC_MESSAGE,"OK");
    //Serial.println("[MQTT] RESET -> Rebooting...");
    delay(100);
    ESP.restart();
    return;
  }
  if (strcmp(msg, "PUZZLE_RESET") == 0) {
    puzzleSolved = false;
    mqttLogf("Command received: %s", msg);
    Serial.println("puzzleReset");            // -> Mega: relock relays
    mqttClient.publish(MQTT_TOPIC_SOLVED, "false", true);
    mqttClient.publish(MQTT_TOPIC_MESSAGE, "OK");
    //Serial.println("[MQTT] PUZZLE_RESET -> OK");
    return;
  }
  if (strcmp(msg, "SOLVE") == 0) {
    puzzleSolved = true;
    mqttLogf("Command received: %s", msg);
    Serial.println("skullSolve");             // -> Mega: actually release
    mqttClient.publish(MQTT_TOPIC_SOLVED, "true", true);
    mqttClient.publish(MQTT_TOPIC_MESSAGE, "SOLVED");
    return;
  }
  if (strcmp(msg, "SKULLS_SOLVED") == 0) {
    // Published by SkullVision when every skull zone is held at once.
    puzzleSolved = true;
    mqttLogf("Vision: skulls solved");
    Serial.println("skullSolve");             // -> Mega over serial
    mqttClient.publish(MQTT_TOPIC_SOLVED, "true", true);
    mqttClient.publish(MQTT_TOPIC_MESSAGE, "SOLVED");
    return;
  }
  if (strcmp(msg, "GAME_START") == 0) {
    puzzleSolved = false;
    mqttLogf("Vision: game start");
    Serial.println("beginGame");              // -> Mega over serial
    mqttClient.publish(MQTT_TOPIC_SOLVED, "false", true);
    return;
  }
  if (strcmp(msg, "OPEN_ALL") == 0) {
    // GM manual override: drop every relay regardless of game state.
    mqttLogf("Command received: %s", msg);
    Serial.println("openAll");                // -> Mega over serial
    mqttClient.publish(MQTT_TOPIC_MESSAGE, "OK");
    return;
  }

  if (strcmp(msg, "displayStatus") == 0) {
    mqttLogf("Command received: %s", msg);
    mqttClient.publish(MQTT_TOPIC_MESSAGE, "Printing Status.");
    Serial.println("displayStatus");
    return;
  }
  //Serial.printf("[MQTT] Unknown command: %s\n", msg);
}

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setKeepAlive(15);
  mqttClient.setSocketTimeout(5);      // v1.5.1: a dead socket is noticed in 5 s, not 15
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
  //Serial.println(buffer);
}

void heartBeat(){
  unsigned long currentTime = millis();
  if(!(currentTime - lastTime > heartBeatPulse))
    return;
  lastTime = currentTime;
  // Announce we're online (status text unchanged for WatchTower/M3; RSSI rides on the log line + its own topic)
  mqttClient.publish(MQTT_TOPIC_STATUS, "ONLINE");
  mqttLogf("%s v%s online rssi=%d reconnects=%u", PROP_NAME, VERSION, WiFi.RSSI(), mqttReconnects);
  char rssi[8]; snprintf(rssi, sizeof(rssi), "%d", WiFi.RSSI());
  mqttClient.publish(MQTT_TOPIC_SYSTEM "/RSSI", rssi);
}

/*
   bool getTouchSensorState(int sensor){
   String cmd = "ts" + sensor.to_String();
   sendMegaCommand(cmd);
   delay(13);
   receiveMegaCommand();
   }
   void touchSensorsStatus(){
   bool sensor1 = getTouchSensorState(1);
   bool sensor2 = getTouchSensorState(2);
   bool sensor3 = getTouchSensorState(3);
   bool sensor4 = getTouchSensorState(4);
   bool sensor5 = getTouchSensorState(5);
   bool sensor6 = getTouchSensorState(6);
   bool sensor7 = getTouchSensorState(7);
   bool sensor8 = getTouchSensorState(8);

   const char * sensor1_state = sensor1  ? "triggered" : "not triggered";
   const char * sensor2_state = sensor2  ? "triggered" : "not triggered";
   const char * sensor3_state = sensor3  ? "triggered" : "not triggered";
   const char * sensor4_state = sensor4  ? "triggered" : "not triggered";
   const char * sensor5_state = sensor5  ? "triggered" : "not triggered";
   const char * sensor6_state = sensor6  ? "triggered" : "not triggered";
   const char * sensor7_state = sensor7  ? "triggered" : "not triggered";
   const char * sensor8_state = sensor8  ? "triggered" : "not triggered";

   String topic1 = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor0";
   mqttClient.publish(topic1.c_str(),sensor1_state);

   String topic2 = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor1";
   mqttClient.publish(topic2.c_str(),sensor2_state);

   String topic3 = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor2";
   mqttClient.publish(topic3.c_str(),sensor3_state);

   String topic4 = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor3";
   mqttClient.publish(topic4.c_str(),sensor4_state);

   String topic5 = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor4";
   mqttClient.publish(topic5.c_str(),sensor5_state);

   String topic6 = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor5";
   mqttClient.publish(topic6.c_str(),sensor6_state);

   String topic7 = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor6";
   mqttClient.publish(topic7.c_str(),sensor7_state);

   String topic8 = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor7";
   mqttClient.publish(topic8.c_str(),sensor8_state);
   }

   bool getCuffState(int cuff){
   sendMegaCommand();
   delay(13);
   }
   void cuffsStatus(){

   bool cuff1 = getCuffState(1);
   bool cuff2 = getCuffState(1);
   bool cuff3 = getCuffState(1);
   bool cuff4 = getCuffState(1);
   bool cuff5 = getCuffState(1);
   bool cuff6 = getCuffState(1);
   bool cuff7 = getCuffState(1);
   bool cuff8 = getCuffState(1);

   const char * cuff1_state = cuff1  ? "closed" : "opened";
   const char * cuff2_state = cuff2  ? "closed" : "opened";
   const char * cuff3_state = cuff3  ? "closed" : "opened";
   const char * cuff4_state = cuff4  ? "closed" : "opened";
   const char * cuff5_state = cuff5  ? "closed" : "opened";
const char * cuff6_state = cuff6  ? "closed" : "opened";
const char * cuff7_state = cuff7  ? "closed" : "opened";
const char * cuff8_state = cuff8  ? "closed" : "opened";

String topic1 = String(MQTT_TOPIC_SYSTEM) + "/Cuff0";
mqttClient.publish(topic1.c_str(),cuff1_state);

String topic2 = String(MQTT_TOPIC_SYSTEM) + "/Cuff1";
mqttClient.publish(topic2.c_str(),cuff2_state);

String topic3 = String(MQTT_TOPIC_SYSTEM) + "/Cuff2";
mqttClient.publish(topic3.c_str(),cuff3_state);

String topic4 = String(MQTT_TOPIC_SYSTEM) + "/Cuff3";
mqttClient.publish(topic4.c_str(),cuff4_state);

String topic5 = String(MQTT_TOPIC_SYSTEM) + "/Cuff4";
mqttClient.publish(topic5.c_str(),cuff5_state);

String topic6 = String(MQTT_TOPIC_SYSTEM) + "/Cuff5";
mqttClient.publish(topic6.c_str(),cuff6_state);

String topic7 = String(MQTT_TOPIC_SYSTEM) + "/Cuff6";
mqttClient.publish(topic7.c_str(),cuff7_state);

String topic8 = String(MQTT_TOPIC_SYSTEM) + "/Cuff7";
mqttClient.publish(topic8.c_str(),cuff8_state);


}
*/

void parseSensorsMessage(const char* msg) {
  int id;
  char state[3];

  if (sscanf(msg, "s%d:%2s", &id, state) != 2) {
    Serial.println("Parse error");
    return;
  }

  String topic = String(MQTT_TOPIC_SYSTEM) + "/TouchSensor" + String(id);

  bool triggered = strcmp(state, "t") == 0;
  String out = triggered ? "Triggered" : "Not Triggered";
  mqttClient.publish(topic.c_str(),out.c_str(),true);  // retained: broker keeps latest state
}


void parseCuffsMessage(const char* msg) {
  int id;
  char state[3];

  if (sscanf(msg, "c%d:%1s", &id, state) != 2) {
    Serial.println("Parse error");
    return;
  }

  String topic = String(MQTT_TOPIC_SYSTEM) + "/Cuff" + String(id);

  bool closed = strcmp(state, "c") == 0;
  String out = closed ? "Closed" : "Opened";
  mqttClient.publish(topic.c_str(),out.c_str(),true);  // retained: broker keeps latest state
}

void parseOtherMessage(const char* msg) {
  char state[3];

  if (sscanf(msg, "p:%2s",state) != 1) {
    Serial.println("Parse error");
    return;
  }

  bool solved = strcmp(state, "s") == 0;
  puzzleSolved = solved;

  mqttClient.publish(MQTT_TOPIC_SOLVED, solved ? "true" : "false", true);

  String out = "Puzzle: ";
  out+= (solved) ? "Solved" : "Not Solved";
  mqttClient.publish(MQTT_TOPIC_STATUS,out.c_str());
}

void parseMessage(String msg){
  char startingLetter = tolower(msg.charAt(0));
  if(startingLetter == 's')
    parseSensorsMessage(msg.c_str());
  else if(startingLetter == 'c')
    parseCuffsMessage(msg.c_str());
  else if(startingLetter == 'b')  // was 'B', but startingLetter is tolower()'d — never matched
    mqttClient.publish(MQTT_TOPIC_MESSAGE,"Beginning the game.");
  else
    parseOtherMessage(msg.c_str());
}



void handleSerialTransmission(){
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      incoming.trim();
      parseMessage(incoming);
      incoming = "";
    } else {
      incoming += c;
    }
  }
}


void program(){
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  
  if(!loadedStatus){
    Serial.println("displayStatus");
    loadedStatus = true;
  }

  handleSerialTransmission();
  heartBeat();
}

void _init(){

  Serial.begin(115200);
  Serial.println("Initializing peripherals.");
  //network setup
  setupWiFi();
  //mqtt setup
  setupMQTT();
  Serial.println("Completed initialization.");
}

// ***************** SETUP *********************
void setup() {
  _init();
}

// **************** MAIN LOOP ****************
void loop() {
  program();
}



