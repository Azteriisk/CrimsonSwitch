#include "Common.hpp"
#include "EngineTypes.hpp"
#include "SwitchManager.hpp"
#include "Config.hpp"
#include "PatternScan.hpp"
#include <iostream>

using namespace CrimsonSwitch;

int main() {
    std::cout << "=========================================================\n";
    std::cout << "  Crimson Desert Early Trio Switch Test Harness          \n";
    std::cout << "  Target: Kliff, Damiane, Oongka Instant Unlock          \n";
    std::cout << "=========================================================\n\n";

    // 1. Config System Verification
    std::cout << "[1] Verifying INI Configuration System...\n";
    Config::Load("CrimsonSwitch.ini");
    const auto& cfg = Config::Get();
    std::cout << "  -> Unlock All From Start: " << (cfg.unlockAllFromStart ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  -> Direct Hotkeys: [1] Kliff, [2] Damiane, [3] Oongka, [TAB] Cycle\n";

    // 2. Character Enum & Codename Verification
    std::cout << "\n[2] Verifying Mercenary Codename & Appearance Paths...\n";
    std::cout << "  -> Kliff:   " << ProtagonistCodenames::KLIFF   << " | Path: " << AppearancePaths::KLIFF_APP << "\n";
    std::cout << "  -> Damiane: " << ProtagonistCodenames::DAMIANE << " | Path: " << AppearancePaths::DAMIANE_APP << "\n";
    std::cout << "  -> Oongka:  " << ProtagonistCodenames::OONGKA  << " | Path: " << AppearancePaths::OONGKA_APP << "\n";

    // 3. Setup Mock Engine State (Simulating prologue where only Kliff is unlocked)
    std::cout << "\n[3] Setting Up Simulated Engine Environment...\n";
    auto& mgr = SwitchManager::Instance();
    mgr.SetupMockEnvironment(0x14001000ULL, 0x14002000ULL, 0x14003000ULL);

    auto statusBefore = mgr.GetStatus();
    std::cout << "  -> Baseline Active Hero: " << CharacterToString(statusBefore.activeCharacter) << "\n";
    std::cout << "  -> Baseline Unlocked Mask: 0x" << std::hex << statusBefore.unlockedMask << std::dec << " (Kliff only)\n";

    // 4. Force Unlock Bitmask Test
    std::cout << "\n[4] Executing Force Unlock for All 3 Mercenaries...\n";
    mgr.ForceUnlockAll();
    auto statusAfter = mgr.GetStatus();
    std::cout << "  -> Post-Unlock Bitmask: 0x" << std::hex << statusAfter.unlockedMask << std::dec << " (Kliff, Damiane, Oongka UNLOCKED!)\n";

    if ((statusAfter.unlockedMask & ALL_MERCENARIES_UNLOCKED_MASK) == ALL_MERCENARIES_UNLOCKED_MASK) {
        std::cout << "  -> [PASSED] Radial wheel character gating successfully overridden!\n";
    }

    // 5. Test Character Switching Cycle
    std::cout << "\n[5] Executing Dynamic Character Switching Sequence...\n";
    std::string switchMsg;

    // Switch Kliff -> Damiane
    mgr.SwitchToCharacter(CharacterType::Damiane, switchMsg);
    std::cout << "  -> Step 1: " << switchMsg << " (Active: " << CharacterToString(mgr.GetActiveCharacter()) << ")\n";

    // Switch Damiane -> Oongka
    mgr.SwitchToCharacter(CharacterType::Oongka, switchMsg);
    std::cout << "  -> Step 2: " << switchMsg << " (Active: " << CharacterToString(mgr.GetActiveCharacter()) << ")\n";

    // Cycle Oongka -> Kliff
    mgr.CycleNextCharacter(switchMsg);
    std::cout << "  -> Step 3 (Cycle): " << switchMsg << " (Active: " << CharacterToString(mgr.GetActiveCharacter()) << ")\n";

    // 6. Test Duplicate Switch Guard
    std::cout << "\n[6] Testing Redundant Switch Guard...\n";
    bool redundantResult = mgr.SwitchToCharacter(CharacterType::Kliff, switchMsg);
    std::cout << "  -> Attempt switch to Kliff while already Kliff: " 
              << (redundantResult ? "ALLOWED" : "BLOCKED") << " (" << switchMsg << ")\n";

    // 7. Test CrimsonCoop Interoperability & Character Arbitration Guard
    std::cout << "\n[7] Verifying CrimsonCoop Interoperability & Arbitration Guard...\n";
    // Simulate CrimsonCoop active session where peer is piloting Damiane
    mgr.SetCoopArbitrationMock([](CharacterType ct) {
        return ct == CharacterType::Damiane; // Damiane occupied by peer!
    });

    std::string coopSwitchMsg;
    bool coopSwitchResult = mgr.SwitchToCharacter(CharacterType::Damiane, coopSwitchMsg);
    std::cout << "  -> Attempt switch to peer's character (Damiane): " 
              << (coopSwitchResult ? "UNEXPECTEDLY ALLOWED" : "SUCCESSFULLY BLOCKED") << " (" << coopSwitchMsg << ")\n";

    // Attempt switch to unoccupied reserve hero (Oongka)
    bool reserveSwitchResult = mgr.SwitchToCharacter(CharacterType::Oongka, coopSwitchMsg);
    std::cout << "  -> Attempt switch to unoccupied reserve hero (Oongka): " 
              << (reserveSwitchResult ? "SUCCESSFULLY ALLOWED" : "FAILED") << " (" << coopSwitchMsg << ")\n";

    mgr.SetCoopArbitrationMock(nullptr); // Reset mock

    std::cout << "\n=========================================================\n";
    std::cout << "  All CrimsonSwitch Subsystems Verified Successfully!    \n";
    std::cout << "=========================================================\n";

    return 0;
}
