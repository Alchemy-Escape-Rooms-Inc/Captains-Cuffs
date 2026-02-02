/* Written By Ryan Laing ("Arduino Guru")
   Alchemy Escape Room "Captain's Cuffs"
   HALL SENSOR VERSION - Arduino Mega

   Puzzle: All engaged cuffs must have their touch sensors activated simultaneously to release

   Hardware:
   - Touch sensors: Detect player touch (HIGH = touched)
   - Hall sensors: Detect magnet when cuff locked (LOW = magnet detected with INPUT_PULLUP)
   - Relays: Control cuff locks (HIGH = locked, LOW = released)

   MQTT Support: Currently disabled (Arduino Mega has no WiFi/Ethernet)
   To enable MQTT, add an Ethernet shield and uncomment the MQTT sections
*/
// MQTT Libraries (disabled)
// #include <PubSubClient.h>
// #include <Ethernet.h>

// ==================== CONFIGURATION ====================
const int numCuffs = 8;
const int touchPins[numCuffs] = {22, 23, 24, 25, 26, 27, 28, 29};
const int hallPins[numCuffs] = {30, 31, 32, 46, 34,48, 36, 37}; // Pins 3 and 5 disabled
const int relayPins[numCuffs] = {38, 39, 40, 41, 42, 43, 44, 45};

const unsigned long debounceDelay = 50;
const unsigned long autoResetDelay = 5 * 60 * 1000UL; // 5 minutes

// MQTT Configuration (disabled)
// byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// IPAddress mqttServer(192, 168, 1, 100);
// const char* mqttClientId = "CaptainsCuffs";
// const int mqttPort = 1883;

// ==================== STATE VARIABLES ====================
struct CuffState {
  bool engaged;          // Hall sensor detects magnet
  bool touched;          // Touch sensor activated
  bool released;         // Cuff has been released
  unsigned long lastTouchTime;
};

CuffState cuffs[numCuffs];
bool puzzleSolved = false;
unsigned long puzzleSolvedTime = 0;

// State tracking for change detection
bool lastCuffStates[numCuffs];
bool lastTouchStates[numCuffs];
bool lastSolutionCheck = false;

// ==================== NETWORK (DISABLED) ====================
// EthernetClient ethClient;
// PubSubClient mqtt(ethClient);

// ==================== SETUP ====================
void setup() {
  Serial.begin(9600);
  //while (!Serial) { ; }   //loop until serial communication is established with a host.

  Serial.println("\n=== CAPTAIN'S CUFFS - HALL SENSOR VERSION ===");
  Serial.println("Platform: Arduino Mega");
  Serial.println("Initializing hardware...");

  // Initialize pins and state
  for (int i = 0; i < numCuffs; i++) {
    pinMode(touchPins[i], INPUT);

    // Skip disabled hall sensor pins
    if (hallPins[i] != -1) {
      pinMode(hallPins[i], INPUT_PULLUP); // Pull-up to prevent floating pins
    }

    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Lock all cuffs initially

    cuffs[i].engaged = false;
    cuffs[i].touched = false;
    cuffs[i].released = false;
    cuffs[i].lastTouchTime = 0;

    lastCuffStates[i] = false;
    lastTouchStates[i] = false;
  }

  // Allow pull-up resistors to stabilize
  Serial.println("Stabilizing sensors...");
  delay(100);

  // MQTT/Network Initialization (disabled)
  /*
  Serial.print("Initializing Ethernet...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println(" FAILED (using DHCP)");
    Serial.println("Trying static IP...");
    IPAddress ip(192, 168, 1, 177);
    IPAddress dns(192, 168, 1, 1);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    Ethernet.begin(mac, ip, dns, gateway, subnet);
  }
  Serial.print(" OK - IP: ");
  Serial.println(Ethernet.localIP());

  mqtt.setServer(mqttServer, mqttPort);
  mqtt.setCallback(mqttCallback);
  connectMQTT();
  */

  Serial.println("\n=== SYSTEM READY ===");
  Serial.println("Monitoring for state changes...");
  Serial.println("Type 'help' for available commands\n");
}

