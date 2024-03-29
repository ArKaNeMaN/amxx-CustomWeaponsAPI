#include <amxmodx>
#include <reapi>
#include <hamsandwich>
#include "Cwapi/Utils"

enum _E_Ham_WeaponHook{
    Ham:Ham_WH_Hook,
    Ham_WH_Func[64],
    bool:Ham_WH_Post,
}

static const _HAM_WEAPON_HOOKS[][_E_Ham_WeaponHook] = {
    {Ham_Spawn, "Hook_Spawn_Post", true},
    {Ham_Item_Holster, "Hook_ItemHolster_Pre", false},
    // {Ham_CS_Item_GetMaxSpeed, "Hook_PlayerGetMaxSpeed", false},
    // {Ham_Weapon_PrimaryAttack, "Hook_PrimaryAttack_Post", true},
    // {Ham_Weapon_PrimaryAttack, "Hook_PrimaryAttack_Pre", false},
    // {Ham_Weapon_SecondaryAttack, "Hook_SecondaryAttack", true},
    // {Ham_Item_AddToPlayer, "Hook_AddItemToPlayer_Post", true},
};

static Trie:gRegisteredRefWeapons = Invalid_Trie;

CWeapons_Hooks_Init() {
    CWeapons_Hooks__RegReapi();

    gRegisteredRefWeapons = TrieCreate();
}

CWeapons_Hooks_RegForWeapon(const sReference[], const sWeaponName[]) {
    // Больше обёрток богу обёрток!!!
    CWeapons_Hooks__RegHamOnce(sReference);
    register_clcmd(sWeaponName, "Hook_WeaponCommand");
}

static CWeapons_Hooks__RegReapi() {
    RegisterHookChain(RG_CSGameRules_PlayerKilled, "Hook_PlayerKilled_Post", true);

    RegisterHookChain(RG_CWeaponBox_SetModel, "Hook_WeaponBoxSetModel_Pre", false);
    RegisterHookChain(RG_CBasePlayer_AddPlayerItem, "Hook_AddPlayerItem_Pre", false);
    RegisterHookChain(RG_CBasePlayer_RemovePlayerItem, "Hook_RemovePlayerItem_Post", true);
    RegisterHookChain(RG_CBasePlayerWeapon_DefaultDeploy, "Hook_DefaultDeploy_Pre", false);
    RegisterHookChain(RG_CBasePlayerWeapon_DefaultDeploy, "Hook_DefaultDeploy_Post", true);
    RegisterHookChain(RG_CBasePlayer_TakeDamage, "Hook_PlayerTakeDamage_Post", false);
    RegisterHookChain(RG_CBasePlayerWeapon_DefaultReload, "Hook_DefaultReload_Pre", false);
    // RegisterHookChain(RG_CBasePlayerWeapon_DefaultReload, "Hook_DefaultReload_Post", true);
    // RegisterHookChain(RG_CBasePlayerWeapon_DefaultShotgunReload, "Hook_DefaultShotgunReload", false);
}

static CWeapons_Hooks__RegHamOnce(const sWeaponName[]) {
    if (!TrieKeyExists(gRegisteredRefWeapons, sWeaponName)) {
        CWeapons_Hooks__RegHam(sWeaponName);

        TrieSetCell(gRegisteredRefWeapons, sWeaponName, true);
    }
}

static CWeapons_Hooks__RegHam(const sWeaponName[]) {
    for (new i = 0; i < sizeof _HAM_WEAPON_HOOKS; i++) {
        RegisterHam(
            _HAM_WEAPON_HOOKS[i][Ham_WH_Hook],
            sWeaponName,
            _HAM_WEAPON_HOOKS[i][Ham_WH_Func],
            _HAM_WEAPON_HOOKS[i][Ham_WH_Post]
        );
    }
}

// Callbacks

