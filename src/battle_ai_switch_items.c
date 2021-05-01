#include "global.h"
#include "battle.h"
#include "battle_ai_switch_items.h"
#include "battle_controllers.h"
#include "battle_script_commands.h"
#include "battle_util.h"
#include "data2.h"
#include "ewram.h"
#include "event_data.h"
#include "pokemon.h"
#include "random.h"
#include "rom_8077ABC.h"
#include "util.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"


extern u8 gUnknown_02023A14_50;

extern u8 gActiveBattler;
extern u16 gBattleTypeFlags;
extern u8 gAbsentBattlerFlags;
extern s32 gBattleMoveDamage;
extern u8 gMoveResultFlags;
extern u16 gDynamicBasePower;
extern u8 gCritMultiplier;
extern u16 gBattlerPartyIndexes[];
extern struct BattlePokemon gBattleMons[];
extern u32 gStatuses3[MAX_BATTLERS_COUNT];
extern const u8 gTypeEffectiveness[];
extern u16 gBattleWeather;
extern u8 gStatStageRatios[][2];

static bool8 ShouldUseItem(void);

// UTILS
static void PartyMonToBattleMon(u8 index, struct Pokemon *party, struct BattlePokemon *battleMon)
{
    s32 size;

    // get species and level
    battleMon->species = GetMonData(&party[index], MON_DATA_SPECIES);
    battleMon->level = GetMonData(&party[index], MON_DATA_LEVEL);

    // get stats
    battleMon->hp = GetMonData(&party[index], MON_DATA_HP);
    battleMon->attack = GetMonData(&party[index], MON_DATA_ATK);
    battleMon->defense = GetMonData(&party[index], MON_DATA_DEF);
    battleMon->spAttack = GetMonData(&party[index], MON_DATA_SPATK);
    battleMon->spDefense = GetMonData(&party[index], MON_DATA_SPDEF);
    battleMon->speed = GetMonData(&party[index], MON_DATA_SPEED);

    // get item and status
    battleMon->item = ITEM_NONE;
    battleMon->status1 = GetMonData(&party[index], MON_DATA_STATUS);

    // get type
    battleMon->type1 = gBaseStats[battleMon->species].type1;
    battleMon->type2 = gBaseStats[battleMon->species].type2;

    // get ability
    if (GetMonData(&party[index], MON_DATA_ALT_ABILITY))
            battleMon->ability = gBaseStats[battleMon->species].ability2;
        else
            battleMon->ability = gBaseStats[battleMon->species].ability1;

    // get moves
    for (size = 0; size < 4; size++)
    {
        battleMon->moves[size] = GetMonData(&party[index], MON_DATA_MOVE1 + size);
        battleMon->pp[size] = GetMonData(&party[index], MON_DATA_PP1 + size);
    }
}

static bool8 HasSuperEffectiveMoveAgainst(struct BattlePokemon *mon1, struct BattlePokemon *mon2) {

    u16 move;
    u8 moveFlags;
    s32 i;

    for (i = 0; i < 4; i++)
    {
        move = mon1->moves[i];
        if (move == 0)
            continue;

        moveFlags = AI_TypeCalc(move, mon2->species, mon2->ability);
        if (moveFlags & MOVE_RESULT_SUPER_EFFECTIVE)
            return TRUE;
    }

    return FALSE;
}


static bool8 HasAdvantageOver(struct BattlePokemon *mon1, struct BattlePokemon *mon2) {
    //rewrite to

    // easy case, mon1 has super effective attack against mon2 and mon2 has none against mon1
    if (HasSuperEffectiveMoveAgainst(mon1, mon2) && !HasSuperEffectiveMoveAgainst(mon2, mon1))
        return TRUE;
    
    // consider other cases here, e.g. "neutral" but a lot of damage

    // more cases, e.g. both super effective but ones is quicker or whatever

    // think of more

    // no argument found that mon1 has advantage over mon2
    return FALSE;
}


static u16 Can1HitKO(u8 attacker, u8 defender) {
    
    u16 move, result;
    s32 i, battleMoveDmg;

    result = 0;
    for (i = 0; i < 4; i++) {
    
        move = gBattleMons[attacker].moves[i];
        if (move == 0)
            continue;
        
        battleMoveDmg = AI_SimulateDmg(attacker, defender, move);
        if (battleMoveDmg < gBattleMons[defender].hp)
            continue;
        
        // priorities go from -6 to 5,
        result |= (1 << (gBattleMoves[move].priority + 6));
    }

    return result;
}

static bool8 Can1HitKOOnSwitchIn(u8 attacker, u8 defender, u8 currentDefender, struct Pokemon *party) {

    u16 move;
    s32 i, battleMoveDmg;
    struct BattlePokemon defenderStruct;

    PartyMonToBattleMon(defender, party, &defenderStruct);

    // check if the player can ko on switch in
    for (i = 0; i < 4; i++) {

        move = gBattleMons[attacker].moves[i];
        if (move == 0)
            continue;

        battleMoveDmg = AI_SimulateDmgOnSwitchIn(attacker, currentDefender, &defenderStruct, move);
        if (battleMoveDmg < defenderStruct.hp)
            continue;
        
        return TRUE;
    }
    return FALSE;
}

