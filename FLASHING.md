# FLASHING.md — Getting the Firmware onto the Board

Three routes, easiest first:

| Route | Needs | For |
|---|---|---|
| **[Web flashing](#1-web-flashing-1-click)** | Chrome or Edge, a USB cable | Everyone. No toolchain, no IDE, no drivers to hunt down |
| **[PlatformIO](#2-platformio-from-source)** | VS Code + PlatformIO | Anyone changing `config.h` or the K803 command sequence |
| **[esptool](#3-esptool-manual)** | Python + esptool | CI, scripted installs, awkward setups |

- [Which build do I flash?](#which-build-do-i-flash)
- [1. Web flashing (1-click)](#1-web-flashing-1-click)
- [2. PlatformIO (from source)](#2-platformio-from-source)
- [3. esptool (manual)](#3-esptool-manual)
- [First boot — what a good log looks like](#first-boot--what-a-good-log-looks-like)
- [Switching between builds](#switching-between-builds)
- [Publishing your own installer page](#publishing-your-own-installer-page-maintainers)
- [Troubleshooting](#troubleshooting)

---

## Which build do I flash?

| Build | Board | Transport | Choose it if |
|---|---|---|---|
| `xiao_esp32c6_ble` | Seeed XIAO ESP32-C6 | BLE (Nordic UART) | **Recommended.** iOS + Android, modern GIS apps, fits inside the dome |
| `esp32dev_spp` | ESP32 DevKitC | Bluetooth Classic SPP | Legacy collectors, Windows GIS software, "choose a paired Bluetooth device" apps |
| `esp32dev_ble` | ESP32 DevKitC | BLE (Nordic UART) | Testing BLE on a DevKitC you already own |

Rule of thumb: **iPhone → BLE.** iOS gives apps no access to Bluetooth Classic SPP without
MFi certification, so a Classic build is invisible to it no matter what you do.

---

## 1. Web flashing (1-click)

[ESP Web Tools](https://esphome.github.io/esp-web-tools/) drives the ESP32 ROM bootloader
straight from the browser over Web Serial. No Arduino IDE, no PlatformIO, no CH340/CP2102
driver chase on most systems.

### Requirements

| | |
|---|---|
| Browser | **Chrome or Edge, desktop, version 89+**. Any Chromium-based browser with Web Serial |
| Not supported | Safari, Firefox (no Web Serial), and browsers on iOS |
| Android | Chrome on Android works with an OTG cable on many devices, but is fiddly — prefer a desktop |
| Page | Must be served over **HTTPS** (or `localhost`) |
| Cable | A **data** USB cable. Charge-only cables enumerate nothing and are a classic time sink |

### Steps

1. Open the installer page:
   **`https://hmd83.github.io/ESP_RTK_ROVER/install/`**
   ⚠️ *Not live yet — no release binaries have been published. Until then, use
   [PlatformIO](#2-platformio-from-source). Maintainers: see
   [Publishing your own installer page](#publishing-your-own-installer-page-maintainers).*
2. Plug the board into USB. **Disconnect the GNSS harness first** — flash it standalone.
3. Pick your build and click **Connect**.
4. Choose the serial port in the browser's dialog:
   - XIAO ESP32-C6 → `USB JTAG/serial debug unit` or similar
   - ESP32 DevKitC → `Silicon Labs CP210x` / `USB-SERIAL CH340`
5. Click **Install**, confirm the erase prompt, and wait ~1–2 minutes.
6. When it finishes, click **Logs & Console** to watch the first boot at 115200.

### If the board is not detected

Put it into download mode by hand:

| Board | How |
|---|---|
| XIAO ESP32-C6 | Hold **BOOT (B)**, tap **RESET (R)**, release **BOOT** |
| ESP32 DevKitC | Hold **BOOT**, tap **EN**, release **BOOT** |

Then click **Connect** again. Some DevKitC clones need this every time; that is normal for
boards without the auto-reset transistor pair.

Windows may still need a USB-serial driver for older DevKitC clones —
[CP210x](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers) or
[CH340](https://www.wch-ic.com/downloads/CH341SER_ZIP.html). The XIAO ESP32-C6 uses native
USB and needs nothing.

---

## 2. PlatformIO (from source)

The route to take the moment you want to change a pin, the device name, the NMEA rate, or the
K803 command sequence.

### Setup

1. Install [VS Code](https://code.visualstudio.com/), then the **PlatformIO IDE** extension.
2. Clone and open the project:

   ```powershell
   git clone https://github.com/hmd83/ESP_RTK_ROVER.git
   cd ESP_RTK_ROVER
   code .
   ```

First build downloads the toolchain, the [pioarduino](https://github.com/pioarduino/platform-espressif32)
ESP32 platform (Arduino core 3.x, required for the C6) and NimBLE. Expect a few minutes once.

### Build and upload

```powershell
pio run -e xiao_esp32c6_ble -t upload    # XIAO ESP32-C6, BLE
pio run -e esp32dev_spp     -t upload    # ESP32 DevKitC, Bluetooth Classic (repo default)
pio run -e esp32dev_ble     -t upload    # ESP32 DevKitC, BLE

pio run                                  # build default env only
pio run -t clean                         # clean
pio run -t erase                         # full chip erase
pio device list                          # find the port
pio device monitor                       # console @ 115200
pio device monitor -p COM7               # ...on a specific port
```

The monitor is configured with `esp32_exception_decoder` and timestamps, so a crash gives you
a decoded backtrace rather than raw addresses.

### Things worth changing in `src/config.h`

| Setting | Why you would touch it |
|---|---|
| `GNSS_BAUD` | Your receiver's COM1 is not at 115200 |
| `LINK_DEVICE_NAME` | You run more than one rover |
| `GNSS_INIT_K803` | Set `0` for an already-configured or non-ComNav receiver |
| `BLE_TX_POWER_DBM` | More BLE range, at the cost of current |
| `SPP_PIN` | An older Android insists on a pairing PIN |
| `DEBUG_USB` | Set `0` to silence the console |

Pins come from `platformio.ini` per environment and override the `config.h` defaults — change
them there, not in the header.

---

## 3. esptool (manual)

```powershell
pip install esptool
```

### Flash offsets

| Chip | Bootloader | Partitions | boot_app0 | Application |
|---|---|---|---|---|
| ESP32 (DevKitC) | `0x1000` | `0x8000` | `0xe000` | `0x10000` |
| ESP32-C6 (XIAO) | `0x0` | `0x8000` | `0xe000` | `0x10000` |

The bootloader offset differs by chip generation. Getting it wrong gives you a board that
never leaves the ROM loader.

### Flash the artifacts a PlatformIO build produced

```powershell
# XIAO ESP32-C6
esptool --chip esp32c6 --port COM7 --baud 921600 write_flash -z `
  0x0     .pio/build/xiao_esp32c6_ble/bootloader.bin `
  0x8000  .pio/build/xiao_esp32c6_ble/partitions.bin `
  0x10000 .pio/build/xiao_esp32c6_ble/firmware.bin

# ESP32 DevKitC
esptool --chip esp32 --port COM7 --baud 921600 write_flash -z `
  0x1000  .pio/build/esp32dev_spp/bootloader.bin `
  0x8000  .pio/build/esp32dev_spp/partitions.bin `
  0x10000 .pio/build/esp32dev_spp/firmware.bin
```

`boot_app0.bin` ships with the Arduino framework package
(`~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin`) and is
only needed for OTA-capable layouts.

Erase everything:

```powershell
esptool --chip esp32c6 --port COM7 erase_flash
```

> esptool v5 renamed some subcommands to hyphenated forms (`write-flash`, `erase-flash`,
> `merge-bin`). If a command is rejected, try the other spelling.

---

## First boot — what a good log looks like

Open the console at **115200**. On a XIAO you may need to reconnect after flashing — native
USB re-enumerates.

```
[GNSS] K803 Lite rover init on COM1
[>] INTERFACEMODE COMPASS COMPASS ON
[<] $command,INTERFACEMODE COMPASS COMPASS ON,response: OK*…
[>] LOG VERSION
[<] #VERSIONA,COM1,0,…;…,"K803Lite",…
[>] UNLOGALL COM1
[<] $command,UNLOGALL COM1,response: OK*…
…
[>] INTERFACEMODE COM1 AUTO AUTO ON
[<] $command,INTERFACEMODE COM1 AUTO AUTO ON,response: OK*…
[GNSS] init done - COM1 is in RTCM auto-detect mode
[BLE] advertising as "ESP32-RTK-4AAE" (xx:xx:xx:xx:4a:ae)
[SYS] ESP32-RTK-4AAE via BLE - UART1 @ 115200 (RX=GPIO17, TX=GPIO16)
[STAT] BLE waiting  up 0 B/s (0)  down 0 B/s (0)
```

**Reading it:**

| Line | Means |
|---|---|
| `[>]` / `[<]` | Command sent / receiver's reply |
| `[<] (no response)` on **every** line | Wiring, power or baud — the receiver is not talking. [HARDWARE.md](HARDWARE.md#bench-test-before-you-close-it) |
| `^^ REJECTED: …` | Link is fine, that specific command is unsupported. [COMMANDS.md](COMMANDS.md) |
| `[SYS] … via BLE` / `via SPP` | Confirms which build is actually running |
| `[STAT] … up 512 B/s` | Receiver is streaming **and** a phone is linked |
| `[STAT] … down 240 B/s` | RTCM corrections are reaching the receiver |

The whole init takes about **13 seconds** before Bluetooth comes up. That is expected — it is
17 commands at 600 ms each, plus a 2 s preamble.

> The init sequence runs before the link starts on purpose: the receiver echoes its replies,
> and a phone that connected early would otherwise be fed the entire command transcript as if
> it were NMEA.

---

## Switching between builds

`esp32dev_spp` uses the `huge_app.csv` partition layout (Bluedroid with Classic enabled does
not fit the default 1.4 MB app partition). The BLE builds use the default layout. **The
partition tables differ**, so switching between them without erasing leaves stale partition
data behind and produces boot loops that look like corrupted firmware.

```powershell
pio run -t erase                              # or: esptool --chip esp32 erase_flash
pio run -e xiao_esp32c6_ble -t upload
```

Erase first. Every time you change transport family.

---

## Publishing your own installer page (maintainers)

ESP Web Tools wants one **merged** binary per build plus a small JSON manifest, served over
HTTPS. GitHub Pages is free and sufficient.

### 1. Build every environment

```powershell
pio run -e xiao_esp32c6_ble
pio run -e esp32dev_spp
pio run -e esp32dev_ble
```

### 2. Merge each into a single flashable image

```powershell
# XIAO ESP32-C6 — bootloader at 0x0
esptool --chip esp32c6 merge_bin -o install/firmware/xiao-c6-ble.bin `
  --flash_mode dio --flash_freq 80m --flash_size 4MB `
  0x0     .pio/build/xiao_esp32c6_ble/bootloader.bin `
  0x8000  .pio/build/xiao_esp32c6_ble/partitions.bin `
  0x10000 .pio/build/xiao_esp32c6_ble/firmware.bin

# ESP32 DevKitC — bootloader at 0x1000
esptool --chip esp32 merge_bin -o install/firmware/esp32-spp.bin `
  --flash_mode dio --flash_freq 40m --flash_size 4MB `
  0x1000  .pio/build/esp32dev_spp/bootloader.bin `
  0x8000  .pio/build/esp32dev_spp/partitions.bin `
  0x10000 .pio/build/esp32dev_spp/firmware.bin
```

A merged image starts at offset `0`, which is why the manifest below has a single part.

### 3. Write a manifest per build

`install/manifest-xiao-c6-ble.json`:

```json
{
  "name": "ESP_RTK_ROVER · XIAO ESP32-C6 (BLE)",
  "version": "1.0.0",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-C6",
      "parts": [{ "path": "firmware/xiao-c6-ble.bin", "offset": 0 }]
    }
  ]
}
```

`install/manifest-esp32-spp.json`:

```json
{
  "name": "ESP_RTK_ROVER · ESP32 DevKitC (Bluetooth Classic SPP)",
  "version": "1.0.0",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [{ "path": "firmware/esp32-spp.bin", "offset": 0 }]
    }
  ]
}
```

`new_install_prompt_erase: true` matters here — it offers a full erase on first install,
which is exactly what the partition-layout difference above requires.

> If your esp-web-tools version rejects `"ESP32-C6"`, upgrade it. C6 support arrived with
> newer esptool-js releases.

### 4. Installer page

`install/index.html`:

```html
<script type="module"
        src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>

<h2>XIAO ESP32-C6 — BLE (recommended)</h2>
<esp-web-install-button manifest="manifest-xiao-c6-ble.json"></esp-web-install-button>

<h2>ESP32 DevKitC — Bluetooth Classic SPP</h2>
<esp-web-install-button manifest="manifest-esp32-spp.json"></esp-web-install-button>
```

Add a `<p slot="unsupported">` inside each button for a friendly message on Safari/Firefox.

### 5. Publish

GitHub Pages is already serving this repo from **`main` / root**, so committing the
`install/` folder is all it takes — the installer lands at
`https://hmd83.github.io/ESP_RTK_ROVER/install/`, alongside the
[dashboard](https://hmd83.github.io/ESP_RTK_ROVER/web/rtk-monitor.html).

Serve the manifest and the `.bin` from the **same origin** as the page, or configure CORS.
Bump `version` in the manifest on every release so returning users are offered the update.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| No port in the browser's dialog | Charge-only cable, missing driver, board not in download mode | Swap the cable first — it is usually the cable. Then drivers, then manual boot mode |
| `Failed to connect: No serial data received` | Not in download mode | Hold BOOT, tap RESET/EN, release BOOT, retry |
| `A fatal error occurred: MD5 of file does not match` | Bad cable or a flaky hub | Short cable, direct port, drop the baud to 115200 |
| Flash succeeds, board boot-loops | Wrong bootloader offset, or leftover partitions from another build | Full erase, then reflash. C6 = `0x0`, ESP32 = `0x1000` |
| `invalid header: 0xffffffff`, `TG0WDT_SYS_RESET` **with the GNSS attached** | **Power sag, not firmware** | [HARDWARE.md → Power](HARDWARE.md#power) |
| Console silent on a XIAO | Native USB re-enumerates after flashing | Reopen the monitor. The firmware waits up to 1.5 s for the host at boot |
| Console prints garbage | Baud mismatch | 115200 |
| Boots fine but the phone never sees it | Wrong build for the transport you expect | Check the `[SYS] … via SPP` / `via BLE` line |
| `[<] (no response)` on every init line | GNSS not connected, wrong baud, TX/RX swapped | [HARDWARE.md](HARDWARE.md#harness-pinout) |
| Board fine on USB, dies when the GNSS is plugged in | Shared power rail | Separate supply, share only GND |

**Bisecting a hardware problem:** flash and verify the board **standalone** first. Only then
add GND, then VCC, then the data lines — one at a time. Whichever step breaks it names the
culprit.

---

Next: back to the **[README](README.md)** for pairing and app setup, or
**[COMMANDS.md](COMMANDS.md)** to change what the receiver outputs.
