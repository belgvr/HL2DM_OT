//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#else
#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

//- V V V - CONVARS - V V V -
ConVar sv_pistol_refire_time("sv_pistol_refire_time", "0.1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum time between pistol shots.");
ConVar sv_pistol_dry_refire_time("sv_pistol_dry_refire_time", "0.2", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum time between empty pistol clicks.");

ConVar sv_pistol_spread_min("sv_pistol_spread_min", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum bullet spread cone in degrees (perfect accuracy).");
ConVar sv_pistol_spread_max("sv_pistol_spread_max", "6.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum bullet spread cone in degrees (full penalty).");

ConVar sv_pistol_viewkick_pitch_min("sv_pistol_viewkick_pitch_min", "0.25", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum vertical view kick (pitch).");
ConVar sv_pistol_viewkick_pitch_max("sv_pistol_viewkick_pitch_max", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum vertical view kick (pitch).");
ConVar sv_pistol_viewkick_yaw("sv_pistol_viewkick_yaw", "0.6", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Magnitude of random horizontal view kick (yaw).");
ConVar sv_pistol_viewkick_roll("sv_pistol_viewkick_roll", "0.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Magnitude of random view roll. (Default: 0)");

ConVar sv_pistol_acc_penalty_initial("sv_pistol_acc_penalty_initial", "0.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Initial accuracy penalty. Set to 0 for perfect accuracy on first shot.");
//- ^ ^ ^ - CONVARS - ^ ^ ^ -

#define	PISTOL_ACCURACY_SHOT_PENALTY_TIME		0.2f	// Applied amount of time each shot adds to the time we must recover from
#define	PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME	1.5f	// Maximum penalty to deal out

#ifdef CLIENT_DLL
#define CWeaponPistol C_WeaponPistol
#endif

//-----------------------------------------------------------------------------
// CWeaponPistol
//-----------------------------------------------------------------------------

class CWeaponPistol : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS(CWeaponPistol, CBaseHL2MPCombatWeapon);

	CWeaponPistol(void);

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	void	Precache(void);
	void	ItemPostFrame(void);
	void	ItemPreFrame(void);
	void	ItemBusyFrame(void);
	void	PrimaryAttack(void);
	void	AddViewKick(void);
	void	DryFire(void);

	void	UpdatePenaltyTime(void);

	Activity	GetPrimaryAttackActivity(void);

	virtual bool Reload(void);

	virtual const Vector& GetBulletSpread(void)
	{
		static Vector cone;

		// Calculate spread vectors from ConVar degrees
		float min_spread = tan(DEG2RAD(sv_pistol_spread_min.GetFloat() / 2.0f));
		Vector minCone(min_spread, min_spread, min_spread);

		float max_spread = tan(DEG2RAD(sv_pistol_spread_max.GetFloat() / 2.0f));
		Vector maxCone(max_spread, max_spread, max_spread);

		float ramp = RemapValClamped(m_flAccuracyPenalty,
			0.0f,
			PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME,
			0.0f,
			1.0f);

		// We lerp from very accurate to inaccurate over time
		VectorLerp(minCone, maxCone, ramp, cone);

		return cone;
	}

	virtual int	GetMinBurst()
	{
		return 1;
	}

	virtual int	GetMaxBurst()
	{
		return 3;
	}

	virtual float GetFireRate(void)
	{
		return 0.5f;
	}

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

private:
	CNetworkVar(float, m_flSoonestPrimaryAttack);
	CNetworkVar(float, m_flLastAttackTime);
	CNetworkVar(float, m_flAccuracyPenalty);
	CNetworkVar(int, m_nNumShotsFired);

private:
	CWeaponPistol(const CWeaponPistol&);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponPistol, DT_WeaponPistol)

BEGIN_NETWORK_TABLE(CWeaponPistol, DT_WeaponPistol)
#ifdef CLIENT_DLL
RecvPropTime(RECVINFO(m_flSoonestPrimaryAttack)),
RecvPropTime(RECVINFO(m_flLastAttackTime)),
RecvPropFloat(RECVINFO(m_flAccuracyPenalty)),
RecvPropInt(RECVINFO(m_nNumShotsFired)),
#else
SendPropTime(SENDINFO(m_flSoonestPrimaryAttack)),
SendPropTime(SENDINFO(m_flLastAttackTime)),
SendPropFloat(SENDINFO(m_flAccuracyPenalty)),
SendPropInt(SENDINFO(m_nNumShotsFired)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponPistol)
DEFINE_PRED_FIELD(m_flSoonestPrimaryAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_flLastAttackTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_flAccuracyPenalty, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_nNumShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_pistol, CWeaponPistol);
PRECACHE_WEAPON_REGISTER(weapon_pistol);

#ifndef CLIENT_DLL
acttable_t CWeaponPistol::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_PISTOL,				false },
};


IMPLEMENT_ACTTABLE(CWeaponPistol);

#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponPistol::CWeaponPistol(void)
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flAccuracyPenalty = sv_pistol_acc_penalty_initial.GetFloat();

	m_fMinRange1 = 24;
	m_fMaxRange1 = 1500;
	m_fMinRange2 = 24;
	m_fMaxRange2 = 200;

	m_bFiresUnderwater = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::Precache(void)
{
	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponPistol::DryFire(void)
{
	WeaponSound(EMPTY);
	SendWeaponAnim(ACT_VM_DRYFIRE);

	m_flSoonestPrimaryAttack = gpGlobals->curtime + sv_pistol_dry_refire_time.GetFloat();
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponPistol::PrimaryAttack(void)
{
	if ((gpGlobals->curtime - m_flLastAttackTime) > 0.5f)
	{
		m_nNumShotsFired = 0;
	}
	else
	{
		m_nNumShotsFired++;
	}

	m_flLastAttackTime = gpGlobals->curtime;
	m_flSoonestPrimaryAttack = gpGlobals->curtime + sv_pistol_refire_time.GetFloat();

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner)
	{
		// Each time the player fires the pistol, reset the view punch. This prevents
		// the aim from 'drifting off' when the player fires very quickly. This may
		// not be the ideal way to achieve this, but it's cheap and it works, which is
		// great for a feature we're evaluating. (sjb)
		pOwner->ViewPunchReset();
	}

	BaseClass::PrimaryAttack();

	// Add an accuracy penalty which can move past our maximum penalty time if we're really spastic
	m_flAccuracyPenalty += PISTOL_ACCURACY_SHOT_PENALTY_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::UpdatePenaltyTime(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	// Check our penalty time decay
	if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_flSoonestPrimaryAttack < gpGlobals->curtime))
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp(m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::ItemPreFrame(void)
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::ItemBusyFrame(void)
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponPistol::ItemPostFrame(void)
{
	BaseClass::ItemPostFrame();

	if (m_bInReload)
		return;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	if (pOwner->m_nButtons & IN_ATTACK2)
	{
		m_flLastAttackTime = gpGlobals->curtime + sv_pistol_refire_time.GetFloat();
		m_flSoonestPrimaryAttack = gpGlobals->curtime + sv_pistol_refire_time.GetFloat();
		m_flNextPrimaryAttack = gpGlobals->curtime + sv_pistol_refire_time.GetFloat();
	}

	//Allow a refire as fast as the player can click
	if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_flSoonestPrimaryAttack < gpGlobals->curtime))
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
	else if ((pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack < gpGlobals->curtime) && (m_iClip1 <= 0))
	{
		DryFire();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeaponPistol::GetPrimaryAttackActivity(void)
{
	if (m_nNumShotsFired < 1)
		return ACT_VM_PRIMARYATTACK;

	if (m_nNumShotsFired < 2)
		return ACT_VM_RECOIL1;

	if (m_nNumShotsFired < 3)
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponPistol::Reload(void)
{
	bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
	if (fRet)
	{
		WeaponSound(RELOAD);
		m_flAccuracyPenalty = sv_pistol_acc_penalty_initial.GetFloat();
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::AddViewKick(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	QAngle	viewPunch;

	float flYaw = sv_pistol_viewkick_yaw.GetFloat();
	float flRoll = sv_pistol_viewkick_roll.GetFloat();

	viewPunch.x = SharedRandomFloat("pistolpax", sv_pistol_viewkick_pitch_min.GetFloat(), sv_pistol_viewkick_pitch_max.GetFloat());
	viewPunch.y = SharedRandomFloat("pistolpay", -flYaw, flYaw);
	viewPunch.z = SharedRandomFloat("pistolpaz", -flRoll, flRoll);

	//Add it to the view punch
	pPlayer->ViewPunch(viewPunch);
}
