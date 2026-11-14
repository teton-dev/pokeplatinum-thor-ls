# Battles with an NPC ally (Platinum and HGSS)

Every battle where the player has a computer-controlled partner, found in the code on 2026-11-14. Each partner needs a
Gen 5 style back sprite (BW2 draws the partner from behind). Today the bridge draws the partner with BW2's back sprite
of its nearest class (`gen5-battles.md`, "Doubles and partners").

## How the games decide

**Platinum** (`src/encounter.c` `Encounter_NewVsTrainer`, `src/scrcmd_trainer.c`):
- two enemy trainers + a partner ID = `BATTLE_TYPE_TRAINER_WITH_AI_PARTNER`;
- two enemy trainers and no partner = `BATTLE_TYPE_TAG_DOUBLES` (two trainers saw you at once: no ally);
- wild battles beside a partner = `BATTLE_TYPE_AI_PARTNER` (`src/overlay006/wild_encounters.c`).
- *Follower partners* are set with `SetVar VAR_PARTNER_TRAINER_ID, <trainer>` + `SetHasPartner`. Any trainer battle
  started with `StartTrainerBattle` or a wild encounter while the flag is set becomes a double battle beside the partner.
  Scripted ones use `StartTagBattle <partner> <enemy1> <enemy2>`.

**HGSS** (`src/encounter.c` `SetupAndStartTrainerBattle`, `src/scrcmd_battle.c`): the same split with
`BATTLE_TYPE_MULTI | BATTLE_TYPE_AI` for the partner; scripted ones use `MultiBattle <partner> <enemy1> <enemy2>`.

## Platinum

### Scripted tag battles (`StartTagBattle`)

| Where | Partner (trainer data) | Class | Against |
| --- | --- | --- | --- |
| Jubilife City, Galactic grunts | Dawn or Lucas, the opposite of the player's gender, by starter (`TRAINER_DAWN/LUCAS_JUBILIFE_CITY_*`) | `TRAINER_CLASS_DP_PLAYER_FEMALE` / `_MALE` | two Galactic grunts |
| Veilstone City, Galactic grunts | Dawn or Lucas, by starter (`TRAINER_DAWN/LUCAS_VEILSTONE_CITY_*`) | same | two Galactic grunts |
| Fight Area | Barry, by starter (`TRAINER_RIVAL_FIGHT_AREA_*`) | `TRAINER_CLASS_RIVAL` | Volkner and Flint |
| Spear Pillar | Barry, by starter (`TRAINER_RIVAL_SPEAR_PILLAR_*`) | `TRAINER_CLASS_RIVAL` | Mars and Jupiter |

### Follower partners (`VAR_PARTNER_TRAINER_ID`)

Their trainer battles and wild encounters (double, two wild Pokémon) while they follow you.

| Where | Partner | Class |
| --- | --- | --- |
| Eterna Forest | Cheryl (`TRAINER_CHERYL_ETERNA_FOREST`) | `TRAINER_CLASS_TRAINER_CHERYL` |
| Wayward Cave | Mira (`TRAINER_MIRA_WAYWARD_CAVE`) | `TRAINER_CLASS_TRAINER_MIRA` |
| Iron Island B2F | Riley (`TRAINER_RILEY_IRON_ISLAND`) | `TRAINER_CLASS_TRAINER_RILEY` |
| Stark Mountain | Buck (`TRAINER_BUCK_STARK_MOUNTAIN`) | `TRAINER_CLASS_TRAINER_BUCK` |
| Victory Road | Marley (`TRAINER_MARLEY_VICTORY_ROAD`) | `TRAINER_CLASS_TRAINER_MARLEY` |

Barry on Routes 201/202 and Amity Square's partner set `SetHasPartner` without a trainer ID, so they never battle.

### Not Gen 5 candidates

Battle Frontier multi battles (`BATTLE_TYPE_FRONTIER_WITH_AI_PARTNER`; the partner is a random Frontier trainer or a link
player). The bridge doesn't take Frontier battles.

### Platinum's sprite list (8 art pieces, 6 new)

1. Cheryl  2. Mira  3. Riley  4. Buck  5. Marley  6. Barry (Platinum's `TRAINER_CLASS_RIVAL`; one art for the three starters)
7. Dawn  8. Lucas: **already done** as the player's back sprites (`gen5/art/*_back`), reuse them as partners.

## HeartGold / SoulSilver

Both games' scripts are the same here. Only two scripted partner battles exist:

| Where | Partner | Class | Against |
| --- | --- | --- | --- |
| Rocket HQ (`scr_seq_0090_D35R0103`; the map name is unchecked) | Lance (`TRAINER_PKMN_TRAINER_LANCE_LANCE`, 675) | `TRAINERCLASS_PKMN_TRAINER_LANCE` | Executive Ariana + a Rocket grunt |
| Lance and Clair's post-game battle (`scr_seq_0111_D44R0102`; the map name is unchecked) | Silver (`TRAINER_PARTNER_RIVAL_1/2/3`, 735 to 737, by the starter) | `TRAINERCLASS_RIVAL` | Champion Lance (3) and Clair (3) |

The `TrainerBattle` + `Save_VarsFlags_CheckHaveFollower` path (escort mode) exists, but no script in HGSS sets
`SetEscortMode` with a follower trainer number (only `CheckEscortMode` is used, in `scr_seq_0146`), so no other HGSS
battle has an ally. The Frontier multi battles are out of scope as above.

### HGSS sprite list (2 pieces)

1. Lance (his own class, not the Champion's: `TRAINERCLASS_CHAMPION` is only his opponent role)  2. Silver.

## Total

Platinum: Cheryl, Mira, Riley, Buck, Marley, Barry (+ Dawn and Lucas done). HGSS: Lance, Silver. **8 new back sprites.**

## Checks still to do

- Confirm on the Thor that Mira's Wayward Cave segment really fights beside her (the code sets her up; I have not
  run it).
- Lance's and Silver's sprites need the HGSS bridge, which doesn't exist yet (`hgss.md`).
