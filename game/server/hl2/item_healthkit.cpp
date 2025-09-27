//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements health kits and wall mounted health chargers.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "gamerules.h"
#include "player.h"
#include "items.h"
#include "in_buttons.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_healthkit("sk_healthkit", "0");
ConVar	sk_healthvial("sk_healthvial", "0");
ConVar	sk_healthcharger("sk_healthcharger", "0");
ConVar  sv_healthcharger_speed("sv_healthcharger_speed", "1", FCVAR_GAMEDLL, "Amount of health given per tick by the health charger.");


//-----------------------------------------------------------------------------
// CHealthKit (CÓDIGO ORIGINAL INTOCADO)
//-----------------------------------------------------------------------------
class CHealthKit : public CItem
{
public:
	DECLARE_CLASS(CHealthKit, CItem);

	void Spawn(void);
	void Precache(void);
	bool MyTouch(CBasePlayer* pPlayer);
};

LINK_ENTITY_TO_CLASS(item_healthkit, CHealthKit);
PRECACHE_REGISTER(item_healthkit);

void CHealthKit::Spawn(void)
{
	Precache();
	SetModel("models/items/healthkit.mdl");
	BaseClass::Spawn();
}

void CHealthKit::Precache(void)
{
	PrecacheModel("models/items/healthkit.mdl");
	PrecacheScriptSound("HealthKit.Touch");
}

bool CHealthKit::MyTouch(CBasePlayer* pPlayer)
{
	if (pPlayer->TakeHealth(sk_healthkit.GetFloat(), DMG_GENERIC))
	{
		CSingleUserRecipientFilter user(pPlayer);
		user.MakeReliable();
		UserMessageBegin(user, "ItemPickup");
		WRITE_STRING(GetClassname());
		MessageEnd();
		CPASAttenuationFilter filter(pPlayer, "HealthKit.Touch");
		EmitSound(filter, pPlayer->entindex(), "HealthKit.Touch");
		if (g_pGameRules->ItemShouldRespawn(this)) { Respawn(); }
		else { UTIL_Remove(this); }
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// CHealthVial (CÓDIGO ORIGINAL INTOCADO)
//-----------------------------------------------------------------------------
class CHealthVial : public CItem
{
public:
	DECLARE_CLASS(CHealthVial, CItem);

	void Spawn(void) { Precache(); SetModel("models/healthvial.mdl"); BaseClass::Spawn(); }
	void Precache(void) { PrecacheModel("models/healthvial.mdl"); PrecacheScriptSound("HealthVial.Touch"); }
	bool MyTouch(CBasePlayer* pPlayer)
	{
		if (pPlayer->TakeHealth(sk_healthvial.GetFloat(), DMG_GENERIC))
		{
			CSingleUserRecipientFilter user(pPlayer);
			user.MakeReliable();
			UserMessageBegin(user, "ItemPickup");
			WRITE_STRING(GetClassname());
			MessageEnd();
			CPASAttenuationFilter filter(pPlayer, "HealthVial.Touch");
			EmitSound(filter, pPlayer->entindex(), "HealthVial.Touch");
			if (g_pGameRules->ItemShouldRespawn(this)) { Respawn(); }
			else { UTIL_Remove(this); }
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_healthvial, CHealthVial);
PRECACHE_REGISTER(item_healthvial);


// =======================================================================
// NOVAS CLASSES COM NOMES ÚNICOS PARA EVITAR CONFLITO
// =======================================================================

//-----------------------------------------------------------------------------
// CMyWallHealth: Versão customizada para func_healthcharger
//-----------------------------------------------------------------------------
class CMyWallHealth : public CBaseToggle
{
public:
	DECLARE_CLASS(CMyWallHealth, CBaseToggle);
	DECLARE_DATADESC();

	void Spawn(void);
	void Precache(void);
	bool KeyValue(const char* szKeyName, const char* szValue);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	// CORREÇÃO: Devolvido ObjectCaps para a entidade ser "usável"
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() | FCAP_CONTINUOUS_USE; }

private:
	void Off(void);
	void Recharge(void);

	float   m_flNextCharge;
	int		m_iReactivate;
	int		m_iJuice;
	int		m_iOn;
	float   m_flSoundTime;
};

// Apontando a entidade do Hammer para a nossa nova classe
LINK_ENTITY_TO_CLASS(func_healthcharger, CMyWallHealth);

BEGIN_DATADESC(CMyWallHealth)
DEFINE_FIELD(m_flNextCharge, FIELD_TIME),
DEFINE_FIELD(m_iReactivate, FIELD_INTEGER),
DEFINE_FIELD(m_iJuice, FIELD_INTEGER),
DEFINE_FIELD(m_iOn, FIELD_INTEGER),
DEFINE_FIELD(m_flSoundTime, FIELD_TIME),
DEFINE_FUNCTION(Off),
DEFINE_FUNCTION(Recharge),
END_DATADESC()

bool CMyWallHealth::KeyValue(const char* szKeyName, const char* szValue)
{
	if (FStrEq(szKeyName, "dmdelay")) { m_iReactivate = atoi(szValue); return true; }
	return BaseClass::KeyValue(szKeyName, szValue);
}

void CMyWallHealth::Spawn(void)
{
	Precache();
	SetSolid(SOLID_BSP);
	SetMoveType(MOVETYPE_PUSH);
	SetModel(STRING(GetModelName()));
	m_iJuice = sk_healthcharger.GetFloat();
}

void CMyWallHealth::Precache(void)
{
	PrecacheScriptSound("WallHealth.Deny");
	PrecacheScriptSound("WallHealth.Start");
	PrecacheScriptSound("WallHealth.LoopingContinueCharge");
	PrecacheScriptSound("WallHealth.Recharge");
}

void CMyWallHealth::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer()) return;
	if (pActivator->GetHealth() >= pActivator->GetMaxHealth()) return;
	if (m_iJuice <= 0) { if (m_flSoundTime <= gpGlobals->curtime) { m_flSoundTime = gpGlobals->curtime + 0.62; EmitSound("WallHealth.Deny"); } return; }

	SetNextThink(gpGlobals->curtime + 0.25f);
	SetThink(&CMyWallHealth::Off);

	if (m_flNextCharge >= gpGlobals->curtime) return;

	if (!m_iOn) { m_iOn++; EmitSound("WallHealth.Start"); m_flSoundTime = 0.56 + gpGlobals->curtime; }
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->curtime)) { m_iOn++; CPASAttenuationFilter filter(this, "WallHealth.LoopingContinueCharge"); filter.MakeReliable(); EmitSound(filter, entindex(), "WallHealth.LoopingContinueCharge"); }

	int iHealthToGive = sv_healthcharger_speed.GetInt();
	if (iHealthToGive <= 0) iHealthToGive = 1;
	if (m_iJuice < iHealthToGive) iHealthToGive = m_iJuice;
	if (iHealthToGive > 0 && pActivator->TakeHealth(iHealthToGive, DMG_GENERIC)) { m_iJuice -= iHealthToGive; }

	m_flNextCharge = gpGlobals->curtime + 0.1;
}

