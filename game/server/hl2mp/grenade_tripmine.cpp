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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern const char* g_pModelNameLaser;

ConVar    sk_plr_dmg_tripmine("sk_plr_dmg_tripmine", "0");
ConVar    sk_npc_dmg_tripmine("sk_npc_dmg_tripmine", "0");
ConVar    sk_tripmine_radius("sk_tripmine_radius", "0");

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

// Força remoção de CVars antigas (remover depois de confirmar que funcionou)
CON_COMMAND(sv_tripmine_cleanup_old_cvars, "Remove old tripmine cvars from memory")
{
	ConVar* oldCvar1 = cvar->FindVar("sv_tripmine_laser_brightness");
	ConVar* oldCvar2 = cvar->FindVar("sv_tripmine_rebels_laser_brightness");
	ConVar* oldCvar3 = cvar->FindVar("sv_tripmine_combine_laser_brightness");

	if (oldCvar1) cvar->UnregisterConCommand(oldCvar1);
	if (oldCvar2) cvar->UnregisterConCommand(oldCvar2);
	if (oldCvar3) cvar->UnregisterConCommand(oldCvar3);

	Msg("Old tripmine brightness CVars removed from memory\n");
}
// ==================================================================================================

LINK_ENTITY_TO_CLASS(npc_tripmine, CTripmineGrenade);

BEGIN_DATADESC(CTripmineGrenade)

DEFINE_FIELD(m_hOwner, FIELD_EHANDLE),
DEFINE_FIELD(m_flPowerUp, FIELD_TIME),
DEFINE_FIELD(m_vecDir, FIELD_VECTOR),
DEFINE_FIELD(m_vecEnd, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_flBeamLength, FIELD_FLOAT),
DEFINE_FIELD(m_pBeam, FIELD_CLASSPTR),
DEFINE_FIELD(m_posOwner, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_angleOwner, FIELD_VECTOR),

// Function Pointers
DEFINE_THINKFUNC(WarningThink),
DEFINE_THINKFUNC(PowerupThink),
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
	m_flDamage = sk_plr_dmg_tripmine.GetFloat();
	m_DmgRadius = sk_tripmine_radius.GetFloat();

	ResetSequenceInfo();
	m_flPlaybackRate = 0;

	UTIL_SetSize(this, Vector(-4, -4, -2), Vector(4, 4, 2));

	m_flPowerUp = gpGlobals->curtime + sv_tripmine_arm_delay.GetFloat();

	SetThink(&CTripmineGrenade::PowerupThink);
	SetNextThink(gpGlobals->curtime + 0.2);

	m_takedamage = DAMAGE_YES;

	m_iHealth = 1;

	EmitSound("TripmineGrenade.Place");
	SetDamage(200);

	// Tripmine sits at 90 on wall so rotate back to get m_vecDir
	QAngle angles = GetAbsAngles();
	angles.x -= 90;

	AngleVectors(angles, &m_vecDir);
	m_vecEnd = GetAbsOrigin() + m_vecDir * 2048;

	AddEffects(EF_NOSHADOW);
}

//-----------------------------------------------------------------------------
// Purpose: Get team colors and brightness based on owner - ALPHA usado como brilho
//-----------------------------------------------------------------------------
void CTripmineGrenade::GetTeamColors(int& r, int& g, int& b, int& brightness)
{
	// Verificar se mp_teamplay está ativo
	ConVar* mp_teamplay = cvar->FindVar("mp_teamplay");
	bool isTeamplayActive = (mp_teamplay && mp_teamplay->GetBool());

	// Se teamplay estiver desativo OU byteams desabilitado, usar configuração padrão
	if (!isTeamplayActive || !sv_tripmine_byteams.GetBool())
	{
		// MODO DEATHMATCH - COR LARANJA PADRÃO
		int a; // Alpha = brightness
		sscanf(sv_tripmine_laser_color.GetString(), "%d,%d,%d,%d", &r, &g, &b, &a);
		r = clamp(r, 0, 255);
		g = clamp(g, 0, 255);
		b = clamp(b, 0, 255);
		brightness = clamp(a, 0, 255); // Alpha vira brightness
		return;
	}

	// MODO TEAMPLAY - Determinar time baseado no owner
	bool isCombineTeam = false;

	if (m_hOwner.Get() && m_hOwner.Get()->IsPlayer())
	{
		CBasePlayer* pPlayer = ToBasePlayer(m_hOwner.Get());
		if (pPlayer)
		{
			// No HL2DM, team 2 = Combine (AZUL), team 3 = Rebels (VERMELHO)
			isCombineTeam = (pPlayer->GetTeamNumber() == 2);
		}
	}

	// Escolher configuração baseado no time
	const char* colorString;
	if (isCombineTeam)
	{
		// COMBINE = AZUL
		colorString = sv_tripmine_combine_laser_color.GetString();
	}
	else
	{
		// REBELS = VERMELHO
		colorString = sv_tripmine_rebels_laser_color.GetString();
	}

	int a; // Alpha = brightness
	sscanf(colorString, "%d,%d,%d,%d", &r, &g, &b, &a);
	r = clamp(r, 0, 255);
	g = clamp(g, 0, 255);
	b = clamp(b, 0, 255);
	brightness = clamp(a, 0, 255); // Alpha vira brightness
}