public Hook_PlayerKilled_Post(const VictimId, const KillerId, const InflictorId) {
    if (KillerId != InflictorId) {
        return;
    }

    if (!IsUserIdValid(KillerId)) {
        return;
    }

    new ItemId = get_member(KillerId, m_pActiveItem);

    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return;
    }
    
    CWeapons_Call(Weapon, CWeapon_OnPlayerKilled, [iWeapon, ItemId, VictimId, KillerId]);
}

public Hook_WeaponCommand(const UserId) {
    if (!is_user_alive(UserId)) {
        return PLUGIN_HANDLED;
    }

    new sWeaponName[CWAPI_WEAPON_NAME_MAX_LEN];
    read_argv(0, sWeaponName, charsmax(sWeaponName));
    
    Dbg_Log("Hook_WeaponCommand(%n): Exec cmd.", UserId);
    
    new Weapon[S_CustomWeapon];
    if (!CWeapons_GetByName(sWeaponName, Weapon)) {
        return PLUGIN_CONTINUE;
    }
    
    Dbg_Log("Hook_WeaponCommand(%n): For weapon '%s'.", UserId, Weapon[CWeapon_Name]);

    engclient_cmd(UserId, Weapon[CWeapon_Reference]);
    return PLUGIN_HANDLED_MAIN;
}

public Hook_Spawn_Post(const ItemId) {
    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return;
    }

    Dbg_Log("Hook_Spawn_Post(%d): For weapon '%s'.", ItemId, Weapon[CWeapon_Name]);

    CWeapons_Call(Weapon, CWeapon_OnSpawn, [iWeapon, ItemId]);
    
    Dbg_Log("Hook_Spawn_Post(%d): Event return - %d", ItemId, Events_GetReturnedValue());

    if (Events_IsRet(CWAPI_STOP_MAIN)) {
        RemoveEntity(ItemId);
        return;
    }

    if (Events_IsRet(CWAPI_STOP)) {
        return;
    }

    // set_member_if_specified(ItemId, m_Weapon_bHasSecondaryAttack, Weapon[CWeapon_HasSecondaryAttack], false);
    rg_set_iteminfo_if_specified(ItemId, ItemInfo_iWeight, Weapon[CWeapon_Weight], -1);
    
    new WeaponIdType:WeaponId = WeaponIdType:rg_get_iteminfo(ItemId, ItemInfo_iId);
    if (WeaponId != WEAPON_KNIFE) {
        rg_set_iteminfo_if_specified(ItemId, ItemInfo_iMaxClip, Weapon[CWeapon_MaxClip], -1);
        rg_set_iteminfo_if_specified(ItemId, ItemInfo_iMaxAmmo1, Weapon[CWeapon_MaxAmmo], -1);

        InstantReload(ItemId);
    }

    MultItemDamage(ItemId, Weapon[CWeapon_DamageMult]);
    
    set_entvar(ItemId, var_CWAPI_ItemOwner, 0);

    CWeapons_Call(Weapon, CWeapon_OnSpawnPost, [iWeapon, ItemId]);
}

public Hook_AddPlayerItem_Pre(const UserId, const ItemId) {
    if (!is_user_connected(UserId)) {
        return HC_CONTINUE;
    }

    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return HC_CONTINUE;
    }

    if (get_entvar(ItemId, var_CWAPI_ItemOwner) == 0) {
        set_entvar(ItemId, var_CWAPI_ItemOwner, UserId);
    }

    CWeapons_Call(Weapon, CWeapon_OnAddPlayerItem, [iWeapon, ItemId, UserId]);

    return HC_CONTINUE;
}

public Hook_RemovePlayerItem_Post(const UserId, const ItemId) {
    if (!is_user_connected(UserId)) {
        return HC_CONTINUE;

    }
    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return HC_CONTINUE;
    }

    CWeapons_Call(Weapon, CWeapon_OnRemovePlayerItem, [iWeapon, ItemId, UserId]);

    return HC_CONTINUE;
}