void CMyWallHealth::Recharge(void)
{
	EmitSound("WallHealth.Recharge");
	m_iJuice = sk_healthcharger.GetFloat();
	SetThink(NULL);
}

void CMyWallHealth::Off(void)
{
	if (m_iOn > 1) StopSound("WallHealth.LoopingContinueCharge");
	m_iOn = 0;

	float flRechargeTime = g_pGameRules->FlHealthChargerRechargeTime();
	if (m_iReactivate > 0)
	{
		flRechargeTime = m_iReactivate;
	}

	if ((m_iJuice < sk_healthcharger.GetFloat()) && (flRechargeTime > 0))
	{
		SetNextThink(gpGlobals->curtime + flRechargeTime);
		SetThink(&CMyWallHealth::Recharge);
	}
	else { SetThink(NULL); }
}

//-----------------------------------------------------------------------------
// CMyNewWallHealth: Versão customizada para item_healthcharger
//-----------------------------------------------------------------------------
class CMyNewWallHealth : public CBaseAnimating
{
public:
	DECLARE_CLASS(CMyNewWallHealth, CBaseAnimating);
	DECLARE_DATADESC();

	void Spawn(void);
	void Precache(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void StudioFrameAdvance(void);

	// CORREÇÃO: Devolvido ObjectCaps para a entidade ser "usável"
	virtual int ObjectCaps(void);

private:
	void Off(void);
	void Recharge(void);

	float   m_flNextCharge;
	int		m_iReactivate;
	int		m_iJuice;
	int		m_iOn;
	float   m_flSoundTime;
	int		m_nState;
	float	m_flJuice;
	bool    m_bPlayerFullyHealed;
};

// Apontando a entidade do Hammer para a nossa nova classe
LINK_ENTITY_TO_CLASS(item_healthcharger, CMyNewWallHealth);

BEGIN_DATADESC(CMyNewWallHealth)
DEFINE_FIELD(m_flNextCharge, FIELD_TIME),
DEFINE_FIELD(m_iReactivate, FIELD_INTEGER),
DEFINE_FIELD(m_iJuice, FIELD_INTEGER),
DEFINE_FIELD(m_iOn, FIELD_INTEGER),
DEFINE_FIELD(m_flSoundTime, FIELD_TIME),
DEFINE_FIELD(m_nState, FIELD_INTEGER),
DEFINE_FIELD(m_flJuice, FIELD_FLOAT),
DEFINE_FIELD(m_bPlayerFullyHealed, FIELD_BOOLEAN),
DEFINE_FUNCTION(Off),
DEFINE_FUNCTION(Recharge),
END_DATADESC()

int CMyNewWallHealth::ObjectCaps(void)
{
	int caps = BaseClass::ObjectCaps() | FCAP_CONTINUOUS_USE;
	if (m_bPlayerFullyHealed)
	{
		caps &= ~FCAP_CONTINUOUS_USE;
		caps |= FCAP_IMPULSE_USE;
	}
	return caps;
}


#define HEALTH_CHARGER_MODEL_NAME "models/props_combine/health_charger001.mdl"

void CMyNewWallHealth::Spawn(void)
{
	Precache();
	SetMoveType(MOVETYPE_NONE);
	SetSolid(SOLID_VPHYSICS);
	VPhysicsInitStatic();
	SetModel(HEALTH_CHARGER_MODEL_NAME);
	AddEffects(EF_NOSHADOW);
	ResetSequence(LookupSequence("idle"));
	m_iJuice = sk_healthcharger.GetFloat();
	m_nState = 0;
	m_iReactivate = 0;
	m_flJuice = m_iJuice;
	m_bPlayerFullyHealed = false;
	StudioFrameAdvance();
}

void CMyNewWallHealth::Precache(void)
{
	PrecacheModel(HEALTH_CHARGER_MODEL_NAME);
	PrecacheScriptSound("WallHealth.Deny");
	PrecacheScriptSound("WallHealth.Start");
	PrecacheScriptSound("WallHealth.LoopingContinueCharge");
	PrecacheScriptSound("WallHealth.Recharge");
}

void CMyNewWallHealth::StudioFrameAdvance(void)
{
	m_flPlaybackRate = 0;
	float flMaxJuice = sk_healthcharger.GetFloat();
	if (flMaxJuice > 0) { SetCycle(1.0f - (m_flJuice / flMaxJuice)); }
	else { SetCycle(1.0f); }
	m_flPrevAnimTime = m_flAnimTime;
	m_flAnimTime = gpGlobals->curtime;
}

void CMyNewWallHealth::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer()) return;

	m_bPlayerFullyHealed = false;

	if (m_iOn) { m_flJuice -= (1.0f / 0.1f) * gpGlobals->frametime; StudioFrameAdvance(); }

	if (m_iJuice <= 0)
	{
		ResetSequence(LookupSequence("emptyclick"));
		m_nState = 1;
		Off();
		if (m_flSoundTime <= gpGlobals->curtime) { m_flSoundTime = gpGlobals->curtime + 0.62; EmitSound("WallHealth.Deny"); }
		return;
	}
	if (pActivator->GetHealth() >= pActivator->GetMaxHealth())
	{
		m_bPlayerFullyHealed = true;
		if (ToBasePlayer(pActivator))
			ToBasePlayer(pActivator)->m_afButtonPressed &= ~IN_USE;
		EmitSound("WallHealth.Deny");
		return;
	}

	SetNextThink(gpGlobals->curtime + 0.25f);
	SetThink(&CMyNewWallHealth::Off);

	if (m_flNextCharge >= gpGlobals->curtime) return;

	if (!m_iOn) { m_iOn++; EmitSound("WallHealth.Start"); m_flSoundTime = 0.56 + gpGlobals->curtime; }
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->curtime)) { m_iOn++; CPASAttenuationFilter filter(this, "WallHealth.LoopingContinueCharge"); filter.MakeReliable(); EmitSound(filter, entindex(), "WallHealth.LoopingContinueCharge"); }

	int iHealthToGive = sv_healthcharger_speed.GetInt();
	if (iHealthToGive <= 0) iHealthToGive = 1;
	if (m_iJuice < iHealthToGive) iHealthToGive = m_iJuice;
	if (iHealthToGive > 0 && pActivator->TakeHealth(iHealthToGive, DMG_GENERIC)) { m_iJuice -= iHealthToGive; }

	m_flNextCharge = gpGlobals->curtime + 0.1;
}

void CMyNewWallHealth::Recharge(void)
{
	EmitSound("WallHealth.Recharge");
	m_flJuice = m_iJuice = sk_healthcharger.GetFloat();
	m_nState = 0;
	ResetSequence(LookupSequence("idle"));
	StudioFrameAdvance();
	m_iReactivate = 0;
	SetThink(NULL);
}

void CMyNewWallHealth::Off(void)
{
	if (m_iOn > 1) StopSound("WallHealth.LoopingContinueCharge");
	if (m_nState == 1) SetCycle(1.0f);
	m_iOn = 0;
	m_flJuice = m_iJuice;
	StudioFrameAdvance();

	float flRechargeTime = g_pGameRules->FlHealthChargerRechargeTime();
	if (m_iReactivate > 0)
	{
		flRechargeTime = m_iReactivate;
	}

	if ((m_iJuice < sk_healthcharger.GetFloat()) && flRechargeTime > 0)
	{
		SetNextThink(gpGlobals->curtime + flRechargeTime);
		SetThink(&CMyNewWallHealth::Recharge);
	}
	else { SetThink(NULL); }
}