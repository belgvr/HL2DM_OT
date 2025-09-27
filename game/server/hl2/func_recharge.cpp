//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== h_battery.cpp ========================================================

  battery-related code

*/

#include "cbase.h"
#include "gamerules.h"
#include "player.h"
#include "engine/IEngineSound.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar	sk_suitcharger("sk_suitcharger", "0");
static ConVar	sk_suitcharger_citadel("sk_suitcharger_citadel", "0");
static ConVar	sk_suitcharger_citadel_maxarmor("sk_suitcharger_citadel_maxarmor", "0");

ConVar sv_suitcharger_speed("sv_suitcharger_speed", "1", FCVAR_GAMEDLL, "Amount of suit given per tick by the normal suit charger.");
ConVar sv_citadelcharger_speed("sv_citadelcharger_speed", "2", FCVAR_GAMEDLL, "Amount of suit given per tick by the Citadel suit charger.");
ConVar sv_citadelcharger_health_speed("sv_citadelcharger_health_speed", "5", FCVAR_GAMEDLL, "Amount of HEALTH given per tick by the Citadel suit charger.");

#define SF_CITADEL_RECHARGER	0x2000
#define SF_KLEINER_RECHARGER	0x4000 // Gives only 25 health

class CRecharge : public CBaseToggle
{
public:
	DECLARE_CLASS(CRecharge, CBaseToggle);
	DECLARE_DATADESC();

	void Spawn();
	bool CreateVPhysics();
	int DrawDebugTextOverlays(void);
	void Off(void);
	void Recharge(void);
	bool KeyValue(const char* szKeyName, const char* szValue);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int	ObjectCaps(void) { return (BaseClass::ObjectCaps() | FCAP_CONTINUOUS_USE); }

private:
	void InputRecharge(inputdata_t& inputdata);

	float MaxJuice() const;
	void UpdateJuice(int newJuice);

	float	m_flNextCharge;
	int		m_iReactivate;
	int		m_iJuice;
	int		m_iOn;
	float   m_flSoundTime;
	int		m_nState;

	COutputFloat m_OutRemainingCharge;
	COutputEvent m_OnHalfEmpty;
	COutputEvent m_OnEmpty;
	COutputEvent m_OnFull;
	COutputEvent m_OnPlayerUse;
	COutputEvent m_OnRestored; 
};

BEGIN_DATADESC(CRecharge)
DEFINE_FIELD(m_flNextCharge, FIELD_TIME),
DEFINE_FIELD(m_iReactivate, FIELD_INTEGER),
DEFINE_FIELD(m_iJuice, FIELD_INTEGER),
DEFINE_FIELD(m_iOn, FIELD_INTEGER),
DEFINE_FIELD(m_flSoundTime, FIELD_TIME),
DEFINE_FIELD(m_nState, FIELD_INTEGER),
DEFINE_FUNCTION(Off),
DEFINE_FUNCTION(Recharge),
DEFINE_OUTPUT(m_OutRemainingCharge, "OutRemainingCharge"),
DEFINE_OUTPUT(m_OnHalfEmpty, "OnHalfEmpty"),
DEFINE_OUTPUT(m_OnEmpty, "OnEmpty"),
DEFINE_OUTPUT(m_OnFull, "OnFull"),
DEFINE_OUTPUT(m_OnPlayerUse, "OnPlayerUse"),
DEFINE_INPUTFUNC(FIELD_VOID, "Recharge", InputRecharge),
END_DATADESC()


LINK_ENTITY_TO_CLASS(func_recharge, CRecharge);


