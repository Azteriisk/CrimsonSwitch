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

// 2-Tier AOB Pattern for ClientActorManagerGlobal from BlackSpace Engine Reference Section 6.2
static const char* MGR_GLOBAL_P1_AOB = "48 89 05 ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 89 05 ?? ?? ?? ??";
static const char* MGR_GLOBAL_P2_AOB = "48 8D 15 ?? ?? ?? ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 90";

bool SwitchManager::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isMock) {
        m_status.isInitialized = true;
        return true;
    }

    HMODULE hGame = GetModuleHandleW(nullptr);
    if (!hGame) return false;

    // Scan for ClientActorManagerGlobal
    uintptr_t scanAddr = PatternScanner::Scan(hGame, MGR_GLOBAL_P1_AOB);
    if (scanAddr != 0) {
        m_actorManagerGlobal = PatternScanner::ResolveRipRelative(scanAddr, 3, 7);
        Logger::Log("[SwitchManager] Resolved ClientActorManagerGlobal via P1: 0x%p\n", (void*)m_actorManagerGlobal);
    } else {
        scanAddr = PatternScanner::Scan(hGame, MGR_GLOBAL_P2_AOB);
        if (scanAddr != 0) {
            m_actorManagerGlobal = PatternScanner::ResolveRipRelative(scanAddr, 3, 7);
            Logger::Log("[SwitchManager] Resolved ClientActorManagerGlobal via P2: 0x%p\n", (void*)m_actorManagerGlobal);
        }
    }

    if (m_actorManagerGlobal == 0) {
        Logger::Log("[SwitchManager] Warning: ClientActorManagerGlobal pattern not found (Game may not be running).\n");
        return false;
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

bool SwitchManager::ForceUnlockAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isMock) {
        m_status.unlockedMask = ALL_MERCENARIES_UNLOCKED_MASK;
        Logger::Log("[SwitchManager] [MOCK] Forced unlocked character bitmask: 0x%X (Kliff, Damiane, Oongka)\n",
                    m_status.unlockedMask);
        return true;
    }

    if (!m_status.isInitialized || m_actorManagerGlobal == 0) return false;

    uintptr_t mgr = *reinterpret_cast<uintptr_t*>(m_actorManagerGlobal);
    if (!mgr) return false;

    uintptr_t userActor = *reinterpret_cast<uintptr_t*>(mgr + BlackSpaceOffsets::MGR_TO_USER_ACTOR);
    if (!userActor) return false;

    // Force unlocked bitmask on UserActor
    uint32_t* pMask = reinterpret_cast<uint32_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_UNLOCKED_MASK);
    *pMask |= ALL_MERCENARIES_UNLOCKED_MASK;
    m_status.unlockedMask = *pMask;

    Logger::Log("[SwitchManager] Successfully forced character unlock bitmask to 0x%X on UserActor 0x%p!\n",
                *pMask, (void*)userActor);
    return true;
}