void CTripmineGrenade::Precache(void)
{
	PrecacheModel("models/Weapons/w_slam.mdl");
	PrecacheModel("sprites/laser.vmt"); // fallback sprite para o laser

	PrecacheScriptSound("TripmineGrenade.Place");
	PrecacheScriptSound("TripmineGrenade.Activate");
	PrecacheScriptSound("TripmineGrenade.StopSound");
}


void CTripmineGrenade::WarningThink(void)
{
	// set to power up
	SetThink(&CTripmineGrenade::PowerupThink);
	SetNextThink(gpGlobals->curtime + 1.0f);
}


void CTripmineGrenade::PowerupThink(void)
{
	if (gpGlobals->curtime > m_flPowerUp)
	{
		MakeBeam();
		RemoveSolidFlags(FSOLID_NOT_SOLID);
		m_bIsLive = true;

		// play enabled sound
		EmitSound("TripmineGrenade.Activate");
	}
	SetNextThink(gpGlobals->curtime + 0.1f);
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

	UTIL_TraceLine(GetAbsOrigin(), m_vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	m_flBeamLength = tr.fraction;

	// If I hit a living thing, send the beam through me so it turns on briefly
	// and then blows the living thing up
	CBaseEntity* pEntity = tr.m_pEnt;
	CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pEntity);

	// Draw length is not the beam length if entity is in the way
	float drawLength = tr.fraction;
	if (pBCC)
	{
		SetOwnerEntity(pBCC);
		UTIL_TraceLine(GetAbsOrigin(), m_vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
		m_flBeamLength = tr.fraction;
		SetOwnerEntity(NULL);
	}

	// set to follow laser spot
	SetThink(&CTripmineGrenade::BeamBreakThink);

	// Delay first think slightly so beam has time
	// to appear if person right in front of it
	SetNextThink(gpGlobals->curtime + 1.0f);

	Vector vecTmpEnd = GetLocalOrigin() + m_vecDir * 2048 * drawLength;

	// Obter cores baseadas no time - ALPHA usado como brilho
	int r, g, b, brightness;
	GetTeamColors(r, g, b, brightness);

	// Criar o beam usando sprite padrão do laser ou fallback
	const char* beamSprite = g_pModelNameLaser;
	if (!beamSprite || strlen(beamSprite) == 0)
	{
		beamSprite = "sprites/laser.vmt"; // fallback
	}

	m_pBeam = CBeam::BeamCreate(beamSprite, sv_tripmine_laser_width.GetFloat());
	if (m_pBeam)
	{
		m_pBeam->PointEntInit(vecTmpEnd, this);
		m_pBeam->SetColor(r, g, b);
		m_pBeam->SetScrollRate(25.6);
		m_pBeam->SetBrightness(brightness); // Brightness vem do canal Alpha

		int beamAttach = LookupAttachment("beam_attach");
		if (beamAttach > 0)
		{
			m_pBeam->SetEndAttachment(beamAttach);
		}
		else
		{
			// Se attachment não existir, usar posição direta
			m_pBeam->SetEndPos(GetAbsOrigin());
		}
	}
}


void CTripmineGrenade::BeamBreakThink(void)
{
	// See if I can go solid yet (has dropper moved out of way?)
	if (IsSolidFlagSet(FSOLID_NOT_SOLID))
	{
		trace_t tr;
		Vector	vUpBit = GetAbsOrigin();
		vUpBit.z += 5.0;

		UTIL_TraceEntity(this, GetAbsOrigin(), vUpBit, MASK_SHOT, &tr);
		if (!tr.startsolid && (tr.fraction == 1.0))
		{
			RemoveSolidFlags(FSOLID_NOT_SOLID);
		}
	}

	trace_t tr;

	// NOT MASK_SHOT because we want only simple hit boxes
	UTIL_TraceLine(GetAbsOrigin(), m_vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

	// ALERT( at_console, "%f : %f\n", tr.flFraction, m_flBeamLength );

	// respawn detect. 
	if (!m_pBeam)
	{
		MakeBeam();
		if (tr.m_pEnt)
			m_hOwner = tr.m_pEnt;	// reset owner too
	}


	CBaseEntity* pEntity = tr.m_pEnt;
	CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pEntity);

	if (pBCC || fabs(m_flBeamLength - tr.fraction) > 0.001)
	{
		m_iHealth = 0;
		Event_Killed(CTakeDamageInfo((CBaseEntity*)m_hOwner, this, 100, GIB_NORMAL));
		return;
	}

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
	SetNextThink(gpGlobals->curtime + sv_tripmine_explode_delay.GetFloat());

	EmitSound("TripmineGrenade.StopSound");
}


void CTripmineGrenade::DelayDeathThink(void)
{
	KillBeam();
	trace_t tr;
	UTIL_TraceLine(GetAbsOrigin() + m_vecDir * 8, GetAbsOrigin() - m_vecDir * 64, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	UTIL_ScreenShake(GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START);

	ExplosionCreate(GetAbsOrigin() + m_vecDir * 8, GetAbsAngles(), m_hOwner, GetDamage(), 200,
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE, 0.0f, this);

	UTIL_Remove(this);
}