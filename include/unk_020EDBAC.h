#ifndef POKEPLATINUM_UNK_020EDBAC_H
#define POKEPLATINUM_UNK_020EDBAC_H

#include "generated/movement_actions.h"

#include "struct_decls/map_object.h"
#include "struct_defs/struct_020EDF0C.h"

#ifdef SDK_PORT
// The Following Renegade Platinum hack adds the actions 154 to 171
#define MOVEMENT_ACTION_COUNT_PORT (MAX_MOVEMENT_ACTION + 18)
extern int (*const *const gMovementActionFuncs[MOVEMENT_ACTION_COUNT_PORT])(MapObject *);
#else
extern int (*const *const gMovementActionFuncs[MAX_MOVEMENT_ACTION])(MapObject *);
#endif
extern const UnkStruct_020EDF0C *const Unk_020EE3A8[];
extern const int *const gMovementActionCodes[];

#endif // POKEPLATINUM_UNK_020EDBAC_H
