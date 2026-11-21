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
            // Unlock all characters (both 0-based and 1-based masks: bits 0..3 = 0x0F)
            if ((*pMask & 0x0F) != 0x0F) {
                *pMask |= 0x0F;
                m_status.unlockedMask = *pMask;
                Logger::Log("[SwitchManager] Unlocked Greymane bitmask to 0x%X on UserActor 0x%p!\n",
                            *pMask, (void*)userActor);
                updated = true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // 2. Add Kliff (1), Damiane (2) and Oongka (3) to CompanionContainer so in-game radial wheel populates!
    if (!m_wheelPopulated && m_fnFindCompanionById != 0) {
        uintptr_t compContainer = GetCompanionContainer();
        if (compContainer != 0) {
            __try {
                uintptr_t countAddr = compContainer + 0x148;
                if (!IsBadReadPtr(reinterpret_cast<const void*>(countAddr), sizeof(uint32_t))) {
                    uint32_t count = *reinterpret_cast<uint32_t*>(countAddr);
                    Logger::Log("[SwitchManager] In-world CompanionContainer: 0x%p, Initial Count: %u\n", (void*)compContainer, count);
                    if (count < 3) {
                        auto fnFindOrAdd = reinterpret_cast<void* (__fastcall*)(void*, uint32_t, bool)>(m_fnFindCompanionById);
                        Logger::Log("[SwitchManager] In-game companion count (%u) < 3. Registering Kliff, Damiane & Oongka into wheel...\n", count);
                        void* pKliff   = fnFindOrAdd(reinterpret_cast<void*>(compContainer), 1, true);
                        void* pDamiane = fnFindOrAdd(reinterpret_cast<void*>(compContainer), 2, true);
                        void* pOongka  = fnFindOrAdd(reinterpret_cast<void*>(compContainer), 3, true);

                        if (pKliff) {
                            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pKliff) + 0x30) = 1;
                            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pKliff) + 0x34) = 0x0101;
                            if (m_status.kliffCcoia != 0) {
                                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pKliff) + 0x38) = m_status.kliffCcoia;
                            }
                        }
                        if (pDamiane) {
                            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pDamiane) + 0x30) = 1;
                            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pDamiane) + 0x34) = 0x0101;
                            if (m_status.damianeCcoia != 0) {
                                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pDamiane) + 0x38) = m_status.damianeCcoia;
                            }
                        }
                        if (pOongka) {
                            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pOongka) + 0x30) = 1;
                            *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(pOongka) + 0x34) = 0x0101;
                            if (m_status.oongkaCcoia != 0) {
                                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pOongka) + 0x38) = m_status.oongkaCcoia;
                            }
                        }

                        uint32_t newCount = *reinterpret_cast<uint32_t*>(countAddr);
                        Logger::Log("[SwitchManager] Radial wheel updated! Companion count is now %u (Kliff, Damiane, Oongka ready).\n", newCount);
                        if (newCount >= 3) {
                            m_wheelPopulated = true;
                        }
                        updated = true;
                    } else {
                        m_wheelPopulated = true;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("[SwitchManager] Exception guarded in CompanionContainer activation.\n");
            }
        }
    }

    // 3. Continuously discover loaded protagonists in live scene
    DiscoverActors();

    return updated;
}

