//2
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL2.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "hl2mp_player.h"
#include "globalstate.h"
#include "game.h"
#include "gamerules.h"
#include "hl2mp_player_shared.h"
#include "predicted_viewmodel.h"
#include "in_buttons.h"
#include "hl2mp_gamerules.h"
#include "KeyValues.h"
#include "team.h"
#include "weapon_hl2mpbase.h"
#include "grenade_satchel.h"
#include "eventqueue.h"
#include "gamestats.h"
#include "ammodef.h"
#include "NextBot.h"
#include "hl2mp/weapon_physcannon.h"

#include "shareddefs.h"

#include "convar.h"
#include "gameinterface.h"
#include "tier1/utlbuffer.h"

#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

#include "ilagcompensationmanager.h"
#include "filesystem.h"
#include "admin/hl2mp_serveradmin.h"

#include "spawnweapons_manager.h"



int g_iLastCitizenModel = 0;
int g_iLastCombineModel = 0;

CBaseEntity* g_pLastCombineSpawn = NULL;
CBaseEntity* g_pLastRebelSpawn = NULL;
extern CBaseEntity* g_pLastSpawn;

ConVar hl2mp_spawn_frag_fallback_radius("hl2mp_spawn_frag_fallback_radius", "48", FCVAR_NONE, "If no spawns are available, kill players with this radius to allow new players to spawn.");

