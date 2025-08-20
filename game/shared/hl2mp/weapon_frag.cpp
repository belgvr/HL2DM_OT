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
#include "c_te_effect_dispatch.h"
#else
#include "hl2mp_player.h"
#include "te_effect_dispatch.h"
#include "grenade_frag.h"
#endif

#include "weapon_ar2.h"
#include "effect_dispatch_data.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// CVars for grenade configuration
ConVar sv_grenade_timer("sv_grenade_timer", "2.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Grenade explosion timer in seconds", true, 0.5f, true, 10.0f);
ConVar sv_grenade_rethrow_delay("sv_grenade_rethrow_delay", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay between grenade throws", true, 0.1f, true, 2.0f);
ConVar sv_grenade_radius("sv_grenade_radius", "4.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Physical radius of the grenade", true, 1.0f, true, 10.0f);
ConVar sv_grenade_damage_radius("sv_grenade_damage_radius", "250.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Damage radius of the grenade explosion", true, 50.0f, true, 500.0f);

// Throw velocities and charging system
ConVar sv_grenade_throw_charge_enabled("sv_grenade_throw_charge_enabled", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable charge system for normal throw (mouse1)", true, 0.0f, true, 1.0f);
ConVar sv_grenade_lob_charge_enabled("sv_grenade_lob_charge_enabled", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable charge system for lob throw (mouse2)", true, 0.0f, true, 1.0f);

ConVar sv_grenade_throw_charge_time("sv_grenade_throw_charge_time", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Time to fully charge normal throw", true, 0.5f, true, 5.0f);
ConVar sv_grenade_lob_charge_time("sv_grenade_lob_charge_time", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Time to fully charge lob throw", true, 0.5f, true, 5.0f);

ConVar sv_grenade_throw_velocity_min("sv_grenade_throw_velocity_min", "600", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum velocity for normal grenade throw", true, 200.0f, true, 2000.0f);
ConVar sv_grenade_throw_velocity_max("sv_grenade_throw_velocity_max", "1800", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum velocity for normal grenade throw", true, 500.0f, true, 3000.0f);

ConVar sv_grenade_lob_velocity_min("sv_grenade_lob_velocity_min", "200", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum velocity for grenade lob throw", true, 100.0f, true, 800.0f);
ConVar sv_grenade_lob_velocity_max("sv_grenade_lob_velocity_max", "600", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum velocity for grenade lob throw", true, 200.0f, true, 1200.0f);

ConVar sv_grenade_lob_upward("sv_grenade_lob_upward", "50", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Upward velocity component for lob throw", true, 0.0f, true, 200.0f);
ConVar sv_grenade_roll_velocity("sv_grenade_roll_velocity", "700", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Velocity for grenade roll", true, 100.0f, true, 1500.0f);

// Adicione essas CVars junto com as outras no topo do arquivo:

// Valores padrão quando charge está DESABILITADO
ConVar sv_grenade_throw_velocity("sv_grenade_throw_velocity", "1200", FCVAR_GAMEDLL | FCVAR_NOTIFY,	"Default velocity for normal grenade throw when charge is disabled", true, 200.0f, true, 3000.0f);

ConVar sv_grenade_lob_velocity("sv_grenade_lob_velocity", "350", FCVAR_GAMEDLL | FCVAR_NOTIFY,	"Default velocity for grenade lob throw when charge is disabled", true, 100.0f, true, 1200.0f);

// Positioning offsets
ConVar sv_grenade_forward_offset("sv_grenade_forward_offset", "18.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Forward offset for grenade spawn position", true, 5.0f, true, 50.0f);
ConVar sv_grenade_right_offset("sv_grenade_right_offset", "8.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Right offset for grenade spawn position", true, 0.0f, true, 20.0f);
ConVar sv_grenade_lob_down_offset("sv_grenade_lob_down_offset", "8.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Downward offset for lob throw position", true, 0.0f, true, 20.0f);

// Angular impulses
ConVar sv_grenade_throw_angular_base("sv_grenade_throw_angular_base", "600", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Base angular impulse for normal throw", true, 0.0f, true, 2000.0f);
ConVar sv_grenade_throw_angular_random("sv_grenade_throw_angular_random", "1200", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Random variation for throw angular impulse", true, 0.0f, true, 3000.0f);
ConVar sv_grenade_lob_angular_base("sv_grenade_lob_angular_base", "200", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Base angular impulse for lob throw", true, 0.0f, true, 1000.0f);
ConVar sv_grenade_lob_angular_random("sv_grenade_lob_angular_random", "600", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Random variation for lob angular impulse", true, 0.0f, true, 2000.0f);
ConVar sv_grenade_roll_angular("sv_grenade_roll_angular", "720", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Angular velocity for grenade roll", true, 0.0f, true, 2000.0f);

// Trajectory adjustments
ConVar sv_grenade_throw_upward("sv_grenade_throw_upward", "0.1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Upward component for normal throw", true, 0.0f, true, 1.0f);

// Charge HUD display
ConVar sv_grenade_charge_hud("sv_grenade_charge_hud", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show charge progress on HUD", true, 0.0f, true, 1.0f);

#define GRENADE_PAUSED_NO			0
#define GRENADE_PAUSED_PRIMARY		1
#define GRENADE_PAUSED_SECONDARY	2

#ifdef CLIENT_DLL
#define CWeaponFrag C_WeaponFrag
#endif

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CWeaponFrag : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS(CWeaponFrag, CBaseHL2MPCombatWeapon);
public:

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponFrag();

	void	Precache(void);
	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
	void	DecrementAmmo(CBaseCombatCharacter* pOwner);
	void	ItemPostFrame(void);

	bool	Deploy(void);
	bool	Holster(CBaseCombatWeapon* pSwitchingTo = NULL);

	bool	Reload(void);

#ifndef CLIENT_DLL
	void Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
#endif

	void	ThrowGrenade(CBasePlayer* pPlayer);
	bool	IsPrimed(bool) { return (m_AttackPaused != 0); }

	// Charge system methods
	void	UpdateChargeAmount(void);
	float	GetCurrentChargeVelocity(bool bIsLob);
	void	StartCharge(void);
	void	ResetCharge(void);

private:

	void	RollGrenade(CBasePlayer* pPlayer);
	void	LobGrenade(CBasePlayer* pPlayer);
	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
	void	CheckThrowPosition(CBasePlayer* pPlayer, const Vector& vecEye, Vector& vecSrc);

	CNetworkVar(bool, m_bRedraw);	//Draw the weapon again after throwing a grenade

	CNetworkVar(int, m_AttackPaused);
	CNetworkVar(bool, m_fDrawbackFinished);

	// Charge system variables (local only for now)
	float	m_flChargeStartTime;	// When the charge started
	float	m_flChargeAmount;		// Current charge level (0.0 to 1.0)

	CWeaponFrag(const CWeaponFrag&);

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif
};

#ifndef CLIENT_DLL

acttable_t	CWeaponFrag::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_GRENADE,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_GRENADE,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_GRENADE,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_GRENADE,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_GRENADE,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_GRENADE,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_GRENADE,					false },
};

IMPLEMENT_ACTTABLE(CWeaponFrag);

#endif

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponFrag, DT_WeaponFrag)

BEGIN_NETWORK_TABLE(CWeaponFrag, DT_WeaponFrag)

#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bRedraw)),
RecvPropBool(RECVINFO(m_fDrawbackFinished)),
RecvPropInt(RECVINFO(m_AttackPaused)),
#else
SendPropBool(SENDINFO(m_bRedraw)),
SendPropBool(SENDINFO(m_fDrawbackFinished)),
SendPropInt(SENDINFO(m_AttackPaused)),
#endif

END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponFrag)
DEFINE_PRED_FIELD(m_bRedraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK),
DEFINE_PRED_FIELD(m_fDrawbackFinished, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK),
DEFINE_PRED_FIELD(m_AttackPaused, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_frag, CWeaponFrag);
PRECACHE_WEAPON_REGISTER(weapon_frag);

CWeaponFrag::CWeaponFrag(void) :
	CBaseHL2MPCombatWeapon()
{
	m_bRedraw = false;
	m_flChargeStartTime = 0.0f;
	m_flChargeAmount = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::Precache(void)
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
	UTIL_PrecacheOther("npc_grenade_frag");
#endif

	PrecacheScriptSound("WeaponFrag.Throw");
	PrecacheScriptSound("WeaponFrag.Roll");
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponFrag::Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	bool fThrewGrenade = false;

	switch (pEvent->event)
	{
	case EVENT_WEAPON_SEQUENCE_FINISHED:
		m_fDrawbackFinished = true;
		break;

	case EVENT_WEAPON_THROW:
		ThrowGrenade(pOwner);
		DecrementAmmo(pOwner);
		fThrewGrenade = true;
		break;

	case EVENT_WEAPON_THROW2:
		RollGrenade(pOwner);
		DecrementAmmo(pOwner);
		fThrewGrenade = true;
		break;

	case EVENT_WEAPON_THROW3:
		LobGrenade(pOwner);
		DecrementAmmo(pOwner);
		fThrewGrenade = true;
		break;

	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}

	if (fThrewGrenade)
	{
		float rethrowDelay = sv_grenade_rethrow_delay.GetFloat();
		m_flNextPrimaryAttack = gpGlobals->curtime + rethrowDelay;
		m_flNextSecondaryAttack = gpGlobals->curtime + rethrowDelay;
		m_flTimeWeaponIdle = FLT_MAX; //NOTE: This is set once the animation has finished up!
	}
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponFrag::Deploy(void)
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;
	ResetCharge();

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponFrag::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;
	ResetCharge();

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponFrag::Reload(void)
{
	if (!HasPrimaryAmmo())
		return false;

	if ((m_bRedraw) && (m_flNextPrimaryAttack <= gpGlobals->curtime) && (m_flNextSecondaryAttack <= gpGlobals->curtime))
	{
		//Redraw the weapon
		SendWeaponAnim(ACT_VM_DRAW);

		//Update our times
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

		//Mark this as done
		m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::SecondaryAttack(void)
{
	if (m_bRedraw)
		return;

	if (!HasPrimaryAmmo())
		return;

	CBaseCombatCharacter* pOwner = GetOwner();

	if (pOwner == NULL)
		return;

	CBasePlayer* pPlayer = ToBasePlayer(pOwner);

	if (pPlayer == NULL)
		return;

	// Note that this is a secondary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_SECONDARY;
	SendWeaponAnim(ACT_VM_PULLBACK_LOW);

	// Start charging if enabled
	if (sv_grenade_throw_charge_enabled.GetBool())
	{
		StartCharge();
	}

	// Don't let weapon idle interfere in the middle of a throw!
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextSecondaryAttack = FLT_MAX;

	// If I'm now out of ammo, switch away
	if (!HasPrimaryAmmo())
	{
		pPlayer->SwitchToNextBestWeapon(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::PrimaryAttack(void)
{
	if (m_bRedraw)
		return;

	CBaseCombatCharacter* pOwner = GetOwner();

	if (pOwner == NULL)
	{
		return;
	}

	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
		return;

	// Note that this is a primary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_PRIMARY;
	SendWeaponAnim(ACT_VM_PULLBACK_HIGH);

	// Start charging if enabled
	if (sv_grenade_lob_charge_enabled.GetBool())
	{
		StartCharge();
	}

	// Put both of these off indefinitely. We do not know how long
	// the player will hold the grenade.
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextPrimaryAttack = FLT_MAX;

	// If I'm now out of ammo, switch away
	if (!HasPrimaryAmmo())
	{
		pPlayer->SwitchToNextBestWeapon(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponFrag::DecrementAmmo(CBaseCombatCharacter* pOwner)
{
	pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::ItemPostFrame(void)
{
	// Update charge amount if charging is enabled for either attack type
	if (m_AttackPaused != GRENADE_PAUSED_NO)
	{
		bool shouldCharge = false;

		if (m_AttackPaused == GRENADE_PAUSED_PRIMARY && sv_grenade_throw_charge_enabled.GetBool())
			shouldCharge = true;
		else if (m_AttackPaused == GRENADE_PAUSED_SECONDARY && sv_grenade_lob_charge_enabled.GetBool())
			shouldCharge = true;

		if (shouldCharge)
			UpdateChargeAmount();
	}

	if (m_fDrawbackFinished)
	{
		CBasePlayer* pOwner = ToBasePlayer(GetOwner());

		if (pOwner)
		{
			switch (m_AttackPaused)
			{
			case GRENADE_PAUSED_PRIMARY:
				if (!(pOwner->m_nButtons & IN_ATTACK))
				{
					SendWeaponAnim(ACT_VM_THROW);
					m_fDrawbackFinished = false;
				}
				break;

			case GRENADE_PAUSED_SECONDARY:
				if (!(pOwner->m_nButtons & IN_ATTACK2))
				{
					//See if we're ducking
					if (pOwner->m_nButtons & IN_DUCK)
					{
						//Send the weapon animation
						SendWeaponAnim(ACT_VM_SECONDARYATTACK);
					}
					else
					{
						//Send the weapon animation
						SendWeaponAnim(ACT_VM_HAULBACK);
					}

					m_fDrawbackFinished = false;
				}
				break;

			default:
				break;
			}
		}
	}

	BaseClass::ItemPostFrame();

	if (m_bRedraw)
	{
		if (IsViewModelSequenceFinished())
		{
			Reload();
		}
	}
}

// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
void CWeaponFrag::CheckThrowPosition(CBasePlayer* pPlayer, const Vector& vecEye, Vector& vecSrc)
{
	trace_t tr;
	float grenadeRadius = sv_grenade_radius.GetFloat();

	UTIL_TraceHull(vecEye, vecSrc, -Vector(grenadeRadius + 2, grenadeRadius + 2, grenadeRadius + 2), Vector(grenadeRadius + 2, grenadeRadius + 2, grenadeRadius + 2),
		pPlayer->PhysicsSolidMaskForEntity(), pPlayer, pPlayer->GetCollisionGroup(), &tr);

	if (tr.DidHit())
	{
		vecSrc = tr.endpos;
	}
}

void DropPrimedFragGrenade(CHL2MP_Player* pPlayer, CBaseCombatWeapon* pGrenade)
{
	CWeaponFrag* pWeaponFrag = dynamic_cast<CWeaponFrag*>(pGrenade);

	if (pWeaponFrag)
	{
		pWeaponFrag->ThrowGrenade(pPlayer);
		pWeaponFrag->DecrementAmmo(pPlayer);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponFrag::ThrowGrenade(CBasePlayer* pPlayer)
{
#ifndef CLIENT_DLL
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors(&vForward, &vRight, NULL);

	float forwardOffset = sv_grenade_forward_offset.GetFloat();
	float rightOffset = sv_grenade_right_offset.GetFloat();
	Vector vecSrc = vecEye + vForward * forwardOffset + vRight * rightOffset;
	CheckThrowPosition(pPlayer, vecEye, vecSrc);

	float upwardComponent = sv_grenade_throw_upward.GetFloat();
	vForward[2] += upwardComponent;

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);

	float throwVelocity;
	if (sv_grenade_throw_charge_enabled.GetBool())
	{
		throwVelocity = GetCurrentChargeVelocity(false); // false = not a lob
	}
	else
	{
		throwVelocity = sv_grenade_throw_velocity.GetFloat(); // ✅ CORRETO: usando valor padrão
	}
	vecThrow += vForward * throwVelocity;

	float angularBase = sv_grenade_throw_angular_base.GetFloat();
	float angularRandom = sv_grenade_throw_angular_random.GetFloat();

	CBaseGrenade* pGrenade = Fraggrenade_Create(vecSrc, vec3_angle, vecThrow,
		AngularImpulse(angularBase, random->RandomInt(-angularRandom, angularRandom), 0),
		pPlayer, sv_grenade_timer.GetFloat(), false);

	if (pGrenade)
	{
		if (pPlayer && pPlayer->m_lifeState != LIFE_ALIVE)
		{
			pPlayer->GetVelocity(&vecThrow, NULL);

			IPhysicsObject* pPhysicsObject = pGrenade->VPhysicsGetObject();
			if (pPhysicsObject)
			{
				pPhysicsObject->SetVelocity(&vecThrow, NULL);
			}
		}

		pGrenade->SetDamage(GetHL2MPWpnData().m_iPlayerDamage);
		pGrenade->SetDamageRadius(sv_grenade_damage_radius.GetFloat());
	}
#endif

	m_bRedraw = true;

	WeaponSound(SINGLE);

	// player "shoot" animation
	pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifdef GAME_DLL
	pPlayer->OnMyWeaponFired(this);
#endif

	// Reset charge after throwing
	ResetCharge();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponFrag::LobGrenade(CBasePlayer* pPlayer)
{
#ifndef CLIENT_DLL
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors(&vForward, &vRight, NULL);

	float forwardOffset = sv_grenade_forward_offset.GetFloat();
	float rightOffset = sv_grenade_right_offset.GetFloat();
	float downOffset = sv_grenade_lob_down_offset.GetFloat();
	Vector vecSrc = vecEye + vForward * forwardOffset + vRight * rightOffset + Vector(0, 0, -downOffset);
	CheckThrowPosition(pPlayer, vecEye, vecSrc);

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);

	float lobVelocity;
	if (sv_grenade_lob_charge_enabled.GetBool())
	{
		lobVelocity = GetCurrentChargeVelocity(true); // true = is a lob
	}
	else
	{
		lobVelocity = sv_grenade_lob_velocity.GetFloat(); // ✅ CORRETO: usando valor padrão
	}

	float upwardVelocity = sv_grenade_lob_upward.GetFloat();
	vecThrow += vForward * lobVelocity + Vector(0, 0, upwardVelocity);

	float angularBase = sv_grenade_lob_angular_base.GetFloat();
	float angularRandom = sv_grenade_lob_angular_random.GetFloat();

	CBaseGrenade* pGrenade = Fraggrenade_Create(vecSrc, vec3_angle, vecThrow,
		AngularImpulse(angularBase, random->RandomInt(-angularRandom, angularRandom), 0),
		pPlayer, sv_grenade_timer.GetFloat(), false);

	if (pGrenade)
	{
		pGrenade->SetDamage(GetHL2MPWpnData().m_iPlayerDamage);
		pGrenade->SetDamageRadius(sv_grenade_damage_radius.GetFloat());
	}
#endif

	WeaponSound(WPN_DOUBLE);

	// player "shoot" animation
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_bRedraw = true;

	// Reset charge after throwing
	ResetCharge();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponFrag::RollGrenade(CBasePlayer* pPlayer)
{
#ifndef CLIENT_DLL
	// BUGBUG: Hardcoded grenade width of 4 - better not change the model :)
	Vector vecSrc;
	pPlayer->CollisionProp()->NormalizedToWorldSpace(Vector(0.5f, 0.5f, 0.0f), &vecSrc);

	float grenadeRadius = sv_grenade_radius.GetFloat();
	vecSrc.z += grenadeRadius;

	Vector vecFacing = pPlayer->BodyDirection2D();
	// no up/down direction
	vecFacing.z = 0;
	VectorNormalize(vecFacing);
	trace_t tr;
	UTIL_TraceLine(vecSrc, vecSrc - Vector(0, 0, 16), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction != 1.0)
	{
		// compute forward vec parallel to floor plane and roll grenade along that
		Vector tangent;
		CrossProduct(vecFacing, tr.plane.normal, tangent);
		CrossProduct(tr.plane.normal, tangent, vecFacing);
	}

	float forwardOffset = sv_grenade_forward_offset.GetFloat();
	vecSrc += (vecFacing * forwardOffset);
	CheckThrowPosition(pPlayer, pPlayer->WorldSpaceCenter(), vecSrc);

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);

	float rollVelocity = sv_grenade_roll_velocity.GetFloat();
	vecThrow += vecFacing * rollVelocity;

	// put it on its side
	QAngle orientation(0, pPlayer->GetLocalAngles().y, -90);
	// roll it
	float rollAngular = sv_grenade_roll_angular.GetFloat();
	AngularImpulse rotSpeed(0, 0, rollAngular);

	CBaseGrenade* pGrenade = Fraggrenade_Create(vecSrc, orientation, vecThrow, rotSpeed,
		pPlayer, sv_grenade_timer.GetFloat(), false);

	if (pGrenade)
	{
		pGrenade->SetDamage(GetHL2MPWpnData().m_iPlayerDamage);
		pGrenade->SetDamageRadius(sv_grenade_damage_radius.GetFloat());
	}

#endif

	WeaponSound(SPECIAL1);

	// player "shoot" animation
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_bRedraw = true;
}

//-----------------------------------------------------------------------------
// Purpose: Charge system implementation
//-----------------------------------------------------------------------------
void CWeaponFrag::StartCharge(void)
{
	m_flChargeStartTime = gpGlobals->curtime;
	m_flChargeAmount = 0.0f;
}

void CWeaponFrag::ResetCharge(void)
{
	m_flChargeStartTime = 0.0f;
	m_flChargeAmount = 0.0f;

	// Clear charge HUD display
	if (sv_grenade_charge_hud.GetBool())
	{
		CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
		if (pPlayer)
		{
			ClientPrint(pPlayer, HUD_PRINTCENTER, ""); // Clear center print
		}
	}
}

void CWeaponFrag::UpdateChargeAmount(void)
{
	if (m_flChargeStartTime <= 0.0f)
		return;

	float chargeTime;

	// Use different charge times based on attack type
	if (m_AttackPaused == GRENADE_PAUSED_PRIMARY)
		chargeTime = sv_grenade_throw_charge_time.GetFloat();
	else if (m_AttackPaused == GRENADE_PAUSED_SECONDARY)
		chargeTime = sv_grenade_lob_charge_time.GetFloat();
	else
		return;

	float elapsed = gpGlobals->curtime - m_flChargeStartTime;

	// Clamp charge amount between 0.0 and 1.0
	m_flChargeAmount = clamp(elapsed / chargeTime, 0.0f, 1.0f);

	// Show charge progress on HUD
	if (sv_grenade_charge_hud.GetBool())
	{
		CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
		if (pPlayer)
		{
			int percentage = (int)(m_flChargeAmount * 100);

			// Create visual progress bar
			char progressBar[32];
			int filledBars = (int)(m_flChargeAmount * 10);

			for (int i = 0; i < 10; i++)
			{
				progressBar[i] = (i < filledBars) ? '|' : '-';
			}
			progressBar[10] = '\0';

			// Different text based on attack type
			const char* attackType = (m_AttackPaused == GRENADE_PAUSED_PRIMARY) ? "THROW" : "LOB";

			ClientPrint(pPlayer, HUD_PRINTCENTER, UTIL_VarArgs("CHARGING %s [%s] %d%%", attackType, progressBar, percentage));
		}
	}
}

float CWeaponFrag::GetCurrentChargeVelocity(bool bIsLob)
{
	bool chargeEnabled = bIsLob ? sv_grenade_lob_charge_enabled.GetBool() : sv_grenade_throw_charge_enabled.GetBool();

	if (!chargeEnabled)
	{
		// Usar valores padrão quando charge está desabilitado
		return bIsLob ? sv_grenade_lob_velocity.GetFloat() : sv_grenade_throw_velocity.GetFloat();
	}

	// Usar sistema de charge com min/max quando habilitado
	float minVel, maxVel;

	if (bIsLob)
	{
		minVel = sv_grenade_lob_velocity_min.GetFloat();
		maxVel = sv_grenade_lob_velocity_max.GetFloat();
	}
	else
	{
		minVel = sv_grenade_throw_velocity_min.GetFloat();
		maxVel = sv_grenade_throw_velocity_max.GetFloat();
	}

	// Linear interpolation entre min e max baseado no charge
	return minVel + (maxVel - minVel) * m_flChargeAmount;
}