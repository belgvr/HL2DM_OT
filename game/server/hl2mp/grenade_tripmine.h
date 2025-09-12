//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef GRENADE_TRIPMINE_H
#define GRENADE_TRIPMINE_H
#ifdef _WIN32
#pragma once
#endif
#include "basegrenade_shared.h"

class CBeam;

class CTripmineGrenade : public CBaseGrenade
{
public:
	DECLARE_CLASS(CTripmineGrenade, CBaseGrenade);

	CTripmineGrenade();
	virtual ~CTripmineGrenade();
	void Spawn(void);
	void Precache(void);
	void PowerUp();
	void GetLaserColor(int& r, int& g, int& b, int& a) const;
	float GetLaserWidth() const;

#if 0 // FIXME: OnTakeDamage_Alive() is no longer called now that base grenade derives from CBaseAnimating
	int OnTakeDamage_Alive(const CTakeDamageInfo& info);
#endif	

	void BeamBreakThink(void);
	void DelayDeathThink(void);
	void Event_Killed(const CTakeDamageInfo& info);
	void AttachToEntity(const CBaseEntity* entity);
	void MakeBeam(void);
	void KillBeam(void);
	void OnPhysGunPickup(CBasePlayer* pPhysGunUser, PhysGunPickup_t reason);

	// Função para sistema de steal/recuperação
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	// Funções para nome do dono
	void SetOwnerName(const char* szName) { Q_strncpy(m_szOwnerName, szName, 64); }
	const char* GetOwnerName() { return m_szOwnerName; }

public:
	EHANDLE		m_hOwner;

private:
	Vector		m_vecDir;
	Vector		m_vecEnd;
	float		m_flBeamLength;
	CBeam* m_pBeam;
	Vector		m_posOwner;
	Vector		m_angleOwner;
	const CBaseEntity* m_pAttachedObject;
	Vector m_vecOldPosAttachedObject;
	QAngle m_vecOldAngAttachedObject;
	char m_szOwnerName[64];  // Nome do dono da tripmine
	bool m_bBroken = false;


	DECLARE_DATADESC();
};

#endif // GRENADE_TRIPMINE_H