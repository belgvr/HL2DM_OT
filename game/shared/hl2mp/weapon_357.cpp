//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Magnum .357 configuravel via ConVar com zoom
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include <prediction.h>
#else
#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeapon357 C_Weapon357
#endif

//-----------------------------------------------------------------------------
// ConVars para configurar a 357
//-----------------------------------------------------------------------------
ConVar sv_357_firerate_delay(
	"sv_357_firerate_delay",
	"0.75",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"Delay between primary .357 shots (in seconds)"
);

ConVar sv_357_zoom_enable(
	"sv_357_zoom_enable",
	"0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"Enable zoom functionality for .357 Magnum (0=disabled, 1=enabled)"
);

ConVar sv_357_zoom_level(
	"sv_357_zoom_level",
	"30",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"FOV level when zoomed with .357 Magnum (lower = more zoom)"
);

ConVar sv_357_recoil_punch_vertical(
	"sv_357_recoil_punch_vertical",
	"8.0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"Vertical recoil punch intensity for .357 Magnum"
);

ConVar sv_357_recoil_punch_horizontal(
	"sv_357_recoil_punch_horizontal",
	"2.0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"Horizontal recoil punch spread for .357 Magnum"
);

ConVar sv_357_recoil_view_vertical(
	"sv_357_recoil_view_vertical",
	"1.0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"Vertical view angle recoil intensity for .357 Magnum"
);

ConVar sv_357_recoil_view_horizontal(
	"sv_357_recoil_view_horizontal",
	"1.0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"Horizontal view angle recoil intensity for .357 Magnum"
);

ConVar sv_357_recoil_enable_viewpunch(
	"sv_357_recoil_enable_viewpunch",
	"1",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"Enable viewpunch recoil for .357 Magnum (0=disabled, 1=enabled)"
);

ConVar sv_357_recoil_enable_viewangle(
	"sv_357_recoil_enable_viewangle",
	"1",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"Enable view angle recoil for .357 Magnum (0=disabled, 1=enabled)"
);

//-----------------------------------------------------------------------------
// CWeapon357
//-----------------------------------------------------------------------------
class CWeapon357 : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS(CWeapon357, CBaseHL2MPCombatWeapon);
public:

	CWeapon357(void);

	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
	void	ItemPostFrame(void);
	bool	Holster(CBaseCombatWeapon* pSwitchingTo = NULL);
	void	Drop(const Vector& vecVelocity);
	bool	Reload(void);

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

private:
	void	ToggleZoom(void);

	bool	m_bInZoom;
	bool	m_bSecondaryPressed;

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
	m_bInZoom = false;
	m_bSecondaryPressed = false;
}

//-----------------------------------------------------------------------------
// Purpose: Ataque primario
//-----------------------------------------------------------------------------
void CWeapon357::PrimaryAttack(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
	{
		return;
	}

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.15;
		}
		return;
	}

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	float delay = sv_357_firerate_delay.GetFloat();
	m_flNextPrimaryAttack = gpGlobals->curtime + delay;

	m_iClip1--;

	if (m_iClip1 <= 0 && m_bInZoom)
	{
#ifndef CLIENT_DLL
		pPlayer->SetFOV(this, 0, 0.1f);
#endif
		m_bInZoom = false;
	}

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	FireBulletsInfo_t info(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType);
	info.m_pAttacker = pPlayer;

	pPlayer->FireBullets(info);

#ifdef CLIENT_DLL
	if (sv_357_recoil_enable_viewangle.GetBool() && prediction->IsFirstTimePredicted())
	{
		QAngle angles;
		engine->GetViewAngles(angles);

		float verticalRecoil = sv_357_recoil_view_vertical.GetFloat();
		float horizontalRecoil = sv_357_recoil_view_horizontal.GetFloat();

		angles.x += random->RandomFloat(-verticalRecoil, verticalRecoil);
		angles.y += random->RandomFloat(-horizontalRecoil, horizontalRecoil);
		angles.z += 0.0f;

		engine->SetViewAngles(angles);
	}
#endif

	if (sv_357_recoil_enable_viewpunch.GetBool())
	{
		float verticalPunch = -sv_357_recoil_punch_vertical.GetFloat();
		float horizontalPunch = sv_357_recoil_punch_horizontal.GetFloat();

		pPlayer->ViewPunch(QAngle(verticalPunch, random->RandomFloat(-horizontalPunch, horizontalPunch), 0));
	}

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ataque secundario
//-----------------------------------------------------------------------------
void CWeapon357::SecondaryAttack(void)
{
	if (!sv_357_zoom_enable.GetBool())
		return;

	if (!m_bSecondaryPressed)
	{
		ToggleZoom();
		m_bSecondaryPressed = true;
	}

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.1f;
}

//-----------------------------------------------------------------------------
// Purpose: Toggle zoom
//-----------------------------------------------------------------------------
void CWeapon357::ToggleZoom(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
		return;

#ifndef CLIENT_DLL
	if (m_bInZoom)
	{
		if (pPlayer->SetFOV(this, 0, 0.2f))
		{
			m_bInZoom = false;
		}
	}
	else
	{
		if (pPlayer->SetFOV(this, sv_357_zoom_level.GetInt(), 0.2f))
		{
			m_bInZoom = true;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: ItemPostFrame
//-----------------------------------------------------------------------------
void CWeapon357::ItemPostFrame(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (!pOwner)
		return;

	if (!(pOwner->m_nButtons & IN_ATTACK2))
	{
		m_bSecondaryPressed = false;
	}

	if (m_bInZoom && (m_bInReload || (m_iClip1 <= 0 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)))
	{
#ifndef CLIENT_DLL
		pOwner->SetFOV(this, 0, 0.1f);
#endif
		m_bInZoom = false;
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Holster
//-----------------------------------------------------------------------------
bool CWeapon357::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	if (m_bInZoom)
	{
		CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
		if (pPlayer)
		{
#ifndef CLIENT_DLL
			pPlayer->SetFOV(this, 0, 0.2f);
#endif
			m_bInZoom = false;
		}
	}

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Drop
//-----------------------------------------------------------------------------
void CWeapon357::Drop(const Vector& vecVelocity)
{
	if (m_bInZoom)
	{
		CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
		if (pPlayer)
		{
#ifndef CLIENT_DLL
			pPlayer->SetFOV(this, 0, 0.2f);
#endif
			m_bInZoom = false;
		}
	}

	BaseClass::Drop(vecVelocity);
}

//-----------------------------------------------------------------------------
// Purpose: Reload - sair do zoom imediatamente
//-----------------------------------------------------------------------------
bool CWeapon357::Reload(void)
{
	if (m_bInZoom)
	{
		CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
		if (pPlayer)
		{
#ifndef CLIENT_DLL
			pPlayer->SetFOV(this, 0, 0.1f);
#endif
			m_bInZoom = false;
		}
	}

	return BaseClass::Reload();
}