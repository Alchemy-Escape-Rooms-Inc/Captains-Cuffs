/* Written By Ryan Laing ("Arduino Guru")
   Alchemy Escape Room "Captain's Cuffs"
   HALL SENSOR VERSION - Arduino Mega

Puzzle (v1.5.0): the number of skull touch sensors held at the same moment
must EQUAL the number of players SkullVision counts in the room. Which skulls
are touched does not matter. Head-count arrives as "n<N>" over serial from the
ESP (MQTT MermaidsTale/CaptainsCuffs/system/People, published by SkullVision).

Hardware:
- Touch sensors: Detect player touch (HIGH = touched)
- Hall sensors: Detect magnet when cuff locked (LOW = magnet detected with INPUT_PULLUP)
- Relays: Control cuff locks (HIGH = locked, LOW = released)

*/


#define VERSION "1.5.0"


// ==================== CONFIGURATION ====================
const int numCuffs = 8;

// Cuffs 3, 4, 7 (MQTT topics Cuff3/Cuff4/Cuff7) are out of service — greyed
// out on the UI. Only cuffs 0, 1, 2, 5, 6 are active. A disabled cuff never
// counts toward the solve, never starts the game, and its relay is never
// energized. Set an entry to false to re-enable that cuff.
const bool disabledCuffs[numCuffs] = {false, false, false, true, true, false, false, true};

bool cuffDisabled(int i) {
  return disabledCuffs[i];
}
const int touchPins[numCuffs] = {22, 23, 24, 25, 26, 27, 28, 29};
const int hallPins[numCuffs] = {30, 31, 32, 33, 34, 35, 36, 37};
const int relayPins[numCuffs] = {38, 39, 40, 41, 42, 43, 44, 45};

const int espResetPin = 49;
const int espOpenPin = 50;
const int espClosePin = 51;


const unsigned long debounceDelay = 50;
const unsigned long autoResetDelay = 5 * 60 * 1000UL; // 5 minutes

// ==================== SKULL ZONES (SkullVision) ====================
// SkullVision watches the camera and reports (a) how many people are in the
// room and (b) which skulls have a person standing at them. The ESP forwards
// both over serial: "n<N>" = head-count, "z<N>:o"/"z<N>:e" = zone occupancy.
//
// SOLVE RULE (v1.5.0): count of touch sensors active at the same moment ==
// playerCount. ANY skulls - the zone occupancy is kept only for the serial
// log / status dump, it no longer gates the solve.
const int numZones = 5;
// Which touch sensor lives in which skull: SkullVision zone id -> index into
// touchPins[]. Cuffs 3, 4, 7 are out of service, so the five skulls map to
// touch sensors 0, 1, 2, 5, 6. Bench-confirmed 2026-09-01.
const int zoneTouchIdx[numZones] = {0, 1, 2, 5, 6};
bool zoneOccupied[numZones] = {false, false, false, false, false};
// Players in the room per SkullVision (debounced there). 0 = unknown / empty
// room = nothing to solve. Not cleared by PUZZLE_RESET - it is live camera
// state, and SkullVision re-sends it every minute anyway.
int playerCount = 0;

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

String incoming = "";
// ==================== SETUP ====================
void setup() {
  Serial.begin(9600);
  Serial3.begin(115200);  //communication between the Arduino Mega and the ESP8266
  while (!Serial) { ; }   //loop until serial communication is established with a host.

  Serial.println("\n=== CAPTAIN'S CUFFS - HALL SENSOR VERSION ===");
  Serial.println("Platform: Arduino Mega");
  Serial.println("Initializing hardware...");

  //Initialize esp pins and state
  pinMode(espResetPin,INPUT_PULLUP);
  pinMode(espOpenPin,INPUT_PULLUP);
  pinMode(espClosePin,INPUT_PULLUP);

  // Initialize pins and state
  for (int i = 0; i < numCuffs; i++) {
    pinMode(touchPins[i], INPUT);

    // Skip disabled hall sensor pins
    if (hallPins[i] != -1) {
      pinMode(hallPins[i], INPUT_PULLUP); // Pull-up to prevent floating pins
    }

    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW); // Lock all cuffs initially

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

  Serial.println("\n=== SYSTEM READY ===");
  Serial.println("Monitoring for state changes...");
  Serial.println("Type 'help' for available commands\n");

  // beginGame() is no longer called here. With the hall sensors removed it
  // blocked forever waiting for a closed cuff and the board never reached
  // loop(). The game now starts on command (MQTT GAME_START via the ESP).
}

