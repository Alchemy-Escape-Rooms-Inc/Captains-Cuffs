# Captain's Cuffs - Troubleshooting Guide

## Quick Reference Emergency Procedures

### EMERGENCY: Player in Distress
**Immediate Actions:**
1. Open Serial Monitor
2. Type: `open all` and press Enter
3. All cuffs release immediately
4. Physical key backup if electronics fail

### EMERGENCY: Power Failure
**Response:**
- Electromagnetic locks are FAIL-SAFE
- Power loss = automatic unlock
- All cuffs release when power disconnected

### EMERGENCY: System Frozen
**Response:**
1. Disconnect power supply (pull plug)
2. Wait 10 seconds
3. Reconnect power
4. System reboots and locks all cuffs
5. Use `open all` to release players

---

## Diagnostic Flowcharts

### Problem: Cuff Won't Unlock

```
Cuff Won't Unlock
       │
       ├─→ [Check Serial Monitor]
       │   └─→ Shows "RELEASED"?
       │       ├─→ YES: Hardware issue
       │       │   └─→ Go to "Lock Mechanical Issue"
       │       └─→ NO: Software/sensor issue
       │           └─→ Go to "Puzzle Not Solving"
       │
       └─→ [Manual Test]
           └─→ Type: "open all"
               ├─→ Works: Puzzle logic issue
               │   └─→ Check hall/touch sensors
               └─→ Doesn't work: Relay/lock issue
                   └─→ Check relay operation
```

### Problem: Touch Sensor Not Working

```
Touch Sensor Not Responding
       │
       ├─→ [Visual Check]
       │   └─→ LED on sensor board lights when touched?
       │       ├─→ YES: Wiring issue
       │       │   └─→ Check signal wire to Arduino
       │       └─→ NO: Sensor power issue
       │           └─→ Check 5V and GND connections
       │
       ├─→ [Serial Monitor Test]
       │   └─→ Type: "test sensors"
       │       └─→ Touch sensor and watch for "WORKING"
       │           ├─→ Detected: Software issue
       │           └─→ Not detected: Hardware issue
       │
       └─→ [Multimeter Test]
           └─→ Measure voltage on signal pin
               ├─→ Changes HIGH when touched: Software issue
               └─→ Stays LOW: Sensor or wiring problem
```

### Problem: Hall Sensor Not Working

```
Hall Sensor Not Detecting
       │
       ├─→ [Magnet Position]
       │   └─→ Magnet within 10mm of sensor?
       │       ├─→ NO: Reposition magnet
       │       └─→ YES: Continue diagnosis
       │
       ├─→ [Serial Monitor Test]
       │   └─→ Type: "test magnets"
       │       └─→ Shows "TRIGGERED" with magnet present?
       │           ├─→ YES: Mounting/alignment issue
       │           └─→ NO: Sensor or wiring issue
       │
       ├─→ [Multimeter Test]
       │   └─→ Measure voltage on sensor output
       │       ├─→ No magnet: ~5V (pulled HIGH)
       │       ├─→ With magnet: ~0V (pulled LOW)
       │       └─→ No change: Sensor failure or wrong polarity
       │
       └─→ [Swap Test]
           └─→ Swap sensor with known working unit
               └─→ Determines if sensor or wiring is bad
```

---

## Common Problems and Solutions

### Puzzle Logic Issues

#### Problem: Puzzle Solves Too Early
**Symptoms:**
- Cuffs release without all players touching sensors
- Release happens with only partial touch

**Diagnosis:**
1. Check Serial Monitor output
2. Look for false touch sensor triggers
3. Type `status` repeatedly and watch for noise

**Causes:**
1. **Touch Sensor Noise**
   - Wire routing near high-voltage lines
   - Electromagnetic interference from relays
   - Poor grounding

2. **Debounce Timing Too Short**
   - Default 50ms may not filter all noise
   - Need longer debounce period

