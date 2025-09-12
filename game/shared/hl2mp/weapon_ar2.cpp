//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include "c_te_effect_dispatch.h"
#else
#include "hl2mp_player.h"
#include "te_effect_dispatch.h"
#include "prop_combine_ball.h"
#endif

#include "weapon_ar2.h"
#include "effect_dispatch_data.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//- V V V - CONVARS - V V V -
// Primary Fire
ConVar sv_ar2_kick_dampen("sv_ar2_kick_dampen", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Dampening factor for AR2 view kick.");
ConVar sv_ar2_kick_max_vertical("sv_ar2_kick_max_vertical", "8.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum vertical kick in degrees for AR2.");
ConVar sv_ar2_kick_slide_limit("sv_ar2_kick_slide_limit", "5.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Slide limit for AR2 view kick in seconds.");

// Secondary Fire (Combine Ball)
ConVar sv_ar2_alt_fire_radius("sv_ar2_alt_fire_radius", "10", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Radius of the AR2's alt-fire combine ball.");
ConVar sv_ar2_alt_fire_duration("sv_ar2_alt_fire_duration", "4", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Duration of the AR2's alt-fire combine ball.");
ConVar sv_ar2_alt_fire_mass("sv_ar2_alt_fire_mass", "150", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Mass of the AR2's alt-fire combine ball.");
ConVar sv_ar2_alt_fire_velocity("sv_ar2_alt_fire_velocity", "1000", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Initial velocity of the AR2's alt-fire combine ball.");
ConVar sv_ar2_alt_fire_charge_time("sv_ar2_alt_fire_charge_time", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Charge-up time for the AR2's alt-fire.");
ConVar sv_ar2_alt_fire_cost("sv_ar2_alt_fire_cost", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Ammo cost for the AR2's alt-fire.");

// Secondary Fire Cooldowns
ConVar sv_ar2_alt_fire_next_primary_delay("sv_ar2_alt_fire_next_primary_delay", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay before primary fire is available after alt-fire.");
ConVar sv_ar2_alt_fire_next_secondary_delay("sv_ar2_alt_fire_next_secondary_delay", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay before another alt-fire can be launched.");
ConVar sv_ar2_alt_fire_dryfire_delay("sv_ar2_alt_fire_dryfire_delay", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay for alt-fire dryfire sound.");

// Secondary Fire View Punch
ConVar sv_ar2_alt_punch_pitch_min("sv_ar2_alt_punch_pitch_min", "-12", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum vertical view punch for alt-fire.");
ConVar sv_ar2_alt_punch_pitch_max("sv_ar2_alt_punch_pitch_max", "-8", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum vertical view punch for alt-fire.");
ConVar sv_ar2_alt_punch_yaw_min("sv_ar2_alt_punch_yaw_min", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum horizontal view punch for alt-fire.");
ConVar sv_ar2_alt_punch_yaw_max("sv_ar2_alt_punch_yaw_max", "2", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum horizontal view punch for alt-fire.");

// NPC Ranges
ConVar sv_ar2_npc_min_range1("sv_ar2_npc_min_range1", "65", FCVAR_GAMEDLL | FCVAR_NOTIFY, "NPC minimum primary fire range.");
ConVar sv_ar2_npc_max_range1("sv_ar2_npc_max_range1", "2048", FCVAR_GAMEDLL | FCVAR_NOTIFY, "NPC maximum primary fire range.");
ConVar sv_ar2_npc_min_range2("sv_ar2_npc_min_range2", "256", FCVAR_GAMEDLL | FCVAR_NOTIFY, "NPC minimum secondary fire range.");
ConVar sv_ar2_npc_max_range2("sv_ar2_npc_max_range2", "1024", FCVAR_GAMEDLL | FCVAR_NOTIFY, "NPC maximum secondary fire range.");
//- ^ ^ ^ - CONVARS - ^ ^ ^ -


//=========================================================
//=========================================================


IMPLEMENT_NETWORKCLASS_ALIASED(WeaponAR2, DT_WeaponAR2)

BEGIN_NETWORK_TABLE(CWeaponAR2, DT_WeaponAR2)
#ifdef GAME_DLL
SendPropBool(SENDINFO(m_bShotDelayed)),
SendPropFloat(SENDINFO(m_flDelayedFire)),
#else
RecvPropBool(RECVINFO(m_bShotDelayed)),
RecvPropFloat(RECVINFO(m_flDelayedFire)),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA(CWeaponAR2)
#ifdef CLIENT_DLL
// misyl: Pred + network this state so it behaves nicely on the client. :D
DEFINE_PRED_FIELD(m_flDelayedFire, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bShotDelayed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_ar2, CWeaponAR2);
PRECACHE_WEAPON_REGISTER(weapon_ar2);


#ifndef CLIENT_DLL

acttable_t	CWeaponAR2::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_AR2,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_AR2,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_AR2,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_AR2,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_AR2,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_AR2,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_AR2,				false },
};

IMPLEMENT_ACTTABLE(CWeaponAR2);

#endif

CWeaponAR2::CWeaponAR2()
{
	m_fMinRange1 = sv_ar2_npc_min_range1.GetFloat();
	m_fMaxRange1 = sv_ar2_npc_max_range1.GetFloat();

	m_fMinRange2 = sv_ar2_npc_min_range2.GetFloat();
	m_fMaxRange2 = sv_ar2_npc_max_range2.GetFloat();

	m_nShotsFired = 0;
	m_nVentPose = -1;

	m_flDelayedFire = 0.0f;
	m_bShotDelayed = false;
}

void CWeaponAR2::Precache(void)
{
	BaseClass::Precache();

#ifndef CLIENT_DLL

	UTIL_PrecacheOther("prop_combine_ball");
	UTIL_PrecacheOther("env_entity_dissolver");
#endif

}

//-----------------------------------------------------------------------------
// Purpose: Handle grenade detonate in-air (even when no ammo is left)
//-----------------------------------------------------------------------------
void CWeaponAR2::ItemPostFrame(void)
{
	// See if we need to fire off our secondary round
	if (m_bShotDelayed && gpGlobals->curtime > m_flDelayedFire)
	{
		DelayedAttack();
	}

	// Update our pose parameter for the vents
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner)
	{
		CBaseViewModel* pVM = pOwner->GetViewModel();

		if (pVM)
		{
			if (m_nVentPose == -1)
			{
				m_nVentPose = pVM->LookupPoseParameter("VentPoses");
			}

			float flVentPose = RemapValClamped(m_nShotsFired, 0, 5, 0.0f, 1.0f);
			pVM->SetPoseParameter(m_nVentPose, flVentPose);
		}
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CWeaponAR2::GetPrimaryAttackActivity(void)
{
	if (m_nShotsFired < 2)
		return ACT_VM_PRIMARYATTACK;

	if (m_nShotsFired < 3)
		return ACT_VM_RECOIL1;

	if (m_nShotsFired < 4)
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &tr - 
//			nDamageType - 
//-----------------------------------------------------------------------------
void CWeaponAR2::DoImpactEffect(trace_t& tr, int nDamageType)
{
	CEffectData data;

	data.m_vOrigin = tr.endpos + (tr.plane.normal * 1.0f);
	data.m_vNormal = tr.plane.normal;

	DispatchEffect("AR2Impact", data);

	BaseClass::DoImpactEffect(tr, nDamageType);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2::DelayedAttack(void)
{
	m_bShotDelayed = false;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	// Deplete the clip completely
	SendWeaponAnim(ACT_VM_SECONDARYATTACK);
	m_flNextSecondaryAttack = pOwner->m_flNextAttack = gpGlobals->curtime + SequenceDuration();

	// Register a muzzleflash for the AI
	pOwner->DoMuzzleFlash();

	WeaponSound(WPN_DOUBLE);

	// Fire the bullets
	Vector vecSrc = pOwner->Weapon_ShootPosition();
	Vector vecAiming = pOwner->GetAutoaimVector(AUTOAIM_2DEGREES);
	Vector impactPoint = vecSrc + (vecAiming * MAX_TRACE_LENGTH);

	// Fire the bullets
	Vector vecVelocity = vecAiming * sv_ar2_alt_fire_velocity.GetFloat();

#ifndef CLIENT_DLL
	// Fire the combine ball
	CreateCombineBall(vecSrc,
		vecVelocity,
		sv_ar2_alt_fire_radius.GetFloat(),
		sv_ar2_alt_fire_mass.GetFloat(),
		sv_ar2_alt_fire_duration.GetFloat(),
		pOwner);

	// View effects
	color32 white = { 255, 255, 255, 64 };
	UTIL_ScreenFade(pOwner, white, 0.1, 0, FFADE_IN);
#endif

	//Disorient the player
	QAngle angles = pOwner->GetLocalAngles();

	angles.x += random->RandomInt(-4, 4);
	angles.y += random->RandomInt(-4, 4);
	angles.z = 0;

	//	pOwner->SnapEyeAngles( angles );

	pOwner->ViewPunch(QAngle(SharedRandomInt("ar2pax", sv_ar2_alt_punch_pitch_max.GetInt(), sv_ar2_alt_punch_pitch_min.GetInt()), SharedRandomInt("ar2pay", sv_ar2_alt_punch_yaw_min.GetInt(), sv_ar2_alt_punch_yaw_max.GetInt()), 0));

	// Decrease ammo
	pOwner->RemoveAmmo(sv_ar2_alt_fire_cost.GetInt(), m_iSecondaryAmmoType);

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + sv_ar2_alt_fire_next_primary_delay.GetFloat();

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextEmptySoundTime = m_flNextSecondaryAttack = gpGlobals->curtime + sv_ar2_alt_fire_next_secondary_delay.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2::SecondaryAttack(void)
{
	if (m_bShotDelayed)
		return;

	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	// Cannot fire underwater
	if ((pPlayer->GetAmmoCount(m_iSecondaryAmmoType) <= 0) || (pPlayer->GetWaterLevel() == 3))
	{
		SendWeaponAnim(ACT_VM_DRYFIRE);
		BaseClass::WeaponSound(EMPTY);
		m_flNextEmptySoundTime = m_flNextSecondaryAttack = gpGlobals->curtime + sv_ar2_alt_fire_dryfire_delay.GetFloat();
		return;
	}

	m_bShotDelayed = true;
	m_flNextEmptySoundTime = m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flDelayedFire = gpGlobals->curtime + sv_ar2_alt_fire_charge_time.GetFloat();

	SendWeaponAnim(ACT_VM_FIDGET);
	WeaponSound(SPECIAL1);
}

//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponAR2::CanHolster(void)
{
	if (m_bShotDelayed)
		return false;

	return BaseClass::CanHolster();
}


bool CWeaponAR2::Deploy(void)
{
	m_bShotDelayed = false;
	m_flDelayedFire = 0.0f;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
//-----------------------------------------------------------------------------
bool CWeaponAR2::Reload(void)
{
	if (m_bShotDelayed)
		return false;

	return BaseClass::Reload();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2::AddViewKick(void)
{
	//Get the view kick
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
		return;

	DoMachineGunKick(pPlayer, sv_ar2_kick_dampen.GetFloat(), sv_ar2_kick_max_vertical.GetFloat(), m_fFireDuration, sv_ar2_kick_slide_limit.GetFloat());
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t* CWeaponAR2::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0,		0.75	},
		{ 5.00,		0.75	},
		{ 3.0,		0.85	},
		{ 5.0 / 3.0,	0.75	},
		{ 1.00,		1.0		},
	};

	COMPILE_TIME_ASSERT(ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}