// ==================== MAIN LOOP ====================
void loop() {
  // MQTT Maintenance (disabled)
  /*
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();
  */

  // Handle auto-reset after puzzle solved
  if (puzzleSolved && (millis() - puzzleSolvedTime >= autoResetDelay)) {
    resetPuzzle();
    return;
  }

  // Skip puzzle logic if already solved
  if (puzzleSolved) return;

  int activeCuffs = 0;
  int activeTouches = 0;
  bool stateChanged = false;

  // Read all sensors and detect changes
  for (int i = 0; i < numCuffs; i++) {
    // Skip disabled cuffs 3 and 5
    //if (i == 3 || i == 5) {
    //  continue;
    //}

    // Skip disabled hall sensors
    bool magnetDetected = (hallPins[i] != -1) ? (digitalRead(hallPins[i]) == LOW) : false;
    bool currentTouch = digitalRead(touchPins[i]) == HIGH;

    cuffs[i].engaged = magnetDetected;
    cuffs[i].touched = currentTouch;

    // Print only on state changes
    if (magnetDetected != lastCuffStates[i]) {
      Serial.print("Cuff ");
      Serial.print(i);
      Serial.print("     Relay ");
      Serial.print(digitalRead(relayPins[i]) == HIGH ? "ON      " : "OFF     ");
      Serial.print("   Hall Sensor ");
      Serial.print(magnetDetected ? "TRIGGERED        " : "NOT TRIGGERED    ");
      Serial.print("   Touch Sensor ");
      Serial.print(currentTouch ? "ACTIVE        " : "NOT ACTIVE    ");
      Serial.print("   Cuffs ");
      Serial.println(digitalRead(relayPins[i]) == HIGH ? "LOCKED" : "UNLOCKED");
      lastCuffStates[i] = magnetDetected;
      stateChanged = true;
    }

    if (currentTouch != lastTouchStates[i]) {
      Serial.print("Cuff ");
      Serial.print(i);
      Serial.print("     Relay ");
      Serial.print(digitalRead(relayPins[i]) == HIGH ? "ON      " : "OFF     ");
      Serial.print("   Hall Sensor ");
      Serial.print(magnetDetected ? "TRIGGERED        " : "NOT TRIGGERED    ");
      Serial.print("   Touch Sensor ");
      Serial.print(currentTouch ? "ACTIVE        " : "NOT ACTIVE    ");
      Serial.print("   Cuffs ");
      Serial.println(digitalRead(relayPins[i]) == HIGH ? "LOCKED" : "UNLOCKED");
      lastTouchStates[i] = currentTouch;
      stateChanged = true;
    }

    // Count active components
    if (magnetDetected) {
      activeCuffs++;

      // Handle touch with debouncing
      if (currentTouch && (millis() - cuffs[i].lastTouchTime > debounceDelay)) {
        activeTouches++;
        cuffs[i].lastTouchTime = millis();
      }
    }
  }

  // Check solution (only print when status changes)
  bool currentSolutionStatus = (activeCuffs > 0 && activeTouches == activeCuffs);

  if (currentSolutionStatus && !lastSolutionCheck) {
    Serial.print("SOLUTION: ");
    Serial.print(activeTouches);
    Serial.print("/");
    Serial.print(activeCuffs);
    Serial.println(" - SOLVING PUZZLE!");
    releaseCuffs();
    stateChanged = true;
  }

  lastSolutionCheck = currentSolutionStatus;

  // Publish status on any state change (disabled)
  // if (stateChanged) {
  //   publishStatus();
  // }

  // Handle serial commands
  if (Serial.available()) {
    handleSerialCommand();
  }
}