**Solutions:**
1. **Improve Wiring**
   ```
   - Separate touch sensor wires from relay wires
   - Use twisted pair for touch sensors
   - Add ferrite beads to touch sensor wires
   - Ensure star ground topology
   ```

2. **Adjust Debounce** (in code, line 23)
   ```cpp
   // Change from:
   const unsigned long debounceDelay = 50;

   // To:
   const unsigned long debounceDelay = 100;  // or 150, 200
   ```

3. **Add Software Filtering**
   ```cpp
   // Require touch to be stable for longer period
   // Contact support for code modification
   ```

4. **Add Hardware Filtering**
   ```
   - Add 0.1μF capacitor between touch sensor OUT and GND
   - Add 10kΩ resistor in series with signal line
   ```

#### Problem: Puzzle Won't Solve
**Symptoms:**
- All players touching sensors
- No release occurs
- No "SOLVING PUZZLE!" message

**Diagnosis:**
1. Type `status` while all players are touching
2. Check which sensors show "ACTIVE"
3. Check which sensors show "TRIGGERED" (hall sensors)

**Common Causes:**
1. **Not All Cuffs Engaged**
   - Hall sensor not detecting magnet
   - Cuff not fully closed
   - Solution: Check hall sensor alignment

2. **Touch Not Registered**
   - Touch sensor malfunction
   - Insufficient contact
   - Solution: Check touch sensor power and operation

3. **Timing Issue**
   - Players not touching simultaneously
   - Debounce delay causing miss
   - Solution: Hold touch longer (>200ms)

**Solutions:**
1. **Verify Hall Sensors**
   ```
   Serial Command: test magnets

   Expected: All engaged cuffs show "TRIGGERED"
   If not: Check magnet position, hall sensor wiring
   ```

2. **Verify Touch Sensors**
   ```
   Serial Command: test sensors

   Expected: All sensors respond to touch
   If not: Check touch sensor power, signal wiring
   ```

3. **Check Simultaneous Touch**
   ```
   - All engaged cuffs must be touched at same time
   - Touch must be held for >50ms (debounce period)
   - Monitor Serial output to see real-time sensor states
   ```

4. **Reduce Required Cuffs** (for testing)
   ```cpp
   // In loop(), add debug output:
   Serial.print("Active Cuffs: ");
   Serial.print(activeCuffs);
   Serial.print(" | Active Touches: ");
   Serial.println(activeTouches);
   ```

### Hardware Issues

#### Problem: Relay Clicking Constantly
**Symptoms:**
- Continuous clicking sound from relay module
- Lock engaging/disengaging rapidly
- Erratic behavior

**Causes:**
1. **Insufficient Power**
   - Power supply can't provide enough current
   - Voltage droops when relay activates
   - Relay turns off, voltage recovers, cycle repeats

2. **Loose Wiring**
   - Intermittent connection
   - Wire making/breaking contact

3. **Control Signal Noise**
   - Arduino pin floating
   - Noise on control line

**Solutions:**
1. **Upgrade Power Supply**
   ```
   Minimum requirements:
   - 5V @ 3A (Arduino + sensors + relays)
   - 12V @ 2.5A (8 locks @ 250mA each)

   Recommended:
   - 5V @ 5A (overhead for startup current)
   - 12V @ 3A (overhead)
   ```

2. **Check Wiring**
   ```
   - Tug test all Dupont connectors
   - Check for broken wires (continuity test)
   - Re-crimp any suspect connections
   - Add strain relief at connectors
   ```

3. **Add Bypass Capacitors**
   ```
   - 100μF electrolytic across 5V and GND at relay module
   - 0.1μF ceramic across each relay coil
   - Reduces voltage spikes
   ```

4. **Software Fix**
   ```cpp
   // Disable specific relay if needed (emergency)
   // Comment out relay control line in code
   ```

#### Problem: Electromagnetic Lock Won't Engage
**Symptoms:**
- Relay clicks ON
- Lock doesn't hold
- Cuff opens easily when should be locked

