# Debug session plan (2 h)

**Status:** lamp assembled. Boards 1 to ~15 work, everything after that only flickers.
**Goal:** find the broken board, don't damage more, leave the lamp in a safe state.

Have at hand: `pcb/diskohedron4w_revf_DEBUG.pdf` (schematic, board top/bottom, diode-test
sheet) and `pcb/board_chain.csv` (which output each board uses, bridges, comments).

## What the symptom says

Each board passes DATA on to the next one (4R receives, STP04 shifts it through,
1D sends it out). CLK and LATCH go to all boards on one shared bus.

- **Boards 1–N fine, N+1 and later flicker:** DATA is lost somewhere between the
  output of board N and the input of board N+1. CLK and LATCH are OK, otherwise
  boards 1–N would flicker too.
- **Prime suspects:**
  - board N's **1D** (DS90LV011A)
  - board N+1's **4R** (SN65LVDS32B)
  - the connector or solder bridges between them
  - a short TP4↔TP39 / TP3↔TP40 on board N if it uses OUT-R
- **Typical cause:** a short ground interruption on the power connector. The ground
  current then flows through the LVDS pins and kills 1D or 4R.

## Priorities

| # | Time | What | Why |
|---|------|------|-----|
| 0 | 10 min | Safe setup | Stop the damage first |
| 1 | 15 min | Find N exactly, without opening anything | Tells you which 1–2 boards to open |
| 2 | 15 min | Power/ground checks on the suspects | Finds the cause, not just the victim |
| 3 | 45 min | Open boards N / N+1, measure, fix | The actual repair |
| 4 | 20 min | Retest + ground retrofit on the opened boards | Stops it coming back |
| 5 | 15 min | Notes in `board_chain.csv` | So next time starts here |

If it runs long, skip 4 except the safety part, and do 5 anyway.

## 0. Safe setup (10 min)

**Before switching on, power off, ohmmeter** (5 min, no need to open the lamp):
- **24 V supply GND** to **board 1 GND** (JP1 pin 2) and to every extra feed point /
  Wago you can reach: each should be **< 0.5 Ω**.
- **+24 V to GND** at the feed: must **not** be near 0 Ω. Electrolytic caps charging
  from the meter is normal: the value climbs slowly.
- If a ground reads high or open, fix that connection first. That is exactly the
  situation that kills LVDS chips.

All OK: switch on as described below. The lamp has run like this before; chips die
when ground is **interrupted**, not from switching on with a good ground. The full
ground and feed retrofit (step 4) takes longer than 2 h for all 30 boards. Do it on
the boards you open, and the rest in a later session.

- **Never plug or unplug anything while powered.** This covers board connectors,
  the controller and USB. Power off, wait ~5 s.
- If you have a lab supply: 24 V with a **current limit of ~1 A** for the first
  power-up. Each board draws little at low brightness, so if the limit trips,
  something is shorted.
- **Controller ground:** the controller's GND must be wired to JP1 pin 2 *before*
  powering anything.
  - If the controller is powered from the laptop's USB and the lamp from its own
    PSU, the laptop and lamp grounds are only joined through the signal wires.
    Use a battery-powered laptop or an isolated USB hub, or power the controller
    from the lamp.
- Keep brightness low (≤ 20 %) for all tests: less current means less ground
  offset.
- Don't leave the lamp running flickering for long. Flicker itself does no harm,
  but whatever caused it may still be there.

## 1. Find N exactly (15 min, lamp closed)

Flash **`software/Arduino/chaintest/chaintest.ino`** to the Arduino. It uses the
same pins as the old firmware and needs no libraries. Open the Serial Monitor at
9600 baud with line ending "Newline", then send:

| Send | Does |
|---|---|
| `c` | chase, one board after another; the board number is printed |
| `16` | only board 16 on (`16r` / `16g` / `16b` / `16w` for one colour) |
| `a` | all boards red |
| `o` | all off |
| `h` / `l` | hold DATA HIGH / LOW for measuring (step 3) |

1. Run the chase slowly and note the **last board that lights correctly = N**.
2. Light only board N, then only N+1, then only N+2, and note what you see:

| What you see | Most likely |
|---|---|
| N+1 lights correctly, N+2 and later flicker | Board N+1's **output** (its 1D, bridges, OUT connector) |
| N+1 already flickers or stays dark | Board N's 1D, or board N+1's 4R / IN connector |
| All boards dark, or 1–N flicker too | Not this: CLK/LATCH bus, board 1 or controller. Go to "shared bus" below |
| Fault moves when you tap / press on the lamp | Connector or cold solder joint, not a dead chip |