public Hook_WeaponBoxSetModel_Pre(const iWeaponBox, const szModelName[]) {
    new ItemId = GetItemFromWeaponBox(iWeaponBox);

    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return HC_CONTINUE;
    }

    CWeapons_Call(Weapon, CWeapon_OnSetWeaponBoxModel, [iWeapon, iWeaponBox, ItemId]);

    if (Events_IsRet(CWAPI_STOP_MAIN)) {
        RemoveWeaponBox(iWeaponBox);
        return HC_BREAK;
    }

    if (
        Events_IsRet(CWAPI_CONTINUE)
        && Weapon[CWeapon_Models][CWeapon_Model_World][0]
    ) {
        SetHookChainArg(2, ATYPE_STRING, Weapon[CWeapon_Models][CWeapon_Model_World]);
    }

    return HC_CONTINUE;
}

public Hook_DefaultDeploy_Pre(const ItemId, szViewModel[], szWeaponModel[]) {
    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return HC_CONTINUE;
    }

    Dbg_Log("Hook_DefaultDeploy_Pre(%d): For weapon '%s'.", ItemId, Weapon[CWeapon_Name]);

    if (Weapon[CWeapon_Models][CWeapon_Model_View][0]) {
        SetHookChainArg(2, ATYPE_STRING, Weapon[CWeapon_Models][CWeapon_Model_View]);
        Dbg_Log("Hook_DefaultDeploy_Pre(%d): Set '%s' as view model.", ItemId, Weapon[CWeapon_Models][CWeapon_Model_View]);
    }
    
    if (Weapon[CWeapon_Models][CWeapon_Model_Player][0]) {
        SetHookChainArg(3, ATYPE_STRING, Weapon[CWeapon_Models][CWeapon_Model_Player]);
        Dbg_Log("Hook_DefaultDeploy_Pre(%d): Set '%s' as player model.", ItemId, Weapon[CWeapon_Models][CWeapon_Model_Player]);
    }
    
    return HC_CONTINUE;
}

public Hook_DefaultDeploy_Post(const ItemId) {
    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return HC_CONTINUE;
    }

    Dbg_Log("Hook_DefaultDeploy_Post(%d): For weapon '%s'.", ItemId, Weapon[CWeapon_Name]);

    new Float:fDeployTime = get_member(get_member(ItemId, m_pPlayer), m_flNextAttack);
    if (Weapon[CWeapon_DeployTime] >= 0.0) {
        fDeployTime = Weapon[CWeapon_DeployTime];
    }

    CWeapons_Call(Weapon, CWeapon_OnDeploy, [iWeapon, ItemId, fDeployTime]);
    
    SetWeaponNextAttack(ItemId, fDeployTime);
    Dbg_Log("Hook_DefaultDeploy_Post(%d): Set next attack after %.2f seconds.", ItemId, fDeployTime);
    
    return HC_CONTINUE;
}

public Hook_ItemHolster_Pre(const ItemId) {
    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return HAM_IGNORED;
    }
    
    Dbg_Log("Hook_ItemHolster_Pre(%d) - Weapon[CWeapon_Name] = %s", ItemId, Weapon[CWeapon_Name]);

    CWeapons_Call(Weapon, CWeapon_OnHolster, [iWeapon, ItemId]);
    
    return HAM_IGNORED;
}

public Hook_PlayerTakeDamage_Post(const VictimId, InflictorId, AttackerId, Float:fDamage, iDamageBits) {
    if (
        !is_user_connected(VictimId)
        || !is_user_connected(AttackerId)
    ) {
        return HC_CONTINUE;
    }
        
    new ItemId = GetAttackerWeapon(AttackerId, InflictorId);

    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return HC_CONTINUE;
    }

    CWeapons_Call(Weapon, CWeapon_OnDamage, [iWeapon, ItemId, VictimId, InflictorId, AttackerId, fDamage, iDamageBits]);
    SetHookChainArg(4, ATYPE_FLOAT, fDamage);
    SetHookChainArg(5, ATYPE_INTEGER, iDamageBits);

    if (Events_IsRet(CWAPI_STOP_MAIN)) {
        SetHookChainReturn(ATYPE_INTEGER, 0);
        return HC_BREAK;
    }

    return HC_CONTINUE;
}