static bool8 IsPlayerSwitchPlusPossible(void) {

    u8 battlerIn1;
    u8 opposingBattler1, opposingBattler2;
    s32 i;
    struct BattlePokemon battleMon;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        // TODO, how to handle doubles??
        return TRUE;
    }
    else
    {
        battlerIn1 = gActiveBattler;
        opposingBattler1 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        opposingBattler2 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    }

    for (i = 0; i < 6; i++)
    {
        if (GetMonData(&gPlayerParty[i], MON_DATA_HP) == 0)
            continue;
        if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES2) == SPECIES_NONE)
            continue;
        if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES2) == SPECIES_EGG)
            continue;
        if (i == gBattlerPartyIndexes[opposingBattler1])
            continue;
        if (i == gBattlerPartyIndexes[opposingBattler2])
            continue;
        
        PartyMonToBattleMon(i, gPlayerParty, &battleMon);

        if (HasAdvantageOver(&battleMon, &gBattleMons[battlerIn1]))
          return TRUE;
    }

    return FALSE;
}

static bool8 IsPlayerSwitchNeutralPossible(void) {

    u8 battlerIn1;
    u8 opposingBattler1, opposingBattler2;
    s32 i;
    struct BattlePokemon battleMon;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        // TODO, how to handle doubles??
        return TRUE;
    }
    else
    {
        battlerIn1 = gActiveBattler;
        opposingBattler1 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        opposingBattler2 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    }

    for (i = 0; i < 6; i++)
    {
        if (GetMonData(&gPlayerParty[i], MON_DATA_HP) == 0)
            continue;
        if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES2) == SPECIES_NONE)
            continue;
        if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES2) == SPECIES_EGG)
            continue;
        if (i == gBattlerPartyIndexes[opposingBattler1])
            continue;
        if (i == gBattlerPartyIndexes[opposingBattler2])
            continue;
        
        PartyMonToBattleMon(i, gPlayerParty, &battleMon);

        if (!HasAdvantageOver(&battleMon, &gBattleMons[battlerIn1])
            && !HasAdvantageOver(&gBattleMons[battlerIn1], &battleMon))
            return TRUE;
    }

    return FALSE;
}


static bool8 isSwitchPlusPossible(void) {
    u8 battlerIn1;
    u8 opposingBattler1, opposingBattler2;
    s32 i;
    struct BattlePokemon battleMon;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        // TODO, how to handle doubles??
        return FALSE;
    }
    else
    {
        battlerIn1 = gActiveBattler;
        opposingBattler1 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        opposingBattler2 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    }

    for (i = 0; i < 6; i++)
    {
        if (GetMonData(&gEnemyParty[i], MON_DATA_HP) == 0)
            continue;
        if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) == SPECIES_NONE)
            continue;
        if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) == SPECIES_EGG)
            continue;
        if (i == gBattlerPartyIndexes[battlerIn1])
            continue;
        
        PartyMonToBattleMon(i, gEnemyParty, &battleMon);

        if (HasAdvantageOver(&battleMon, &gBattleMons[opposingBattler1]))
            return TRUE;
    }

    return FALSE;    
}

static bool8 isSwitchNeutralPossible(void) {
    u8 battlerIn1;
    u8 opposingBattler1, opposingBattler2;
    s32 i;
    struct BattlePokemon battleMon;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        // TODO, how to handle doubles??
        return FALSE;
    }
    else
    {
        battlerIn1 = gActiveBattler;
        opposingBattler1 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        opposingBattler2 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    }

    for (i = 0; i < 6; i++)
    {
        if (GetMonData(&gEnemyParty[i], MON_DATA_HP) == 0)
            continue;
        if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) == SPECIES_NONE)
            continue;
        if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) == SPECIES_EGG)
            continue;
        if (i == gBattlerPartyIndexes[battlerIn1])
            continue;
        
        PartyMonToBattleMon(i, gEnemyParty, &battleMon);

        if (!HasAdvantageOver(&battleMon, &gBattleMons[opposingBattler1])
            && !HasAdvantageOver(&gBattleMons[opposingBattler1], &battleMon))
            return TRUE;
    }

    return FALSE;    
}


// STATES

/*
    Table of the player's state-action probabilities.
    This is a placeholder and will be updated by a 
    moving average in the future.

            | stay | swap n | swap + |
            --------------------------
        p + |  255 |   0    |    0   |
        p n |  200 |   25   |    30  |
        p - |   0  |  127   |   128  |
            --------------------------       
*/
const u8 PLAYER_STATE_ACTION_PROBABILITIES[3][3] = {
    { 255, 0, 0 },      // at an advantage -> stays in
    { 200, 25, 30 },    // neutral -> stays in
    { 255, 0, 0},     // at a disadvantage -> swaps out
};