**Diagnosis:**
1. **Check Voltage at Lock**
   ```
   With relay ON:
   - Measure voltage across lock terminals
   - Should be 12V ±0.5V
   - If less than 11V: power supply issue
   ```

2. **Check Current Draw**
   ```
   - Use multimeter in series with lock
   - Should draw ~250mA
   - If much less: lock coil damaged
   - If much more: short circuit in lock
   ```

3. **Mechanical Test**
   ```
   - Apply 12V directly to lock (bench test)
   - Should have strong magnetic pull
   - If weak/no pull: replace lock
   ```

**Causes & Solutions:**
1. **Alignment Issue**
   - Strike plate not contacting lock face
   - **Solution:** Adjust strike plate position (±1mm)

2. **Insufficient Voltage**
   - Voltage drop in wiring
   - **Solution:** Use 18 AWG wire for lock power, shorter runs

3. **Lock Failure**
   - Internal coil damaged
   - **Solution:** Replace electromagnetic lock

4. **Relay Contact Issue**
   - Contact burnt/pitted
   - High resistance connection
   - **Solution:** Replace relay module

#### Problem: Touch Sensor Too Sensitive (False Triggers)
**Symptoms:**
- Touch sensor activates without being touched
- Shows "ACTIVE" randomly
- Puzzle solves unexpectedly

**Diagnosis:**
```
1. Disconnect touch sensor from Arduino
2. Monitor sensor LED (on sensor board)
3. If LED lights without touch: sensor or environmental issue
4. If LED stays off: wiring issue causing false signals
```

**Causes:**
1. **Electromagnetic Interference**
   - Touch sensor wire near relay wires
   - AC power lines nearby
   - Mobile phones/radios

2. **Static Electricity**
   - Dry environment
   - Synthetic materials rubbing

3. **Sensor Calibration**
   - Sensitivity set too high
   - Detection threshold too low

**Solutions:**
1. **Improve Wire Routing**
   ```
   - Separate touch sensor wires from relay/lock wires (>2 inches)
   - Use twisted pair or shielded cable
   - Add ferrite beads at both ends
   ```

2. **Add Shielding**
   ```
   - Wrap touch sensor wire in aluminum foil
   - Connect foil to GND
   - Ensure no contact with signal conductor
   ```

3. **Adjust Sensitivity** (Hardware)
   ```
   Most TTP223 modules have sensitivity adjustment:
   - Look for "AHLB" solder jumper
   - Modify onboard capacitor (advanced)
   - Consult sensor datasheet
   ```

4. **Adjust Sensitivity** (Software)
   ```cpp
   // Increase debounce delay (line 23):
   const unsigned long debounceDelay = 150;  // was 50

   // Add requirement for sustained touch:
   // Contact support for code modification
   ```

5. **Environmental Control**
   ```
   - Increase humidity (40-60% RH)
   - Use anti-static spray on synthetic materials
   - Ground any metal enclosures
   ```

#### Problem: Touch Sensor Not Sensitive Enough
**Symptoms:**
- Must press very hard to register
- Touch sometimes not detected
- Inconsistent response

**Diagnosis:**
```
1. Check sensor LED lights when touched
2. Measure voltage on OUT pin (should go HIGH)
3. Test sensor directly (bypass Arduino)
```

**Causes:**
1. **Barrier Too Thick**
   - Touch sensor behind >5mm plastic
   - Non-conductive barrier

2. **Poor Ground Connection**
   - Sensor ground not connected
   - High resistance ground path

3. **Sensor Calibration**
   - Sensitivity set too low
   - Detection threshold too high

**Solutions:**
1. **Reduce Barrier**
   ```
   - Maximum plastic thickness: 3mm
   - Use thinner materials
   - Ensure no air gap between sensor and surface
   ```

