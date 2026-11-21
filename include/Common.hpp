#pragma once

#include <cstdint>
#include <string>

namespace CrimsonSwitch {

enum class CharacterType : uint8_t {
    None    = 0,
    Kliff   = 1,
    Damiane = 2,
    Oongka  = 3
};

inline const char* CharacterToString(CharacterType character) {
    switch (character) {
        case CharacterType::Kliff:   return "Kliff";
        case CharacterType::Damiane: return "Damiane";
        case CharacterType::Oongka:  return "Oongka";
        default:                     return "Unknown";
    }
}

inline CharacterType IdToCharacter(uint32_t id) {
    switch (id) {
        case 1:  return CharacterType::Kliff;
        case 2:  return CharacterType::Damiane;
        case 3:  return CharacterType::Oongka;
        default: return CharacterType::None;
    }
}

inline uint32_t CharacterToId(CharacterType character) {
    return static_cast<uint32_t>(character);
}

} // namespace CrimsonSwitch
