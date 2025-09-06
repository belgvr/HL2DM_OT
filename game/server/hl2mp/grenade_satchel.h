//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Satchel Charge
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#ifndef	SATCHEL_H
#define	SATCHEL_H
#ifdef _WIN32
#pragma once
#endif
#include "basegrenade_shared.h"
#include "hl2mp/weapon_slam.h"

class CSoundPatch;
class CSprite;

class CSatchelCharge : public CBaseGrenade
{
public:
	DECLARE_CLASS(CSatchelCharge, CBaseGrenade);
	void			Spawn(void);
	void			Precache(void);
	void			BounceSound(void);
	void			SatchelTouch(CBaseEntity* pOther);
	void			SatchelThink(void);
	void			GetTeamColors(int& r, int& g, int& b, int& a);
	void			InputExplode(inputdata_t& inputdata);
	void			Deactivate(void);
	virtual int		OnTakeDamage(const CTakeDamageInfo& info);  // <- ADICIONAR ESTA LINHA

	// Funções para o sistema de steal
	void			SetOwnerName(const char* szName) { Q_strncpy(m_szOwnerName, szName, 64); }
	const char* GetOwnerName() { return m_szOwnerName; }

	CSatchelCharge();
	~CSatchelCharge();
	DECLARE_DATADESC();


	// Variáveis públicas
	float			m_flNextBounceSoundTime;
	bool			m_bInAir;
	Vector			m_vLastPosition;
	CWeapon_SLAM* m_pMyWeaponSLAM;	// Who shot me..
	bool			m_bIsAttached;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

private:
	void				CreateEffects(void);
	CHandle<CSprite>	m_hGlowSprite;
	char				m_szOwnerName[64];  // Nome do dono da SLAM
};

#endif	//SATCHEL_H
