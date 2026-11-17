# Wiring – ESP32 version

Three parts: the ESP32 to board 1 (signals), the power distribution, and the
board chain.

## 1. ESP32 → board 1 (JP1)

JP1 is the 5-pin "external uC" header on the **first** board of the chain.
Its pins are through-hole, so you can solder wires directly.

```mermaid
flowchart LR
    subgraph ESP["ESP32 DevKit"]
        G18["GPIO 18 (SPI CLK)"]
        G23["GPIO 23 (SPI MOSI)"]
        G4["GPIO 4 (SPI CS)"]
        EGND["GND"]
        E5V["5V / VIN"]
    end

    subgraph JP1["Board 1 – JP1"]
        P1["pin 1  +3V3  (leave free!)"]
        P2["pin 2  GND"]
        P3["pin 3  EXTCLK"]
        P4["pin 4  EXTDATA"]
        P5["pin 5  EXTOE (= LE / latch)"]
    end

    subgraph D4["Board 1 – chip 4D SN65LVDS047"]
        DRV["LVDS driver<br/>(only fitted on board 1)"]
    end

    G18 --> P3
    G23 --> P4
    G4 --> P5
    EGND --- P2
    P3 --> DRV
    P4 --> DRV
    P5 --> DRV

    BUCK["24 V → 5 V buck module"] --> E5V
    PD["10 kΩ pull-down<br/>(optional)"] -.- P5
```

| ESP32   | JP1 pin | Signal          | Note                                             |
|---------|---------|-----------------|--------------------------------------------------|
| GPIO 18 | 3       | EXTCLK          | SPI clock, 4 MHz                                 |
| GPIO 23 | 4       | EXTDATA         | SPI data                                         |
| GPIO 4  | 5       | EXTOE (LE)      | SPI chip-select = latch                          |
| GND     | 2       | GND             | short, solid wire                                |
| –       | 1       | +3V3            | **don't connect** the ESP32's own 3.3 V here     |

### ESP32-C3 variant (e.g. C3 SuperMini)

```mermaid
flowchart LR
    subgraph C3["ESP32-C3 board"]
        C6["GPIO 6 (SPI CLK)"]
        C7["GPIO 7 (SPI MOSI)"]
        C10["GPIO 10 (SPI CS)"]
        CGND["GND"]
        C5V["5V"]
    end

    subgraph JP1["Board 1 – JP1"]
        P1["pin 1  +3V3  (leave free!)"]
        P2["pin 2  GND"]
        P3["pin 3  EXTCLK"]
        P4["pin 4  EXTDATA"]
        P5["pin 5  EXTOE (= LE / latch)"]
    end

    C6 -- "CLK" --> P3
    C7 -- "DATA" --> P4
    C10 -- "LE" --> P5
    CGND --- P2

    BUCK["24 V → 5 V buck module"] --> C5V
    BUCK -. "GND" .- CGND
    PD["10 kΩ pull-down<br/>(optional)"] -.- P5
```

| ESP32-C3 | JP1 pin | Signal     |
|----------|---------|------------|
| GPIO 6   | 3       | EXTCLK     |
| GPIO 7   | 4       | EXTDATA    |
| GPIO 10  | 5       | EXTOE (LE) |
| GND      | 2       | GND        |
| –        | 1       | +3V3: don't connect |

The pins are set automatically in `config.h` when you compile for the C3.
Don't use GPIO 2/8/9 (boot strapping), 18/19 (USB) or 12–17 (flash) for this.
On the SuperMini the pins are printed as `6`, `7`, `10` next to the pads. Use
any `G` pad for GND.

- Keep these wires short (< 20 cm) and twist each signal wire with a GND wire if you can.
- **Optional:** a 10 kΩ resistor from JP1 pin 5 to pin 2. It keeps the latch quiet while the ESP32 boots, which reduces the random flash at power-on.
- Check pins 1 and 2 with a multimeter before soldering: pin 2 must beep against the big capacitor's GND pad.

## 2. Power

One PSU feeds everything through Wago blocks: the start of the chain, extra
feeds every ~6 boards, and the ESP32's buck module.

```mermaid
flowchart LR
    PSU["24 V PSU<br/>(200 W)"]
    PSU -- "+24 V ≥1.5 mm²" --> WP["Wago +24 V"]
    PSU -- "GND ≥1.5 mm²" --> WG["Wago GND"]

    WP --> BUCK["24 V → 5 V buck"]
    WG --- BUCK
    BUCK -- "5 V" --> ESP["ESP32"]
    WG --- ESP

    WP -- "fuse 2–3 A" --> B1["Board 1 POWERIN"]
    WG --- B1
    WP -- "fuse 2–3 A" --> B7["Board 7<br/>spare POWEROUT"]
    WG --- B7
    WP -- "fuse 2–3 A" --> B13["Board 13<br/>spare POWEROUT"]
    WG --- B13
    WP -- "fuse 2–3 A" --> B19["Board 19<br/>spare POWEROUT"]
    WG --- B19
    WP -- "fuse 2–3 A" --> B25["Board 25<br/>spare POWEROUT"]
    WG --- B25
    WP -- "fuse 2–3 A" --> B30["Board 30<br/>spare POWEROUT"]
    WG --- B30
```

- **One PSU only.** All feeds go back to the same Wago blocks.
- Feed wires: ≥ 0.75 mm², or thinner with an inline 2–3 A fuse in +24 V.
- **Polarity differs between outlets:** POWEROUT-S is the reverse of L/R.
  Measure each feed connector before putting it into the Wagos.
- The board numbers above are an example. Use whichever boards have a free
  power outlet, about every 6th board.

### Extra ground wires (recommended)

So that one bad power contact can't push current through the LVDS chips:

```mermaid
flowchart LR
    STAR(("GND star point<br/>(inside the sphere)"))
    STAR -- "≥1.5 mm²" --- WG["Wago GND / PSU"]
    STAR --- J1["Board 1 JP1 pin 2"]
    STAR --- J2["Board 2 JP1 pin 2"]
    STAR --- J3["…"]
    STAR --- J30["Board 30 JP1 pin 2"]
```

0.25–0.5 mm² per board is enough.

## 3. Board chain

DATA is re-sent by every board. CLK and LE are **one shared line** across all
30 boards, carried on by the solder bridges.

```mermaid
flowchart LR
    ESP["ESP32"] -- "CLK / DATA / LE<br/>3.3 V logic" --> B1

    subgraph B1["Board 1"]
        direction TB
        D4["4D: logic → LVDS"]
    end

    B1 == "DATA (re-sent)<br/>CLK + LE (shared line)" ==> B2["Board 2"]
    B2 == "via hermaphrodite<br/>connectors" ==> B3["Board 3"]
    B3 ==> DOTS["…"]
    DOTS ==> B30

    subgraph B30["Board 30 = end of line"]
        direction TB
        R["R2 + R3 = 100 Ω<br/>terminators"]
    end
```

| Part                     | Board 1    | Boards 2–29 | Last board |
|--------------------------|------------|-------------|------------|
| `4D` SN65LVDS047         | fitted     | **empty**   | **empty**  |
| R2 / R3 (100 Ω)          | empty      | empty       | **fitted** |
| CLK/LE solder bridges    | to the ONE outlet that continues the chain (same as DATA) | same | none |

The "last board" is wherever the CLK/LE line ends. There must be no branches,
otherwise every dead end needs its own terminator (see the debug PDF,
`pcb/diskohedron4w_revf_DEBUG.pdf`).