static bool8 BattlerIsAlmostKO(u8 battlerIn, u8 opposingBattler) {

    u16 aiCanKOPlayer, playerCanKOAi, priority;
    s32 i, battlerInSpeed, opposingBattlerSpeed;

    aiCanKOPlayer = Can1HitKO(battlerIn, opposingBattler);
    playerCanKOAi = Can1HitKO(opposingBattler, battlerIn);

    for (i = 15; i >= 0; i--) {

        priority = 1 << i;
        // same priority
        if ((aiCanKOPlayer & priority) && (playerCanKOAi & priority)) {
        
            battlerInSpeed = gBattleMons[battlerIn].speed;
            opposingBattlerSpeed = gBattleMons[opposingBattler].speed;
            // adjust for swift swim / chlorophyll
            if (WEATHER_HAS_EFFECT) {
                if ((gBattleMons[battlerIn].ability == ABILITY_SWIFT_SWIM && (gBattleWeather & WEATHER_RAIN_ANY)))
                    battlerInSpeed *= 2;
                if ((gBattleMons[battlerIn].ability == ABILITY_CHLOROPHYLL && (gBattleWeather & WEATHER_SUN_ANY)))
                    battlerInSpeed *= 2;
                if ((gBattleMons[opposingBattler].ability == ABILITY_SWIFT_SWIM && (gBattleWeather & WEATHER_RAIN_ANY)))
                    opposingBattlerSpeed *= 2;
                if ((gBattleMons[opposingBattler].ability == ABILITY_CHLOROPHYLL && (gBattleWeather & WEATHER_SUN_ANY)))
                    opposingBattlerSpeed *= 2;
            }
            // adjust for boosts
            battlerInSpeed *= gStatStageRatios[gBattleMons[battlerIn].statStages[STAT_STAGE_SPEED]][0];
            battlerInSpeed /= gStatStageRatios[gBattleMons[battlerIn].statStages[STAT_STAGE_SPEED]][1];
            opposingBattlerSpeed *= gStatStageRatios[gBattleMons[opposingBattler].statStages[STAT_STAGE_SPEED]][0];
            opposingBattlerSpeed /= gStatStageRatios[gBattleMons[opposingBattler].statStages[STAT_STAGE_SPEED]][1];
            // adjust for badge boost
            if (FlagGet(FLAG_BADGE03_GET)) {
                opposingBattlerSpeed = (opposingBattlerSpeed * 110) / 100;
            }
            // adjust for paralysis
            if (gBattleMons[opposingBattler].status1 & STATUS1_PARALYSIS) {
                opposingBattlerSpeed /= 4;
            }
            
            return opposingBattlerSpeed > battlerInSpeed;
        }
            
        // player priority
        if (playerCanKOAi & priority)
            return TRUE;
        // ai priority
        if (aiCanKOPlayer & priority)
            return FALSE;

    }

    return FALSE;
}


static bool8 IsAlmostKO(void) {

    u8 battlerIn1, opposingBattler1;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        //u8 battlerIn2, opposingBattler2;
        return FALSE;

    }

    battlerIn1 = gActiveBattler;
    opposingBattler1 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        
    return BattlerIsAlmostKO(battlerIn1, opposingBattler1);
}


static bool8 IsPlayerAdvantageState(void) {

    u8 battlerIn1;
    u8 opposingBattler1;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        // TODO, how to handle doubles??
        return FALSE;
    }
    else
    {
        battlerIn1 = gActiveBattler;
        opposingBattler1 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    }

    return HasAdvantageOver(&gBattleMons[opposingBattler1], &gBattleMons[battlerIn1]);
}


static bool8 IsPlayerDisadvantageState(void) {

    u8 battlerIn1;
    u8 opposingBattler1;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        // TODO, how to handle doubles??
        return FALSE;
    }
    else
    {
        battlerIn1 = gActiveBattler;
        opposingBattler1 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    }

    return HasAdvantageOver(&gBattleMons[battlerIn1], &gBattleMons[opposingBattler1]);
}


// AI SWITCH ITEMS


static bool8 ShouldSwitchIfPerishSong(void)
{
    if (gStatuses3[gActiveBattler] & STATUS3_PERISH_SONG
        && gDisableStructs[gActiveBattler].perishSongTimer1 == 0)
    {
        ewram160C8arr(GetBattlerPosition(gActiveBattler)) = 6; // gBattleStruct->AI_monToSwitchIntoId[GetBattlerPosition(gActiveBattler)] = 6;
        BtlController_EmitTwoReturnValues(1, 2, 0);
        return TRUE;
    }

    return FALSE;
}

