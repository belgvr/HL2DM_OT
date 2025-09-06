//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "gamerules.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "engine/IEngineSound.h"

#if defined(CLIENT_DLL)
#include "c_hl2mp_player.h"
#else
#include "hl2mp_player.h"
#include "grenade_tripmine.h"
#include "grenade_satchel.h"
#include "entitylist.h"
#include "eventqueue.h"
#endif

#include "hl2mp/weapon_slam.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ==================================================================================================
// CVARS FOR SLAM CUSTOMIZATION
// ==================================================================================================
// Throw/Charge System
ConVar sv_slam_throw_charge_enable("sv_slam_throw_charge_enable", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enables charge-up for the SLAM satchel throw.");
ConVar sv_slam_charge_time("sv_slam_charge_time", "2.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Max charge-up time for all SLAM throws in seconds.");
ConVar sv_slam_charge_hud("sv_slam_charge_hud", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Shows a charge bar on the HUD if enabled for SLAM.");
ConVar sv_slam_throw_velocity_min("sv_slam_throw_velocity_min", "200.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum velocity for a charged satchel throw.");
ConVar sv_slam_throw_velocity_max("sv_slam_throw_velocity_max", "500.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum velocity for a charged satchel throw.");
ConVar sv_slam_throw_forward_offset("sv_slam_throw_forward_offset", "18.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Forward offset for the SLAM throw origin.");
ConVar sv_slam_throw_velocity_default("sv_slam_throw_velocity_default", "500.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Default velocity for satchel throw when charge is disabled ");

// Timing/Delay CVars
ConVar sv_slam_deploy_delay("sv_slam_deploy_delay", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay after deploying SLAM before next action can be taken.");
ConVar sv_slam_secondary_delay("sv_slam_secondary_delay", "0.3", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay after secondary attack (detonate) before next action.");
ConVar sv_slam_primary_delay("sv_slam_primary_delay", "0.3", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay after primary attack (place/throw) before next action.");
ConVar sv_slam_switch_mode_delay("sv_slam_switch_mode_delay", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay when switching between tripmine/satchel modes.");
ConVar sv_slam_holster_delay("sv_slam_holster_delay", "0.2", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay when holstering SLAM.");
ConVar sv_slam_wall_switch_delay("sv_slam_wall_switch_delay", "0.3", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay when switching between wall attach and throw modes.");
ConVar sv_slam_detonate_ready_delay("sv_slam_detonate_ready_delay", "0.2", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay for detonation explosion after trigger.");
ConVar sv_slam_hud_update_rate("sv_slam_hud_update_rate", "0.05", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Rate at which charge HUD updates (in seconds).");

// Other settings
ConVar sv_slam_attachment_range("sv_slam_attachment_range", "42.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Range for attaching SLAM to surfaces.");
ConVar sv_slam_throw_vertical_offset("sv_slam_throw_vertical_offset", "24.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Vertical offset for throw origin.");
ConVar sv_slam_attach_range("sv_slam_attach_range", "128.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Range for satchel attachment mode.");

// SLAM Steal System
ConVar sv_slam_allow_steal("sv_slam_allow_steal", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Allow players to steal armed SLAMs with +use key");
ConVar sv_slam_steal_range("sv_slam_steal_range", "100.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Range for stealing SLAMs");
ConVar sv_slam_steal_announce("sv_slam_steal_announce", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Announce SLAM steals in chat");

#define	SLAM_PRIMARY_VOLUME		450

IMPLEMENT_NETWORKCLASS_ALIASED(Weapon_SLAM, DT_Weapon_SLAM)

BEGIN_NETWORK_TABLE(CWeapon_SLAM, DT_Weapon_SLAM)
#ifdef CLIENT_DLL
RecvPropInt(RECVINFO(m_tSlamState)),
RecvPropBool(RECVINFO(m_bDetonatorArmed)),
RecvPropBool(RECVINFO(m_bNeedDetonatorDraw)),
RecvPropBool(RECVINFO(m_bNeedDetonatorHolster)),
RecvPropBool(RECVINFO(m_bNeedReload)),
RecvPropBool(RECVINFO(m_bClearReload)),
RecvPropBool(RECVINFO(m_bThrowSatchel)),
RecvPropBool(RECVINFO(m_bAttachSatchel)),
RecvPropBool(RECVINFO(m_bAttachTripmine)),
#else
SendPropInt(SENDINFO(m_tSlamState)),
SendPropBool(SENDINFO(m_bDetonatorArmed)),
SendPropBool(SENDINFO(m_bNeedDetonatorDraw)),
SendPropBool(SENDINFO(m_bNeedDetonatorHolster)),
SendPropBool(SENDINFO(m_bNeedReload)),
SendPropBool(SENDINFO(m_bClearReload)),
SendPropBool(SENDINFO(m_bThrowSatchel)),
SendPropBool(SENDINFO(m_bAttachSatchel)),
SendPropBool(SENDINFO(m_bAttachTripmine)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL

BEGIN_PREDICTION_DATA(CWeapon_SLAM)
DEFINE_PRED_FIELD(m_tSlamState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bDetonatorArmed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bNeedDetonatorDraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bNeedDetonatorHolster, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bNeedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bClearReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bThrowSatchel, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bAttachSatchel, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bAttachTripmine, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()

#endif

LINK_ENTITY_TO_CLASS(weapon_slam, CWeapon_SLAM);
PRECACHE_WEAPON_REGISTER(weapon_slam);

#ifndef CLIENT_DLL

BEGIN_DATADESC(CWeapon_SLAM)

DEFINE_FIELD(m_tSlamState, FIELD_INTEGER),
DEFINE_FIELD(m_bDetonatorArmed, FIELD_BOOLEAN),
DEFINE_FIELD(m_bNeedDetonatorDraw, FIELD_BOOLEAN),
DEFINE_FIELD(m_bNeedDetonatorHolster, FIELD_BOOLEAN),
DEFINE_FIELD(m_bNeedReload, FIELD_BOOLEAN),
DEFINE_FIELD(m_bClearReload, FIELD_BOOLEAN),
DEFINE_FIELD(m_bThrowSatchel, FIELD_BOOLEAN),
DEFINE_FIELD(m_bAttachSatchel, FIELD_BOOLEAN),
DEFINE_FIELD(m_bAttachTripmine, FIELD_BOOLEAN),
DEFINE_FIELD(m_flWallSwitchTime, FIELD_TIME),
DEFINE_FIELD(m_flChargeStartTime, FIELD_TIME),
DEFINE_FIELD(m_flLastHUDUpdateTime, FIELD_TIME),
DEFINE_FIELD(m_bIsCharging, FIELD_BOOLEAN),

// Function Pointers
DEFINE_FUNCTION(SlamTouch),

END_DATADESC()

acttable_t	CWeapon_SLAM::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true },
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_SLAM,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_SLAM,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_SLAM,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_SLAM,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_SLAM,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_SLAM,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_SLAM,					false },
};

IMPLEMENT_ACTTABLE(CWeapon_SLAM);
#endif

void CWeapon_SLAM::Spawn()
{
	BaseClass::Spawn();

	Precache();

	FallInit();// get ready to fall down

	m_tSlamState = (int)SLAM_SATCHEL_THROW;
	m_flWallSwitchTime = 0;

	// Give 1 piece of default ammo when first picked up
	m_iClip2 = 1;
}

void CWeapon_SLAM::Precache(void)
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
	UTIL_PrecacheOther("npc_tripmine");
	UTIL_PrecacheOther("npc_satchel");
#endif

	PrecacheScriptSound("Weapon_SLAM.SatchelDetonate");
	PrecacheScriptSound("Weapon_SLAM.SatchelThrow");
}

//------------------------------------------------------------------------------
// Purpose : Override to use slam's pickup touch function
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CWeapon_SLAM::SetPickupTouch(void)
{
	SetTouch(&CWeapon_SLAM::SlamTouch);
#ifndef CLIENT_DLL
	if (GetSpawnFlags() & SF_NORESPAWN)
	{
		SetThink(&CWeapon_SLAM::SUB_Remove);
		SetNextThink(gpGlobals->curtime + 30.0f);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Override so give correct ammo
// Input  : pOther - the entity that touched me
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::SlamTouch(CBaseEntity* pOther)
{
#ifdef GAME_DLL
	CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pOther);

	// Can I even pick stuff up?
	if (pBCC && !pBCC->IsAllowedToPickupWeapons())
		return;
#endif

	// ---------------------------------------------------
	//  First give weapon to touching entity if allowed
	// ---------------------------------------------------
	BaseClass::DefaultTouch(pOther);
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool CWeapon_SLAM::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	SetThink(NULL);
	return BaseClass::Holster(pSwitchingTo);
}

#ifdef GAME_DLL
const CUtlVector< CBaseEntity* >& CWeapon_SLAM::GetSatchelVector()
{
	m_SatchelVector.RemoveAll();

	CBaseEntity* pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname(pEntity, "npc_satchel")) != NULL)
	{
		CSatchelCharge* pSatchel = dynamic_cast<CSatchelCharge*>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() && GetOwner() && pSatchel->GetThrower() == GetOwner())
		{
			m_SatchelVector.AddToTail(pSatchel);
		}
	}

	return m_SatchelVector;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: SLAM has no reload, but must call weapon idle to update state
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeapon_SLAM::Reload(void)
{
	WeaponIdle();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::PrimaryAttack(void)
{
	CBaseCombatCharacter* pOwner = GetOwner();
	if (!pOwner)
	{
		return;
	}

	// Only allow this attack if we have ammo
	if (pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0)
	{
		return;
	}

	switch (m_tSlamState)
	{
	case SLAM_TRIPMINE_READY:
		StartTripmineAttach();
		break;
	case SLAM_SATCHEL_THROW:
		// Apenas inicia o carregamento se a CVar estiver ativada
		if (sv_slam_throw_charge_enable.GetBool())
		{
			m_bIsCharging = true;
			m_flChargeStartTime = gpGlobals->curtime;
			SendWeaponAnim(ACT_SLAM_THROW_THROW_ND);
			m_flNextPrimaryAttack = FLT_MAX;
			m_flNextSecondaryAttack = FLT_MAX;
		}
		else
		{
			// Comportamento original
			StartSatchelThrow();
			m_bThrowSatchel = true;
		}
		break;
	case SLAM_SATCHEL_ATTACH:
		StartSatchelAttach();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Secondary attack switches between satchel charge and tripmine mode
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::SecondaryAttack(void)
{
	CBaseCombatCharacter* pOwner = GetOwner();
	if (!pOwner)
	{
		return;
	}

	if (m_bDetonatorArmed)
	{
		StartSatchelDetonate();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::SatchelDetonate()
{
#ifndef CLIENT_DLL
	CBaseEntity* pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname(pEntity, "npc_satchel")) != NULL)
	{
		CSatchelCharge* pSatchel = dynamic_cast<CSatchelCharge*>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() && GetOwner() && pSatchel->GetThrower() == GetOwner())
		{
			// USANDO CVAR PARA DELAY DE DETONAÇÃO
			g_EventQueue.AddEvent(pSatchel, "Explode", sv_slam_detonate_ready_delay.GetFloat(), GetOwner(), GetOwner());
		}
	}
#endif
	// Play sound for pressing the detonator
	EmitSound("Weapon_SLAM.SatchelDetonate");

	m_bDetonatorArmed = false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if there are any undetonated charges in the world
//			that belong to this player
// Input  :
// Output  :
//-----------------------------------------------------------------------------
bool CWeapon_SLAM::AnyUndetonatedCharges(void)
{
#ifndef CLIENT_DLL
	CBaseEntity* pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname(pEntity, "npc_satchel")) != NULL)
	{
		CSatchelCharge* pSatchel = dynamic_cast<CSatchelCharge*>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() && pSatchel->GetThrower() == GetOwner())
		{
			return true;
		}
	}
#endif
	return false;
}

ConVar mp_allow_immediate_slam_detonation("mp_allow_immediate_slam_detonation", "0", FCVAR_NOTIFY, "If enabled, allows a thrown SLAM to be detonated as soon as it is thrown rather than wait for the deploy animation of the detonator to finish");

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::StartSatchelDetonate()
{
	Activity activity = GetActivity();
	if (activity != ACT_SLAM_STICKWALL_IDLE && activity != ACT_SLAM_THROW_TO_STICKWALL)
	{
		if (!mp_allow_immediate_slam_detonation.GetBool())
		{
			if (GetActivity() != ACT_SLAM_STICKWALL_IDLE
				&& GetActivity() != ACT_SLAM_DETONATOR_IDLE && GetActivity() != ACT_SLAM_THROW_IDLE)
				return;
		}

		// -----------------------------------------
		//  Play detonate animation
		// -----------------------------------------
		if (m_bNeedReload || m_tSlamState == SLAM_TRIPMINE_READY)
		{
			SendWeaponAnim(ACT_SLAM_DETONATOR_DETONATE);
		}
		else
		{
			SendWeaponAnim(m_tSlamState == SLAM_SATCHEL_THROW ? ACT_SLAM_THROW_DETONATE : ACT_SLAM_STICKWALL_DETONATE);
		}
	}
	else
	{
		SendWeaponAnim(ACT_SLAM_STICKWALL_DETONATE);
	}
	SatchelDetonate();

	// USANDO CVARS PARA DELAYS
	m_flNextPrimaryAttack = gpGlobals->curtime + sv_slam_secondary_delay.GetFloat();
	m_flNextSecondaryAttack = gpGlobals->curtime + sv_slam_secondary_delay.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::TripmineAttach(void)
{
	CHL2MP_Player* pOwner = ToHL2MPPlayer(GetOwner());
	if (!pOwner)
	{
		return;
	}

	m_bAttachTripmine = false;

	Vector vecSrc, vecAiming;
	// Take the eye position and direction
	vecSrc = pOwner->EyePosition();
	QAngle angles = pOwner->GetLocalAngles();
	AngleVectors(angles, &vecAiming);

	trace_t tr;
	if (CanAttachSLAM(&tr))
	{
		CBaseEntity* pEntity = tr.m_pEnt;
		if (pEntity && !(pEntity->GetFlags() & FL_CONVEYOR))
		{
#ifndef CLIENT_DLL
			CBaseEntity* pEntity = tr.m_pEnt;
			CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pEntity);
			if (pBCC)
			{
				return;
			}

			QAngle angles;
			VectorAngles(tr.plane.normal, angles);
			angles.x += 90;

			CBaseEntity* pEnt = CBaseEntity::Create("npc_tripmine", tr.endpos + tr.plane.normal * 3, angles, NULL);
			CTripmineGrenade* pMine = (CTripmineGrenade*)pEnt;
			pMine->AttachToEntity(pEntity);
			pMine->m_hOwner = GetOwner();

			//// GUARDAR NOME DO DONO DA TRIPMINE
			//CBasePlayer* pOwnerPlayer = ToBasePlayer(GetOwner());
			//if (pOwnerPlayer)
			//{
			//	pMine->SetOwnerName(pOwnerPlayer->GetPlayerName());
			//}
#endif
			pOwner->RemoveAmmo(1, m_iSecondaryAmmoType);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::StartTripmineAttach(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
	{
		return;
	}

	Vector vecSrc, vecAiming;

	// Take the eye position and direction
	vecSrc = pPlayer->EyePosition();

	QAngle angles = pPlayer->GetLocalAngles();

	AngleVectors(angles, &vecAiming);

	trace_t tr;

	if (CanAttachSLAM(&tr))
	{
		CBaseEntity* pEntity = tr.m_pEnt;
		if (pEntity && !(pEntity->GetFlags() & FL_CONVEYOR))
		{
			// player "shoot" animation
			pPlayer->SetAnimation(PLAYER_ATTACK1);

			// -----------------------------------------
			//  Play attach animation
			// -----------------------------------------

			if (m_bDetonatorArmed)
			{
				SendWeaponAnim(ACT_SLAM_STICKWALL_ATTACH);
			}
			else
			{
				SendWeaponAnim(ACT_SLAM_TRIPMINE_ATTACH);
			}

			m_bNeedReload = true;
			m_bAttachTripmine = true;
			m_bNeedDetonatorDraw = m_bDetonatorArmed;
		}
	}

	// USANDO CVARS PARA DELAYS
	m_flNextPrimaryAttack = gpGlobals->curtime + sv_slam_primary_delay.GetFloat();
	m_flNextSecondaryAttack = gpGlobals->curtime + sv_slam_primary_delay.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::SatchelThrow(float flVelocity)
{
#ifndef CLIENT_DLL
	m_bThrowSatchel = false;

	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer) return;

	// Se charge estiver desabilitado, usa velocidade padrão
	if (!sv_slam_throw_charge_enable.GetBool())
	{
		flVelocity = sv_slam_throw_velocity_default.GetFloat();
	}

	Vector vecSrc = pPlayer->WorldSpaceCenter();
	Vector vecFacing = pPlayer->BodyDirection3D();
	vecSrc = vecSrc + vecFacing * sv_slam_throw_forward_offset.GetFloat();
	vecSrc.z += sv_slam_throw_vertical_offset.GetFloat();

	Vector vecThrow;
	GetOwner()->GetVelocity(&vecThrow, NULL);
	vecThrow += vecFacing * flVelocity;

	if (CanAttachSLAM())
	{
		vecThrow = vecFacing;
		vecSrc = pPlayer->WorldSpaceCenter() + vecFacing * 5.0;
	}

	CSatchelCharge* pSatchel = (CSatchelCharge*)Create("npc_satchel", vecSrc, vec3_angle, GetOwner());

	if (pSatchel)
	{
		pSatchel->SetThrower(GetOwner());
		pSatchel->ApplyAbsVelocityImpulse(vecThrow);
		pSatchel->SetLocalAngularVelocity(QAngle(0, 400, 0));
		pSatchel->m_bIsLive = true;
		pSatchel->m_pMyWeaponSLAM = this;

		// GUARDAR NOME DO DONO
		//CBasePlayer* pOwner = ToBasePlayer(GetOwner());
		//if (pOwner)
		//{
		//	//Q_strncpy(pSatchel->m_szOwnerName, pOwner->GetPlayerName(), 64);
		//	pSatchel->SetOwnerName(pOwner->GetPlayerName());

		//}

		// SÓ REMOVE MUNIÇÃO SE SATCHEL FOI CRIADO COM SUCESSO
		pPlayer->RemoveAmmo(1, m_iSecondaryAmmoType);

		// SE CHEGOU A 0 MUNIÇÃO, FORÇA RELOAD PARA MODO DETONADOR
		if (pPlayer->GetAmmoCount(m_iSecondaryAmmoType) <= 0 && AnyUndetonatedCharges())
		{
			m_bDetonatorArmed = true;
			m_bNeedReload = true;
			m_bNeedDetonatorDraw = true;
		}
	}

	pPlayer->SetAnimation(PLAYER_ATTACK1);
#endif

	EmitSound("Weapon_SLAM.SatchelThrow");

	m_flNextPrimaryAttack = gpGlobals->curtime + sv_slam_primary_delay.GetFloat();
	m_flNextSecondaryAttack = gpGlobals->curtime + sv_slam_primary_delay.GetFloat();

	pPlayer->OnMyWeaponFired(this);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::StartSatchelThrow(void)
{
	// -----------------------------------------
	//  Play throw animation
	// -----------------------------------------
	if (m_bDetonatorArmed)
	{
		SendWeaponAnim(ACT_SLAM_THROW_THROW);
	}
	else
	{
		SendWeaponAnim(ACT_SLAM_THROW_THROW_ND);
		if (!m_bDetonatorArmed)
		{
			m_bDetonatorArmed = true;
			m_bNeedDetonatorDraw = true;
		}
	}

	m_bNeedReload = true;
	m_bThrowSatchel = true;

	// USANDO CVARS PARA DELAYS
	m_flNextPrimaryAttack = gpGlobals->curtime + sv_slam_primary_delay.GetFloat();
	m_flNextSecondaryAttack = gpGlobals->curtime + sv_slam_primary_delay.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::SatchelAttach(void)
{
#ifndef CLIENT_DLL
	CBaseCombatCharacter* pOwner = GetOwner();
	if (!pOwner)
	{
		return;
	}

	m_bAttachSatchel = false;

	Vector vecSrc = pOwner->Weapon_ShootPosition();
	Vector vecAiming = pOwner->BodyDirection2D();

	trace_t tr;

	if (CanAttachSLAM(&tr))
	{
		CBaseEntity* pEntity = tr.m_pEnt;
		if (pEntity && !(pEntity->GetFlags() & FL_CONVEYOR))
		{
			QAngle angles;
			VectorAngles(tr.plane.normal, angles);
			angles.y -= 90;
			angles.z -= 90;
			tr.endpos.z -= 6.0f;

			CSatchelCharge* pSatchel = (CSatchelCharge*)CBaseEntity::Create("npc_satchel", tr.endpos + tr.plane.normal * 3, angles, NULL);
			pSatchel->SetMoveType(MOVETYPE_FLY); // no gravity
			pSatchel->m_bIsAttached = true;
			pSatchel->m_bIsLive = true;
			pSatchel->SetThrower(GetOwner());
			pSatchel->SetOwnerEntity(((CBaseEntity*)GetOwner()));
			pSatchel->m_pMyWeaponSLAM = this;

			pOwner->RemoveAmmo(1, m_iSecondaryAmmoType);
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::StartSatchelAttach(void)
{
#ifndef CLIENT_DLL
	CBaseCombatCharacter* pOwner = GetOwner();
	if (!pOwner)
	{
		return;
	}

	Vector vecSrc = pOwner->Weapon_ShootPosition();
	Vector vecAiming = pOwner->BodyDirection2D();

	trace_t tr;

	// USANDO CVAR PARA RANGE DE ATTACHMENT
	UTIL_TraceLine(vecSrc, vecSrc + (vecAiming * sv_slam_attach_range.GetFloat()), MASK_SOLID, pOwner, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0)
	{
		CBaseEntity* pEntity = tr.m_pEnt;
		if (pEntity && !(pEntity->GetFlags() & FL_CONVEYOR))
		{
			// Only the player fires this way so we can cast
			CBasePlayer* pPlayer = ToBasePlayer(pOwner);

			// player "shoot" animation
			pPlayer->SetAnimation(PLAYER_ATTACK1);

			// -----------------------------------------
			//  Play attach animation
			// -----------------------------------------
			if (m_bDetonatorArmed)
			{
				SendWeaponAnim(ACT_SLAM_STICKWALL_ATTACH);
			}
			else
			{
				SendWeaponAnim(ACT_SLAM_STICKWALL_ND_ATTACH);
				if (!m_bDetonatorArmed)
				{
					m_bDetonatorArmed = true;
					m_bNeedDetonatorDraw = true;
				}
			}

			m_bNeedReload = true;
			m_bAttachSatchel = true;

			// USANDO CVARS PARA DELAYS
			m_flNextPrimaryAttack = gpGlobals->curtime + sv_slam_primary_delay.GetFloat();
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::SetSlamState(int newState)
{
	// Set set and set idle time so animation gets updated with state change
	m_tSlamState = newState;
	SetWeaponIdleTime(gpGlobals->curtime);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::SLAMThink(void)
{
	if (m_flWallSwitchTime > gpGlobals->curtime)
		return;

	// If not in tripmine mode we need to check to see if we are close to
	// a wall. If we are we go into satchel_attach mode
	CBaseCombatCharacter* pOwner = GetOwner();

	if ((pOwner && pOwner->GetAmmoCount(m_iSecondaryAmmoType) > 0))
	{
		if (CanAttachSLAM())
		{
			if (m_tSlamState == SLAM_SATCHEL_THROW)
			{
				SetSlamState(SLAM_TRIPMINE_READY);
				int iAnim = m_bDetonatorArmed ? ACT_SLAM_THROW_TO_STICKWALL : ACT_SLAM_THROW_TO_TRIPMINE_ND;
				SendWeaponAnim(iAnim);
				// USANDO CVAR PARA WALL SWITCH DELAY
				m_flWallSwitchTime = gpGlobals->curtime + sv_slam_wall_switch_delay.GetFloat();
				m_bNeedReload = false;
			}
		}
		else
		{
			if (m_tSlamState == SLAM_TRIPMINE_READY)
			{
				SetSlamState(SLAM_SATCHEL_THROW);
				int iAnim = m_bDetonatorArmed ? ACT_SLAM_STICKWALL_TO_THROW : ACT_SLAM_TRIPMINE_TO_THROW_ND;
				SendWeaponAnim(iAnim);
				// USANDO CVAR PARA WALL SWITCH DELAY
				m_flWallSwitchTime = gpGlobals->curtime + sv_slam_wall_switch_delay.GetFloat();
				m_bNeedReload = m_bAttachTripmine = false;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeapon_SLAM::CanAttachSLAM(trace_t* pTrace)
{
	CHL2MP_Player* pOwner = ToHL2MPPlayer(GetOwner());

	if (!pOwner)
	{
		return false;
	}

	Vector vecSrc, vecAiming;

	// Take the eye position and direction
	vecSrc = pOwner->EyePosition();

	QAngle angles = pOwner->GetLocalAngles();

	AngleVectors(angles, &vecAiming);

	trace_t tr;

	// USANDO CVAR PARA ATTACHMENT RANGE
	Vector	vecEnd = vecSrc + (vecAiming * sv_slam_attachment_range.GetFloat());
	UTIL_TraceLine(vecSrc, vecEnd, MASK_SOLID, pOwner, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0)
	{
		// Don't attach to a living creature
		if (tr.m_pEnt)
		{
			CBaseEntity* pEntity = tr.m_pEnt;
			CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pEntity);
			if (pBCC)
			{
				return false;
			}

			CBaseGrenade* pGrenade = dynamic_cast<CBaseGrenade*>(pEntity);
			if (pGrenade)
			{
				return false;
			}
		}

		if (pTrace != NULL)
		{
			*pTrace = tr;
		}

		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override so SLAM to so secondary attack when no secondary ammo
//			but satchel is in the world
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::ItemPostFrame(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
	{
		return;
	}

	// Se estiver carregando, processa o carregamento
	if (m_bIsCharging)
	{
		float flChargeTime = gpGlobals->curtime - m_flChargeStartTime;
		float flChargePercent = clamp(flChargeTime / sv_slam_charge_time.GetFloat(), 0.0f, 1.0f);

		// USANDO CVAR PARA HUD UPDATE RATE
		if (sv_slam_charge_hud.GetBool() && (gpGlobals->curtime - m_flLastHUDUpdateTime) > sv_slam_hud_update_rate.GetFloat())
		{
			char hudText[256];
			int barLength = 20;
			int filled = (int)(flChargePercent * barLength);
			char bar[22];
			for (int i = 0; i < barLength; i++)
			{
				bar[i] = (i < filled) ? '|' : ' ';
			}
			bar[barLength] = '\0';
			Q_snprintf(hudText, sizeof(hudText), "Charging SLAM [%s] %d%%", bar, (int)(flChargePercent * 100));
			ClientPrint(pOwner, HUD_PRINTCENTER, hudText);
			m_flLastHUDUpdateTime = gpGlobals->curtime;
		}

		// Se o jogador soltou o botão, lança a granada
		if (!(pOwner->m_nButtons & IN_ATTACK))
		{
			float flVelocity = Lerp(flChargePercent, sv_slam_throw_velocity_min.GetFloat(), sv_slam_throw_velocity_max.GetFloat());
			SatchelThrow(flVelocity);
			m_bIsCharging = false;
			ClientPrint(pOwner, HUD_PRINTCENTER, "\n"); // Limpa o HUD
		}
	}
	else
	{
		// Lógica original quando não está carregando
		SLAMThink();

		if (AnyUndetonatedCharges())
			m_bDetonatorArmed = true;
		else
			m_bDetonatorArmed = false;

		if ((pOwner->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime))
		{
			SecondaryAttack();
		}
		else if (!m_bNeedReload && (pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
		{
			PrimaryAttack();
		}
		else
		{
			WeaponIdle();
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Switch to next best weapon
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::Weapon_Switch(void)
{
	// Note that we may pick the SLAM again, when we switch
	// weapons, in which case we have to save and restore the 
	// detonator armed state.
	// The SLAMs may be about to blow up, but haven't done so yet
	// and the deploy function will find the undetonated charges
	// and we are armed
	bool saveState = m_bDetonatorArmed;
	CBaseCombatCharacter* pOwner = GetOwner();
	pOwner->SwitchToNextBestWeapon(pOwner->GetActiveWeapon());
	if (pOwner->GetActiveWeapon() == this)
	{
		m_bDetonatorArmed = saveState;
	}

#ifndef CLIENT_DLL
	// If not armed and have no ammo
	if (!m_bDetonatorArmed && pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0)
	{
		pOwner->ClearActiveWeapon();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeapon_SLAM::WeaponIdle(void)
{
	// Ready to switch animations?
	if (HasWeaponIdleTimeElapsed())
	{
		// Don't allow throw to attach switch unless in idle
		if (m_bClearReload)
		{
			m_bNeedReload = false;
			m_bClearReload = false;
		}
		CBaseCombatCharacter* pOwner = GetOwner();
		if (!pOwner)
		{
			return;
		}

		int iAnim = 0;

		if (m_bThrowSatchel)
		{
			// Sempre usa velocidade padrão aqui, SatchelThrow vai decidir internamente
			SatchelThrow(sv_slam_throw_velocity_default.GetFloat());

			// Se não tem mais munição após o throw, força transição para detonador
			if (pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0 && m_bDetonatorArmed)
			{
				iAnim = ACT_SLAM_DETONATOR_IDLE;
			}
			else if (m_bDetonatorArmed && !m_bNeedDetonatorDraw)
			{
				iAnim = ACT_SLAM_THROW_THROW2;
			}
			else
			{
				iAnim = ACT_SLAM_THROW_THROW_ND2;
			}
		}
		else if (m_bAttachSatchel)
		{
			SatchelAttach();
			if (m_bDetonatorArmed && !m_bNeedDetonatorDraw)
			{
				iAnim = ACT_SLAM_STICKWALL_ATTACH2;
			}
			else
			{
				iAnim = ACT_SLAM_STICKWALL_ND_ATTACH2;
			}
		}
		else if (m_bAttachTripmine)
		{
			TripmineAttach();
			iAnim = m_bNeedDetonatorDraw ? ACT_SLAM_STICKWALL_ATTACH2 : ACT_SLAM_TRIPMINE_ATTACH2;
		}
		else if (m_bNeedReload)
		{
			// Se o dono tiver munição, faz a animação de recarregar
			if (pOwner->GetAmmoCount(m_iSecondaryAmmoType) > 0)
			{
				switch (m_tSlamState)
				{
				case SLAM_TRIPMINE_READY:
				{
					iAnim = m_bNeedDetonatorDraw ? ACT_SLAM_STICKWALL_DRAW : ACT_SLAM_TRIPMINE_DRAW;
				}
				break;
				case SLAM_SATCHEL_ATTACH:
				{
					if (m_bNeedDetonatorHolster)
					{
						iAnim = ACT_SLAM_STICKWALL_DETONATOR_HOLSTER;
						m_bNeedDetonatorHolster = false;
					}
					else if (m_bDetonatorArmed)
					{
						iAnim = m_bNeedDetonatorDraw ? ACT_SLAM_DETONATOR_STICKWALL_DRAW : ACT_SLAM_STICKWALL_DRAW;
						m_bNeedDetonatorDraw = false;
					}
					else
					{
						iAnim = ACT_SLAM_STICKWALL_ND_DRAW;
					}
				}
				break;
				case SLAM_SATCHEL_THROW:
				{
					if (m_bNeedDetonatorHolster)
					{
						iAnim = ACT_SLAM_THROW_DETONATOR_HOLSTER;
						m_bNeedDetonatorHolster = false;
					}
					else if (m_bDetonatorArmed)
					{
						iAnim = m_bNeedDetonatorDraw ? ACT_SLAM_DETONATOR_THROW_DRAW : ACT_SLAM_THROW_DRAW;
						m_bNeedDetonatorDraw = false;
					}
					else
					{
						iAnim = ACT_SLAM_THROW_ND_DRAW;
					}
				}
				break;
				}
				m_bClearReload = true;
			}
			// Se não tiver munição e o detonador estiver armado, faz a animação de idle
			else if (m_bDetonatorArmed)
			{
				iAnim = m_bNeedDetonatorDraw ? ACT_SLAM_DETONATOR_DRAW : ACT_SLAM_DETONATOR_IDLE;
				m_bNeedDetonatorDraw = false;
			}
			else
			{
#ifndef CLIENT_DLL
				pOwner->Weapon_Drop(this);
				UTIL_Remove(this);
#endif
			}
		}
		// Se não tiver munição e não estiver armado, dropa a arma
		else if (pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0 && !m_bDetonatorArmed)
		{
#ifndef CLIENT_DLL
			pOwner->Weapon_Drop(this);
			UTIL_Remove(this);
#endif
		}
		// Se não precisar recarregar, apenas faz o idle
		else
		{
			switch (m_tSlamState)
			{
			case SLAM_TRIPMINE_READY:
			{
				iAnim = m_bDetonatorArmed ? ACT_SLAM_STICKWALL_IDLE : ACT_SLAM_TRIPMINE_IDLE;
				m_flWallSwitchTime = 0;
			}
			break;
			case SLAM_SATCHEL_THROW:
			{
				if (m_bNeedDetonatorHolster)
				{
					iAnim = ACT_SLAM_THROW_DETONATOR_HOLSTER;
					m_bNeedDetonatorHolster = false;
				}
				else
				{
					iAnim = m_bDetonatorArmed ? ACT_SLAM_THROW_IDLE : ACT_SLAM_THROW_ND_IDLE;
					m_flWallSwitchTime = 0;
				}
			}
			break;
			case SLAM_SATCHEL_ATTACH:
			{
				if (m_bNeedDetonatorHolster)
				{
					iAnim = ACT_SLAM_STICKWALL_DETONATOR_HOLSTER;
					m_bNeedDetonatorHolster = false;
				}
				else
				{
					iAnim = m_bDetonatorArmed ? ACT_SLAM_STICKWALL_IDLE : ACT_SLAM_TRIPMINE_IDLE;
					m_flWallSwitchTime = 0;
				}
			}
			break;
			}
		}
		SendWeaponAnim(iAnim);
	}
}

bool CWeapon_SLAM::Deploy(void)
{
	m_bThrowSatchel = false;
	m_bAttachTripmine = false;

	CBaseCombatCharacter* pOwner = GetOwner();
	if (!pOwner)
	{
		return false;
	}

	m_bDetonatorArmed = AnyUndetonatedCharges();

	SetModel(GetViewModel());

	m_tSlamState = (int)SLAM_SATCHEL_THROW;

	// ------------------------------
	// Pick the right draw animation
	// ------------------------------
	int iActivity;

	// If detonator is already armed
	m_bNeedReload = false;
	if (m_bDetonatorArmed)
	{
		if (pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0)
		{
			iActivity = ACT_SLAM_DETONATOR_DRAW;
			m_bNeedReload = true;
		}
		else if (CanAttachSLAM())
		{
			iActivity = ACT_SLAM_DETONATOR_STICKWALL_DRAW;
			SetSlamState(SLAM_TRIPMINE_READY);
		}
		else
		{
			iActivity = ACT_SLAM_DETONATOR_THROW_DRAW;
			SetSlamState(SLAM_SATCHEL_THROW);
		}
	}
	else
	{
		if (CanAttachSLAM())
		{
			iActivity = ACT_SLAM_TRIPMINE_DRAW;
			SetSlamState(SLAM_TRIPMINE_READY);
		}
		else
		{
			iActivity = ACT_SLAM_THROW_ND_DRAW;
			SetSlamState(SLAM_SATCHEL_THROW);
		}
	}

	return DefaultDeploy((char*)GetViewModel(), (char*)GetWorldModel(), iActivity, (char*)GetAnimPrefix());
}

bool CWeapon_SLAM::HasAnyAmmo(void)
{
	if (AnyUndetonatedCharges() || m_iClip2 > 0)
		return true;
	else
		return false;

	return BaseClass::HasAnyAmmo();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CWeapon_SLAM::CWeapon_SLAM(void)
{
	m_tSlamState = (int)SLAM_SATCHEL_THROW;
	m_bDetonatorArmed = false;
	m_bNeedReload = true;
	m_bClearReload = false;
	m_bThrowSatchel = false;
	m_bAttachSatchel = false;
	m_bAttachTripmine = false;
	m_bNeedDetonatorDraw = false;
	m_bNeedDetonatorHolster = false;

	// INICIALIZAR NOVAS VARIÁVEIS
	m_flChargeStartTime = 0.0f;
	m_flLastHUDUpdateTime = 0.0f;
	m_bIsCharging = false;
}