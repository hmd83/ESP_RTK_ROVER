# COMMANDS.md — ComNav K803 Command Reference

How the **SinoGNSS K803 Lite** is taken out of ECOVACS base-station mode and turned into an
RTK rover, and how to change that configuration yourself.

- [How the receiver talks](#how-the-receiver-talks)
- [The point of no return](#the-point-of-no-return)
- [The shipped boot sequence](#the-shipped-boot-sequence)
- [Command reference](#command-reference)
  - [Unlogging](#unlogging)
  - [Port configuration](#port-configuration)
  - [Positioning mode](#positioning-mode)
  - [NMEA output](#nmea-output)
  - [Diagnostics](#diagnostics)
  - [Interface mode and differential input](#interface-mode-and-differential-input)
  - [Persistence and reset](#persistence-and-reset)
- [Output rate recipes](#output-rate-recipes)
- [RTCM input](#rtcm-input)
- [Changing the sequence in firmware](#changing-the-sequence-in-firmware)
- [Configuring from a PC](#configuring-from-a-pc)
- [Recovery: when the port stops answering](#recovery-when-the-port-stops-answering)
- [Gotchas](#gotchas)

---

## Status of the information here

| Mark | Meaning |
|---|---|
| ✅ | **In the shipped boot sequence and verified on the photographed K803 Lite.** |
| 🔷 | From the ComNav / NovAtel-style command set. Widely supported, but **not** exercised by this firmware — confirm on your unit before relying on it. |

Command availability varies with firmware build and **authorization**. A command your unit
does not support is rejected, not silently ignored — check the reply.

---

## How the receiver talks

- **Transport:** COM1, `115200 8N1`, 3.3 V LVTTL.
- **Syntax:** abbreviated ASCII, one command per line, case-insensitive in practice.
- **Terminator:** `\r\n` (CRLF). The firmware appends it; a terminal set to LF-only will hang
  with no reply.
- **Replies** depend on firmware build. Both of these are normal:

  ```
  <OK
  $command,LOG COM1 GPGGA ONTIME 1,response: OK*4A
  ```

- **Rejections** contain `Invalid` or `ERROR`:

  ```
  $command,RTKOBSMODE 0,response: Invalid Message ID*2F
  ```

- **Logs** are asynchronous. Replies arrive interleaved with whatever the receiver is already
  streaming, so never stop reading at the first newline. The firmware collects for a whole
  settle window (`GNSS_CMD_WAIT_MS`, default 600 ms) instead — see
  [`src/gnss_init.cpp`](src/gnss_init.cpp).

The firmware logs the whole exchange to the USB console:

```
[>] LOG COM1 GPGGA ONTIME 1
[<] $command,LOG COM1 GPGGA ONTIME 1,response: OK*4A
```

and flags failures:

```
    ^^ REJECTED: RTKOBSMODE 0
```

> `GNSS_CMD_PORT` in `src/config.h` is **the receiver's own port name** (`COM1`), i.e. the
> physical port your ESP32 is wired to. It has nothing to do with a Windows COM port number.

---

## The point of no return

```
INTERFACEMODE COM1 AUTO AUTO ON
```

This puts COM1 into **differential auto-detect**: the port starts interpreting incoming bytes
as RTCM corrections rather than as ASCII commands. That is exactly what an RTK rover needs —
and it means **the port stops accepting commands**.

Consequences:

1. Every `LOG` and every setting must be in place **before** this line. Ordering in the boot
   sequence is not cosmetic.
2. To get the command console back, you must first send the escape hatch — see
   [Recovery](#recovery-when-the-port-stops-answering).

---

## The shipped boot sequence

Run by `gnssInit()` on every boot, before Bluetooth starts. Roughly **13 seconds**:
2 × 1000 ms preamble + 17 × `GNSS_CMD_WAIT_MS` (600 ms).

Running it every boot is deliberate — the receiver is left in a known state no matter what
the previous session, a firmware experiment, or the mower did to it.

### Preamble

| # | Command | Why | |
|---|---|---|---|
| 1 | `INTERFACEMODE COMPASS COMPASS ON` | Escape hatch. A previous run left COM1 in RTCM auto-detect, where ASCII is ignored; this pulls it back to the ComNav ASCII/binary protocol. On a cold receiver it is a harmless no-op. **Must be first.** | ✅ |
| 2 | `LOG VERSION` | Proves the link works and records model, firmware build and authorization in the boot log. | ✅ |

### Main sequence

| # | Command | Why | |
|---|---|---|---|
| 3 | `UNLOGALL COM1` | Stop everything the factory base-station config was streaming on COM1. | ✅ |
| 4 | `UNLOGALL COM2` | Same for COM2 — the port that fed the QD-02 data module we dropped. | ✅ |
| 5 | `UNLOGALL COM3` | Same for COM3. | ✅ |
| 6 | `FIX NONE` | **Critical.** Clears the stored base-station coordinate. A base is `FIX POSITION`-ed to a surveyed point and will not compute a rover solution while that is set. | ✅ |
| 7 | `RTKOBSMODE 0` | Select **rover** observation mode. | ✅ |
| 8 | `RTKDYNAMICS FOOT` | Kinematic model. `FOOT` = walking survey. See the table below. | ✅ |
| 9 | `UNLOCKOUTALL` | Re-enable every satellite/constellation the base config may have locked out. | ✅ |
| 10 | `LOG COM1 GPGGA ONTIME 1` | Position, fix quality, sats, HDOP, altitude, **correction age**, base ID. The single most important sentence — NTRIP clients also echo it back to the caster. | ✅ |
| 11 | `LOG COM1 GPGST ONTIME 1` | Position-error statistics: σ latitude / σ longitude / σ altitude. This is where "6 mm" comes from. | ✅ |
| 12 | `LOG COM1 GPGSA ONTIME 1` | Active satellites, PDOP / HDOP / VDOP. | ✅ |
| 13 | `LOG COM1 GPGSV ONTIME 1` | Satellites in view with SNR. Useful for siting; the bulkiest sentence by far. | ✅ |
| 14 | `LOG COM1 GPRMC ONTIME 1` | Recommended minimum: position, speed, course, date. Some apps refuse to start without it. | ✅ |
| 15 | `LOG COM1 GPVTG ONTIME 1` | Course and speed over ground. | ✅ |
| 16 | `LOG COM1 GPZDA ONTIME 1` | UTC date and time. Lets an app set the device clock from GNSS. | ✅ |
| 17 | `LOG COMCONFIG` | Dump the port configuration to the boot log — **while the port still answers**. | ✅ |
| 18 | `INTERFACEMODE COM1 AUTO AUTO ON` | Enable differential auto-detect. RTCM3 from the phone now reaches the RTK engine. [Point of no return](#the-point-of-no-return). | ✅ |
| 19 | `INTERFACEMODE SAVECONFIG` | Persist. See the note under [Persistence](#persistence-and-reset). | ✅ |

After the sequence the firmware drains the echo tail so the first bytes a phone receives are
live NMEA, not a command transcript.

> `LOG COM1 …` targets are written with the literal port name from `GNSS_CMD_PORT`. If your
> ESP32 is wired to COM2, change that one define and the whole sequence follows.

---

## Command reference

### Unlogging

| Command | Effect | |
|---|---|---|
| `UNLOGALL COM1` | Stop all logs on COM1 | ✅ |
| `UNLOGALL` | Stop all logs on **all** ports | 🔷 |
| `UNLOG COM1 GPGSV` | Stop one specific log | 🔷 |

Always unlog before configuring. A factory base station streams RTCM and proprietary
messages continuously; leaving them on wastes bandwidth and confuses NMEA parsers.

### Port configuration

```
COM COM1 115200 N 8 1 N OFF
     │    │      │ │ │ │  └── handshake
     │    │      │ │ │ └───── parity (again, echo)
     │    │      │ │ └─────── stop bits
     │    │      │ └───────── data bits
     │    │      └─────────── parity
     │    └──────────────────  baud
     └───────────────────────  port
```

| Command | Effect | |
|---|---|---|
| `COM COM1 115200 N 8 1 N OFF` | Set COM1 to 115200 8N1, no handshaking | 🔷 |
| `LOG COMCONFIG` | Report the current port settings | ✅ |

> ⚠️ **Changing the baud rate will lock you out.** The receiver switches the moment the
> command is acknowledged; your terminal (or the ESP32) is still on the old rate. If you
> change it *and* save it, you must also change `GNSS_BAUD` in `src/config.h` and reflash,
> or the boot sequence will fail on every subsequent power-up. 115200 is fast enough for
> everything documented here — there is no reason to change it.

### Positioning mode

| Command | Effect | |
|---|---|---|
| `FIX NONE` | Clear any fixed/base coordinate — required for rover operation | ✅ |
| `RTKOBSMODE 0` | Rover observation mode | ✅ |
| `UNLOCKOUTALL` | Re-enable all satellites previously locked out | ✅ |
| `RTKDYNAMICS <mode>` | Kinematic model, see below | ✅ |
| `FIX POSITION <lat> <lon> <hgt>` | Turn the unit **back into a base** at a known coordinate | 🔷 |
| `LOCKOUT <sv>` | Exclude a specific satellite | 🔷 |

**`RTKDYNAMICS` modes:**

| Mode | Use |
|---|---|
| `FOOT` | Walking survey — **the firmware default**, and what you want for a pole rover |
| `LAND` | Vehicle-mounted |
| `AIR` | Aircraft / drone |
| `STATIC` | Stationary occupation |
| `AUTO` | Let the receiver decide |

Change it in `kCommands[]` if you are mounting the rover on a vehicle or a drone.

### NMEA output

```
LOG <port> <sentence> ONTIME <interval>
```

`<interval>` is in **seconds**: `1` = 1 Hz, `0.2` = 5 Hz, `0.1` = 10 Hz.

| Sentence | Contents | Consumed by |
|---|---|---|
| `GPGGA` | Time, position, **fix quality**, sats used, HDOP, MSL altitude, geoid separation, **correction age**, base station ID | Everything. Also uploaded to VRS/MAC casters |
| `GPGST` | σ latitude, σ longitude, σ altitude — the precision estimate | Dashboard "Quality", SW Maps ΔH/ΔV |
| `GPGSA` | Active satellites, PDOP / HDOP / VDOP | Dashboard DOPs |
| `GPGSV` | Satellites in view + SNR per satellite | Sky plots, siting |
| `GPRMC` | Position, speed, course, date | Apps that require RMC |
| `GPVTG` | Course and speed over ground | Motion display |
| `GPZDA` | UTC date and time | Clock sync |

**GGA fix-quality field (field 6)** — the number your whole setup is judged by:

| Value | Meaning | Typical accuracy |
|---|---|---|
| `0` | No fix | — |
| `1` | Single / autonomous | 1.5–3 m |
| `2` | DGPS / SBAS | 0.5–2 m |
| `4` | **RTK FIXED** | **~10 mm** ← the goal |
| `5` | RTK FLOAT | 0.2–1 m |

The dashboard and SW Maps both decode this. `4` is what you are aiming for.

> **Talker IDs:** ComNav emits `GP…` for these logs, but multi-constellation firmware may
> emit `GN…` (e.g. `$GNGGA`). Both the dashboard and every serious GIS app match on the last
> three characters, so either is fine. Some receivers accept `GNGGA` as a log name too; if
> `LOG COM1 GNGGA ONTIME 1` is rejected, use the `GP` form — the output talker ID is decided
> by the receiver, not by the log name.

### Diagnostics

#### `LOG VERSION`

```
LOG VERSION
```

Returns model, firmware build, serial number and enabled authorizations. Run this first on a
new unit — it tells you which constellations, frequencies and output rates you actually own.
✅

#### `LOG COMCONFIG`

Reports current port settings. The boot sequence runs it **before** `INTERFACEMODE … AUTO
AUTO ON`, because afterwards the port will not answer. ✅

#### Other useful logs

| Command | Purpose | |
|---|---|---|
| `LOG BESTPOSA ONTIME 1` | ComNav/NovAtel ASCII position with solution status and stdevs — richer than GGA | 🔷 |
| `LOG RTKPOSA ONTIME 1` | RTK solution detail | 🔷 |
| `LOG SATVISA ONTIME 10` | Satellite visibility | 🔷 |

These are ASCII logs, not NMEA. Handy on the bench; leave them off in the field, since GIS
apps will not parse them and they eat bandwidth.

### Interface mode and differential input

```
INTERFACEMODE <port> <rx-protocol> <tx-protocol> <responses on/off>
```

| Command | Effect | |
|---|---|---|
| `INTERFACEMODE COM1 AUTO AUTO ON` | Auto-detect on both directions — RTCM3 corrections in, logs out. **Rover mode.** | ✅ |
| `INTERFACEMODE COMPASS COMPASS ON` | Back to the ComNav protocol so ASCII commands are parsed again. **The escape hatch.** | ✅ |
| `INTERFACEMODE COM1 RTCMV3 NOVATEL ON` | Explicit: RTCM3 in, ComNav/NovAtel out. Use if `AUTO` misbehaves on your build | 🔷 |
| `INTERFACEMODE COM1 NOVATEL NOVATEL ON` | ASCII both ways — no corrections accepted | 🔷 |

With `AUTO` on the receive side, the RTCM3 stream your phone forwards is detected and fed to
the RTK engine with no further configuration. That is the entire correction path.

### Persistence and reset

| Command | Effect | |
|---|---|---|
| `SAVECONFIG` | Write the current configuration to non-volatile memory | 🔷 |
| `INTERFACEMODE SAVECONFIG` | The literal final line of the shipped sequence — see the note below | ✅ |
| `RESET` | Warm reset; configuration retained | 🔷 |
| `FRESET` | **Factory reset.** Wipes configuration, saved almanac and ephemeris | 🔷 ⚠️ |

> **On the last line of the boot sequence.** The firmware sends
> `INTERFACEMODE SAVECONFIG`; the documented ComNav form for persisting configuration is a
> bare `SAVECONFIG`. It does not matter operationally here, because the ESP32 replays the
> whole sequence on every boot and never relies on stored state — which is also why the
> rover survives someone else's experiments. If you want the configuration to persist
> independently of the ESP32, send a bare `SAVECONFIG` manually **before**
> `INTERFACEMODE COM1 AUTO AUTO ON`, and verify the response.

> `FRESET` also clears the almanac, so the next cold start is slow (minutes, not seconds).
> Prefer `UNLOGALL` + explicit configuration. Keep `FRESET` for a receiver that is genuinely
> stuck.

---

## Output rate recipes

Edit `kCommands[]` in [`src/gnss_init.cpp`](src/gnss_init.cpp) and reflash.

### 1 Hz — the default

```
LOG COM1 GPGGA ONTIME 1
LOG COM1 GPGST ONTIME 1
LOG COM1 GPGSA ONTIME 1
LOG COM1 GPGSV ONTIME 1
LOG COM1 GPRMC ONTIME 1
LOG COM1 GPVTG ONTIME 1
LOG COM1 GPZDA ONTIME 1
```

Right for point survey and mapping. ~1 KB/s.

### 5 Hz — moving rover

```
LOG COM1 GPGGA ONTIME 0.2
LOG COM1 GPGST ONTIME 0.2
LOG COM1 GPRMC ONTIME 0.2
LOG COM1 GPVTG ONTIME 0.2
LOG COM1 GPGSA ONTIME 1
LOG COM1 GPGSV ONTIME 5
LOG COM1 GPZDA ONTIME 1
```

Note what changed: the **position** sentences go to 5 Hz, while `GSV` — which is several
sentences per epoch across four constellations — drops to once every 5 s. Nothing on a map
needs a satellite list at 5 Hz.

### 10 Hz — machine guidance, vehicle tracking

```
LOG COM1 GPGGA ONTIME 0.1
LOG COM1 GPRMC ONTIME 0.1
LOG COM1 GPVTG ONTIME 0.1
LOG COM1 GPGST ONTIME 1
LOG COM1 GPGSA ONTIME 1
LOG COM1 GPZDA ONTIME 1
```

Drop `GSV` entirely and keep only what actually needs the rate.

### Bandwidth reality check

At 115200 baud you have about **11.5 KB/s**. A GGA is ~80 bytes; GSV across four
constellations can be **500+ bytes per epoch on its own**.

| Configuration | Approx. load | Verdict |
|---|---|---|
| Full set @ 1 Hz | ~1 KB/s | ✅ Comfortable |
| Position set @ 5 Hz + GSV @ 0.2 Hz | ~2 KB/s | ✅ Fine |
| Position set @ 10 Hz, no GSV | ~3 KB/s | ✅ Fine |
| **Full set including GSV @ 10 Hz** | **~10 KB/s** | ❌ Saturates the UART and the BLE link; sentences get dropped or truncated |

Symptoms of over-subscription: rising bad-checksum counts in the dashboard footer, truncated
sentences, an app that stutters. If you see that, cut `GSV` first.

> High output rates may require an authorization your unit does not have. If
> `ONTIME 0.1` is rejected, check `LOG VERSION`.

---

## RTCM input

Nothing to configure — `INTERFACEMODE COM1 AUTO AUTO ON` auto-detects the stream. What the
receiver expects from a caster:

| Message | Contents |
|---|---|
| 1005 / 1006 | Base station ARP coordinates |
| 1074 / 1075 / 1077 | GPS MSM observations |
| 1084 / 1085 / 1087 | GLONASS MSM |
| 1094 / 1095 / 1097 | Galileo MSM |
| 1124 / 1125 / 1127 | BeiDou MSM |
| 1230 | GLONASS code-phase biases |
| 1004 / 1012 | Legacy GPS / GLONASS observables |

Any standard NTRIP mount point serving RTCM 3.x works. **BeiDou (1124-series) matters** on a
ComNav receiver — the engine is strong on BDS, and a mount point without it leaves
performance on the table.

**Correction age**, in GGA field 13, is the health metric to watch:

| Age | Meaning |
|---|---|
| < 2 s | Healthy |
| 2–5 s | Acceptable |
| 5–10 s | Degrading — the dashboard turns this amber |
| > 10 s | Fix will drop to FLOAT — dashboard turns it red |

Growing age with a connected NTRIP client means the phone's cellular link is stalling, not
the receiver.

---

## Changing the sequence in firmware

All of it lives in one array in [`src/gnss_init.cpp`](src/gnss_init.cpp):

```c
static const char *const kCommands[] = {
    "UNLOGALL COM1",
    ...
    "LOG " GNSS_CMD_PORT " GPGGA ONTIME 1",
    ...
    "INTERFACEMODE " GNSS_CMD_PORT " AUTO AUTO ON",   // LAST
};
```

Rules:

1. `INTERFACEMODE … AUTO AUTO ON` **stays last.** Anything after it is shouted into a port
   that is no longer listening.
2. Add diagnostics before `LOG COMCONFIG`, not after.
3. `GNSS_CMD_PORT` is a macro — use the concatenation form so one define changes every line.
4. Watch the boot log for `REJECTED` after any change.
5. Set `GNSS_INIT_K803 0` in `src/config.h` to skip the whole thing — for a receiver you
   configured by hand, or a non-ComNav one (u-blox, Septentrio). The bridge still works; it
   just stops touching the receiver's configuration.

Timing: the sequence costs `2 s + (n × GNSS_CMD_WAIT_MS)`. Lowering `GNSS_CMD_WAIT_MS` speeds
up boot at the cost of possibly reading a reply too early — 600 ms is a comfortable margin,
300 ms usually works.

---

## Configuring from a PC

Useful for exploring the command set before committing anything to firmware.

1. Wire a **3.3 V** USB-TTL adapter to the harness: adapter RX ← receiver TX, adapter TX →
   receiver RX, GND ↔ GND. Power the receiver from its own supply.
2. Open a terminal at **115200 8N1** — PuTTY, `pio device monitor`, CoolTerm, RealTerm.
3. Set line ending to **CR+LF**. This is the most common reason "nothing responds".
4. Turn on local echo if you want to see what you type.
5. Start with:

   ```
   INTERFACEMODE COMPASS COMPASS ON
   LOG VERSION
   UNLOGALL COM1
   ```

   If `LOG VERSION` answers, you have a working console.

6. Explore. Then put whatever you settled on into `kCommands[]` and let the ESP32 do it
   every boot.

> With `INTERFACEMODE COM1 AUTO AUTO ON` already active from a previous session, the first
> command will appear to do nothing. Send the `COMPASS` line first — it is exactly why the
> firmware sends it too.

---

## Recovery: when the port stops answering

**Symptom:** every command returns `(no response)`, or the receiver streams NMEA and ignores
you completely.

**Cause, nine times out of ten:** the port is in RTCM differential auto-detect from a
previous session.

**Fix:**

```
INTERFACEMODE COMPASS COMPASS ON
```

Send it, wait a second, then try `LOG VERSION`. This is the first thing `gnssInit()` sends
on every boot, which is why the firmware recovers by itself from a state that would leave a
manual user stuck.

If it still does not answer, work through:

| Check | How |
|---|---|
| Wrong baud | Try 9600, 19200, 38400, 57600, 115200, 230400 |
| TX/RX swapped | Swap the pair. This is the most common wiring error |
| No common ground | Meter it. UART without a shared reference does not work |
| No power | Confirm 5 V at the harness |
| Line ending | Must be CR+LF |
| Receiver genuinely lost | `FRESET`, then reconfigure. Expect a slow first cold start |

The ESP32 boot log tells you which of these it is: `(no response)` on **every** line means
wiring, power or baud. Correct replies followed by `REJECTED` on specific commands means the
link is fine and only those commands are unsupported.

---

## Gotchas

- **Order is load-bearing.** `INTERFACEMODE … AUTO AUTO ON` last, always.
- **`FIX NONE` is not optional.** A leftover base coordinate silently prevents a rover
  solution, and the failure looks like "RTK never fixes" rather than like a configuration
  error.
- **Base-station leftovers.** The unit came configured to *transmit* corrections. Unlog
  COM2/COM3 as well — we dropped the QD-02 data link, and there is no reason to keep feeding
  a port nothing is listening on.
- **Reply parsing.** The firmware only looks for `Invalid` and `ERROR`. A command can be
  accepted syntactically and still be a no-op if the feature is not authorized — check
  `LOG VERSION`.
- **Don't save a baud change** unless you also change `GNSS_BAUD` and reflash.
- **Talker IDs vary.** `$GPGGA` vs `$GNGGA` is normal, and neither breaks anything.
- **Board revisions vary.** These commands are verified on one K803 Lite from one GOAT base
  station. If your unit behaves differently, please open an issue with `LOG VERSION` output
  and the boot log.

---

Next: **[FLASHING.md](FLASHING.md)** — getting the firmware onto the board.
