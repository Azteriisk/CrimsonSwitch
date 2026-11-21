# CrimsonSwitch

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![DirectX 12](https://img.shields.io/badge/DirectX-12-green.svg)](https://docs.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**CrimsonSwitch** is an instantaneous character unlock and direct switching mod for *Crimson Desert*. It unlocks **Damiane** and **Oongka** from the very beginning of the prologue, bypassing the story-locked progression gates and giving players direct 1-key mercenary switching throughout the entire game.

---

## Features

* **Instant Prologue Unlock:** Overrides the BlackSpace Engine character unlock bitmask (`0x0E`), immediately activating Damiane and Oongka on the in-game radial wheel.
* **Direct Hotkey Swapping (Gameplay Actions Protected):** Switch heroes instantly without interfering with combat keys (1/2/3 potions, Tab lock-on):
  * `[Alt + 1]` $\rightarrow$ Switch to **Kliff**
  * `[Alt + 2]` $\rightarrow$ Switch to **Damiane**
  * `[Alt + 3]` $\rightarrow$ Switch to **Oongka**
  * `[Alt + C]` $\rightarrow$ Cycle to Next Mercenary
  *(Configurable to dedicated Function keys F6–F9 or Numpad 1–3 in `CrimsonSwitch.ini`)*
* **Native In-Game Radial Wheel Unlocked:** Controller and keyboard players can also switch anytime using the native in-game radial wheel (holding `C` or Gamepad D-pad).
* **Zero-Footprint Standalone Launcher (`CrimsonSwitchLauncher.exe`):**
  * Never touches base game files on disk.
  * Injects purely into RAM for your play session.
  * Full **Hot-Attach** support: launch through Steam anytime, then double-click the launcher to inject into the running game in ~100ms.
* **Seamless Co-Op Compatibility:** Interoperates perfectly with [CrimsonCoop](https://github.com/Azteriisk/CrimsonCoop). In co-op sessions, character arbitration guards automatically ensure no two players pilot the same mercenary while allowing dynamic 2-player reserve swapping.

---

## Configuration (`CrimsonSwitch.ini`)

```ini
[General]
UnlockAllFromStart = true
EnableDirectHotkeys = true
SpawnMissingCompanions = true

[Hotkeys]
; 0x31 = '1', 0x32 = '2', 0x33 = '3', 0x09 = TAB
KeyKliff = 0x31
KeyDamiane = 0x32
KeyOongka = 0x33
KeyCycleNext = 0x09
```

---

## Building from Source

```powershell
cd C:\Users\thera\code\CrimsonSwitch
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## License

This project is licensed under the [MIT License](LICENSE).
