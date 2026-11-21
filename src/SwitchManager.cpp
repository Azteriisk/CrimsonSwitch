#include "SwitchManager.hpp"
#include "EngineTypes.hpp"
#include "PatternScan.hpp"
#include "Logger.hpp"
#include <iostream>

namespace CrimsonSwitch {

SwitchManager& SwitchManager::Instance() {
    static SwitchManager instance;
    return instance;
}

// 1. Root GameInstance pattern from RVA 0x9e9bd0:
// 48 8B 05 [disp32] 48 8B 48 20 44 8B 47 38
static const char* GAME_INSTANCE_AOB = "48 8B 05 ?? ?? ?? ?? 48 8B 48 20 44 8B 47 38";

// 2. SelectOrActivateCompanion from RVA 0xa8ed30:
static const char* SELECT_COMPANION_AOB = "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 40 8B DA 48 8B F1 48 8B 91 40 01 00 00";

// 3. FindCompanionById from RVA 0xa7d7f0:
static const char* FIND_COMPANION_AOB = "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 20 8B DA 48 8B E9 48 8B 91 40 01 00 00";

bool SwitchManager::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isMock) {
        m_status.isInitialized = true;
        return true;
    }

    HMODULE hGame = GetModuleHandleW(nullptr);
    if (!hGame) return false;

    uintptr_t base = reinterpret_cast<uintptr_t>(hGame);

    // 1. Scan for GameInstanceGlobal & EngineWorldGlobal
    uintptr_t scanAddr = PatternScanner::Scan(hGame, GAME_INSTANCE_AOB);
    if (scanAddr != 0) {
        m_gameInstanceGlobal = PatternScanner::ResolveRipRelative(scanAddr, 3, 7);
        m_engineWorldGlobal = m_gameInstanceGlobal - sizeof(uintptr_t);
        Logger::Log("[SwitchManager] Resolved GameInstanceGlobal via AOB: 0x%p (EngineWorldGlobal: 0x%p)\n",
                    (void*)m_gameInstanceGlobal, (void*)m_engineWorldGlobal);
    } else {
        m_gameInstanceGlobal = base + 0x724B350;
        m_engineWorldGlobal  = base + 0x724B348;
        Logger::Log("[SwitchManager] Using verified RVA GameInstanceGlobal: 0x%p (EngineWorldGlobal: 0x%p)\n",
                    (void*)m_gameInstanceGlobal, (void*)m_engineWorldGlobal);
    }
    m_actorManagerGlobal = m_gameInstanceGlobal;

    // 2. Scan for SelectOrActivateCompanion
    uintptr_t scanSelect = PatternScanner::Scan(hGame, SELECT_COMPANION_AOB);
    if (scanSelect != 0) {
        m_fnSelectOrActivateCompanion = scanSelect;
        Logger::Log("[SwitchManager] Resolved SelectOrActivateCompanion via AOB: 0x%p\n", (void*)m_fnSelectOrActivateCompanion);
    } else {
        m_fnSelectOrActivateCompanion = base + 0xa8ed30;
        Logger::Log("[SwitchManager] Using verified RVA SelectOrActivateCompanion: 0x%p\n", (void*)m_fnSelectOrActivateCompanion);
    }

    // 3. Scan for FindCompanionById
    uintptr_t scanFind = PatternScanner::Scan(hGame, FIND_COMPANION_AOB);
    if (scanFind != 0) {
        m_fnFindCompanionById = scanFind;
        Logger::Log("[SwitchManager] Resolved FindCompanionById via AOB: 0x%p\n", (void*)m_fnFindCompanionById);
    } else {
        m_fnFindCompanionById = base + 0xa7d7f0;
        Logger::Log("[SwitchManager] Using verified RVA FindCompanionById: 0x%p\n", (void*)m_fnFindCompanionById);
    }

    m_status.isInitialized = true;
    return true;
}

