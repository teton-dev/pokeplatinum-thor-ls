// Platinum's trainer classes as Black 2 and White 2's, for the sprite (Gen 5 battles, milestone 5). Platinum's
// class name is shown (BtlSetup.trainerNames), so a class BW2 doesn't have takes the nearest one's sprite: a Bug
// Catcher a Youngster's, Team Galactic Team Plasma's, Platinum's Elite Four BW2's. Sinnoh's Gym Leaders and Cynthia
// have their own, from BW2's Pokémon World Tournament. Generated once from both games' class lists; "same" where
// the name is the same.

#include "types.h"

#include "constants/trainer_classes.h"

static const u16 sClasses[] = {
    [0] = TRAINER_CLASS_HERO, // PLAYER_MALE (nearest)
    [1] = TRAINER_CLASS_HEROINE, // PLAYER_FEMALE (nearest)
    [2] = TRAINER_CLASS_YOUNGSTER, // YOUNGSTER
    [3] = TRAINER_CLASS_LASS, // LASS
    [4] = TRAINER_CLASS_BACKPACKER_M, // CAMPER (nearest)
    [5] = TRAINER_CLASS_BACKPACKER_F, // PICNICKER (nearest)
    [6] = TRAINER_CLASS_YOUNGSTER, // BUG_CATCHER (nearest)
    [7] = TRAINER_CLASS_PARASOL_LADY, // AROMA_LADY (nearest)
    [8] = TRAINER_CLASS_TWINS, // TWINS
    [9] = TRAINER_CLASS_HIKER, // HIKER
    [10] = TRAINER_CLASS_BATTLE_GIRL, // BATTLE_GIRL
    [11] = TRAINER_CLASS_FISHERMAN, // FISHERMAN
    [12] = TRAINER_CLASS_CYCLIST_M, // CYCLIST_MALE (nearest)
    [13] = TRAINER_CLASS_CYCLIST_F, // CYCLIST_FEMALE (nearest)
    [14] = TRAINER_CLASS_BLACK_BELT, // BLACK_BELT
    [15] = TRAINER_CLASS_ARTIST, // ARTIST
    [16] = TRAINER_CLASS_PKMN_BREEDER_M, // BREEDER_MALE (nearest)
    [17] = TRAINER_CLASS_PKMN_BREEDER_F, // BREEDER_FEMALE (nearest)
    [18] = TRAINER_CLASS_PKMN_RANGER_F, // COWGIRL (nearest)
    [19] = TRAINER_CLASS_STRIKER, // JOGGER (nearest)
    [20] = TRAINER_CLASS_POKEFAN_M, // POKEFAN_MALE (nearest)
    [21] = TRAINER_CLASS_POKEFAN_F, // POKEFAN_FEMALE (nearest)
    [22] = TRAINER_CLASS_PRESCHOOLER_F, // POKE_KID (nearest)
    [23] = TRAINER_CLASS_BACKERS_M, // YOUNG_COUPLE (nearest)
    [24] = TRAINER_CLASS_ACE_TRAINER_M, // ACE_TRAINER_MALE (nearest)
    [25] = TRAINER_CLASS_ACE_TRAINER_F, // ACE_TRAINER_FEMALE (nearest)
    [26] = TRAINER_CLASS_WAITRESS, // WAITRESS
    [27] = TRAINER_CLASS_VETERAN_M, // VETERAN (nearest)
    [28] = TRAINER_CLASS_NINJA_M, // NINJA_BOY (nearest)
    [29] = TRAINER_CLASS_ROUGHNECK, // DRAGON_TAMER (nearest)
    [30] = TRAINER_CLASS_PILOT, // BIRD_KEEPER (nearest)
    [31] = TRAINER_CLASS_ACE_TRAINER_M, // DOUBLE_TEAM (nearest)
    [32] = TRAINER_CLASS_RICH_BOY, // RICH_BOY
    [33] = TRAINER_CLASS_LADY, // LADY
    [34] = TRAINER_CLASS_GENTLEMAN, // GENTLEMAN
    [35] = TRAINER_CLASS_SOCIALITE, // SOCIALITE
    [36] = TRAINER_CLASS_BEAUTY, // BEAUTY
    [37] = TRAINER_CLASS_POKEFAN_M, // COLLECTOR (nearest)
    [38] = TRAINER_CLASS_POLICEMAN, // POLICEMAN
    [39] = TRAINER_CLASS_PKMN_RANGER_M, // RANGER_MALE (nearest)
    [40] = TRAINER_CLASS_PKMN_RANGER_F, // RANGER_FEMALE (nearest)
    [41] = TRAINER_CLASS_SCIENTIST_M_52, // SCIENTIST (nearest)
    [42] = TRAINER_CLASS_SWIMMER_M, // SWIMMER_MALE (nearest)
    [43] = TRAINER_CLASS_SWIMMER_F, // SWIMMER_FEMALE (nearest)
    [44] = TRAINER_CLASS_PRESCHOOLER_M, // TUBER_MALE (nearest)
    [45] = TRAINER_CLASS_PRESCHOOLER_F, // TUBER_FEMALE (nearest)
    [46] = TRAINER_CLASS_FISHERMAN, // SAILOR (nearest)
    [47] = TRAINER_CLASS_TWINS, // SIS_AND_BRO (nearest)
    [48] = TRAINER_CLASS_HIKER, // RUIN_MANIAC (nearest)
    [49] = TRAINER_CLASS_PSYCHIC_M, // PSYCHIC_MALE (nearest)
    [50] = TRAINER_CLASS_PSYCHIC_F, // PSYCHIC_FEMALE (nearest)
    [51] = TRAINER_CLASS_GENTLEMAN, // PI (nearest)
    [52] = TRAINER_CLASS_GUITARIST, // GUITARIST
    [53] = TRAINER_CLASS_ACE_TRAINER_M, // ACE_TRAINER_SNOW_MALE (nearest)
    [54] = TRAINER_CLASS_ACE_TRAINER_F, // ACE_TRAINER_SNOW_FEMALE (nearest)
    [55] = TRAINER_CLASS_ACE_TRAINER_M, // SKIER_MALE (nearest)
    [56] = TRAINER_CLASS_ACE_TRAINER_F, // SKIER_FEMALE (nearest)
    [57] = TRAINER_CLASS_ROUGHNECK, // ROUGHNECK
    [58] = TRAINER_CLASS_HARLEQUIN_32, // CLOWN (nearest)
    [59] = TRAINER_CLASS_WORKER_26, // WORKER (nearest)
    [60] = TRAINER_CLASS_SCHOOL_KID_M, // SCHOOL_KID_MALE (nearest)
    [61] = TRAINER_CLASS_SCHOOL_KID_F, // SCHOOL_KID_FEMALE (nearest)
    [62] = TRAINER_CLASS_ROARK, // LEADER_ROARK (nearest)
    [63] = TRAINER_CLASS_PKMN_TRAINER_RIVAL, // RIVAL (nearest)
    [64] = TRAINER_CLASS_BYRON, // LEADER_BYRON (nearest)
    [65] = TRAINER_CLASS_ELITE_FOUR_GRIMSLEY, // ELITE_FOUR_AARON (nearest)
    [66] = TRAINER_CLASS_ELITE_FOUR_SHAUNTAL, // ELITE_FOUR_BERTHA (nearest)
    [67] = TRAINER_CLASS_ELITE_FOUR_MARSHAL, // ELITE_FOUR_FLINT (nearest)
    [68] = TRAINER_CLASS_ELITE_FOUR_CAITLIN, // ELITE_FOUR_LUCIAN (nearest)
    [69] = TRAINER_CLASS_PKMN_TRAINER_CYNTHIA, // CHAMPION_CYNTHIA (nearest)
    [70] = TRAINER_CLASS_GENTLEMAN, // BELLE_AND_PA (nearest)
    [71] = TRAINER_CLASS_HIKER, // RANCHER (nearest)
    [72] = TRAINER_CLASS_TEAM_PLASMA_GRUNT_F, // COMMANDER_MARS (nearest)
    [73] = TRAINER_CLASS_TEAM_PLASMA_GRUNT_M, // GALACTIC_GRUNT_MALE (nearest)
    [74] = TRAINER_CLASS_GARDENIA, // LEADER_GARDENIA (nearest)
    [75] = TRAINER_CLASS_WAKE, // LEADER_WAKE (nearest)
    [76] = TRAINER_CLASS_MAYLENE, // LEADER_MAYLENE (nearest)
    [77] = TRAINER_CLASS_FANTINA, // LEADER_FANTINA (nearest)
    [78] = TRAINER_CLASS_CANDICE, // LEADER_CANDICE (nearest)
    [79] = TRAINER_CLASS_VOLKNER, // LEADER_VOLKNER (nearest)
    [80] = TRAINER_CLASS_PARASOL_LADY, // PARASOL_LADY
    [81] = TRAINER_CLASS_WAITER, // WAITER
    [82] = TRAINER_CLASS_GUY_IN_SUIT_223, // INTERVIEWERS (nearest)
    [83] = TRAINER_CLASS_WORKER_26, // CAMERAMAN (nearest)
    [84] = TRAINER_CLASS_GIRL_IN_SUIT_224, // REPORTER (nearest)
    [85] = TRAINER_CLASS_LADY, // IDOL (nearest)
    [86] = TRAINER_CLASS_TEAM_PLASMA_GHETSIS, // GALACTIC_BOSS (nearest)
    [87] = TRAINER_CLASS_TEAM_PLASMA_GRUNT_F, // COMMANDER_JUPITER (nearest)
    [88] = TRAINER_CLASS_TEAM_PLASMA_GRUNT_M, // COMMANDER_SATURN (nearest)
    [89] = TRAINER_CLASS_TEAM_PLASMA_GRUNT_F, // GALACTIC_GRUNT_FEMALE (nearest)
    [90] = TRAINER_CLASS_ACE_TRAINER_F, // TRAINER_CHERYL (nearest)
    [91] = TRAINER_CLASS_ACE_TRAINER_M, // TRAINER_RILEY (nearest)
    [92] = TRAINER_CLASS_ACE_TRAINER_F, // TRAINER_MARLEY (nearest)
    [93] = TRAINER_CLASS_ACE_TRAINER_M, // TRAINER_BUCK (nearest)
    [94] = TRAINER_CLASS_PRESCHOOLER_F, // TRAINER_MIRA (nearest)
    [95] = TRAINER_CLASS_HERO, // DP_PLAYER_MALE (nearest)
    [96] = TRAINER_CLASS_HEROINE, // DP_PLAYER_FEMALE (nearest)
    [97] = TRAINER_CLASS_SUBWAY_BOSS_INGO, // TOWER_TYCOON (nearest)
    [98] = TRAINER_CLASS_MAID, // MAID
    [99] = TRAINER_CLASS_LADY, // HALL_MATRON (nearest)
    [100] = TRAINER_CLASS_SUBWAY_BOSS_EMMET, // FACTORY_HEAD (nearest)
    [101] = TRAINER_CLASS_HARLEQUIN_32, // ARCADE_STAR (nearest)
    [102] = TRAINER_CLASS_WAITER, // CASTLE_VALET (nearest)
    [103] = TRAINER_CLASS_HERO, // DP_PLAYER_MALE_2 (nearest)
    [104] = TRAINER_CLASS_HEROINE, // DP_PLAYER_FEMALE_2 (nearest)
};

