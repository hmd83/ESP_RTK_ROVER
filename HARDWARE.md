# HARDWARE.md — Teardown, Pinout & Wiring

Everything needed to get a **ComNav SinoGNSS K803 Lite** out of an **ECOVACS GOAT RTK base
station** and onto an ESP32.

- [Before you start](#before-you-start)
- [What is inside the dome](#what-is-inside-the-dome)
- [Teardown](#teardown)
- [The K803 Lite](#the-k803-lite)
- [Harness pinout](#harness-pinout)
- [Wiring — XIAO ESP32-C6](#wiring--xiao-esp32-c6-primary)
- [Wiring — ESP32 DevKitC](#wiring--esp32-devkitc-classic-spp)
- [Power](#power)
- [PPS (optional)](#pps-optional)
- [Antenna](#antenna)
- [Reassembly and mounting](#reassembly-and-mounting)
- [Bench test before you close it](#bench-test-before-you-close-it)
- [Photo checklist](#photo-checklist)

---

## Before you start

> ⚠️ Opening the base station **voids any remaining warranty** and permanently ends its
> usefulness as a mower base. Buy a unit that is already surplus.

- Everything here is **low voltage** (5 V from a USB supply). There is no mains circuitry
  inside, and no battery to puncture in the base-station dome.
- **Disconnect power before touching anything.**
- The K803 is a static-sensitive OEM module on a carrier board. Ground yourself. Handle the
  board by its edges.
- Do not hot-plug the UART. Connect GND first, always.
- Work over a tray. The mounting screws are small and the ground plane is heavy.

**Tools:** PH1 and PH0 screwdrivers, a plastic spudger, a fine soldering iron (only if your
board revision has no mating connector), a multimeter, and a USB-TTL adapter if you want to
talk to the receiver from a PC first.

---

## What is inside the dome

| Part | What it does | Do we use it? |
|---|---|---|
| **SinoGNSS K803 Lite** OEM module | The GNSS + RTK engine. This is the prize. | ✅ Everything |
| **QinNav QD-02** module | The base station's own data link to the mower. **We drop it** — the phone link is Bluetooth BLE/SPP instead. | ❌ Not used |
| Carrier board | Power regulation, port breakout, status LED, button | ✅ As power + port breakout |
| Patch antenna + machined ground plane | Multi-band antenna with a proper ground plane under it | ✅ Kept exactly as-is |
| Weatherproof dome, gasket, pole mount | Housing, already IP-rated and pole-mountable | ✅ Reused as the rover housing |

The ground plane and antenna are a real part of why this performs like a survey rover
instead of like a phone GPS. Keep them together; do not "improve" the stack-up.

<p align="center">
  <img src="media/teardown-board-harness.jpeg" alt="Base station opened: K803 Lite carrier board on its ground plane, 4-wire harness routed to the lid" width="500"><br>
  <sub>Opened unit. The 4-wire harness (yellow/blue/green/orange) leaves the carrier board's
  white JST header and runs to the ESP32 mounted in the lid.</sub>
</p>

---

## Teardown

1. **Unscrew the dome from the pole mount** and disconnect the original power cable.
2. **Remove the base screws.** They sit around the underside rim, under the rubber feet on
   some revisions. Keep them separated — lengths differ.
3. **Lift the lid straight up, slowly.** A short twisted pair (red/black on the photographed
   unit) runs from the carrier board to the lid. Do not yank it. Unplug it at the board if
   you need the halves fully apart.
4. **Note the gasket seat** before you disturb it. You need it later for weatherproofing.
5. **Identify the boards.** The larger shielded can labelled `K803Lite GNSS Module` /
   `SinoGNSS by ComNav Technology Ltd.` is the receiver. The second, smaller shielded module
   (`QinNav QD-02`) is the base station's data link — we drop it entirely and use Bluetooth
   BLE/SPP instead, so leave it alone and unpowered.
6. **Find the white JST header** on the carrier board next to the K803. This is COM1 plus
   power, and it is where the whole project connects. See [Harness pinout](#harness-pinout).
7. **Leave the receiver board bolted to the ground plane.** There is no reason to remove it,
   and every reason not to disturb the antenna feed.

> Board revisions vary between GOAT models (G1 / O800 / A1600) and production runs.
> Silkscreen, connector position and pin order are **not guaranteed** to match the photos.
> Verify with a multimeter before you connect anything — see below.

> **Placeholder:** `media/teardown-01-screws.jpg` — underside with screw positions circled.
> **Placeholder:** `media/teardown-02-lid-open.jpg` — lid lifted, internal cable visible.
> **Placeholder:** `media/teardown-03-board-overview.jpg` — annotated board: K803, QD-02,
> JST header, button, antenna feed.

---

## The K803 Lite

Markings on the photographed unit:

```
SinoGNSS  by ComNav Technology Ltd.
K803Lite GNSS Module
PN: 2180300014
SN: 01A009673059
```

Indicative capability, from ComNav's published K803 material. **Your unit's actual enabled
feature set depends on its authorization** — confirm with `LOG VERSION`
(see [COMMANDS.md](COMMANDS.md#log-version)):

| | |
|---|---|
| Constellations | BDS, GPS, GLONASS, Galileo (multi-frequency) |
| RTK accuracy | ~8 mm + 1 ppm horizontal, ~15 mm + 1 ppm vertical |
| Correction input | RTCM 3.x |
| Output | NMEA 0183 + ComNav/NovAtel-style ASCII and binary logs |
| Ports | COM1 / COM2 / COM3, **3.3 V LVTTL**, default 115200 8N1 |
| Time pulse | 1 PPS output |
| Typical draw | 150–300 mA during acquisition — see [Power](#power) |

Observed in the field on this build: **ΔH 6 mm, ΔV 9 mm** with a 1.0 s correction age.

> The COM ports are **3.3 V logic** and connect directly to an ESP32 or ESP32-C6 with no
> level shifter. Do **not** feed 5 V into a COM pin. If you are unsure about your board
> revision, scope the receiver's TX line before wiring it up — idle should sit at 3.3 V.

---

## Harness pinout

Four wires on the white JST header next to the K803: **5 V, GND, GNSS TX, GNSS RX**.

<p align="center">
  <img src="media/pinout-k803-harness.jpeg" alt="Carrier board with 5V, GND, GNSS TX and GNSS RX called out on the white JST header" width="720">
</p>

| Signal | Wire colour in the photo | Direction (receiver's point of view) | Goes to |
|---|---|---|---|
| **5 V** | orange | Power **in** | ESP32 `5V` / `VIN`, or a separate 5 V supply |
| **GND** | yellow | — | ESP32 `GND` (**always connect, always first**) |
| **GNSS TX** | blue | **Out** — NMEA to you | ESP32 **RX** pin |
| **GNSS RX** | green | **In** — RTCM3 from you | ESP32 **TX** pin |

Wire colours are this build's convention, not a standard. Verify the pins on **your** board
before powering anything:

1. **GND** — continuity to the ground plane / mounting screws / USB shell.
2. **5 V** — with the original supply connected, measure 5 V ±5 % to GND. Disconnect again.
3. **TX vs RX** — power the board from its original supply and probe each candidate pin with
   a USB-TTL adapter at 115200 8N1. The one spitting text (`$GNGGA,…` or ComNav ASCII) is
   the receiver's **TX**. The remaining one is **RX**.
4. If the pins are unlabelled and you cannot identify RX, wire TX+GND first and confirm you
   receive data. Then add RX and confirm the receiver answers `LOG VERSION`.

> **Placeholder:** `media/pinout-jst-closeup.jpg` — macro of the JST header with pin 1 marked.
> **Placeholder:** `media/pinout-silkscreen.jpg` — silkscreen labels beside the header.

---

## Wiring — XIAO ESP32-C6 (primary)

Build env `xiao_esp32c6_ble`. `UART1`, `PIN_GNSS_RX=17`, `PIN_GNSS_TX=16`.

| K803 harness | XIAO pad | GPIO | Note |
|---|---|---|---|
| GNSS **TX** (blue) | **D7** | GPIO17 | ESP32 RX ← receiver TX |
| GNSS **RX** (green) | **D6** | GPIO16 | ESP32 TX → receiver RX |
| **GND** (yellow) | **GND** | — | Mandatory common ground |
| **5 V** (orange) | **5V** | — | VBUS passthrough from USB-C. Read [Power](#power). |

```
 ECOVACS GOAT base station                     Seeed XIAO ESP32-C6
 ┌───────────────────────────────┐             ┌───────────────────┐
 │  K803 Lite  COM1              │             │                   │
 │   TX  ●───────── blue ────────┼─────────────┤ D7 (GPIO17)  RX   │
 │   RX  ●───────── green ───────┼─────────────┤ D6 (GPIO16)  TX   │
 │  GND  ●───────── yellow ──────┼─────────────┤ GND               │
 │  5 V  ●───────── orange ──────┼─────────────┤ 5V (VBUS)         │
 │                               │             │                   │
 │  patch antenna + groundplane  │             │  USB-C ───────────┼──▶ power bank
 └───────────────────────────────┘             └───────────────────┘
```

The photographed build mounts the XIAO **inside the dome lid** and runs a single USB-C cable
out through the existing cable gland to a power bank. That is the whole rover: one dome, one
cable, one battery.

Notes specific to the C6:

- The C6 has **no Bluetooth Classic radio**. BLE builds only — this is by design, and it is
  also what iOS requires.
- `ARDUINO_USB_CDC_ON_BOOT=1`: the console is the native USB port. It only enumerates once
  the host is attached, so the firmware waits up to 1.5 s at boot before logging.
- The onboard user LED is **active-low**; the build sets `LED_ACTIVE_LOW=1`.
- The XIAO ESP32-C6 has an RF switch for its internal/external antenna. The firmware does
  not touch it, so the board's default applies. If BLE range disappoints inside a sealed
  dome, that switch and an external antenna are the first thing to look at.

---

## Wiring — ESP32 DevKitC (Classic SPP)

Build env `esp32dev_spp`. `UART2`, `PIN_GNSS_RX=16`, `PIN_GNSS_TX=17`.

| K803 harness | DevKitC pin | Silkscreen |
|---|---|---|
| GNSS **TX** (blue) | **GPIO16** | `RX2` |
| GNSS **RX** (green) | **GPIO17** | `TX2` |
| **GND** (yellow) | `GND` | — |
| **5 V** (orange) | `VIN` / `5V` | Read [Power](#power) |

> ⚠️ **Do not use GPIO1/GPIO3 (`TX0`/`RX0`).** They belong to the onboard USB-serial chip.
> The receiver would fight the USB bridge, and flashing and the serial monitor would stop
> working. UART2 on GPIO16/17 is the correct spare port.

> ⚠️ **ESP32-WROVER modules:** GPIO16/17 are consumed by PSRAM. Move the receiver to e.g.
> GPIO25/GPIO26 and rebuild with `-DPIN_GNSS_RX=25 -DPIN_GNSS_TX=26`. Plain WROOM-32
> DevKitC boards are fine as shipped.

---

## Power

**This is the single most common failure in the whole project.** Read it before wiring.

### The numbers

- K803 Lite: **150–300 mA** during acquisition, more at cold start.
- ESP32 with Bluetooth Classic TX peaks: several hundred mA in short bursts.
- A DevKitC's onboard AMS1117 regulator plus a thin USB cable cannot serve both.

### The failure mode

Powering the receiver from the DevKitC's `3V3` or `VIN` pin puts it on the same USB rail as
the ESP32. The receiver's inrush plus a Bluetooth TX peak sags 5 V below the regulator's
dropout, the 3.3 V rail follows, and the **flash chip stops responding** to the ROM
bootloader:

```
rst:0x7 (TG0WDT_SYS_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
invalid header: 0xffffffff
flash read err, 988
```

There is usually **no** `Brownout detector was triggered` message — the detector trips at
about 2.43 V, but flash reads already fail nearer 2.8 V. People chase this as a firmware bug
for hours. It is not a firmware bug.

### Fix, in order

1. **Power the receiver from its own supply**, sharing **only GND** with the ESP32.
2. Add **470–1000 µF electrolytic + 100 nF ceramic** across 5 V / GND at the board.
3. Use a wall adapter or powered hub, and a **short, thick** USB cable. Charge-only cables
   and 1 m of 28 AWG are both fatal here.

### Topologies that work

| Setup | Verdict |
|---|---|
| Power bank → XIAO USB-C → XIAO `5V` pin → receiver | ✅ The photographed build. Power bank sources amps happily; the XIAO's 5 V pin is straight VBUS, not a regulator output. |
| Separate 5 V supply → receiver; USB → ESP32; **GND tied together** | ✅ Most robust. Do this on the bench. |
| USB → DevKitC → `VIN` → receiver, thin cable | ⚠️ Marginal. Add bulk capacitance or expect boot loops. |
| DevKitC `3V3` pin → receiver | ❌ Through the onboard regulator. Do not. |
| Power supplies not sharing a ground | ❌ No common reference; UART will not work, and you can damage pins. |

### Bisecting a boot loop

Add one wire at a time: **GND** → **VCC** → **data lines**. Whichever step first triggers the
loop names the culprit. If it loops with GND + VCC and **no data wires attached**, it is
definitively power.

---

## PPS (optional)

The K803 exposes a **1 PPS** time-pulse output. **The firmware does not use it** — nothing in
`src/` reads a PPS pin, and NMEA timestamps are enough for GIS work.

Wire it only if you are building something that needs hardware-timed sampling (camera
triggering, sensor fusion, an NTP server):

| Signal | Level | Suggested ESP32 pin | Notes |
|---|---|---|---|
| PPS | 3.3 V logic, ~1 Hz pulse | any free GPIO with interrupt capability | Keep the lead short; it is a timing reference |

You will need to locate the PPS pad on your carrier board revision — it is not part of the
4-wire harness above. Attach a scope or a logic analyser and look for the 1 Hz pulse once
the receiver has a position fix; PPS is typically not asserted before then.

> **Placeholder:** `media/pps-pad.jpg` — PPS test point identified on the carrier board.

---

## Antenna

Leave the antenna stack alone. The dome contains a multi-band patch over a machined metal
ground plane, at the spacing its designer chose — this is a big part of the multipath
rejection you are getting for 50 €.

> ⚠️ **It is not a characterised antenna.** There is no ANTEX calibration for it, so the
> offset between the electrical phase centre and any physical point you measure to is unknown
> — systematic, mostly in height, and invisible in the receiver's own σ estimates. Read
> [README → Antenna phase centre](README.md#antenna-phase-centre--the-uncharacterised-part)
> before trusting a height. Measuring to a consistent point and orienting the dome the same
> way every setup makes that offset constant, which is what makes it calibratable later.

Practical siting matters more than any modification:

- **Open sky.** Under a tree or against a wall you get FLOAT, not FIX.
- **Away from reflectors.** Metal roofs, cars and wet surfaces cause multipath.
- **On a pole, not on the ground.** The dome has a standard mount; use it.
- **Let it settle.** Cold start to RTK FIX in open sky is typically 30–60 s.

---

## Reassembly and mounting

1. **Strain-relieve the USB cable** at the gland. The cable will get tugged; the XIAO's USB-C
   connector should never be what takes the load.
2. **Insulate the ESP32.** Kapton tape or a printed tray. Do not let the board rest against
   the ground plane or the antenna feed.
3. **Keep the RF path clear.** Do not route the harness or the ESP32 directly over the patch
   antenna.
4. **Reseat the gasket** and check that the lid closes without pinching wires.
5. **Test outdoors before you seal it.** See below.

> **Placeholder:** `media/assembly-esp-in-lid.jpg` — XIAO mounted in the lid with strain
> relief.
> **Placeholder:** `media/assembly-closed.jpg` — closed unit on the pole with the USB tail.

---

## Bench test before you close it

1. Connect **GND only**, then power the receiver from its own supply. Confirm no boot loop.
2. Flash the firmware ([FLASHING.md](FLASHING.md)) and open the serial monitor at 115200.
3. Add the receiver **TX → ESP RX** wire. In the boot log, the `[<]` lines should show
   replies from the K803 rather than `(no response)`:

   ```
   [GNSS] K803 Lite rover init on COM1
   [>] INTERFACEMODE COMPASS COMPASS ON
   [<] $command,INTERFACEMODE COMPASS COMPASS ON,response: OK*…
   [>] LOG VERSION
   [<] #VERSIONA,COM1,…
   ```

4. Add the **ESP TX → receiver RX** wire and reboot. Commands should now be accepted; watch
   for `REJECTED` lines (see [COMMANDS.md](COMMANDS.md)).
5. Take it outside. Within a minute the `[STAT]` line should show real uplink traffic:

   ```
   [STAT] BLE linked  up 512 B/s (25600)  down 0 B/s (0)
   ```

6. Connect a phone, start an NTRIP client, and confirm `down` goes above 0 and the fix
   reaches **RTK FIX**.
7. *Then* close the dome.

---

## Photo checklist

Photos worth capturing while it is open — drop them in `media/` and the placeholders above
become real:

| File | Shot |
|---|---|
| `teardown-01-screws.jpg` | Underside, screw positions |
| `teardown-02-lid-open.jpg` | Lid lifted, internal cable |
| `teardown-03-board-overview.jpg` | Annotated board: K803, QD-02, JST, button, antenna feed |
| `pinout-jst-closeup.jpg` | JST header macro, pin 1 marked |
| `pinout-silkscreen.jpg` | Silkscreen labels beside the header |
| `wiring-xiao.jpg` | XIAO wired up, wire colours visible |
| `wiring-devkitc.jpg` | DevKitC equivalent |
| `pps-pad.jpg` | PPS test point |
| `assembly-esp-in-lid.jpg` | ESP32 mounted and strain-relieved |
| `assembly-closed.jpg` | Finished unit on a pole |
| `dashboard.png` | Web dashboard with a live RTK fix |

---

Next: **[COMMANDS.md](COMMANDS.md)** — how the K803 is switched from base to rover.