public Hook_DefaultReload_Pre(const ItemId, iClipSize, iAnim, Float:fDelay) {
    new Weapon[S_CustomWeapon], T_CustomWeapon:iWeapon;
    if (!CWeapons_GetByItem(ItemId, Weapon, iWeapon)) {
        return HC_CONTINUE;
    }

    if (Weapon[CWeapon_ReloadTime] >= 0.0) {
        fDelay = Weapon[CWeapon_ReloadTime];
    }

    CWeapons_Call(Weapon, CWeapon_OnReload, [iWeapon, ItemId, iClipSize, iAnim, fDelay]);
    SetHookChainArg(2, ATYPE_INTEGER, iClipSize);
    SetHookChainArg(3, ATYPE_INTEGER, iAnim);
    SetHookChainArg(4, ATYPE_FLOAT, fDelay);

    if (
        Events_IsRet(CWAPI_STOP_MAIN)
        || get_member(ItemId, m_Weapon_iClip) >= iClipSize
    ) {
        SetWeaponIdleAnim(get_member(ItemId, m_pPlayer), ItemId);

        SetHookChainReturn(ATYPE_INTEGER, false);
        return HC_BREAK;
    }

    return HC_CONTINUE;
}

// public Hook_DefaultReload_Post(const ItemId, iClipSize, iAnim, Float:fDelay) {
//     new WeaponId = GetWeapId(ItemId);
//     if (!IsCustomWeapon(WeaponId)) {
//         return HC_CONTINUE;
//     }

//     new Data[CWAPI_WeaponData];
//     ArrayGetArray(CustomWeapons, WeaponId, Data);

//     if (Data[CWAPI_WD_Accuracy] >= 0.0) {
//         set_member(ItemId, m_Weapon_flAccuracy, Data[CWAPI_WD_Accuracy]);
//     }

//     return HC_CONTINUE;
// }

// public Hook_DefaultShotgunReload(const ItemId, iAnim, iStartAnim, Float:fDelay, Float:fStartDelay, const pszReloadSound1[], const pszReloadSound2[]) {
//     new WeaponId = GetWeapId(ItemId);
//     if (!IsCustomWeapon(WeaponId)) {
//         return HC_CONTINUE;
//     }

//     new UserId = get_member(ItemId, m_pPlayer);

//     if (
//         get_member(ItemId, m_Weapon_iClip) >= rg_get_iteminfo(ItemId, ItemInfo_iMaxClip)
//         || !CallWeaponEvent(WeaponId, CWAPI_WE_Reload, ItemId)
//     ) {
//         SetWeaponIdleAnim(UserId, ItemId);
//         SetHookChainReturn(ATYPE_BOOL, false);

//         return HC_BREAK;
//     }
    
//     new Data[CWAPI_WeaponData];
//     ArrayGetArray(CustomWeapons, WeaponId, Data);

//     if (Data[CWAPI_WD_ReloadTime] >= 0.0) {
//         SetHookChainArg(4, ATYPE_FLOAT, Data[CWAPI_WD_ReloadTime]);
//         SetHookChainArg(5, ATYPE_FLOAT, Data[CWAPI_WD_ReloadTime]);
//     }
    
//     return HC_CONTINUE;
// }

// public Hook_PlayerGetMaxSpeed(const ItemId) {
//     if (!IsCustomWeapon(GetWeapId(ItemId))) {
//         return HAM_IGNORED;
//     }
    
//     new Data[CWAPI_WeaponData];
//     ArrayGetArray(CustomWeapons, GetWeapId(ItemId), Data);

