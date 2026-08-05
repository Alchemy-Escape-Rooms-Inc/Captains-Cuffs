/* Written By Ryan Laing ("Arduino Guru")
   Alchemy Escape Room "Captain's Cuffs"
   HALL SENSOR VERSION - Arduino Mega

Puzzle: Each closed cuff independently latches (locks) itself as its hall
sensor detects the internal magnet. The puzzle unlocks when every LATCHED
cuff has its touch pad activated simultaneously.

Hardware:
- Touch sensors: Detect player touch (HIGH = touched)
- Hall sensors: Detect magnet when cuff locked (LOW = magnet detected with INPUT_PULLUP)
- Relays: Control cuff locks (HIGH = locked, LOW = released)

v1.3.0: hall/touch debouncing + per-cuff independent latching. A latched
cuff stays locked (relay HIGH) even if its hall sensor drops out mid-game;
only puzzle solve or manual reset can unlatch it.
*/


#define VERSION "1.3.0"


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


// A raw pin reading must be stable for this long before it becomes the
// "stable" value the game logic sees. Filters EMI/mechanical bounce that
// was causing false 1/1 solves when 2 cuffs were physically closed.
const unsigned long hallDebounceDelay = 150;
const unsigned long touchDebounceDelay = 50;
const unsigned long autoResetDelay = 5 * 60 * 1000UL;  // 5 minutes

// ==================== STATE VARIABLES ====================
// Raw = most recent pin read. Stable = the debounced value.
// Stable follows Raw only after (now - lastChange) > debounceDelay.
bool hallRaw[numCuffs];
bool hallStable[numCuffs];
unsigned long hallLastChange[numCuffs];

bool touchRaw[numCuffs];
bool touchStable[numCuffs];
unsigned long touchLastChange[numCuffs];

// Once a cuff's stable hall reads TRIGGERED it latches: relay goes HIGH and
// stays HIGH. Only releaseCuffs() (solve) or resetPuzzle() clears it.
bool cuffLatched[numCuffs];

bool puzzleSolved = false;
unsigned long puzzleSolvedTime = 0;
bool lastSolutionCheck = false;
bool gameBegan = false;  // fires the "Begin" MQTT signal once per game

String incoming = "";

// ==================== SETUP ====================
void setup() {
  Serial.begin(9600);
  Serial3.begin(115200);  // link to embedded ESP8266

  Serial.println("\n=== CAPTAIN'S CUFFS v1.3.0 - PER-CUFF LATCHING ===");
  Serial.println("Platform: Arduino Mega");
  Serial.println("Initializing hardware...");

  pinMode(espResetPin, INPUT_PULLUP);
  pinMode(espOpenPin, INPUT_PULLUP);
  pinMode(espClosePin, INPUT_PULLUP);

  for (int i = 0; i < numCuffs; i++) {
    pinMode(touchPins[i], INPUT);
    pinMode(hallPins[i], INPUT_PULLUP);
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);  // all cuffs start UNLOCKED

    hallRaw[i] = false;
    hallStable[i] = false;
    hallLastChange[i] = 0;
    touchRaw[i] = false;
    touchStable[i] = false;
    touchLastChange[i] = 0;
    cuffLatched[i] = false;
  }

  Serial.println("Stabilizing sensors...");
  delay(100);

  Serial.println("\n=== SYSTEM READY ===");
  Serial.println("Waiting for players to close cuffs...");
  Serial.println("Type 'help' for available commands\n");
}