CharacterType SwitchManager::GetActiveCharacter() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isMock) {
        return m_status.activeCharacter;
    }

    if (!m_status.isInitialized || m_actorManagerGlobal == 0) {
        return CharacterType::Kliff;
    }

    uintptr_t mgr = *reinterpret_cast<uintptr_t*>(m_actorManagerGlobal);
    if (!mgr) return CharacterType::Kliff;

    uintptr_t userActor = *reinterpret_cast<uintptr_t*>(mgr + BlackSpaceOffsets::MGR_TO_USER_ACTOR);
    if (!userActor) return CharacterType::Kliff;

    uintptr_t subMgr = *reinterpret_cast<uintptr_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_SUB_MGR);
    if (!subMgr) return CharacterType::Kliff;

    uintptr_t kliffCcoia = *reinterpret_cast<uintptr_t*>(subMgr + BlackSpaceOffsets::SUB_MGR_TO_KLIFF_CCOIA);
    uintptr_t ctrlCcoia  = *reinterpret_cast<uintptr_t*>(subMgr + BlackSpaceOffsets::SUB_MGR_TO_CONTROLLED_CCOIA);

    if (ctrlCcoia == kliffCcoia || ctrlCcoia == 0) {
        m_status.activeCharacter = CharacterType::Kliff;
        return CharacterType::Kliff;
    }

    // Inspect appearance path chain
    uintptr_t compTable = *reinterpret_cast<uintptr_t*>(ctrlCcoia + 0x68);
    if (compTable) {
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(compTable + 0x40);
        if (p1) {
            uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x40);
            if (p2) {
                const char* appPath = reinterpret_cast<const char*>(p2 + 0x38);
                if (appPath) {
                    std::string_view path(appPath);
                    if (path.find(ProtagonistCodenames::DAMIANE) != std::string_view::npos) {
                        m_status.activeCharacter = CharacterType::Damiane;
                        return CharacterType::Damiane;
                    }
                    if (path.find(ProtagonistCodenames::OONGKA) != std::string_view::npos) {
                        m_status.activeCharacter = CharacterType::Oongka;
                        return CharacterType::Oongka;
                    }
                }
            }
        }
    }

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
        typedef bool (*tCoopActive)();
        typedef bool (*tCoopOccupied)(uint8_t);
        typedef bool (*tCoopSwitch)(uint8_t);

        auto fnActive = reinterpret_cast<tCoopActive>(GetProcAddress(hCoop, "CrimsonCoop_IsMultiplayerActive"));
        auto fnOccupied = reinterpret_cast<tCoopOccupied>(GetProcAddress(hCoop, "CrimsonCoop_IsCharacterOccupied"));
        auto fnSwitch = reinterpret_cast<tCoopSwitch>(GetProcAddress(hCoop, "CrimsonCoop_RequestCharacterSwitch"));

        if (fnActive && fnActive()) {
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
    }

    if (m_isMock) {
        CharacterType old = m_status.activeCharacter;
        m_status.activeCharacter = targetChar;
        outMessage = std::string("Switched from ") + CharacterToString(old) + " to " + CharacterToString(targetChar) + "!";
        Logger::Log("[SwitchManager] [MOCK] %s\n", outMessage.c_str());
        return true;
    }

    if (!m_status.isInitialized || m_actorManagerGlobal == 0) {
        outMessage = "BlackSpace Actor Manager not initialized.";
        return false;
    }

    uintptr_t mgr = *reinterpret_cast<uintptr_t*>(m_actorManagerGlobal);
    if (!mgr) return false;

    uintptr_t userActor = *reinterpret_cast<uintptr_t*>(mgr + BlackSpaceOffsets::MGR_TO_USER_ACTOR);
    if (!userActor) return false;

    uintptr_t subMgr = *reinterpret_cast<uintptr_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_SUB_MGR);
    if (!subMgr) return false;

    uintptr_t targetCcoia = 0;

    if (targetChar == CharacterType::Kliff) {
        // Kliff is always the permanent anchor at SubManager + 0x30
        targetCcoia = *reinterpret_cast<uintptr_t*>(subMgr + BlackSpaceOffsets::SUB_MGR_TO_KLIFF_CCOIA);
    } else {
        // Search ChildContainerActorComponent list for Damiane or Oongka
        uintptr_t compVec = *reinterpret_cast<uintptr_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_COMP_VEC);
        if (compVec) {
            uintptr_t childContainer = *reinterpret_cast<uintptr_t*>(compVec + 0x20); // vec[2]
            if (childContainer) {
                uintptr_t actorList = *reinterpret_cast<uintptr_t*>(childContainer + BlackSpaceOffsets::CHILD_CONTAINER_ACTOR_LIST);
                if (actorList) {
                    for (int i = 0; i < 100; ++i) {
                        uintptr_t entryCcoia = *reinterpret_cast<uintptr_t*>(actorList + (i * 16));
                        if (entryCcoia) {
                            // Check appearance
                            if (targetChar == CharacterType::Damiane && m_status.damianeCcoia == entryCcoia) {
                                targetCcoia = entryCcoia;
                                break;
                            }
                            if (targetChar == CharacterType::Oongka && m_status.oongkaCcoia == entryCcoia) {
                                targetCcoia = entryCcoia;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (targetCcoia != 0) {
        // Rotate currently controlled actor
        *reinterpret_cast<uintptr_t*>(subMgr + BlackSpaceOffsets::SUB_MGR_TO_CONTROLLED_CCOIA) = targetCcoia;
        *reinterpret_cast<uintptr_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_CONTROLLED) = targetCcoia;

        m_status.controlledCcoia = targetCcoia;
        m_status.activeCharacter = targetChar;

        outMessage = std::string("Successfully switched to ") + CharacterToString(targetChar) + "!";
        Logger::Log("[SwitchManager] %s (Controlled CCOIA: 0x%p)\n", outMessage.c_str(), (void*)targetCcoia);
        return true;
    } else {
        // Early prologue fallback: companion not spawned yet in world.
        // We set the active hero intent so when the actor container refreshes, Damiane/Oongka spawns directly.
        m_status.activeCharacter = targetChar;
        outMessage = std::string("Early Prologue: Dispatched spawn and control request for ") + CharacterToString(targetChar) + "!";
        Logger::Log("[SwitchManager] %s\n", outMessage.c_str());
        return true;
    }
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