static bool8 ShouldSwitchIfWonderGuard(void)
{
    u8 opposingBattler;
    u8 moveFlags;
    s32 i, j;
    u16 move;

    if(gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
        return FALSE;

    if (gBattleMons[GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)].ability == ABILITY_WONDER_GUARD)
    {
        // Check if Pokemon has a super effective move.
        for (opposingBattler = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), i = 0; i < MAX_MON_MOVES; ++i)
        {
            move = gBattleMons[gActiveBattler].moves[i];
            if (move == MOVE_NONE)
                continue;
            moveFlags = AI_TypeCalc(move, gBattleMons[opposingBattler].species, gBattleMons[opposingBattler].ability);
            if (moveFlags & MOVE_RESULT_SUPER_EFFECTIVE)
                return FALSE;
        }
        // Find a Pokemon in the party that has a super effective move.
        for (i = 0; i < PARTY_SIZE; ++i)
        {
            if (GetMonData(&gEnemyParty[i], MON_DATA_HP) == 0)
                continue;
            if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) == SPECIES_NONE)
                continue;
            if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) == SPECIES_EGG)
                continue;
            if (i == gBattlerPartyIndexes[gActiveBattler])
                continue;
            GetMonData(&gEnemyParty[i], MON_DATA_SPECIES); // Unused return value.
            GetMonData(&gEnemyParty[i], MON_DATA_ALT_ABILITY); // Unused return value.
        
            for (opposingBattler = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), j = 0; j < MAX_MON_MOVES; ++j)
            {
                move = GetMonData(&gEnemyParty[i], MON_DATA_MOVE1 + j);
                if (move == MOVE_NONE)
                    continue;
                moveFlags = AI_TypeCalc(move, gBattleMons[opposingBattler].species, gBattleMons[opposingBattler].ability);
                if (moveFlags & MOVE_RESULT_SUPER_EFFECTIVE && Random() % 3 < 2)
                {
                    // We found a mon.
                    ewram160C8arr(GetBattlerPosition(gActiveBattler)) = i; // gBattleStruct->AI_monToSwitchIntoId[GetBattlerPosition(gActiveBattler)] = i;
                    BtlController_EmitTwoReturnValues(1, B_ACTION_SWITCH, 0);
                    return TRUE;
                }
            }
        }
    }
    return FALSE; // There is not a single Pokemon in the party that has a super effective move against a mon with Wonder Guard.
}

static bool8 ShouldSwitchIfAlmostKO(void) {

    if (!isSwitchPlusPossible() && !isSwitchNeutralPossible())
        return FALSE;

    // if almost KO but useful & has a good switch in -> switch
    // if almost KO but not useful -> fallback
    return FALSE;
}


const u8 AI_ACTION_SWITCH_ADVANTAGE = 1;
const u8 AI_ACTION_SWITCH_NEUTRAL = 2;
const u8 AI_ACTION_STAY_IN = 4;

u8 AI_ChooseAction(void) {

    u8 aiSwitchPlusScore;
    u8 aiSwitchNeutralScore;
    u8 aiStayInScore;

    s32 overallScore;
    s32 randomNumber;

    u8 playerAdvantageState = 0;
    u8 playerNeutralState = 1;
    u8 playerDisadvantageState = 2;

    u8 playerStayInAction = 0;
    u8 playerSwitchNeutralAction = 1;
    u8 playerSwitchPlusAction = 2;

    u8 player_state_action_probabilities[3][3];
    s32 i, j;

    // pick random action to remain unpredictable
    if (Random() % 64 == 0) {
        return 1 << (Random() % 3);
    }

    // copy state-action probabilities and adapt to current situation
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            player_state_action_probabilities[i][j] = PLAYER_STATE_ACTION_PROBABILITIES[i][j];
        }
    }

    if (!IsPlayerSwitchNeutralPossible()) {
        // likelihood for plus switch rises
        for (i = 0; i < 3; i++) {
            player_state_action_probabilities[i][2] += player_state_action_probabilities[i][1];
            player_state_action_probabilities[i][1] = 0;
        }
    }

    if (!IsPlayerSwitchPlusPossible()) {
        // likelihood for no switch rises
        for (i = 0; i < 3; i++) {
            player_state_action_probabilities[i][0] += player_state_action_probabilities[i][2];
            player_state_action_probabilities[i][2] = 0;
        }
    }


    // pick best action based on expectation
    // rewards:
    // 1   if expected advantage for ai
    // 0.5 if outcome unclear
    // 0   if expected disadvantage for ai
    if (IsPlayerAdvantageState()) {

        aiSwitchPlusScore = 0;
        aiSwitchPlusScore += player_state_action_probabilities[playerAdvantageState][playerStayInAction] * 1;
        aiSwitchPlusScore += player_state_action_probabilities[playerAdvantageState][playerSwitchNeutralAction] / 2;
        aiSwitchPlusScore += player_state_action_probabilities[playerAdvantageState][playerSwitchPlusAction] / 2;

        aiSwitchNeutralScore = 0;
        aiSwitchNeutralScore += player_state_action_probabilities[playerAdvantageState][playerStayInAction] / 2;
        aiSwitchNeutralScore += player_state_action_probabilities[playerAdvantageState][playerSwitchNeutralAction] / 2;
        aiSwitchNeutralScore += player_state_action_probabilities[playerAdvantageState][playerSwitchPlusAction] / 2;

        aiStayInScore = 0;
        aiStayInScore += player_state_action_probabilities[playerAdvantageState][playerStayInAction] * 0;
        aiStayInScore += player_state_action_probabilities[playerAdvantageState][playerSwitchNeutralAction] * 1; // free move
        aiStayInScore += player_state_action_probabilities[playerAdvantageState][playerSwitchPlusAction] * 0;

    } else if (IsPlayerDisadvantageState()) {

        aiSwitchPlusScore = 0;
        aiSwitchPlusScore += player_state_action_probabilities[playerDisadvantageState][playerStayInAction] * 0; // lost a move
        aiSwitchPlusScore += player_state_action_probabilities[playerDisadvantageState][playerSwitchNeutralAction] / 2;
        aiSwitchPlusScore += player_state_action_probabilities[playerDisadvantageState][playerSwitchPlusAction] / 2;

        aiSwitchNeutralScore = 0;
        aiSwitchNeutralScore += player_state_action_probabilities[playerDisadvantageState][playerStayInAction] * 0; // lost a move
        aiSwitchNeutralScore += player_state_action_probabilities[playerDisadvantageState][playerSwitchNeutralAction] / 2;
        aiSwitchNeutralScore += player_state_action_probabilities[playerDisadvantageState][playerSwitchPlusAction] / 2;

        aiStayInScore = 0;
        aiStayInScore += player_state_action_probabilities[playerDisadvantageState][playerStayInAction] * 1;
        aiStayInScore += player_state_action_probabilities[playerDisadvantageState][playerSwitchNeutralAction] * 1; // free move
        aiStayInScore += player_state_action_probabilities[playerDisadvantageState][playerSwitchPlusAction] / 2; // free move

    } else {

        aiSwitchPlusScore = 0;
        aiSwitchPlusScore += player_state_action_probabilities[playerNeutralState][playerStayInAction] * 1;
        aiSwitchPlusScore += player_state_action_probabilities[playerNeutralState][playerSwitchNeutralAction] / 2;
        aiSwitchPlusScore += player_state_action_probabilities[playerNeutralState][playerSwitchPlusAction] / 2;

        aiSwitchNeutralScore = 0;
        aiSwitchNeutralScore += player_state_action_probabilities[playerNeutralState][playerStayInAction] * 0; // lost a move
        aiSwitchNeutralScore += player_state_action_probabilities[playerNeutralState][playerSwitchNeutralAction] / 2;
        aiSwitchNeutralScore += player_state_action_probabilities[playerNeutralState][playerSwitchPlusAction] / 2;

        aiStayInScore = 0;
        aiStayInScore += player_state_action_probabilities[playerNeutralState][playerStayInAction] * 1;
        aiStayInScore += player_state_action_probabilities[playerNeutralState][playerSwitchNeutralAction] * 1; // free move
        aiStayInScore += player_state_action_probabilities[playerNeutralState][playerSwitchPlusAction] * 0;

    }

    overallScore = aiSwitchPlusScore + aiSwitchNeutralScore + aiStayInScore + 1;
    randomNumber = (Random() % overallScore) + 1; // from 1 to overallScore (inclusive)

    // switch for advantage
    if (randomNumber <= aiSwitchPlusScore)
        return AI_ACTION_SWITCH_ADVANTAGE;

    // switch in a neutral pokemon
    if (randomNumber <= (aiSwitchPlusScore + aiSwitchNeutralScore))
        return AI_ACTION_SWITCH_NEUTRAL;

    // stay in
    return AI_ACTION_STAY_IN;
}