bool CRecharge::KeyValue(const char* szKeyName, const char* szValue)
{
	if (FStrEq(szKeyName, "style") || FStrEq(szKeyName, "height") || FStrEq(szKeyName, "value1") || FStrEq(szKeyName, "value2") || FStrEq(szKeyName, "value3"))
	{
	}
	else if (FStrEq(szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(szValue);
	}
	else
	{
		return BaseClass::KeyValue(szKeyName, szValue);
	}

	return true;
}

void CRecharge::Spawn()
{
	Precache();
	SetSolid(SOLID_BSP);
	SetMoveType(MOVETYPE_PUSH);
	SetModel(STRING(GetModelName()));
	UpdateJuice(MaxJuice());
	m_nState = 0;
	CreateVPhysics();
}

bool CRecharge::CreateVPhysics()
{
	VPhysicsInitStatic();
	return true;
}

int CRecharge::DrawDebugTextOverlays(void)
{
	int text_offset = BaseClass::DrawDebugTextOverlays();
	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];
		Q_snprintf(tempstr, sizeof(tempstr), "Charge left: %i", m_iJuice);
		EntityText(text_offset, tempstr, 0);
		text_offset++;
	}
	return text_offset;
}

float CRecharge::MaxJuice()	const
{
	if (HasSpawnFlags(SF_CITADEL_RECHARGER))
	{
		return sk_suitcharger_citadel.GetFloat();
	}
	return sk_suitcharger.GetFloat();
}

void CRecharge::InputRecharge(inputdata_t& inputdata)
{
	Recharge();
}

void CRecharge::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		return;

	if (!((CBasePlayer*)pActivator)->IsSuitEquipped())
	{
		if (m_flSoundTime <= gpGlobals->curtime)
		{
			m_flSoundTime = gpGlobals->curtime + 0.62;
			EmitSound("SuitRecharge.Deny");
		}
		return;
	}

	if (m_iJuice <= 0)
	{
		m_nState = 1;
		Off();
		if (m_flSoundTime <= gpGlobals->curtime)
		{
			m_flSoundTime = gpGlobals->curtime + 0.62;
			EmitSound("SuitRecharge.Deny");
		}
		return;
	}

	SetNextThink(gpGlobals->curtime + 0.25);
	SetThink(&CRecharge::Off);

	if (m_flNextCharge >= gpGlobals->curtime)
		return;

	m_hActivator = pActivator;
	if (!m_hActivator->IsPlayer())
		return;

	if (!m_iOn)
	{
		m_iOn++;
		EmitSound("SuitRecharge.Start");
		m_flSoundTime = 0.56 + gpGlobals->curtime;
		m_OnPlayerUse.FireOutput(pActivator, this);
	}
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->curtime))
	{
		m_iOn++;
		CPASAttenuationFilter filter(this, "SuitRecharge.ChargingLoop");
		filter.MakeReliable();
		EmitSound(filter, entindex(), "SuitRecharge.ChargingLoop");
	}

	CBasePlayer* pl = (CBasePlayer*)m_hActivator.Get();
	int nMaxArmor = 100;
	int nIncrementArmor;

	if (HasSpawnFlags(SF_CITADEL_RECHARGER))
	{
		nMaxArmor = sk_suitcharger_citadel_maxarmor.GetInt();
		nIncrementArmor = sv_citadelcharger_speed.GetInt();
		if (pActivator->GetHealth() < pActivator->GetMaxHealth())
		{
			pActivator->TakeHealth(sv_citadelcharger_health_speed.GetInt(), DMG_GENERIC);
		}
	}
	else
	{
		nIncrementArmor = sv_suitcharger_speed.GetInt();
	}

	if (nIncrementArmor <= 0)
		nIncrementArmor = 1;

	if (m_iJuice < nIncrementArmor)
		nIncrementArmor = m_iJuice;

	if (nIncrementArmor > 0 && pl->ArmorValue() < nMaxArmor)
	{
		UpdateJuice(m_iJuice - nIncrementArmor);
		pl->IncrementArmorValue(nIncrementArmor, nMaxArmor);
	}

	float flRemaining = m_iJuice / MaxJuice();
	m_OutRemainingCharge.Set(flRemaining, pActivator, this);
	m_flNextCharge = gpGlobals->curtime + 0.1;
}

