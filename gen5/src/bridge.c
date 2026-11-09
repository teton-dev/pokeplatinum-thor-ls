// Pokémon between the port's Gen5Mon (gen5/api/gen5.h) and BW2's, for the Gen 5 battles (milestone 4)
//
// The port gives each Pokémon in full; BW2 builds its own from it and works out the stats from its own data. After the
// battle the port reads back what a battle changes and keeps the rest of its own Pokémon.

#include "types.h"

#include "constants/pokemon.h"
#include "gen5.h"
#include "gfl/heap.h"
#include "pml/poke_party.h"

static const u32 sIvParams[GEN5_STAT_COUNT] = {
    PKM_PARAM_IV_HP, PKM_PARAM_IV_ATTACK, PKM_PARAM_IV_DEFENSE,
    PKM_PARAM_IV_SPEED, PKM_PARAM_IV_SP_ATTACK, PKM_PARAM_IV_SP_DEFENSE,
};

static const u32 sEvParams[GEN5_STAT_COUNT] = {
    PKM_PARAM_EV_HP, PKM_PARAM_EV_ATTACK, PKM_PARAM_EV_DEFENSE,
    PKM_PARAM_EV_SPEED, PKM_PARAM_EV_SP_ATTACK, PKM_PARAM_EV_SP_DEFENSE,
};

// A BW2 Pokémon from mon, in a block of heapId
PartyPkm *Gen5_NewPkm(const Gen5Mon *mon, HeapID heapId)
{
    PartyPkm *pkm = GFL_HeapAllocate(heapId, PokeParty_GetPkmRawSize(), TRUE, __FILE__, __LINE__);
    int i;

    PokeParty_CreatePkm(pkm, mon->species, mon->level, mon->otId, PKM_IVS_RANDOM, mon->personality);
    PokeParty_SetParam(pkm, PKM_PARAM_FORM, mon->form);
    PokeParty_SetParam(pkm, PKM_PARAM_EXP, mon->exp);
    PokeParty_SetParam(pkm, PKM_PARAM_ITEM, mon->heldItem);
    PokeParty_SetParam(pkm, PKM_PARAM_ABILITY, mon->ability);
    // BW2 keeps the nature apart from the personality; Platinum's is the personality's
    PokeParty_SetParam(pkm, PKM_PARAM_NATURE, mon->nature);
    PokeParty_SetParam(pkm, PKM_PARAM_HAPPINESS, mon->friendship);
    for (i = 0; i < 4; i++) {
        PokeParty_SetParam(pkm, PKM_PARAM_MOVE1 + i, mon->moves[i]);
        PokeParty_SetParam(pkm, PKM_PARAM_MOVE1_PP_UP + i, mon->ppUps[i]);
        PokeParty_SetParam(pkm, PKM_PARAM_MOVE1_PP + i, mon->pp[i]);
    }
    for (i = 0; i < GEN5_STAT_COUNT; i++) {
        PokeParty_SetParam(pkm, sIvParams[i], mon->ivs[i]);
        PokeParty_SetParam(pkm, sEvParams[i], mon->evs[i]);
    }
    PokeParty_SetParam(pkm, PKM_PARAM_POKEBALL, mon->ball);
    PokeParty_SetParam(pkm, PKM_PARAM_MET_LEVEL, mon->metLevel);
    PokeParty_SetParam(pkm, PKM_PARAM_ORIGIN_GAME, mon->originGame);
    PokeParty_SetParam(pkm, PKM_PARAM_REGION, mon->language);
    PokeParty_SetParam(pkm, PKM_PARAM_OT_GENDER, mon->otGender);
    PokeParty_SetParam(pkm, PKM_PARAM_POKERUS, mon->pokerus);
    PokeParty_SetParam(pkm, PKM_PARAM_FATEFUL_ENCOUNTER, mon->fatefulEncounter);
    PokeParty_SetParam(pkm, PKM_PARAM_OT_NAME_RAW, (uptr)mon->otName);
    if (mon->hasNickname) {
        PokeParty_SetParam(pkm, PKM_PARAM_NICKNAME_RAW, (uptr)mon->nickname);
    }
    if (mon->isEgg) {
        PokeParty_SetParam(pkm, PKM_PARAM_IS_EGG, TRUE);
    }
    // The stats from the level, IVs, EVs and nature, then the HP and status the Pokémon has now
    PokeParty_RecalcStats(pkm);
    u32 hp = mon->hp;
    if (hp > PokeParty_GetParam(pkm, PKM_PARAM_MAX_HP, NULL)) {
        hp = PokeParty_GetParam(pkm, PKM_PARAM_MAX_HP, NULL);
    }
    PokeParty_SetParam(pkm, PKM_PARAM_HP, hp);
    PokeParty_SetParam(pkm, PKM_PARAM_STATUS, mon->status);
    return pkm;
}

// What a battle can change, read back into mon (which keeps the rest)
void Gen5_ReadPkm(PartyPkm *pkm, Gen5Mon *mon)
{
    int i;

    mon->species = PokeParty_GetParam(pkm, PKM_PARAM_SPECIES, NULL);
    mon->form = PokeParty_GetParam(pkm, PKM_PARAM_FORM, NULL);
    mon->exp = PokeParty_GetParam(pkm, PKM_PARAM_EXP, NULL);
    mon->level = PokeParty_GetParam(pkm, PKM_PARAM_LEVEL, NULL);
    mon->heldItem = PokeParty_GetParam(pkm, PKM_PARAM_ITEM, NULL);
    mon->friendship = PokeParty_GetParam(pkm, PKM_PARAM_HAPPINESS, NULL);
    for (i = 0; i < 4; i++) {
        mon->moves[i] = PokeParty_GetParam(pkm, PKM_PARAM_MOVE1 + i, NULL);
        mon->pp[i] = PokeParty_GetParam(pkm, PKM_PARAM_MOVE1_PP + i, NULL);
        mon->ppUps[i] = PokeParty_GetParam(pkm, PKM_PARAM_MOVE1_PP_UP + i, NULL);
    }
    for (i = 0; i < GEN5_STAT_COUNT; i++) {
        mon->evs[i] = PokeParty_GetParam(pkm, sEvParams[i], NULL);
    }
    mon->hp = PokeParty_GetParam(pkm, PKM_PARAM_HP, NULL);
    mon->status = PokeParty_GetParam(pkm, PKM_PARAM_STATUS, NULL);
    mon->ball = PokeParty_GetParam(pkm, PKM_PARAM_POKEBALL, NULL);
    mon->pokerus = PokeParty_GetParam(pkm, PKM_PARAM_POKERUS, NULL);
}