// ==================== PUZZLE LOGIC ====================
void releaseCuffs() {
  Serial.println("\n=== RELEASING CUFFS ===");

  for (int i = 0; i < numCuffs; i++) {
    if (cuffs[i].engaged) {
      digitalWrite(relayPins[i], LOW);
      cuffs[i].released = true;
      Serial.print("Cuff ");
      Serial.print(i);
      Serial.println(" RELEASED");
    }
  }

  puzzleSolved = true;
  puzzleSolvedTime = millis();

  Serial.println("*** PUZZLE SOLVED! ***");
  Serial.print("Auto-reset in ");
  Serial.print(autoResetDelay / 1000);
  Serial.println(" seconds\n");

  // mqtt.publish("cuffs/status", "solved"); // MQTT disabled
}

void resetPuzzle() {
  Serial.println("\n=== RESETTING PUZZLE ===");

  for (int i = 0; i < numCuffs; i++) {
    digitalWrite(relayPins[i], HIGH);
    cuffs[i].released = false;
  }

  puzzleSolved = false;
  Serial.println("Ready for next players\n");

  // publishStatus(); // MQTT disabled
  // mqtt.publish("cuffs/status", "reset"); // MQTT disabled
}


void openCuff(byte index){
    digitalWrite(relayPins[index], LOW);
    cuffs[index].released = true;
    Serial.print("Cuff ");
    Serial.print(index);
    Serial.println(" opened");
}

void openAllCuffs() {
  Serial.println("\n=== OPENING ALL CUFFS ===");

  for (int i = 0; i < numCuffs; i++)
    openCuff(i);

  Serial.println("All cuffs opened\n");
  // publishStatus(); // MQTT disabled
  // mqtt.publish("cuffs/status", "all_released"); // MQTT disabled
}


void closeCuff(byte index){
    digitalWrite(relayPins[index], HIGH);
    cuffs[index].released = false;
    Serial.print("Cuff ");
    Serial.print(index);
    Serial.println(" closed");
}


void closeAllCuffs() {
  Serial.println("\n=== CLOSING ALL CUFFS ===");

  for (int i = 0; i < numCuffs; i++)
    closeCuff(i);
  puzzleSolved = false;
  Serial.println("All cuffs closed\n");
  // publishStatus(); // MQTT disabled
  // mqtt.publish("cuffs/status", "all_locked"); // MQTT disabled
}
// ==================== STATUS DISPLAY ====================
void printDetailedStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("Puzzle solved: ");
  Serial.println(puzzleSolved ? "YES" : "NO");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println("s");

  if (puzzleSolved) {
    Serial.print("Reset in: ");
    Serial.print((autoResetDelay - (millis() - puzzleSolvedTime)) / 1000);
    Serial.println("s");
  }

  Serial.println();
  Serial.println("CUFF | LOCK STATUS | TOUCH SENSOR | HALL SENSOR");
  Serial.println("-----|-------------|--------------|-------------");

  for (int i = 0; i < numCuffs; i++) {
    bool isLocked = digitalRead(relayPins[i]) == HIGH;
    bool isTouched = digitalRead(touchPins[i]) == HIGH;
    bool magnetDetected = (hallPins[i] != -1) ? (digitalRead(hallPins[i]) == LOW) : false;

    Serial.print("  ");
    Serial.print(i);
    Serial.print("  |   ");

    // Disable cuffs 3 and 5
    //if (i == 3 || i == 5) {
    //  Serial.println("DISABLED   |   DISABLED    |   DISABLED");
    //  continue;
    //}

    Serial.print(isLocked ? "LOCKED   " : "UNLOCKED ");
    Serial.print(" |   ");
    Serial.print(isTouched ? "ACTIVE      " : "NOT ACTIVE  ");
    Serial.print(" |   ");
    Serial.println(magnetDetected ? "CLOSED" : "OPEN  ");
  }
  Serial.println("===============================================\n");
}

