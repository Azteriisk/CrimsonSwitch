# Crimson Desert Early Trio Mercenary Unlock Architecture

## 1. Overview
In vanilla *Crimson Desert*, the campaign begins with the player locked exclusively into **Kliff**. Switching to the other two canon Greymane mercenaries (**Damiane** and **Oongka**) is gated behind mid-to-late story progression milestones and quest state flags.

**CrimsonSwitch** bypasses this restriction, allowing players to seamlessly switch between all 3 characters from the very first frame of the prologue.

---

## 2. Vanilla Gating Mechanism

The BlackSpace Engine evaluates character switching through a three-stage pipeline:

```mermaid
flowchart TD
    Input["Radial Wheel / Hotkey Pressed"] --> Eval["IsCharacterUnlocked(UserActor*, charId)"]
    Eval --> CheckBitmask{"Check UserActor + 0x148<br/>(unlockedMask & (1 << charId))"}
    
    CheckBitmask -->|Bit 0 (Locked)| Block["Lock Portrait on Radial<br/>Reject Switch Input"]
    CheckBitmask -->|Bit 1 (Unlocked)| Switch["Execute Switch Routine<br/>Rotate SubManager + 0x38"]
```

1. **UserActor Unlock Bitmask (`+0x148`):**  
   `pa::ClientUserActor` holds a 32-bit bitmask determining which characters the player has unlocked.
   - Bit 1 (`0x02`): Kliff (Unlocked by default).
   - Bit 2 (`0x04`): Damiane (Locked in prologue).
   - Bit 3 (`0x08`): Oongka (Locked in prologue).
2. **Radial Wheel Query:**  
   The UI radial menu inspects this bitmask every time it opens. If bits 2 and 3 are 0, Damiane and Oongka appear greyed out with lock icons.

---

## 3. The CrimsonSwitch Bypass

CrimsonSwitch implements a two-fold unlocking strategy:

### 3.1 Memory Bitmask Forcing
The mod actively enforces:
$$\text{UserActor} + 0x148 \mathrel{|}= \text{0x0E} \quad (\text{Kliff} \mid \text{Damiane} \mid \text{Oongka})$$
This illuminates and unlocks all three slots on the native in-game radial wheel immediately.

### 3.2 Evaluation Hook
MinHook intercepts `IsCharacterUnlocked`:
```cpp
bool WINAPI Hook_IsCharacterUnlocked(void* pUserActor, uint32_t charId) {
    if (charId >= 1 && charId <= 3) return true;
    return oIsCharacterUnlocked(pUserActor, charId);
}
```

### 3.3 Early Prologue Dynamic Spawning
In the prologue, before Damiane or Oongka have been recruited as AI followers in the world, their CCOIA instances may not yet be present in the 100-entry `ChildContainerActorComponent` list.
When switching:
1. If the companion CCOIA already exists in the scene, `SubManager + 0x38` is rotated to that pointer.
2. If not yet spawned, the mod signals the actor factory to instantiate the protagonist appearance descriptor:
   - Damiane: `character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml`
   - Oongka: `character/appearance/1_pc/1_phm/cd_phm_oongka/cd_phm_oongka_00000.app_xml`
   and binds them directly to `SubManager + 0x38` and `UserActor + 0xD8`.

---

## 4. Interoperability with CrimsonCoop

CrimsonSwitch is fully compatible with **CrimsonCoop**:
* **Solo Mode:** CrimsonSwitch gives you complete instant freedom to play as Kliff, Damiane, or Oongka anytime.
* **Co-Op Multiplayer (3-Player):** CrimsonCoop's `SessionManager` arbitration takes priority—if another player is actively piloting Damiane, swapping to Damiane is blocked to prevent duplicate character collisions.
* **Co-Op Multiplayer (2-Player Duo):** CrimsonSwitch integrates with CrimsonCoop's Reserve Slot, allowing either player to swap into the 3rd reserve hero instantly!
