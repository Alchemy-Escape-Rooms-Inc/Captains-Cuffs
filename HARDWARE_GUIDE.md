# Captain's Cuffs - Hardware Assembly Guide

## Table of Contents
1. [Bill of Materials](#bill-of-materials)
2. [Tools Required](#tools-required)
3. [Component Specifications](#component-specifications)
4. [Wiring Diagrams](#wiring-diagrams)
5. [Assembly Instructions](#assembly-instructions)
6. [Testing Procedures](#testing-procedures)
7. [Troubleshooting Hardware](#troubleshooting-hardware)

## Bill of Materials

### Electronics Components

| Item | Quantity | Part Number | Specifications | Estimated Cost |
|------|----------|-------------|----------------|----------------|
| Arduino Mega 2560 | 1 | - | ATmega2560, 54 digital I/O | $40 |
| Hall Effect Sensor | 8 | A3144 or SS443A | Unipolar, active-low, 4.5-24V | $8 |
| Capacitive Touch Sensor | 8 | TTP223 | Digital output, 2-5.5V | $12 |
| Relay Module 8-Channel | 1 | - | 5V coil, 10A contacts | $15 |
| Electromagnetic Lock | 8 | - | 12V, 250mA, fail-safe | $80 |
| Power Supply | 1 | - | Dual output 5V/3A + 12V/3A | $25 |
| Barrel Jack Adapter | 1 | - | For Arduino power | $3 |
| Wire (22 AWG stranded) | 50ft | - | Red, Black, assorted colors | $10 |
| Wire (18 AWG) | 20ft | - | For lock power | $5 |
| Dupont Connectors | 100 | - | Female, 2.54mm pitch | $8 |
| Heat Shrink Tubing | 1 set | - | Assorted sizes | $8 |
| Magnets | 8 | - | Neodymium 10mm x 3mm | $10 |
| Terminal Blocks | 2 | - | 12-position screw terminal | $6 |
| Project Box | 1 | - | 200mm x 150mm x 75mm | $15 |

**Total Electronics Cost:** ~$245

### 3D Printed Parts

| Component | Quantity | Material | Print Time | Infill |
|-----------|----------|----------|------------|--------|
| Cuff Main Body | 16 | PLA | 4h each | 30% |
| Cuff Underside | 16 | PLA | 2h each | 30% |
| Anchor Link | 8 | PLA | 3h each | 50% |
| Outer Link (Left) | 8 | PLA | 3h each | 30% |
| Outer Link (Right) | 8 | PLA | 3h each | 30% |
| Link Cover | 8 | PLA | 1h each | 20% |

**Estimated Filament:** 4-5kg PLA
**Total Print Time:** ~200 hours

### Hardware (Screws, Nuts, Bolts)

| Item | Quantity | Size | Purpose |
|------|----------|------|---------|
| M3 x 10mm Screws | 100 | - | Cuff assembly |
| M3 x 20mm Screws | 50 | - | Lock mounting |
| M3 Nuts | 150 | - | General assembly |
| M3 Lock Washers | 50 | - | Prevent loosening |
| M4 x 30mm Bolts | 16 | - | Main cuff hinge |
| M4 Nylock Nuts | 16 | - | Hinge retention |

## Tools Required

### Essential Tools
- Soldering iron (temperature controlled)
- Wire strippers (22-18 AWG)
- Crimping tool (Dupont connectors)
- Multimeter (voltage, continuity)
- Screwdriver set (Phillips, flathead)
- Allen key set (hex drivers)
- Heat gun (for heat shrink)
- Helping hands/PCB holder

### Optional Tools
- Logic analyzer (debugging)
- Oscilloscope (signal analysis)
- Label maker (wire identification)
- 3D printer (for parts)
- Wire loom/sleeving tools

## Component Specifications

### Arduino Mega 2560

**Microcontroller:** ATmega2560
**Operating Voltage:** 5V
**Input Voltage:** 7-12V (recommended)
**Digital I/O Pins:** 54 (15 PWM)
**Analog Inputs:** 16
**Flash Memory:** 256 KB
**SRAM:** 8 KB
**EEPROM:** 4 KB
**Clock Speed:** 16 MHz

**Power Consumption:**
- Active: ~50mA @ 5V
- Sleep: ~15mA @ 5V

### Hall Effect Sensors (A3144)

**Type:** Unipolar Hall Effect Switch
**Output:** Active-low (open collector)
**Operating Voltage:** 4.5-24V
**Output Current:** Max 25mA
**Operate Point:** 100 Gauss typical
**Release Point:** 40 Gauss typical
**Response Time:** <10μs

**Pinout:**
```
   [Front View]
    ___
   |   |
   |   |
   |___|
    | | |
    1 2 3
    | | |
   VCC GND OUT
```

**Wiring:**
- Pin 1 (VCC): 5V from Arduino
- Pin 2 (GND): Common ground
- Pin 3 (OUT): To Arduino digital pin (30-37) with INPUT_PULLUP

**Behavior:**
- No magnet: Output HIGH (pulled up to 5V)
- Magnet present: Output LOW (pulled to ground)

### Capacitive Touch Sensors (TTP223)

**Type:** Capacitive Touch Switch IC
**Operating Voltage:** 2.0-5.5V
**Output Mode:** Active-high CMOS
**Response Time:** 220ms (default)
**Current Consumption:** 1.5μA standby, 3.5mA active
**Touch Sensitivity:** Adjustable via onboard capacitor

**Pinout:**
```
   [Top View]
    ___________
   |           |
   | [  PAD  ] |  ← Touch pad area
   |___________|
    | | |
    V G O
    | | |
   VCC GND OUT
```

**Wiring:**
- VCC: 5V from Arduino
- GND: Common ground
- OUT: To Arduino digital pin (22-29)

**Behavior:**
- Not touched: Output LOW (0V)
- Touched: Output HIGH (5V)
- Note: Can detect touch through plastic (<5mm)

### 8-Channel Relay Module

**Type:** Optocoupler isolated relay
**Coil Voltage:** 5V DC
**Coil Current:** ~70mA per relay
**Contact Rating:** 10A @ 250VAC, 10A @ 30VDC
**Switching Time:** 10ms typical
**Isolation:** 2500V optocoupler

**Pinout (per channel):**
```
Control Side:
- VCC: 5V power
- GND: Ground
- IN1-IN8: Signal inputs (active-low)

Relay Side (per channel):
- COM: Common terminal
- NO: Normally Open (closed when energized)
- NC: Normally Closed (open when energized)
```

**Wiring:**
- VCC: 5V from power supply (not Arduino!)
- GND: Common ground
- IN1-IN8: Arduino pins 38-45
- COM: Lock positive terminal
- NO: 12V positive from power supply
- NC: Not used

**Important:**
- Relay module may draw up to 560mA (8 x 70mA)
- Power from external 5V supply, not Arduino
- Active-LOW trigger (LOW = relay ON)

### Electromagnetic Locks

**Type:** Fail-safe electromagnetic lock
**Voltage:** 12V DC
**Current:** 250mA per lock
**Holding Force:** 60lbs (27kg) typical
**Duty Cycle:** 100% continuous
**Response Time:** <50ms

**Terminals:**
- Red (+): Positive 12V
- Black (-): Ground/negative

**Fail-Safe Operation:**
- Power applied: LOCKED
- Power removed: UNLOCKED
- Safe for emergency power loss

**Mounting:**
- Lock body: Fixed to frame
- Strike plate: Attached to moving part
- Alignment critical (±1mm tolerance)

## Wiring Diagrams

### Power Distribution

```
┌─────────────────────────────────────────────┐
│         POWER SUPPLY (Dual Output)          │
│                                             │
│  5V/3A Output          12V/3A Output        │
└─────┬──────────────────────┬────────────────┘
      │                      │
      │                      │
      ├──► Arduino Mega (Vin)│
      │                      │
      ├──► Relay Module VCC  │
      │                      │
      ├──► Touch Sensors (8x)│
      │                      │
      ├──► Hall Sensors (8x) │
      │                      │
      └──► Common Ground ◄───┴──► Relay COM (via NO)
                                   │
                                   ├──► Lock 0
                                   ├──► Lock 1
                                   ├──► Lock 2
                                   ├──► Lock 3
                                   ├──► Lock 4
                                   ├──► Lock 5
                                   ├──► Lock 6
                                   └──► Lock 7
```

### Arduino Connection Overview

```
ARDUINO MEGA 2560
┌─────────────────────────────────────────┐
│                                         │
│  Digital I/O                            │
│  ┌────────────────┐                     │
│  │ 22-29  Touch   │◄─── Touch Sensors   │
│  │ 30-37  Hall    │◄─── Hall Sensors    │
│  │ 38-45  Relay   │───► Relay Module    │
│  └────────────────┘                     │
│                                         │
│  Power                                  │
│  ┌────────────────┐                     │
│  │ 5V    ├────────┼───► Sensor Power    │
│  │ GND   ├────────┼───► Common Ground   │
│  │ Vin   ◄────────┼──── 7-12V DC In     │
│  └────────────────┘                     │
└─────────────────────────────────────────┘
```

### Single Cuff Wiring (Detailed)

```
CUFF 0 EXAMPLE:
                    ARDUINO MEGA
                    ┌──────────┐
Touch Sensor        │          │
TTP223             │   Pin 22 ◄├── Touch Signal
┌────────┐         │          │
│  VCC ──┼─────────┤ 5V       │
│  GND ──┼─────────┤ GND      │
│  OUT ──┼─────────┤ Pin 22   │
└────────┘         │          │
                   │          │
Hall Sensor        │   Pin 30 ◄├── Hall Signal
A3144              │          │
┌────────┐         │          │
│  VCC ──┼─────────┤ 5V       │
│  GND ──┼─────────┤ GND      │
│  OUT ──┼─────────┤ Pin 30   │
└────────┘         │          │
                   │          │
                   │   Pin 38 ├──► Relay IN1
                   │          │
                   └──────────┘

Relay Module                  12V Power Supply
┌────────────┐               ┌──────────┐
│  IN1   ◄───┼───────────────┤          │
│  VCC   ────┼───────────────┤ 5V       │
│  GND   ────┼───────────────┤ GND      │
│            │               │          │
│  COM ──────┼───────────────┤ Lock +   │
│  NO  ──────┼───────────────┤ 12V +    │
│  NC        │ (not used)    │          │
└────────────┘               │ GND  ────┼── Lock -
                             └──────────┘
```

### Complete System Wiring Table

| Component | Arduino Pin | Wire Color | Destination | Notes |
|-----------|-------------|------------|-------------|-------|
| Touch 0 | 22 | Yellow | TTP223 OUT | Pull-up not needed |
| Touch 1 | 23 | Yellow | TTP223 OUT | |
| Touch 2 | 24 | Yellow | TTP223 OUT | |
| Touch 3 | 25 | Yellow | TTP223 OUT | |
| Touch 4 | 26 | Yellow | TTP223 OUT | |
| Touch 5 | 27 | Yellow | TTP223 OUT | |
| Touch 6 | 28 | Yellow | TTP223 OUT | |
| Touch 7 | 29 | Yellow | TTP223 OUT | |
| Hall 0 | 30 | Green | A3144 OUT | INPUT_PULLUP enabled |
| Hall 1 | 31 | Green | A3144 OUT | |
| Hall 2 | 32 | Green | A3144 OUT | |
| Hall 3 | 33 | - | DISABLED | Set to -1 in code |
| Hall 4 | 34 | Green | A3144 OUT | |
| Hall 5 | 35 | - | DISABLED | Set to -1 in code |
| Hall 6 | 36 | Green | A3144 OUT | |
| Hall 7 | 37 | Green | A3144 OUT | |
| Relay 0 | 38 | Blue | Relay IN1 | Active-LOW |
| Relay 1 | 39 | Blue | Relay IN2 | |
| Relay 2 | 40 | Blue | Relay IN3 | |
| Relay 3 | 41 | Blue | Relay IN4 | |
| Relay 4 | 42 | Blue | Relay IN5 | |
| Relay 5 | 43 | Blue | Relay IN6 | |
| Relay 6 | 44 | Blue | Relay IN7 | |
| Relay 7 | 45 | Blue | Relay IN8 | |
| 5V | Multiple | Red | All sensors + relay | External 5V supply |
| GND | Multiple | Black | Common ground | Star ground topology |

## Assembly Instructions

### Phase 1: Electronics Preparation (2 hours)

#### Step 1: Prepare Wires
1. Cut wires to appropriate lengths (measure actual installation)
   - Touch sensors: 1-2m each (depending on cuff location)
   - Hall sensors: 1-2m each
   - Relay to lock: 0.5-1m each (18 AWG)
   - Control wires: Use color code
2. Strip 5mm from each end
3. Add heat shrink before crimping
4. Crimp Dupont connectors to Arduino/sensor ends
5. Label each wire (e.g., "Touch-0", "Hall-2", "Relay-5")

#### Step 2: Prepare Control Box
1. Mount Arduino Mega in project box
   - Use M3 standoffs (10mm height)
   - Secure with M3 screws
2. Mount relay module
   - Keep separate from Arduino (heat/noise)
   - Use standoffs or double-sided tape
3. Install terminal blocks
   - One for 5V distribution
   - One for ground distribution
4. Mount power supply
   - External is recommended
   - If internal: ensure adequate cooling

#### Step 3: Power Distribution
1. Connect power supply outputs to terminal blocks
   - 5V output → 5V terminal block (+)
   - 12V output → Relay power section
   - Ground → Ground terminal block
2. Wire Arduino power
   - 5V terminal → Arduino 5V pin OR
   - 7-12V → Arduino Vin (barrel jack)
3. Wire relay module power
   - 5V terminal → Relay VCC (NOT from Arduino!)
   - Ground terminal → Relay GND

#### Step 4: Connect Control Signals
1. Connect touch sensors
   - Pin 22-29 → Relay IN1-IN8
   - Use cable loom for organization
   - Leave extra length for service access
2. Connect hall sensors
   - Pin 30, 31, 32, 34, 36, 37 → Hall sensor outputs
   - Skip pins 33, 35 (disabled)
3. Connect relay control
   - Pin 38-45 → Relay IN1-IN8
   - Double-check pin mapping

### Phase 2: Cuff Assembly (1 hour per cuff)

#### Step 1: Print and Prepare 3D Parts
1. Print all parts from STL files
2. Remove support material
3. Clean up any stringing
4. Test fit all components before assembly

#### Step 2: Install Hall Sensor
1. Position sensor near magnet location
   - Test range: magnet should trigger within 10mm
   - Secure with hot glue or epoxy
2. Route wires through channel
3. Exit wires at connection point

#### Step 3: Install Touch Sensor
1. Position on accessible surface
   - Player must be able to touch easily
   - Can be behind thin plastic (<3mm)
2. Secure sensor
   - Use double-sided tape or epoxy
   - Ensure touch pad is accessible
3. Route wires with hall sensor wires

#### Step 4: Install Electromagnetic Lock
1. Mount lock body to fixed frame
   - Use M3 x 20mm screws
   - Ensure alignment with strike plate
2. Mount strike plate to moving cuff
   - Test engagement before final assembly
3. Connect lock wires to relay output
   - COM and NO terminals
   - Use 18 AWG wire (higher current)
4. Test mechanical operation
   - Cuff should close smoothly
   - Lock should engage with <1mm gap

#### Step 5: Install Magnet
1. Position magnet to trigger hall sensor
   - Test with multimeter (sensor output should go LOW)
   - Mark position before gluing
2. Secure magnet
   - Use epoxy (neodymium is strong!)
   - Ensure polarity is correct (test first)
3. Test hall sensor triggering
   - Move cuff through full range of motion
   - Sensor should trigger reliably when closed

#### Step 6: Final Cuff Assembly
1. Assemble main cuff body
   - Install hinge with M4 bolts
   - Use Nylock nuts to prevent loosening
2. Attach all covers and decorative parts
3. Route all wires neatly
   - Use cable ties
   - Protect wires from pinch points
4. Label cuff number (0-7)

### Phase 3: System Integration (2 hours)

#### Step 1: Connect All Cuffs to Control Box
1. Connect touch sensor wires (22-29)
2. Connect hall sensor wires (30-37, skip 33, 35)
3. Connect lock power wires to relay outputs
4. Organize wires with cable management

#### Step 2: Power Distribution Check
1. Verify all 5V connections with multimeter
2. Verify all ground connections (continuity test)
3. Check 12V at each relay NO terminal
4. Ensure no shorts between power rails

#### Step 3: Upload Arduino Code
1. Connect Arduino to computer via USB
2. Open Arduino IDE
3. Select: Tools → Board → Arduino Mega 2560
4. Select: Tools → Port → (your COM port)
5. Click Upload
6. Wait for "Done uploading"
7. Open Serial Monitor (9600 baud)
8. Verify "SYSTEM READY" message appears

### Phase 4: Testing and Calibration (1 hour)

#### Step 1: Basic Sensor Test
```
Serial Monitor Command: test all
```
1. Hall sensors: Move magnets, verify detection
2. Touch sensors: Touch each pad, verify response
3. Relays: Listen for clicks, verify lock engagement

#### Step 2: Individual Cuff Testing
For each cuff (0-7):
1. Close cuff (engage lock)
2. Verify hall sensor shows "TRIGGERED"
3. Touch sensor pad
4. Verify touch sensor shows "ACTIVE"
5. Open cuff
6. Verify hall sensor shows "NOT TRIGGERED"

#### Step 3: Puzzle Logic Test
1. Close multiple cuffs (engage locks)
2. Touch all engaged cuff sensors simultaneously
3. Verify all engaged cuffs release
4. Check auto-reset after 5 minutes

#### Step 4: Emergency Procedures Test
1. Test `open all` command (immediate unlock)
2. Test `reset` command (re-lock all)
3. Verify physical key backup works

## Testing Procedures

### Pre-Installation Tests

#### Power Supply Test
1. **Voltage Test**
   - Measure 5V output: Should be 4.9-5.1V
   - Measure 12V output: Should be 11.8-12.2V
   - Under load (all locks engaged): Should stay within range

2. **Current Capacity Test**
   - Connect 8 locks (worst case: 8 x 250mA = 2A)
   - Measure voltage drop under load
   - Should not drop below 11.5V

#### Arduino Test
1. **Upload Test Sketch**
   - Upload blink sketch to test Arduino
   - Verify LED blinks (confirms Arduino works)

2. **Pin Test**
   - Use multimeter continuity mode
   - Test each pin header for continuity
   - Verify no shorts between adjacent pins

### Component Tests

#### Hall Sensor Test (Bench)
1. Connect hall sensor to breadboard
   - VCC → 5V
   - GND → Ground
   - OUT → Test LED + resistor
2. Move magnet near sensor
3. LED should turn ON when magnet close (active-low)

#### Touch Sensor Test (Bench)
1. Connect touch sensor to breadboard
   - VCC → 5V
   - GND → Ground
   - OUT → Test LED + resistor
2. Touch sensor pad
3. LED should turn ON when touched (active-high)

#### Relay Test (Bench)
1. Connect relay module
   - VCC → 5V (external supply!)
   - GND → Ground
   - IN1 → Arduino pin 38
2. Set pin 38 LOW (digitalWrite(38, LOW))
3. Should hear audible click
4. Measure resistance across NO/COM: ~0Ω (closed)
5. Set pin 38 HIGH
6. Measure resistance across NO/COM: ∞ (open)

### Installation Tests

#### Wiring Continuity Test
1. Disconnect power
2. Use multimeter continuity mode
3. Test each wire end-to-end
4. Document any failed connections

#### Sensor Installation Test
1. Power system
2. Open Serial Monitor
3. Type: `status`
4. Verify all sensors show expected states
   - Hall sensors: Match physical magnet position
   - Touch sensors: NOT ACTIVE (not being touched)
   - Relays: ON (locked) for initial state

### Functional Tests

#### Individual Cuff Test
```
Test sequence for Cuff N:
1. Close cuff (engage lock)
2. Serial: Type "status"
3. Verify: Hall sensor = TRIGGERED
4. Touch sensor pad
5. Serial: Verify Touch sensor = ACTIVE
6. Open cuff (disengage lock)
7. Serial: Verify Hall sensor = NOT TRIGGERED
8. Release touch pad
9. Serial: Verify Touch sensor = NOT ACTIVE
```

#### Full Puzzle Test
```
1. Serial: Type "close all"
2. Engage 4 cuffs (close around player wrists/ankles)
3. Serial: Verify 4 hall sensors show TRIGGERED
4. Touch 3 of 4 sensors (not simultaneous)
5. Serial: Should NOT solve
6. Touch all 4 sensors simultaneously
7. Serial: Should show "SOLVING PUZZLE!"
8. Verify: All 4 cuffs release
9. Wait 5 minutes
10. Serial: Should show "RESETTING PUZZLE"
11. Verify: All cuffs lock again
```

#### Stress Test
```
1. Run puzzle solve/reset cycle 10 times
2. Monitor for any failures
3. Check relay temperatures (should be <50°C)
4. Check lock temperatures (should be <40°C)
5. Verify no loose connections
```

## Troubleshooting Hardware

### Power Issues

#### Symptom: Arduino won't power on
**Check:**
- [ ] Power supply output voltage (should be 7-12V for Vin or 5V for 5V pin)
- [ ] Power connector firmly seated
- [ ] Polarity correct (center-positive for barrel jack)
- [ ] USB cable functional (try different cable)

**Solution:**
- Use regulated 5V supply to 5V pin
- Use 7-12V supply to Vin or barrel jack
- Check for shorts on Arduino board

#### Symptom: Sensors not working
**Check:**
- [ ] 5V at sensor VCC pin (measure with multimeter)
- [ ] Ground continuity (sensor GND to Arduino GND)
- [ ] Signal wire connected to correct Arduino pin

**Solution:**
- Verify power supply 5V output under load
- Check terminal block connections
- Re-seat sensor connectors

#### Symptom: Relays clicking but locks not engaging
**Check:**
- [ ] 12V at relay COM terminal
- [ ] 12V at relay NO terminal when relay on
- [ ] Lock wire continuity
- [ ] Lock polarity (red to +, black to -)

**Solution:**
- Measure voltage at lock terminals (should be 12V when locked)
- Check relay contact condition (replace if burnt)
- Verify power supply 12V output can supply 2A+

### Sensor Issues

#### Symptom: Hall sensor always triggered
**Check:**
- [ ] Magnet stuck in position near sensor
- [ ] Sensor wiring (should have pull-up enabled in code)
- [ ] Sensor output voltage (should be ~5V without magnet)

**Solution:**
- Remove magnet, check sensor state
- Verify INPUT_PULLUP in code
- Replace hall sensor if damaged

#### Symptom: Hall sensor never triggers
**Check:**
- [ ] Magnet position (should be <10mm from sensor)
- [ ] Magnet polarity (flip magnet, try again)
- [ ] Sensor power (5V at VCC)
- [ ] Sensor output changes with magnet (measure voltage)

**Solution:**
- Reposition magnet closer to sensor
- Test sensor on breadboard
- Replace sensor if no output change

#### Symptom: Touch sensor too sensitive
**Check:**
- [ ] Wire routing (keep away from AC power, relay wires)
- [ ] Sensor sensitivity adjustment (resistor/capacitor on board)
- [ ] False triggers from nearby objects

**Solution:**
- Add shielding to touch sensor wires
- Adjust debounceDelay in code (increase from 50ms)
- Add grounded metal shield around touch pad

#### Symptom: Touch sensor not sensitive enough
**Check:**
- [ ] Touch pad accessible (not behind thick plastic)
- [ ] Sensor power (5V at VCC)
- [ ] Output voltage changes when touched

**Solution:**
- Remove barriers between finger and sensor pad
- Increase touch pad area (copper foil)
- Adjust sensor sensitivity (onboard jumper/capacitor)

### Relay Issues

#### Symptom: Relay won't click
**Check:**
- [ ] 5V at relay module VCC
- [ ] Control signal from Arduino (measure pin voltage)
- [ ] LED on relay module (should light when triggered)

**Solution:**
- Power relay module from external 5V (not Arduino)
- Verify control signal is going LOW (active-low)
- Replace relay module if LED lights but no click

#### Symptom: Relay clicks but no continuity
**Check:**
- [ ] Measure resistance across COM and NO when relay on (should be ~0Ω)
- [ ] Visual inspection of relay contacts (look for burning/pitting)

**Solution:**
- Replace relay module (contacts worn out)

### Lock Issues

#### Symptom: Lock doesn't engage
**Check:**
- [ ] 12V at lock terminals when relay on
- [ ] Lock alignment (strike plate within 1mm of lock face)
- [ ] Lock current draw (should be ~250mA)

**Solution:**
- Adjust strike plate position
- Check relay contact rating (10A is sufficient)
- Replace lock if internal coil damaged

#### Symptom: Lock won't release
**Check:**
- [ ] 0V at lock terminals when relay off
- [ ] Mechanical binding (cuff parts interfering)

**Solution:**
- Verify relay turns off (should click off)
- Check mechanical assembly for binding
- Emergency: Use physical key backup

### Wiring Issues

#### Symptom: Intermittent sensor readings
**Check:**
- [ ] Connection crimps (tug test - should not pull out)
- [ ] Wire routing (no stress at connectors)
- [ ] Broken wire (continuity test)

**Solution:**
- Re-crimp connectors
- Add strain relief at connection points
- Replace damaged wires

#### Symptom: Multiple sensors interfering
**Check:**
- [ ] Shared ground (should be star topology from terminal block)
- [ ] Wire routing (keep signal wires away from power wires)
- [ ] Proper shielding on long wire runs

**Solution:**
- Implement star ground (all grounds to single point)
- Use twisted pair for sensor signals
- Add ferrite beads to reduce noise

## Maintenance Schedule

### Daily (Before Each Game Session)
- [ ] Visual inspection of all connections
- [ ] Run `test all` command via Serial Monitor
- [ ] Test emergency unlock (`open all`)
- [ ] Check for any error messages

### Weekly
- [ ] Inspect 3D printed parts for cracks
- [ ] Check relay contacts for burning/pitting
- [ ] Clean touch sensor surfaces
- [ ] Verify lock alignment
- [ ] Tighten any loose screws

### Monthly
- [ ] Full system disassembly and inspection
- [ ] Check all solder joints
- [ ] Measure power supply voltages under load
- [ ] Test backup power (if installed)
- [ ] Update maintenance log

### Annually
- [ ] Replace all touch sensors (preventive)
- [ ] Replace relay module (preventive)
- [ ] Inspect electromagnetic locks (wear)
- [ ] Check all crimp connections
- [ ] Full system retest and recalibration

## Safety Checklist

### Electrical Safety
- [ ] All high-voltage (12V) connections insulated
- [ ] Fuse on 12V power supply (3A recommended)
- [ ] No exposed metal at voltage >5V
- [ ] Common ground for all components
- [ ] Power supply has overload protection

### Player Safety
- [ ] Emergency unlock tested and accessible
- [ ] Physical key backup available
- [ ] Staff trained on emergency procedures
- [ ] Camera monitoring in place
- [ ] Cuffs cannot over-tighten
- [ ] All edges deburred (no sharp corners)
- [ ] Lock holding force appropriate (<100lbs)

### Fire Safety
- [ ] No overheating components (check with thermal camera)
- [ ] Wire gauge appropriate for current
- [ ] Project box has ventilation
- [ ] Fire extinguisher nearby
- [ ] Power supply has thermal shutoff

## Resources and References

### Component Datasheets
- Arduino Mega: https://www.arduino.cc/en/Main/Arduino_Mega2560
- A3144 Hall Sensor: [Search manufacturer datasheet]
- TTP223 Touch IC: [Search manufacturer datasheet]

### Wire Gauge Reference
| Current | Wire Gauge | Max Length |
|---------|------------|------------|
| <500mA | 24 AWG | 10m |
| <1A | 22 AWG | 10m |
| <3A | 18 AWG | 5m |

### Crimp Tool Settings
- Dupont Connectors: Medium setting
- Wire size: 22-24 AWG
- Strip length: 5mm

### Recommended Suppliers
- Electronics: Adafruit, SparkFun, Digi-Key, Mouser
- 3D Filament: Hatchbox, eSUN, Prusament
- Hardware: McMaster-Carr, Fastenal
- Wire: Striveday, Remington Industries

---

**Document Version:** 1.0
**Last Updated:** [Current Date]
**Author:** Technical Documentation Team
**Based on code by:** Ryan Laing