// ==================== MQTT (DISABLED) ====================
/*
void connectMQTT() {
  Serial.print("Connecting to MQTT broker...");

  int attempts = 0;
  while (!mqtt.connected() && attempts < 5) {
    if (mqtt.connect(mqttClientId)) {
      Serial.println(" CONNECTED");
      mqtt.subscribe("cuffs/command/#");
      mqtt.publish("cuffs/status", "online");
      publishStatus();
      return;
    }
    Serial.print(".");
    delay(1000);
    attempts++;
  }

  Serial.println(" FAILED (continuing without MQTT)");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MQTT received: ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(message);

  if (strcmp(topic, "cuffs/command/reset") == 0) {
    resetPuzzle();
  } else if (strcmp(topic, "cuffs/command/status") == 0) {
    printDetailedStatus();
    publishStatus();
  } else if (strcmp(topic, "cuffs/command/lockall") == 0) {
    closeAllCuffs();
  } else if (strcmp(topic, "cuffs/command/releaseall") == 0) {
    openAllCuffs();
  }
}

void publishStatus() {
  if (!mqtt.connected()) return;

  // Detailed JSON status
  String status = "{";
  status += "\"solved\":" + String(puzzleSolved ? "true" : "false");
  status += ",\"cuffs\":[";

  for (int i = 0; i < numCuffs; i++) {
    if (i > 0) status += ",";
    status += "{\"id\":" + String(i);
    status += ",\"engaged\":" + String(cuffs[i].engaged ? "true" : "false");
    status += ",\"touched\":" + String(cuffs[i].touched ? "true" : "false");
    status += ",\"released\":" + String(cuffs[i].released ? "true" : "false");
    status += "}";
  }

  status += "]}";
  mqtt.publish("cuffs/status", status.c_str());

  // Clean human-readable lock status
  String lockStatus = "Cuffs: ";
  for (int i = 0; i < numCuffs; i++) {
    bool isLocked = digitalRead(relayPins[i]) == HIGH;
    lockStatus += String(i) + "=";
    lockStatus += isLocked ? "LOCKED" : "UNLOCKED";
    if (i < numCuffs - 1) lockStatus += ", ";
  }
  mqtt.publish("cuffs/locks", lockStatus.c_str());

  // Summary counts
  int lockedCount = 0;
  int engagedCount = 0;
  for (int i = 0; i < numCuffs; i++) {
    if (digitalRead(relayPins[i]) == HIGH) lockedCount++;
    if (cuffs[i].engaged) engagedCount++;
  }

  String summary = "Locked: " + String(lockedCount) + "/" + String(numCuffs) +
                   " | Engaged: " + String(engagedCount) + "/" + String(numCuffs) +
                   " | Status: " + String(puzzleSolved ? "SOLVED" : "ACTIVE");
  mqtt.publish("cuffs/summary", summary.c_str());
}
*/

// ==================== SERIAL COMMANDS ====================
void handleSerialCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();

  if (command == "status") {
    printDetailedStatus();
    // publishStatus(); // MQTT disabled
  } else if (command == "reset") {
    Serial.println("Manual reset triggered");
    resetPuzzle();
  } else if (command == "open all" || command == "openall") {
    openAllCuffs();
  } else if (command == "close all" || command == "closeall") {
    closeAllCuffs();
  } else if (command == "test relays" || command == "testrelays") {
    testAllRelays();
  } else if (command == "test sensors" || command == "testsensors") {
    testAllTouchSensors();
  } else if (command == "test magnets" || command == "testmagnets") {
    testAllHallSensors();
  } else if (command == "test all" || command == "testall") {
    testAllComponents();
  } else if (command == "help") {
    printHelp();
  } else if (command.startsWith("open cuff") || command.startsWith("opencuff")){
    (command.length() > 10) ? openCuff(command.substring(11).toInt()) : openCuff(command.substring(10).toInt());
  } else if (command.startsWith("close cuff") || command.startsWith("closecuff")){
    (command.length() > 11) ? openCuff(command.substring(12).toInt()) : openCuff(command.substring(11).toInt());
  } else if (command.length() > 0) {
    Serial.println("Unknown command. Type 'help' for available commands.");
  }
}

