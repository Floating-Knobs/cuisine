# Setting up a workspace

From a bare computer to a flashing board. The only hardware you need is the
instrument and a USB cable.

## What you need

| | |
|---|---|
| computer | Windows, macOS or Linux |
| USB cable | the board's CH340 is a USB-serial bridge, not a mass-storage device |
| the board | only for flashing; building works without it |

## 1. Git

| platform | how |
|---|---|
| Windows | [git-scm.com/download/win](https://git-scm.com/download/win), default options are fine |
| macOS | `brew install git`, or the Xcode command line tools |
| Linux | your package manager, e.g. `sudo apt install git` |

Check it: `git --version`.

## 2. Editor and PlatformIO

Either editor works; the project is developed in Cursor.

**Option A — the extension (recommended).** Open VS Code or Cursor →
Extensions → search **PlatformIO IDE** → install. It brings PlatformIO Core,
`avr-gcc` and the framework with it, and installs what it needs on first run.
This is the least error-prone path because the editor and the toolchain share an
environment.

**Option B — standalone core.** `pip install -U platformio`, or the official
installer from platformio.org. Then `pio` is available from any terminal.

If `pio` is reported as not found, it is installed but not on `PATH`:

| platform | location |
|---|---|
| Windows | `%USERPROFILE%\.platformio\penv\Scripts` |
| macOS / Linux | `~/.platformio/penv/bin` |

Using the editor's integrated terminal sidesteps this entirely, because the
extension adds it to `PATH` there.

## 3. USB-serial driver

The board talks through a **CH340**. Without a driver, no port appears and
flashing fails with `unable to open programmer`.

| platform | what to do |
|---|---|
| Windows | install the WCH **CH341SER** driver, then check Device Manager → *Ports (COM & LPT)* for `USB-SERIAL CH340 (COMx)` |
| macOS | usually built in; the port appears as `/dev/cu.wchusbserial*` |
| Linux | usually built in; add yourself to `dialout` (`sudo usermod -aG dialout $USER`) and log back in |

## 4. Get the code

```
git clone <repository-url>
```

Downloading the ZIP also works, but then updating means downloading again by
hand. A path containing spaces is fine; some PlatformIO tooling is happier
without one.

## 5. First build

Open the folder that contains `platformio.ini` — the repository root, not `src/`
or `lib/`.

```
pio run -e ATmega4809
```

The first build downloads the `atmelmegaavr` platform and takes a few minutes.
Later builds take seconds.

Expected result:

```
RAM:   [=====     ]  53.4% (used 3280 bytes from 6144 bytes)
Flash: [==========]  95.7% (used 47030 bytes from 49152 bytes)
SUCCESS
```

Those figures move by a few dozen bytes between commits; what matters is that
`SUCCESS` appears and the flash percentage is read against 48,640 B, not the
49,152 B PlatformIO prints.

The flash percentage is computed against 49,152 B, but the image links at
`0x200`, so the real ceiling is **48,640 B** — see
[firmware.md](firmware.md#reading-the-numbers).

## 6. First flash

```
pio device list
pio run -e ATmega4809 -t upload --upload-port COM5
```

Substitute the port `pio device list` reports; it changes when the board is
replugged. The upload takes about 17 s: write, then verify.

**The production image is silent on serial by design.** If you open a terminal
and see nothing, that is not a failed build. Flash the test firmware instead if
you want the board to answer:

```
pio run -e bench -t upload --upload-port COM5
```

## Troubleshooting

| symptom | cause | fix |
|---|---|---|
| `pio: command not found` | not on `PATH` | use the editor's terminal, or add the directory from step 2 |
| `unable to open programmer arduino on port COMx` | wrong port, or no driver | `pio device list`; install the CH340 driver |
| `Device Descriptor Request Failed` / code 43 | the CH340 has wedged | unplug and replug the board; only that fixes it |
| `address 0xc0xx out of range` | the image passed 48,640 B | shorten a string or drop a `Serial.print` |
| first build takes minutes | platform download | wait once, it is cached |
| upload works, board seems dead | production is silent | flash `bench` and send `?` |