// ==================== MAIN LOOP ====================
void loop() {

  receiveESPCommand();
  // Handle auto-reset after puzzle solved
  if (puzzleSolved && (millis() - puzzleSolvedTime >= autoResetDelay)) {
    resetPuzzle();
    return;
  }

  // NOTE: sensors are scanned even after a solve — guests opening their
  // cuffs post-solve must still be reported, or MQTT shows them locked.
  // Only the win-check below is skipped while puzzleSolved.

  int activeCuffs = 0;
  int activeTouches = 0;
  bool stateChanged = false;

  // Read all sensors and detect changes
  for (int i = 0; i < numCuffs; i++) {
    if (cuffDisabled(i)) {
      continue;
    }

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

      //MQTT stuff
      Serial3.println("c" + String(i) + ":" + ((magnetDetected) ? "c":"o"));

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

      //MQTT stuff
      Serial3.println("s" + String(i) + ":" + ((currentTouch) ? "t":"nt"));
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

  // Check solution (only while unsolved; only print when status changes).
  // Solve = the number of skull touch sensors held right now equals the
  // number of players SkullVision counts. Any skulls. More touches than
  // players does NOT solve (two hands on two skulls with one player = no).
  // playerCount 0 = camera sees nobody / has not reported = nothing to solve.
  if (!puzzleSolved) {
    int touchedSkulls = 0;
    for (int z = 0; z < numZones; z++) {
      int t = zoneTouchIdx[z];
      if (!cuffDisabled(t) && cuffs[t].touched) touchedSkulls++;
    }
    bool currentSolutionStatus = (playerCount > 0 && touchedSkulls == playerCount);

    if (currentSolutionStatus && !lastSolutionCheck) {
      Serial.print("SOLUTION: ");
      Serial.print(touchedSkulls);
      Serial.print(" skulls touched for ");
      Serial.print(playerCount);
      Serial.println(" players - SOLVING PUZZLE!");
      solveFromVision();   // releases relays, sets puzzleSolved, sends p:s
      stateChanged = true;
    }

    lastSolutionCheck = currentSolutionStatus;
  }

  // Publish status on any state change (disabled)
  // if (stateChanged) {
  //   publishStatus();
  // }

  // Handle serial commands
  if (Serial.available()) {
    handleSerialCommand();
  }
}

void printSensorsStatus(){
  //cuffs status
  for(int i = 0; i < 8; i++)
    Serial3.println("c" + String(i) + ":" + ((lastCuffStates[i]) ? "c":"o"));
  //touchsensors status
  for(int i = 0; i < 8; i++)
    Serial3.println("s" + String(i) + ":" + ((lastTouchStates[i]) ? "t":"nt"));
}

bool checkForAnyClosedCuff(){
  for(int i = 0; i < 8; i++)
    if(!cuffDisabled(i) && !digitalRead(hallPins[i]))
      return true;
  return false;
}

// Game start no longer comes from a magnet. It comes from the GM / room
// controller over MQTT (GAME_START -> ESP -> "beginGame" over serial).
bool gameRunning = false;

void beginGame(){
  if (gameRunning) return;
  for(int i = 0; i < numCuffs; i++)
    if(!cuffDisabled(i))
      digitalWrite(relayPins[i], HIGH);
  gameRunning = true;
  Serial.println("Beginning the game.");
  Serial3.println("Begin");
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
    if (cuffDisabled(i)) continue;
    digitalWrite(relayPins[i], HIGH);
    cuffs[i].released = false;
  }

  puzzleSolved = false;
  lastSolutionCheck = false;
  gameRunning = false;   // allow the next GAME_START to run beginGame again
  Serial.println("Ready for next players\n");
  Serial3.println("p:ns");

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
  if (cuffDisabled(index)) {
    Serial.print("Cuff ");
    Serial.print(index);
    Serial.println(" is DISABLED - not locking");
    return;
  }
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
  lastSolutionCheck = false;
  Serial3.println("p:ns");
  Serial.println("All cuffs closed\n");
  // publishStatus(); // MQTT disabled
  // mqtt.publish("cuffs/status", "all_locked"); // MQTT disabled
}
// ==================== STATUS DISPLAY ====================
void printDetailedStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("Puzzle solved: ");
  Serial.println(puzzleSolved ? "YES" : "NO");
  Serial.print("Players (SkullVision): ");
  Serial.print(playerCount);
  Serial.println(" -> solve = that many skulls touched at once");
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

    if (cuffDisabled(i)) {
      Serial.println("DISABLED   |   DISABLED    |   DISABLED");
      continue;
    }

    Serial.print(isLocked ? "LOCKED   " : "UNLOCKED ");
    Serial.print(" |   ");
    Serial.print(isTouched ? "ACTIVE      " : "NOT ACTIVE  ");
    Serial.print(" |   ");
    Serial.println(magnetDetected ? "CLOSED" : "OPEN  ");
  }
  Serial.println("===============================================\n");
}









