# Crimson Desert Trio Mercenary Unlock Architecture & Reversing Notes

> [!WARNING]
> **Research Document (Pre-Alpha):** This document describes the reverse-engineered BlackSpace Engine companion management architecture in `CrimsonDesert.exe` and current efforts to unlock the trio mercenaries early in the game.

---

## 1. Overview
In vanilla *Crimson Desert*, the campaign begins with the player locked exclusively into **Kliff**. Switching to the other two canon Greymane mercenaries (**Damiane** and **Oongka**) is gated behind mid-to-late story progression milestones and quest state flags.

**CrimsonSwitch** investigates bypassing this restriction, allowing players to seamlessly switch between all 3 characters from the prologue without modifying game files on disk.

---

## 2. Vanilla Gating Mechanism & Discovered Functions

Through static and dynamic analysis of `CrimsonDesert.exe` (Steam x64 build), the companion management subsystem has been mapped:

### 2.1 Core Disassembled Engine Routines

1. **`SpawnOrUnlockCompanion` (`RVA 0xa7dc20` / `0x140a7dc20`)**
   - Signature: `void __fastcall SpawnOrUnlockCompanion(void* compContainer, uint32_t charId)`
   - Scanned via AOB: `40 55 53 56 57 41 56 48 8D 6C 24 C9 48 81 EC A0 00 00 00 C5 F8 29 B4 24 90 00 00 00 8B DA 4C 8B F1 33 F6 48 8D 55 DF 48 8B 49 30`
   - Purpose: Validates story progression, calls the companion actor spawner (`0x140a98410`), sets active byte (`+0x30 = 1`), and configures the radial wheel availability status (`+0x35 = 0x0101`).
   - Progression Branch: Located at `0x140a7dc20 + 0x35`:
     ```assembly
     0F 84 23 01 00 00   je 0x140a7dd7e
     ```
     Bypassing this jump with 6 NOPs (`90 90 90 90 90 90`) forces execution into the unlock pipeline.

2. **`FindCompanionById` (`RVA 0xa7d7f0` / `0x140a7d7f0`)**
   - Signature: `CompanionEntry* __fastcall FindCompanionById(void* compContainer, uint32_t charId, bool allocateIfMissing)`
   - Scanned via AOB: `40 53 48 83 EC 20 8B DA 48 8B F9 83 FA 01 72 3B 83 FA 04 77 36`
   - Purpose: Searches the companion entry array in `compContainer + 0x140`. If `allocateIfMissing` is `true`, it increments the companion count at `compContainer + 0x148` and zero-initializes a new `0x58`-byte entry.

3. **`SelectOrActivateCompanion` (`RVA 0xa8ed30` / `0x140a8ed30`)**
   - Signature: `void __fastcall SelectOrActivateCompanion(void* compContainer, uint32_t charId)`
   - Scanned via AOB: `40 53 48 83 EC 20 8B DA 48 8B F9 E8`
   - Purpose: The engine's native selection handler invoked when a companion slot is selected from the UI radial menu.

---

## 3. CompanionContainer & Struct Layouts

### Pointer Chain to CompanionContainer:
```
EngineWorldGlobal (base + 0x724B348)
  └── [0x30] SubWorld
        └── [0x1230] PlacementContext
              └── [0x90] CompanionContainer
```

### `CompanionContainer` Offsets:
- `+0x30` (`uintptr_t`): Definitions Context
  - `+0x30 -> +0x50` (`uintptr_t`): Companion Archetype & Asset Registry (must be non-null before spawning)
- `+0x140` (`uintptr_t`): Pointer to Companion Entries array (stride `0x58`)
- `+0x148` (`uint32_t`): Active Companion Count (0 in early prologue, 1 after Kliff initializes, 3 when trio registered)

### `CompanionEntry` (0x58 bytes):
```cpp
struct CompanionEntry {
    uint32_t charId;          // +0x00: 1 = Kliff, 2 = Damiane, 3 = Oongka
    uint8_t  padding04[0x2C]; // +0x04
    uint8_t  isActive;        // +0x30: 1 if companion is registered/active
    uint8_t  padding31[0x03]; // +0x31
    uint8_t  selectionState;  // +0x34: 1 if selectable
    uint16_t wheelFlags;      // +0x35: 0x0101 illuminates and enables slot on radial wheel
    uint8_t  padding37[0x01]; // +0x37
    uintptr_t ccoiaActor;     // +0x38: Pointer to Character Controller Object In-world Actor
    uint8_t  padding40[0x18]; // +0x40
};
```

---

## 4. Current Reverse-Engineering Challenges

1. **Hollow Entries:** Calling `FindCompanionById(compContainer, id, true)` creates entries where `ccoiaActor (+0x38) == NULL`. The engine UI checks `ccoiaActor != NULL` before illuminating the portrait; without an actor, it displays a locked gray helmet.
2. **Actor Factory Invocation:** `SpawnOrUnlockCompanion` invokes `0x140a98410` to spawn the actor. Early in Chapter 1, this factory call does not instantiate the 3D entity, likely due to an uninitialized scene layer or missing story quest registration.
3. **Target Appearance XML Assets:**
   - Kliff: `character/appearance/1_pc/1_phm/cd_phm_macduff/cd_phm_macduff_00000.app_xml`
   - Damiane: `character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml`
   - Oongka: `character/appearance/1_pc/1_phm/cd_phm_oongka/cd_phm_oongka_00000.app_xml`

Contributions, IDA/Ghidra pseudocode, and hooks are welcome!