void CRecharge::Recharge(void)
{
	// Recarrega completamente
	m_iJuice = 100; // ou substitui pelo campo correto que define a capacidade

	// Volta para o estado "cheio"
	m_nState = 0;

	// Para de pensar até ser usado de novo
	SetThink(NULL);
}


void CRecharge::Off(void)
{
	if (m_iOn > 1)
	{
		StopSound("SuitRecharge.ChargingLoop");
	}
	m_iOn = 0;

	// CORREÇÃO: Lógica de recarga parcial para o carregador antigo
	float flRechargeTime = g_pGameRules->FlHEVChargerRechargeTime();
	if (m_iReactivate > 0) // Usa o valor do dmdelay se existir
	{
		flRechargeTime = m_iReactivate;
	}

	if ((m_iJuice < MaxJuice()) && (flRechargeTime > 0))
	{
		SetNextThink(gpGlobals->curtime + flRechargeTime);
		SetThink(&CRecharge::Recharge);
	}
	else
	{
		SetThink(NULL);
	}
}

//=========================================================================================================================
// CNewRecharge (versão com modelo)
//=========================================================================================================================

class CNewRecharge : public CBaseAnimating
{
public:
	DECLARE_CLASS(CNewRecharge, CBaseAnimating);
	DECLARE_DATADESC();

	void Spawn();
	bool CreateVPhysics();
	int DrawDebugTextOverlays(void);
	void Off(void);
	void Recharge(void);
	bool KeyValue(const char* szKeyName, const char* szValue);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int	ObjectCaps(void); // Alterado para função
	void SetInitialCharge(void);

private:
	void InputRecharge(inputdata_t& inputdata);
	void InputSetCharge(inputdata_t& inputdata);
	float MaxJuice() const;
	void UpdateJuice(int newJuice);
	void Precache(void);

	float	m_flNextCharge;
	int		m_iReactivate;
	int		m_iJuice;
	int		m_iOn;
	float   m_flSoundTime;
	int		m_nState;
	//int		m_iCaps; // Substituído pela função ObjectCaps()
	bool    m_bPlayerAtMax;
	int		m_iMaxJuice;
	float	m_flJuice;

	COutputFloat m_OutRemainingCharge;
	COutputEvent m_OnHalfEmpty;
	COutputEvent m_OnEmpty;
	COutputEvent m_OnFull;
	COutputEvent m_OnPlayerUse;
	COutputEvent m_OnRestored; 

	virtual void StudioFrameAdvance(void);
};

BEGIN_DATADESC(CNewRecharge)
DEFINE_FIELD(m_flNextCharge, FIELD_TIME),
DEFINE_FIELD(m_iReactivate, FIELD_INTEGER),
DEFINE_FIELD(m_iJuice, FIELD_INTEGER),
DEFINE_FIELD(m_iOn, FIELD_INTEGER),
DEFINE_FIELD(m_flSoundTime, FIELD_TIME),
DEFINE_FIELD(m_nState, FIELD_INTEGER),
//DEFINE_FIELD(m_iCaps, FIELD_INTEGER), // Removido
DEFINE_FIELD(m_bPlayerAtMax, FIELD_BOOLEAN),
DEFINE_FIELD(m_iMaxJuice, FIELD_INTEGER),
DEFINE_FIELD(m_flJuice, FIELD_FLOAT),
DEFINE_FUNCTION(Off),
DEFINE_FUNCTION(Recharge),
DEFINE_OUTPUT(m_OutRemainingCharge, "OutRemainingCharge"),
DEFINE_OUTPUT(m_OnHalfEmpty, "OnHalfEmpty"),
DEFINE_OUTPUT(m_OnEmpty, "OnEmpty"),
DEFINE_OUTPUT(m_OnFull, "OnFull"),
DEFINE_OUTPUT(m_OnPlayerUse, "OnPlayerUse"),
DEFINE_OUTPUT(m_OnRestored, "OnRestored"),
DEFINE_INPUTFUNC(FIELD_VOID, "Recharge", InputRecharge),
DEFINE_INPUTFUNC(FIELD_INTEGER, "SetCharge", InputSetCharge),
END_DATADESC()