// ==================== MAIN LOOP ====================
void loop() {
  receiveESPCommand();

  if (puzzleSolved && (millis() - puzzleSolvedTime >= autoResetDelay)) {
    resetPuzzle();
    return;
  }

  unsigned long now = millis();
  int latchedCount = 0;
  int touchedLatchedCount = 0;

  for (int i = 0; i < numCuffs; i++) {
    if (cuffDisabled(i)) continue;

    // ---- Debounce hall ----
    bool rawH = (digitalRead(hallPins[i]) == LOW);
    if (rawH != hallRaw[i]) {
      hallRaw[i] = rawH;
      hallLastChange[i] = now;
    }
    if (hallRaw[i] != hallStable[i] && (now - hallLastChange[i] >= hallDebounceDelay)) {
      hallStable[i] = hallRaw[i];
      onHallStateChanged(i, hallStable[i]);
    }

    // ---- Debounce touch ----
    bool rawT = (digitalRead(touchPins[i]) == HIGH);
    if (rawT != touchRaw[i]) {
      touchRaw[i] = rawT;
      touchLastChange[i] = now;
    }
    if (touchRaw[i] != touchStable[i] && (now - touchLastChange[i] >= touchDebounceDelay)) {
      touchStable[i] = touchRaw[i];
      onTouchStateChanged(i, touchStable[i]);
    }

    // ---- Latch: stable hall TRIGGERED locks this cuff, once ----
    if (hallStable[i] && !cuffLatched[i] && !puzzleSolved) {
      cuffLatched[i] = true;
      digitalWrite(relayPins[i], HIGH);
      Serial.print("Cuff "); Serial.print(i); Serial.println(" LATCHED (locked)");
      if (!gameBegan) {
        gameBegan = true;
        Serial3.println("Begin");
        Serial.println("=== GAME STARTED (first cuff latched) ===");
      }
    }

    // ---- Count for solve check: only LATCHED cuffs count ----
    if (cuffLatched[i]) {
      latchedCount++;
      if (touchStable[i]) {
        touchedLatchedCount++;
      }
    }
  }

  // ---- Solve check ----
  if (!puzzleSolved) {
    bool currentSolutionStatus = (latchedCount > 0 && touchedLatchedCount == latchedCount);
    if (currentSolutionStatus && !lastSolutionCheck) {
      Serial.print("SOLUTION: ");
      Serial.print(touchedLatchedCount);
      Serial.print("/");
      Serial.print(latchedCount);
      Serial.println(" - SOLVING PUZZLE!");
      releaseCuffs();
      Serial3.println("p:s");
    }
    lastSolutionCheck = currentSolutionStatus;
  }

  if (Serial.available()) {
    handleSerialCommand();
  }
}

void onHallStateChanged(int i, bool triggered) {
  Serial.print("Cuff "); Serial.print(i);
  Serial.print("  Hall "); Serial.print(triggered ? "TRIGGERED" : "RELEASED");
  Serial.print("  Latched: "); Serial.print(cuffLatched[i] ? "YES" : "no");
  Serial.print("  Touch: "); Serial.println(touchStable[i] ? "ACTIVE" : "off");
  Serial3.println("c" + String(i) + ":" + (triggered ? "c" : "o"));
}

void onTouchStateChanged(int i, bool active) {
  Serial.print("Cuff "); Serial.print(i);
  Serial.print("  Touch "); Serial.print(active ? "ACTIVE" : "off");
  Serial.print("  Latched: "); Serial.print(cuffLatched[i] ? "YES" : "no");
  Serial.print("  Hall: "); Serial.println(hallStable[i] ? "TRIGGERED" : "released");
  Serial3.println("s" + String(i) + ":" + (active ? "t" : "nt"));
}

bool checkForAnyClosedCuff() {
  for (int i = 0; i < numCuffs; i++)
    if (!cuffDisabled(i) && !digitalRead(hallPins[i]))
      return true;
  return false;
}


// ==================== PUZZLE LOGIC ====================
void releaseCuffs() {
  Serial.println("\n=== RELEASING CUFFS ===");
  for (int i = 0; i < numCuffs; i++) {
    if (cuffLatched[i]) {
      digitalWrite(relayPins[i], LOW);
      cuffLatched[i] = false;
      Serial.print("Cuff "); Serial.print(i); Serial.println(" RELEASED");
    }
  }
  puzzleSolved = true;
  puzzleSolvedTime = millis();
  Serial.println("*** PUZZLE SOLVED! ***");
  Serial.print("Auto-reset in ");
  Serial.print(autoResetDelay / 1000);
  Serial.println(" seconds\n");
}

void resetPuzzle() {
  Serial.println("\n=== RESETTING PUZZLE ===");
  for (int i = 0; i < numCuffs; i++) {
    if (cuffDisabled(i)) continue;
    digitalWrite(relayPins[i], LOW);
    cuffLatched[i] = false;
    hallStable[i] = false;
    touchStable[i] = false;
  }
  puzzleSolved = false;
  lastSolutionCheck = false;
  gameBegan = false;
  Serial.println("Ready. Close a cuff to start the next game.\n");
  Serial3.println("p:ns");
}


void openCuff(byte index) {
  digitalWrite(relayPins[index], LOW);
  cuffLatched[index] = false;
  Serial.print("Cuff "); Serial.print(index); Serial.println(" opened");
}

void openAllCuffs() {
  Serial.println("\n=== OPENING ALL CUFFS ===");
  for (int i = 0; i < numCuffs; i++)
    openCuff(i);
  Serial.println("All cuffs opened\n");
}


void closeCuff(byte index) {
  if (cuffDisabled(index)) {
    Serial.print("Cuff "); Serial.print(index); Serial.println(" is DISABLED - not locking");
    return;
  }
  digitalWrite(relayPins[index], HIGH);
  cuffLatched[index] = true;
  Serial.print("Cuff "); Serial.print(index); Serial.println(" closed (manually latched)");
}


