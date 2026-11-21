#pragma once

#include "Common.hpp"
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace CrimsonSwitch {

/**
 * @brief BlackSpace Engine Reverse-Engineered Memory Offsets for Character Switching
 * 
 * Target Executable: CrimsonDesert.exe (x86-64)
 * Static Chain:
 *   [ClientActorManagerGlobal] -> pa::ClientActorManager
 *       -> +0x58: pa::ClientUserActor
 *           -> +0x08: CCOIA Sub-Manager
 *               -> +0x30: Kliff CCOIA (Persistent world anchor)
 *               -> +0x38: Currently Controlled CCOIA (Rotates on switch)
 *           -> +0xD8: Shortcut to Controlled CCOIA
 *           -> +0x148: Unlocked Characters Bitmask (Bit 1: Kliff, Bit 2: Damiane, Bit 3: Oongka)
 */
namespace BlackSpaceOffsets {
    inline constexpr std::ptrdiff_t MGR_TO_USER_ACTOR            = 0x58;
    inline constexpr std::ptrdiff_t USER_ACTOR_TO_SUB_MGR        = 0x08;
    inline constexpr std::ptrdiff_t SUB_MGR_TO_KLIFF_CCOIA       = 0x30;
    inline constexpr std::ptrdiff_t SUB_MGR_TO_CONTROLLED_CCOIA  = 0x38;
    inline constexpr std::ptrdiff_t USER_ACTOR_TO_CONTROLLED     = 0xD8;
    inline constexpr std::ptrdiff_t USER_ACTOR_UNLOCKED_MASK     = 0x148;
    inline constexpr std::ptrdiff_t USER_ACTOR_TO_COMP_VEC       = 0x78;
    inline constexpr std::ptrdiff_t CHILD_CONTAINER_ACTOR_LIST   = 0x18;
}

namespace ProtagonistCodenames {
    inline constexpr std::string_view KLIFF   = "cd_phm_macduff";
    inline constexpr std::string_view DAMIANE = "cd_phw_damian";
    inline constexpr std::string_view OONGKA  = "cd_phm_oongka";
}

namespace AppearancePaths {
    inline constexpr std::string_view KLIFF_APP = 
        "character/appearance/1_pc/1_phm/cd_phm_macduff/cd_phm_macduff_00000.app_xml";
    inline constexpr std::string_view DAMIANE_APP = 
        "character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml";
    inline constexpr std::string_view OONGKA_APP = 
        "character/appearance/1_pc/1_phm/cd_phm_oongka/cd_phm_oongka_00000.app_xml";
}

// Bitmask representing all three mercenaries unlocked:
// (1 << 1) Kliff | (1 << 2) Damiane | (1 << 3) Oongka = 0x02 | 0x04 | 0x08 = 0x0E (or 0x07 under 0-indexed)
inline constexpr uint32_t ALL_MERCENARIES_UNLOCKED_MASK = 0x0E;

} // namespace CrimsonSwitch