2. **Increase Touch Pad Area**
   ```
   - Add copper foil to extend touch area
   - Solder wire from sensor PAD pin to foil
   - Insulate with thin plastic film
   ```

3. **Check Grounding**
   ```
   - Verify continuity from sensor GND to Arduino GND
   - Ensure low resistance (<1Ω)
   - Use star ground topology
   ```

4. **Adjust Sensitivity** (Hardware)
   ```
   - Modify TTP223 sensitivity
   - Check for onboard jumper/resistor
   - Increase detection capacitor value
   ```

#### Problem: Hall Sensor Always Shows "TRIGGERED"
**Symptoms:**
- Hall sensor detects magnet even when far away
- Shows "TRIGGERED" constantly
- Never shows "NOT TRIGGERED"

**Diagnosis:**
```
1. Remove magnet completely (>1 meter away)
2. Check Serial Monitor: status
3. If still "TRIGGERED": wiring or sensor issue
4. Measure voltage on hall sensor OUT pin:
   - Should be ~5V without magnet (pulled HIGH)
   - If ~0V: sensor stuck ON or wiring issue
```

**Causes:**
1. **Magnet Stuck Nearby**
   - Magnet not removed
   - Hidden magnet in structure

2. **Wiring Issue**
   - OUT pin shorted to GND
   - No pull-up resistor

3. **Wrong Sensor Type**
   - Bipolar sensor (requires alternating polarity)
   - Wrong pin configuration

4. **Sensor Damage**
   - Hall sensor failed internally
   - Permanently triggered

**Solutions:**
1. **Verify Magnet Removed**
   ```
   - Move magnet >1 meter away
   - Check for other magnets in area
   - Use compass to detect magnetic fields
   ```

2. **Check Wiring**
   ```
   - Verify OUTPUT_PULLUP enabled in code
   - Check wire continuity
   - Ensure no shorts to GND
   ```

3. **Verify Sensor Type**
   ```
   A3144 Pinout (correct):
   Pin 1: VCC (5V)
   Pin 2: GND
   Pin 3: OUT (active-low)

   - Verify sensor part number matches
   - Check pinout against datasheet
   ```

4. **Replace Sensor**
   ```
   - Swap with known good sensor
   - If problem moves: wiring issue
   - If problem stays: sensor damaged
   ```

5. **Code Check**
   ```cpp
   // Verify pin mode set correctly (setup function):
   if (hallPins[i] != -1) {
     pinMode(hallPins[i], INPUT_PULLUP);  // Must have INPUT_PULLUP
   }

   // Verify read logic (loop function):
   bool magnetDetected = (digitalRead(hallPins[i]) == LOW);  // Active-low
   ```

#### Problem: Hall Sensor Never Triggers
**Symptoms:**
- Hall sensor never detects magnet
- Always shows "NOT TRIGGERED"
- Shows "OPEN" even with magnet present

**Diagnosis:**
```
1. Place magnet directly on sensor surface
2. Check Serial Monitor: status
3. If still not triggered: wiring or sensor issue
4. Measure voltage on hall sensor OUT pin:
   - Without magnet: ~5V (HIGH)
   - With magnet: ~0V (LOW)
   - If no change: sensor or magnet issue
```

**Causes:**
1. **Magnet Too Far**
   - Distance >10mm
   - Weak magnet

2. **Wrong Polarity**
   - Unipolar sensor requires specific pole
   - Magnet flipped wrong direction

3. **Sensor Not Powered**
   - No 5V at VCC pin
   - GND not connected

4. **Wiring Issue**
   - OUT pin not connected to Arduino
   - Wrong Arduino pin

**Solutions:**
1. **Reposition Magnet**
   ```
   - Move magnet within 5mm of sensor
   - Test different positions
   - Optimal: Directly over sensor IC
   ```

2. **Check Magnet Polarity**
   ```
   - A3144 is unipolar (south pole only)
   - Flip magnet 180° and test again
   - Mark correct orientation on magnet
   ```