static bool8 ShouldSwitch(void)
{
    u8 battlerIn1, battlerIn2;
    s32 i;
    s32 availableToSwitch;
    u8 aiAction;

    if (gBattleMons[gActiveBattler].status2 & (STATUS2_WRAPPED | STATUS2_ESCAPE_PREVENTION))
        return FALSE;
    if (gStatuses3[gActiveBattler] & STATUS3_ROOTED)
        return FALSE;
    if (AbilityBattleEffects(ABILITYEFFECT_CHECK_OTHER_SIDE, gActiveBattler, ABILITY_SHADOW_TAG, 0, 0))
        return FALSE;
    if (AbilityBattleEffects(ABILITYEFFECT_CHECK_OTHER_SIDE, gActiveBattler, ABILITY_ARENA_TRAP, 0, 0))
        return FALSE; // misses the flying or levitate check
    if (AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, ABILITY_MAGNET_PULL, 0, 0))
    {
        if (gBattleMons[gActiveBattler].type1 == TYPE_STEEL)
            return FALSE;
        if (gBattleMons[gActiveBattler].type2 == TYPE_STEEL)
            return FALSE;
    }

    availableToSwitch = 0;
    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerIn1 = gActiveBattler;
        if (gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(GetBattlerPosition(gActiveBattler) ^ BIT_FLANK)])
            battlerIn2 = gActiveBattler;
        else
            battlerIn2 = GetBattlerAtPosition(GetBattlerPosition(gActiveBattler) ^ BIT_FLANK);
    }
    else
    {
        battlerIn1 = gActiveBattler;
        battlerIn2 = gActiveBattler;
    }

    for (i = 0; i < 6; i++)
    {
        if (GetMonData(&gEnemyParty[i], MON_DATA_HP) == 0)
            continue;
        if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) == SPECIES_NONE)
            continue;
        if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) == SPECIES_EGG)
            continue;
        if (i == gBattlerPartyIndexes[battlerIn1])
            continue;
        if (i == gBattlerPartyIndexes[battlerIn2])
            continue;
        if (i == ewram16068arr(battlerIn1))
            continue;
        if (i == ewram16068arr(battlerIn2))
            continue;

        availableToSwitch++;
    }

    // cannot switch
    if (availableToSwitch == 0)
        return FALSE;
    // must switch because perish song
    if (ShouldSwitchIfPerishSong())
        aiAction = (AI_ACTION_SWITCH_ADVANTAGE | AI_ACTION_SWITCH_NEUTRAL);
    // must switch because wonder guard
    if (ShouldSwitchIfWonderGuard())
        return TRUE;

    // switch if pokemon is almost ko
    if (IsAlmostKO()) {
        if (ShouldSwitchIfAlmostKO())
            aiAction = (AI_ACTION_SWITCH_ADVANTAGE | AI_ACTION_SWITCH_NEUTRAL);
        else 
            return FALSE;
    } else {
        aiAction = AI_ChooseAction();
    }

    // switch to pokemon with advantage
    if (aiAction & AI_ACTION_SWITCH_ADVANTAGE) {

        // find most suitable mon to switch into
        s32 monToSwitchId = GetMostSuitableMonToSwitchInto(FALSE);
        if (monToSwitchId == 6)
            return FALSE;

        // switch
        ewram160C8arr(GetBattlerPosition(gActiveBattler)) = monToSwitchId;
        BtlController_EmitTwoReturnValues(1, B_ACTION_SWITCH, 0);
        return TRUE;
    }

    // switch neutral pokemon
    if (aiAction & (AI_ACTION_SWITCH_NEUTRAL | AI_ACTION_SWITCH_ADVANTAGE)) {

        // find most suitable mon to switch into
        s32 monToSwitchId = GetMostSuitableMonToSwitchInto(TRUE);
        if (monToSwitchId == 6)
            return FALSE;

        // switch
        ewram160C8arr(GetBattlerPosition(gActiveBattler)) = monToSwitchId;
        BtlController_EmitTwoReturnValues(1, B_ACTION_SWITCH, 0);
        return TRUE;
    }

    if (aiAction & AI_ACTION_STAY_IN)
        return FALSE;

    return FALSE;

}

