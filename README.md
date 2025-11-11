# Captain's Cuffs - Escape Room Puzzle System

## Overview

Captain's Cuffs is an interactive escape room puzzle where players must simultaneously touch all engaged handcuff sensors to unlock them. The system uses an Arduino Mega with hall sensors, touch sensors, and relay-controlled electromagnetic locks.

**Written by:** Ryan Laing ("Arduino Guru")
**Platform:** Arduino Mega 2560
**Escape Room:** Alchemy Escape Rooms

## Puzzle Concept

Players are secured with electronic handcuffs. To escape, **all players wearing locked cuffs must touch their touch sensors simultaneously**. The puzzle creates a cooperative challenge requiring coordination and teamwork.

### Puzzle Logic

1. **Cuff Engagement**: Hall sensors detect when a cuff is locked (magnet present)
2. **Touch Detection**: Each cuff has a capacitive touch sensor
3. **Solution Check**: System continuously monitors if ALL engaged cuffs have their touch sensors activated
4. **Release**: When condition met, all cuffs unlock via relay-controlled locks
5. **Auto-Reset**: After 5 minutes, the puzzle automatically resets for the next group

## Hardware Requirements

### Components

| Component | Quantity | Specification |
|-----------|----------|---------------|
| Arduino Mega 2560 | 1 | Main controller |
| Hall Effect Sensors | 8 | Detect magnet (cuff engaged) |
| Capacitive Touch Sensors | 8 | Detect player touch |
| Relay Modules | 8 | Control electromagnetic locks |
| Electromagnetic Locks | 8 | Lock/unlock cuffs |
| Power Supply | 1 | 12V for locks + 5V for Arduino |
| Handcuff Hardware | 8 sets | 3D printed parts (see STL files) |

### Pin Configuration

#### Touch Sensors (Digital Input)
- Cuff 0: Pin 22
- Cuff 1: Pin 23
- Cuff 2: Pin 24
- Cuff 3: Pin 25
- Cuff 4: Pin 26
- Cuff 5: Pin 27
- Cuff 6: Pin 28
- Cuff 7: Pin 29

#### Hall Sensors (Digital Input with INPUT_PULLUP)
- Cuff 0: Pin 30
- Cuff 1: Pin 31
- Cuff 2: Pin 32
- Cuff 3: **DISABLED** (set to -1)
- Cuff 4: Pin 34
- Cuff 5: **DISABLED** (set to -1)
- Cuff 6: Pin 36
- Cuff 7: Pin 37

#### Relays (Digital Output)
- Cuff 0: Pin 38
- Cuff 1: Pin 39
- Cuff 2: Pin 40
- Cuff 3: Pin 41
- Cuff 4: Pin 42
- Cuff 5: Pin 43
- Cuff 6: Pin 44
- Cuff 7: Pin 45

### Wiring Diagram

```
ARDUINO MEGA 2560
┌─────────────────────────────────────┐
│                                     │
│  [22-29] ──► Touch Sensors          │
│  [30-37] ──► Hall Sensors           │
│  [38-45] ──► Relay Modules          │
│                                     │
│  [5V]    ──► Sensor Power           │
│  [GND]   ──► Common Ground          │
└─────────────────────────────────────┘
         │
         ▼
    Power Supply
    ├─ 5V DC  → Arduino + Sensors
    └─ 12V DC → Electromagnetic Locks
```

## Installation

### Arduino IDE Setup

1. **Install Arduino IDE** (version 1.8.19 or newer)
   - Download from: https://www.arduino.cc/en/software

2. **Connect Arduino Mega**
   - USB cable to computer
   - Select board: Tools → Board → Arduino Mega 2560
   - Select port: Tools → Port → (your COM port)

3. **Upload Code**
   - Open `CaptainsCuffs.ino`
   - Click Upload button (→)
   - Wait for "Done uploading" message

### Hardware Assembly

1. **Mount Components**
   - Install hall sensors inside cuff mechanism near magnet position
   - Install touch sensors on accessible part of cuff (player contact area)
   - Mount relay module in control box
   - Install electromagnetic locks in cuff mechanism

2. **Wire Connections**
   - Connect all sensors to appropriate pins (see Pin Configuration)
   - Connect relay COM to lock positive terminal
   - Connect relay NO to power supply positive
   - Connect lock negative to power supply ground
   - Ensure all grounds are common