/*
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
// ==================== ESP COMMANDS ==================
void receiveESPCommand(){
  while(Serial3.available()){
    char c = Serial3.read();
    if(c == '\n') {
      incoming.trim();
      handleESPCommand(incoming);
      incoming = "";
    } else {
      incoming += c;
    }
  }
}

void sendCommand(String cmd){
  Serial3.println(cmd);
}

void handleESPCommand(String cmd){
  if(cmd.length() >= 2 && cmd.charAt(0) == 'n' && isDigit(cmd.charAt(1))){
    // Head-count from SkullVision via ESP: "n<N>" = solve target
    int n = cmd.substring(1).toInt();
    if(n >= 0 && n <= 9 && n != playerCount){
      playerCount = n;
      Serial.print("Players: ");
      Serial.print(playerCount);
      Serial.println(" (solve = that many skulls touched at once)");
    }
  }
  else if(cmd.length() >= 4 && cmd.charAt(0) == 'z' && cmd.indexOf(':') > 1){
    // Zone occupancy from SkullVision via ESP: "z<N>:o" / "z<N>:e"
    int sep = cmd.indexOf(':');
    int zone = cmd.substring(1, sep).toInt();
    char state = cmd.charAt(sep + 1);
    if(zone >= 0 && zone < numZones){
      zoneOccupied[zone] = (state == 'o');
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(zoneOccupied[zone] ? " OCCUPIED" : " empty");
    }
  }
  else if(cmd == "displayStatus"){
    printSensorsStatus();
  }
  else if(cmd == "beginGame"){
    beginGame();
  }
  else if(cmd == "skullSolve"){
    solveFromVision();
  }
  else if(cmd == "puzzleReset"){
    resetPuzzle();
  }
  else if(cmd == "openAll"){
    openAllCuffs();
  }
}

// Solve triggered by SkullVision (or the GM's manual SOLVE) arriving over
// MQTT -> ESP -> serial. releaseCuffs() only drops relays where a hall
// sensor saw a magnet; with the hall sensors removed that is never, so the
// vision solve needs its own release that ignores 'engaged'.
void solveFromVision(){
  Serial.println("\n=== SOLVE FROM VISION ===");
  for (int i = 0; i < numCuffs; i++) {
    if (cuffDisabled(i)) continue;
    digitalWrite(relayPins[i], LOW);
    cuffs[i].released = true;
  }
  puzzleSolved = true;
  puzzleSolvedTime = millis();
  Serial3.println("p:s");
  Serial.println("*** PUZZLE SOLVED (vision) ***");
}
// ==================== SERIAL COMMANDS ====================
void handleSerialCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();

  if (command == "status") {
    printDetailedStatus();
    // publishStatus(); // MQTT disabled
  } else if (command == "reset") {
    resetPuzzle();
  }
  else if (command == "open all" || command == "openall") {
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
  }

}

void testAllRelays() {
  Serial.println("\n=== TESTING RELAYS ===");

  for (int i = 0; i < numCuffs; i++) {
    if (cuffDisabled(i)) {
      Serial.print("Cuff ");
      Serial.print(i);
      Serial.println(": DISABLED - skipped");
      continue;
    }

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
  for (int i = 0; i < numCuffs; i++)
    if (cuffDisabled(i)) sensorTested[i] = true; // don't wait on disabled cuffs

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
    Serial.println((!cuffDisabled(i) && hallPins[i] != -1) ? (digitalRead(hallPins[i]) == LOW ? "Y" : "N") : "DISABLED");
  }

  Serial.println("\nMonitoring for changes (15s)...");

  unsigned long startTime = millis();
  bool lastStates[numCuffs];

  for (int i = 0; i < numCuffs; i++) {
    lastStates[i] = (hallPins[i] != -1) ? (digitalRead(hallPins[i]) == LOW) : false;
  }

  while (millis() - startTime < 15000) {
    for (int i = 0; i < numCuffs; i++) {
      if (cuffDisabled(i) || hallPins[i] == -1) continue; // Skip disabled pins

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