3. **Verify Power**
   ```
   Using multimeter:
   - VCC pin: Should be 5V ±0.25V
   - GND pin: Should be 0V (continuity to Arduino GND)
   - If not: check power wiring
   ```

4. **Check Signal Wiring**
   ```
   - Verify OUT pin connected to correct Arduino pin
   - Check pin mapping in code:
     hallPins[0] = 30
     hallPins[1] = 31
     etc.
   - Continuity test from sensor OUT to Arduino pin
   ```

5. **Test Sensor** (Bench Test)
   ```
   1. Connect sensor to breadboard:
      - Pin 1 (VCC) → 5V
      - Pin 2 (GND) → GND
      - Pin 3 (OUT) → LED + 1kΩ resistor → GND
   2. LED should be ON (sensor output HIGH)
   3. Move magnet close to sensor
   4. LED should turn OFF (sensor output LOW)
   5. If no change: replace sensor
   ```

6. **Upgrade Magnet**
   ```
   - Use stronger neodymium magnet (N52 grade)
   - Larger magnet (12mm diameter vs 10mm)
   - Stack multiple magnets
   ```

### System-Wide Issues

#### Problem: Arduino Not Responding
**Symptoms:**
- No Serial Monitor output
- Commands not accepted
- No "SYSTEM READY" message on boot

**Diagnosis:**
```
1. Check Power LED on Arduino (should be ON)
2. Check TX/RX LEDs during communication (should flash)
3. Try uploading simple Blink sketch
```

**Causes & Solutions:**
1. **USB Connection Issue**
   ```
   - Try different USB cable (data-capable, not just power)
   - Try different USB port on computer
   - Check for driver issues (Windows Device Manager)
   ```

2. **Wrong COM Port Selected**
   ```
   Arduino IDE:
   - Tools → Port → Select correct COM port
   - Look for "Arduino Mega" in port description
   ```

3. **Wrong Baud Rate**
   ```
   Serial Monitor:
   - Set to 9600 baud (bottom right)
   - Must match code: Serial.begin(9600)
   ```

4. **Code Stuck in Loop**
   ```
   - Arduino may be running but stuck
   - Re-upload code to restart
   - Check for infinite loops in custom code
   ```

5. **Arduino Damaged**
   ```
   - Test with known-good Arduino
   - Check for burnt components
   - Verify voltage regulator output (5V)
   ```

#### Problem: Random Resets
**Symptoms:**
- Arduino reboots unexpectedly
- "SYSTEM READY" message appears repeatedly
- Lose puzzle state

**Causes:**
1. **Power Supply Inadequate**
   - Voltage droops when relays activate
   - Arduino brownout reset triggered

2. **EMI from Relays**
   - Relay coil generates voltage spike when de-energized
   - Spike causes microcontroller reset

3. **Watchdog Timer**
   - Long-running function blocks watchdog reset
   - System automatically reboots

**Solutions:**
1. **Stabilize Power**
   ```
   - Upgrade power supply (higher current capacity)
   - Add 470μF capacitor across Arduino Vin and GND
   - Use separate 5V regulator for Arduino
   ```

2. **Suppress EMI**
   ```
   - Add flyback diode across each relay coil
   - Add 0.1μF capacitor across relay contacts
   - Keep relay module away from Arduino
   ```

3. **Improve Grounding**
   ```
   - Star ground topology (all grounds to one point)
   - Separate analog and digital grounds if possible
   - Use wide ground traces/wires
   ```

#### Problem: System Slow to Respond
**Symptoms:**
- Delay between touch and release
- Serial commands lag
- Status updates slow

**Causes:**
1. **Delay() Blocking Code**
   - Using delay() stops all processing
   - System can't respond during delay

2. **Serial Print Overhead**
   - Too much data printed to Serial
   - Serial transmission takes time

3. **Sensor Reading Delays**
   - Polling sensors too slowly
   - Debounce delay too long

