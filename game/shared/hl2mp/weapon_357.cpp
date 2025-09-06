//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#else
#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeapon357 C_Weapon357
#endif

// ==================================================================================================
// CVARS FOR 357 REVOLVER CUSTOMIZATION
// ==================================================================================================
ConVar sv_357_zoom_enable("sv_357_zoom_enable", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable zoom for 357 revolver (0=disabled, 1=enabled)");
ConVar sv_357_zoom_level("sv_357_zoom_level", "2.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Zoom level for 357 revolver (default: 2.0)");

ConVar sv_357_recoil_punch_vertical("sv_357_recoil_punch_vertical", "8.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Vertical punch recoil for 357 revolver (default: 8.0)");
ConVar sv_357_recoil_punch_horizontal("sv_357_recoil_punch_horizontal", "2.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Horizontal punch recoil for 357 revolver (default: 2.0)");

ConVar sv_357_recoil_view_vertical("sv_357_recoil_view_vertical", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Vertical view angle recoil for 357 revolver (default: 1.0)");
ConVar sv_357_recoil_view_horizontal("sv_357_recoil_view_horizontal", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Horizontal view angle recoil for 357 revolver (default: 1.0)");

ConVar sv_357_recoil_enable_viewpunch("sv_357_recoil_enable_viewpunch", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable view punch recoil for 357 revolver (0=disabled, 1=enabled)");
ConVar sv_357_recoil_enable_viewangle("sv_357_recoil_enable_viewangle", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable view angle recoil for 357 revolver (0=disabled, 1=enabled)");

ConVar sv_357_firerate_delay("sv_357_firerate_delay", "0.75", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay between shots for 357 revolver (default: 0.75 seconds)");

//-----------------------------------------------------------------------------
// CWeapon357
//-----------------------------------------------------------------------------

class CWeapon357 : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS(CWeapon357, CBaseHL2MPCombatWeapon);

	CWeapon357(void);

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	void	ItemPostFrame(void);
	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
	bool	Reload(void);
	virtual bool Holster(CBaseCombatWeapon* pSwitchingTo);

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

private:
	bool m_bZoomed;
	bool m_bLastAttack2State;

	CWeapon357(const CWeapon357&);
};

IMPLEMENT_NETWORKCLASS_ALIASED(Weapon357, DT_Weapon357)

BEGIN_NETWORK_TABLE(CWeapon357, DT_Weapon357)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA(CWeapon357)
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_357, CWeapon357);
PRECACHE_WEAPON_REGISTER(weapon_357);

#ifndef CLIENT_DLL
acttable_t CWeapon357::m_acttable[] =
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

IMPLEMENT_ACTTABLE(CWeapon357);
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon357::CWeapon357(void)
{
	m_bReloadsSingly = false;
	m_bFiresUnderwater = false;
	m_bZoomed = false;
	m_bLastAttack2State = false;
}

//-----------------------------------------------------------------------------
// Purpose: Zoom toggle igual pistola
//-----------------------------------------------------------------------------
void CWeapon357::ItemPostFrame(void)
{
	BaseClass::ItemPostFrame();

	if (m_bInReload)
		return;

	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
		return;

	bool bAttack2 = (pPlayer->m_nButtons & IN_ATTACK2) != 0;
	if (sv_357_zoom_enable.GetBool())
	{
		if (bAttack2 && !m_bLastAttack2State)
		{
			if (!m_bZoomed)
			{
				int zoomFOV = sv_357_zoom_level.GetInt();
				pPlayer->SetFOV(this, zoomFOV, 0.2f);
				m_bZoomed = true;
			}
			else
			{
				pPlayer->SetFOV(this, 0, 0.2f);
				m_bZoomed = false;
			}
		}
		m_bLastAttack2State = bAttack2;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Atira
//-----------------------------------------------------------------------------
void CWeapon357::PrimaryAttack(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
		return;

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.15;
		}
		return;
	}

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	// Fire rate delay (cvar)
	float fireRate = sv_357_firerate_delay.GetFloat();
	m_flNextPrimaryAttack = gpGlobals->curtime + fireRate;
	m_flNextSecondaryAttack = gpGlobals->curtime + fireRate;

	m_iClip1--;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	FireBulletsInfo_t info(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType);
	info.m_pAttacker = pPlayer;

	pPlayer->FireBullets(info);

	// Recoil (cvars)
	float punchVertical = sv_357_recoil_punch_vertical.GetFloat();
	float punchHorizontal = sv_357_recoil_punch_horizontal.GetFloat();
	float viewVertical = sv_357_recoil_view_vertical.GetFloat();
	float viewHorizontal = sv_357_recoil_view_horizontal.GetFloat();

	bool enableViewPunch = sv_357_recoil_enable_viewpunch.GetBool();
	bool enableViewAngle = sv_357_recoil_enable_viewangle.GetBool();

	QAngle angles = pPlayer->GetLocalAngles();

	if (enableViewAngle)
	{
		angles.x += random->RandomFloat(-viewVertical, viewVertical);
		angles.y += random->RandomFloat(-viewHorizontal, viewHorizontal);
		angles.z = 0;
		pPlayer->SetLocalAngles(angles);
	}

	if (enableViewPunch)
	{
		pPlayer->ViewPunch(QAngle(-punchVertical, random->RandomFloat(-punchHorizontal, punchHorizontal), 0));
	}

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: SecondaryAttack vazio, zoom controlado por ItemPostFrame
//-----------------------------------------------------------------------------
void CWeapon357::SecondaryAttack(void)
{
	// Zoom control is handled in ItemPostFrame
}

//-----------------------------------------------------------------------------
// Purpose: Remove zoom ao recarregar - SÓ SE ESTIVER EM ZOOM
//-----------------------------------------------------------------------------
bool CWeapon357::Reload(void)
{
	bool fRet = BaseClass::Reload();
	if (fRet)
	{
		// SÓ RESETA ZOOM SE REALMENTE ESTIVER EM ZOOM
		if (m_bZoomed)
		{
			CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
			if (pPlayer)
			{
				pPlayer->SetFOV(this, 0, 0.2f);
				m_bZoomed = false;
			}
		}
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: Remove zoom ao guardar arma
//-----------------------------------------------------------------------------
bool CWeapon357::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer && m_bZoomed)
	{
		pPlayer->SetFOV(this, 0, 0.2f);
		m_bZoomed = false;
	}
	m_bLastAttack2State = false; // Evita flicker do zoom no próximo uso
	return CBaseHL2MPCombatWeapon::Holster(pSwitchingTo);
}
