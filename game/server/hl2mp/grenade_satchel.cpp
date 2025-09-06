//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "explode.h"
#include "Sprite.h"
#include "grenade_satchel.h"
#include "basegrenade_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SLAM_SPRITE	"sprites/glow1.vmt"
extern ConVar sv_slam_allow_steal;
extern ConVar sv_slam_steal_announce;
extern ConVar sv_slam_steal_range;


ConVar    sk_plr_dmg_satchel("sk_plr_dmg_satchel", "0");
ConVar    sk_npc_dmg_satchel("sk_npc_dmg_satchel", "0");
ConVar    sk_satchel_radius("sk_satchel_radius", "0");

// ==================================================================================================
// NOVAS CVARS PARA CUSTOMIZAÇÃO DA SATCHEL
// ==================================================================================================
ConVar sv_satchel_glow_sprite("sv_satchel_glow_sprite", "sprites/glow1.vmt", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sprite material for the satchel's glow");
ConVar sv_satchel_glow_color("sv_satchel_glow_color", "255,80,0,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Color of the satchel's glow in R,G,B,A format");
ConVar sv_satchel_damage("sv_satchel_damage", "150", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Damage amount for satchel charge explosions (remote detonation)");
ConVar sv_satchel_radius("sv_satchel_radius", "250", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Damage radius for satchel charge explosions (remote detonation)");

// CVARS PARA CORES POR TIME (só funciona quando mp_teamplay está 1)
ConVar sv_satchel_byteams("sv_satchel_byteams", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable team-based colors for satchel when mp_teamplay is 1 (0=disabled, 1=enabled)");
ConVar sv_satchel_rebels_glow_color("sv_satchel_rebels_glow_color", "255,0,0,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Glow color for rebels team satchel in R,G,B,A format");
ConVar sv_satchel_combine_glow_color("sv_satchel_combine_glow_color", "0,100,255,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Glow color for combine team satchel in R,G,B,A format");

// CVARS PARA CONTROLE DO GLOW
ConVar sv_satchel_glow_brightness("sv_satchel_glow_brightness", "255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Brightness of the satchel glow (0-255)");
ConVar sv_satchel_glow_scale("sv_satchel_glow_scale", "0.2", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Scale of the satchel glow sprite");
ConVar sv_satchel_glow_width("sv_satchel_glow_width", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Width/thickness of the satchel glow");
ConVar sv_satchel_glow_strobe("sv_satchel_glow_strobe", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable strobe effect for satchel glow (0=solid, 1=strobe)");
// ==================================================================================================

BEGIN_DATADESC(CSatchelCharge)

DEFINE_FIELD(m_flNextBounceSoundTime, FIELD_TIME),
DEFINE_FIELD(m_bInAir, FIELD_BOOLEAN),
DEFINE_FIELD(m_vLastPosition, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_pMyWeaponSLAM, FIELD_CLASSPTR),
DEFINE_FIELD(m_bIsAttached, FIELD_BOOLEAN),

// Function Pointers
DEFINE_THINKFUNC(SatchelThink),

// Inputs
DEFINE_INPUTFUNC(FIELD_VOID, "Explode", InputExplode),

END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_satchel, CSatchelCharge);

//=========================================================
// Deactivate - do whatever it is we do to an orphaned 
// satchel when we don't want it in the world anymore.
//=========================================================
void CSatchelCharge::Deactivate(void)
{
	AddSolidFlags(FSOLID_NOT_SOLID);
	UTIL_Remove(this);

	if (m_hGlowSprite != NULL)
	{
		UTIL_Remove(m_hGlowSprite);
		m_hGlowSprite = NULL;
	}
}


void CSatchelCharge::Spawn(void)
{
	Precache();
	SetModel("models/Weapons/w_slam.mdl");

	VPhysicsInitNormal(SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, false);
	SetMoveType(MOVETYPE_VPHYSICS);

	SetCollisionGroup(COLLISION_GROUP_WEAPON);

	UTIL_SetSize(this, Vector(-6, -6, -2), Vector(6, 6, 2));

	SetThink(&CSatchelCharge::SatchelThink);
	SetNextThink(gpGlobals->curtime + 0.1f);

	m_flDamage = sk_plr_dmg_satchel.GetFloat();
	m_DmgRadius = sk_satchel_radius.GetFloat();
	m_takedamage = DAMAGE_YES;
	m_iHealth = 1;

	SetGravity(UTIL_ScaleForGravity(560));	// slightly lower gravity
	SetFriction(1.0);
	SetSequence(1);
	SetDamage(sv_satchel_damage.GetInt());

	m_bIsAttached = false;
	m_bInAir = true;
	m_flNextBounceSoundTime = 0;

	m_vLastPosition = vec3_origin;

	m_hGlowSprite = NULL;
	CreateEffects();
}

//-----------------------------------------------------------------------------
// Purpose: Get team colors based on thrower
//-----------------------------------------------------------------------------
void CSatchelCharge::GetTeamColors(int& r, int& g, int& b, int& a)
{
	// Verificar se mp_teamplay está ativo
	ConVar* mp_teamplay = cvar->FindVar("mp_teamplay");
	bool isTeamplayActive = (mp_teamplay && mp_teamplay->GetBool());

	// Se teamplay estiver desativo OU byteams desabilitado, usar cores padrão
	if (!isTeamplayActive || !sv_satchel_byteams.GetBool())
	{
		sscanf(sv_satchel_glow_color.GetString(), "%d,%d,%d,%d", &r, &g, &b, &a);
		r = clamp(r, 0, 255);
		g = clamp(g, 0, 255);
		b = clamp(b, 0, 255);
		a = clamp(a, 0, 255);
		return;
	}

	// Determinar time baseado no thrower (só quando teamplay ativo)
	bool isCombineTeam = false;

	if (GetThrower() && GetThrower()->IsPlayer())
	{
		CBasePlayer* pPlayer = ToBasePlayer(GetThrower());
		if (pPlayer)
		{
			// No HL2DM, team 2 = Combine, team 3 = Rebels
			isCombineTeam = (pPlayer->GetTeamNumber() == 2);
		}
	}

	// Escolher cores baseado no time - REBELS = VERMELHO, COMBINE = AZUL
	const char* colorString;
	if (isCombineTeam)
	{
		colorString = sv_satchel_combine_glow_color.GetString();
	}
	else
	{
		colorString = sv_satchel_rebels_glow_color.GetString();
	}

	sscanf(colorString, "%d,%d,%d,%d", &r, &g, &b, &a);
	r = clamp(r, 0, 255);
	g = clamp(g, 0, 255);
	b = clamp(b, 0, 255);
	a = clamp(a, 0, 255);
}

//-----------------------------------------------------------------------------
// Purpose: Start up any effects for us
//-----------------------------------------------------------------------------
void CSatchelCharge::CreateEffects(void)
{
	// Only do this once
	if (m_hGlowSprite != NULL)
		return;

	// Create a blinking light to show we're an active SLAM
	m_hGlowSprite = CSprite::SpriteCreate(sv_satchel_glow_sprite.GetString(), GetAbsOrigin(), false);
	m_hGlowSprite->SetAttachment(this, 0);

	// Get team colors
	int r, g, b, a;
	GetTeamColors(r, g, b, a);

	// Usar tipo de render e efeito configuráveis
	int renderFx = sv_satchel_glow_strobe.GetBool() ? kRenderFxStrobeFast : kRenderFxNone;
	m_hGlowSprite->SetTransparency(kRenderTransAdd, r, g, b, a, renderFx);

	m_hGlowSprite->SetBrightness(sv_satchel_glow_brightness.GetFloat(), 1.0f);
	m_hGlowSprite->SetScale(sv_satchel_glow_scale.GetFloat(), sv_satchel_glow_width.GetFloat());
	m_hGlowSprite->TurnOn();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output  :
//-----------------------------------------------------------------------------
void CSatchelCharge::InputExplode(inputdata_t& inputdata)
{
	ExplosionCreate(GetAbsOrigin() + Vector(0, 0, 16), GetAbsAngles(), GetThrower(), GetDamage(), 200,
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE, 0.0f, this);

	UTIL_Remove(this);
}


void CSatchelCharge::SatchelThink(void)
{
	// If attached resize so player can pick up off wall
	if (m_bIsAttached)
	{
		UTIL_SetSize(this, Vector(-2, -2, -6), Vector(2, 2, 6));
	}

	// See if I can lose my owner (has dropper moved out of way?)
	// Want do this so owner can shoot the satchel charge
	if (GetOwnerEntity())
	{
		trace_t tr;
		Vector	vUpABit = GetAbsOrigin();
		vUpABit.z += 5.0;

		CBaseEntity* saveOwner = GetOwnerEntity();
		SetOwnerEntity(NULL);
		UTIL_TraceEntity(this, GetAbsOrigin(), vUpABit, MASK_SOLID, &tr);
		if (tr.startsolid || tr.fraction != 1.0)
		{
			SetOwnerEntity(saveOwner);
		}
	}

	// Bounce movement code gets this think stuck occasionally so check if I've 
	// succeeded in moving, otherwise kill my motions.
	else if ((GetAbsOrigin() - m_vLastPosition).LengthSqr() < 1)
	{
		SetAbsVelocity(vec3_origin);

		QAngle angVel = GetLocalAngularVelocity();
		angVel.y = 0;
		SetLocalAngularVelocity(angVel);

		// Clear think function
		SetThink(NULL);
		return;
	}
	m_vLastPosition = GetAbsOrigin();

	StudioFrameAdvance();
	SetNextThink(gpGlobals->curtime + 0.1f);

	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	// Is it attached to a wall?
	if (m_bIsAttached)
	{
		return;
	}
}

void CSatchelCharge::Precache(void)
{
	PrecacheModel("models/Weapons/w_slam.mdl");
	PrecacheModel(sv_satchel_glow_sprite.GetString());
}

void CSatchelCharge::BounceSound(void)
{
	if (gpGlobals->curtime > m_flNextBounceSoundTime)
	{
		m_flNextBounceSoundTime = gpGlobals->curtime + 0.1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CSatchelCharge::CSatchelCharge(void)
{
	m_vLastPosition.Init();
	m_pMyWeaponSLAM = NULL;
}

CSatchelCharge::~CSatchelCharge(void)
{
	if (m_hGlowSprite != NULL)
	{
		UTIL_Remove(m_hGlowSprite);
		m_hGlowSprite = NULL;
	}
}

void CSatchelCharge::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBasePlayer* pPlayer = ToBasePlayer(pActivator);
	if (!pPlayer)
		return;

	if (!sv_slam_allow_steal.GetBool())
		return;

	// Apenas permite uso se o jogador estiver dentro do range de roubo
	float flStealRange = sv_slam_steal_range.GetFloat();
	if ((pPlayer->GetAbsOrigin() - GetAbsOrigin()).Length() > flStealRange)
		return;

	// Sempre permite recuperar/roubar SLAMs
	int currentAmmo = pPlayer->GetAmmoCount("slam");

	// Se já tem o máximo (5), descartar
	if (currentAmmo >= 5)
	{
		EmitSound("Weapon_SLAM.SatchelDetonate");
		if (sv_slam_steal_announce.GetBool())
		{
			ClientPrint(pPlayer, HUD_PRINTCENTER, "*FULL* SLAM deactivated");
		}
		UTIL_Remove(this);
		return;
	}

	// Dar apenas 1 munição
	pPlayer->GiveAmmo(1, "slam", false);
	EmitSound("Player.PickupWeapon");

	if (!pPlayer->Weapon_OwnsThisType("weapon_slam"))
	{
		pPlayer->GiveNamedItem("weapon_slam");
	}

	// Mensagem de HUD aparece somente se sv_slam_steal_announce for 1
	if (sv_slam_steal_announce.GetBool())
	{
		ClientPrint(pPlayer, HUD_PRINTCENTER, "SLAM Stolen");
	}
	UTIL_Remove(this);
}
int CSatchelCharge::OnTakeDamage(const CTakeDamageInfo& info)
{
	// Se recebeu dano suficiente, explodir
	if (m_iHealth <= 0)
	{
		ExplosionCreate(GetAbsOrigin() + Vector(0, 0, 16), GetAbsAngles(), GetThrower(),
			sv_satchel_damage.GetFloat(), sk_satchel_radius.GetFloat(),  // <- era GetInt() e faltava vírgula
			SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE, 0.0f, this);
		UTIL_Remove(this);
		return 0;  // <- adicionar return
	}
	return BaseClass::OnTakeDamage(info);
}