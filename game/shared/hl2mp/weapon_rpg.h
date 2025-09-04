//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_RPG_H
#define WEAPON_RPG_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#include "iviewrender_beams.h"
#endif

#ifndef CLIENT_DLL
#include "Sprite.h"
#include "npcevent.h"
#include "beam_shared.h"

class CWeaponRPG;
class RocketTrail;

// Forward declaration
class CLaserDot;

//###########################################################################
//	>> CMissile		(missile launcher class is below this one!)
//###########################################################################
class CMissile : public CBaseCombatCharacter
{
	DECLARE_CLASS(CMissile, CBaseCombatCharacter);
	DECLARE_DATADESC();

public:
	CMissile();
	~CMissile();

#ifdef HL1_DLL
	Class_T Classify(void) { return CLASS_NONE; }
#else
	Class_T Classify(void) { return CLASS_MISSILE; }
#endif

	void	Spawn(void);
	void	Precache(void);
	void	MissileTouch(CBaseEntity* pOther);
	void	Explode(void);
	void	ShotDown(void);
	void	AccelerateThink(void);
	void	AugerThink(void);
	void	IgniteThink(void);
	void	SeekThink(void);
	void	DumbFire(void);
	void	SetGracePeriod(float flGracePeriod);

	int		OnTakeDamage_Alive(const CTakeDamageInfo& info);
	void	Event_Killed(const CTakeDamageInfo& info);

	virtual float	GetDamage() { return m_flDamage; }
	virtual void	SetDamage(float flDamage) { m_flDamage = flDamage; }

	// NEW: Dumb fire mode for secondary attack
	void	SetDumbFireMode(bool bDumbFire) { m_bDumbFireMode = bDumbFire; }
	bool	IsDumbFireMode() const { return m_bDumbFireMode; }

	unsigned int PhysicsSolidMaskForEntity(void) const;

	CHandle<CWeaponRPG>		m_hOwner;

	static CMissile* Create(const Vector& vecOrigin, const QAngle& vecAngles, edict_t* pentOwner);

protected:
	virtual void DoExplosion();
	virtual void ComputeActualDotPosition(CLaserDot* pLaserDot, Vector* pActualDotPosition, float* pHomingSpeed);
	virtual int AugerHealth() { return m_iMaxHealth - 20; }

	// Creates the smoke trail
	void CreateSmokeTrail(void);

	// Gets the shooting position 
	void GetShootPosition(CLaserDot* pLaserDot, Vector* pShootPosition);

	CHandle<RocketTrail>	m_hRocketTrail;
	float					m_flAugerTime;		// Amount of time to auger before blowing up anyway
	float					m_flMarkDeadTime;
	float					m_flDamage;

	// NEW: Additional members for enhanced functionality
	bool					m_bDumbFireMode;	// Secondary fire mode
	EHANDLE					m_hAttacker;		// Who shot down the missile

private:
	float					m_flGracePeriodEndsAt;
};


//-----------------------------------------------------------------------------
// Laser dot control
//-----------------------------------------------------------------------------
CBaseEntity* CreateLaserDot(const Vector& origin, CBaseEntity* pOwner, bool bVisibleDot);
void SetLaserDotTarget(CBaseEntity* pLaserDot, CBaseEntity* pTarget);
void EnableLaserDot(CBaseEntity* pLaserDot, bool bEnable);

//-----------------------------------------------------------------------------
// Specialized mizzizzile
//-----------------------------------------------------------------------------
class CAPCMissile : public CMissile
{
	DECLARE_CLASS(CAPCMissile, CMissile);
	DECLARE_DATADESC();

public:
	static CAPCMissile* Create(const Vector& vecOrigin, const QAngle& vecAngles, const Vector& vecVelocity, CBaseEntity* pOwner);

	CAPCMissile();
	~CAPCMissile();
	void	IgniteDelay(void);
	void	AugerDelay(float flDelayTime);
	void	ExplodeDelay(float flDelayTime);
	void	DisableGuiding();
#if defined( HL2_DLL )
	virtual Class_T Classify(void) { return CLASS_COMBINE; }
#endif

	void	AimAtSpecificTarget(CBaseEntity* pTarget);
	void	SetGuidanceHint(const char* pHintName);

	CAPCMissile* m_pNext;

protected:
	virtual void DoExplosion();
	virtual void ComputeActualDotPosition(CLaserDot* pLaserDot, Vector* pActualDotPosition, float* pHomingSpeed);
	virtual int AugerHealth();

private:
	void Init();
	void ComputeLeadingPosition(const Vector& vecShootPosition, CBaseEntity* pTarget, Vector* pLeadPosition);
	void BeginSeekThink();
	void AugerStartThink();
	void ExplodeThink();
	void APCMissileTouch(CBaseEntity* pOther);

