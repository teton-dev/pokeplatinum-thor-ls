// The Safari Zone in BW2's battle engine: Platinum's rules (battle_controller_player.c's Safari commands, the catch
// math of battle_script.c, the flee check of battle_display.c) for a wild battle in which none of the player's Pokémon
// is on the field. BW2 has no Safari Zone, so the view runs the battle itself (btlv_core.c's patch,
// BtlvCore_PortCmdSafari) and calls these for what happens; the port's own text is made here (Unicode, like the
// weather's and the Nuzlocke catch refusal's).

#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gen5.h"
#include "gfl/str.h"

// The two stages' multipliers, as Platinum's tables: numerator over denominator
static const u8 sMultiplier[13][2] = {
    { 10, 40 }, { 10, 35 }, { 10, 30 }, { 10, 25 }, { 10, 20 }, { 10, 15 }, { 10, 10 },
    { 15, 10 }, { 20, 10 }, { 25, 10 }, { 30, 10 }, { 35, 10 }, { 40, 10 },
};

#define STAGE_MAX 12

static struct {
    int active;
    int balls;
    int catchRate;
    int fleeRate;
    int catchStage;  // Platinum's safariCatchStage, 6 at the start
    int escapeStage; // Platinum's safariEscapeCount, 6 at the start
    int fled;
    unsigned short name[GEN5_NAME_LENGTH];
    unsigned short player[8];
} sSafari;

// speciesName is BW2's name of the wild Pokémon's species, for when it has no nickname (Platinum's are in capitals)
void Gen5_PortSafariBegin(const Gen5Battle *battle, const unsigned short *speciesName)
{
    memset(&sSafari, 0, sizeof(sSafari));
    if (!battle->safari) {
        return;
    }
    // POKEPLATINUM_GEN5_SAFARI_SEED=N makes the dice the same every time, for the game's test scripts
    const char *seed = getenv("POKEPLATINUM_GEN5_SAFARI_SEED");
    srand(seed != NULL && *seed != '\0' ? (unsigned)atoi(seed) : (unsigned)time(NULL));
    sSafari.active = TRUE;
    sSafari.balls = battle->safariBalls;
    sSafari.catchRate = battle->safariCatchRate;
    sSafari.fleeRate = battle->safariFleeRate;
    sSafari.catchStage = 6;
    sSafari.escapeStage = 6;
    memcpy(sSafari.name, battle->wild.hasNickname ? battle->wild.nickname : speciesName, sizeof(sSafari.name));
    memcpy(sSafari.player, battle->player.name, sizeof(sSafari.player));
    printf("Gen 5: a Safari Zone battle, %d balls, catch rate %d, flee rate %d\n", sSafari.balls, sSafari.catchRate,
        sSafari.fleeRate);
}

void Gen5_PortSafariEnd(Gen5Battle *battle)
{
    if (sSafari.active) {
        battle->safariBalls = sSafari.balls;
        battle->safariFled = sSafari.fled;
    }
    sSafari.active = FALSE;
}

int Gen5_PortSafari(void)
{
    return sSafari.active;
}

int Gen5_PortSafariBalls(void)
{
    return sSafari.balls;
}

// The wild Pokémon's flight is the battle's end for the result
void Gen5_PortSafariSetFled(void)
{
    sSafari.fled = TRUE;
}

static unsigned Rand(void)
{
    return (unsigned)rand();
}

static unsigned ISqrt(unsigned long value)
{
    unsigned long root = 0;
    unsigned long bit = 1UL << 30;

    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (unsigned)root;
}

// A Safari Ball thrown: takes one, and says how many times it shakes (0 to 3) and whether it catches. Platinum's
// BattleScript_CalcCatchShakes for a Safari Ball (ball modifier 15) on an unhurt Pokémon with no status
void Gen5_PortSafariThrowBall(int *shakes, int *caught)
{
    unsigned speciesMod = sSafari.catchRate * sMultiplier[sSafari.catchStage][0] / sMultiplier[sSafari.catchStage][1];
    unsigned rate = (speciesMod * 15 / 10) / 3; // full HP: (3 * HP - 2 * HP) / (3 * HP)
    int count;

    if (sSafari.balls > 0) {
        sSafari.balls--;
    }
    if (rate >= 255) {
        count = 4;
    } else {
        unsigned root = ISqrt((0xFFUL << 16) / (rate == 0 ? 1 : rate));
        root = ISqrt(root);
        rate = (0xFFFFu << 4) / (root == 0 ? 1 : root);
        // Four checks, as Platinum's (4 shakes is a catch); BW2's throw animation shows 0 to 3 shakes, then the catch
        for (count = 0; count < 4; count++) {
            if (Rand() % 65536 >= rate) {
                break;
            }
        }
    }
    *caught = count == 4;
    *shakes = count == 4 ? 3 : count;
}

