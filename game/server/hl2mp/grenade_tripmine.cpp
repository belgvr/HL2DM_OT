//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the tripmine grenade.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "shake.h"
#include "grenade_tripmine.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "explode.h"
#include "hl2mp_gamerules.h"

#include "ammodef.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TRIPMINE_POWERUP_DELAY 2.0f

extern const char* g_pModelNameLaser;

extern ConVar sv_slam_allow_steal;
extern ConVar sv_slam_steal_announce;
extern ConVar sv_slam_steal_range;

ConVar    sk_plr_dmg_tripmine("sk_plr_dmg_tripmine", "200");
ConVar    sk_npc_dmg_tripmine("sk_npc_dmg_tripmine", "0");
ConVar    sk_tripmine_radius("sk_tripmine_radius", "200");


// ==================================================================================================
// CVARS PARA CUSTOMIZAÇÃO DO TRIPMINE - ALPHA CONTROLA O BRILHO
// ==================================================================================================
ConVar sv_tripmine_laser_color("sv_tripmine_laser_color", "255,80,0,128", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Color of the tripmine laser in R,G,B,A format (A = brightness 0-255)");
ConVar sv_tripmine_laser_width("sv_tripmine_laser_width", "0.35", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Width/thickness of the tripmine laser beam");

// CVARS PARA CORES POR TIME (só funciona quando mp_teamplay está 1) - ALPHA CONTROLA O BRILHO
ConVar sv_tripmine_byteams("sv_tripmine_byteams", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable team-based colors for tripmine when mp_teamplay is 1 (0=disabled, 1=enabled)");
ConVar sv_tripmine_rebels_laser_color("sv_tripmine_rebels_laser_color", "255,0,0,128", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Laser color for rebels team tripmine in R,G,B,A format (A = brightness 0-255)");
ConVar sv_tripmine_combine_laser_color("sv_tripmine_combine_laser_color", "0,100,255,128", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Laser color for combine team tripmine in R,G,B,A format (A = brightness 0-255)");

// CVARS PARA CONTROLE DE TIMING
ConVar sv_tripmine_arm_delay("sv_tripmine_arm_delay", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Time in seconds before tripmine arms and activates laser (default: 2.0)");
ConVar sv_tripmine_explode_delay("sv_tripmine_explode_delay", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Time in seconds between laser break detection and explosion (default: 0.25)");

LINK_ENTITY_TO_CLASS(npc_tripmine, CTripmineGrenade);

BEGIN_DATADESC(CTripmineGrenade)

DEFINE_FIELD(m_hOwner, FIELD_EHANDLE),
// DEFINE_FIELD( m_flPowerUp,	FIELD_TIME ),
DEFINE_FIELD(m_vecDir, FIELD_VECTOR),
DEFINE_FIELD(m_vecEnd, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_flBeamLength, FIELD_FLOAT),
DEFINE_FIELD(m_pBeam, FIELD_CLASSPTR),
DEFINE_FIELD(m_posOwner, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_angleOwner, FIELD_VECTOR),

// Function Pointers
// DEFINE_THINKFUNC( WarningThink ),
// DEFINE_THINKFUNC( PowerupThink ),
DEFINE_THINKFUNC(PowerUp),
DEFINE_THINKFUNC(BeamBreakThink),
DEFINE_THINKFUNC(DelayDeathThink),

END_DATADESC()

CTripmineGrenade::CTripmineGrenade()
{
	m_vecDir.Init();
	m_vecEnd.Init();
	m_posOwner.Init();
	m_angleOwner.Init();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor to clean up the beam
//-----------------------------------------------------------------------------
CTripmineGrenade::~CTripmineGrenade()
{
	KillBeam();
}

void CTripmineGrenade::Spawn(void)
{
	Precache();
	// motor
	SetMoveType(MOVETYPE_FLY);
	SetSolid(SOLID_BBOX);
	SetModel("models/Weapons/w_slam.mdl");

	IPhysicsObject* pObject = VPhysicsInitNormal(SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, true);
	pObject->EnableMotion(false);
	SetCollisionGroup(COLLISION_GROUP_WEAPON);

	SetCycle(0.0f);
	m_nBody = 3;

	ResetSequenceInfo();
	m_flPlaybackRate = 0;

	UTIL_SetSize(this, Vector(-4, -4, -2), Vector(4, 4, 2));

	SetThink(&ThisClass::PowerUp);
	//SetNextThink( gpGlobals->curtime + TRIPMINE_POWERUP_DELAY );
	SetNextThink(gpGlobals->curtime + sv_tripmine_arm_delay.GetFloat());

	m_takedamage = DAMAGE_YES;

	m_iHealth = 1;

	EmitSound("TripmineGrenade.Place");
	SetDamage(sk_plr_dmg_tripmine.GetFloat());
	SetDamageRadius(sk_tripmine_radius.GetFloat());

	// Tripmine sits at 90 on wall so rotate back to get m_vecDir
	QAngle angles = GetAbsAngles();
	angles.x -= 90;

	AngleVectors(angles, &m_vecDir);
	m_vecEnd = GetAbsOrigin() + m_vecDir * 32768;

	AddEffects(EF_NOSHADOW);

	m_pAttachedObject = nullptr;
}


void CTripmineGrenade::GetLaserColor(int& r, int& g, int& b, int& a) const
{
	const char* colorStr = sv_tripmine_laser_color.GetString();

	// Se for por time, escolha a cor do time
	if (sv_tripmine_byteams.GetBool())
	{
		CBasePlayer* pPlayer = ToBasePlayer(m_hOwner);
		if (pPlayer)
		{
			int team = pPlayer->GetTeamNumber();
			if (team == TEAM_REBELS)
				colorStr = sv_tripmine_rebels_laser_color.GetString();
			else if (team == TEAM_COMBINE)
				colorStr = sv_tripmine_combine_laser_color.GetString();
		}
	}

	// Parse R,G,B,A
	sscanf(colorStr, "%d,%d,%d,%d", &r, &g, &b, &a);
}

float CTripmineGrenade::GetLaserWidth() const
{
	return sv_tripmine_laser_width.GetFloat();
}

void CTripmineGrenade::Precache(void)
{
	PrecacheModel("models/Weapons/w_slam.mdl");

	PrecacheScriptSound("TripmineGrenade.Place");
	PrecacheScriptSound("TripmineGrenade.Activate");
}


void CTripmineGrenade::PowerUp(void)
{
	MakeBeam();
	RemoveSolidFlags(FSOLID_NOT_SOLID);
	m_bIsLive = true;

	// play enabled sound
	EmitSound("TripmineGrenade.Activate");

	CBasePlayer* pPlayer = ToBasePlayer(m_hOwner);
	if (pPlayer && pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
	{
		SetThink(&CTripmineGrenade::DelayDeathThink);
	}
}


void CTripmineGrenade::KillBeam(void)
{
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = NULL;
	}
}


void CTripmineGrenade::MakeBeam(void)
{
	trace_t tr;

	int beamAttach = LookupAttachment("beam_attach");
	Vector vecBeamOrigin;

	// Get the actual position of the beam attachment point
	GetAttachment(beamAttach, vecBeamOrigin);

	UTIL_TraceLine(vecBeamOrigin, m_vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	m_flBeamLength = tr.fraction;

	if (tr.startsolid || tr.allsolid)
	{
		// Detonate the tripmine if it is inside a brush

		SetThink(&CTripmineGrenade::Detonate);
		SetNextThink(gpGlobals->curtime + 0.1f);
		return;
	}

	// If I hit a living thing, send the beam through me so it turns on briefly
	// and then blows the living thing up
	CBaseEntity* pEntity = tr.m_pEnt;
	CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pEntity);

	// Draw length is not the beam length if entity is in the way

	if (pBCC)
	{
		SetOwnerEntity(pBCC);
		UTIL_TraceLine(vecBeamOrigin, m_vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		SetOwnerEntity(NULL);

	}

	m_flBeamLength = tr.fraction;

	// set to follow laser spot
	SetThink(&CTripmineGrenade::BeamBreakThink);

	// Delay first think slightly so beam has time
	// to appear if person right in front of it
	SetNextThink(gpGlobals->curtime + 1.0f);

	/*m_pBeam = CBeam::BeamCreate( g_pModelNameLaser, 0.35 );
	m_pBeam->PointEntInit( tr.endpos, this );
	m_pBeam->SetColor( 255, 55, 52 );
	m_pBeam->SetScrollRate( 25.6 );
	m_pBeam->SetBrightness( 64 );*/

	int r = 255, g = 80, b = 0, a = 128;
	GetLaserColor(r, g, b, a);

	m_pBeam = CBeam::BeamCreate(g_pModelNameLaser, GetLaserWidth());
	m_pBeam->PointEntInit(tr.endpos, this);
	m_pBeam->SetColor(r, g, b);
	m_pBeam->SetScrollRate(25.6);
	m_pBeam->SetBrightness(a); // Usando alpha como brilho

	m_pBeam->SetEndAttachment(beamAttach);
}

void CTripmineGrenade::AttachToEntity(const CBaseEntity* entity)
{
	Assert(m_pAttachedObject == NULL);
	m_pAttachedObject = entity;
	m_vecOldPosAttachedObject = entity->GetAbsOrigin();
	m_vecOldAngAttachedObject = entity->GetAbsAngles();
}

void CTripmineGrenade::BeamBreakThink(void)
{
	// See if I can go solid yet (has dropper moved out of way?)
	if (IsSolidFlagSet(FSOLID_NOT_SOLID))
	{
		trace_t tr;
		Vector vUpBit;

		// Get the position of the "beam_attach" attachment
		int beamAttach = LookupAttachment("beam_attach");
		GetAttachment(beamAttach, vUpBit);

		// Move the attachment point slightly up to avoid clipping
		vUpBit.z += 5.0f;

		// Perform the trace from the beam attachment upwards
		UTIL_TraceEntity(this, vUpBit, vUpBit, MASK_SHOT, &tr);
		if (!tr.startsolid && (tr.fraction == 1.0))
		{
			RemoveSolidFlags(FSOLID_NOT_SOLID);
		}
	}

	trace_t tr;

	// Get the position of the "beam_attach" attachment
	Vector vecBeamOrigin;
	int beamAttach = LookupAttachment("beam_attach");
	GetAttachment(beamAttach, vecBeamOrigin);

	// Perform the trace from the beam attachment to the beam's end point
	UTIL_TraceLine(vecBeamOrigin, m_vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

	// respawn detect
	if (!m_pBeam)
	{
		MakeBeam();
		if (tr.m_pEnt)
			m_hOwner = tr.m_pEnt;  // reset owner too
	}

	CBaseEntity* pEntity = tr.m_pEnt;
	CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pEntity);

	// If beam hits a character or if the beam length has changed, detonate
	if (pBCC || fabs(m_flBeamLength - tr.fraction) > 0.001)
	{
		m_iHealth = 0;
		Event_Killed(CTakeDamageInfo((CBaseEntity*)m_hOwner, this, 100, GIB_NORMAL));
		return;
	}

	// Check if the attached object has moved, if so, detonate
	if (m_pAttachedObject &&
		(!VectorsAreEqual(m_vecOldPosAttachedObject, m_pAttachedObject->GetAbsOrigin(), 1.0f)
			|| !QAnglesAreEqual(m_vecOldAngAttachedObject, m_pAttachedObject->GetAbsAngles(), 1.0f)))
	{
		SetHealth(0);
		Event_Killed(CTakeDamageInfo((CBaseEntity*)m_hOwner, this, 100, GIB_NORMAL));
		return;
	}

	// Keep checking for beam breaks
	SetNextThink(gpGlobals->curtime + 0.05f);
}

#if 0 // FIXME: OnTakeDamage_Alive() is no longer called now that base grenade derives from CBaseAnimating
int CTripmineGrenade::OnTakeDamage_Alive(const CTakeDamageInfo& info)
{
	if (gpGlobals->curtime < m_flPowerUp && info.GetDamage() < m_iHealth)
	{
		// disable
		// Create( "weapon_tripmine", GetLocalOrigin() + m_vecDir * 24, GetAngles() );
		SetThink(&CTripmineGrenade::SUB_Remove);
		SetNextThink(gpGlobals->curtime + 0.1f);
		KillBeam();
		return FALSE;
	}
	return BaseClass::OnTakeDamage_Alive(info);
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CTripmineGrenade::Event_Killed(const CTakeDamageInfo& info)
{
	m_takedamage = DAMAGE_NO;

	SetThink(&CTripmineGrenade::DelayDeathThink);
	//SetNextThink( gpGlobals->curtime + 0.25 );
	SetNextThink(gpGlobals->curtime + sv_tripmine_explode_delay.GetFloat());

	EmitSound("TripmineGrenade.StopSound");
}


void CTripmineGrenade::DelayDeathThink(void)
{
	KillBeam();
	trace_t tr;
	UTIL_TraceLine(GetAbsOrigin() + m_vecDir * 8, GetAbsOrigin() - m_vecDir * 64, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	UTIL_ScreenShake(GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START);

	ExplosionCreate(GetAbsOrigin() + m_vecDir * 8, GetAbsAngles(), m_hOwner, GetDamage(), GetDamageRadius(),
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE, 0.0f, this);

	UTIL_Remove(this);
}

void CTripmineGrenade::OnPhysGunPickup(CBasePlayer* pPhysGunUser, PhysGunPickup_t reason)
{
	m_iHealth = 0;
	Event_Killed(CTakeDamageInfo((CBaseEntity*)m_hOwner, this, 100, GIB_NORMAL));
	BaseClass::OnPhysGunPickup(pPhysGunUser, reason);
}

void CTripmineGrenade::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBasePlayer* pPlayer = ToBasePlayer(pActivator);
	if (!pPlayer)
		return;

	if (!sv_slam_allow_steal.GetBool())
		return;

	// SÓ PERMITE USO SE A TRIPMINE ESTIVER TOTALMENTE ARMADA
	if (!m_bIsLive)
		return;

	// Apenas permite uso se o jogador estiver dentro do range de roubo
	float flStealRange = sv_slam_steal_range.GetFloat();
	if ((pPlayer->GetAbsOrigin() - GetAbsOrigin()).Length() > flStealRange)
		return;

	int currentAmmo = pPlayer->GetAmmoCount("slam");

	if (currentAmmo >= 5)
	{
		EmitSound("Weapon_SLAM.SatchelDetonate");
		if (sv_slam_steal_announce.GetBool())
		{
			ClientPrint(pPlayer, HUD_PRINTCENTER, "*FULL* SLAM deactivated");
		}
		KillBeam(); // Precisamos remover o feixe antes de remover a entidade
		UTIL_Remove(this);
		return;
	}

	EmitSound("Player.PickupWeapon");

	// LÓGICA CORRIGIDA AQUI
	if (!pPlayer->Weapon_OwnsThisType("weapon_slam"))
	{
		// Primeiro, dá a arma para o jogador
		pPlayer->GiveNamedItem("weapon_slam");

		// Depois, DEFINE a munição como 1 para evitar que ele ganhe a munição padrão
		int iAmmoIndex = GetAmmoDef()->Index("slam");
		if (iAmmoIndex != -1)
		{
			pPlayer->SetAmmoCount(1, iAmmoIndex);
		}
	}
	else
	{
		// Se o jogador já tem a arma, apenas adiciona 1 de munição
		pPlayer->GiveAmmo(1, "slam", false);
	}

	// Mensagem de HUD aparece somente se sv_slam_steal_announce for 1
	if (sv_slam_steal_announce.GetBool())
	{
		ClientPrint(pPlayer, HUD_PRINTCENTER, "SLAM Stolen");
	}

	KillBeam(); // Remova o feixe antes de remover a Tripmine
	UTIL_Remove(this);
}