void SwitchManager::SetupMockEnvironment(uintptr_t mockUserActor, uintptr_t mockSubMgr, uintptr_t mockKliff) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isMock = true;
    m_status.isInitialized = true;
    m_status.userActorAddress = mockUserActor;
    m_status.subManagerAddress = mockSubMgr;
    m_status.kliffCcoia = mockKliff;
    m_status.controlledCcoia = mockKliff;
    m_status.activeCharacter = CharacterType::Kliff;
    m_status.unlockedMask = 0x02; // Vanilla starts with only Kliff (bit 1)
}

void SwitchManager::SetCoopArbitrationMock(std::function<bool(CharacterType)> isOccupiedFn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mockCoopArbitration = std::move(isOccupiedFn);
}

uintptr_t SwitchManager::GetCompanionContainer() {
    if (m_isMock) return 0x14002000ULL;
    if (m_engineWorldGlobal == 0) return 0;

    __try {
        if (IsBadReadPtr(reinterpret_cast<const void*>(m_engineWorldGlobal), sizeof(uintptr_t))) return 0;
        uintptr_t engineWorld = *reinterpret_cast<uintptr_t*>(m_engineWorldGlobal);
        if (engineWorld < 0x10000 || engineWorld > 0x7FFFFFFFFFFF) return 0;

        uintptr_t subWorldAddr = engineWorld + 0x30;
        if (IsBadReadPtr(reinterpret_cast<const void*>(subWorldAddr), sizeof(uintptr_t))) return 0;
        uintptr_t subWorld = *reinterpret_cast<uintptr_t*>(subWorldAddr);
        if (subWorld < 0x10000 || subWorld > 0x7FFFFFFFFFFF) return 0;

        uintptr_t placementAddr = subWorld + 0x1230;
        if (IsBadReadPtr(reinterpret_cast<const void*>(placementAddr), sizeof(uintptr_t))) return 0;
        uintptr_t placement = *reinterpret_cast<uintptr_t*>(placementAddr);
        if (placement < 0x10000 || placement > 0x7FFFFFFFFFFF) return 0;

        uintptr_t compContainerAddr = placement + 0x90;
        if (IsBadReadPtr(reinterpret_cast<const void*>(compContainerAddr), sizeof(uintptr_t))) return 0;
        uintptr_t compContainer = *reinterpret_cast<uintptr_t*>(compContainerAddr);
        if (compContainer < 0x10000 || compContainer > 0x7FFFFFFFFFFF) return 0;

        return compContainer;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uintptr_t SwitchManager::GetClientActorManager() {
    if (m_isMock) return 0x14001000ULL;
    if (m_gameInstanceGlobal == 0) return 0;

    __try {
        if (IsBadReadPtr(reinterpret_cast<const void*>(m_gameInstanceGlobal), sizeof(uintptr_t))) return 0;
        uintptr_t gameInstance = *reinterpret_cast<uintptr_t*>(m_gameInstanceGlobal);
        if (gameInstance < 0x10000 || gameInstance > 0x7FFFFFFFFFFF) return 0;

        uintptr_t subContextAddr = gameInstance + 0x20;
        if (IsBadReadPtr(reinterpret_cast<const void*>(subContextAddr), sizeof(uintptr_t))) return 0;
        uintptr_t subContext = *reinterpret_cast<uintptr_t*>(subContextAddr);
        if (subContext < 0x10000 || subContext > 0x7FFFFFFFFFFF) return 0;

        uintptr_t actorMgrAddr = subContext + 0x1150;
        if (IsBadReadPtr(reinterpret_cast<const void*>(actorMgrAddr), sizeof(uintptr_t))) return 0;
        uintptr_t actorMgr = *reinterpret_cast<uintptr_t*>(actorMgrAddr);
        if (actorMgr < 0x10000 || actorMgr > 0x7FFFFFFFFFFF) return 0;

        return actorMgr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uintptr_t SwitchManager::GetUserActor() {
    if (m_isMock) return m_status.userActorAddress;
    uintptr_t actorMgr = GetClientActorManager();
    if (actorMgr == 0) return 0;

    __try {
        uintptr_t userActorAddr = actorMgr + BlackSpaceOffsets::MGR_TO_USER_ACTOR; // +0x58
        if (IsBadReadPtr(reinterpret_cast<const void*>(userActorAddr), sizeof(uintptr_t))) return 0;
        uintptr_t userActor = *reinterpret_cast<uintptr_t*>(userActorAddr);
        if (userActor < 0x10000 || userActor > 0x7FFFFFFFFFFF) return 0;

        return userActor;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool SwitchManager::ForceUnlockAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isMock) {
        m_status.unlockedMask = ALL_MERCENARIES_UNLOCKED_MASK;
        Logger::Log("[SwitchManager] [MOCK] Forced unlocked character bitmask: 0x%X (Kliff, Damiane, Oongka)\n",
                    m_status.unlockedMask);
        return true;
    }

    // CRITICAL GUARD: Only run when the player is actually loaded in the game world!
    // During splash screens, engine boot, and main menu, UserActor is null (0).
    // Calling companion registration while outside the live world causes engine launch crashes.
    uintptr_t userActor = GetUserActor();
    if (userActor == 0) {
        m_wheelPopulated = false; // Reset state when outside active game world
        return false;
    }

    bool updated = false;

    // 1. Bitmask unlock on UserActor
    __try {
        uintptr_t maskAddr = userActor + BlackSpaceOffsets::USER_ACTOR_UNLOCKED_MASK;
        if (!IsBadWritePtr(reinterpret_cast<void*>(maskAddr), sizeof(uint32_t))) {
            uint32_t* pMask = reinterpret_cast<uint32_t*>(maskAddr);
            if ((*pMask & ALL_MERCENARIES_UNLOCKED_MASK) != ALL_MERCENARIES_UNLOCKED_MASK) {
                *pMask |= ALL_MERCENARIES_UNLOCKED_MASK;
                m_status.unlockedMask = *pMask;
                Logger::Log("[SwitchManager] Unlocked Greymane bitmask to 0x%X on UserActor 0x%p!\n",
                            *pMask, (void*)userActor);
                updated = true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // 2. Add Damiane (2) and Oongka (3) to CompanionContainer so in-game radial wheel populates!
    // CRITICAL CONDITIONS:
    // - Player must be loaded in-world (userActor != 0 checked above)
    // - Wheel not already populated in this session (!m_wheelPopulated)
    // - Count must be >= 1 (Kliff is present in live world) AND < 3
    if (!m_wheelPopulated && m_fnSelectOrActivateCompanion != 0) {
        uintptr_t compContainer = GetCompanionContainer();
        if (compContainer != 0) {
            __try {
                uintptr_t countAddr = compContainer + 0x148;
                if (!IsBadReadPtr(reinterpret_cast<const void*>(countAddr), sizeof(uint32_t))) {
                    uint32_t count = *reinterpret_cast<uint32_t*>(countAddr);
                    if (count >= 1 && count < 3) {
                        auto fnActivate = reinterpret_cast<void* (__fastcall*)(void*, uint32_t)>(m_fnSelectOrActivateCompanion);
                        Logger::Log("[SwitchManager] In-game companion count (%u) < 3. Registering Damiane & Oongka into wheel...\n", count);
                        fnActivate(reinterpret_cast<void*>(compContainer), 2); // Register Damiane
                        fnActivate(reinterpret_cast<void*>(compContainer), 3); // Register Oongka

                        // Re-activate current hero
                        uint32_t activeCid = (m_status.activeCharacter == CharacterType::Damiane) ? 2 :
                                             ((m_status.activeCharacter == CharacterType::Oongka) ? 3 : 1);
                        fnActivate(reinterpret_cast<void*>(compContainer), activeCid);

                        uint32_t newCount = *reinterpret_cast<uint32_t*>(countAddr);
                        Logger::Log("[SwitchManager] Radial wheel updated! Companion count is now %u (Kliff, Damiane, Oongka ready).\n", newCount);
                        m_wheelPopulated = true;
                        updated = true;
                    } else if (count >= 3) {
                        m_wheelPopulated = true;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("[SwitchManager] Exception guarded in CompanionContainer activation.\n");
            }
        }
    }

    return updated;
}

CharacterType SwitchManager::GetActiveCharacter() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status.activeCharacter;
}

SwitchStatus SwitchManager::GetStatus() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

bool SwitchManager::SwitchToCharacter(CharacterType targetChar, std::string& outMessage) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (targetChar == m_status.activeCharacter) {
        outMessage = std::string("Already playing as ") + CharacterToString(targetChar) + ".";
        return false;
    }

    // 1. Interop check: Mock arbitration (for automated offline tests)
    if (m_mockCoopArbitration && m_mockCoopArbitration(targetChar)) {
        outMessage = std::string(CharacterToString(targetChar)) + " is currently piloted by a co-op peer!";
        Logger::Log("[SwitchManager] [INTEROP] Switch blocked: %s\n", outMessage.c_str());
        return false;
    }

    // 2. Interop check: Real runtime coordination with CrimsonCoop.dll if co-loaded
    HMODULE hCoop = GetModuleHandleW(L"CrimsonCoop.dll");
    if (hCoop) {
        typedef bool (*tCoopOccupied)(uint8_t);
        typedef bool (*tCoopSwitch)(uint8_t);

        auto fnOccupied = reinterpret_cast<tCoopOccupied>(GetProcAddress(hCoop, "CrimsonCoop_IsCharacterOccupied"));
        auto fnSwitch = reinterpret_cast<tCoopSwitch>(GetProcAddress(hCoop, "CrimsonCoop_RequestCharacterSwitch"));

        uint8_t cid = (targetChar == CharacterType::Kliff) ? 1 : ((targetChar == CharacterType::Damiane) ? 2 : 3);
        if (fnOccupied && fnOccupied(cid)) {
            outMessage = std::string(CharacterToString(targetChar)) + " is currently piloted by a co-op peer!";
            Logger::Log("[SwitchManager] [CrimsonCoop Interop] Blocked switch: %s\n", outMessage.c_str());
            return false;
        }
        if (fnSwitch) {
            fnSwitch(cid);
            Logger::Log("[SwitchManager] [CrimsonCoop Interop] Notified CrimsonCoop session of switch to %s.\n",
                        CharacterToString(targetChar));
        }
    }

    if (m_isMock) {
        CharacterType old = m_status.activeCharacter;
        m_status.activeCharacter = targetChar;
        outMessage = std::string("Switched from ") + CharacterToString(old) + " to " + CharacterToString(targetChar) + "!";
        Logger::Log("[SwitchManager] [MOCK] %s\n", outMessage.c_str());
        return true;
    }

    uintptr_t compContainer = GetCompanionContainer();
    if (compContainer != 0 && m_fnSelectOrActivateCompanion != 0) {
        __try {
            uint32_t cid = (targetChar == CharacterType::Kliff) ? 1 : ((targetChar == CharacterType::Damiane) ? 2 : 3);
            auto fnActivate = reinterpret_cast<void* (__fastcall*)(void*, uint32_t)>(m_fnSelectOrActivateCompanion);
            void* pComp = fnActivate(reinterpret_cast<void*>(compContainer), cid);
            if (pComp != nullptr) {
                m_status.activeCharacter = targetChar;
                outMessage = std::string("Successfully switched to ") + CharacterToString(targetChar) + "!";
                Logger::Log("[SwitchManager] %s (Companion: 0x%p)\n", outMessage.c_str(), pComp);
                return true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            outMessage = "Guarded against exception during companion activation.";
            Logger::Log("[SwitchManager] Exception in SelectOrActivateCompanion.\n");
            return false;
        }
    }

    // Fallback when outside live world / before container created
    m_status.activeCharacter = targetChar;
    outMessage = std::string("Early game preference set to ") + CharacterToString(targetChar) + ".";
    Logger::Log("[SwitchManager] %s\n", outMessage.c_str());
    return true;
}

bool SwitchManager::CycleNextCharacter(std::string& outMessage) {
    CharacterType current = GetActiveCharacter();
    CharacterType next = CharacterType::Kliff;

    if (current == CharacterType::Kliff)   next = CharacterType::Damiane;
    else if (current == CharacterType::Damiane) next = CharacterType::Oongka;
    else if (current == CharacterType::Oongka)  next = CharacterType::Kliff;

    return SwitchToCharacter(next, outMessage);
}

} // namespace CrimsonSwitch