**Solutions:**
1. **Optimize Code**
   ```cpp
   - Replace delay() with millis() timing
   - Reduce Serial.print() frequency
   - Only print on state changes
   ```

2. **Reduce Debounce**
   ```cpp
   // Current value (line 23):
   const unsigned long debounceDelay = 50;  // milliseconds

   // If no noise issues, can reduce to:
   const unsigned long debounceDelay = 20;  // faster response
   ```

3. **Increase Baud Rate** (Advanced)
   ```cpp
   // In setup():
   Serial.begin(115200);  // instead of 9600

   // Note: Update Serial Monitor baud rate to match
   ```

---

## Testing Sequences

### Quick Pre-Game Test (2 minutes)
```
Purpose: Verify system functional before players arrive

1. Power on system
   ✓ Wait for "SYSTEM READY" message

2. Type: status
   ✓ All 8 cuffs show LOCKED
   ✓ No sensors show false triggers

3. Type: test relays
   ✓ Hear 8 distinct clicks
   ✓ Visual: see locks disengage/engage

4. Engage 2-3 cuffs (close around fixed object)
   ✓ Hall sensors show TRIGGERED
   ✓ Touch sensors respond when touched

5. Type: open all
   ✓ All cuffs release

6. Type: close all
   ✓ All cuffs lock

Result: System ready for game
```

### Comprehensive Test (15 minutes)
```
Purpose: Full system validation

1. Power Test
   ├─ Measure 5V rail: 4.9-5.1V
   ├─ Measure 12V rail: 11.8-12.2V
   └─ Check under load (all relays ON)

2. Sensor Test
   ├─ Type: test sensors (30s)
   │  └─ Touch all 8 sensors, verify detection
   ├─ Type: test magnets (15s)
   │  └─ Move magnets, verify detection
   └─ Type: test relays
      └─ Listen for 8 clean clicks

3. Puzzle Logic Test
   ├─ Close 4 cuffs (simulate 2 players)
   ├─ Touch 3 of 4 (should NOT solve)
   ├─ Touch all 4 simultaneously
   ├─ Verify: All 4 release
   └─ Wait 5 minutes → verify auto-reset

4. Emergency Test
   ├─ Engage all cuffs
   ├─ Type: open all
   └─ Verify: Immediate release

5. Documentation
   └─ Log any issues or anomalies

Result: Full confidence in system
```

### Bench Test (New Hardware)
```
Purpose: Test individual components before installation

1. Arduino Test
   ├─ Upload Blink sketch
   ├─ Verify LED blinks
   └─ Test Serial communication

2. Sensor Test (Each)
   ├─ Connect to breadboard
   ├─ Verify power (5V, GND)
   ├─ Test output with LED
   └─ Mark working sensors

3. Relay Test (Each Channel)
   ├─ Connect relay module
   ├─ Test each channel individually
   ├─ Listen for click
   ├─ Measure COM-NO resistance (should be ~0Ω when ON)
   └─ Mark working channels

4. Lock Test (Each)
   ├─ Apply 12V directly
   ├─ Verify strong magnetic pull
   ├─ Measure current (should be ~250mA)
   └─ Mark working locks

5. Integration Test
   ├─ Wire one complete cuff (all components)
   ├─ Upload main code
   ├─ Test cuff 0 operation
   └─ Verify: Hall sensor, Touch sensor, Relay, Lock

Result: Confidence in hardware before installation
```

---

## Maintenance Procedures

### Daily Checklist (5 minutes)
```
Before each game session:

□ Visual inspection
  - Check for loose wires
  - Verify nothing disconnected
  - Look for damage to 3D printed parts

□ Power check
  - System boots successfully
  - "SYSTEM READY" message appears

□ Quick function test
  - Type: status
  - All cuffs show expected state

□ Emergency unlock test
  - Type: open all
  - Verify immediate response

□ Lock all cuffs for game
  - Type: close all
  - Ready for players
```