// Bait: the Pokémon is more at ease; returns 0 when it is busy eating (1 in 10), else 1. Platinum's
// BattleControllerPlayer_SafariBaitCommand
int Gen5_PortSafariBait(void)
{
    const unsigned roll = Rand() % 10;

    if (sSafari.catchStage < STAGE_MAX) {
        sSafari.catchStage++;
    }
    if (roll != 0 && sSafari.escapeStage < STAGE_MAX) {
        sSafari.escapeStage++;
    }
    return roll != 0;
}

// Mud ("rock"): returns 0 when it is beside itself with anger (1 in 10), else 1. Platinum's
// BattleControllerPlayer_SafariRockCommand
int Gen5_PortSafariMud(void)
{
    const unsigned roll = Rand() % 10;

    if (sSafari.escapeStage > 0) {
        sSafari.escapeStage--;
    }
    if (roll != 0 && sSafari.catchStage > 0) {
        sSafari.catchStage--;
    }
    return roll != 0;
}

// After the player's turn: whether the Pokémon runs away (Task_SafariPokemonSetCommandSelection)
int Gen5_PortSafariFlees(void)
{
    const unsigned rate = sSafari.fleeRate * sMultiplier[sSafari.escapeStage][0] / sMultiplier[sSafari.escapeStage][1];

    return Rand() % 255 <= rate;
}

// The player's turn texts. Names are the wild Pokémon's and the player's, Unicode with 0xFFFF after the last character
static int Append(unsigned short *out, int length, const unsigned short *name)
{
    for (int i = 0; name[i] != 0xFFFF && i < 11; i++) {
        out[length++] = name[i];
    }
    return length;
}

static int AppendAscii(unsigned short *out, int length, const char *text)
{
    for (; *text != '\0'; text++) {
        out[length++] = (unsigned char)*text;
    }
    return length;
}

// The text for id (the GEN5_SAFARI_TEXT_ values below), in strbuf
void Gen5_PortSafariText(StrBuf *strbuf, int id)
{
    unsigned short chars[96];
    int length = 0;
    int mon = FALSE;
    int player = FALSE;
    const char *before = "";
    const char *middle = "";
    const char *after = "";

    switch (id) {
    case 0: // What will the player do?
        before = "What will ";
        after = " do?";
        player = TRUE;
        break;
    case 1:
        before = "";
        middle = " used one Safari Ball!";
        player = TRUE;
        break;
    case 2:
        middle = " threw some Bait at the ";
        after = "!";
        player = TRUE;
        mon = TRUE;
        break;
    case 3:
        middle = " threw mud at the ";
        after = "!";
        player = TRUE;
        mon = TRUE;
        break;
    case 4:
        after = " is eating!";
        mon = TRUE;
        break;
    case 5:
        after = " is busy eating!";
        mon = TRUE;
        break;
    case 6:
        after = " is angry!";
        mon = TRUE;
        break;
    case 7:
        after = " is beside itself with anger!";
        mon = TRUE;
        break;
    case 8:
        after = " is watching carefully!";
        mon = TRUE;
        break;
    case 9:
        before = "Wild ";
        after = " fled!";
        mon = TRUE;
        break;
    case 10:
        before = "Got away safely!";
        break;
    case 11:
        before = "You're out of Safari Balls! Game over!";
        break;
    default:
        break;
    }
    // The order: the player's name, then the middle text, then the Pokémon's name, then the end. A line with only the
    // Pokémon (eating, angry) has it first
    length = AppendAscii(chars, length, before);
    if (player && id != 0) {
        length = Append(chars, length, sSafari.player);
        length = AppendAscii(chars, length, middle);
        if (mon) {
            length = Append(chars, length, sSafari.name);
        }
    } else if (player) {
        length = Append(chars, length, sSafari.player);
    } else if (mon) {
        length = Append(chars, length, sSafari.name);
    }
    length = AppendAscii(chars, length, after);
    chars[length] = 0xFFFF;
    GFL_StrBufLoadString(strbuf, chars);
}

// The top screen's bar of the balls left (btlv_scu.c's patch)
void Gen5_PortSafariBarText(StrBuf *strbuf, int balls)
{
    char text[32];
    unsigned short chars[32];
    int length = 0;

    snprintf(text, sizeof(text), "Safari Balls: %d", balls);
    for (const char *c = text; *c != '\0'; c++) {
        chars[length++] = (unsigned char)*c;
    }
    chars[length] = 0xFFFF;
    GFL_StrBufLoadString(strbuf, chars);
}