3. **Power Supply**
   - Arduino: 5V via USB or barrel jack (7-12V)
   - Locks: 12V DC from power supply through relays
   - Total current capacity: Minimum 3A (locks draw ~250mA each)

## Operation

### Serial Monitor Commands

Connect via Serial Monitor (9600 baud) to control and monitor the system:

| Command | Description |
|---------|-------------|
| `status` | Display current status of all cuffs |
| `reset` | Manually reset puzzle (lock all cuffs) |
| `open all` / `openall` | Emergency unlock all cuffs |
| `close all` / `closeall` | Lock all cuffs |
| `test relays` | Test all relay operation |
| `test sensors` | Test touch sensors (30s) |
| `test magnets` | Test hall sensors (15s) |
| `test all` | Run complete system test |
| `help` | Show available commands |

### Startup Sequence

```
=== CAPTAIN'S CUFFS - HALL SENSOR VERSION ===
Platform: Arduino Mega
Initializing hardware...
Stabilizing sensors...

=== SYSTEM READY ===
Monitoring for state changes...
Type 'help' for available commands
```

### During Operation

The system monitors sensors continuously and prints state changes:

```
Cuff 0     Relay ON       Hall Sensor TRIGGERED        Touch Sensor NOT ACTIVE    Cuffs LOCKED
Cuff 0     Relay ON       Hall Sensor TRIGGERED        Touch Sensor ACTIVE        Cuffs LOCKED
SOLUTION: 6/6 - SOLVING PUZZLE!

=== RELEASING CUFFS ===
Cuff 0 RELEASED
Cuff 1 RELEASED
...
*** PUZZLE SOLVED! ***
Auto-reset in 300 seconds
```

### Status Display

Use `status` command to see detailed system state:

```
=== SYSTEM STATUS ===
Puzzle solved: NO
Uptime: 1234s

CUFF | LOCK STATUS | TOUCH SENSOR | HALL SENSOR
-----|-------------|--------------|-------------
  0  |   LOCKED    |   ACTIVE      |   CLOSED
  1  |   LOCKED    |   NOT ACTIVE  |   CLOSED
  2  |   LOCKED    |   ACTIVE      |   CLOSED
  3  |   DISABLED  |   DISABLED    |   DISABLED
  4  |   LOCKED    |   ACTIVE      |   CLOSED
  5  |   DISABLED  |   DISABLED    |   DISABLED
  6  |   UNLOCKED  |   NOT ACTIVE  |   OPEN
  7  |   LOCKED    |   ACTIVE      |   CLOSED
===============================================
```

## Testing Procedures

### Pre-Game Testing

Run these tests before each game session:

1. **Visual Inspection**
   - Check all wiring connections
   - Verify power supply output (5V and 12V)
   - Inspect 3D printed parts for damage

2. **System Test** (via Serial Monitor)
   ```
   test all
   ```
   - Hall sensors: Move magnets near sensors, verify detection
   - Touch sensors: Touch each pad within 30s timeout
   - Relays: Listen for click, verify lock engagement

3. **Puzzle Flow Test**
   - Lock all cuffs: `close all`
   - Engage multiple cuffs with magnets
   - Touch all engaged sensors simultaneously
   - Verify all engaged cuffs release

4. **Auto-Reset Test**
   - Solve puzzle
   - Wait 5 minutes
   - Verify automatic reset occurs

### Component Testing

#### Individual Relay Test
```
test relays
```
Each relay cycles: OFF (unlock) → ON (lock) with 1 second delay

#### Touch Sensor Test
```
test sensors
```
Touch each sensor within 30 seconds. System reports working sensors.

#### Hall Sensor Test
```
test magnets
```
System displays current magnet detection status and monitors for 15 seconds.

## Troubleshooting

### Common Issues

#### Cuff Won't Unlock

**Symptoms:** Cuff remains locked after puzzle solved

**Possible Causes:**
1. Relay failure
2. Lock mechanism jammed
3. Insufficient power to lock
4. Wiring issue

**Solutions:**
1. Test relay: `test relays` - listen for click
2. Check relay LED indicator
3. Measure voltage at lock terminals (should be 0V when unlocked)
4. Use emergency unlock: `open all`
5. Check power supply current capacity (min 3A)

#### Touch Sensor Not Responding

**Symptoms:** Touch not detected in status or puzzle

**Possible Causes:**
1. Sensor disconnected
2. Sensor damaged
3. Pin configuration incorrect
4. Grounding issue