void testAllRelays() {
  Serial.println("\n=== TESTING RELAYS ===");

  for (int i = 0; i < numCuffs; i++) {
    Serial.print("Cuff ");
    Serial.print(i);
    Serial.print(": ");

    digitalWrite(relayPins[i], LOW);
    Serial.print("OPEN ");
    delay(1000);

    digitalWrite(relayPins[i], HIGH);
    Serial.println("CLOSE");
    delay(500);
  }

  Serial.println("Relay test complete\n");
}

void testAllTouchSensors() {
  Serial.println("\n=== TESTING TOUCH SENSORS ===");
  Serial.println("Touch each sensor (30s timeout)");
  Serial.println("CUFF | STATUS");
  Serial.println("-----|-------");

  unsigned long startTime = millis();
  bool sensorTested[numCuffs] = {false};

  while (millis() - startTime < 30000) {
    for (int i = 0; i < numCuffs; i++) {
      bool touched = digitalRead(touchPins[i]) == HIGH;
      if (touched && !sensorTested[i]) {
        Serial.print("  ");
        Serial.print(i);
        Serial.println("  | WORKING");
        sensorTested[i] = true;
      }
    }

    bool allTested = true;
    for (int i = 0; i < numCuffs; i++) {
      if (!sensorTested[i]) {
        allTested = false;
        break;
      }
    }

    if (allTested) {
      Serial.println("\nAll sensors tested successfully!");
      return;
    }

    delay(100);
  }

  Serial.println("\nNot tested:");
  for (int i = 0; i < numCuffs; i++) {
    if (!sensorTested[i]) {
      Serial.print("  ");
      Serial.print(i);
      Serial.println("  | NOT TESTED");
    }
  }

  Serial.println("Test complete\n");
}

void testAllHallSensors() {
  Serial.println("\n=== TESTING HALL SENSORS ===");
  Serial.println("Current status:");
  Serial.println("CUFF | MAGNET");
  Serial.println("-----|-------");

  for (int i = 0; i < numCuffs; i++) {
    Serial.print("  ");
    Serial.print(i);
    Serial.print("  |   ");
    Serial.println((hallPins[i] != -1) ? (digitalRead(hallPins[i]) == LOW ? "Y" : "N") : "DISABLED");
  }

  Serial.println("\nMonitoring for changes (15s)...");

  unsigned long startTime = millis();
  bool lastStates[numCuffs];

  for (int i = 0; i < numCuffs; i++) {
    lastStates[i] = (hallPins[i] != -1) ? (digitalRead(hallPins[i]) == LOW) : false;
  }

  while (millis() - startTime < 15000) {
    for (int i = 0; i < numCuffs; i++) {
      if (hallPins[i] == -1) continue; // Skip disabled pins

      bool currentState = digitalRead(hallPins[i]) == LOW;
      if (currentState != lastStates[i]) {
        Serial.print("Cuff ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(currentState ? "MAGNET DETECTED" : "magnet removed");
        lastStates[i] = currentState;
      }
    }
    delay(50);
  }

  Serial.println("Test complete\n");
}

void testAllComponents() {
  Serial.println("\n=== TESTING ALL COMPONENTS ===\n");

  testAllHallSensors();
  delay(1000);
  testAllTouchSensors();
  delay(1000);
  testAllRelays();

  Serial.println("=== ALL TESTS COMPLETE ===\n");
}

void printHelp() {
  Serial.println("\n=== AVAILABLE COMMANDS ===");
  Serial.println("status       - Show system status");
  Serial.println("reset        - Reset puzzle");
  Serial.println("open all     - Open all cuffs");
  Serial.println("close all    - Close all cuffs");
  Serial.println("test relays  - Test all relays");
  Serial.println("test sensors - Test touch sensors");
  Serial.println("test magnets - Test hall sensors");
  Serial.println("test all     - Run all tests");
  Serial.println("help         - Show this help");
  Serial.println("===========================\n");
}