void AI_TrySwitchOrUseItem(void)
{
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
    {
        if (ShouldSwitch())
        {
            ewram16068arr(gActiveBattler) = ewram160C8arr(GetBattlerPosition(gActiveBattler));
            return;
        }
        else
        {
            #if DEBUG
            if (!(gUnknown_02023A14_50 & 0x20) && ShouldUseItem())
                return;
            #else
            if (ShouldUseItem())
                return;
            #endif
        }
    }

    BtlController_EmitTwoReturnValues(1, B_ACTION_USE_MOVE, (gActiveBattler ^ BIT_SIDE) << 8);
}

static void ModulateByTypeEffectiveness(u8 attackType, u8 defenseType1, u8 defenseType2, u8 *var)
{
    s32 i = 0;

    while (TYPE_EFFECT_ATK_TYPE(i) != TYPE_ENDTABLE)
    {
        if (TYPE_EFFECT_ATK_TYPE(i) == TYPE_FORESIGHT)
        {
            i += 3;
            continue;
        }
        else if (TYPE_EFFECT_ATK_TYPE(i) == attackType)
        {
            // check type1
            if (TYPE_EFFECT_DEF_TYPE(i) == defenseType1)
                *var = (*var * TYPE_EFFECT_MULTIPLIER(i)) / 10;
            // check type2
            if (TYPE_EFFECT_DEF_TYPE(i) == defenseType2 && defenseType1 != defenseType2)
                *var = (*var * TYPE_EFFECT_MULTIPLIER(i)) / 10;
        }
        i += 3;
    }
}