**Solutions:**
1. Run sensor test: `test sensors`
2. Check wiring to touch sensor pins (22-29)
3. Verify sensor has power (5V)
4. Test sensor directly with multimeter (should read HIGH when touched)
5. Check for loose connections

#### Hall Sensor Always Triggered

**Symptoms:** Hall sensor shows TRIGGERED without magnet

**Possible Causes:**
1. Magnet stuck in position
2. Sensor wiring issue
3. INPUT_PULLUP not set
4. Sensor damaged

**Solutions:**
1. Run magnet test: `test magnets`
2. Remove magnet completely and check status
3. Verify hall sensor pins 30-37 (33, 35 disabled)
4. Check sensor type (active-low with pull-up expected)
5. Measure voltage on sensor output (should be ~5V without magnet)

#### Puzzle Solves Immediately

**Symptoms:** Cuffs unlock without all touches

**Possible Causes:**
1. Touch sensors reading false positives
2. Debounce timing too short
3. Logic error in code

**Solutions:**
1. Check touch sensor noise (use `status` repeatedly)
2. Increase debounceDelay (currently 50ms)
3. Verify sensor wiring doesn't cross high-voltage lines
4. Add shielding to touch sensor wires

#### Puzzle Won't Solve

**Symptoms:** All cuffs touched but no release

**Possible Causes:**
1. Not all cuffs engaged (hall sensor issue)
2. Touch sensors not registering
3. Timing issue (not simultaneous)

**Solutions:**
1. Check `status` output - verify hall sensors show TRIGGERED
2. Verify touch sensors show ACTIVE when touched
3. Touch sensors must be held during check (not just momentary)
4. Check debounce timing (50ms might need adjustment)

### Emergency Procedures

#### Emergency Unlock
```
open all
```
Immediately releases all cuffs regardless of state. Use for:
- Player distress
- System malfunction
- Emergency evacuation
- End of game session

#### Force Reset
```
reset
```
Locks all cuffs and resets puzzle state. Use for:
- Puzzle stuck in solved state
- Need to restart without power cycle
- Quick turnaround between groups

#### Power Cycle
1. Disconnect power supply
2. Wait 10 seconds
3. Reconnect power
4. Wait for "SYSTEM READY" message
5. Run `test all` to verify operation

### Diagnostic Commands

#### Check Current State
```
status
```
Shows all cuff states, sensor readings, and puzzle status.

#### Monitor Live Changes
Keep Serial Monitor open during operation. System prints state changes in real-time.

### Hardware Diagnostics

#### Check Relay Operation
1. Upload code and open Serial Monitor
2. Type: `test relays`
3. Listen for relay clicks (8 times)
4. If no click on specific relay:
   - Check relay module power
   - Check wiring to pins 38-45
   - Test relay manually with jumper wire

#### Check Touch Sensor
1. Type: `test sensors`
2. Touch each sensor pad
3. Should see "WORKING" confirmation
4. If not detected:
   - Use multimeter on sensor output
   - Should read HIGH (~5V) when touched
   - Check 5V power and ground to sensor

#### Check Hall Sensor
1. Type: `test magnets`
2. Move magnet away from sensor
3. Should see "magnet removed"
4. Move magnet close to sensor
5. Should see "MAGNET DETECTED"
6. If not working:
   - Hall sensors are active-low (LOW = magnet present)
   - Check INPUT_PULLUP is enabled
   - Verify sensor type (A3144 or similar)
   - Test with multimeter (should read ~0V with magnet)

## Advanced Features

### MQTT Integration (Currently Disabled)

The code includes MQTT support for remote monitoring and control. This requires:

1. **Hardware**: Arduino Ethernet Shield or WiFi Shield
2. **Network**: MQTT broker (e.g., Mosquitto)
3. **Enable**: Uncomment MQTT sections in code

#### MQTT Topics

**Published by Arduino:**
- `cuffs/status` - JSON with full system state
- `cuffs/locks` - Human-readable lock status
- `cuffs/summary` - Counts and status summary

**Subscribed by Arduino:**
- `cuffs/command/reset` - Reset puzzle
- `cuffs/command/status` - Request status update
- `cuffs/command/lockall` - Lock all cuffs
- `cuffs/command/releaseall` - Release all cuffs

#### Enable MQTT

1. Uncomment includes at top:
   ```cpp
   #include <PubSubClient.h>
   #include <Ethernet.h>
   ```