### Weekly Maintenance (15 minutes)
```
End of week:

□ Deep cleaning
  - Clean touch sensor surfaces (isopropyl alcohol)
  - Remove dust from electronics
  - Wipe down cuff exteriors

□ Mechanical inspection
  - Check for cracks in 3D printed parts
  - Verify lock alignment
  - Tighten any loose screws
  - Test hinge operation

□ Electrical inspection
  - Check for burnt relay contacts
  - Look for discolored wires (overheating)
  - Verify all connectors firmly seated

□ Performance test
  - Run "test all" sequence
  - Document any issues
  - Compare to baseline performance

□ Update maintenance log
  - Record test results
  - Note any anomalies
  - Schedule repairs if needed
```

### Monthly Maintenance (1 hour)
```
End of month:

□ Complete disassembly
  - Power down system
  - Disconnect all cuffs
  - Open control box

□ Component inspection
  - Visual inspection of Arduino
  - Check all solder joints
  - Inspect relay module closely
  - Verify lock condition

□ Electrical testing
  - Measure power supply voltages
  - Check current draw under load
  - Test relay contact resistance
  - Verify lock current draw

□ Software check
  - Re-upload code (ensure no corruption)
  - Verify configuration matches hardware
  - Check for any available updates

□ Performance baseline
  - Run comprehensive test
  - Document all measurements
  - Compare to previous month
  - Track degradation trends

□ Preventive replacements
  - Consider replacing high-wear items:
    * Touch sensors (if >10,000 activations)
    * Relay module (if >50,000 cycles)
    * Electromagnetic locks (if >10,000 cycles)

□ Update documentation
  - Maintenance log
  - Issue tracking
  - Performance metrics
```

---

## Error Messages and Codes

### Serial Monitor Messages

| Message | Meaning | Action |
|---------|---------|--------|
| `=== SYSTEM READY ===` | Normal boot successful | None - system operational |
| `SOLUTION: X/Y - SOLVING PUZZLE!` | Puzzle solved (X touches on Y engaged cuffs) | None - normal operation |
| `*** PUZZLE SOLVED! ***` | All engaged cuffs released | Wait for auto-reset or manual reset |
| `=== RESETTING PUZZLE ===` | System returning to ready state | None - normal operation |
| `Unknown command` | Serial command not recognized | Type `help` for valid commands |
| No message on boot | Arduino not starting | Check power, re-upload code |

### Status Command Output Interpretation

```
Example Output:
CUFF | LOCK STATUS | TOUCH SENSOR | HALL SENSOR
-----|-------------|--------------|-------------
  0  |   LOCKED    |   ACTIVE      |   CLOSED
  1  |   LOCKED    |   NOT ACTIVE  |   CLOSED
  2  |   UNLOCKED  |   NOT ACTIVE  |   OPEN
  3  |   DISABLED  |   DISABLED    |   DISABLED
```

**Interpretation:**
- **Cuff 0**: Locked, player touching, cuff closed (NORMAL ENGAGED)
- **Cuff 1**: Locked, not touched, cuff closed (WAITING FOR TOUCH)
- **Cuff 2**: Unlocked, cuff open (NOT IN USE or RELEASED)
- **Cuff 3**: Disabled in code (INTENTIONALLY OFF)

**Red Flags:**
- LOCKED but OPEN: Hall sensor not detecting, check magnet/sensor
- UNLOCKED but CLOSED: Relay stuck or lock failed
- Random ACTIVE states: Touch sensor noise, check wiring

---

## Spare Parts and Replacement

### Recommended Spare Parts

| Component | Keep on Hand | Replacement Frequency |
|-----------|--------------|----------------------|
| Touch Sensors (TTP223) | 4 | Every 6 months (preventive) |
| Hall Sensors (A3144) | 4 | As needed (rarely fail) |
| Relay Module (8-channel) | 1 | Every 12 months (preventive) |
| Electromagnetic Locks | 2 | As needed (mechanical wear) |
| Dupont Connectors | 50 | As needed (crimp failures) |
| Wire (22 AWG) | 20ft | As needed (damage) |
| Arduino Mega 2560 | 1 | Unlikely (keep for emergency) |