// SECTION FOR HITSOUNDS
ConVar sv_hitsounds_enabled("sv_hitsounds_enabled", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable/disable all custom hit and kill sounds.");
// CORRIGIDO: Adicionado 'true, fMin, true, fMax' para definir os limites corretamente.
ConVar sv_hitsounds_volume("sv_hitsounds_volume", "0.7", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Volume for all custom hit and kill sounds.", true, 0.0f, true, 1.0f);
ConVar sv_hitsounds_hit_body_enabled("sv_hitsounds_hit_body_enabled", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable/disable body hit sounds.");
ConVar sv_hitsounds_hit_head_enabled("sv_hitsounds_hit_head_enabled", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable/disable headshot hit sounds.");
ConVar sv_hitsounds_kill_body_enabled("sv_hitsounds_kill_body_enabled", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable/disable normal kill sounds.");
ConVar sv_hitsounds_kill_head_enabled("sv_hitsounds_kill_head_enabled", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable/disable headshot kill sounds.");
ConVar sv_hitsounds_pitch_enabled("sv_hitsounds_pitch_enabled", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable/disable incremental pitch for consecutive hits.");
// CORRIGIDO: Adicionado 'true, fMin, true, fMax' para definir os limites corretamente.
ConVar sv_hitsounds_pitch_increment("sv_hitsounds_pitch_increment", "5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Pitch increase per consecutive hit (100 is normal).", true, 0, true, 50);
ConVar sv_hitsounds_pitch_max("sv_hitsounds_pitch_max", "180", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum pitch value (100 is normal).", true, 100, true, 255);
ConVar sv_hitsounds_pitch_reset_time("sv_hitsounds_pitch_reset_time", "0.6", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Time in seconds before consecutive hit pitch resets.");
ConVar sv_hitsounds_ff("sv_hitsounds_ff", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Play hit sounds on friendly fire? 0=No, 1=Yes");
// Cvars de arquivos de som
ConVar sv_hitsounds_body_sound("sv_hitsounds_body_sound", "buttons/button10.wav", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sound file for body hits.");
ConVar sv_hitsounds_head_sound("sv_hitsounds_head_sound", "buttons/button16.wav", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sound file for headshot hits.");
ConVar sv_hitsounds_kill_body_sound("sv_hitsounds_kill_body_sound", "buttons/button9.wav", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sound file for body kills.");
ConVar sv_hitsounds_kill_head_sound("sv_hitsounds_kill_head_sound", "npc/sniper/sniper1.wav", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sound file for headshot kills.");
// Volume por som (multiplicador 0.0–1.0)
ConVar sv_hitsounds_body_volume("sv_hitsounds_body_volume", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Volume multiplier for body hit sound.", true, 0.0f, true, 1.0f);
ConVar sv_hitsounds_head_volume("sv_hitsounds_head_volume", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Volume multiplier for headshot hit sound.", true, 0.0f, true, 1.0f);
ConVar sv_hitsounds_kill_body_volume("sv_hitsounds_kill_body_volume", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Volume multiplier for body kill sound.", true, 0.0f, true, 1.0f);
ConVar sv_hitsounds_kill_head_volume("sv_hitsounds_kill_head_volume", "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Volume multiplier for headshot kill sound.", true, 0.0f, true, 1.0f);
// Pitch base por som (100 = normal)
ConVar sv_hitsounds_pitch_base_hit_body("sv_hitsounds_pitch_base_hit_body", "100", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Base pitch for body hit.", true, 1, true, 255);
ConVar sv_hitsounds_pitch_base_hit_head("sv_hitsounds_pitch_base_hit_head", "100", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Base pitch for headshot hit.", true, 1, true, 255);
ConVar sv_hitsounds_pitch_base_kill_body("sv_hitsounds_pitch_base_kill_body", "100", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Base pitch for body kill.", true, 1, true, 255);
ConVar sv_hitsounds_pitch_base_kill_head("sv_hitsounds_pitch_base_kill_head", "100", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Base pitch for headshot kill.", true, 1, true, 255);
// END SECTION FOR HITSOUNDS

// FOV System CVars
ConVar sv_fov_min("sv_fov_min", "75", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum FOV value players can set.", true, 60.0f, true, 90.0f);
ConVar sv_fov_max("sv_fov_max", "120", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Maximum FOV value players can set.", true, 90.0f, true, 170.0f);
ConVar sv_fov_default("sv_fov_default", "90", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Default FOV value for new players.", true, 60.0f, true, 170.0f);


extern ConVar sv_killerinfo_airkill_enable;
extern ConVar sv_killerinfo_bouncekill_enable;

#define HL2MP_COMMAND_MAX_RATE 0.3

void DropPrimedFragGrenade(CHL2MP_Player* pPlayer, CBaseCombatWeapon* pGrenade);

LINK_ENTITY_TO_CLASS(player, CHL2MP_Player);

LINK_ENTITY_TO_CLASS(info_player_combine, CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_rebel, CPointEntity);

// specific to the local player
BEGIN_SEND_TABLE_NOBASE(CHL2MP_Player, DT_HL2MPLocalPlayerExclusive)
// send a hi-res origin to the local player for use in prediction
SendPropVectorXY(SENDINFO(m_vecOrigin), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY),
SendPropFloat(SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ),

SendPropFloat(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f),
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN),

END_SEND_TABLE()

// all players except the local player
BEGIN_SEND_TABLE_NOBASE(CHL2MP_Player, DT_HL2MPNonLocalPlayerExclusive)
// send a lo-res origin to other players
SendPropVectorXY(SENDINFO(m_vecOrigin), -1, SPROP_COORD_MP_LOWPRECISION | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY),
SendPropFloat(SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_COORD_MP_LOWPRECISION | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ),

SendPropFloat(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f),
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN),

END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST(CHL2MP_Player, DT_HL2MP_Player)
SendPropExclude("DT_BaseEntity", "m_vecOrigin"),

// misyl:
// m_flMaxspeed is fully predicted by the client and the client's
// maxspeed is sent in the user message.
// Other games like DOD, etc don't use this var at all and just fully
// predict in GameMovement, but the HL2 codebase doesn't do that and modifies this
// on the player.
// So, just never send it, and don't predict it on the client either.
SendPropExclude("DT_BasePlayer", "m_flMaxspeed"),


// Data that only gets sent to the local player
SendPropDataTable("hl2mplocaldata", 0, &REFERENCE_SEND_TABLE(DT_HL2MPLocalPlayerExclusive), SendProxy_SendLocalDataTable),

// Data that gets sent to all other players
SendPropDataTable("hl2mpnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_HL2MPNonLocalPlayerExclusive), SendProxy_SendNonLocalDataTable),

SendPropEHandle(SENDINFO(m_hRagdoll)),
SendPropInt(SENDINFO(m_iSpawnInterpCounter), 4),
SendPropInt(SENDINFO(m_iPlayerSoundType), 3),

SendPropExclude("DT_BaseAnimating", "m_flPoseParameter"),
SendPropExclude("DT_BaseFlex", "m_viewtarget"),

//	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
//	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),	
END_SEND_TABLE()

BEGIN_DATADESC(CHL2MP_Player)
END_DATADESC()

BEGIN_ENT_SCRIPTDESC(CHL2MP_Player, CHL2_Player, "Half-Life 2: Deathmatch Player")
END_SCRIPTDESC();

const char* g_ppszRandomCitizenModels[] =
{
	"models/humans/group03/male_01.mdl",
	"models/humans/group03/male_02.mdl",
	"models/humans/group03/female_01.mdl",
	"models/humans/group03/male_03.mdl",
	"models/humans/group03/female_02.mdl",
	"models/humans/group03/male_04.mdl",
	"models/humans/group03/female_03.mdl",
	"models/humans/group03/male_05.mdl",
	"models/humans/group03/female_04.mdl",
	"models/humans/group03/male_06.mdl",
	"models/humans/group03/female_06.mdl",
	"models/humans/group03/male_07.mdl",
	"models/humans/group03/female_07.mdl",
	"models/humans/group03/male_08.mdl",
	"models/humans/group03/male_09.mdl",
};

const char* g_ppszRandomCombineModels[] =
{
	"models/combine_soldier.mdl",
	"models/combine_soldier_prisonguard.mdl",
	"models/combine_super_soldier.mdl",
	"models/police.mdl",
};


#define MAX_COMBINE_MODELS 4
#define MODEL_CHANGE_INTERVAL 5.0f
#define TEAM_CHANGE_INTERVAL 5.0f

#define HL2MPPLAYER_PHYSDAMAGE_SCALE 4.0f

#pragma warning( disable : 4355 )

CHL2MP_Player::CHL2MP_Player() : m_PlayerAnimState(this)
{
	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	m_iSpawnInterpCounter = 0;

	m_bEnterObserver = false;
	m_bReady = false;

	// ADICIONE ESTAS LINHAS
	m_flLastHitSoundTime = 0.0f;
	m_iConsecutiveHits = 0;

	m_iLastHitGroup = 0;

	BaseClass::ChangeTeam(0);

	//UseClientSideAnimation();
}

CHL2MP_Player::~CHL2MP_Player(void)
{

}

void CHL2MP_Player::UpdateOnRemove(void)
{
	if (auto p = GetLadderMove())
	{
		UTIL_Remove((CBaseEntity*)(p->m_hReservedSpot.Get()));
		p->m_hReservedSpot = NULL;
	}

	if (m_hRagdoll)
	{
		UTIL_RemoveImmediate(m_hRagdoll);
		m_hRagdoll = NULL;
	}

	BaseClass::UpdateOnRemove();
}

void CHL2MP_Player::Precache(void)
{
	BaseClass::Precache();

	PrecacheModel("sprites/glow01.vmt");

	//Precache Citizen models
	int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);
	int i;

	for (i = 0; i < nHeads; ++i)
		PrecacheModel(g_ppszRandomCitizenModels[i]);

	//Precache Combine Models
	nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

	for (i = 0; i < nHeads; ++i)
		PrecacheModel(g_ppszRandomCombineModels[i]);

	PrecacheFootStepSounds();

	PrecacheScriptSound("NPC_MetroPolice.Die");
	PrecacheScriptSound("NPC_CombineS.Die");
	PrecacheScriptSound("NPC_Citizen.die");

	PrecacheScriptSound(sv_hitsounds_body_sound.GetString());
	PrecacheScriptSound(sv_hitsounds_head_sound.GetString());
	PrecacheScriptSound(sv_hitsounds_kill_body_sound.GetString());
	PrecacheScriptSound(sv_hitsounds_kill_head_sound.GetString());
}

//New function for player feedback
void CHL2MP_Player::OnDamageDealt(CBasePlayer* pVictim, const CTakeDamageInfo& info)
{
	CHL2MP_Player* pHL2MPVictim = ToHL2MPPlayer(pVictim);
	if (!pHL2MPVictim || !sv_hitsounds_enabled.GetBool())
		return;

	// Teamplay / Friendly Fire
	if (HL2MPRules()->IsTeamplay() && this->InSameTeam(pHL2MPVictim))
	{
		if (!g_pGameRules->FPlayerCanTakeDamage(pHL2MPVictim, this, info) || !sv_hitsounds_ff.GetBool())
			return;
	}

	const bool bIsKill = (pHL2MPVictim->GetHealth() <= 0);
	const int hitGroup = pHL2MPVictim->m_iLastHitGroup;

	// ===== Seleção do som + volume + pitch base =====
	const char* soundFile = nullptr;
	float typeVolume = 1.0f;
	int basePitch = 100;

	if (bIsKill)
	{
		if (hitGroup == HITGROUP_HEAD && sv_hitsounds_kill_head_enabled.GetBool())
		{
			soundFile = sv_hitsounds_kill_head_sound.GetString();
			typeVolume = sv_hitsounds_kill_head_volume.GetFloat();
			basePitch = sv_hitsounds_pitch_base_kill_head.GetInt();
		}
		else if (sv_hitsounds_kill_body_enabled.GetBool())
		{
			soundFile = sv_hitsounds_kill_body_sound.GetString();
			typeVolume = sv_hitsounds_kill_body_volume.GetFloat();
			basePitch = sv_hitsounds_pitch_base_kill_body.GetInt();
		}
	}
	else
	{
		if (hitGroup == HITGROUP_HEAD && sv_hitsounds_hit_head_enabled.GetBool())
		{
			soundFile = sv_hitsounds_head_sound.GetString();
			typeVolume = sv_hitsounds_head_volume.GetFloat();
			basePitch = sv_hitsounds_pitch_base_hit_head.GetInt();
		}
		else if (sv_hitsounds_hit_body_enabled.GetBool())
		{
			soundFile = sv_hitsounds_body_sound.GetString();
			typeVolume = sv_hitsounds_body_volume.GetFloat();
			basePitch = sv_hitsounds_pitch_base_hit_body.GetInt();
		}
	}

	if (!soundFile || soundFile[0] == '\0')
		return;

	// ===== Pitch incremental =====
	int pitch = basePitch;

	if (sv_hitsounds_pitch_enabled.GetBool())
	{
		// Reset se passou tempo sem acerto
		if ((gpGlobals->curtime - this->m_flLastHitSoundTime) > sv_hitsounds_pitch_reset_time.GetFloat())
			this->m_iConsecutiveHits = 0;

		this->m_flLastHitSoundTime = gpGlobals->curtime;

		// Incrementa somente a partir do segundo hit consecutivo
		if (this->m_iConsecutiveHits > 0)
		{
			pitch += (this->m_iConsecutiveHits * sv_hitsounds_pitch_increment.GetInt());
			if (pitch > sv_hitsounds_pitch_max.GetInt())
				pitch = sv_hitsounds_pitch_max.GetInt();
		}

		this->m_iConsecutiveHits++;

		// Reset após kill
		if (bIsKill)
			this->m_iConsecutiveHits = 0;
	}

	// ===== Volume final =====
	float finalVolume = sv_hitsounds_volume.GetFloat() * typeVolume;
	finalVolume = clamp(finalVolume, 0.0f, 1.0f);

	// ===== Toca som só pro atirador =====
	CSingleUserRecipientFilter filter(this);
	filter.MakeReliable();

	EmitSound_t ep;
	ep.m_nChannel = CHAN_AUTO;
	ep.m_pSoundName = soundFile;
	ep.m_flVolume = finalVolume;
	ep.m_SoundLevel = SNDLVL_NONE;
	ep.m_nPitch = pitch;

	EmitSound(filter, this->entindex(), ep);
}

// New function to trace attack (for hit/kill sounds)
void CHL2MP_Player::TraceAttack(const CTakeDamageInfo& info, const Vector& vecDir, trace_t* ptr, CDmgAccumulator* pAccumulator)
{
	// A única coisa que fazemos aqui agora é anotar onde o tiro acertou.
	if (ptr)
	{
		m_iLastHitGroup = ptr->hitgroup;
	}
	else
	{
		m_iLastHitGroup = -1; // Usa um valor inválido se não houver um traço
	}

	BaseClass::TraceAttack(info, vecDir, ptr, pAccumulator);
}

void ReloadGameRules()
{
	// vanilla deathmatch
	CreateGameRulesObject("CHL2MPRules");
}

void CHL2MP_Player::GiveAllItems(void)
{
	EquipSuit();

	//SetArmorValue(100);
	//SetHealth(100);

	CBasePlayer::GiveAmmo(255, "Pistol");
	CBasePlayer::GiveAmmo(255, "AR2");
	CBasePlayer::GiveAmmo(5, "AR2AltFire");
	CBasePlayer::GiveAmmo(255, "SMG1");
	CBasePlayer::GiveAmmo(5, "smg1_grenade");
	CBasePlayer::GiveAmmo(255, "Buckshot");
	CBasePlayer::GiveAmmo(32, "357");
	CBasePlayer::GiveAmmo(3, "rpg_round");
	CBasePlayer::GiveAmmo(16, "XBowBolt");
	CBasePlayer::GiveAmmo(5, "grenade");
	CBasePlayer::GiveAmmo(5, "slam");
	GiveNamedItem("weapon_crowbar");
	GiveNamedItem("weapon_stunstick");
	GiveNamedItem("weapon_pistol");
	GiveNamedItem("weapon_357");
	GiveNamedItem("weapon_smg1");
	GiveNamedItem("weapon_ar2");
	GiveNamedItem("weapon_shotgun");
	GiveNamedItem("weapon_frag");
	GiveNamedItem("weapon_crossbow");
	GiveNamedItem("weapon_rpg");
	GiveNamedItem("weapon_slam");
	GiveNamedItem("weapon_physcannon");

}

void CHL2MP_Player::GiveDefaultItems(void)
{
	EquipSuit();

	CBasePlayer::GiveAmmo(255, "Pistol");
	CBasePlayer::GiveAmmo(45, "SMG1");
	CBasePlayer::GiveAmmo(1, "grenade");
	CBasePlayer::GiveAmmo(6, "Buckshot");
	CBasePlayer::GiveAmmo(6, "357");

	const char* iModelName = NULL;
	iModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

	if (HL2MPRules()->IsTeamplay() == false)
	{
		if (Q_stristr(iModelName, "combine"))
			//	if ( GetPlayerModelType() == PLAYER_SOUNDS_METROPOLICE || GetPlayerModelType() == PLAYER_SOUNDS_COMBINESOLDIER 
		{
			GiveNamedItem("weapon_stunstick");
		}
		else if (Q_stristr(iModelName, "police"))
		{
			GiveNamedItem("weapon_stunstick");
		}
		//else if ( GetPlayerModelType() == PLAYER_SOUNDS_CITIZEN )
		else if (Q_stristr(iModelName, "models/human"))
		{
			GiveNamedItem("weapon_crowbar");
		}
	}
	else
	{
		if (GetTeamNumber() == TEAM_COMBINE) {
			GiveNamedItem("weapon_stunstick");
		}
		else if (GetTeamNumber() == TEAM_REBELS) {
			GiveNamedItem("weapon_crowbar");
		}
	}

	GiveNamedItem("weapon_pistol");
	GiveNamedItem("weapon_smg1");
	GiveNamedItem("weapon_frag");
	GiveNamedItem("weapon_physcannon");

	const char* szDefaultWeaponName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_defaultweapon");

	CBaseCombatWeapon* pDefaultWeapon = Weapon_OwnsThisType(szDefaultWeaponName);

	if (pDefaultWeapon)
	{
		Weapon_Switch(pDefaultWeapon);
	}
	else
	{
		Weapon_Switch(Weapon_OwnsThisType("weapon_physcannon"));
	}
}

void CHL2MP_Player::PickDefaultSpawnTeam(void)
{
	if (GetTeamNumber() == 0)
	{
		if (HL2MPRules()->IsTeamplay() == false)
		{
			if (GetModelPtr() == NULL)
			{
				const char* szModelName = NULL;
				szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

				if (ValidatePlayerModel(szModelName) == false)
				{
					char szReturnString[512];

					Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel models/combine_soldier.mdl\n");
					engine->ClientCommand(edict(), szReturnString);
				}

				ChangeTeam(TEAM_UNASSIGNED);
			}
		}
		else
		{
			CTeam* pCombine = g_Teams[TEAM_COMBINE];
			CTeam* pRebels = g_Teams[TEAM_REBELS];

			if (pCombine == NULL || pRebels == NULL)
			{
				ChangeTeam(random->RandomInt(TEAM_COMBINE, TEAM_REBELS));
			}
			else
			{
				if (pCombine->GetNumPlayers() > pRebels->GetNumPlayers())
				{
					ChangeTeam(TEAM_REBELS);
				}
				else if (pCombine->GetNumPlayers() < pRebels->GetNumPlayers())
				{
					ChangeTeam(TEAM_COMBINE);
				}
				else
				{
					ChangeTeam(random->RandomInt(TEAM_COMBINE, TEAM_REBELS));
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets HL2 specific defaults.
//-----------------------------------------------------------------------------
void CHL2MP_Player::DelayedLoadPlayerSettings()
{
	LoadPlayerSettings();
}

void CHL2MP_Player::Spawn(void)
{
	//Msg("CHL2MP_Player::Spawn called - footstep system loaded\n");
	//BaseClass::Spawn();

	// m_flNextModelChangeTime = 0.0f;
	// m_flNextTeamChangeTime = 0.0f;

	m_flLastHitSoundTime = 0.0f;
	m_iConsecutiveHits = 0;
	m_iLastHitGroup = 0;

	PickDefaultSpawnTeam();

	BaseClass::Spawn();

	SetContextThink(&CHL2MP_Player::DelayedLoadPlayerSettings, gpGlobals->curtime + 0.1f, "LoadPlayerSettingsContext");

	CompensateScoreOnTeamSwitch(false);
	CompensateTeamScoreOnTeamSwitch(false);

	if (!IsObserver())
	{
		pl.deadflag = false;
		RemoveSolidFlags(FSOLID_NOT_SOLID);

		RemoveEffects(EF_NODRAW);

		SetAllowPickupWeaponThroughObstacle(true);
		GiveDefaultItems();
		SetAllowPickupWeaponThroughObstacle(false);
	}

	SetNumAnimOverlays(3);
	ResetAnimation();

	m_nRenderFX = kRenderNormal;

	m_Local.m_iHideHUD = 0;

	AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

	m_impactEnergyScale = HL2MPPLAYER_PHYSDAMAGE_SCALE;

	if (HL2MPRules()->IsIntermission())
	{
		AddFlag(FL_FROZEN);
	}
	else
	{
		RemoveFlag(FL_FROZEN);
	}

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);

	m_bReady = false;
	if (g_pSpawnWeaponsManager && sv_spawnweapons_enable.GetBool())
	{
		// Small delay to ensure player is fully initialized
		SetContextThink(&CHL2MP_Player::ApplySpawnLoadoutThink, gpGlobals->curtime + 0.1f, "SpawnLoadout");
	}
}

bool CHL2MP_Player::ValidatePlayerModel(const char* pModel)
{
	int iModels = ARRAYSIZE(g_ppszRandomCitizenModels);
	int i;

	for (i = 0; i < iModels; ++i)
	{
		if (!Q_stricmp(g_ppszRandomCitizenModels[i], pModel))
		{
			return true;
		}
	}

	iModels = ARRAYSIZE(g_ppszRandomCombineModels);

	for (i = 0; i < iModels; ++i)
	{
		if (!Q_stricmp(g_ppszRandomCombineModels[i], pModel))
		{
			return true;
		}
	}

	return false;
}

void CHL2MP_Player::PickupObject(CBaseEntity* pObject, bool bLimitMassAndSize)
{

}

void CHL2MP_Player::SetPlayerTeamModel(void)
{
	const char* szModelName = NULL;
	szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

	int modelIndex = modelinfo->GetModelIndex(szModelName);

	if (modelIndex == -1 || ValidatePlayerModel(szModelName) == false)
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", szModelName);
		engine->ClientCommand(edict(), szReturnString);
	}

	if (GetTeamNumber() == TEAM_COMBINE)
	{
		if (Q_stristr(szModelName, "models/human"))
		{
			int nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

			g_iLastCombineModel = (g_iLastCombineModel + 1) % nHeads;
			szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];
		}

		m_iModelType = TEAM_COMBINE;
	}
	else if (GetTeamNumber() == TEAM_REBELS)
	{
		if (!Q_stristr(szModelName, "models/human"))
		{
			int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);

			g_iLastCitizenModel = (g_iLastCitizenModel + 1) % nHeads;
			szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];
		}

		m_iModelType = TEAM_REBELS;
	}

	SetModel(szModelName);
	SetupPlayerSoundsByModel(szModelName);

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetPlayerModel(void)
{
	const char* szModelName = NULL;
	const char* pszCurrentModelName = modelinfo->GetModelName(GetModel());

	szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

	if (ValidatePlayerModel(szModelName) == false)
	{
		char szReturnString[512];

		if (ValidatePlayerModel(pszCurrentModelName) == false)
		{
			pszCurrentModelName = "models/Combine_Soldier.mdl";
		}

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", pszCurrentModelName);
		engine->ClientCommand(edict(), szReturnString);

		szModelName = pszCurrentModelName;
	}

	if (GetTeamNumber() == TEAM_COMBINE)
	{
		int nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

		g_iLastCombineModel = (g_iLastCombineModel + 1) % nHeads;
		szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];

		m_iModelType = TEAM_COMBINE;
	}
	else if (GetTeamNumber() == TEAM_REBELS)
	{
		int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);

		g_iLastCitizenModel = (g_iLastCitizenModel + 1) % nHeads;
		szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];

		m_iModelType = TEAM_REBELS;
	}
	else
	{
		if (Q_strlen(szModelName) == 0)
		{
			szModelName = g_ppszRandomCitizenModels[0];
		}

		if (Q_stristr(szModelName, "models/human"))
		{
			m_iModelType = TEAM_REBELS;
		}
		else
		{
			m_iModelType = TEAM_COMBINE;
		}
	}

	int modelIndex = modelinfo->GetModelIndex(szModelName);

	if (modelIndex == -1)
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", szModelName);
		engine->ClientCommand(edict(), szReturnString);
	}

	SetModel(szModelName);
	SetupPlayerSoundsByModel(szModelName);

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetupPlayerSoundsByModel(const char* pModelName)
{
	if (Q_stristr(pModelName, "models"))
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_CITIZEN;
	}
	//	else if ( Q_stristr(pModelName, "police" ) )
	//	{
	//		m_iPlayerSoundType = (int)PLAYER_SOUNDS_METROPOLICE;
	//	}
	//	else if ( Q_stristr(pModelName, "combine" ) )
	//	{
	//		m_iPlayerSoundType = (int)PLAYER_SOUNDS_COMBINESOLDIER;
	//	}
}

void CHL2MP_Player::ResetAnimation(void)
{
	if (IsAlive())
	{
		SetSequence(-1);
		SetActivity(ACT_INVALID);

		if (!GetAbsVelocity().x && !GetAbsVelocity().y)
			SetAnimation(PLAYER_IDLE);
		else if ((GetAbsVelocity().x || GetAbsVelocity().y) && (GetFlags() & FL_ONGROUND))
			SetAnimation(PLAYER_WALK);
		else if (GetWaterLevel() > 1)
			SetAnimation(PLAYER_WALK);
		else if ((GetFlags() & FL_ONGROUND) != FL_ONGROUND)
			SetAnimation(PLAYER_JUMP);
		else
			SetAnimation(PLAYER_IDLE);
	}
}


bool CHL2MP_Player::Weapon_Switch(CBaseCombatWeapon* pWeapon, int viewmodelindex)
{
	bool bRet = BaseClass::Weapon_Switch(pWeapon, viewmodelindex);

	if (bRet == true)
	{
		ResetAnimation();
	}

	return bRet;
}

void CHL2MP_Player::PreThink(void)
{
	QAngle vOldAngles = GetLocalAngles();
	QAngle vTempAngles = GetLocalAngles();

	vTempAngles = EyeAngles();

	if (vTempAngles[PITCH] > 180.0f)
	{
		vTempAngles[PITCH] -= 360.0f;
	}

	SetLocalAngles(vTempAngles);

	BaseClass::PreThink();
	State_PreThink();

	//Reset bullet force accumulator, only lasts one frame
	m_vecTotalBulletForce = vec3_origin;
	SetLocalAngles(vOldAngles);
}

void CHL2MP_Player::PostThink(void)
{
	BaseClass::PostThink();

	if (GetFlags() & FL_DUCKING)
	{
		SetCollisionBounds(VEC_CROUCH_TRACE_MIN, VEC_CROUCH_TRACE_MAX);
	}

	m_PlayerAnimState.Update();

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles(angles);
}

void CHL2MP_Player::PlayerDeathThink()
{
	if (!IsObserver())
	{
		BaseClass::PlayerDeathThink();
	}
}

void CHL2MP_Player::FireBullets(const FireBulletsInfo_t& info)
{
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation(this, this->GetCurrentCommand());

	FireBulletsInfo_t modinfo = info;

	CWeaponHL2MPBase* pWeapon = dynamic_cast<CWeaponHL2MPBase*>(GetActiveWeapon());

	if (pWeapon)
	{
		modinfo.m_iPlayerDamage = modinfo.m_flDamage = pWeapon->GetHL2MPWpnData().m_iPlayerDamage;
	}

	NoteWeaponFired();

	BaseClass::FireBullets(modinfo);

	// Move other players back to history positions based on local player's lag
	lagcompensation->FinishLagCompensation(this);

	if (pWeapon)
		this->OnMyWeaponFired(pWeapon);
}

void CHL2MP_Player::OnMyWeaponFired(CBaseCombatWeapon* weapon)
{
	BaseClass::OnMyWeaponFired(weapon);

	TheNextBots().OnWeaponFired(this, weapon);
}

void CHL2MP_Player::NoteWeaponFired(void)
{
	Assert(m_pCurrentCommand);
	if (m_pCurrentCommand)
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

extern ConVar sv_maxunlag;

bool CHL2MP_Player::WantsLagCompensationOnEntity(const CBasePlayer* pPlayer, const CUserCmd* pCmd, const CBitVec<MAX_EDICTS>* pEntityTransmitBits) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if (!((pCmd->buttons & IN_ATTACK) || (pCmd->buttons & IN_ATTACK2)) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5))
		return false;

	// If this entity hasn't been transmitted to us and acked, then don't bother lag compensating it.
	if (pEntityTransmitBits && !pEntityTransmitBits->Get(pPlayer->entindex()))
		return false;

	const Vector& vMyOrigin = GetAbsOrigin();
	const Vector& vHisOrigin = pPlayer->GetAbsOrigin();

	// get max distance player could have moved within max lag compensation time, 
	// multiply by 1.5 to to avoid "dead zones"  (sqrt(2) would be the exact value)
	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	// If the player is within this distance, lag compensate them in case they're running past us.
	if (vHisOrigin.DistTo(vMyOrigin) < maxDistance)
		return true;

	// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
	Vector vForward;
	AngleVectors(pCmd->viewangles, &vForward);

	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize(vDiff);

	float flCosAngle = 0.707107f;	// 45 degree angle
	if (vForward.Dot(vDiff) < flCosAngle)
		return false;

	return true;
}

Activity CHL2MP_Player::TranslateTeamActivity(Activity ActToTranslate)
{
	if (m_iModelType == TEAM_COMBINE)
		return ActToTranslate;

	if (ActToTranslate == ACT_RUN)
		return ACT_RUN_AIM_AGITATED;

	if (ActToTranslate == ACT_IDLE)
		return ACT_IDLE_AIM_AGITATED;

	if (ActToTranslate == ACT_WALK)
		return ACT_WALK_AIM_AGITATED;

	return ActToTranslate;
}

extern ConVar hl2_normspeed;

// Set the activity based on an event or current state
void CHL2MP_Player::SetAnimation(PLAYER_ANIM playerAnim)
{
	int animDesired;

	float speed;

	speed = GetAbsVelocity().Length2D();


	// bool bRunning = true;

	//Revisit!
/*	if ( ( m_nButtons & ( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) ) )
	{
		if ( speed > 1.0f && speed < hl2_normspeed.GetFloat() - 20.0f )
		{
			bRunning = false;
		}
	}*/

	if (GetFlags() & (FL_FROZEN | FL_ATCONTROLS))
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	Activity idealActivity = ACT_HL2MP_RUN;

	// This could stand to be redone. Why is playerAnim abstracted from activity? (sjb)
	if (playerAnim == PLAYER_JUMP)
	{
		idealActivity = ACT_HL2MP_JUMP;
	}
	else if (playerAnim == PLAYER_DIE)
	{
		if (m_lifeState == LIFE_ALIVE)
		{
			return;
		}
	}
	else if (playerAnim == PLAYER_ATTACK1)
	{
		if (GetActivity() == ACT_HOVER ||
			GetActivity() == ACT_SWIM ||
			GetActivity() == ACT_HOP ||
			GetActivity() == ACT_LEAP ||
			GetActivity() == ACT_DIESIMPLE)
		{
			idealActivity = GetActivity();
		}
		else
		{
			idealActivity = ACT_HL2MP_GESTURE_RANGE_ATTACK;
		}
	}
	else if (playerAnim == PLAYER_RELOAD)
	{
		idealActivity = ACT_HL2MP_GESTURE_RELOAD;
	}
	else if (playerAnim == PLAYER_IDLE || playerAnim == PLAYER_WALK)
	{
		if (!(GetFlags() & FL_ONGROUND) && GetActivity() == ACT_HL2MP_JUMP)	// Still jumping
		{
			idealActivity = GetActivity();
		}
		/*
		else if ( GetWaterLevel() > 1 )
		{
			if ( speed == 0 )
				idealActivity = ACT_HOVER;
			else
				idealActivity = ACT_SWIM;
		}
		*/
		else
		{
			if (GetFlags() & FL_ANIMDUCKING)
			{
				if (speed > 0)
				{
					idealActivity = ACT_HL2MP_WALK_CROUCH;
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE_CROUCH;
				}
			}
			else
			{
				if (speed > 0)
				{
					/*
					if ( bRunning == false )
					{
						idealActivity = ACT_WALK;
					}
					else
					*/
					{
						idealActivity = ACT_HL2MP_RUN;
					}
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE;
				}
			}
		}

		idealActivity = TranslateTeamActivity(idealActivity);
	}

	if (idealActivity == ACT_HL2MP_GESTURE_RANGE_ATTACK)
	{
		RestartGesture(Weapon_TranslateActivity(idealActivity));

		// FIXME: this seems a bit wacked
		//
		// misyl: it was and was causing a pred error every time.
		// the weapons already call SendWeaponAnim with the right activity.
		//Weapon_SetActivity( Weapon_TranslateActivity( ACT_RANGE_ATTACK1 ), 0 );

		return;
	}
	else if (idealActivity == ACT_HL2MP_GESTURE_RELOAD)
	{
		RestartGesture(Weapon_TranslateActivity(idealActivity));
		return;
	}
	else
	{
		SetActivity(idealActivity);

		animDesired = SelectWeightedSequence(Weapon_TranslateActivity(idealActivity));

		if (animDesired == -1)
		{
			animDesired = SelectWeightedSequence(idealActivity);

			if (animDesired == -1)
			{
				animDesired = 0;
			}
		}

		// Already using the desired animation?
		if (GetSequence() == animDesired)
			return;

		m_flPlaybackRate = 1.0;
		ResetSequence(animDesired);
		SetCycle(0);
		return;
	}

	// Already using the desired animation?
	if (GetSequence() == animDesired)
		return;

	//Msg( "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	ResetSequence(animDesired);
	SetCycle(0);
}


extern int	gEvilImpulse101;
//-----------------------------------------------------------------------------
// Purpose: Player reacts to bumping a weapon. 
// Input  : pWeapon - the weapon that the player bumped into.
// Output : Returns true if player picked up the weapon
//-----------------------------------------------------------------------------
bool CHL2MP_Player::BumpWeapon(CBaseCombatWeapon* pWeapon)
{
	CBaseCombatCharacter* pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if (!IsAllowedToPickupWeapons())
		return false;

	if (pOwner || !Weapon_CanUse(pWeapon) || !g_pGameRules->CanHavePlayerItem(this, pWeapon))
	{
		if (gEvilImpulse101)
		{
			UTIL_Remove(pWeapon);
		}
		return false;
	}

	// Don't let the player fetch weapons through walls (use MASK_SOLID so that you can't pickup through windows)
	if (!GetAllowPickupWeaponThroughObstacle())
	{
		// Don't let the player fetch weapons through walls (use MASK_SOLID so that you can't pickup through windows)
		if (!pWeapon->FVisible(this, MASK_SOLID) && !(GetFlags() & FL_NOTARGET))
		{
			return false;
		}
	}

	bool bOwnsWeaponAlready = !!Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType());

	if (bOwnsWeaponAlready == true)
	{
		//If we have room for the ammo, then "take" the weapon too.
		if (Weapon_EquipAmmoOnly(pWeapon))
		{
			pWeapon->CheckRespawn();

			UTIL_Remove(pWeapon);
			return true;
		}
		else
		{
			return false;
		}
	}

	pWeapon->CheckRespawn();
	Weapon_Equip(pWeapon);

	return true;
}

void CHL2MP_Player::LadderRespawnFix()
{
	if (auto lm = GetLadderMove())
	{
		if (lm->m_bForceLadderMove)
		{
			lm->m_bForceLadderMove = false;
			if (lm->m_hReservedSpot)
			{
				UTIL_Remove((CBaseEntity*)lm->m_hReservedSpot.Get());
				lm->m_hReservedSpot = NULL;
			}
		}
	}
}

void CHL2MP_Player::ChangeTeam(int iTeam)
{
	LadderRespawnFix();

	bool bKill = false;
	bool bWasSpectator = false;
	bool bIsDead = !IsAlive();
	bool bTeamplay = GameRules()->IsTeamplay();
	int iPrevTeam = GetTeamNumber();

	if (HL2MPRules()->IsTeamplay() != true && iTeam != TEAM_SPECTATOR)
	{
		//don't let them try to join combine or rebels during deathmatch.
		iTeam = TEAM_UNASSIGNED;
	}

	if (GetTeamNumber() == TEAM_SPECTATOR)
	{
		bWasSpectator = true;
	}

	BaseClass::ChangeTeam(iTeam);

	iTeam = GetTeamNumber();

	if (HL2MPRules()->IsTeamplay())
	{
		SetPlayerTeamModel();
		if (iPrevTeam != TEAM_UNASSIGNED)
		{
			bKill = false;
		}
	}
	else
	{
		SetPlayerModel();
	}

	if (iPrevTeam == iTeam)
		return;

	m_flNextTeamChangeTime = gpGlobals->curtime + TEAM_CHANGE_INTERVAL;

	if (HL2MPRules()->IsTeamplay())
	{
		SetPlayerTeamModel();
	}
	else
	{
		SetPlayerModel();
	}

	if (bWasSpectator)
	{
		Spawn();
		return;
	}

	DetonateTripmines();
	ClearUseEntity();

	if (iTeam == TEAM_SPECTATOR)
	{
		CBaseCombatWeapon* pWeapon = GetActiveWeapon();

		if (pWeapon && !pWeapon->Holster())
		{
			CWeaponPhysCannon* physcannon = dynamic_cast<CWeaponPhysCannon*>(pWeapon);

			if (physcannon)
			{
				physcannon->ForceDrop();
				physcannon->DestroyEffects();
			}
		}
		else
			ForceDropOfCarriedPhysObjects(NULL);

		StopZooming();
		SetSuitUpdate(NULL, false, 0);

		RemoveAllItems(true);

		if (FlashlightIsOn())
			FlashlightTurnOff();

		State_Transition(STATE_OBSERVER_MODE);
	}

	if (IsInAVehicle())
	{
		LeaveVehicle();
	}

	/*
		if ( bTeamplay && !bIsDead && !IsCompensatingScoreOnTeamSwitch() && iTeam != TEAM_SPECTATOR )
		{
			IncrementFragCount( 1 );
			IncrementDeathCount( -1 );

			CompensateScoreOnTeamSwitch( true );
		}
	 */

	if (bKill == true)
	{
		CommitSuicide();
		CTakeDamageInfo dmgInfo(this, this, 1000, DMG_GENERIC);
		TakeDamage(dmgInfo);
	}
}

bool CHL2MP_Player::HandleCommand_JoinTeam(int team)
{
	if (team == TEAM_SPECTATOR && IsHLTV())
	{
		ChangeTeam(TEAM_SPECTATOR);
		ResetDeathCount();
		ResetFragCount();
		return true;
	}

	if (!GetGlobalTeam(team) || team == 0)
	{
		ClientPrint(this, HUD_PRINTCONSOLE, "Please enter a valid team index\n");
		return false;
	}

	// Don't do anything if you join your own team
	if (team == GetTeamNumber())
	{
		return false;
	}

	// end early
	if (this->GetTeamNumber() == TEAM_SPECTATOR)
	{
		ChangeTeam(team);
		return true;
	}

	if (team == TEAM_SPECTATOR)
	{
		// Prevent this is the cvar is set
		if (!mp_allowspectators.GetInt())
		{
			ClientPrint(this, HUD_PRINTCENTER, "#Cannot_Be_Spectator");
			return false;
		}

		/*
		if ( GetTeamNumber() != TEAM_UNASSIGNED && !IsDead() )
		{
			m_fNextSuicideTime = gpGlobals->curtime;	// allow the suicide to work

			CommitSuicide();

			// add 1 to frags to balance out the 1 subtracted for killing yourself
			IncrementFragCount( 1 );
		}
		*/
		ChangeTeam(TEAM_SPECTATOR);

		return true;
	}
	else
	{
		StopObserverMode();
		State_Transition(STATE_ACTIVE);
	}

	// Switch their actual team...
	ChangeTeam(team);

	return true;
}

/*
void CHL2MP_Player::SendFOVCommand(int fov)
{
	char command[64];
	Q_snprintf(command, sizeof(command), "fov %d\n", fov);
	engine->ClientCommand(edict(), command);
	ClientPrint(this, HUD_PRINTTALK, "test");
}
*/

bool CHL2MP_Player::ClientCommand(const CCommand& args)
{
	if (FStrEq(args[0], "spectate"))
	{
		if (ShouldRunRateLimitedCommand(args))
		{
			// instantly join spectators
			HandleCommand_JoinTeam(TEAM_SPECTATOR);
		}
		return true;
	}
	else if (FStrEq(args[0], "jointeam"))
	{
		if (ShouldRunRateLimitedCommand(args))
		{
			int iTeam = atoi(args[1]);
			HandleCommand_JoinTeam(iTeam);
		}
		return true;
	}
	else if (FStrEq(args[0], "joingame"))
	{
		if (GetTeamNumber() == TEAM_SPECTATOR)
			ChangeTeam(random->RandomInt(2, 3));

		return true;
	}

	return BaseClass::ClientCommand(args);
}

void CHL2MP_Player::CheatImpulseCommands(int iImpulse)
{
	switch (iImpulse)
	{
	case 101:
	{
		if (sv_cheats->GetBool())
		{
			GiveAllItems();
		}
	}
	break;

	default:
		BaseClass::CheatImpulseCommands(iImpulse);
	}
}

bool CHL2MP_Player::ShouldRunRateLimitedCommand(const CCommand& args)
{
	int i = m_RateLimitLastCommandTimes.Find(args[0]);
	if (i == m_RateLimitLastCommandTimes.InvalidIndex())
	{
		m_RateLimitLastCommandTimes.Insert(args[0], gpGlobals->curtime);
		return true;
	}
	else if ((gpGlobals->curtime - m_RateLimitLastCommandTimes[i]) < HL2MP_COMMAND_MAX_RATE)
	{
		// Too fast.
		return false;
	}
	else
	{
		m_RateLimitLastCommandTimes[i] = gpGlobals->curtime;
		return true;
	}
}

void CHL2MP_Player::CreateViewModel(int index /*=0*/)
{
	Assert(index >= 0 && index < MAX_VIEWMODELS);

	if (GetViewModel(index))
		return;

	CPredictedViewModel* vm = (CPredictedViewModel*)CreateEntityByName("predicted_viewmodel");
	if (vm)
	{
		vm->SetAbsOrigin(GetAbsOrigin());
		vm->SetOwner(this);
		vm->SetIndex(index);
		DispatchSpawn(vm);
		vm->FollowEntity(this, false);
		m_hViewModel.Set(index, vm);
	}
}

bool CHL2MP_Player::BecomeRagdollOnClient(const Vector& force)
{
	return true;
}

// -------------------------------------------------------------------------------- //
// Ragdoll entities.
// -------------------------------------------------------------------------------- //

class CHL2MPRagdoll : public CBaseAnimatingOverlay
{
public:
	DECLARE_CLASS(CHL2MPRagdoll, CBaseAnimatingOverlay);
	DECLARE_SERVERCLASS();

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState(FL_EDICT_ALWAYS);
	}

public:
	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle(CBaseEntity, m_hPlayer);	// networked entity handle 
	CNetworkVector(m_vecRagdollVelocity);
	CNetworkVector(m_vecRagdollOrigin);
};

LINK_ENTITY_TO_CLASS(hl2mp_ragdoll, CHL2MPRagdoll);

IMPLEMENT_SERVERCLASS_ST_NOBASE(CHL2MPRagdoll, DT_HL2MPRagdoll)
SendPropVector(SENDINFO(m_vecRagdollOrigin), -1, SPROP_COORD),
SendPropEHandle(SENDINFO(m_hPlayer)),
SendPropModelIndex(SENDINFO(m_nModelIndex)),
SendPropInt(SENDINFO(m_nForceBone), 8, 0),
SendPropVector(SENDINFO(m_vecForce), -1, SPROP_NOSCALE),
SendPropVector(SENDINFO(m_vecRagdollVelocity))
END_SEND_TABLE()


void CHL2MP_Player::CreateRagdollEntity(void)
{
	if (m_hRagdoll)
	{
		UTIL_RemoveImmediate(m_hRagdoll);
		m_hRagdoll = NULL;
	}

	// If we already have a ragdoll, don't make another one.
	CHL2MPRagdoll* pRagdoll = dynamic_cast<CHL2MPRagdoll*>(m_hRagdoll.Get());

	if (!pRagdoll)
	{
		// create a new one
		pRagdoll = dynamic_cast<CHL2MPRagdoll*>(CreateEntityByName("hl2mp_ragdoll"));
	}

	if (pRagdoll)
	{
		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->m_vecForce = m_vecTotalBulletForce;
		pRagdoll->SetAbsOrigin(GetAbsOrigin());
	}

	// ragdolls will be removed on round restart automatically
	m_hRagdoll = pRagdoll;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL2MP_Player::FlashlightIsOn(void)
{
	return IsEffectActive(EF_DIMLIGHT);
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2MP_Player::FlashlightTurnOn(void)
{
	if (flashlight.GetInt() > 0 && IsAlive())
	{
		AddEffects(EF_DIMLIGHT);
		EmitSound("HL2Player.FlashlightOn");
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2MP_Player::FlashlightTurnOff(void)
{
	RemoveEffects(EF_DIMLIGHT);

	if (IsAlive())
	{
		EmitSound("HL2Player.FlashlightOff");
	}
}

void CHL2MP_Player::Weapon_Drop(CBaseCombatWeapon* pWeapon, const Vector* pvecTarget, const Vector* pVelocity)
{
	//Drop a grenade if it's primed.
	if (GetActiveWeapon())
	{
		CBaseCombatWeapon* pGrenade = Weapon_OwnsThisType("weapon_frag");

		if (GetActiveWeapon() == pGrenade)
		{
			if ((m_nButtons & IN_ATTACK) || (m_nButtons & IN_ATTACK2))
			{
				DropPrimedFragGrenade(this, pGrenade);
				return;
			}
		}
	}

	BaseClass::Weapon_Drop(pWeapon, pvecTarget, pVelocity);
}

int CHL2MP_Player::GetMaxAmmo(int iAmmoIndex) const
{
	if (iAmmoIndex == -1)
		return 0;

	if (GetAmmoDef()->MaxCarry(iAmmoIndex) == INFINITE_AMMO)
		return 999;

	return GetAmmoDef()->MaxCarry(iAmmoIndex);
}

void CHL2MP_Player::DetonateTripmines(void)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname(pEntity, "npc_satchel")) != NULL)
	{
		CSatchelCharge* pSatchel = dynamic_cast<CSatchelCharge*>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() == this)
		{
			g_EventQueue.AddEvent(pSatchel, "Explode", 0.20, this, this);
		}
	}
}


int CHL2MP_Player::OnTakeDamage(const CTakeDamageInfo& inputInfo)
{
	// Pega o atirador a partir das informações de dano
	CHL2MP_Player* pAttacker = ToHL2MPPlayer(inputInfo.GetAttacker());

	// Se o golpe NÃO for fatal, pedimos para o atirador tocar o som de ACERTO.
	if (pAttacker && pAttacker != this && GetHealth() > inputInfo.GetDamage())
	{
		pAttacker->OnDamageDealt(this, inputInfo);

		// Show damage display for non-fatal damage
		if (HL2MPRules())
		{
			int healthAfterDamage = GetHealth() - (int)inputInfo.GetDamage();
			if (healthAfterDamage < 0) healthAfterDamage = 0;
			HL2MPRules()->ShowDamageDisplay(pAttacker, this, (int)inputInfo.GetDamage(), false, healthAfterDamage);
		}
	}

	// Continua com a lógica original de dano da Source Engine
	return BaseClass::OnTakeDamage(inputInfo);
}


/*
void CHL2MP_Player::DeathSound( const CTakeDamageInfo &info )
{
	if ( m_hRagdoll && m_hRagdoll->GetBaseAnimating()->IsDissolving() )
		 return;

	char szDeathSound[128];

	Q_snprintf(szDeathSound, sizeof(szDeathSound ), "%s.Die", GetPlayerModelSoundPrefix() );

	const char *pModelName = STRING( GetModelName() );

	CSoundParameters params;
	if ( GetParametersForSound( szDeathSound, params, pModelName ) == false )
		return;

	Vector vecOrigin = GetAbsOrigin();

	CRecipientFilter filter;
	filter.AddRecipientsByPAS( vecOrigin );

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep );
}*/
void CHL2MP_Player::DeathSound(const CTakeDamageInfo& info)
{
	if (m_hRagdoll && m_hRagdoll->GetBaseAnimating()->IsDissolving())
		return;

	const char* pModelName = STRING(GetModelName());

	// Îïðåäåëÿåì ïðåôèêñ îçâó÷êè â çàâèñèìîñòè îò ìîäåëè
	const char* szPrefix = "NPC_Citizen";

	if (Q_stristr(pModelName, "police"))
		szPrefix = "NPC_MetroPolice";
	else if (Q_stristr(pModelName, "combine"))
		szPrefix = "NPC_CombineS";

	char szDeathSound[128];
	Q_snprintf(szDeathSound, sizeof(szDeathSound), "%s.Die", szPrefix);

	CSoundParameters params;
	if (!GetParametersForSound(szDeathSound, params, pModelName))
		return;

	Vector vecOrigin = GetAbsOrigin();
	CRecipientFilter filter;
	filter.AddRecipientsByPAS(vecOrigin);

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound(filter, entindex(), ep);
}
CBaseEntity* CHL2MP_Player::EntSelectSpawnPoint(void)
{
	CBaseEntity* pSpot = NULL;
	CBaseEntity* pLastSpawnPoint = g_pLastSpawn;
	const char* pSpawnpointName = "info_player_deathmatch";

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_COMBINE)
		{
			pSpawnpointName = "info_player_combine";
			pLastSpawnPoint = g_pLastCombineSpawn;
		}
		else if (GetTeamNumber() == TEAM_REBELS)
		{
			pSpawnpointName = "info_player_rebel";
			pLastSpawnPoint = g_pLastRebelSpawn;
		}

		if (gEntList.FindEntityByClassname(NULL, pSpawnpointName) == NULL)
		{
			pSpawnpointName = "info_player_deathmatch";
			pLastSpawnPoint = g_pLastSpawn;
		}
	}

	pSpot = pLastSpawnPoint;
	// Randomize the start spot
	for (int i = random->RandomInt(1, 5); i > 0; i--)
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
	if (!pSpot)  // skip over the null point
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);

	CBaseEntity* pFirstSpot = pSpot;

	do
	{
		if (pSpot)
		{
			// check if pSpot is valid
			if (g_pGameRules->IsSpawnPointValid(pSpot, this))
			{
				if (pSpot->GetLocalOrigin() == vec3_origin)
				{
					pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
					continue;
				}

				// if so, go to pSpot
				goto ReturnSpot;
			}
		}
		// increment pSpot
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
	} while (pSpot != pFirstSpot); // loop if we're not back to the start

	// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
	if (pSpot && (TEAM_SPECTATOR != GetPlayerInfo()->GetTeamIndex()))
	{
		goto ReturnSpot;
	}

ReturnSpot:

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_COMBINE)
		{
			g_pLastCombineSpawn = pSpot;
		}
		else if (GetTeamNumber() == TEAM_REBELS)
		{
			g_pLastRebelSpawn = pSpot;
		}
	}

	if (!pSpot)
	{
		pSpot = gEntList.FindEntityByClassnameRandom(pSpot, "info_player_start");

		if (pSpot)
			goto ReturnSpot;
		else
			return CBaseEntity::Instance(INDEXENT(0));
	}

	g_pLastSpawn = pSpot;

	m_flSlamProtectTime = gpGlobals->curtime + 0.9;

	return pSpot;
}


CON_COMMAND(timeleft, "prints the time remaining in the match")
{
	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());

	int iTimeRemaining = (int)HL2MPRules()->GetMapRemainingTime();

	if (iTimeRemaining == 0)
	{
		if (pPlayer)
		{
			ClientPrint(pPlayer, HUD_PRINTTALK, "This game has no timelimit.");
		}
		else
		{
			Msg("* No Time Limit *\n");
		}
	}
	else
	{
		int iMinutes, iSeconds;
		iMinutes = iTimeRemaining / 60;
		iSeconds = iTimeRemaining % 60;

		char minutes[8];
		char seconds[8];

		Q_snprintf(minutes, sizeof(minutes), "%d", iMinutes);
		Q_snprintf(seconds, sizeof(seconds), "%2.2d", iSeconds);

		if (pPlayer)
		{
			ClientPrint(pPlayer, HUD_PRINTTALK, "Time left in map: %s1:%s2", minutes, seconds);
		}
		else
		{
			Msg("Time Remaining:  %s:%s\n", minutes, seconds);
		}
	}
}


void CHL2MP_Player::Reset()
{
	ResetDeathCount();
	ResetFragCount();
}

bool CHL2MP_Player::IsReady()
{
	return m_bReady;
}

void CHL2MP_Player::SetReady(bool bReady)
{
	m_bReady = bReady;
}

void CHL2MP_Player::CheckChatText(char* p, int bufsize)
{
	CHL2MP_Admin::CheckChatText(p, bufsize);

	// Check for FOV command
	// Check for FOV command
	if (Q_strnicmp(p, "!fov", 4) == 0)
	{
		HandleFOVCommand(p);
	}

	//Look for escape sequences and replace
	char* buf = new char[bufsize];
	int pos = 0;

	// Parse say text for escape sequences
	for (char* pSrc = p; pSrc != NULL && *pSrc != 0 && pos < bufsize - 1; pSrc++)
	{
		// copy each char across
		buf[pos] = *pSrc;
		pos++;
	}

	buf[pos] = '\0';

	// copy buf back into p
	Q_strncpy(p, buf, bufsize);
	delete[] buf;

	const char* pReadyCheck = p;
	HL2MPRules()->CheckChatForReadySignal(this, pReadyCheck);
}

void CHL2MP_Player::State_Transition(HL2MPPlayerState newState)
{
	State_Leave();
	State_Enter(newState);
}


void CHL2MP_Player::State_Enter(HL2MPPlayerState newState)
{
	m_iPlayerState = newState;
	m_pCurStateInfo = State_LookupInfo(newState);

	// Initialize the new state.
	if (m_pCurStateInfo && m_pCurStateInfo->pfnEnterState)
		(this->*m_pCurStateInfo->pfnEnterState)();
}


void CHL2MP_Player::State_Leave()
{
	if (m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState)
	{
		(this->*m_pCurStateInfo->pfnLeaveState)();
	}
}


void CHL2MP_Player::State_PreThink()
{
	if (m_pCurStateInfo && m_pCurStateInfo->pfnPreThink)
	{
		(this->*m_pCurStateInfo->pfnPreThink)();
	}
}


CHL2MPPlayerStateInfo* CHL2MP_Player::State_LookupInfo(HL2MPPlayerState state)
{
	// This table MUST match the 
	static CHL2MPPlayerStateInfo playerStateInfos[] =
	{
		{ STATE_ACTIVE,			"STATE_ACTIVE",			&CHL2MP_Player::State_Enter_ACTIVE, NULL, &CHL2MP_Player::State_PreThink_ACTIVE },
		{ STATE_OBSERVER_MODE,	"STATE_OBSERVER_MODE",	&CHL2MP_Player::State_Enter_OBSERVER_MODE,	NULL, &CHL2MP_Player::State_PreThink_OBSERVER_MODE }
	};

	for (int i = 0; i < ARRAYSIZE(playerStateInfos); i++)
	{
		if (playerStateInfos[i].m_iPlayerState == state)
			return &playerStateInfos[i];
	}

	return NULL;
}

bool CHL2MP_Player::StartObserverMode(int mode)
{
	//we only want to go into observer mode if the player asked to, not on a death timeout
	if (m_bEnterObserver == true)
	{
		VPhysicsDestroyObject();
		return BaseClass::StartObserverMode(mode);
	}
	return false;
}

void CHL2MP_Player::StopObserverMode()
{
	m_bEnterObserver = false;
	BaseClass::StopObserverMode();
}

void CHL2MP_Player::State_Enter_OBSERVER_MODE()
{
	int observerMode = m_iObserverLastMode;
	if (IsNetClient())
	{
		const char* pIdealMode = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_spec_mode");
		if (pIdealMode)
		{
			observerMode = atoi(pIdealMode);
			if (observerMode <= OBS_MODE_FIXED || observerMode > OBS_MODE_ROAMING)
			{
				observerMode = m_iObserverLastMode;
			}
		}
	}
	m_bEnterObserver = true;
	StartObserverMode(observerMode);
}

void CHL2MP_Player::State_PreThink_OBSERVER_MODE()
{
	// Make sure nobody has changed any of our state.
	//	Assert( GetMoveType() == MOVETYPE_FLY );
	Assert(m_takedamage == DAMAGE_NO);
	Assert(IsSolidFlagSet(FSOLID_NOT_SOLID));
	//	Assert( IsEffectActive( EF_NODRAW ) );

	// Must be dead.
	Assert(m_lifeState == LIFE_DEAD);
	Assert(pl.deadflag);
}


void CHL2MP_Player::State_Enter_ACTIVE()
{
	SetMoveType(MOVETYPE_WALK);

	// md 8/15/07 - They'll get set back to solid when they actually respawn. If we set them solid now and mp_forcerespawn
	// is false, then they'll be spectating but blocking live players from moving.
	// RemoveSolidFlags( FSOLID_NOT_SOLID );

	m_Local.m_iHideHUD = 0;
}


void CHL2MP_Player::State_PreThink_ACTIVE()
{
	//we don't really need to do anything here. 
	//This state_prethink structure came over from CS:S and was doing an assert check that fails the way hl2dm handles death
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2MP_Player::CanHearAndReadChatFrom(CBasePlayer* pPlayer)
{
	// can always hear the console unless we're ignoring all chat
	if (!pPlayer)
		return false;

	return true;
}

//-----------------------------------------------------------------------------------------------------
// Return true if the given threat is aiming in our direction
bool CHL2MP_Player::IsThreatAimingTowardMe(CBaseEntity* threat, float cosTolerance) const
{
	CHL2MP_Player* player = ToHL2MPPlayer(threat);
	Vector to = GetAbsOrigin() - threat->GetAbsOrigin();
	Vector forward;

	if (player == NULL)
	{
		return false;
	}

	// is the player pointing at me?
	player->EyeVectors(&forward);

	if (DotProduct(to, forward) > cosTolerance)
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
// Return true if the given threat is aiming in our direction and firing its weapon
bool CHL2MP_Player::IsThreatFiringAtMe(CBaseEntity* threat) const
{
	if (IsThreatAimingTowardMe(threat))
	{
		CHL2MP_Player* player = ToHL2MPPlayer(threat);

		if (player)
		{
			return player->IsFiringWeapon();
		}
	}

	return false;
}

bool CHL2MP_Player::SavePlayerSettings()
{
	if (IsBot())
		return false;

	const char* steamID3 = engine->GetPlayerNetworkIDString(edict());

	if ((V_stristr(steamID3, "BOT") != nullptr))
		return false;
	uint64 steamID64 = ConvertSteamID3ToSteamID64(steamID3);

	char filename[MAX_PATH];
	Q_snprintf(filename, sizeof(filename), "cfg/core/%llu.txt", steamID64);

	if (!filesystem->FileExists("cfg/core", "GAME"))
	{
		filesystem->CreateDirHierarchy("cfg/core", "GAME");
	}

	KeyValues* kv = new KeyValues("Settings");

	kv->LoadFromFile(filesystem, filename, "MOD");

	KeyValues* playerSettings = kv->FindKey(UTIL_VarArgs("%llu", steamID64), true);
	playerSettings->SetInt("FOV", GetNewFOV());

	if (kv->SaveToFile(filesystem, filename, "MOD"))
	{
		DevMsg("Player settings saved successfully in cfg/core/.\n");
	}
	else
	{
		Warning("Failed to save player settings in cfg/core/.\n");
	}

	kv->deleteThis();
	return true;
}

bool CHL2MP_Player::LoadPlayerSettings()
{
	if (IsBot())
		return false;

	const char* steamID3 = engine->GetPlayerNetworkIDString(edict());
	uint64 steamID64 = ConvertSteamID3ToSteamID64(steamID3);

	char filename[MAX_PATH];
	Q_snprintf(filename, sizeof(filename), "cfg/core/%llu.txt", steamID64);

	KeyValues* kv = new KeyValues("Settings");

	if (!kv->LoadFromFile(filesystem, filename, "MOD"))
	{
		Warning("Couldn't load settings from file %s, creating a new one with default values...\n", filename);

		SetNewFOV(90);

		KeyValues* playerSettings = new KeyValues(UTIL_VarArgs("%llu", steamID64));
		playerSettings->SetInt("FOV", GetNewFOV());

		kv->AddSubKey(playerSettings);

		if (kv->SaveToFile(filesystem, filename, "MOD"))
		{
			Msg("Default settings for player %s have been saved to %s\n", steamID3, filename);
		}
		else
		{
			Warning("Failed to save default settings to file %s\n", filename);
			kv->deleteThis();
			return false;
		}

		kv->deleteThis();
		return true;
	}

	KeyValues* playerSettings = kv->FindKey(UTIL_VarArgs("%llu", steamID64));
	if (!playerSettings)
	{
		Warning("Player settings not found in file %s\n", filename);
		kv->deleteThis();
		return false;
	}

	SetNewFOV(playerSettings->GetInt("FOV", GetNewFOV()));

	kv->deleteThis();
	return true;
}

void CHL2MP_Player::Event_Killed(const CTakeDamageInfo& info)
{
	if (g_pSpawnWeaponsManager)
	{
		g_pSpawnWeaponsManager->OnPlayerDeath(this);
	}
	// ====================================================================================
	// DETECÇÃO DE AIRKILL (VERSÃO FINAL E SIMPLIFICADA)
	// ====================================================================================
	bool bWasAirKill = false;
	if (sv_killerinfo_airkill_enable.GetBool())
	{
		// A lógica agora é simples: um Airkill acontece se UMA das duas condições for verdadeira.

		// Condição 1: A vítima tem velocidade vertical alta? (Pega rocket jumps e quedas rápidas)
		Vector velocity = this->GetAbsVelocity();
		float verticalSpeed = fabs(velocity.z);
		if (verticalSpeed > sv_killerinfo_airkill_velocity_threshold.GetFloat())
		{
			bWasAirKill = true;
		}
		// Condição 2: Se não, a vítima está a uma altura mínima do chão? (Pega pulos normais e jogadores planando)
		else
		{
			trace_t height_tr;
			Vector vecStart = this->GetAbsOrigin();
			Vector vecEnd = vecStart - Vector(0, 0, sv_killerinfo_airkill_height_threshold.GetFloat());
			UTIL_TraceLine(vecStart, vecEnd, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &height_tr);

			// Se o trace longo (cuja distância é controlada pela ConVar) não bateu em nada, é Airkill.
			if (height_tr.fraction > 0.99f)
			{
				bWasAirKill = true;
			}
		}
	}

	// Lógica de morte original (continua a partir daqui)
	LadderRespawnFix();
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce(m_vecTotalBulletForce);
	SetNumAnimOverlays(0);
	CreateRagdollEntity();
	DetonateTripmines();

	BaseClass::Event_Killed(subinfo);

	if (info.GetDamageType() & DMG_DISSOLVE)
	{
		if (m_hRagdoll)
		{
			m_hRagdoll->GetBaseAnimating()->Dissolve(NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL);
		}
	}

	if (HL2MPRules()->IsTeamplay())
	{
		CBaseEntity* pAttackerEnt = info.GetAttacker();
		if (pAttackerEnt && !pAttackerEnt->InSameTeam(this) && pAttackerEnt->GetTeamNumber() != TEAM_UNASSIGNED)
			pAttackerEnt->GetTeam()->AddScore(1);
		else
			GetTeam()->AddScore(-1);
	}

	FlashlightTurnOff();
	m_lifeState = LIFE_DEAD;
	RemoveEffects(EF_NODRAW);
	StopZooming();

	// Lógica de som
	CHL2MP_Player* pAttacker = ToHL2MPPlayer(info.GetAttacker());
	if (pAttacker && pAttacker != this)
	{
		pAttacker->OnDamageDealt(this, info);
	}

	// Sistema de killer info
	CHL2MP_Player* pKiller = dynamic_cast<CHL2MP_Player*>(info.GetAttacker());
	if (pKiller && pKiller != this && pKiller->IsPlayer())
	{
		const char* weaponName = "world";
		CBaseEntity* pInflictor = info.GetInflictor();
		if (pInflictor)
		{
			weaponName = pInflictor->GetClassname();
			if (Q_strnicmp(weaponName, "weapon_", 7) == 0)
				weaponName += 7;
			else if (FClassnameIs(pInflictor, "crossbow_bolt"))
				weaponName = "crossbow";
		}

		int hitGroup = this->m_iLastHitGroup;

		bool bWasBounceKill = (sv_killerinfo_bouncekill_enable.GetBool() && HL2MPRules()->WasBounceKill(info));

		// <<< ADICIONADO: Pega o número de ricochetes da flecha >>>
		int iBounceCount = HL2MPRules()->GetBounceCount(info);

		// <<< MODIFICADO: Passa o iBounceCount como o último parâmetro >>>
		HL2MPRules()->DisplayKillerInfo(this, pKiller, weaponName, hitGroup, bWasAirKill, bWasBounceKill, iBounceCount);
	}
}

void CHL2MP_Player::HandleFOVCommand(const char* command)
{
	// Skip "!fov"
	const char* args = command + 4;

	// Skip whitespace
	while (*args == ' ' || *args == '\t') args++;

	// If no arguments, show current FOV
	if (*args == '\0')
	{
		int currentFOV = GetNewFOV();

		CSingleUserRecipientFilter filter(this);
		filter.MakeReliable();

		char message[128];
		Q_snprintf(message, sizeof(message),
			CHAT_FULLWHITE "Your current FOV is " CHAT_RED "%d", currentFOV);

		UTIL_SayText2Filter(filter, this, true, message, "", "", "", "");
		return;
	}

	// Try to parse the FOV value
	char* endPtr;
	int newFOV = strtol(args, &endPtr, 10);

	// Get min/max from CVars
	int minFOV = sv_fov_min.GetInt();
	int maxFOV = sv_fov_max.GetInt();

	// Check if parsing failed (non-numeric input)
	if (endPtr == args || (*endPtr != '\0' && *endPtr != ' ' && *endPtr != '\t'))
	{
		// Invalid input - not a number
		CSingleUserRecipientFilter filter(this);
		filter.MakeReliable();

		char message[256];
		Q_snprintf(message, sizeof(message),
			CHAT_FULLWHITE "Please use a number from " CHAT_RED "%d" CHAT_FULLWHITE " to " CHAT_RED "%d",
			minFOV, maxFOV);

		UTIL_SayText2Filter(filter, this, true, message, "", "", "", "");
		return;
	}

	// Check if FOV is within valid range
	if (newFOV < minFOV || newFOV > maxFOV)
	{
		CSingleUserRecipientFilter filter(this);
		filter.MakeReliable();

		char message[256];
		Q_snprintf(message, sizeof(message),
			CHAT_FULLWHITE "Please use a number from " CHAT_RED "%d" CHAT_FULLWHITE " to " CHAT_RED "%d",
			minFOV, maxFOV);

		UTIL_SayText2Filter(filter, this, true, message, "", "", "", "");
		return;
	}

	// Set the new FOV
	SetNewFOV(newFOV);
	SavePlayerSettings(); // Save immediately

	// Send confirmation message
	CSingleUserRecipientFilter filter(this);
	filter.MakeReliable();

	char message[128];
	Q_snprintf(message, sizeof(message),
		CHAT_FULLWHITE "Your FOV has been set to " CHAT_RED "%d", newFOV);

	UTIL_SayText2Filter(filter, this, true, message, "", "", "", "");

	// Apply the FOV change immediately
	ApplyFOVChange();
}

void CHL2MP_Player::ApplyFOVChange()
{
	// Send the FOV to the client
	char fovCommand[32];
	Q_snprintf(fovCommand, sizeof(fovCommand), "fov %d\n", GetNewFOV());
	engine->ClientCommand(edict(), fovCommand);
}

void CHL2MP_Player::ApplySpawnLoadoutThink()
{
	if (g_pSpawnWeaponsManager)
	{
		g_pSpawnWeaponsManager->ApplyPlayerLoadout(this);
	}
}