void closeAllCuffs() {
  Serial.println("\n=== CLOSING ALL CUFFS ===");
  for (int i = 0; i < numCuffs; i++)
    closeCuff(i);
  puzzleSolved = false;
  lastSolutionCheck = false;
  Serial3.println("p:ns");
  Serial.println("All cuffs closed\n");
}

// ==================== STATUS DISPLAY ====================
void printDetailedStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("Version: "); Serial.println(VERSION);
  Serial.print("Puzzle solved: "); Serial.println(puzzleSolved ? "YES" : "NO");
  Serial.print("Game began: "); Serial.println(gameBegan ? "YES" : "NO");
  Serial.print("Uptime: "); Serial.print(millis() / 1000); Serial.println("s");
  if (puzzleSolved) {
    Serial.print("Reset in: ");
    Serial.print((autoResetDelay - (millis() - puzzleSolvedTime)) / 1000);
    Serial.println("s");
  }
  Serial.println();
  Serial.println("CUFF | LATCHED | HALL(stable) | TOUCH(stable) | HALL(raw)");
  Serial.println("-----|---------|--------------|---------------|----------");
  for (int i = 0; i < numCuffs; i++) {
    if (cuffDisabled(i)) {
      Serial.print("  "); Serial.print(i); Serial.println("  | DISABLED");
      continue;
    }
    Serial.print("  "); Serial.print(i); Serial.print("  |   ");
    Serial.print(cuffLatched[i] ? "YES  " : "no   "); Serial.print("  |   ");
    Serial.print(hallStable[i] ? "TRIG " : "open "); Serial.print("     |   ");
    Serial.print(touchStable[i] ? "ACTIVE " : "off    "); Serial.print("    |   ");
    Serial.println((digitalRead(hallPins[i]) == LOW) ? "trig" : "open");
  }
  Serial.println("===============================================\n");
}


// ==================== ESP COMMANDS ==================
void receiveESPCommand() {
  while (Serial3.available()) {
    char c = Serial3.read();
    if (c == '\n') {
      incoming.trim();
      handleESPCommand(incoming);
      incoming = "";
    } else {
      incoming += c;
    }
  }
}

void sendCommand(String cmd) {
  Serial3.println(cmd);
}

void handleESPCommand(String cmd) {
  if (strcmp(cmd.c_str(), "displayStatus") == 0) {
    printSensorsStatus();
  }
}

void printSensorsStatus() {
  for (int i = 0; i < 8; i++)
    Serial3.println("c" + String(i) + ":" + ((hallStable[i]) ? "c" : "o"));
  for (int i = 0; i < 8; i++)
    Serial3.println("s" + String(i) + ":" + ((touchStable[i]) ? "t" : "nt"));
}

// ==================== SERIAL COMMANDS ====================
void handleSerialCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();

  if (command == "status") {
    printDetailedStatus();
  } else if (command == "reset") {
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
  }
}

void testAllRelays() {
  Serial.println("\n=== TESTING RELAYS ===");
  for (int i = 0; i < numCuffs; i++) {
    if (cuffDisabled(i)) {
      Serial.print("Cuff "); Serial.print(i); Serial.println(": DISABLED - skipped");
      continue;
    }
    Serial.print("Cuff "); Serial.print(i); Serial.print(": ");
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
    if (cuffDisabled(i)) sensorTested[i] = true;

  while (millis() - startTime < 30000) {
    for (int i = 0; i < numCuffs; i++) {
      bool touched = digitalRead(touchPins[i]) == HIGH;
      if (touched && !sensorTested[i]) {
        Serial.print("  "); Serial.print(i); Serial.println("  | WORKING");
        sensorTested[i] = true;
      }
    }
    bool allTested = true;
    for (int i = 0; i < numCuffs; i++) {
      if (!sensorTested[i]) { allTested = false; break; }
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
      Serial.print("  "); Serial.print(i); Serial.println("  | NOT TESTED");
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
    Serial.print("  "); Serial.print(i); Serial.print("  |   ");
    Serial.println(!cuffDisabled(i) ? (digitalRead(hallPins[i]) == LOW ? "Y" : "N") : "DISABLED");
  }
  Serial.println("\nMonitoring for changes (15s)...");
  unsigned long startTime = millis();
  bool lastStates[numCuffs];
  for (int i = 0; i < numCuffs; i++) {
    lastStates[i] = (digitalRead(hallPins[i]) == LOW);
  }
  while (millis() - startTime < 15000) {
    for (int i = 0; i < numCuffs; i++) {
      if (cuffDisabled(i)) continue;
      bool currentState = digitalRead(hallPins[i]) == LOW;
      if (currentState != lastStates[i]) {
        Serial.print("Cuff "); Serial.print(i); Serial.print(": ");
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