u8 GetMostSuitableMonToSwitchInto(bool8 makeNeutralSwitch)
{
    u8 opposingBattler;
    u8 bestDmg; // note : should be changed to s32
    u8 bestMonId;
    u8 battlerIn1, battlerIn2;
    s32 i, j;
    u8 invalidMons;
    u16 move;
    s32 permutation[6];
    s32 temp;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerIn1 = gActiveBattler;
        if (gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(GetBattlerPosition(gActiveBattler) ^ BIT_FLANK)])
            battlerIn2 = gActiveBattler;
        else
            battlerIn2 = GetBattlerAtPosition(GetBattlerPosition(gActiveBattler) ^ BIT_FLANK);

        // UB: It considers the opponent only player's side even though it can battle alongside player;
        opposingBattler = Random() & BIT_FLANK;
        if (gAbsentBattlerFlags & gBitTable[opposingBattler])
            opposingBattler ^= BIT_FLANK;
    }
    else
    {
        opposingBattler = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        battlerIn1 = gActiveBattler;
        battlerIn2 = gActiveBattler;
    }

    // temporary solution, find random mon
    if (makeNeutralSwitch) {
        // create array of random permutations
        for (i = 0; i < 6; i++) {
            permutation[i] = i;
        }
        for (i = 0; i < 6; i++) {
            j = Random() % 6;
            temp = permutation[i];
            permutation[i] = permutation[j];
            permutation[j] = temp;
        }
        for (i = 0; i < 6; i++)
        {
            if ((u16)(GetMonData(&gEnemyParty[permutation[i]], MON_DATA_SPECIES)) == SPECIES_NONE)
                continue;
            if (GetMonData(&gEnemyParty[permutation[i]], MON_DATA_HP) == 0)
                continue;
            if (gBattlerPartyIndexes[battlerIn1] == permutation[i])
                continue;
            if (gBattlerPartyIndexes[battlerIn2] == permutation[i])
                continue;
            if (permutation[i] == ewram16068arr(battlerIn1))
                continue;
            if (permutation[i] == ewram16068arr(battlerIn2))
                continue;
            if (Can1HitKOOnSwitchIn(opposingBattler, permutation[i], battlerIn1, gEnemyParty))
                continue;
            // current implementation: random switch
            // TODO: Make it a smart switch
            return permutation[i];
        }
        // no neutral switch possible
        return 6;
    } else {}

    // TODO: remove everything below and move it to HasAdvantageOver...
    // maybe rewrite HasAdvantageOver such that it returns flags -> higher flags are more important
    invalidMons = 0;

    while (invalidMons != 0x3F) // all mons are invalid
    {
        bestDmg = 0;
        bestMonId = 6;
        // find the mon which type is the most suitable offensively
        for (i = 0; i < 6; i++)
        {
            u16 species = GetMonData(&gEnemyParty[i], MON_DATA_SPECIES);
            if (species != SPECIES_NONE
                && GetMonData(&gEnemyParty[i], MON_DATA_HP) != 0
                && !(gBitTable[i] & invalidMons)
                && gBattlerPartyIndexes[battlerIn1] != i
                && gBattlerPartyIndexes[battlerIn2] != i
                && i != ewram16068arr(battlerIn1)
                && i != ewram16068arr(battlerIn2)
                && !Can1HitKOOnSwitchIn(opposingBattler, i, battlerIn1, gEnemyParty))
            {
                u8 type1 = gBaseStats[species].type1;
                u8 type2 = gBaseStats[species].type2;
                u8 typeDmg = 10;
                ModulateByTypeEffectiveness(gBattleMons[opposingBattler].type1, type1, type2, &typeDmg);
                ModulateByTypeEffectiveness(gBattleMons[opposingBattler].type2, type1, type2, &typeDmg);
                if (bestDmg < typeDmg)
                {
                    bestDmg = typeDmg;
                    bestMonId = i;
                }
            }
            else
            {
                invalidMons |= gBitTable[i];
            }
        }

        // ok, we know the mon has the right typing but does it have at least one super effective move?
        if (bestMonId != 6)
        {
            for (i = 0; i < 4; i++)
            {
                move = GetMonData(&gEnemyParty[bestMonId], MON_DATA_MOVE1 + i);
                if (move != MOVE_NONE && TypeCalc(move, gActiveBattler, opposingBattler) & MOVE_RESULT_SUPER_EFFECTIVE)
                    break;
            }

            if (i != 4)
                return bestMonId; // has both the typing and at least one super effective move

            invalidMons |= gBitTable[bestMonId]; // sorry buddy, we want something better
        }
        else
        {
            invalidMons = 0x3F; // no viable mon to switch
        }
    }

    gDynamicBasePower = 0;
    gBattleStruct->dynamicMoveType = 0;
    gBattleStruct->dmgMultiplier = 1;
    gMoveResultFlags = 0;
    gCritMultiplier = 1;
    bestDmg = 0;
    bestMonId = 6;

    // if we couldn't find the best mon in terms of typing, find the one that deals most damage
    for (i = 0; i < 6; i++)
    {
        if ((u16)(GetMonData(&gEnemyParty[i], MON_DATA_SPECIES)) == SPECIES_NONE)
            continue;
        if (GetMonData(&gEnemyParty[i], MON_DATA_HP) == 0)
            continue;
        if (gBattlerPartyIndexes[battlerIn1] == i)
            continue;
        if (gBattlerPartyIndexes[battlerIn2] == i)
            continue;
        if (i == ewram16068arr(battlerIn1))
            continue;
        if (i == ewram16068arr(battlerIn2))
            continue;
        if (Can1HitKOOnSwitchIn(opposingBattler, i, battlerIn1, gEnemyParty))
            continue;

        for (j = 0; j < 4; j++)
        {
            move = GetMonData(&gEnemyParty[i], MON_DATA_MOVE1 + j);
            gBattleMoveDamage = 0;
            if (move != MOVE_NONE && gBattleMoves[move].power != 1)
            {
                AI_CalcDmg(gActiveBattler, opposingBattler);
                TypeCalc(move, gActiveBattler, opposingBattler);
            }
            if (bestDmg < gBattleMoveDamage)
            {
                bestDmg = gBattleMoveDamage;
                bestMonId = i;
            }
        }
    }

    return bestMonId;
}

// TODO: use PokemonItemEffect struct instead of u8 once it's documented
static u8 GetAI_ItemType(u8 itemId, const u8 *itemEffect) // NOTE: should take u16 as item Id argument
{
    if (itemId == ITEM_FULL_RESTORE)
        return AI_ITEM_FULL_RESTORE;
    if (itemEffect[4] & 4)
        return AI_ITEM_HEAL_HP;
    if (itemEffect[3] & 0x3F)
        return AI_ITEM_CURE_CONDITION;
    if (itemEffect[0] & 0x3F || itemEffect[1] != 0 || itemEffect[2] != 0)
        return AI_ITEM_X_STAT;
    if (itemEffect[3] & 0x80)
        return AI_ITEM_GUARD_SPECS;

    return AI_ITEM_NOT_RECOGNIZABLE;
}

