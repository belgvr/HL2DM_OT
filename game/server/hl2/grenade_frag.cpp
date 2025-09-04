//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basegrenade_shared.h"

#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "grenade_frag.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define FRAG_GRENADE_BLIP_FREQUENCY			1.0f
#define FRAG_GRENADE_BLIP_FAST_FREQUENCY	0.3f

#define FRAG_GRENADE_GRACE_TIME_AFTER_PICKUP 1.5f
#define FRAG_GRENADE_WARN_TIME 1.5f

const float GRENADE_COEFFICIENT_OF_RESTITUTION = 0.2f;

ConVar sk_plr_dmg_fraggrenade("sk_plr_dmg_fraggrenade", "0");
ConVar sk_npc_dmg_fraggrenade("sk_npc_dmg_fraggrenade", "0");
ConVar sk_fraggrenade_radius("sk_fraggrenade_radius", "0");

// ==================================================================================================
// INÍCIO DAS MODIFICAÇÕES: NOVAS VARIAVEIS DE CONSOLE PARA CUSTOMIZAÇÃO
// ==================================================================================================
ConVar sv_grenade_frag_glow_sprite("sv_grenade_frag_glow_sprite", "sprites/glow01.vmt", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sprite material for the frag grenade's glow.");
ConVar sv_grenade_frag_trail_sprite("sv_grenade_frag_trail_sprite", "sprites/laser.vmt", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sprite material for the frag grenade's trail.");
ConVar sv_grenade_frag_glow_color("sv_grenade_frag_glow_color", "255,80,0,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Color of the frag grenade's glow in R,G,B,A format.");
ConVar sv_grenade_frag_trail_color("sv_grenade_frag_trail_color", "255,80,0,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Color of the frag grenade's trail in R,G,B,A format.");

// NOVAS VARIAVEIS DE CONSOLE PARA O RASTRO (TRAIL)
ConVar sv_grenade_frag_trail_lifetime("sv_grenade_frag_trail_lifetime", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Lifetime in seconds of the grenade trail, affecting its length.");
ConVar sv_grenade_frag_trail_startwidth("sv_grenade_frag_trail_startwidth", "8.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Starting width of the grenade trail.");
ConVar sv_grenade_frag_trail_endwidth("sv_grenade_frag_trail_endwidth", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Ending width of the grenade trail.");

// NOVAS VARIAVEIS DE CONSOLE PARA CORES POR TIME (só funciona quando mp_teamplay está 1)
ConVar sv_grenade_frag_byteams("sv_grenade_frag_byteams", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable team-based colors for frag grenades when mp_teamplay is 1 (0=disabled, 1=enabled)");
ConVar sv_grenade_frag_rebels_trail_color("sv_grenade_frag_rebels_trail_color", "255,0,0,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Trail color for rebels team grenades in R,G,B,A format");
ConVar sv_grenade_frag_rebels_glow_color("sv_grenade_frag_rebels_glow_color", "255,0,0,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Glow color for rebels team grenades in R,G,B,A format");
ConVar sv_grenade_frag_combine_trail_color("sv_grenade_frag_combine_trail_color", "0,100,255,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Trail color for combine team grenades in R,G,B,A format");
ConVar sv_grenade_frag_combine_glow_color("sv_grenade_frag_combine_glow_color", "0,100,255,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Glow color for combine team grenades in R,G,B,A format");
// ==================================================================================================
// FIM DAS MODIFICAÇÕES
// ==================================================================================================

extern ConVar mp_teamplay;

#define GRENADE_MODEL "models/Weapons/w_grenade.mdl"

class CGrenadeFrag : public CBaseGrenade
{
	DECLARE_CLASS(CGrenadeFrag, CBaseGrenade);

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	~CGrenadeFrag(void);

public:
	void	Spawn(void);
	void	OnRestore(void);
	void	Precache(void);
	bool	CreateVPhysics(void);
	void	CreateEffects(void);
	void	SetTimer(float detonateDelay, float warnDelay);
	void	SetVelocity(const Vector& velocity, const AngularImpulse& angVelocity);
	int		OnTakeDamage(const CTakeDamageInfo& inputInfo);
	void	BlipSound() { EmitSound("Grenade.Blip"); }
	void	DelayThink();
	void	VPhysicsUpdate(IPhysicsObject* pPhysics);
	void	OnPhysGunPickup(CBasePlayer* pPhysGunUser, PhysGunPickup_t reason);
	void	SetCombineSpawned(bool combineSpawned) { m_combineSpawned = combineSpawned; }
	bool	IsCombineSpawned(void) const { return m_combineSpawned; }
	void	SetPunted(bool punt) { m_punted = punt; }
	bool	WasPunted(void) const { return m_punted; }

	// this function only used in episodic.

	void	InputSetTimer(inputdata_t& inputdata);

	void ChangeTeamOwnership(CBasePlayer* pNewOwner);
	float GetOriginalTimerLength() { return m_flOriginalTimer; }


private:
	void	GetTeamColors(int& r, int& g, int& b, int& a, bool isTrail);
	float m_flOriginalTimer; // Armazenar timer original

protected:
	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;

	float	m_flNextBlipTime;
	bool	m_inSolid;
	bool	m_combineSpawned;
	bool	m_punted;

};

LINK_ENTITY_TO_CLASS(npc_grenade_frag, CGrenadeFrag);

BEGIN_DATADESC(CGrenadeFrag)

// Fields
DEFINE_FIELD(m_pMainGlow, FIELD_EHANDLE),
DEFINE_FIELD(m_pGlowTrail, FIELD_EHANDLE),
DEFINE_FIELD(m_flNextBlipTime, FIELD_TIME),
DEFINE_FIELD(m_inSolid, FIELD_BOOLEAN),
DEFINE_FIELD(m_combineSpawned, FIELD_BOOLEAN),
DEFINE_FIELD(m_punted, FIELD_BOOLEAN),
DEFINE_FIELD(m_flOriginalTimer, FIELD_FLOAT), // ← ADICIONAR ESTA LINHA

// Function Pointers
DEFINE_THINKFUNC(DelayThink),

// Inputs
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetTimer", InputSetTimer),



END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGrenadeFrag::~CGrenadeFrag(void)
{
}

void CGrenadeFrag::Spawn(void)
{
	Precache();

	SetModel(GRENADE_MODEL);

	if (GetOwnerEntity() && GetOwnerEntity()->IsPlayer())
	{
		m_flDamage = sk_plr_dmg_fraggrenade.GetFloat();
		m_DmgRadius = sk_fraggrenade_radius.GetFloat();
	}
	else
	{
		m_flDamage = sk_npc_dmg_fraggrenade.GetFloat();
		m_DmgRadius = sk_fraggrenade_radius.GetFloat();
	}

	m_takedamage = DAMAGE_YES;
	m_iHealth = 1;

	SetSize(-Vector(4, 4, 4), Vector(4, 4, 4));
	SetCollisionGroup(COLLISION_GROUP_WEAPON);
	CreateVPhysics();

	BlipSound();
	m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FREQUENCY;

	AddSolidFlags(FSOLID_NOT_STANDABLE);

	m_combineSpawned = false;
	m_punted = false;

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeFrag::OnRestore(void)
{
	// If we were primed and ready to detonate, put FX on us.
	if (m_flDetonateTime > 0)
		CreateEffects();

	BaseClass::OnRestore();
}

//-----------------------------------------------------------------------------
// Purpose: Get team colors based on owner
//-----------------------------------------------------------------------------
void CGrenadeFrag::GetTeamColors(int& r, int& g, int& b, int& a, bool isTrail)
{
	// Verificar se mp_teamplay está ativo
	ConVar* mp_teamplay = cvar->FindVar("mp_teamplay");
	bool isTeamplayActive = (mp_teamplay && mp_teamplay->GetBool());

	// Se teamplay estiver desativo OU byteams desabilitado, usar cores padrão
	if (!isTeamplayActive || !sv_grenade_frag_byteams.GetBool())
	{
		const char* colorString = isTrail ? sv_grenade_frag_trail_color.GetString() : sv_grenade_frag_glow_color.GetString();
		sscanf(colorString, "%d,%d,%d,%d", &r, &g, &b, &a);
		r = clamp(r, 0, 255);
		g = clamp(g, 0, 255);
		b = clamp(b, 0, 255);
		a = clamp(a, 0, 255);
		return;
	}

	// Determinar time baseado no owner (só quando teamplay ativo)
	bool isCombineTeam = false;

	if (GetOwnerEntity() && GetOwnerEntity()->IsPlayer())
	{
		CBasePlayer* pPlayer = ToBasePlayer(GetOwnerEntity());
		if (pPlayer)
		{
			// No HL2DM, team 2 = Rebels, team 3 = Combine
			isCombineTeam = (pPlayer->GetTeamNumber() == 3);
		}
	}
	else if (m_combineSpawned)
	{
		isCombineTeam = true;
	}

	// Escolher cores baseado no time - REBELS = VERMELHO, COMBINE = AZUL
	const char* colorString;
	if (isCombineTeam)
	{
		colorString = isTrail ? sv_grenade_frag_rebels_trail_color.GetString() : sv_grenade_frag_rebels_glow_color.GetString();
	}
	else
	{
		colorString = isTrail ? sv_grenade_frag_combine_trail_color.GetString() : sv_grenade_frag_combine_glow_color.GetString();
	}

	sscanf(colorString, "%d,%d,%d,%d", &r, &g, &b, &a);
	r = clamp(r, 0, 255);
	g = clamp(g, 0, 255);
	b = clamp(b, 0, 255);
	a = clamp(a, 0, 255);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// ==================================================================================================
// INÍCIO DAS MODIFICAÇÕES: Função CreateEffects modificada para usar cores por time
// ==================================================================================================
void CGrenadeFrag::CreateEffects(void)
{
	// Start up the eye glow
	if (!m_pMainGlow.Get())
	{
		m_pMainGlow = CSprite::SpriteCreate(sv_grenade_frag_glow_sprite.GetString(), GetLocalOrigin(), false);
	}

	//int	nAttachment = LookupAttachment("fuse");
	Vector attachmentpos(0, 0, 8.5);
	QAngle attachmentangle(0, 0, 90);
	VectorAngles(attachmentpos, attachmentangle);

	if (m_pMainGlow != NULL)
	{
		//m_pMainGlow->FollowEntity(this);
		//m_pMainGlow->SetAttachment(this, nAttachment);

		int r, g, b, a;
		GetTeamColors(r, g, b, a, false); // false = glow color
		m_pMainGlow->SetTransparency(kRenderGlow, r, g, b, a, kRenderFxNoDissipation);

		m_pMainGlow->SetScale(0.2f);
		m_pMainGlow->SetGlowProxySize(4.0f);
		m_pMainGlow->SetParent(this);

		m_pMainGlow->SetLocalOrigin(attachmentpos);
		m_pMainGlow->SetLocalAngles(attachmentangle);

		m_pMainGlow->SetMoveType(MOVETYPE_NONE);
	}

	// Start up the eye trail
	if (!m_pGlowTrail.Get())
	//{
	//	m_pGlowTrail = CSpriteTrail::SpriteTrailCreate(sv_grenade_frag_trail_sprite.GetString(), GetLocalOrigin(), false);
	//}
		m_pGlowTrail = CSpriteTrail::SpriteTrailCreate(sv_grenade_frag_trail_sprite.GetString(), GetLocalOrigin(), false);


	if (m_pGlowTrail != NULL)
	{
		//m_pGlowTrail->FollowEntity(this);
		//m_pGlowTrail->SetAttachment(this, nAttachment);

		int r, g, b, a;
		GetTeamColors(r, g, b, a, true); // true = trail color
		m_pGlowTrail->SetTransparency(kRenderTransAdd, r, g, b, a, kRenderFxNone);

		m_pGlowTrail->SetStartWidth(sv_grenade_frag_trail_startwidth.GetFloat());
		m_pGlowTrail->SetEndWidth(sv_grenade_frag_trail_endwidth.GetFloat());
		m_pGlowTrail->SetLifeTime(sv_grenade_frag_trail_lifetime.GetFloat());
		m_pGlowTrail->SetParent(this);

		m_pGlowTrail->SetLocalOrigin(attachmentpos);
		m_pGlowTrail->SetLocalAngles(attachmentangle);

		m_pGlowTrail->SetMoveType(MOVETYPE_NONE);
	}
}
// ==================================================================================================
// FIM DAS MODIFICAÇÕES
// ==================================================================================================


bool CGrenadeFrag::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal(SOLID_BBOX, 0, false);
	return true;
}

// this will hit only things that are in newCollisionGroup, but NOT in collisionGroupAlreadyChecked
class CTraceFilterCollisionGroupDelta : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE(CTraceFilterCollisionGroupDelta);

	CTraceFilterCollisionGroupDelta(const IHandleEntity* passentity, int collisionGroupAlreadyChecked, int newCollisionGroup)
		: m_pPassEnt(passentity), m_collisionGroupAlreadyChecked(collisionGroupAlreadyChecked), m_newCollisionGroup(newCollisionGroup)
	{
	}

	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (!PassServerEntityFilter(pHandleEntity, m_pPassEnt))
			return false;
		CBaseEntity* pEntity = EntityFromEntityHandle(pHandleEntity);

		if (pEntity)
		{
			if (g_pGameRules->ShouldCollide(m_collisionGroupAlreadyChecked, pEntity->GetCollisionGroup()))
				return false;
			if (g_pGameRules->ShouldCollide(m_newCollisionGroup, pEntity->GetCollisionGroup()))
				return true;
		}

		return false;
	}

protected:
	const IHandleEntity* m_pPassEnt;
	int		m_collisionGroupAlreadyChecked;
	int		m_newCollisionGroup;
};

void CGrenadeFrag::VPhysicsUpdate(IPhysicsObject* pPhysics)
{
	BaseClass::VPhysicsUpdate(pPhysics);
	Vector vel;
	AngularImpulse angVel;
	pPhysics->GetVelocity(&vel, &angVel);

	Vector start = GetAbsOrigin();
	// find all entities that my collision group wouldn't hit, but COLLISION_GROUP_NONE would and bounce off of them as a ray cast
	CTraceFilterCollisionGroupDelta filter(this, GetCollisionGroup(), COLLISION_GROUP_NONE);
	trace_t tr;

	// UNDONE: Hull won't work with hitboxes - hits outer hull.  But the whole point of this test is to hit hitboxes.
#if 0
	UTIL_TraceHull(start, start + vel * gpGlobals->frametime, CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), CONTENTS_HITBOX | CONTENTS_MONSTER | CONTENTS_SOLID, &filter, &tr);
#else
	UTIL_TraceLine(start, start + vel * gpGlobals->frametime, CONTENTS_HITBOX | CONTENTS_MONSTER | CONTENTS_SOLID, &filter, &tr);
#endif
	if (tr.startsolid)
	{
		if (!m_inSolid)
		{
			// UNDONE: Do a better contact solution that uses relative velocity?
			vel *= -GRENADE_COEFFICIENT_OF_RESTITUTION; // bounce backwards
			pPhysics->SetVelocity(&vel, NULL);
		}
		m_inSolid = true;
		return;
	}
	m_inSolid = false;
	if (tr.DidHit())
	{
		Vector dir = vel;
		VectorNormalize(dir);
		// send a tiny amount of damage so the character will react to getting bonked
		CTakeDamageInfo info(this, GetThrower(), pPhysics->GetMass() * vel, GetAbsOrigin(), 0.1f, DMG_CRUSH);
		tr.m_pEnt->TakeDamage(info);

		// reflect velocity around normal
		vel = -2.0f * tr.plane.normal * DotProduct(vel, tr.plane.normal) + vel;

		// absorb 80% in impact
		vel *= GRENADE_COEFFICIENT_OF_RESTITUTION;
		angVel *= -0.5f;
		pPhysics->SetVelocity(&vel, &angVel);
	}
}

// ==================================================================================================
// INÍCIO DAS MODIFICAÇÕES: Função Precache modificada
// ==================================================================================================
void CGrenadeFrag::Precache(void)
{
	PrecacheModel(GRENADE_MODEL);

	PrecacheScriptSound("Grenade.Blip");

	// MODIFICADO: Carrega os sprites a partir das ConVars
	PrecacheModel(sv_grenade_frag_glow_sprite.GetString());
	PrecacheModel(sv_grenade_frag_trail_sprite.GetString());

	BaseClass::Precache();
}
// ==================================================================================================
// FIM DAS MODIFICAÇÕES
// ==================================================================================================

void CGrenadeFrag::SetTimer(float detonateDelay, float warnDelay)
{
	// Salvar timer original na primeira vez
	if (m_flOriginalTimer <= 0)
	{
		m_flOriginalTimer = detonateDelay;
	}

	m_flDetonateTime = gpGlobals->curtime + detonateDelay;
	m_flWarnAITime = gpGlobals->curtime + warnDelay;
	SetThink(&CGrenadeFrag::DelayThink);
	SetNextThink(gpGlobals->curtime);

	CreateEffects();
}

void CGrenadeFrag::OnPhysGunPickup(CBasePlayer* pPhysGunUser, PhysGunPickup_t reason)
{
	// NOVA FUNCIONALIDADE: Verificar mudança de time
	ChangeTeamOwnership(pPhysGunUser);

	SetThrower(pPhysGunUser);

#ifdef HL2MP
	SetTimer(FRAG_GRENADE_GRACE_TIME_AFTER_PICKUP, FRAG_GRENADE_GRACE_TIME_AFTER_PICKUP / 2);

	BlipSound();
	m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FAST_FREQUENCY;
	m_bHasWarnedAI = true;
#else
	if (IsX360())
	{
		// Give 'em a couple of seconds to aim and throw. 
		SetTimer(2.0f, 1.0f);
		BlipSound();
		m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FAST_FREQUENCY;
	}
#endif

#ifdef HL2_EPISODIC
	SetPunted(true);
#endif

	BaseClass::OnPhysGunPickup(pPhysGunUser, reason);
}

void CGrenadeFrag::DelayThink()
{
	if (gpGlobals->curtime > m_flDetonateTime)
	{
		Detonate();
		return;
	}

	if (!m_bHasWarnedAI && gpGlobals->curtime >= m_flWarnAITime)
	{
#if !defined( CLIENT_DLL )
		CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this);
#endif
		m_bHasWarnedAI = true;
	}

	if (gpGlobals->curtime > m_flNextBlipTime)
	{
		BlipSound();

		if (m_bHasWarnedAI)
		{
			m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FAST_FREQUENCY;
		}
		else
		{
			m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FREQUENCY;
		}
	}

	SetNextThink(gpGlobals->curtime + 0.1);
}

void CGrenadeFrag::SetVelocity(const Vector& velocity, const AngularImpulse& angVelocity)
{
	IPhysicsObject* pPhysicsObject = VPhysicsGetObject();
	if (pPhysicsObject)
	{
		pPhysicsObject->AddVelocity(&velocity, &angVelocity);
	}
}

int CGrenadeFrag::OnTakeDamage(const CTakeDamageInfo& inputInfo)
{
	// Manually apply vphysics because BaseCombatCharacter takedamage doesn't call back to CBaseEntity OnTakeDamage
	VPhysicsTakeDamage(inputInfo);

	// Grenades only suffer blast damage and burn damage.
	if (!(inputInfo.GetDamageType() & (DMG_BLAST | DMG_BURN)))
		return 0;

	return BaseClass::OnTakeDamage(inputInfo);
}


void CGrenadeFrag::InputSetTimer(inputdata_t& inputdata)
{
	SetTimer(inputdata.value.Float(), inputdata.value.Float() - FRAG_GRENADE_WARN_TIME);
}

CBaseGrenade* Fraggrenade_Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner, float timer, bool combineSpawned)
{
	// Don't set the owner here, or the player can't interact with grenades he's thrown
	CGrenadeFrag* pGrenade = (CGrenadeFrag*)CBaseEntity::Create("npc_grenade_frag", position, angles, pOwner);

	pGrenade->SetTimer(timer, timer - FRAG_GRENADE_WARN_TIME);
	pGrenade->SetVelocity(velocity, angVelocity);
	pGrenade->SetThrower(ToBaseCombatCharacter(pOwner));
	pGrenade->m_takedamage = DAMAGE_EVENTS_ONLY;
	pGrenade->SetCombineSpawned(combineSpawned);

	return pGrenade;
}

bool Fraggrenade_WasPunted(const CBaseEntity* pEntity)
{
	const CGrenadeFrag* pFrag = dynamic_cast<const CGrenadeFrag*>(pEntity);
	if (pFrag)
	{
		return pFrag->WasPunted();
	}

	return false;
}

bool Fraggrenade_WasCreatedByCombine(const CBaseEntity* pEntity)
{
	const CGrenadeFrag* pFrag = dynamic_cast<const CGrenadeFrag*>(pEntity);
	if (pFrag)
	{
		return pFrag->IsCombineSpawned();
	}

	return false;
}

void CGrenadeFrag::ChangeTeamOwnership(CBasePlayer* pNewOwner)
{
	ConVar* mp_teamplay = cvar->FindVar("mp_teamplay");
	if (!mp_teamplay || !mp_teamplay->GetBool())
		return; // Só funciona em teamplay

	CBasePlayer* pOriginalOwner = ToBasePlayer(GetOwnerEntity());

	if (!pOriginalOwner || !pNewOwner)
		return;

	// Verificar se são de times diferentes
	if (pOriginalOwner->GetTeamNumber() != pNewOwner->GetTeamNumber())
	{
		// Trocar dono
		SetOwnerEntity(pNewOwner);
		SetThrower(pNewOwner);

		// Resetar timer da granada para o valor original
		if (m_flOriginalTimer > 0)
		{
			SetTimer(m_flOriginalTimer, m_flOriginalTimer - FRAG_GRENADE_WARN_TIME);
		}

		// Recrear efeitos visuais com nova cor do time
		if (m_pMainGlow.Get())
		{
			m_pMainGlow->Remove();
			m_pMainGlow = NULL;
		}
		if (m_pGlowTrail.Get())
		{
			m_pGlowTrail->Remove();
			m_pGlowTrail = NULL;
		}

		// Criar novos efeitos com cor do novo time
		CreateEffects();

		// Som de mudança de time
		EmitSound("Grenade.Blip"); // Ou crie um som específico

		// Debug/feedback
		DevMsg("Granada trocou de time: %s -> %s\n",
			pOriginalOwner->GetPlayerName(),
			pNewOwner->GetPlayerName());
	}
}
