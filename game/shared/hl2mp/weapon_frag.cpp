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

#define GRENADE_TIMER	2.5f //Seconds

#define GRENADE_PAUSED_NO			0
#define GRENADE_PAUSED_PRIMARY		1
#define GRENADE_PAUSED_SECONDARY	2

#define GRENADE_RADIUS	4.0f // inches

#define GRENADE_DAMAGE_RADIUS 250.0f

#ifdef CLIENT_DLL
#define CWeaponFrag C_WeaponFrag
#endif


// --- Grenade ConVars Section - Paste this block at the top of your weapon_frag.cpp file ---

// Detonation and re-throw timings
ConVar sv_grenade_timer("sv_grenade_timer", "2.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Detonation time for the grenade in seconds. Default is the classic.");
ConVar sv_grenade_rethrow_delay("sv_grenade_rethrow_delay", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Delay before you can throw another grenade. No spamming!");

// Physical properties
ConVar sv_grenade_radius("sv_grenade_radius", "4.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Radius of the grenade itself. Bigger means more boom, but also more 'stuck in a doorway' incidents.");
ConVar sv_grenade_damage_radius("sv_grenade_damage_radius", "250.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Blast damage radius. Go big or go home.");
ConVar sv_grenade_throw_upward("sv_grenade_throw_upward", "0.1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Upward bias for grenade throws (0.0-1.0). Helps clear small obstacles.");

// Charge system settings
ConVar sv_grenade_throw_charge_enable("sv_grenade_throw_charge_enable", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enables charge-up for the primary throw.");
ConVar sv_grenade_lob_charge_enable("sv_grenade_lob_charge_enable", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enables charge-up for the secondary throw (lob/roll).");
ConVar sv_grenade_roll_charge_enable("sv_grenade_roll_charge_enable", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enables charge-up for grenade rolls when ducking.");
ConVar sv_grenade_charge_time("sv_grenade_charge_time", "2.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Max charge-up time for all grenade throws in seconds.");

// HUD ConVars (NOVAS)
ConVar sv_grenade_throw_charge_hud("sv_grenade_throw_charge_hud", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Shows HUD charge bar for primary throw.");
ConVar sv_grenade_lob_charge_hud("sv_grenade_lob_charge_hud", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Shows HUD charge bar for lob throw.");
ConVar sv_grenade_roll_charge_hud("sv_grenade_roll_charge_hud", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Shows HUD charge bar for roll throw.");

// Throw velocities
ConVar sv_grenade_throw_velocity("sv_grenade_throw_velocity", "1200.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Default primary throw velocity (no charge).");
ConVar sv_grenade_lob_velocity("sv_grenade_lob_velocity", "350.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Default secondary throw velocity (no charge).");
ConVar sv_grenade_roll_velocity("sv_grenade_roll_velocity", "700.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Default roll velocity. Spin it to win it.");

ConVar sv_grenade_throw_velocity_min("sv_grenade_throw_velocity_min", "500.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum velocity for a charged primary throw.");
ConVar sv_grenade_throw_velocity_max("sv_grenade_throw_velocity_max", "1200.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum velocity for a charged primary throw.");
ConVar sv_grenade_lob_velocity_min("sv_grenade_lob_velocity_min", "150.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum velocity for a charged secondary lob.");
ConVar sv_grenade_lob_velocity_max("sv_grenade_lob_velocity_max", "350.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum velocity for a charged secondary lob.");
ConVar sv_grenade_roll_velocity_min("sv_grenade_roll_velocity_min", "200.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum velocity for a charged roll.");
ConVar sv_grenade_roll_velocity_max("sv_grenade_roll_velocity_max", "700.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum velocity for a charged roll.");

// Throw offsets and angles
ConVar sv_grenade_forward_offset("sv_grenade_forward_offset", "18.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Forward offset for the throw origin.");
ConVar sv_grenade_right_offset("sv_grenade_right_offset", "8.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Rightward offset for the throw origin.");
ConVar sv_grenade_lob_down_offset("sv_grenade_lob_down_offset", "-8.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Downward offset for the lob throw. 'Aim for the feet' mode.");

// Angular impulses
ConVar sv_grenade_throw_angular_base("sv_grenade_throw_angular_base", "600", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Base angular impulse for primary throw.");
ConVar sv_grenade_throw_angular_random("sv_grenade_throw_angular_random", "1200", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Random angular impulse for primary throw. Adds some chaotic spin.");
ConVar sv_grenade_lob_angular_base("sv_grenade_lob_angular_base", "200", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Base angular impulse for lob.");
ConVar sv_grenade_lob_angular_random("sv_grenade_lob_angular_random", "600", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Random angular impulse for lob.");
ConVar sv_grenade_roll_angular("sv_grenade_roll_angular", "720", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Angular impulse for rolling grenades. Makes it spin like a fidget spinner.");

// --- End of Grenade ConVars Section ---


//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CWeaponFrag: public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS( CWeaponFrag, CBaseHL2MPCombatWeapon );
public:

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponFrag();

	void	Precache( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	DecrementAmmo( CBaseCombatCharacter *pOwner );
	void	ItemPostFrame( void );

	bool	Deploy( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	
	bool	Reload( void );

#ifndef CLIENT_DLL
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

	//void	ThrowGrenade( CBasePlayer *pPlayer );
	void ThrowGrenade(CBasePlayer* pPlayer, float flVelocity);
	bool	IsPrimed( bool ) { return ( m_AttackPaused != 0 );	}
	
private:

	float m_flChargeStartTime; // The moment the player started charging the throw.
	float m_flLastHUDUpdateTime; // To rate-limit HUD updates.

	//void	RollGrenade( CBasePlayer *pPlayer );
	//void	LobGrenade( CBasePlayer *pPlayer );

	void LobGrenade(CBasePlayer* pPlayer, float flVelocity);
	void RollGrenade(CBasePlayer* pPlayer, float flVelocity);
	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
	void    CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc, const QAngle &angles = vec3_angle );

	CNetworkVar( bool,	m_bRedraw );	//Draw the weapon again after throwing a grenade
	
	CNetworkVar( int,	m_AttackPaused );
	CNetworkVar( bool,	m_fDrawbackFinished );

	CWeaponFrag( const CWeaponFrag & );

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

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFrag, DT_WeaponFrag )

BEGIN_NETWORK_TABLE( CWeaponFrag, DT_WeaponFrag )

#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bRedraw ) ),
	RecvPropBool( RECVINFO( m_fDrawbackFinished ) ),
	RecvPropInt( RECVINFO( m_AttackPaused ) ),
#else
	SendPropBool( SENDINFO( m_bRedraw ) ),
	SendPropBool( SENDINFO( m_fDrawbackFinished ) ),
	SendPropInt( SENDINFO( m_AttackPaused ) ),
#endif
	
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponFrag )
	DEFINE_PRED_FIELD( m_bRedraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_fDrawbackFinished, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_AttackPaused, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_frag, CWeaponFrag );
PRECACHE_WEAPON_REGISTER(weapon_frag);

CWeaponFrag::CWeaponFrag( void ) :
	CBaseHL2MPCombatWeapon()
{
	m_bRedraw = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::Precache( void )
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "npc_grenade_frag" );
#endif

	PrecacheScriptSound( "WeaponFrag.Throw" );
	PrecacheScriptSound( "WeaponFrag.Roll" );
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
// A sua função Operator_HandleAnimEvent deve ficar assim
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
		// Corrigido: Agora usa a ConVar de velocidade padrão
		ThrowGrenade(pOwner, sv_grenade_throw_velocity.GetFloat());
		DecrementAmmo(pOwner);
		fThrewGrenade = true;
		break;

	case EVENT_WEAPON_THROW2:
		// Corrigido: Agora usa a ConVar de velocidade padrão
		RollGrenade(pOwner, sv_grenade_roll_velocity.GetFloat());
		DecrementAmmo(pOwner);
		fThrewGrenade = true;
		break;

	case EVENT_WEAPON_THROW3:
		// Corrigido: Agora usa a ConVar de velocidade padrão
		LobGrenade(pOwner, sv_grenade_lob_velocity.GetFloat());
		DecrementAmmo(pOwner);
		fThrewGrenade = true;
		break;

	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}

#define RETHROW_DELAY	0.5
	if (fThrewGrenade)
	{
		// Corrigido: Agora usa a ConVar de atraso
		m_flNextPrimaryAttack = gpGlobals->curtime + sv_grenade_rethrow_delay.GetFloat();
		m_flNextSecondaryAttack = gpGlobals->curtime + sv_grenade_rethrow_delay.GetFloat();
		m_flTimeWeaponIdle = FLT_MAX;
	}
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponFrag::Deploy( void )
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponFrag::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	// ** NOVA LINHA ADICIONADA: Resetar o estado de ataque ao guardar a arma **
	m_AttackPaused = GRENADE_PAUSED_NO;

	m_bRedraw = false;
	m_fDrawbackFinished = false;

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponFrag::Reload( void )
{
	if ( !HasPrimaryAmmo() )
		return false;

	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

		//Mark this as done
		m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// --- Substitution for SecondaryAttack ---
void CWeaponFrag::SecondaryAttack(void)
{
	if (m_bRedraw)
		return;

	CBaseCombatCharacter* pOwner = GetOwner();

	if (pOwner == NULL)
		return;

	CBasePlayer* pPlayer = ToBasePlayer(pOwner);

	if (pPlayer == NULL)
		return;

	if (!HasPrimaryAmmo())
	{
		pPlayer->SwitchToNextBestWeapon(this);
		return;
	}

	// Note that this is a secondary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_SECONDARY;

	// Store the time we started charging
	m_flChargeStartTime = gpGlobals->curtime;

	// See if we're ducking to determine the animation
	if (pPlayer->m_nButtons & IN_DUCK)
	{
		SendWeaponAnim(ACT_VM_PULLBACK_LOW);
	}
	else
	{
		SendWeaponAnim(ACT_VM_PULLBACK_LOW);
	}

	// Don't let weapon idle interfere in the middle of a throw!
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextSecondaryAttack = FLT_MAX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// --- Substitution for PrimaryAttack ---
void CWeaponFrag::PrimaryAttack(void)
{
	if (m_bRedraw)
		return;

	CBaseCombatCharacter* pOwner = GetOwner();

	if (pOwner == NULL)
		return;

	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
		return;

	if (!HasPrimaryAmmo())
	{
		pPlayer->SwitchToNextBestWeapon(this);
		return;
	}

	// Set a flag to mark we are now charging the grenade
	m_AttackPaused = GRENADE_PAUSED_PRIMARY;

	// Store the time we started charging
	m_flChargeStartTime = gpGlobals->curtime;

	SendWeaponAnim(ACT_VM_PULLBACK_HIGH);

	// Put both of these off indefinitely. We do not know how long
	// the player will hold the grenade.
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextPrimaryAttack = FLT_MAX;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponFrag::DecrementAmmo( CBaseCombatCharacter *pOwner )
{
	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

// --- Substitution for ItemPostFrame ---
void CWeaponFrag::ItemPostFrame(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
	{
		BaseClass::ItemPostFrame();
		return;
	}

	// Determine if charge is enabled for the current attack
	bool isDucking = (pOwner->m_nButtons & IN_DUCK);
	bool bChargeEnabled = false;
	const char* throwType = "";
	bool showHud = false;

	if (m_AttackPaused == GRENADE_PAUSED_PRIMARY)
	{
		bChargeEnabled = sv_grenade_throw_charge_enable.GetBool();
		throwType = "Throw";
		showHud = sv_grenade_throw_charge_hud.GetBool();
	}
	else if (m_AttackPaused == GRENADE_PAUSED_SECONDARY)
	{
		if (isDucking)
		{
			bChargeEnabled = sv_grenade_roll_charge_enable.GetBool();
			throwType = "Roll";
			showHud = sv_grenade_roll_charge_hud.GetBool();
		}
		else
		{
			bChargeEnabled = sv_grenade_lob_charge_enable.GetBool();
			throwType = "Lob";
			showHud = sv_grenade_lob_charge_hud.GetBool();
		}
	}

	// If a charge is enabled and paused, handle the charge logic
	if (bChargeEnabled && m_AttackPaused != GRENADE_PAUSED_NO)
	{
		// Calculate the charge percentage
		float flChargeTime = gpGlobals->curtime - m_flChargeStartTime;
		float flMaxChargeTime = sv_grenade_charge_time.GetFloat();
		float flChargePercent = clamp(flChargeTime / flMaxChargeTime, 0.0f, 1.0f);

		// Show HUD bar if enabled
		if (showHud && (gpGlobals->curtime - m_flLastHUDUpdateTime) > 0.05f)
		{
			char hudText[256];
			int barLength = 20;
			int filled = (int)(flChargePercent * barLength);

			char bar[22];
			for (int i = 0; i < barLength; i++)
			{
				bar[i] = (i < filled) ? '|' : ' ';
			}
			bar[barLength] = '\0';

			Q_snprintf(hudText, sizeof(hudText), "Charging %s [%s] %d%%", throwType, bar, (int)(flChargePercent * 100));
			ClientPrint(pOwner, HUD_PRINTCENTER, hudText);
			m_flLastHUDUpdateTime = gpGlobals->curtime;
		}

		// Check if the player has released the attack button
		bool bReleasedPrimary = (m_AttackPaused == GRENADE_PAUSED_PRIMARY && !(pOwner->m_nButtons & IN_ATTACK));
		bool bReleasedSecondary = (m_AttackPaused == GRENADE_PAUSED_SECONDARY && !(pOwner->m_nButtons & IN_ATTACK2));

		if (bReleasedPrimary || bReleasedSecondary)
		{
			float minVel = 0.0f;
			float maxVel = 0.0f;

			if (m_AttackPaused == GRENADE_PAUSED_PRIMARY)
			{
				minVel = sv_grenade_throw_velocity_min.GetFloat();
				maxVel = sv_grenade_throw_velocity_max.GetFloat();
				ThrowGrenade(pOwner, Lerp(flChargePercent, minVel, maxVel));
			}
			else // Secondary
			{
				if (isDucking)
				{
					minVel = sv_grenade_roll_velocity_min.GetFloat();
					maxVel = sv_grenade_roll_velocity_max.GetFloat();
					RollGrenade(pOwner, Lerp(flChargePercent, minVel, maxVel));
				}
				else
				{
					// Use default lob velocity as minimum
					minVel = sv_grenade_lob_velocity.GetFloat();
					maxVel = sv_grenade_lob_velocity_max.GetFloat();
					LobGrenade(pOwner, Lerp(flChargePercent, minVel, maxVel));
				}
			}

			// We are done with the charge, reset the state
			m_AttackPaused = GRENADE_PAUSED_NO;
			m_flChargeStartTime = 0.0f;
			m_flNextPrimaryAttack = gpGlobals->curtime + sv_grenade_rethrow_delay.GetFloat();
			m_flNextSecondaryAttack = gpGlobals->curtime + sv_grenade_rethrow_delay.GetFloat();
			pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
			m_bRedraw = true;
			ClientPrint(pOwner, HUD_PRINTCENTER, "\n"); // Clear HUD message
		}
	}
	else if (m_AttackPaused != GRENADE_PAUSED_NO)
	{
		// Handle the case where the button was released but charge is disabled
		bool bReleasedPrimary = (m_AttackPaused == GRENADE_PAUSED_PRIMARY && !(pOwner->m_nButtons & IN_ATTACK));
		bool bReleasedSecondary = (m_AttackPaused == GRENADE_PAUSED_SECONDARY && !(pOwner->m_nButtons & IN_ATTACK2));

		if (bReleasedPrimary || bReleasedSecondary)
		{
			if (m_AttackPaused == GRENADE_PAUSED_PRIMARY)
			{
				ThrowGrenade(pOwner, sv_grenade_throw_velocity.GetFloat());
			}
			else // Secondary
			{
				if (isDucking)
				{
					RollGrenade(pOwner, sv_grenade_roll_velocity.GetFloat());
				}
				else
				{
					LobGrenade(pOwner, sv_grenade_lob_velocity.GetFloat());
				}
			}

			m_AttackPaused = GRENADE_PAUSED_NO;
			m_flChargeStartTime = 0.0f;
			m_flNextPrimaryAttack = gpGlobals->curtime + sv_grenade_rethrow_delay.GetFloat();
			m_flNextSecondaryAttack = gpGlobals->curtime + sv_grenade_rethrow_delay.GetFloat();
			pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
			m_bRedraw = true;
		}
	}

	BaseClass::ItemPostFrame();

	if (m_bRedraw && IsViewModelSequenceFinished())
	{
		Reload();
	}
}

	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
void CWeaponFrag::CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc, const QAngle &angles )
{
	// Compute an extended AABB that takes into account the requested grenade rotation.
	// This will prevent grenade from going through nearby solids when model initially intersects with any.
	matrix3x4_t rotation;
	AngleMatrix( angles, rotation );
	Vector mins, maxs;
	RotateAABB( rotation, -Vector( GRENADE_RADIUS + 2, GRENADE_RADIUS + 2, 2 ), Vector( GRENADE_RADIUS + 2, GRENADE_RADIUS + 2, GRENADE_RADIUS * 2 + 2 ), mins, maxs );

	trace_t tr;

	UTIL_TraceHull( vecEye, vecSrc, mins, maxs, pPlayer->PhysicsSolidMaskForEntity(), pPlayer, pPlayer->GetCollisionGroup(), &tr );

	if ( tr.DidHit() )
	{
		vecSrc = tr.endpos;
	}
}

// Substitua a função por esta nova versão
void DropPrimedFragGrenade(CHL2MP_Player* pPlayer, CBaseCombatWeapon* pGrenade)
{
	CWeaponFrag* pWeaponFrag = dynamic_cast<CWeaponFrag*>(pGrenade);

	if (pWeaponFrag)
	{
		// Use a ConVar de velocidade padrão para o arremesso
		pWeaponFrag->ThrowGrenade(pPlayer, sv_grenade_throw_velocity.GetFloat());
		pWeaponFrag->DecrementAmmo(pPlayer);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
// Substitua a função por esta nova versão.
void CWeaponFrag::ThrowGrenade(CBasePlayer* pPlayer, float flVelocity)
{
#ifndef CLIENT_DLL
	Vector vecEye = pPlayer->EyePosition();
	Vector vForward, vRight;

	pPlayer->EyeVectors(&vForward, &vRight, NULL);
	Vector vecSrc = vecEye + vForward * sv_grenade_forward_offset.GetFloat() + vRight * sv_grenade_right_offset.GetFloat();
	CheckThrowPosition(pPlayer, vecEye, vecSrc);

	vForward[2] += sv_grenade_throw_upward.GetFloat();

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);
	vecThrow += vForward * flVelocity;

	// Use ConVars for angular impulse
	AngularImpulse angImpulse(sv_grenade_throw_angular_base.GetInt(), random->RandomInt(-sv_grenade_throw_angular_random.GetInt(), sv_grenade_throw_angular_random.GetInt()), 0);

	CBaseGrenade* pGrenade = Fraggrenade_Create(vecSrc, vec3_angle, vecThrow, angImpulse, pPlayer, sv_grenade_timer.GetFloat(), false);

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

	WeaponSound(SINGLE);
	pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifdef GAME_DLL
	pPlayer->OnMyWeaponFired(this);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
// Substitua a função por esta nova versão
void CWeaponFrag::LobGrenade(CBasePlayer* pPlayer, float flVelocity)
{
#ifndef CLIENT_DLL
	Vector vecEye = pPlayer->EyePosition();
	Vector vForward, vRight;

	pPlayer->EyeVectors(&vForward, &vRight, NULL);
	Vector vecSrc = vecEye + vForward * sv_grenade_forward_offset.GetFloat() + vRight * sv_grenade_right_offset.GetFloat() + Vector(0, 0, sv_grenade_lob_down_offset.GetFloat());
	CheckThrowPosition(pPlayer, vecEye, vecSrc);

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);
	vecThrow += vForward * flVelocity + Vector(0, 0, 50); // Apenas um pequeno impulso para cima no lob

	// Use ConVars for angular impulse
	AngularImpulse angImpulse(sv_grenade_lob_angular_base.GetInt(), random->RandomInt(-sv_grenade_lob_angular_random.GetInt(), sv_grenade_lob_angular_random.GetInt()), 0);

	CBaseGrenade* pGrenade = Fraggrenade_Create(vecSrc, vec3_angle, vecThrow, angImpulse, pPlayer, sv_grenade_timer.GetFloat(), false);

	if (pGrenade)
	{
		pGrenade->SetDamage(GetHL2MPWpnData().m_iPlayerDamage);
		pGrenade->SetDamageRadius(sv_grenade_damage_radius.GetFloat());
	}
#endif

	WeaponSound(WPN_DOUBLE);
	pPlayer->SetAnimation(PLAYER_ATTACK1);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
// Substitua a função por esta nova versão
void CWeaponFrag::RollGrenade(CBasePlayer* pPlayer, float flVelocity)
{
#ifndef CLIENT_DLL
	Vector vecSrc;
	pPlayer->CollisionProp()->NormalizedToWorldSpace(Vector(0.5f, 0.5f, 0.0f), &vecSrc);
	vecSrc.z += sv_grenade_radius.GetFloat(); // Use a ConVar para a altura do raio

	Vector vecFacing = pPlayer->BodyDirection2D();
	vecFacing.z = 0;
	VectorNormalize(vecFacing);

	trace_t tr;
	UTIL_TraceLine(vecSrc, vecSrc - Vector(0, 0, 16), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction != 1.0)
	{
		Vector tangent;
		CrossProduct(vecFacing, tr.plane.normal, tangent);
		CrossProduct(tr.plane.normal, tangent, vecFacing);
	}
	vecSrc += (vecFacing * sv_grenade_forward_offset.GetFloat()); // Use a ConVar para o offset frontal

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);
	vecThrow += vecFacing * flVelocity;

	QAngle orientation(0, pPlayer->GetLocalAngles().y, -90);
	AngularImpulse rotSpeed(0, 0, sv_grenade_roll_angular.GetInt()); // Use a ConVar para a velocidade angular
	CheckThrowPosition(pPlayer, pPlayer->WorldSpaceCenter(), vecSrc, orientation);
	CBaseGrenade* pGrenade = Fraggrenade_Create(vecSrc, orientation, vecThrow, rotSpeed, pPlayer, sv_grenade_timer.GetFloat(), false);

	if (pGrenade)
	{
		pGrenade->SetDamage(GetHL2MPWpnData().m_iPlayerDamage);
		pGrenade->SetDamageRadius(sv_grenade_damage_radius.GetFloat());
	}
#endif

	CDisablePredictionFiltering disablePred;
	WeaponSound(SPECIAL1);
	pPlayer->SetAnimation(PLAYER_ATTACK1);
}