LINK_ENTITY_TO_CLASS(item_suitcharger, CNewRecharge);

#define SUIT_CHARGER_MODEL_NAME "models/props_combine/suit_charger001.mdl"
#define CHARGE_RATE 0.25f
#define CHARGES_PER_SECOND 1 / CHARGE_RATE
#define CITADEL_CHARGES_PER_SECOND 10 / CHARGE_RATE
#define CALLS_PER_SECOND 7.0f * CHARGES_PER_SECOND

int CNewRecharge::ObjectCaps(void)
{
	int caps = BaseClass::ObjectCaps() | FCAP_CONTINUOUS_USE;
	if (m_bPlayerAtMax)
	{
		caps &= ~FCAP_CONTINUOUS_USE;
		caps |= FCAP_IMPULSE_USE;
	}
	return caps;
}


bool CNewRecharge::KeyValue(const char* szKeyName, const char* szValue)
{
	if (FStrEq(szKeyName, "style") || FStrEq(szKeyName, "height") || FStrEq(szKeyName, "value1") || FStrEq(szKeyName, "value2") || FStrEq(szKeyName, "value3"))
	{
	}
	else if (FStrEq(szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(szValue);
	}
	else
	{
		return BaseClass::KeyValue(szKeyName, szValue);
	}
	return true;
}

void CNewRecharge::Precache(void)
{
	PrecacheModel(SUIT_CHARGER_MODEL_NAME);
	PrecacheScriptSound("SuitRecharge.Deny");
	PrecacheScriptSound("SuitRecharge.Start");
	PrecacheScriptSound("SuitRecharge.ChargingLoop");
}

void CNewRecharge::SetInitialCharge(void)
{
	if (HasSpawnFlags(SF_KLEINER_RECHARGER))
	{
		m_iMaxJuice = 25.0f;
		return;
	}
	if (HasSpawnFlags(SF_CITADEL_RECHARGER))
	{
		m_iMaxJuice = sk_suitcharger_citadel.GetFloat();
		return;
	}
	m_iMaxJuice = sk_suitcharger.GetFloat();
}

void CNewRecharge::Spawn()
{
	Precache();
	SetMoveType(MOVETYPE_NONE);
	SetSolid(SOLID_VPHYSICS);
	CreateVPhysics();
	SetModel(SUIT_CHARGER_MODEL_NAME);
	AddEffects(EF_NOSHADOW);
	ResetSequence(LookupSequence("idle"));
	SetInitialCharge();
	UpdateJuice(MaxJuice());
	m_nState = 0;
	m_bPlayerAtMax = false;
	// m_iCaps = FCAP_CONTINUOUS_USE; // Removido
	m_flJuice = m_iJuice;
	//m_iReactivate = 0; // Removido para usar o valor de dmdelay
	SetCycle(1.0f - (m_flJuice / MaxJuice()));
}

bool CNewRecharge::CreateVPhysics()
{
	VPhysicsInitStatic();
	return true;
}

int CNewRecharge::DrawDebugTextOverlays(void)
{
	int text_offset = BaseClass::DrawDebugTextOverlays();
	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];
		Q_snprintf(tempstr, sizeof(tempstr), "Charge left: %i", m_iJuice);
		EntityText(text_offset, tempstr, 0);
		text_offset++;
	}
	return text_offset;
}

void CNewRecharge::StudioFrameAdvance(void)
{
	m_flPlaybackRate = 0;
	float flMaxJuice = MaxJuice();
	if (flMaxJuice <= 0) flMaxJuice = 1.0;
	float flNewJuice = 1.0f - (float)(m_flJuice / flMaxJuice);
	SetCycle(flNewJuice);
	if (!m_flPrevAnimTime)
	{
		m_flPrevAnimTime = gpGlobals->curtime;
	}
	m_flPrevAnimTime = m_flAnimTime;
	m_flAnimTime = gpGlobals->curtime;
}

float CNewRecharge::MaxJuice() const
{
	return m_iMaxJuice;
}

