# Firmware changes needed for SkullVision

**I did not edit your .ino files.** These are the changes to make. Read them,
then either apply them yourself or tell me to.

There is a gap in the current firmware that this script cannot work around:
**nothing an MQTT client publishes ever reaches the Mega.** The ESP handles
every command locally. `SOLVE` sets the ESP's own `puzzleSolved` flag and
publishes it — the Mega never hears about it and the relays never move. The
only thing that crosses the serial link today is `displayStatus`.

Three changes, in priority order.

---

## 1. CRITICAL — the Mega currently hangs at boot with the cuffs removed

`CaptainsCuffsMega.ino`, `beginGame()`:

```cpp
void beginGame(){
  //game starts when any cuff is closed
    while(!checkForAnyClosedCuff());
```

With the hall sensors gone, those pins are `INPUT_PULLUP` and read HIGH
forever. `checkForAnyClosedCuff()` never returns true, so `beginGame()` never
returns, so `setup()` never finishes. The board is stuck before `loop()` ever
runs: no serial commands, no `open all`, no MQTT telemetry. It looks like a
dead board.

Fix — replace `beginGame()` with:

```cpp
// Game start no longer comes from a magnet. It comes from the GM / room
// controller over MQTT, or from the first skull going occupied.
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
```

And in `setup()`, **delete the `beginGame();` call at the end.** Let the game
be started by a command instead. Startup should never block on a sensor.

`checkForAnyClosedCuff()` can stay for now — nothing calls it once the
blocking loop is gone. Delete it when you do the real cleanup pass.

---

## 2. ESP — forward vision commands to the Mega

`CaptainsCuffsESP.ino`, inside `mqttCallback()`, add these next to the other
command handlers. Do **not** publish back to `MQTT_TOPIC_COMMAND` from here —
the board is subscribed to it and you get an infinite loop. The existing
comment in your file already warns about that.

```cpp
  if (strcmp(msg, "SKULLS_SOLVED") == 0) {
    puzzleSolved = true;
    mqttLogf("Vision: skulls solved");
    Serial.println("skullSolve");            // -> Mega over serial
    mqttClient.publish(MQTT_TOPIC_SOLVED, "true", true);
    mqttClient.publish(MQTT_TOPIC_MESSAGE, "SOLVED");
    return;
  }
  if (strcmp(msg, "GAME_START") == 0) {
    puzzleSolved = false;
    mqttLogf("Vision: game start");
    Serial.println("beginGame");             // -> Mega over serial
    mqttClient.publish(MQTT_TOPIC_SOLVED, "false", true);
    return;
  }
```

Note your existing `SOLVE` handler has the same blind spot — it never tells
the Mega either. Worth adding `Serial.println("skullSolve");` to it too so the
GM's manual `SOLVE` actually opens something.

---

## 3. Mega — act on the forwarded commands

`CaptainsCuffsMega.ino`, `handleESPCommand()` currently only knows one word:

```cpp
void handleESPCommand(String cmd){
  if(strcmp(cmd.c_str(),"displayStatus") == 0){
    printSensorsStatus();
  }
}
```

Replace with:

```cpp
void handleESPCommand(String cmd){
  if(cmd == "displayStatus"){
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
```

And add the new solve function. The old `releaseCuffs()` only drops relays
where `cuffs[i].engaged` is true — with the hall sensors gone that is always
false, so it would fire and release nothing:

```cpp
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
```

---

## What is still an open decision

`solveFromVision()` drops the eight cuff relays. **Those relays do not go
anywhere useful now** — the electromagnetic locks came off with the cuffs.

Before this is a working puzzle you have to tell me what the solve actually
fires: a door maglock, a drawer, a Sprite video cue, lighting. Whatever it is,
it either gets wired to one of those existing relay channels (fastest — the
relay board and its 12V supply are already installed and tested) or it becomes
an MQTT message to another prop.

Wiring it to relay channel 0 and leaving the other seven dark is the move I'd
make with a walkthrough this close.