//     if (Data[CWAPI_WD_MaxWalkSpeed] >= 0) {
//         SetHamReturnFloat(Data[CWAPI_WD_MaxWalkSpeed]);
//         return HAM_SUPERCEDE;
//     }

//     return HAM_IGNORED;
// }

// public Hook_PrimaryAttack_Pre(ItemId) {
//     new WeaponId = GetWeapId(ItemId);

//     if (
//         !IsCustomWeapon(WeaponId)
//         || get_member(ItemId, m_Weapon_iClip) < 1
//         || (
//             IsPistol(ItemId)
//             && get_member(ItemId, m_Weapon_iShotsFired)+1 > 1
//         )
//     ) {
//         return;
//     }

//     new Data[CWAPI_WeaponData];
//     ArrayGetArray(CustomWeapons, WeaponId, Data);
    
//     CallWeaponEvent(WeaponId, CWAPI_WE_PrimaryAttack, ItemId);
//     return;
// }

// public Hook_PrimaryAttack_Post(ItemId) {
//     new WeaponId = GetWeapId(ItemId);

//     if (
//         !IsCustomWeapon(WeaponId)
//         || get_member(ItemId, m_Weapon_fFireOnEmpty)
//         || (
//             IsPistol(ItemId)
//             && get_member(ItemId, m_Weapon_iShotsFired) > 1
//         )
//     ) {
//         return HAM_IGNORED;
//     }

//     new Data[CWAPI_WeaponData];
//     ArrayGetArray(CustomWeapons, WeaponId, Data);

//     // CallWeaponEvent(WeaponId, CWAPI_WE_PrimaryAttack, ItemId);

//     if (Data[CWAPI_WD_PrimaryAttackRate] > 0.0) {
//         SetWeaponNextAttack(ItemId, Data[CWAPI_WD_PrimaryAttackRate]);
//     }

//     new UserId = get_member(ItemId, m_pPlayer);

//     if (IsWeaponSilenced(ItemId)) if (Data[CWAPI_WD_Sounds][CWAPI_WS_ShotSilent][0]) {
//         rh_emit_sound2(UserId, 0, CHAN_WEAPON, Data[CWAPI_WD_Sounds][CWAPI_WS_ShotSilent]);
//     } else if (Data[CWAPI_WD_Sounds][CWAPI_WS_Shot][0]) {
//         rh_emit_sound2(UserId, 0, CHAN_WEAPON, Data[CWAPI_WD_Sounds][CWAPI_WS_Shot]);
//     }

//     return HAM_IGNORED;
// }

// public Hook_SecondaryAttack(ItemId) {
//     new WeaponId = GetWeapId(ItemId);
//     if (!IsCustomWeapon(WeaponId)) {
//         return HAM_IGNORED;
//     }

//     if (!CallWeaponEvent(WeaponId, CWAPI_WE_SecondaryAttack, ItemId)) {
//         return HAM_IGNORED;
//     }

//     new Data[CWAPI_WeaponData];
//     ArrayGetArray(CustomWeapons, WeaponId, Data);

//     if (Data[CWAPI_WD_SecondaryAttackRate] > 0.0) {
//         SetWeaponNextAttack(ItemId, Data[CWAPI_WD_SecondaryAttackRate]);
//     }
    
//     return HAM_IGNORED;
// }

// public Hook_AddItemToPlayer_Post(const ItemId, const UserId) {
//     new WeaponId = GetWeapId(ItemId);
//     if (!IsCustomWeapon(WeaponId)) {
//         return HAM_IGNORED;
//     }

//     new Data[CWAPI_WeaponData];
//     ArrayGetArray(CustomWeapons, WeaponId, Data);

//     if (Data[CWAPI_WD_HasCustomHud]) {
//         ShowWeaponListHud(UserId, ItemId);
//     }

//     return HAM_IGNORED;
// }
