#pragma once

#include "Common.hpp"
#include <string>
#include <mutex>
#include <functional>

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

    // Engine resolution helpers
    uintptr_t GetCompanionContainer();
    uintptr_t GetUserActor();
    uintptr_t GetClientActorManager();
    uintptr_t GetSelectOrActivateFunction() const { return m_fnSelectOrActivateCompanion; }
    uintptr_t GetFindCompanionFunction() const { return m_fnFindCompanionById; }
    uintptr_t GetSpawnCompanionFunction() const { return m_fnSpawnOrUnlockCompanion; }

    // Force character unlock bitmask on UserActor (Unlocks radial wheel)
    bool ForceUnlockAll();
    void DiscoverActors();

    // Execute character switch
    bool SwitchToCharacter(CharacterType targetChar, std::string& outMessage);
    bool CycleNextCharacter(std::string& outMessage);

    // Direct mock simulation for offline testing
    void SetupMockEnvironment(uintptr_t mockUserActor, uintptr_t mockSubMgr, uintptr_t mockKliff);
    void SetCoopArbitrationMock(std::function<bool(CharacterType)> isOccupiedFn);

private:
    SwitchManager() = default;
    ~SwitchManager() = default;

    mutable std::mutex m_mutex;
    SwitchStatus m_status{};
    uintptr_t m_gameInstanceGlobal{0};
    uintptr_t m_engineWorldGlobal{0};
    uintptr_t m_actorManagerGlobal{0};
    uintptr_t m_fnSelectOrActivateCompanion{0};
    uintptr_t m_fnFindCompanionById{0};
    uintptr_t m_fnSpawnOrUnlockCompanion{0};
    bool m_wheelPopulated{false};
    bool m_isMock{false};
    std::function<bool(CharacterType)> m_mockCoopArbitration{nullptr};
};

} // namespace CrimsonSwitch