### Replacement Procedures

#### Replace Touch Sensor
```
1. Power down system
2. Disconnect old sensor (3 wires: VCC, GND, OUT)
3. Remove old sensor from cuff (hot glue or tape)
4. Install new sensor in same position
5. Connect wires: VCC (red), GND (black), OUT (yellow)
6. Power up system
7. Test: type "test sensors" and touch new sensor
8. Verify: Should show "WORKING"
```

#### Replace Hall Sensor
```
1. Power down system
2. Disconnect old sensor (3 wires: VCC, GND, OUT)
3. Remove old sensor from cuff
4. Install new sensor (NOTE: Orientation matters!)
5. Position sensor near magnet location (test first)
6. Connect wires: VCC (red), GND (black), OUT (green)
7. Power up system
8. Test: type "test magnets" and move magnet
9. Verify: Should show "MAGNET DETECTED" / "magnet removed"
```

#### Replace Relay Module
```
1. Power down system
2. Disconnect ALL wires from old module:
   - Control signals (IN1-IN8)
   - Power (VCC, GND)
   - Lock connections (COM, NO terminals)
3. Label ALL wires before removal!
4. Install new module
5. Reconnect control signals to correct channels:
   Pin 38 → IN1 (Cuff 0)
   Pin 39 → IN2 (Cuff 1)
   etc.
6. Reconnect power (VCC, GND)
7. Reconnect lock wires (COM, NO)
8. Power up system
9. Test: type "test relays"
10. Verify: Should hear 8 clicks
```

#### Replace Electromagnetic Lock
```
1. Power down system
2. Test new lock (apply 12V, verify pull strength)
3. Remove old lock:
   - Unscrew mounting screws (M3 x 20mm)
   - Disconnect wires from relay terminal
4. Install new lock:
   - Position exactly where old lock was
   - Verify strike plate alignment (±1mm)
   - Secure with M3 screws
5. Connect wires:
   - Red (+) to relay COM
   - Black (-) to power supply GND
6. Power up system
7. Test engagement:
   - Type "close all"
   - Verify lock holds when relay ON
   - Type "open all"
   - Verify lock releases when relay OFF
```

---

## Contact and Support

### Emergency Contact
**For player emergencies:**
1. Use `open all` command immediately
2. Have physical key backup ready
3. Call emergency services if needed (911)

### Technical Support
**For system issues:**
- Document exact symptoms
- Capture Serial Monitor output
- Note any recent changes
- Run diagnostic tests before contacting

### Code Developer
**Original Code Author:** Ryan Laing ("Arduino Guru")
**Escape Room:** Alchemy Escape Rooms

### Documentation
- README.md: General system overview
- HARDWARE_GUIDE.md: Assembly and wiring details
- TROUBLESHOOTING.md: This file

---

## Appendix: Advanced Diagnostics

### Logic Analyzer Capture
For intermittent issues, use logic analyzer to capture:
- Touch sensor output signals
- Hall sensor output signals
- Relay control signals
- Look for noise, glitches, timing issues

### Oscilloscope Analysis
For power quality issues:
- Capture 5V rail during relay switching
- Look for voltage droops (should stay >4.7V)
- Measure relay coil voltage spikes
- Verify no excessive ripple (should be <50mV peak-peak)

### Thermal Imaging
For overheating issues:
- Use thermal camera or IR thermometer
- Check relay module temperature (should be <50°C)
- Check lock temperature during continuous use (should be <40°C)
- Check power supply temperature (should be <60°C)
- Hot spots indicate: overload, poor connections, inadequate ventilation

---

**Document Version:** 1.0
**Last Updated:** [Current Date]
**Maintainer:** Technical Support Team