3. **Touch test** (powered, carefully): put a finger on the 1D (small SOT23-5) and
   4R (SOIC-16) of boards N and N+1. A dead LVDS chip often runs **hot** because it
   shorts internally. A hot chip is your answer.

## 2. Power and ground checks (15 min)

Do these at boards N and N+1, on the power connector or C1/C2. With the lamp powered:

- **24 V present** on both boards.
- **3.3 V** on both: C3/C4, or JP1 pin 1 to pin 2. Should be 3.2–3.4 V. If it's low
  on one board, that board has a shorted chip.
- **Ground offset:** with the meter on DC mV, put the black probe on board 1's GND
  (JP1 pin 2) and the red probe on board N+1's GND.
  - < 100 mV: OK.
  - Several hundred mV or more: ground connection problem along the chain. **This is
    the root cause to fix**, otherwise new chips will die.
- **Power off, ohmmeter:** GND of board N to GND of board N+1 should be **< 0.5 Ω**.
  If not, the 2-pin power connector or its solder joint is bad.

## 3. Open boards N and N+1 (45 min)

**First, visual check:**
- **4D** fitted? It must only be on board 1. If it's on any other board, it fights
  the bus. Remove it.
- **R2 / R3** fitted? Only on board 30.
- **Solder bridges:** compare with `board_chain.csv`.
  - All 6 bridges of the used output are closed, and no others.
  - **OUT-R boards:** TP4 must go to TP10 and TP3 to TP9, and neither may touch
    TP39/TP40.
- **IN / OUT connector pins:** bent, cracked, or burnt.

**Measure DATA with the test sketch.** Send `h`, then `l`. The LEDs don't change; only
the data line is held at one level along the whole chain. Put the meter on DC volts,
red probe on **TP39**, black on **TP40**, and check one board after another:
- `h` should read about **+0.35 V**, `l` about **−0.35 V**.
- The first board where the reading doesn't flip is right after the broken spot.
- If board N's DATA out (TP4 red / TP3 black) flips but board N+1's TP39/TP40 doesn't,
  the problem is the connector or bridges between them.

**Measure LVDS while running** (DC voltmeter to GND):

| Signal | Board | Test pads | Expected | Bad |
|---|---|---|---|---|
| DATA in | N+1 | TP39 / TP40 | each ~1.0–1.4 V | ~0 V or ~3 V |
| DATA out | N | TP4 / TP3 | each ~1.0–1.4 V | ~0 V or ~3 V |

- A line stuck at 0 V or 3.3 V means a dead driver or a short.
- **DATA+ and DATA− exactly equal** means they're shorted together.

**Power off, diode test:** use page 6 of the debug PDF. Compare the 1D and 4R pins of
the suspect board with a **known-good board** (e.g. board 2). A pin that reads
0.0 V or open where the good board shows ~0.5–0.7 V is a blown input or output.

**Fix:**
- Dead **1D (DS90LV011A)** or **4R (SN65LVDS32B)**: replace the chip. Hot air or two
  irons. 1D is the easy one.
- No spare chips: swap the whole board with a spare, if you have one.
- Bad connector: reflow or replace it.

## 4. Retest and prevent (20 min)

- Close, power up with the current limit, run the chase through all 30 boards.
- On the boards you have open anyway, add the **extra GND wire**: JP1 pin 2, or a
  C1/C2 minus pad, to the star point / Wago GND. See `software/esp32/WIRING.md`.
- **Extra 24 V/GND feeds** at boards 1, 7, 13, 19, 25. Same Wago, all in parallel,
  short thick wires.

## 5. Write it down (15 min)

In `pcb/board_chain.csv`, add for the repaired boards: what was broken, which chip
was replaced, and the measured values. Also note which output board 1 actually
uses (L or R).

## If it's the shared bus after all (all boards flicker)

- **Check R2/R3:** fitted on board 30 only. Power off, then measure between CLK+
  and CLK− (TP6/TP5) on any board: ~100 Ω means one terminator, which is correct.
  - ~50 Ω means two terminators.
  - Open means none, or the bus is broken somewhere.
- **Check 4D:** fitted on board 1 only.
- **Check the controller wiring:** JP1 pins 2–5, and ground.

## Not for tomorrow

ESP32 switch-over, Art-Net, new effects. Get the chain healthy first. The ESP32
won't fix a dead LVDS chip.