2. Configure network settings:
   ```cpp
   IPAddress mqttServer(192, 168, 1, 100);
   const int mqttPort = 1883;
   ```

3. Uncomment all MQTT function calls in code

4. Install required libraries:
   - PubSubClient
   - Ethernet (or WiFi)

### Configuration Options

#### Timing Adjustments

**Debounce Delay** (line 23):
```cpp
const unsigned long debounceDelay = 50;  // milliseconds
```
- Increase if touch sensors are too sensitive
- Decrease for faster response (minimum 20ms)

**Auto-Reset Delay** (line 24):
```cpp
const unsigned long autoResetDelay = 5 * 60 * 1000UL;  // 5 minutes
```
- Adjust timing between puzzle solve and auto-reset
- Format: minutes * 60 * 1000 (milliseconds)

#### Disable Specific Cuffs

Modify `hallPins` array (line 21):
```cpp
const int hallPins[numCuffs] = {30, 31, 32, -1, 34, -1, 36, 37};
```
- Set pin to `-1` to disable that cuff's hall sensor
- Cuffs 3 and 5 currently disabled
- Disabled cuffs won't participate in puzzle solution

## File Structure

```
Captains-Cuffs/
├── CaptainsCuffs.ino          # Main Arduino sketch
├── README.md                   # This file
├── HARDWARE_GUIDE.md          # Detailed wiring and assembly
├── TROUBLESHOOTING.md         # Extended troubleshooting guide
├── 8-3-2025/                  # 3D printed parts archive
│   ├── Left Side/
│   └── Right Side/
├── *.STL                       # 3D printable components
│   ├── Anchor Link.STL
│   ├── Cuff Under Side Left 7-8-25.STL
│   ├── cuff parts main 2 7-8-25.STL
│   └── ...
└── 8-3-2025.zip               # Archive of 3D models
```

## Maintenance

### Regular Checks

**Before Each Game:**
- Visual inspection of all connections
- Run `test all` command
- Verify puzzle solve/reset cycle

**Weekly:**
- Check electromagnetic lock operation
- Inspect 3D printed parts for wear
- Clean touch sensor surfaces
- Verify relay contacts (no burning/pitting)

**Monthly:**
- Full system test
- Check all solder joints
- Verify power supply voltages
- Update documentation with any issues

### Replacement Parts

Keep spares on hand:
- Touch sensors (most common failure)
- Relays (contacts wear over time)
- Electromagnetic locks (mechanical wear)
- Hall sensors (rare failures)
- 3D printed parts (wear and tear)

## Safety Considerations

### Electrical Safety
- Use proper gauge wire for lock current
- Ensure all high-voltage connections are insulated
- Keep 12V power supply isolated from 5V logic
- Use fuses on lock power circuits

### Player Safety
- **Always have manual override key** for locks
- Test emergency unlock (`open all`) before each session
- Monitor players via camera during game
- Have physical key backup for electronic locks
- Ensure cuffs can't tighten beyond safety point

### Emergency Procedures
1. Keep Serial Monitor accessible for quick `open all` command
2. Have physical key within 10 feet of puzzle
3. Train all staff on emergency unlock procedures
4. Test emergency unlock daily

## Support and Contact

**Arduino Code Developer:** Ryan Laing ("Arduino Guru")
**Escape Room:** Alchemy Escape Rooms

For technical support or questions about this system, document all:
- Serial Monitor output
- Test results (`test all`)
- Symptoms and reproduction steps
- Hardware configuration changes

## License and Credits

**Code Author:** Ryan Laing
**Escape Room Design:** Alchemy Escape Rooms
**Hardware Platform:** Arduino Mega 2560

This is proprietary escape room puzzle hardware. Modifications should maintain safety features and emergency unlock capabilities.

## Version History

- **Current Version**: Hall Sensor Implementation
  - Uses hall sensors instead of microswitches
  - Cuffs 3 and 5 disabled
  - 8 cuff configuration
  - Auto-reset after 5 minutes
  - MQTT disabled (no network hardware)

## Future Enhancements

- [ ] Add Ethernet shield for MQTT remote monitoring
- [ ] Implement web interface for control room
- [ ] Add audio feedback when puzzle solved
- [ ] Log game statistics (solve time, attempts)
- [ ] Add difficulty levels (different touch patterns)
- [ ] Implement hint system via MQTT
