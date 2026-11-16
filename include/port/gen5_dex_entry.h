#ifndef POKEPLATINUM_PORT_GEN5_DEX_ENTRY_H
#define POKEPLATINUM_PORT_GEN5_DEX_ENTRY_H

#include "pokedex.h"
#include "pokemon.h"

// Platinum's Pokédex entry for a newly caught species, after a battle in BW2's engine (as Platinum's battle shows it
// after a catch): Start sets up its display and opens it, Run runs a frame (TRUE once closed), Free tears it down
BOOL Gen5DexEntry_Start(Pokemon *mon, const Pokedex *pokedex, enum HeapID heapID);
BOOL Gen5DexEntry_Run(void);
void Gen5DexEntry_Free(void);

#endif
