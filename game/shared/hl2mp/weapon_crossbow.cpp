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
#include "IEffects.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "beam_shared.h"
#include "func_break.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "effect_dispatch_data.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define BOLT_MODEL			"models/crossbow_bolt.mdl"
#define BOLT_MODEL	"models/crossbow_bolt.mdl"

#define BOLT_AIR_VELOCITY	3500
#define BOLT_WATER_VELOCITY	1500
#define	BOLT_SKIN_NORMAL	0
#define BOLT_SKIN_GLOW		1

// ==================================================================================================
// INÍCIO DAS MODIFICAÇÕES: NOVAS VARIAVEIS DE CONSOLE PARA CROSSBOW TRAIL
// ==================================================================================================
ConVar sv_crossbow_trail_enable("sv_crossbow_trail_enable", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable/disable crossbow bolt trails (0=disabled, 1=enabled)");
ConVar sv_crossbow_trail_sprite("sv_crossbow_trail_sprite", "sprites/laser.vmt", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sprite material for the crossbow bolt's trail.");
ConVar sv_crossbow_trail_color("sv_crossbow_trail_color", "255,80,0,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Color of the crossbow bolt's trail in R,G,B,A format.");
ConVar sv_crossbow_trail_lifetime("sv_crossbow_trail_lifetime", "3.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Lifetime in seconds of the crossbow bolt trail, affecting its length.");
ConVar sv_crossbow_trail_startwidth("sv_crossbow_trail_startwidth", "8.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Starting width of the crossbow bolt trail.");
ConVar sv_crossbow_trail_endwidth("sv_crossbow_trail_endwidth", "4.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Ending width of the crossbow bolt trail.");

// VARIAVEIS DE CONSOLE PARA CORES POR TIME (só funciona quando mp_teamplay está 1)
ConVar sv_crossbow_trail_byteams("sv_crossbow_trail_byteams", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable team-based colors for crossbow bolt trails when mp_teamplay is 1 (0=disabled, 1=enabled)");
ConVar sv_crossbow_trail_rebels_color("sv_crossbow_trail_rebels_color", "255,0,0,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Trail color for rebels team crossbow bolts in R,G,B,A format");
ConVar sv_crossbow_trail_combine_color("sv_crossbow_trail_combine_color", "0,100,255,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Trail color for combine team crossbow bolts in R,G,B,A format");
ConVar sv_crossbow_trail_linger("sv_crossbow_trail_linger", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Whether the trail should linger after the bolt is destroyed until its lifetime (as defined in sv_crossbow_trail_lifetime) (0=no, 1=yes)");

ConVar sv_crossbow_bounce_new_sound("sv_crossbow_bounce_new_sound", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Use new sound when crossbow bolt bounces (0=old sound, 1=new sound)");
// ==================================================================================================
// FIM DAS MODIFICAÇÕES
// ==================================================================================================

#ifndef CLIENT_DLL

extern ConVar sk_plr_dmg_crossbow;
extern ConVar sk_npc_dmg_crossbow;
extern ConVar teamplay;

void TE_StickyBolt(IRecipientFilter& filter, float delay, Vector vecDirection, const Vector* origin);

//-----------------------------------------------------------------------------
// Crossbow Bolt
//-----------------------------------------------------------------------------
class CCrossbowBolt : public CBaseCombatCharacter
{
	DECLARE_CLASS(CCrossbowBolt, CBaseCombatCharacter);

public:
	CCrossbowBolt() { m_bHasBounced = false; };
	~CCrossbowBolt();

	Class_T Classify(void) { return CLASS_NONE; }
	void ApplyTrailColor(void);

	// Remova as funções antigas que não são mais usadas pela BoltTouch
	// void FadeTrailImmediately(void);
	// void DetachTrail(void);
	// virtual void UpdateOnRemove(void);


public:
	void Spawn(void);
	void Precache(void);
	void BubbleThink(void);
	void BoltTouch(CBaseEntity* pOther);
	void DelayedRemoveThink(void);
	bool CreateVPhysics(void);
	unsigned int PhysicsSolidMaskForEntity() const;
	static CCrossbowBolt* BoltCreate(const Vector& vecOrigin, const QAngle& angAngles, int iDamage, CBasePlayer* pentOwner = NULL);


protected:
	bool CreateSprites(void);
	void GetTrailColors(int& r, int& g, int& b, int& a);

	CHandle<CSprite> m_pGlowSprite;
	CHandle<CSpriteTrail> m_pBoltTrail;

	int m_iDamage;
	bool m_bHasBounced;

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};



LINK_ENTITY_TO_CLASS(crossbow_bolt, CCrossbowBolt);

BEGIN_DATADESC(CCrossbowBolt)
// Function Pointers
DEFINE_FUNCTION(BubbleThink),
DEFINE_FUNCTION(BoltTouch),
DEFINE_FUNCTION(DelayedRemoveThink),

// These are recreated on reload, they don't need storage
DEFINE_FIELD(m_pGlowSprite, FIELD_EHANDLE),
DEFINE_FIELD(m_pBoltTrail, FIELD_EHANDLE),
END_DATADESC()


IMPLEMENT_SERVERCLASS_ST(CCrossbowBolt, DT_CrossbowBolt)
END_SEND_TABLE()

CCrossbowBolt* CCrossbowBolt::BoltCreate(const Vector& vecOrigin, const QAngle& angAngles, int iDamage, CBasePlayer* pentOwner)
{
	// Create a new entity with CCrossbowBolt private data
	CCrossbowBolt* pBolt = (CCrossbowBolt*)CreateEntityByName("crossbow_bolt");
	UTIL_SetOrigin(pBolt, vecOrigin);
	pBolt->SetAbsAngles(angAngles);
	pBolt->Spawn();
	pBolt->SetOwnerEntity(pentOwner);

	pBolt->m_iDamage = iDamage;

	// NOVO: Aplicar cor do trail DEPOIS que o owner foi definido
	pBolt->ApplyTrailColor();

	return pBolt;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCrossbowBolt::~CCrossbowBolt(void)
{
	if (m_pGlowSprite)
	{
		UTIL_Remove(m_pGlowSprite);
	}

	// Verificar se trail existe antes de tentar remover
	if (m_pBoltTrail)
	{
		UTIL_Remove(m_pBoltTrail);
	}
}


void CCrossbowBolt::ApplyTrailColor(void)
{
	// Verificar se trail está habilitado E se existe
	if (!sv_crossbow_trail_enable.GetBool() || !m_pBoltTrail)
		return;

	int r, g, b, a;
	GetTrailColors(r, g, b, a);

	m_pBoltTrail->SetTransparency(kRenderTransAdd, r, g, b, a, kRenderFxNone);
}


//-----------------------------------------------------------------------------
// Purpose: Versão SIMPLIFICADA da GetTrailColors (sem debug excessivo)
//-----------------------------------------------------------------------------
void CCrossbowBolt::GetTrailColors(int& r, int& g, int& b, int& a)
{
	// Cor padrão
	const char* defaultColor = sv_crossbow_trail_color.GetString();
	sscanf(defaultColor, "%d,%d,%d,%d", &r, &g, &b, &a);

	// Verificações
	if (!sv_crossbow_trail_byteams.GetBool())
		return;

	ConVar* mp_teamplay = cvar->FindVar("mp_teamplay");
	if (!mp_teamplay || !mp_teamplay->GetBool())
		return;

	CBasePlayer* pPlayer = ToBasePlayer(GetOwnerEntity());
	if (!pPlayer)
		return;

	int teamNum = pPlayer->GetTeamNumber();

	const char* colorString = nullptr;

	if (teamNum == 2) // Combine
	{
		colorString = sv_crossbow_trail_combine_color.GetString();
	}
	else if (teamNum == 3) // Rebels  
	{
		colorString = sv_crossbow_trail_rebels_color.GetString();
	}

	if (colorString && strlen(colorString) > 0)
	{
		sscanf(colorString, "%d,%d,%d,%d", &r, &g, &b, &a);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCrossbowBolt::CreateVPhysics(void)
{
	// Create the object in the physics system
	VPhysicsInitNormal(SOLID_BBOX, FSOLID_NOT_STANDABLE, false);

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
unsigned int CCrossbowBolt::PhysicsSolidMaskForEntity() const
{
	return (BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX) & ~CONTENTS_GRATE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
// ==================================================================================================
// INÍCIO DAS MODIFICAÇÕES: Função CreateSprites modificada para incluir trail
// ==================================================================================================
bool CCrossbowBolt::CreateSprites(void)
{
	// Start up the eye glow
	m_pGlowSprite = CSprite::SpriteCreate("sprites/light_glow02_noz.vmt", GetLocalOrigin(), false);

	if (m_pGlowSprite != NULL)
	{
		m_pGlowSprite->FollowEntity(this);
		m_pGlowSprite->SetTransparency(kRenderGlow, 255, 255, 255, 128, kRenderFxNoDissipation);
		m_pGlowSprite->SetScale(0.2f);
		m_pGlowSprite->TurnOff();
	}

	// NOVO: Criar o trail do crossbow bolt APENAS se estiver habilitado
	if (sv_crossbow_trail_enable.GetBool())
	{
		m_pBoltTrail = CSpriteTrail::SpriteTrailCreate(sv_crossbow_trail_sprite.GetString(), GetLocalOrigin(), false);

		if (m_pBoltTrail != NULL)
		{
			m_pBoltTrail->FollowEntity(this);

			// COR TEMPORÁRIA - será substituída quando o owner for definido
			m_pBoltTrail->SetTransparency(kRenderTransAdd, 255, 80, 0, 255, kRenderFxNone);

			m_pBoltTrail->SetStartWidth(sv_crossbow_trail_startwidth.GetFloat());
			m_pBoltTrail->SetEndWidth(sv_crossbow_trail_endwidth.GetFloat());
			m_pBoltTrail->SetLifeTime(sv_crossbow_trail_lifetime.GetFloat());
		}
	}
	else
	{
		// Trail desabilitado - setar como NULL
		m_pBoltTrail = NULL;
	}

	return true;
}


// ==================================================================================================
// FIM DAS MODIFICAÇÕES
// ==================================================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrossbowBolt::Spawn(void)
{
	Precache();

	SetModel(BOLT_MODEL);
	SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
	UTIL_SetSize(this, -Vector(1, 1, 1), Vector(1, 1, 1));
	SetSolid(SOLID_BBOX);
	SetGravity(0.05f);

	// Make sure we're updated if we're underwater
	UpdateWaterState();

	SetTouch(&CCrossbowBolt::BoltTouch);

	SetThink(&CCrossbowBolt::BubbleThink);
	SetNextThink(gpGlobals->curtime + 0.1f);

	CreateSprites();

	// Make us glow until we've hit the wall
	m_nSkin = BOLT_SKIN_GLOW;
	//life counter
	m_iHealth = 0;
}

// ==================================================================================================
// INÍCIO DAS MODIFICAÇÕES: Função Precache modificada
// ==================================================================================================
void CCrossbowBolt::Precache(void)
{
	PrecacheModel(BOLT_MODEL);

	PrecacheModel("sprites/light_glow02_noz.vmt");

	// NOVO: Precache do sprite trail
	PrecacheModel(sv_crossbow_trail_sprite.GetString());
}
// ==================================================================================================
// FIM DAS MODIFICAÇÕES
// ==================================================================================================


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
// ==================================================================================================
// INÍCIO DA CORREÇÃO: Função BoltTouch restaurada com novas features integradas
// ==================================================================================================
void CCrossbowBolt::BoltTouch(CBaseEntity* pOther)
{
	if (!pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS))
		return;

	// Obter o dono da flecha
	CBaseEntity* pOwner = GetOwnerEntity();

	if (!pOwner)
	{
		UTIL_Remove(this);
		return;
	}

	// MODIFICAÇÃO: Pega o lifetime do trail para usar depois
	float trail_lifetime = sv_crossbow_trail_lifetime.GetFloat();
	if (trail_lifetime <= 0.1f) trail_lifetime = 0.1f;


	// Se acertamos uma entidade que pode tomar dano
	if (pOther->m_takedamage != DAMAGE_NO)
	{
		trace_t tr;
		tr = BaseClass::GetTouchTrace();
		Vector	vecNormalizedVel = GetAbsVelocity();
		ClearMultiDamage();
		VectorNormalize(vecNormalizedVel);

		// Lógica de dano
		if (pOwner->IsPlayer() && pOther->IsNPC())
		{
			CTakeDamageInfo dmgInfo(this, pOwner, m_iDamage, DMG_NEVERGIB);
			dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
			CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
			dmgInfo.SetDamagePosition(tr.endpos);
			pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);
		}
		else
		{
			CTakeDamageInfo dmgInfo(this, pOwner, m_iDamage, DMG_BULLET | DMG_NEVERGIB);
			CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
			dmgInfo.SetDamagePosition(tr.endpos);
			pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);
		}

		ApplyMultiDamage();

		// A flecha deve atravessar vidro quebrável
		if (pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
			return;

		SetAbsVelocity(Vector(0, 0, 0));

		// Toca o som de acerto no corpo
		EmitSound("Weapon_Crossbow.BoltHitBody");

		// Lógica para fincar a flecha na parede atrás do alvo
		Vector vForward;
		AngleVectors(GetAbsAngles(), &vForward);
		VectorNormalize(vForward);
		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + vForward * 128, MASK_OPAQUE, pOther, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction != 1.0f)
		{
			if (tr.m_pEnt == NULL || (tr.m_pEnt && tr.m_pEnt->GetMoveType() == MOVETYPE_NONE))
			{
				CEffectData	data;
				data.m_vOrigin = tr.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr.fraction != 1.0;
				DispatchEffect("BoltImpact", data);
			}
		}

		SetTouch(NULL);
		SetThink(NULL);

		// MODIFICAÇÃO: Lógica de remoção da flecha com LINGER
		if (sv_crossbow_trail_linger.GetBool())
		{
			SetMoveType(MOVETYPE_NONE);
			AddEffects(EF_NODRAW);
			SetSolid(SOLID_NONE);
			SetThink(&CCrossbowBolt::DelayedRemoveThink);
			SetNextThink(gpGlobals->curtime + trail_lifetime);
		}
		else
		{
			if (m_pBoltTrail) UTIL_Remove(m_pBoltTrail);
			SetThink(&CCrossbowBolt::SUB_Remove);
			SetNextThink(gpGlobals->curtime);
		}
	}
	else // Se acertamos o mundo ou um prop
	{
		trace_t tr;
		tr = BaseClass::GetTouchTrace();

		// Se acertamos o mundo (e não o céu)
		if (pOther->GetMoveType() == MOVETYPE_NONE && !(tr.surface.flags & SURF_SKY))
		{
			Vector vecDir = GetAbsVelocity();
			float speed = VectorNormalize(vecDir);
			float hitDot = DotProduct(tr.plane.normal, -vecDir);

			// Lógica de ricochete
			if ((hitDot < 0.5f) && (speed > 100))
			{
				Vector vReflection = 2.0f * tr.plane.normal * hitDot + vecDir;
				QAngle reflectAngles;
				VectorAngles(vReflection, reflectAngles);
				SetLocalAngles(reflectAngles);
				SetAbsVelocity(vReflection * speed * 0.75f);
				m_bHasBounced = true; // Para lógica de achievement/dano especial

				// MODIFICAÇÃO: Lógica do novo som de ricochete
				if (sv_crossbow_bounce_new_sound.GetBool())
				{
					EmitSound("Weapon_Crossbow.BoltHitWorldNEW");
				}
				else
				{
					EmitSound("Weapon_Crossbow.BoltHitWorld");
				}
			}
			else // Flecha fincou na parede
			{
				SetMoveType(MOVETYPE_NONE);
				AddEffects(EF_NODRAW);
				SetTouch(NULL);

				Vector vForward;
				AngleVectors(GetAbsAngles(), &vForward);
				VectorNormalize(vForward);
				CEffectData data;
				data.m_vOrigin = tr.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = 0;
				DispatchEffect("BoltImpact", data);

				UTIL_ImpactTrace(&tr, DMG_BULLET);
				EmitSound("Weapon_Crossbow.BoltHitWorld");

				// MODIFICAÇÃO: Lógica de remoção da flecha com LINGER
				if (sv_crossbow_trail_linger.GetBool())
				{
					SetMoveType(MOVETYPE_NONE);
					AddEffects(EF_NODRAW);
					SetSolid(SOLID_NONE);
					SetThink(&CCrossbowBolt::DelayedRemoveThink);
					SetNextThink(gpGlobals->curtime + trail_lifetime);
				}
				else
				{
					if (m_pBoltTrail) UTIL_Remove(m_pBoltTrail);
					SetThink(&CCrossbowBolt::SUB_Remove);
					SetNextThink(gpGlobals->curtime);
				}

				if (m_pGlowSprite != NULL)
				{
					m_pGlowSprite->TurnOn();
					m_pGlowSprite->FadeAndDie(3.0f);
				}
			}

			// Efeitos de faísca
			if (UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER)
			{
				g_pEffects->Sparks(GetAbsOrigin());
			}
		}
		else // Acertamos outra coisa (props físicos, etc.)
		{
			// Criar decalque se não for o céu
			if ((tr.surface.flags & SURF_SKY) == false)
			{
				UTIL_ImpactTrace(&tr, DMG_BULLET);
			}

			// MODIFICAÇÃO: Aqui, em vez de aplicar força customizada, simplesmente removemos a flecha
			// como o original faz. O sistema de física já vai lidar com o impacto no prop.
			if (m_pBoltTrail) UTIL_Remove(m_pBoltTrail);
			UTIL_Remove(this);
		}
	}

	if (g_pGameRules->IsMultiplayer())
	{
		// Lógica multiplayer adicional pode vir aqui se necessário
	}
}
// ==================================================================================================
// FIM DA CORREÇÃO
// ==================================================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrossbowBolt::BubbleThink(void)
{
	QAngle angNewAngles;

	VectorAngles(GetAbsVelocity(), angNewAngles);
	SetAbsAngles(angNewAngles);

	SetNextThink(gpGlobals->curtime + 0.1f);

	if (GetWaterLevel() == 0)
		return;

	UTIL_BubbleTrail(GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5);
}

#endif

//-----------------------------------------------------------------------------
// CWeaponCrossbow
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL
#define CWeaponCrossbow C_WeaponCrossbow
#endif

class CWeaponCrossbow : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS(CWeaponCrossbow, CBaseHL2MPCombatWeapon);
public:

	CWeaponCrossbow(void);

	virtual void	Precache(void);
	//virtual void	DefaultTouch(CBaseEntity* pOther);
	virtual void	PrimaryAttack(void);
	virtual void	SecondaryAttack(void);
	virtual bool	Deploy(void);
	virtual bool	Holster(CBaseCombatWeapon* pSwitchingTo = NULL);
	virtual bool	Reload(void);
	virtual void	ItemPostFrame(void);
	virtual void	ItemBusyFrame(void);
	virtual bool	SendWeaponAnim(int iActivity);

#ifndef CLIENT_DLL
	virtual void Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
#endif

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

private:

	void	SetSkin(int skinNum);
	void	CheckZoomToggle(void);
	void	FireBolt(void);
	void	SetBolt(int iSetting);
	void	ToggleZoom(void);

	// Various states for the crossbow's charger
	enum ChargerState_t
	{
		CHARGER_STATE_START_LOAD,
		CHARGER_STATE_START_CHARGE,
		CHARGER_STATE_READY,
		CHARGER_STATE_DISCHARGE,
		CHARGER_STATE_OFF,
	};

	void	CreateChargerEffects(void);
	void	SetChargerState(ChargerState_t state);
	void	DoLoadEffect(void);

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

private:

	// Charger effects
	ChargerState_t		m_nChargeState;

#ifndef CLIENT_DLL
	CHandle<CSprite>	m_hChargerSprite;
#endif

	CNetworkVar(bool, m_bInZoom);
	CNetworkVar(bool, m_bMustReload);

	CWeaponCrossbow(const CWeaponCrossbow&);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponCrossbow, DT_WeaponCrossbow)

BEGIN_NETWORK_TABLE(CWeaponCrossbow, DT_WeaponCrossbow)
#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bInZoom)),
RecvPropBool(RECVINFO(m_bMustReload)),
#else
SendPropBool(SENDINFO(m_bInZoom)),
SendPropBool(SENDINFO(m_bMustReload)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponCrossbow)
DEFINE_PRED_FIELD(m_bInZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bMustReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),

// misyl: Can't predict this easily as it comes from some animevent stuff...
DEFINE_PRED_FIELD(m_nSkin, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_crossbow, CWeaponCrossbow);

PRECACHE_WEAPON_REGISTER(weapon_crossbow);

#ifndef CLIENT_DLL

acttable_t	CWeaponCrossbow::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_CROSSBOW,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_CROSSBOW,						false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_CROSSBOW,				false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_CROSSBOW,				false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_CROSSBOW,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_AR2,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_CROSSBOW,					false },
};

IMPLEMENT_ACTTABLE(CWeaponCrossbow);

#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponCrossbow::CWeaponCrossbow(void)
{
	m_bReloadsSingly = true;
	m_bFiresUnderwater = true;
	m_bInZoom = false;
	m_bMustReload = false;
}

#define	CROSSBOW_GLOW_SPRITE	"sprites/light_glow02_noz.vmt"
#define	CROSSBOW_GLOW_SPRITE2	"sprites/blueflare1.vmt"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Precache(void)
{
#ifndef CLIENT_DLL
	UTIL_PrecacheOther("crossbow_bolt");
#endif

	PrecacheScriptSound("Weapon_Crossbow.BoltHitBody");
	PrecacheScriptSound("Weapon_Crossbow.BoltHitWorld");
	PrecacheScriptSound("Weapon_Crossbow.BoltSkewer");

	PrecacheModel(CROSSBOW_GLOW_SPRITE);
	PrecacheModel(CROSSBOW_GLOW_SPRITE2);

	// NOVO: Precache do sprite trail do crossbow
	PrecacheModel(sv_crossbow_trail_sprite.GetString());

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCrossbow::PrimaryAttack(void)
{
	if (m_bInZoom && g_pGameRules->IsMultiplayer())
	{
		//		FireSniperBolt();
		FireBolt();
	}
	else
	{
		FireBolt();
	}

	// Signal a reload
	m_bMustReload = true;

	SetWeaponIdleTime(gpGlobals->curtime + SequenceDuration(ACT_VM_PRIMARYATTACK));

#ifdef GAME_DLL
	CBasePlayer* player = ToBasePlayer(GetOwner());
	if (player)
		player->OnMyWeaponFired(this);
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SecondaryAttack(void)
{
	//NOTENOTE: The zooming is handled by the post/busy frames
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Reload(void)
{
	if (BaseClass::Reload())
	{
		m_bMustReload = false;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::CheckZoomToggle(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer->m_afButtonPressed & IN_ATTACK2)
	{
		ToggleZoom();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ItemBusyFrame(void)
{
	// Allow zoom toggling even when we're reloading
	CheckZoomToggle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ItemPostFrame(void)
{
	// Allow zoom toggling
	CheckZoomToggle();

	if (m_bMustReload && HasWeaponIdleTimeElapsed())
	{
		Reload();
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::FireBolt(void)
{
	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

#ifndef CLIENT_DLL
	Vector vecAiming = pOwner->GetAutoaimVector(0);
	Vector vecSrc = pOwner->Weapon_ShootPosition();

	QAngle angAiming;
	VectorAngles(vecAiming, angAiming);

	CCrossbowBolt* pBolt = CCrossbowBolt::BoltCreate(vecSrc, angAiming, GetHL2MPWpnData().m_iPlayerDamage, pOwner);

	if (pOwner->GetWaterLevel() == 3)
	{
		pBolt->SetAbsVelocity(vecAiming * BOLT_WATER_VELOCITY);
	}
	else
	{
		pBolt->SetAbsVelocity(vecAiming * BOLT_AIR_VELOCITY);
	}

#endif

	m_iClip1--;

	pOwner->ViewPunch(QAngle(-2, 0, 0));

	WeaponSound(SINGLE);
	WeaponSound(SPECIAL2);

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);

	if (!m_iClip1 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pOwner->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;

	DoLoadEffect();
	SetChargerState(CHARGER_STATE_DISCHARGE);
}
//-----------------------------------------------------------------------------
// Purpose: Sets whether or not the bolt is visible
//-----------------------------------------------------------------------------
inline void CWeaponCrossbow::SetBolt(int iSetting)
{
	int iBody = FindBodygroupByName("bolt");
	if (iBody != -1 || (GetOwner() && GetOwner()->IsPlayer())) // HACKHACK: Player models check the viewmodel instead of the worldmodel, so we have to do this manually
		SetBodygroup(iBody, iSetting);
	else
		m_nSkin = iSetting;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Deploy(void)
{
	if (m_iClip1 <= 0)
	{
		return DefaultDeploy((char*)GetViewModel(), (char*)GetWorldModel(), ACT_CROSSBOW_DRAW_UNLOADED, (char*)GetAnimPrefix());
	}

	SetSkin(BOLT_SKIN_GLOW);
	SetBolt(0);
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSwitchingTo - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	if (m_bInZoom)
	{
		ToggleZoom();
	}

	SetChargerState(CHARGER_STATE_OFF);

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ToggleZoom(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

#ifndef CLIENT_DLL

	if (m_bInZoom)
	{
		if (pPlayer->SetFOV(this, 0, 0.2f))
		{
			m_bInZoom = false;
		}
	}
	else
	{
		if (pPlayer->SetFOV(this, 20, 0.1f))
		{
			m_bInZoom = true;
		}
	}
#endif
}

#define	BOLT_TIP_ATTACHMENT	2

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::CreateChargerEffects(void)
{
#ifndef CLIENT_DLL
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (m_hChargerSprite != NULL)
		return;

	m_hChargerSprite = CSprite::SpriteCreate(CROSSBOW_GLOW_SPRITE, GetAbsOrigin(), false);

	if (m_hChargerSprite)
	{
		m_hChargerSprite->SetAttachment(pOwner->GetViewModel(), BOLT_TIP_ATTACHMENT);
		m_hChargerSprite->SetTransparency(kRenderTransAdd, 255, 128, 0, 255, kRenderFxNoDissipation);
		m_hChargerSprite->SetBrightness(0);
		m_hChargerSprite->SetScale(0.1f);
		m_hChargerSprite->TurnOff();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : skinNum - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SetSkin(int skinNum)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	CBaseViewModel* pViewModel = pOwner->GetViewModel();

	if (pViewModel == NULL)
		return;

	pViewModel->m_nSkin = skinNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::DoLoadEffect(void)
{
	SetSkin(BOLT_SKIN_GLOW);

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	CBaseViewModel* pViewModel = pOwner->GetViewModel();

	if (pViewModel == NULL)
		return;

	CEffectData	data;

#ifdef CLIENT_DLL
	data.m_hEntity = pViewModel->GetRefEHandle();
#else
	data.m_nEntIndex = pViewModel->entindex();
#endif
	data.m_vOrigin = GetAbsOrigin();
	data.m_nAttachmentIndex = 1;
#ifndef CLIENT_DLL
	DispatchEffectNoPred("CrossbowLoad", data);



	CSprite* pBlast = CSprite::SpriteCreate(CROSSBOW_GLOW_SPRITE2, GetAbsOrigin(), false);

	if (pBlast)
	{
		pBlast->SetAttachment(pOwner->GetViewModel(), 1);
		pBlast->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
		pBlast->SetBrightness(128);
		pBlast->SetScale(0.2f);
		pBlast->FadeOutFromSpawn();
	}
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SetChargerState(ChargerState_t state)
{
	// Make sure we're setup
	CreateChargerEffects();

	// Don't do this twice
	if (state == m_nChargeState)
		return;

	m_nChargeState = state;

	switch (m_nChargeState)
	{
	case CHARGER_STATE_START_LOAD:

		WeaponSound(SPECIAL1);

		// Shoot some sparks and draw a beam between the two outer points
		DoLoadEffect();
		SetBolt(0);

		break;
#ifndef CLIENT_DLL
	case CHARGER_STATE_START_CHARGE:
	{
		if (m_hChargerSprite == NULL)
			break;

		m_hChargerSprite->SetBrightness(32, 0.5f);
		m_hChargerSprite->SetScale(0.025f, 0.5f);
		m_hChargerSprite->TurnOn();
	}

	break;

	case CHARGER_STATE_READY:
	{
		// Get fully charged
		if (m_hChargerSprite == NULL)
			break;

		m_hChargerSprite->SetBrightness(80, 1.0f);
		m_hChargerSprite->SetScale(0.1f, 0.5f);
		m_hChargerSprite->TurnOn();
	}

	break;

	case CHARGER_STATE_DISCHARGE:
	{
		SetSkin(BOLT_SKIN_NORMAL);

		if (m_hChargerSprite == NULL)
			break;

		m_hChargerSprite->SetBrightness(0);
		m_hChargerSprite->TurnOff();
	}

	break;
#endif
	case CHARGER_STATE_OFF:
	{
		SetSkin(BOLT_SKIN_NORMAL);

#ifndef CLIENT_DLL
		if (m_hChargerSprite == NULL)
			break;

		m_hChargerSprite->SetBrightness(0);
		m_hChargerSprite->TurnOff();
#endif
	}
	break;

	default:
		break;
	}
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
	// misyl: Disable pred filtering in this server-only section.
	CDisablePredictionFiltering disablePred;

	switch (pEvent->event)
	{
	case EVENT_WEAPON_THROW:
		SetChargerState(CHARGER_STATE_START_LOAD);
		break;

	case EVENT_WEAPON_THROW2:
		SetChargerState(CHARGER_STATE_START_CHARGE);
		break;

	case EVENT_WEAPON_THROW3:
		SetChargerState(CHARGER_STATE_READY);
		break;

	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Set the desired activity for the weapon and its viewmodel counterpart
// Input  : iActivity - activity to play
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::SendWeaponAnim(int iActivity)
{
	int newActivity = iActivity;

	// The last shot needs a non-loaded activity
	if ((newActivity == ACT_VM_IDLE) && (m_iClip1 <= 0))
	{
		newActivity = ACT_VM_FIDGET;
	}

	//For now, just set the ideal activity and be done with it
	return BaseClass::SendWeaponAnim(newActivity);
}
// Adicione este bloco de código inteiro em weapon_crossbow.cpp

//void CWeaponCrossbow::DefaultTouch(CBaseEntity* pOther)
//{
//	// Converte a entidade que tocou na arma para um jogador
//	CBasePlayer* pPlayer = ToBasePlayer(pOther);
//	if (!pPlayer)
//	{
//		// Se não for um jogador, não faz nada
//		return;
//	}
//
//	// Verifica se o jogador já possui uma crossbow
//	if (pPlayer->Weapon_OwnsThisType(GetClassname()))
//	{
//		// Se ele já tem, não damos a arma, apenas a munição.
//		// Esta é a nossa lógica customizada que ignora o pente da arma no chão.
//
//		// Damos uma quantidade fixa de flechas (ex: 5)
//		if (pPlayer->GiveAmmo(5, m_iPrimaryAmmoType))
//		{
//			// Toca o som de pegar munição
//			pPlayer->EmitSound("BaseCombatWeapon.AmmoPickup");
//
//			// Remove a arma do chão
//			UTIL_Remove(this);
//		}
//	}
//	else
//	{
//		// Se o jogador não tem uma crossbow, deixamos a lógica padrão do jogo
//		// decidir se ele pode ou não pegar a arma.
//		BaseClass::DefaultTouch(pOther);
//	}
//}

void CCrossbowBolt::DelayedRemoveThink(void)
{
	// Verificar se trail existe antes de tentar remover
	if (m_pBoltTrail)
	{
		UTIL_Remove(m_pBoltTrail);
		m_pBoltTrail = NULL;
	}

	// Remove o brilho
	if (m_pGlowSprite)
	{
		UTIL_Remove(m_pGlowSprite);
		m_pGlowSprite = NULL;
	}

	// Remove a própria flecha
	UTIL_Remove(this);
}