// BW2's class for Platinum's, or a Youngster's for one out of range
u32 Gen5_TrainerClass(u32 platinumClass)
{
    if (platinumClass < NELEMS(sClasses) && sClasses[platinumClass] != 0) {
        return sClasses[platinumClass];
    }
    return TRAINER_CLASS_YOUNGSTER;
}

// Platinum's trainer AI flags as BW2's. Each flag is one of the AI's scripts, run in bit order, and BW2's first eleven
// are Platinum's (basic, attack evaluation, expert, setup, risky, extremes, Baton Pass, tag, HP check, weather,
// harassment), reworked for Gen 5: a trainer keeps the AI level Platinum gave it. Platinum's flags for wild
// battles (roamers, the Safari Zone, the catching tutorial, bits 29 to 31) are BW2's 11 to 13. Renegade Platinum and
// HeartGold/SoulSilver, which number their flags as Platinum does, get their own table here if theirs differ.
static const u8 sPlatinumAIFlags[32] = {
    [0] = 0, [1] = 1, [2] = 2, [3] = 3, [4] = 4, [5] = 5, [6] = 6, [7] = 7, [8] = 8, [9] = 9, [10] = 10,
    [11 ... 28] = 0xFF,
    [29] = 11, [30] = 12, [31] = 13,
};

u32 Gen5_TrainerAIFlags(u32 platinumFlags)
{
    u32 flags = 0;
    for (int i = 0; i < 32; i++) {
        if ((platinumFlags & (1u << i)) && sPlatinumAIFlags[i] != 0xFF) {
            flags |= 1u << sPlatinumAIFlags[i];
        }
    }
    return flags;
}