static bool8 ShouldUseItem(void)
{
    s32 i;
    u8 validMons = 0;
    bool8 shouldUse = FALSE;

    for (i = 0; i < 6; i++)
    {
        if (GetMonData(&gEnemyParty[i], MON_DATA_HP) != 0
            && GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) != SPECIES_NONE
            && GetMonData(&gEnemyParty[i], MON_DATA_SPECIES2) != SPECIES_EGG)
        {
            validMons++;
        }
    }

    for (i = 0; i < 4; i++)
    {
        u16 item;
        const u8 *itemEffects;
        u8 paramOffset;
        u8 battlerSide;

        if (i != 0 && validMons > (AI_BATTLE_HISTORY->numItems - i) + 1)
            continue;
        item = AI_BATTLE_HISTORY->trainerItems[i];
        if (item == ITEM_NONE)
            continue;
        if (gItemEffectTable[item - 13] == NULL)
            continue;

        if (item == ITEM_ENIGMA_BERRY)
            itemEffects = gSaveBlock1.enigmaBerry.itemEffect;
        else
            itemEffects = gItemEffectTable[item - 13];

        ewram160D8(gActiveBattler) = GetAI_ItemType(item, itemEffects);

        switch (ewram160D8(gActiveBattler))
        {
        case AI_ITEM_FULL_RESTORE:
            if (gBattleMons[gActiveBattler].hp >= gBattleMons[gActiveBattler].maxHP / 4)
                break;
            if (gBattleMons[gActiveBattler].hp == 0)
                break;
            shouldUse = TRUE;
            break;
        case AI_ITEM_HEAL_HP:
            paramOffset = GetItemEffectParamOffset(item, 4, 4);
            if (paramOffset == 0)
                break;
            if (gBattleMons[gActiveBattler].hp == 0)
                break;
            if (gBattleMons[gActiveBattler].hp < gBattleMons[gActiveBattler].maxHP / 4 || gBattleMons[gActiveBattler].maxHP - gBattleMons[gActiveBattler].hp > itemEffects[paramOffset])
                shouldUse = TRUE;
            break;
        case AI_ITEM_CURE_CONDITION:
            ewram160DA(gActiveBattler) = 0;
            if (itemEffects[3] & 0x20 && gBattleMons[gActiveBattler].status1 & STATUS1_SLEEP)
            {
               ewram160DA(gActiveBattler) |= 0x20;
                shouldUse = TRUE;
            }
            if (itemEffects[3] & 0x10 && (gBattleMons[gActiveBattler].status1 & STATUS1_POISON || gBattleMons[gActiveBattler].status1 & STATUS1_TOXIC_POISON))
            {
               ewram160DA(gActiveBattler) |= 0x10;
                shouldUse = TRUE;
            }
            if (itemEffects[3] & 0x8 && gBattleMons[gActiveBattler].status1 & STATUS1_BURN)
            {
               ewram160DA(gActiveBattler) |= 0x8;
                shouldUse = TRUE;
            }
            if (itemEffects[3] & 0x4 && gBattleMons[gActiveBattler].status1 & STATUS1_FREEZE)
            {
               ewram160DA(gActiveBattler) |= 0x4;
                shouldUse = TRUE;
            }
            if (itemEffects[3] & 0x2 && gBattleMons[gActiveBattler].status1 & STATUS1_PARALYSIS)
            {
               ewram160DA(gActiveBattler) |= 0x2;
                shouldUse = TRUE;
            }
            if (itemEffects[3] & 0x1 && gBattleMons[gActiveBattler].status2 & STATUS2_CONFUSION)
            {
               ewram160DA(gActiveBattler) |= 0x1;
                shouldUse = TRUE;
            }
            break;
        case AI_ITEM_X_STAT:
           ewram160DA(gActiveBattler) = 0;
            if (gDisableStructs[gActiveBattler].isFirstTurn == 0)
                break;
            if (itemEffects[0] & 0xF)
               ewram160DA(gActiveBattler) |= 0x1;
            if (itemEffects[1] & 0xF0)
               ewram160DA(gActiveBattler) |= 0x2;
            if (itemEffects[1] & 0xF)
               ewram160DA(gActiveBattler) |= 0x4;
            if (itemEffects[2] & 0xF)
               ewram160DA(gActiveBattler) |= 0x8;
            if (itemEffects[2] & 0xF0)
               ewram160DA(gActiveBattler) |= 0x20;
            if (itemEffects[0] & 0x30)
               ewram160DA(gActiveBattler) |= 0x80;
            shouldUse = TRUE;
            break;
        case AI_ITEM_GUARD_SPECS:
            battlerSide = GetBattlerSide(gActiveBattler);
            if (gDisableStructs[gActiveBattler].isFirstTurn != 0 && gSideTimers[battlerSide].mistTimer == 0)
                shouldUse = TRUE;
            break;
        case AI_ITEM_NOT_RECOGNIZABLE:
            return FALSE;
        }

        if (shouldUse)
        {
            BtlController_EmitTwoReturnValues(1, B_ACTION_USE_ITEM, 0);
            ewram160D4(gActiveBattler) = item;
            AI_BATTLE_HISTORY->trainerItems[i] = 0;
            return shouldUse;
        }
    }

    return FALSE;
}