static bool ObjectHasRttiName(uintptr_t obj, const char* expectedSubstr) {
    if (obj < 0x10000 || obj > 0x7FFFFFFFFFFF) return false;
    __try {
        if (IsBadReadPtr(reinterpret_cast<const void*>(obj), sizeof(uintptr_t))) return false;
        uintptr_t vtable = *reinterpret_cast<uintptr_t*>(obj);
        if (vtable < 0x10000 || vtable > 0x7FFFFFFFFFFF) return false;
        if (IsBadReadPtr(reinterpret_cast<const void*>(vtable - 8), sizeof(uintptr_t))) return false;
        uintptr_t col = *reinterpret_cast<uintptr_t*>(vtable - 8);
        if (col < 0x10000 || col > 0x7FFFFFFFFFFF) return false;
        if (IsBadReadPtr(reinterpret_cast<const void*>(col), 0x20)) return false;

        uint32_t sig = *reinterpret_cast<uint32_t*>(col);
        if (sig != 1) return false;

        uint32_t selfRva = *reinterpret_cast<uint32_t*>(col + 0x14);
        if (selfRva == 0 || selfRva > 0x10000000) return false;
        uintptr_t imageBase = col - selfRva;

        uint32_t tdRva = *reinterpret_cast<uint32_t*>(col + 0x0C);
        if (tdRva == 0 || tdRva > 0x10000000) return false;
        uintptr_t td = imageBase + tdRva;
        if (IsBadReadPtr(reinterpret_cast<const void*>(td + 0x10), 32)) return false;

        const char* name = reinterpret_cast<const char*>(td + 0x10);
        return strstr(name, expectedSubstr) != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static std::string GetAppearancePath(uintptr_t ccoia) {
    if (!ccoia) return "";
    __try {
        uintptr_t ptr1 = *reinterpret_cast<uintptr_t*>(ccoia + 0x68);
        if (ptr1 < 0x10000 || ptr1 > 0x7FFFFFFFFFFF) return "";
        if (IsBadReadPtr(reinterpret_cast<const void*>(ptr1), 64 * sizeof(uintptr_t))) return "";

        uintptr_t ccc = 0;
        for (int i = 0; i < 64; ++i) {
            uintptr_t comp = *reinterpret_cast<uintptr_t*>(ptr1 + i * sizeof(uintptr_t));
            if (comp < 0x10000 || comp > 0x7FFFFFFFFFFF) continue;
            if (ObjectHasRttiName(comp, "ClientCharacterControlActorComponent")) {
                ccc = comp;
                break;
            }
        }
        if (ccc == 0) return "";

        uintptr_t ptr3 = *reinterpret_cast<uintptr_t*>(ccc + 0x40);
        if (ptr3 < 0x10000 || ptr3 > 0x7FFFFFFFFFFF) return "";

        uintptr_t strStruct = ptr3 + 0x38;
        uint64_t strCap = *reinterpret_cast<uint64_t*>(strStruct + 0x18);
        const char* strData = reinterpret_cast<const char*>(strStruct);
        if (strCap >= 16) {
            strData = *reinterpret_cast<const char**>(strStruct);
        }
        if (strData == nullptr || IsBadReadPtr(strData, 16)) return "";
        return std::string(strData);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return "";
    }
}

void SwitchManager::DiscoverActors() {
    if (m_isMock) return;
    uintptr_t actorMgr = GetClientActorManager();
    if (actorMgr == 0) return;
    uintptr_t userActor = GetUserActor();
    if (userActor == 0) return;

    static uint64_t s_lastScan = 0;
    uint64_t now = GetTickCount64();
    if (now - s_lastScan < 1500) return; // Scan at most once every 1.5s
    s_lastScan = now;

    __try {
        uintptr_t subMgr = *reinterpret_cast<uintptr_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_SUB_MGR);
        if (subMgr == 0) return;
        uintptr_t kliffCcoia = *reinterpret_cast<uintptr_t*>(subMgr + BlackSpaceOffsets::SUB_MGR_TO_KLIFF_CCOIA);
        if (kliffCcoia == 0) return;

        m_status.kliffCcoia = kliffCcoia;

        static bool s_loggedKliff = false;
        if (!s_loggedKliff) {
            std::string kliffApp = GetAppearancePath(kliffCcoia);
            Logger::Log("[SwitchManager] Kliff appearance verified: %s\n", kliffApp.c_str());
            s_loggedKliff = true;
        }

        // 1. Sweep UserActor + 0x78 (ChildContainerActorComponent 100-entry list)
        uintptr_t userVec = *reinterpret_cast<uintptr_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_COMP_VEC);
        if (userVec >= 0x10000 && userVec <= 0x7FFFFFFFFFFF) {
            uintptr_t childComp = *reinterpret_cast<uintptr_t*>(userVec + 0x20); // vec[2]
            if (childComp >= 0x10000 && childComp <= 0x7FFFFFFFFFFF) {
                uintptr_t actorList = *reinterpret_cast<uintptr_t*>(childComp + BlackSpaceOffsets::CHILD_CONTAINER_ACTOR_LIST);
                if (actorList >= 0x10000 && actorList <= 0x7FFFFFFFFFFF) {
                    for (int i = 0; i < 30; ++i) {
                        uintptr_t actor = *reinterpret_cast<uintptr_t*>(actorList + i * 16);
                        if (actor >= 0x10000 && actor <= 0x7FFFFFFFFFFF) {
                            std::string app = GetAppearancePath(actor);
                            if (app.find("cd_phw_damian") != std::string::npos && m_status.damianeCcoia != actor) {
                                m_status.damianeCcoia = actor;
                                Logger::Log("[SwitchManager] *** ChildList DAMIANE at 0x%p ***\n", (void*)actor);
                            } else if (app.find("cd_phm_oongka") != std::string::npos && m_status.oongkaCcoia != actor) {
                                m_status.oongkaCcoia = actor;
                                Logger::Log("[SwitchManager] *** ChildList OONGKA at 0x%p ***\n", (void*)actor);
                            }
                        }
                    }
                }
            }
        }

        // 2. Sweep ClientActorManager for dense actor vector [array_ptr, count, cap]
        for (std::ptrdiff_t off = 0x80; off <= 0x300; off += 8) {
            uintptr_t candidateArray = *reinterpret_cast<uintptr_t*>(actorMgr + off);
            if (candidateArray < 0x10000 || candidateArray > 0x7FFFFFFFFFFF) continue;
            if (IsBadReadPtr(reinterpret_cast<const void*>(candidateArray), 16)) continue;

            uintptr_t firstActor = *reinterpret_cast<uintptr_t*>(candidateArray);
            if (firstActor == kliffCcoia) {
                uint32_t liveCount = *reinterpret_cast<uint32_t*>(actorMgr + off + 8);
                uint32_t capacity  = *reinterpret_cast<uint32_t*>(actorMgr + off + 12);

                if (liveCount > 0 && liveCount <= 4096) {
                    for (uint32_t i = 1; i < liveCount; ++i) {
                        uintptr_t actor = *reinterpret_cast<uintptr_t*>(candidateArray + i * 8);
                        if (actor < 0x10000 || actor > 0x7FFFFFFFFFFF) continue;
                        std::string app = GetAppearancePath(actor);
                        if (app.find("cd_phw_damian") != std::string::npos && m_status.damianeCcoia != actor) {
                            m_status.damianeCcoia = actor;
                            Logger::Log("[SwitchManager] *** DAMIANE CCOIA DISCOVERED AT 0x%p ***\n", (void*)actor);
                        } else if (app.find("cd_phm_oongka") != std::string::npos && m_status.oongkaCcoia != actor) {
                            m_status.oongkaCcoia = actor;
                            Logger::Log("[SwitchManager] *** OONGKA CCOIA DISCOVERED AT 0x%p ***\n", (void*)actor);
                        }
                    }
                }
                break;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Logger::Log("[SwitchManager] Exception in DiscoverActors.\n");
    }
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

        typedef void (*tSetWatermarkHero)(const wchar_t*);
        auto fnSetHero = reinterpret_cast<tSetWatermarkHero>(GetProcAddress(hCoop, "CrimsonCoop_SetWatermarkHero"));
        if (fnSetHero) {
            std::wstring wname = (targetChar == CharacterType::Damiane) ? L"Damiane" :
                                 ((targetChar == CharacterType::Oongka) ? L"Oongka" : L"Kliff");
            fnSetHero(wname.c_str());
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
    uint32_t cid = (targetChar == CharacterType::Kliff) ? 1 : ((targetChar == CharacterType::Damiane) ? 2 : 3);

    // Refresh actors immediately
    DiscoverActors();

    uintptr_t userActor = GetUserActor();
    uintptr_t subMgr = 0;
    if (userActor != 0) {
        subMgr = *reinterpret_cast<uintptr_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_SUB_MGR);
    }

    uintptr_t targetCcoia = 0;
    if (targetChar == CharacterType::Kliff) {
        targetCcoia = m_status.kliffCcoia;
    } else if (targetChar == CharacterType::Damiane) {
        targetCcoia = m_status.damianeCcoia;
    } else if (targetChar == CharacterType::Oongka) {
        targetCcoia = m_status.oongkaCcoia;
    }

    // 1. Activate companion in CompanionContainer
    if (compContainer != 0 && m_fnSelectOrActivateCompanion != 0) {
        __try {
            auto fnActivate = reinterpret_cast<void* (__fastcall*)(void*, uint32_t)>(m_fnSelectOrActivateCompanion);
            void* pComp = fnActivate(reinterpret_cast<void*>(compContainer), cid);
            Logger::Log("[SwitchManager] SelectOrActivateCompanion(%u) -> 0x%p\n", cid, pComp);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // 2. Rotate SubManager + 0x38 and UserActor + 0xD8 to the chosen hero!
    if (targetCcoia != 0 && subMgr != 0 && userActor != 0) {
        __try {
            *reinterpret_cast<uintptr_t*>(subMgr + BlackSpaceOffsets::SUB_MGR_TO_CONTROLLED_CCOIA) = targetCcoia;
            *reinterpret_cast<uintptr_t*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_CONTROLLED)   = targetCcoia;
            m_status.controlledCcoia = targetCcoia;
            Logger::Log("[SwitchManager] Rotated controlled actor to 0x%p (%s)!\n",
                        (void*)targetCcoia, CharacterToString(targetChar));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("[SwitchManager] Exception rotating controlled CCOIA.\n");
        }
    } else if (targetCcoia == 0 && m_status.kliffCcoia != 0) {
        // Fallback: Morph appearance XML descriptor on active player CCOIA
        __try {
            uintptr_t ptr1 = *reinterpret_cast<uintptr_t*>(m_status.kliffCcoia + 0x68);
            if (ptr1 >= 0x10000 && ptr1 <= 0x7FFFFFFFFFFF) {
                uintptr_t ccc = 0;
                for (int i = 0; i < 64; ++i) {
                    uintptr_t comp = *reinterpret_cast<uintptr_t*>(ptr1 + i * sizeof(uintptr_t));
                    if (ObjectHasRttiName(comp, "ClientCharacterControlActorComponent")) {
                        ccc = comp;
                        break;
                    }
                }
                if (ccc != 0) {
                    uintptr_t ptr3 = *reinterpret_cast<uintptr_t*>(ccc + 0x40);
                    if (ptr3 >= 0x10000 && ptr3 <= 0x7FFFFFFFFFFF) {
                        uintptr_t strStruct = ptr3 + 0x38;
                        uint64_t cap = *reinterpret_cast<uint64_t*>(strStruct + 0x18);
                        char* buf = (cap >= 16) ? *reinterpret_cast<char**>(strStruct) : reinterpret_cast<char*>(strStruct);
                        if (buf && !IsBadWritePtr(buf, 80)) {
                            std::string_view newApp = (targetChar == CharacterType::Damiane) ? AppearancePaths::DAMIANE_APP :
                                                      ((targetChar == CharacterType::Oongka) ? AppearancePaths::OONGKA_APP :
                                                                                               AppearancePaths::KLIFF_APP);
                            memcpy(buf, newApp.data(), newApp.size());
                            buf[newApp.size()] = '\0';
                            *reinterpret_cast<uint64_t*>(strStruct + 0x10) = newApp.size();
                            Logger::Log("[SwitchManager] Morphed player appearance descriptor to: %s\n", buf);
                        }
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("[SwitchManager] Exception in appearance morph fallback.\n");
        }
    }

    m_status.activeCharacter = targetChar;
    outMessage = std::string("Successfully switched to ") + CharacterToString(targetChar) + "!";
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