void CRecharge::UpdateJuice(int newJuice)
{
	int oldJuice = m_iJuice;
	int halfJuice = (int)(MaxJuice() * 0.5f);

	// Se a energia DIMINUIU
	if (newJuice < oldJuice)
	{
		// Cruzou para metade ou menos?
		if (newJuice <= halfJuice && oldJuice > halfJuice)
		{
			m_OnHalfEmpty.FireOutput(this, this);
		}

		// Chegou a zero?
		if (newJuice <= 0 && oldJuice > 0)
		{
			m_OnEmpty.FireOutput(this, this);
		}
	}
	// Se a energia AUMENTOU
	else if (newJuice > oldJuice)
	{
		// Passou de volta para cima da metade?
		if (newJuice > halfJuice && oldJuice <= halfJuice)
		{
			m_OnRestored.FireOutput(this, this);
		}

		// Chegou ao máximo?
		if (newJuice == (int)MaxJuice())
		{
			m_OnFull.FireOutput(this, this);

			// Garante que o sinal de restauração também seja enviado ao ficar cheio
			if (oldJuice <= halfJuice)
			{
				m_OnRestored.FireOutput(this, this);
			}
		}
	}

	m_iJuice = newJuice;
}

void CNewRecharge::InputRecharge(inputdata_t& inputdata)
{
	Recharge();
}

void CNewRecharge::InputSetCharge(inputdata_t& inputdata)
{
	ResetSequence(LookupSequence("idle"));
	int iJuice = inputdata.value.Int();
	m_flJuice = m_iMaxJuice = m_iJuice = iJuice;
	StudioFrameAdvance();
}

void CNewRecharge::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		return;

	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pActivator);
	m_bPlayerAtMax = false;

	if (m_iOn)
	{
		float flCharges = CHARGES_PER_SECOND;
		float flCalls = CALLS_PER_SECOND;
		if (HasSpawnFlags(SF_CITADEL_RECHARGER))
			flCharges = CITADEL_CHARGES_PER_SECOND;
		m_flJuice -= flCharges / flCalls;
		StudioFrameAdvance();
	}

	if (!pPlayer->IsSuitEquipped())
	{
		if (m_flSoundTime <= gpGlobals->curtime)
		{
			m_flSoundTime = gpGlobals->curtime + 0.62;
			EmitSound("SuitRecharge.Deny");
		}
		return;
	}

	if (m_iJuice <= 0)
	{
		ResetSequence(LookupSequence("emptyclick"));
		m_nState = 1;
		Off();
		if (m_flSoundTime <= gpGlobals->curtime)
		{
			m_flSoundTime = gpGlobals->curtime + 0.62;
			EmitSound("SuitRecharge.Deny");
		}
		return;
	}

	int nMaxArmor = 100;
	if (HasSpawnFlags(SF_CITADEL_RECHARGER))
	{
		nMaxArmor = sk_suitcharger_citadel_maxarmor.GetInt();
	}

	int nIncrementArmor;
	if (HasSpawnFlags(SF_CITADEL_RECHARGER))
	{
		nIncrementArmor = sv_citadelcharger_speed.GetInt();
		if (pActivator->GetHealth() < pActivator->GetMaxHealth() && m_flNextCharge < gpGlobals->curtime)
		{
			pActivator->TakeHealth(sv_citadelcharger_health_speed.GetInt(), DMG_GENERIC);
		}
	}
	else
	{
		nIncrementArmor = sv_suitcharger_speed.GetInt();
	}

	if (pPlayer->ArmorValue() >= nMaxArmor)
	{
		if (!HasSpawnFlags(SF_CITADEL_RECHARGER) || (HasSpawnFlags(SF_CITADEL_RECHARGER) && pActivator->GetHealth() >= pActivator->GetMaxHealth()))
		{
			pPlayer->m_afButtonPressed &= ~IN_USE;
			m_bPlayerAtMax = true;
			EmitSound("SuitRecharge.Deny");
			return;
		}
	}

	SetNextThink(gpGlobals->curtime + CHARGE_RATE);
	SetThink(&CNewRecharge::Off);

	if (m_flNextCharge >= gpGlobals->curtime)
		return;

	if (!m_iOn)
	{
		m_iOn++;
		EmitSound("SuitRecharge.Start");
		m_flSoundTime = 0.56 + gpGlobals->curtime;
		m_OnPlayerUse.FireOutput(pActivator, this);
	}
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->curtime))
	{
		m_iOn++;
		CPASAttenuationFilter filter(this, "SuitRecharge.ChargingLoop");
		filter.MakeReliable();
		EmitSound(filter, entindex(), "SuitRecharge.ChargingLoop");
	}

	if (pPlayer->ArmorValue() < nMaxArmor)
	{
		if (nIncrementArmor <= 0)
			nIncrementArmor = 1;
		if (m_iJuice < nIncrementArmor)
			nIncrementArmor = m_iJuice;

		if (nIncrementArmor > 0)
		{
			UpdateJuice(m_iJuice - nIncrementArmor);
			pPlayer->IncrementArmorValue(nIncrementArmor, nMaxArmor);
		}
	}

	float flRemaining = m_iJuice / MaxJuice();
	m_OutRemainingCharge.Set(flRemaining, pActivator, this);
	m_flNextCharge = gpGlobals->curtime + 0.1;
}