	float	m_flReachedTargetTime;
	float	m_flIgnitionTime;
	bool	m_bGuidingDisabled;
	float   m_flLastHomingSpeed;
	EHANDLE m_hSpecificTarget;
	string_t m_strHint;
};


//-----------------------------------------------------------------------------
// Finds apc missiles in cone
//-----------------------------------------------------------------------------
CAPCMissile* FindAPCMissileInCone(const Vector& vecOrigin, const Vector& vecDirection, float flAngle);

#endif // CLIENT_DLL guard

//-----------------------------------------------------------------------------
// RPG
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL
#define CWeaponRPG C_WeaponRPG
#endif

class CWeaponRPG : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS(CWeaponRPG, CBaseHL2MPCombatWeapon);
public:

	CWeaponRPG();
	~CWeaponRPG();

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	void	Precache(void);

	void	PrimaryAttack(void);
	void	SecondaryAttack(void);		// NEW: Secondary attack for laser toggle
	virtual float GetFireRate(void) { return 1; };
	void	ItemPostFrame(void);

	void	Activate(void);
	void	DecrementAmmo(CBaseCombatCharacter* pOwner);

	bool	Deploy(void);
	bool	Holster(CBaseCombatWeapon* pSwitchingTo = NULL);
	bool	Reload(void);
	bool	WeaponShouldBeLowered(void);
	bool	Lower(void);

	bool	CanHolster(void) const;

	virtual void Drop(const Vector& vecVelocity);

	int		GetMinBurst() { return 1; }
	int		GetMaxBurst() { return 1; }
	float	GetMinRestTime() { return 4.0; }
	float	GetMaxRestTime() { return 4.0; }

	void	StartGuiding(void);
	void	StopGuiding(void);
	void	ToggleGuiding(void);
	bool	IsGuiding(void);

	void	NotifyRocketDied(void);

	bool	HasAnyAmmo(void);

	void	SuppressGuiding(bool state = true);

	void	CreateLaserPointer(void);
	void	UpdateLaserPosition(Vector vecMuzzlePos = vec3_origin, Vector vecEndPos = vec3_origin);
	Vector	GetLaserPosition(void);

	bool m_bLastKnownLaserState;  // Estado do laser antes da munição acabar
	bool m_bLastAttack2State;

	// NPC RPG users cheat and directly set the laser pointer's origin
	void	UpdateNPCLaserPosition(const Vector& vecTarget);
	void	SetNPCLaserPosition(const Vector& vecTarget);
	const Vector& GetNPCLaserPosition(void);

	// NEW: Missile access
	CBaseEntity* GetMissile(void) { return m_hMissile; }

#ifdef CLIENT_DLL

	// We need to render opaque and translucent pieces
	virtual RenderGroup_t	GetRenderGroup(void) { return RENDER_GROUP_TWOPASS; }

	virtual void	NotifyShouldTransmit(ShouldTransmitState_t state);
	virtual int		DrawModel(int flags);
	virtual void	ViewModelDrawn(C_BaseViewModel* pBaseViewModel);
	virtual bool	IsTranslucent(void);

	void			InitBeam(void);
	void			GetWeaponAttachment(int attachmentId, Vector& outVector, Vector* dir = NULL);
	void			DrawEffects(void);

	CMaterialReference	m_hSpriteMaterial;	// Used for the laser glint
	CMaterialReference	m_hBeamMaterial;	// Used for the laser beam
	Beam_t* m_pBeam;			// Laser beam temp entity

#endif	//CLIENT_DLL

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

protected:

	CNetworkVar(bool, m_bInitialStateUpdate);
	CNetworkVar(bool, m_bGuiding);
	CNetworkVar(bool, m_bHideGuiding);

	CNetworkHandle(CBaseEntity, m_hMissile);
	CNetworkVar(Vector, m_vecLaserDot);

	// NEW: Laser toggle mode
	bool			m_bLaserToggleMode;		// Controls if laser is enabled/disabled

#ifndef CLIENT_DLL
	CHandle<CLaserDot>	m_hLaserDot;
#endif

private:
	bool m_bLaserTogglePressed;
	int m_iRPGShotCount;
	CWeaponRPG(const CWeaponRPG&);
};

//-----------------------------------------------------------------------------
// CVars - External declarations
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
extern ConVar sv_rpg_missile_ignition_delay;
extern ConVar sv_rpg_secondary_mode;
extern ConVar sv_rpg_secondary_missile_velocity;
extern ConVar sv_rpg_missile_shotdown;
extern ConVar sv_rpg_missile_kill_credit;
extern ConVar sv_rpg_missile_hitbox_scale;
extern ConVar sv_rpg_weapon_switch;
extern ConVar sv_rpg_missile_shotdown_explode;
#endif

#endif // WEAPON_RPG_H