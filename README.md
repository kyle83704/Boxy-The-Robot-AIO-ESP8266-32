# Boxy The Robot AIO

PlatformIO firmware and web controller for Boxy, with build targets for ESP32
and ESP8266. Configure the robot's controller, web interface, melody, optional
hardware, and pin assignments independently for each build environment.

## Contents

- [Requirements and first build](#requirements-and-first-build)
- [Build and upload](#build-and-upload)
- [Configuration flags](#configuration-flags)
- [Ready-to-use configurations](#ready-to-use-configurations)
- [Wi-Fi and controller access](#wi-fi-and-controller-access)
- [Hardware and pin assignments](#hardware-and-pin-assignments)
- [Making and selecting songs](#making-and-selecting-songs)
- [Changing the web interface](#changing-the-web-interface)
- [Project structure](#project-structure)
- [GitHub checklist](#github-checklist)

## Requirements and first build

Install [PlatformIO Core](https://platformio.org/install/cli) or the
PlatformIO IDE extension for Visual Studio Code. The supplied environments
target:

| Environment | Default board | PlatformIO board ID |
|---|---|---|
| ESP32 | ESP32 Dev Module | `esp32dev` |
| ESP8266 | NodeMCU 1.0 (ESP-12E) | `nodemcuv2` |

Open a terminal in the folder containing `platformio.ini`. PlatformIO fetches
the toolchains and libraries on the first build.

## Build and upload

Build one board or both:

```text
pio run -e esp32
pio run -e esp8266
pio run
```

Upload to a connected board:

```text
pio run -e esp32 -t upload
pio run -e esp8266 -t upload
```

If PlatformIO cannot identify the serial port, specify it:

```text
pio run -e esp32 -t upload --upload-port COM5
pio run -e esp8266 -t upload --upload-port COM6
```

Replace the example COM ports with the port shown by your operating system.
To use a different board, change the relevant `board` value in
`platformio.ini` to a supported PlatformIO board ID.

The compiled firmware files are written under `.pio/build/esp32/` and
`.pio/build/esp8266/`. Do not add `.pio/` build output to a source commit.

## Configuration flags

Add a `build_flags` block inside `[env:esp32]` or `[env:esp8266]` in
`platformio.ini`. Flags are independent per environment. If an environment
already has `build_flags`, add the new `-D` options to that block rather than
adding a second `build_flags` key.

### Controller and theme

| Flag | Values | Default | Description |
|---|---|---:|---|
| `BOXY_SINGLE_INTERFACE` | `-1`, `0`-`3` | `-1` | `-1` shows every interface; `0` Joystick; `1` Dual Joystick; `2` Roam; `3` ByLine. |
| `BOXY_LIGHT_THEME` | `0` or `1` | `0` | `0` uses dark colors; `1` uses light colors. |

These settings affect the web controller served by the robot. They do not
change the browser-based song tools or standalone HTML files.

### Songs

| Flag | Value | Melody |
|---|---:|---|
| `BOXY_SONG` | `0` | Custom melody in `aio_boxy_roam6/song.h` (default). |
| `BOXY_SONG` | `1` | Built-in Boxy Riff. |
| `BOXY_SONG` | `2` | Built-in ascending scale. |

Music is played on the buzzer during Roam mode while moving forward. The buzzer
is monophonic; it plays one note at a time.

### Optional hardware

Set a feature flag to `0` to disable that hardware in the firmware. Set it to
`1` to enable it.

| Flag | Default | Effect when disabled |
|---|---:|---|
| `BOXY_ENABLE_MOTORS` | `1` | Motor outputs do nothing; driving interfaces are hidden. |
| `BOXY_ENABLE_SERVO` | `1` | Servo is not initialized or moved; angle controls are hidden. |
| `BOXY_ENABLE_ULTRASONIC` | `1` | No ultrasonic measurements; distance status is hidden. |
| `BOXY_ENABLE_IR_SENSORS` | `1` | IR pins are not read; IR status is hidden and Roam does not use IR obstacle checks. |
| `BOXY_ENABLE_BUZZER` | `1` | Horn and song playback do nothing; buzzer controls are hidden. |
| `BOXY_ENABLE_NEOPIXEL` | `1` | NeoPixel library and strip are not initialized. |
| `BOXY_ENABLE_STATUS_LED` | `1` on ESP32, `0` on ESP8266 | Status LED is not initialized; its control/status is hidden. |

The webpage obtains these settings from the firmware at `/ui-config.js`, so its
theme, selected interface, and hardware controls stay in sync with the build.

### Pin overrides

Board-specific defaults and the complete list of pin override names are in
[`hardware_config.h`](aio_boxy_roam6/hardware_config.h). Override a pin in
`build_flags`, for example:

```ini
build_flags =
    -D BOXY_PIN_BUZZER=5
    -D BOXY_PIN_LEFTA=16
```

Available pin flags are `BOXY_PIN_LEFTA`, `BOXY_PIN_LEFTB`,
`BOXY_PIN_RIGHTA`, `BOXY_PIN_RIGHTB`, `BOXY_PIN_STATUS_LED`,
`BOXY_PIN_BUZZER`, `BOXY_PIN_SERVO`, `BOXY_PIN_ULTRASONIC_TRIG`,
`BOXY_PIN_ULTRASONIC_ECHO`, `BOXY_PIN_NEOPIXEL`, `BOXY_PIN_IR_LEFT`, and
`BOXY_PIN_IR_RIGHT`. Values are GPIO numbers, not Arduino board aliases.

Review the board pinout before changing pins. Do not assign the same GPIO to
two attached devices. ESP8266 GPIO0, GPIO2, and GPIO15 affect boot mode and
must remain at the board's required logic levels during startup.

## Ready-to-use configurations

Copy the requested flags into the indicated existing environment section in
`platformio.ini`, then build/upload using the command shown. Keep the existing
`platform`, `board`, and `lib_deps` entries.

### 1. Full controller, dark theme

This is the default configuration for either supported board: all four
interfaces and hardware features are enabled, using the custom song.

No `build_flags` are required:

```text
pio run -e esp32
pio run -e esp8266
```

### 2. Joystick-only interface with light theme

Add under `[env:esp32]` for an ESP32 joystick controller:

```ini
build_flags =
    -D BOXY_SINGLE_INTERFACE=0
    -D BOXY_LIGHT_THEME=1
```

Build or upload with `pio run -e esp32` or
`pio run -e esp32 -t upload`.

### 3. Autonomous Roam on ESP8266 with a built-in tune

Add under `[env:esp8266]`:

```ini
build_flags =
    -D BOXY_SINGLE_INTERFACE=2
    -D BOXY_LIGHT_THEME=1
    -D BOXY_SONG=1
```

Roam needs motors to move. The servo and ultrasonic sensor enable its scanning
and distance-based steering; with either disabled, that part of obstacle
avoidance is unavailable. IR sensors add close-obstacle detection and can be
disabled with `-D BOXY_ENABLE_IR_SENSORS=0` if they are not installed.
Build/upload with `pio run -e esp8266` or `pio run -e esp8266 -t upload`.

### 4. ByLine controller without sensors or NeoPixels

Add under either board's environment:

```ini
build_flags =
    -D BOXY_SINGLE_INTERFACE=3
    -D BOXY_ENABLE_ULTRASONIC=0
    -D BOXY_ENABLE_IR_SENSORS=0
    -D BOXY_ENABLE_NEOPIXEL=0
```

This keeps the motors, buzzer, and servo active while removing the unused
sensors and lights.

### 5. Motor test build

To keep only the driving UI and motor outputs, add under a board environment:

```ini
build_flags =
    -D BOXY_SINGLE_INTERFACE=0
    -D BOXY_ENABLE_SERVO=0
    -D BOXY_ENABLE_ULTRASONIC=0
    -D BOXY_ENABLE_IR_SENSORS=0
    -D BOXY_ENABLE_BUZZER=0
    -D BOXY_ENABLE_NEOPIXEL=0
    -D BOXY_ENABLE_STATUS_LED=0
```

Use a secured, raised robot and test low motor speeds first.

### 6. Minimal hardware build

For a controller with no motor or accessory hardware, add:

```ini
build_flags =
    -D BOXY_ENABLE_MOTORS=0
    -D BOXY_ENABLE_SERVO=0
    -D BOXY_ENABLE_ULTRASONIC=0
    -D BOXY_ENABLE_IR_SENSORS=0
    -D BOXY_ENABLE_BUZZER=0
    -D BOXY_ENABLE_NEOPIXEL=0
    -D BOXY_ENABLE_STATUS_LED=0
```

The driving modes and motor controls are hidden because the motors are
disabled. The device still serves the web page and WebSocket interface.

## Wi-Fi and controller access

Without station credentials, the firmware starts an open access point named
`Boxy-AP`. Join that Wi-Fi network and open `http://192.168.4.1/` in a browser.
When station credentials are configured, firmware tries that network for up
to 10 seconds and falls back to the access point if it cannot connect. On a
connected local network, the configured hostname is `geff`; mDNS may make it
available at `http://geff.local/`.

To use a home network, add these flags to the relevant environment's
`build_flags`:

```ini
    -D BOXY_STA_SSID=\"your-wifi-name\"
    -D BOXY_STA_PASSWORD=\"your-wifi-password\"
```

Replace the example values locally. **Never commit real Wi-Fi credentials to
GitHub.** If committing a sample configuration, retain obvious placeholders
only. Rebuild and upload after changing credentials.

## Hardware and pin assignments

The ESP32 pin assignments retain the original project wiring. The ESP8266
defaults are a compact example map and may need to be changed for a specific
board and wiring harness. In particular:

- ESP8266 GPIO1 and GPIO3 are used for NeoPixel data and ultrasonic echo, so
  serial logging is disabled.
- ESP8266 GPIO2 is also the default ultrasonic trigger pin. Do not enable the
  separate status LED on that same pin while using this default ultrasonic
  wiring; change a pin assignment if both features are needed.
- The ESP8266 status LED feature defaults to disabled.
- ESP8266 GPIO0, GPIO2, and GPIO15 are boot-strapping pins. Attached circuits
  must not prevent the board from booting.
- Ultrasonic echo pins must not receive a voltage above the microcontroller's
  GPIO rating. Use an appropriate level shifter or divider when required by
  the sensor.
- Power servos and motor drivers from suitable supplies; do not power motors
  directly from ESP GPIO pins.

Always check your exact development board and modules before wiring. Disable
unused hardware features and update pin overrides to avoid conflicts.

## Making and selecting songs

The robot plays melodies using a buzzer; it does not play audio files or
polyphonic music. The melody plays repeatedly during Roam mode when the robot
is in its forward-moving state. Disabling `BOXY_ENABLE_BUZZER` also disables
song playback.

### Edit the custom song

1. Open [`song-converter.html`](aio_boxy_roam6/song-converter.html) in a
   browser.
2. Paste Arduino melody source containing note `#define`s, an `int melody[]`
   array, and an `int tempo` value. Use music you created or have permission
   to use.
3. Convert the melody and copy the generated code into
   [`song.h`](aio_boxy_roam6/song.h).
4. Ensure the generated identifiers are named `customsongMelody`,
   `customsongTempo`, and `customsongNotes`. If the converter generated
   different names, rename those three identifiers to match.
5. Build with `BOXY_SONG=0`, or omit `BOXY_SONG` because `0` is the default.

The melody array alternates note and duration values:

```cpp
const int customsongMelody[] = {
  NOTE_C4, 8, NOTE_E4, 8, NOTE_G4, 4, REST, 4
};
const int customsongTempo = 120;
const int customsongNotes =
    sizeof(customsongMelody) / sizeof(customsongMelody[0]) / 2;
```

The note is a frequency constant such as `NOTE_C4`; the duration is a divider
of a whole note (`8` is an eighth note, `4` a quarter note). `REST` creates
silence. The tempo is in beats per minute. A negative duration divider
indicates a dotted note in the Arduino song format.

### Add another built-in song

Add a melody array and its tempo/note count in
[`music_songs.h`](aio_boxy_roam6/music_songs.h), add a new `BOXY_SONG` case,
and update the flag's validation error and this README's song table. Use the
same alternating note/duration format. Then build each target to verify the
song compiles for both platforms.

## Changing the web interface

The embedded controller page is [`modes.h`](aio_boxy_roam6/modes.h), which
contains the HTML, CSS, and JavaScript. Change the embedded file when you want
to modify the firmware-served UI. [`light-theme.html`](aio_boxy_roam6/light-theme.html)
is a standalone design reference and is not compiled into the firmware.

[`web_ui.cpp`](aio_boxy_roam6/web_ui.cpp) serves the embedded page and the
generated `/ui-config.js` endpoint. Keep the configuration endpoint in place:
the page uses it to select the interface, theme, and enabled hardware controls.

## Project structure

| File | Purpose |
|---|---|
| [`platformio.ini`](platformio.ini) | ESP32/ESP8266 environments, boards, and library dependencies. |
| [`aio_boxy_roam6.cpp`](aio_boxy_roam6/aio_boxy_roam6.cpp) | Firmware entry points, robot behavior, sensors, Wi-Fi, and WebSocket commands. |
| [`hardware_config.h`](aio_boxy_roam6/hardware_config.h) | Hardware feature flags, board pins, and optional hardware wrappers. |
| [`music_player.cpp`](aio_boxy_roam6/music_player.cpp) | Non-blocking buzzer melody playback. |
| [`music_songs.h`](aio_boxy_roam6/music_songs.h) | Selects the custom or built-in melody at compile time. |
| [`song.h`](aio_boxy_roam6/song.h) | Editable custom song data and note definitions. |
| [`web_ui.cpp`](aio_boxy_roam6/web_ui.cpp) | HTTP routes and generated web UI build configuration. |
| [`modes.h`](aio_boxy_roam6/modes.h) | Embedded controller HTML, CSS, and JavaScript. |
| [`song-converter.html`](aio_boxy_roam6/song-converter.html) | Browser tool for converting Arduino melody source. |

## GitHub checklist

Before publishing or committing:

1. Build both supported targets: `pio run -e esp32` and
   `pio run -e esp8266`.
2. Review `platformio.ini` for personal credentials and remove them or replace
   them with placeholders.
3. Confirm board models, pin wiring, and hardware feature defaults match the
   documented setup.
4. Include source files and documentation; exclude `.pio/`, compiled firmware,
   editor state, and personal configuration.
5. Credit and follow the license terms for any melody or library source you
   add. Do not commit songs unless you have permission to redistribute them.
