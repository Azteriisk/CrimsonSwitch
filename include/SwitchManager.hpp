#pragma once

#include "Common.hpp"
#include <string>
#include <mutex>

namespace CrimsonSwitch {

struct SwitchStatus {
    CharacterType activeCharacter{CharacterType::Kliff};
    uint32_t      unlockedMask{0};
    uintptr_t     userActorAddress{0};
    uintptr_t     subManagerAddress{0};
    uintptr_t     kliffCcoia{0};
    uintptr_t     damianeCcoia{0};
    uintptr_t     oongkaCcoia{0};
    uintptr_t     controlledCcoia{0};
    bool          isInitialized{false};
};

class SwitchManager {
public:
    static SwitchManager& Instance();

    // Initialize with game module base and scan for actor manager
    bool Initialize();

    // Query active state
    CharacterType GetActiveCharacter();
    SwitchStatus GetStatus();

    // Force character unlock bitmask on UserActor (Unlocks radial wheel)
    bool ForceUnlockAll();

    // Execute character switch
    bool SwitchToCharacter(CharacterType targetChar, std::string& outMessage);
    bool CycleNextCharacter(std::string& outMessage);

    // Direct mock simulation for offline testing
    void SetupMockEnvironment(uintptr_t mockUserActor, uintptr_t mockSubMgr, uintptr_t mockKliff);

private:
    SwitchManager() = default;
    ~SwitchManager() = default;

    mutable std::mutex m_mutex;
    SwitchStatus m_status{};
    uintptr_t m_actorManagerGlobal{0};
    bool m_isMock{false};
};

} // namespace CrimsonSwitch