void CNewRecharge::Recharge(void)
{
	EmitSound("SuitRecharge.Start");
	ResetSequence(LookupSequence("idle"));
	UpdateJuice(MaxJuice());
	m_nState = 0;
	m_flJuice = m_iJuice;
	StudioFrameAdvance();
	SetThink(&CNewRecharge::SUB_DoNothing);
}

void CNewRecharge::Off(void)
{
	if (m_iOn > 1)
	{
		StopSound("SuitRecharge.ChargingLoop");
	}
	if (m_nState == 1)
	{
		SetCycle(1.0f);
	}
	m_iOn = 0;
	m_flJuice = m_iJuice;

	// CORREÇÃO: Lógica de recarga parcial para o carregador novo
	float flRechargeTime = g_pGameRules->FlHEVChargerRechargeTime();
	if (HasSpawnFlags(SF_CITADEL_RECHARGER))
	{
		flRechargeTime *= 2;
	}
	if (m_iReactivate > 0) // Usa o valor de dmdelay se existir
	{
		flRechargeTime = m_iReactivate;
	}

	if ((m_iJuice < MaxJuice()) && flRechargeTime > 0)
	{
		SetNextThink(gpGlobals->curtime + flRechargeTime);
		SetThink(&CNewRecharge::Recharge);
	}
	else
	{
		SetThink(NULL);
	}
}
void CNewRecharge::UpdateJuice(int newJuice)
{
	int oldJuice = m_iJuice;
	int halfJuice = (int)(MaxJuice() * 0.5f);

	// Se a energia DIMINUIU
	if (newJuice < oldJuice)
	{
		// Cruzou para metade ou menos?
		if (newJuice <= halfJuice && oldJuice > halfJuice)
		{
			m_OnHalfEmpty.FireOutput(this, this);
		}

		// Chegou a zero?
		if (newJuice <= 0 && oldJuice > 0)
		{
			m_OnEmpty.FireOutput(this, this);
		}
	}
	// Se a energia AUMENTOU
	else if (newJuice > oldJuice)
	{
		// Passou de volta para cima da metade?
		if (newJuice > halfJuice && oldJuice <= halfJuice)
		{
			m_OnRestored.FireOutput(this, this);
		}

		// Chegou ao máximo?
		if (newJuice == (int)MaxJuice())
		{
			m_OnFull.FireOutput(this, this);

			// Garante que o sinal de restauração também seja enviado ao ficar cheio
			if (oldJuice <= halfJuice)
			{
				m_OnRestored.FireOutput(this, this);
			}
		}
	}

	m_iJuice = newJuice;
}