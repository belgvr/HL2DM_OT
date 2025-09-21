﻿//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hl2mp_gamerules.h"

#include "hl2mp_savescores.h" 

#include "viewport_panel_names.h"
#include "gameeventdefs.h"
#include <KeyValues.h>
#include "ammodef.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#else

#include "nav_mesh.h"
#include "eventqueue.h"
#include "player.h"
#include "gamerules.h"
#include "game.h"
#include "items.h"
#include "entitylist.h"
#include "mapentities.h"
#include "in_buttons.h"
#include <ctype.h>
#include "voice_gamemgr.h"
#include "iscorer.h"
#include "hl2mp_player.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "team.h"
#include "voice_gamemgr.h"
#include "hl2mp_gameinterface.h"
#include "hl2mp_cvars.h"
#include "hl2_player.h"
#include "game.h"

#include "shareddefs.h"

extern void respawn(CBaseEntity* pEdict, bool fCopyCorpse);

extern bool FindInList(const char** pStrings, const char* pToFind);

ConVar sv_hl2mp_weapon_respawn_time("sv_hl2mp_weapon_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_hl2mp_item_respawn_time("sv_hl2mp_item_respawn_time", "30", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_report_client_settings("sv_report_client_settings", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY);

// TimeLEft ConVars
ConVar sv_timeleft_enable("sv_timeleft_enable", "1", 0, "If non-zero,enables time left indication on the HUD.", true, 0.0, true, 1.0);
ConVar sv_timeleft_teamscore("sv_timeleft_teamscore", "1", 0, "If non-zero,enables team scores on the HUD (left Combine, right Rebels)\nMust be enabled to use \"sv_timeleft_color_override\".", true, 0.0, true, 1.0);
ConVar sv_timeleft_color("sv_timeleft_color", "255,180,0", 0, "RGB color for timeleft display (format: R,G,B)");
ConVar sv_timeleft_channel("sv_timeleft_channel", "0", 0, "Alpha/Intensity.", true, 0.0, true, 5.0);
ConVar sv_timeleft_x("sv_timeleft_x", "-1");
ConVar sv_timeleft_y("sv_timeleft_y", "0.01");

// Equalizer ConVars
ConVar sv_equalizer_combine_color("sv_equalizer_combine_color", "0,100,255", 0, "RGB color for Combine team in equalizer mode (format: R,G,B)");
ConVar sv_equalizer_rebel_color("sv_equalizer_rebel_color", "255,0,0", 0, "RGB color for Rebel team in equalizer mode (format: R,G,B)");
ConVar sv_equalizer_color("sv_equalizer_color", "255,80,0", 0, "RGB color for players in equalizer mode when teamplay is disabled (format: R,G,B)");
// Equalizer Model ConVars  
ConVar sv_equalizer_model_combine("sv_equalizer_model_combine", "models/combine_super_soldier.mdl", 0, "Model for Combine team in equalizer mode");
ConVar sv_equalizer_model_rebel("sv_equalizer_model_rebel", "models/kleiner.mdl", 0, "Model for Rebel team in equalizer mode");
ConVar sv_equalizer_model("sv_equalizer_model", "models/alyx.mdl", 0, "Model for all players when teamplay is disabled in equalizer mode");

//New or Old TargetID style.
ConVar sv_targetid_style("sv_targetid_style", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Target name display style. 0 = Original (name at the bottom), 1 = Custom (HUD at the center of the screen)");

// Damage Display ConVars
ConVar sv_damage_display("sv_damage_display", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable/disable damage display system. 0 = off, 1 = on");
ConVar sv_damage_display_area("sv_damage_display_area", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Area for damage text: 1 = center screen, 2 = hint area, 3 = chat area");
ConVar sv_damage_display_own("sv_damage_display_own", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show your own damage received. 0 = off, 1 = on");
ConVar sv_damage_display_ff("sv_damage_display_ff", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show friendly fire damage. 0 = off, 1 = on");
ConVar sv_damage_display_kill("sv_damage_display_kill", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show KILL message when killing enemy");
ConVar sv_damage_display_x("sv_damage_display_x", "-1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "X position for damage info. -1 = center, 0 = left, 1 = right");
ConVar sv_damage_display_y("sv_damage_display_y", "0.53", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Y position for damage info. -1 = center, 0 = bottom, 1 = top");
ConVar sv_damage_display_kill_x("sv_damage_display_kill_x", "-1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "X position for kill/special kill message");
ConVar sv_damage_display_kill_y("sv_damage_display_kill_y", "0.35", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Y position for kill/special kill message");
ConVar sv_damage_display_hold_time("sv_damage_display_hold_time", ".5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "How long damage info displays");
ConVar sv_damage_display_fx_style("sv_damage_display_fx_style", "2", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Effect style: 0/1 = fade, 2 = flash");
ConVar sv_damage_display_fadein_time("sv_damage_display_fadein_time", "0.025", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Fade in time for damage display");
ConVar sv_damage_display_fadeout_time("sv_damage_display_fadeout_time", "0.15", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Fade out time for damage display");
ConVar sv_damage_display_show_hp_left_on_kill("sv_damage_display_show_hp_left_on_kill", "0", FCVAR_ARCHIVE, "Shows enemy's remaining health on kill '[0 HP left]' 0 = off, 1 = on");

// Health-based color ConVars
ConVar sv_damage_display_color_high("sv_damage_display_color_high", "0,255,40", FCVAR_GAMEDLL | FCVAR_NOTIFY, "RGB color for high HP (75-99). Format: R,G,B");
ConVar sv_damage_display_color_medium("sv_damage_display_color_medium", "255,200,0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "RGB color for medium HP (27-74). Format: R,G,B");
ConVar sv_damage_display_color_low("sv_damage_display_color_low", "255,20,0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "RGB color for low HP (1-25). Format: R,G,B");
ConVar sv_damage_display_color_dead("sv_damage_display_color_dead", "0,200,255", FCVAR_GAMEDLL | FCVAR_NOTIFY, "RGB color for 0 HP. Format: R,G,B");
ConVar sv_damage_display_color_kill("sv_damage_display_color_kill", "255,20,0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "RGB color for KILL message. Format: R,G,B");

// HP thresholds
ConVar sv_damage_display_hp_high_min("sv_damage_display_hp_high_min", "75", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum HP for high color");
ConVar sv_damage_display_hp_medium_min("sv_damage_display_hp_medium_min", "26", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum HP for medium color");
ConVar sv_damage_display_hp_low_min("sv_damage_display_hp_low_min", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum HP for low color");

// ConVars for Special Kill Display
ConVar sv_damage_display_special_kills("sv_damage_display_special_kills", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable special announcements (Headshot, Airkill, etc.) instead of a generic KILL message.");
ConVar sv_damage_display_special_kills_additional_time("sv_damage_display_special_kills_additional_time", "0.5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Additional time (in seconds) that special kill messages are displayed.");
ConVar sv_damage_display_combo_style("sv_damage_display_combo_style", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "How to display combination kills. 0 = Single Line (HEADSHOT AIRKILL!), 1 = Stacked.");
ConVar sv_damage_display_combo_separator("sv_damage_display_combo_separator", " ", FCVAR_GAMEDLL | FCVAR_NOTIFY, "The character(s) to put between combo messages in single-line style.");

// ConVars for Special Kill Colors
ConVar sv_damage_display_color_headshot("sv_damage_display_color_headshot", "0,255,0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "RGB color for HEADSHOT message. Format: R,G,B");
ConVar sv_damage_display_color_airkill("sv_damage_display_color_airkill", "255,105,180", FCVAR_GAMEDLL | FCVAR_NOTIFY, "RGB color for AIRKILL message. Format: R,G,B");
ConVar sv_damage_display_color_bouncekill("sv_damage_display_color_bouncekill", "255,80,0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "RGB color for BOUNCE KILL message. Format: R,G,B");

// ConVars for Damage Display Special Kills
ConVar sv_damage_display_airkill_enable("sv_damage_display_airkill_enable", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable AIRKILL message on the damage display HUD.");
ConVar sv_damage_display_airkill_velocity_threshold("sv_damage_display_airkill_velocity_threshold", "500", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Vertical velocity threshold for damage display airkill detection (units/sec).");
ConVar sv_damage_display_airkill_height_threshold("sv_damage_display_airkill_height_threshold", "5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Height above ground threshold for damage display airkill detection (units).");



//Killer Info ConVars
ConVar sv_killerinfo_enable("sv_killerinfo_enable", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable killer info display system");
ConVar sv_killerinfo_show_weapon("sv_killerinfo_show_weapon", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show weapon used to kill");
ConVar sv_killerinfo_show_health("sv_killerinfo_show_health", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show killer's remaining health");
ConVar sv_killerinfo_show_health_negative_values("sv_killerinfo_show_health_negative_values", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show negative health values (overhealed players) for killer's remaining health. 0=disabled, 1=enabled, 2=show as 'DEAD'");
ConVar sv_killerinfo_show_armor("sv_killerinfo_show_armor", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show killer's remaining armor");
ConVar sv_killerinfo_show_distance("sv_killerinfo_show_distance", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Show distance to killer");
ConVar sv_killerinfo_distance_unit("sv_killerinfo_distance_unit", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Distance unit: 0=meters, 1=feet");
ConVar sv_killerinfo_airkill_velocity_threshold("sv_killerinfo_airkill_velocity_threshold", "500", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Vertical velocity threshold for airkill detection (units/sec)");
ConVar sv_killerinfo_airkill_height_threshold("sv_killerinfo_airkill_height_threshold", "5", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Height above ground threshold for airkill detection (units)");
//ConVar sv_killerinfo_airkill_trace_fraction("sv_killerinfo_airkill_trace_fraction", "0.9", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Trace fraction threshold for airkill detection (0.0-1.0)");
//ConVar sv_killerinfo_airkill_ground_override("sv_killerinfo_airkill_ground_override", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Override airkill if player is touching ground (0=disabled, 1=enabled)");
ConVar sv_killerinfo_airkill_enable("sv_killerinfo_airkill_enable", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable airkill detection entirely (0=disabled, 1=enabled) (has some bugs yet, sometimes it can show airkill even when the victim is on the ground, use at your own risk)");
ConVar sv_killerinfo_headshot_enable("sv_killerinfo_headshot_enable", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable headshot detection display (0=disabled, 1=enabled)");
ConVar sv_killerinfo_bouncekill_enable("sv_killerinfo_bouncekill_enable", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Enable bounce kill detection display (0=disabled, 1=enabled)");
ConVar sv_killerinfo_bounce_counter("sv_killerinfo_bounce_counter", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Display the bounce count on ricochet kills (0=disabled, 1=enabled)");
ConVar sv_killerinfo_bounce_counter_min("sv_killerinfo_bounce_counter_min", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Minimum number of bounces to show the bounce count");


extern ConVar mp_chattime;
extern ConVar sv_rtv_mintime;
extern ConVar sv_rtv_mintime;
extern ConVar sv_rtv_enabled;
extern ConVar sv_rtv_enabled;
extern void StartMapVote();

extern CBaseEntity* g_pLastCombineSpawn;
extern CBaseEntity* g_pLastRebelSpawn;

#define WEAPON_MAX_DISTANCE_FROM_SPAWN 64

#endif


void ParseRGBColor(const char* colorString, int& r, int& g, int& b)
{
	r = g = b = 255; // valores padrão

	if (!colorString || !*colorString)
		return;

	char tempString[64];
	Q_strncpy(tempString, colorString, sizeof(tempString));

	char* token = strtok(tempString, ",");
	if (token) {
		r = clamp(atoi(token), 0, 255);

		token = strtok(NULL, ",");
		if (token) {
			g = clamp(atoi(token), 0, 255);

			token = strtok(NULL, ",");
			if (token) {
				b = clamp(atoi(token), 0, 255);
			}
		}
	}
}

REGISTER_GAMERULES_CLASS(CHL2MPRules);

BEGIN_NETWORK_TABLE_NOBASE(CHL2MPRules, DT_HL2MPRules)

#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bTeamPlayEnabled)),
#else
SendPropBool(SENDINFO(m_bTeamPlayEnabled)),
#endif

END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS(hl2mp_gamerules, CHL2MPGameRulesProxy);
IMPLEMENT_NETWORKCLASS_ALIASED(HL2MPGameRulesProxy, DT_HL2MPGameRulesProxy)

static HL2MPViewVectors g_HL2MPViewVectors(
	Vector(0, 0, 64),       //VEC_VIEW (m_vView) 

	Vector(-16, -16, 0),	  //VEC_HULL_MIN (m_vHullMin)
	Vector(16, 16, 72),	  //VEC_HULL_MAX (m_vHullMax)

	Vector(-16, -16, 0),	  //VEC_DUCK_HULL_MIN (m_vDuckHullMin)
	Vector(16, 16, 36),	  //VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector(0, 0, 28),		  //VEC_DUCK_VIEW		(m_vDuckView)

	Vector(-10, -10, -10),	  //VEC_OBS_HULL_MIN	(m_vObsHullMin)
	Vector(10, 10, 10),	  //VEC_OBS_HULL_MAX	(m_vObsHullMax)

	Vector(0, 0, 14),		  //VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight)

	Vector(-16, -16, 0),	  //VEC_CROUCH_TRACE_MIN (m_vCrouchTraceMin)
	Vector(16, 16, 60)	  //VEC_CROUCH_TRACE_MAX (m_vCrouchTraceMax)
);

static const char* s_PreserveEnts[] =
{
	"ai_network",
	"ai_hint",
	"hl2mp_gamerules",
	"team_manager",
	"player_manager",
	"env_soundscape",
	"env_soundscape_proxy",
	"env_soundscape_triggerable",
	"env_sun",
	"env_wind",
	"env_fog_controller",
	"env_tonemap_controller",
	"func_brush",
	"func_wall",
	"func_buyzone",
	"func_illusionary",
	"infodecal",
	"info_projecteddecal",
	"info_node",
	"info_target",
	"info_node_hint",
	"info_player_deathmatch",
	"info_player_combine",
	"info_player_rebel",
	"info_map_parameters",
	"keyframe_rope",
	"move_rope",
	"info_ladder",
	"player",
	"point_viewcontrol",
	"scene_manager",
	"shadow_control",
	"sky_camera",
	"soundent",
	"trigger_soundscape",
	"viewmodel",
	"predicted_viewmodel",
	"worldspawn",
	"point_devshot_camera",
	"", // END Marker
};
bool bAdminMapChange = false;
extern int g_voters;
extern int g_votes;
extern int g_votesneeded;
extern bool g_votebegun;
extern bool g_votehasended;
extern int g_votetime;
extern int g_timetortv;
extern bool g_rtvbooted;
extern CUtlDict<int, unsigned short> g_mapVotes;
extern CUtlVector<CBasePlayer*> g_playersWhoVoted;
extern CUtlVector<CUtlString> g_currentVoteMaps;
extern CUtlDict<CUtlString, unsigned short> g_nominatedMaps;

#ifdef CLIENT_DLL
void RecvProxy_HL2MPRules(const RecvProp* pProp, void** pOut, void* pData, int objectID)
{
	CHL2MPRules* pRules = HL2MPRules();
	Assert(pRules);
	*pOut = pRules;
}

BEGIN_RECV_TABLE(CHL2MPGameRulesProxy, DT_HL2MPGameRulesProxy)
RecvPropDataTable("hl2mp_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE(DT_HL2MPRules), RecvProxy_HL2MPRules)
END_RECV_TABLE()
#else
void* SendProxy_HL2MPRules(const SendProp* pProp, const void* pStructBase, const void* pData, CSendProxyRecipients* pRecipients, int objectID)
{
	CHL2MPRules* pRules = HL2MPRules();
	Assert(pRules);
	return pRules;
}

BEGIN_SEND_TABLE(CHL2MPGameRulesProxy, DT_HL2MPGameRulesProxy)
SendPropDataTable("hl2mp_gamerules_data", 0, &REFERENCE_SEND_TABLE(DT_HL2MPRules), SendProxy_HL2MPRules)
END_SEND_TABLE()
#endif

#ifndef CLIENT_DLL

class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer(CBasePlayer* pListener, CBasePlayer* pTalker, bool& bProximity)
	{
		return (pListener->GetTeamNumber() == pTalker->GetTeamNumber());
	}
};
CVoiceGameMgrHelper g_VoiceGameMgrHelper;
IVoiceGameMgrHelper* g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;

#endif

// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
char* sTeamNames[] =
{
	"Unassigned",
	"Spectator",
	"Combine",
	"Rebels",
};

#ifndef CLIENT_DLL
void sv_equalizer_changed(IConVar* pConVar, const char* pOldString, float flOldValue)
{
	if (!((ConVar*)pConVar)->GetBool())  // If equalizer is disabled
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer)
			{
				// Reset player's render color and effects
				pPlayer->SetRenderColor(255, 255, 255);   // Reset color to white
				pPlayer->SetRenderMode(kRenderNormal);    // Reset to normal render mode

				// Access render properties through networked variables
				pPlayer->m_nRenderFX = kRenderFxNone;     // Reset render FX
				pPlayer->m_nRenderMode = kRenderNormal;   // Reset render mode
			}
		}
	}
}

ConVar sv_equalizer("sv_equalizer", "0", 0, "If non-zero, increase player visibility with bright colors", sv_equalizer_changed);
#endif

CHL2MPRules::CHL2MPRules()
{
#ifndef CLIENT_DLL
	// Create the team managers
	for (int i = 0; i < ARRAYSIZE(sTeamNames); i++)
	{
		CTeam* pTeam = static_cast<CTeam*>(CreateEntityByName("team_manager"));
		pTeam->Init(sTeamNames[i], i);

		g_Teams.AddToTail(pTeam);
	}

	m_bTeamPlayEnabled = teamplay.GetBool();
	m_flIntermissionEndTime = 0.0f;
	m_flGameStartTime = 0;

	m_hRespawnableItemsAndWeapons.RemoveAll();
	m_tmNextPeriodicThink = 0;
	m_flRestartGameTime = 0;
	m_bCompleteReset = false;
	m_bHeardAllPlayersReady = false;
	m_bAwaitingReadyRestart = false;
	m_bChangelevelDone = false;

	bAdminMapChange = false;
	bMapChangeOnGoing = false;
	bMapChange = false;
	m_flMapChangeTime = 0.0f;
	Q_strncpy(m_scheduledMapName, "", sizeof(m_scheduledMapName));

	g_voters = 0;
	g_votes = 0;
	g_votesneeded = 0;
	g_votetime = 0;
	g_timetortv = 0;
	g_votebegun = false;
	g_votehasended = false;
	g_rtvbooted = false;
	g_mapVotes.RemoveAll();
	g_playersWhoVoted.RemoveAll();
	g_nominatedMaps.RemoveAll();
#endif
}

const CViewVectors* CHL2MPRules::GetViewVectors()const
{
	return &g_HL2MPViewVectors;
}

const HL2MPViewVectors* CHL2MPRules::GetHL2MPViewVectors()const
{
	return &g_HL2MPViewVectors;
}

CHL2MPRules::~CHL2MPRules(void)
{
#ifndef CLIENT_DLL
	// Note, don't delete each team since they are in the gEntList and will 
	// automatically be deleted from there, instead.
	g_Teams.Purge();
#endif
}

void CHL2MPRules::CreateStandardEntities(void)
{

#ifndef CLIENT_DLL
	// Create the entity that will send our data to the client.

	BaseClass::CreateStandardEntities();

	g_pLastCombineSpawn = NULL;
	g_pLastRebelSpawn = NULL;

#ifdef DBGFLAG_ASSERT
	CBaseEntity* pEnt =
#endif
		CBaseEntity::Create("hl2mp_gamerules", vec3_origin, vec3_angle);
	Assert(pEnt);
#endif
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHL2MPRules::FlWeaponRespawnTime(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	if (weaponstay.GetInt() > 0)
	{
		// make sure it's only certain weapons
		if (!(pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD))
		{
			return 0;		// weapon respawns almost instantly
		}
	}

	return sv_hl2mp_weapon_respawn_time.GetFloat();
#endif

	return 0;		// weapon respawns almost instantly
}


bool CHL2MPRules::IsIntermission(void)
{
#ifndef CLIENT_DLL
	return m_flIntermissionEndTime > gpGlobals->curtime;
#endif

	return false;
}

void CHL2MPRules::PlayerKilled(CBasePlayer* pVictim, const CTakeDamageInfo& info)
{
	if (IsIntermission())
		return;

	// Show kill message
	CBasePlayer* pAttacker = ToBasePlayer(info.GetAttacker());
	if (pAttacker && pAttacker != pVictim)
	{
		// Safely get the victim as an HL2MP_Player to access m_iLastHitGroup
		CHL2MP_Player* pVictimPlayer = ToHL2MPPlayer(pVictim);
		int lastHitGroup = pVictimPlayer ? pVictimPlayer->m_iLastHitGroup : 0;

		bool wasAirKill = WasAirKill(pVictim, info);
		bool wasBounceKill = WasBounceKill(info);
		int bounceCount = GetBounceCount(info);

		// *** THIS IS THE FIX ***
		// We now pass 'lastHitGroup' instead of 'info.GetDamageType()'
		ShowDamageDisplay(pAttacker, pVictim, (int)info.GetDamage(), true, 0, lastHitGroup, wasAirKill, wasBounceKill, bounceCount);
	}

	BaseClass::PlayerKilled(pVictim, info);
}

#ifdef GAME_DLL
CBaseEntity* FindEntityByName(const char* name)
{
	return gEntList.FindEntityByName(NULL, name);
}
#endif

void CHL2MPRules::Think(void)
{
#ifndef CLIENT_DLL

	CGameRules::Think();

	/*
		EQUALIZER
	*/
	if (sv_equalizer.GetBool())
	{
		CBaseEntity* pLightingTarget = FindEntityByName("sf_equalizer_hax");

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer && pPlayer->GetTeamNumber() != TEAM_SPECTATOR)
			{
				const char* forcedModel = nullptr;

				// Determine model based on team and mode
				if (IsTeamplay())
				{
					if (pPlayer->GetTeamNumber() == TEAM_COMBINE)
					{
						forcedModel = sv_equalizer_model_combine.GetString();
					}
					else if (pPlayer->GetTeamNumber() == TEAM_REBELS)
					{
						forcedModel = sv_equalizer_model_rebel.GetString();
					}
					else // TEAM_UNASSIGNED in teamplay
					{
						forcedModel = sv_equalizer_model.GetString();
					}
				}
				else
				{
					// Deathmatch mode - everyone uses the same model
					forcedModel = sv_equalizer_model.GetString();
				}

				const char* currentModel = modelinfo->GetModelName(pPlayer->GetModel());
				BaseClass::Precache();
				CBaseEntity::PrecacheModel(forcedModel);
				CBaseEntity::PrecacheScriptSound("NPC_CombineS.Die");

				if (Q_stricmp(currentModel, forcedModel) != 0)
				{
					pPlayer->SetModel(forcedModel);
				}

				// Set render color based on team
				int r, g, b;
				if (IsTeamplay())
				{
					if (pPlayer->GetTeamNumber() == TEAM_COMBINE)
					{
						ParseRGBColor(sv_equalizer_combine_color.GetString(), r, g, b);
					}
					else if (pPlayer->GetTeamNumber() == TEAM_REBELS)
					{
						ParseRGBColor(sv_equalizer_rebel_color.GetString(), r, g, b);
					}
					else if (pPlayer->GetTeamNumber() == TEAM_UNASSIGNED)
					{
						r = 0; g = 255; b = 0; // Green for unassigned
					}
				}
				else
				{
					// Teamplay disabled, use single color
					ParseRGBColor(sv_equalizer_color.GetString(), r, g, b);
				}

				pPlayer->SetRenderColor(r, g, b);
				pPlayer->SetRenderMode(kRenderTransAdd);
				pPlayer->m_nRenderFX = kRenderFxGlowShell;
				pPlayer->SetRenderColorA(255);

				if (pLightingTarget)
				{
					pPlayer->SetLightingOrigin(pLightingTarget);
				}
			}
		}
	}

	HandleNewTargetID();
	HandleTimeleft();
	HandlePlayerNetworkCheck();
	HandleMapVotes();

	if (sv_rtv_enabled.GetBool() && (mp_timelimit.GetFloat() > 0) && GetMapRemainingTime() <= 20 && !g_votebegun && !g_votehasended && !bAdminMapChange)
	{
		g_votebegun = true;

		UTIL_PrintToAllClients(CHAT_INFO "Voting for next map has started...\n");
		StartMapVote();
	}

	if (g_fGameOver)   // someone else quit the game already
	{
		// check to see if we should change levels now
		if (m_flIntermissionEndTime < gpGlobals->curtime)
		{
			if (!m_bChangelevelDone)
			{
				ChangeLevel(); // intermission is over
				m_bChangelevelDone = true;
			}
		}

		return;
	}

	//	float flTimeLimit = mp_timelimit.GetFloat() * 60;
	float flFragLimit = fraglimit.GetFloat();

	if (GetMapRemainingTime() < 0)
	{
		GoToIntermission();
		return;
	}

	if (flFragLimit)
	{
		if (IsTeamplay() == true)
		{
			CTeam* pCombine = g_Teams[TEAM_COMBINE];
			CTeam* pRebels = g_Teams[TEAM_REBELS];

			if (pCombine->GetScore() >= flFragLimit || pRebels->GetScore() >= flFragLimit)
			{
				GoToIntermission();
				return;
			}
		}
		else
		{
			// check if any player is over the frag limit
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

				if (pPlayer && pPlayer->FragCount() >= flFragLimit)
				{
					GoToIntermission();
					return;
				}
			}
		}
	}

	if (gpGlobals->curtime > m_tmNextPeriodicThink)
	{
		CheckAllPlayersReady();
		CheckRestartGame();
		m_tmNextPeriodicThink = gpGlobals->curtime + 1.0;
	}

	if (m_flRestartGameTime > 0.0f && m_flRestartGameTime <= gpGlobals->curtime)
	{
		RestartGame();
	}

	if (m_bAwaitingReadyRestart && m_bHeardAllPlayersReady)
	{
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "All players ready. Game will restart in 5 seconds");
		UTIL_ClientPrintAll(HUD_PRINTCONSOLE, "All players ready. Game will restart in 5 seconds");

		m_flRestartGameTime = gpGlobals->curtime + 5;
		m_bAwaitingReadyRestart = false;
	}

	ManageObjectRelocation();
	RemoveAllPlayersEquipment();

#endif
}

#ifndef CLIENT_DLL
void CHL2MPRules::HandleMapVotes()
{
	if (!g_rtvbooted)
	{
		g_timetortv = gpGlobals->curtime + sv_rtv_mintime.GetInt();
		g_rtvbooted = true;
	}

	if (g_votebegun && !g_votehasended && (gpGlobals->curtime >= g_votetime))
	{
		CUtlString winningMap;
		int totalVotes = g_playersWhoVoted.Count();

		if (totalVotes == 0)
		{
			if (g_currentVoteMaps.Count() > 0)
			{
				winningMap = g_currentVoteMaps[RandomInt(0, g_currentVoteMaps.Count() - 1)];
				UTIL_PrintToAllClients(UTIL_VarArgs(CHAT_ADMIN "No votes were cast. Randomly selecting map: " CHAT_INFO "%s\n", winningMap.Get()));
			}
			else
			{
				UTIL_PrintToAllClients(CHAT_RED "No maps available for random selection.\n");
				return;
			}
		}
		else
		{
			CUtlVector<CUtlString> tiedMaps;
			int highestVotes = 0;

			for (unsigned int i = 0; i < g_mapVotes.Count(); i++)
			{
				int votes = g_mapVotes[i];
				const char* mapName = g_mapVotes.GetElementName(i);

				if (votes > highestVotes)
				{
					highestVotes = votes;
					winningMap = mapName;
					tiedMaps.RemoveAll();
					tiedMaps.AddToTail(mapName);
				}
				else if (votes == highestVotes)
				{
					tiedMaps.AddToTail(mapName);
				}
			}

			if (tiedMaps.Count() > 1)
			{
				winningMap = tiedMaps[RandomInt(0, tiedMaps.Count() - 1)];
			}

			float winningPercentage = (static_cast<float>(highestVotes) / totalVotes) * 100.0f;

			UTIL_PrintToAllClients(UTIL_VarArgs(
				CHAT_ADMIN "Players have spoken! Winning map is " CHAT_INFO "%s " CHAT_DEFAULT "(%.2f%% of %d vote%s).\n",
				winningMap.Get(), winningPercentage, totalVotes, totalVotes < 2 ? "" : "s"));

			if (mp_chattime.GetInt() <= 5)
			{
				mp_chattime.SetValue(10);
			}

			GoToIntermission();
		}

		engine->ServerCommand(CFmtStr("sa map %s\n", winningMap.Get()));

		g_votebegun = false;
		g_votehasended = true;
		g_mapVotes.RemoveAll();
		g_playersWhoVoted.RemoveAll();
		g_currentVoteMaps.RemoveAll();
	}

	if (IsMapChangeOnGoing() && IsMapChange())
	{
		SetMapChange(false);
		m_flMapChangeTime = gpGlobals->curtime + 5.0f;
	}

	if (IsMapChangeOnGoing() && gpGlobals->curtime > m_flMapChangeTime)
	{
		SetMapChange(false);
		SetMapChangeOnGoing(false);

		if (Q_strlen(m_scheduledMapName) > 0)
		{
			engine->ServerCommand(UTIL_VarArgs("changelevel %s\n", m_scheduledMapName));
		}
	}
}

bool IsValidPositiveInteger(const char* str)
{
	// Check if the string is not empty and doesn't start with '+' or '-'
	if (str == nullptr || *str == '\0' || *str == '+' || *str == '-')
		return false;

	// Ensure all characters are digits
	for (const char* p = str; *p; p++)
	{
		if (!isdigit(*p))
			return false;
	}

	return true;
}

void CHL2MPRules::HandlePlayerNetworkCheck()
{
	// We don't want people using 0
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (gpGlobals->curtime > m_tmNextPeriodicThink)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer && !pPlayer->IsBot() && !pPlayer->IsHLTV())
			{
				// Fetch client-side settings from the player
				const char* cl_updaterate = engine->GetClientConVarValue(pPlayer->entindex(), "cl_updaterate");
				const char* cl_cmdrate = engine->GetClientConVarValue(pPlayer->entindex(), "cl_cmdrate");

				bool shouldKick = false;
				char kickReason[128] = "";

				// Validate and convert cl_updaterate
				if (!IsValidPositiveInteger(cl_updaterate))
				{
					shouldKick = true;
					Q_snprintf(kickReason, sizeof(kickReason), "cl_updaterate is invalid (value: %s)", cl_updaterate);
				}
				else
				{
					int updaterate = atoi(cl_updaterate);
					if (updaterate <= 0)
					{
						shouldKick = true;
						Q_snprintf(kickReason, sizeof(kickReason), "cl_updaterate is invalid (value: %d)", updaterate);
					}
				}

				// Validate and convert cl_cmdrate
				if (!IsValidPositiveInteger(cl_cmdrate))
				{
					shouldKick = true;
					Q_snprintf(kickReason, sizeof(kickReason), "cl_cmdrate is invalid (value: %s)", cl_cmdrate);
				}
				else
				{
					int cmdrate = atoi(cl_cmdrate);
					if (cmdrate <= 0)
					{
						shouldKick = true;
						Q_snprintf(kickReason, sizeof(kickReason), "cl_cmdrate is invalid (value: %d)", cmdrate);
					}
				}

				if (shouldKick)
				{
					// Get the player's user ID instead of the entity index
					int userID = pPlayer->GetUserID();  // This will provide the correct user ID for kicking

					engine->ServerCommand(UTIL_VarArgs("kickid %d %s\n", userID, kickReason));  // Use userID instead of entindex()
					return;
				}
			}
		}
	}
}

void CHL2MPRules::HandleTimeleft()
{
	if (GetMapRemainingTime() <= 0 || !sv_timeleft_enable.GetBool())
		return;

	if (gpGlobals->curtime <= m_tmNextPeriodicThink)
		return;

	hudtextparms_t textParams = CreateTextParams();

	int iTimeRemaining = (int)GetMapRemainingTime();
	char stime[64];
	FormatTimeRemaining(iTimeRemaining, stime, sizeof(stime));

	if (sv_timeleft_teamscore.GetBool())
	{
		if (IsTeamplay())
		{
			UpdateTeamScoreColors(textParams);
		}
		else
		{
			DisplayUnassignedTeamStats(textParams, stime);
			DisplaySpectatorStats(textParams, stime);
			return;
		}
	}

	SendHudMessagesToPlayers(textParams, stime);
}

hudtextparms_t CHL2MPRules::CreateTextParams()
{
	hudtextparms_t textParams;
	textParams.channel = sv_timeleft_channel.GetInt();

	if (!IsTeamplay())
	{
		int r, g, b;
		ParseRGBColor(sv_timeleft_color.GetString(), r, g, b);
		textParams.r1 = r;
		textParams.g1 = g;
		textParams.b1 = b;
	}

	textParams.a1 = 255;
	textParams.x = sv_timeleft_x.GetFloat();
	textParams.y = sv_timeleft_y.GetFloat();
	textParams.effect = 0;
	textParams.fadeinTime = 0;
	textParams.fadeoutTime = 0;
	textParams.holdTime = 1.10;
	textParams.fxTime = 0;
	return textParams;
}

void CHL2MPRules::FormatTimeRemaining(int iTimeRemaining, char* buffer, size_t bufferSize)
{
	int iMinutes = (iTimeRemaining / 60) % 60;
	int iSeconds = iTimeRemaining % 60;
	int iHours = (iTimeRemaining / 3600) % 24;
	int iDays = (iTimeRemaining / 86400);

	if (IsTeamplay())
	{
		CTeam* pCombine = g_Teams[TEAM_COMBINE];
		CTeam* pRebels = g_Teams[TEAM_REBELS];

		int combineScore = pCombine ? pCombine->GetScore() : 0;
		int rebelsScore = pRebels ? pRebels->GetScore() : 0;

		if (iTimeRemaining >= 86400)
		{
			Q_snprintf(buffer, bufferSize, "%d %2.2d:%2.2d:%2.2d:%2.2d %d",
				combineScore, iDays, iHours, iMinutes, iSeconds, rebelsScore);
		}
		else if (iTimeRemaining >= 3600)
		{
			Q_snprintf(buffer, bufferSize, "%d %2.2d:%2.2d:%2.2d %d",
				combineScore, iHours, iMinutes, iSeconds, rebelsScore);
		}
		else
		{
			Q_snprintf(buffer, bufferSize, "%d %d:%2.2d %d",
				combineScore, iMinutes, iSeconds, rebelsScore);
		}
	}
	else
	{
		FormatStandardTime(iTimeRemaining, buffer, bufferSize);
	}
}

void CHL2MPRules::FormatStandardTime(int iTimeRemaining, char* buffer, size_t bufferSize)
{
	int iMinutes = (iTimeRemaining / 60) % 60;
	int iSeconds = iTimeRemaining % 60;
	int iHours = (iTimeRemaining / 3600) % 24;
	int iDays = (iTimeRemaining / 86400);

	if (iTimeRemaining >= 86400)
		Q_snprintf(buffer, bufferSize, "%2.2d:%2.2d:%2.2d:%2.2d", iDays, iHours, iMinutes, iSeconds);
	else if (iTimeRemaining >= 3600)
		Q_snprintf(buffer, bufferSize, "%2.2d:%2.2d:%2.2d", iHours, iMinutes, iSeconds);
	else
		Q_snprintf(buffer, bufferSize, "%d:%2.2d", iMinutes, iSeconds);
}

void CHL2MPRules::UpdateTeamScoreColors(hudtextparms_t& textParams)
{
	CTeam* pCombine = g_Teams[TEAM_COMBINE];
	CTeam* pRebels = g_Teams[TEAM_REBELS];

	if (pCombine->GetScore() > pRebels->GetScore())
	{
		textParams.r1 = 159;
		textParams.g1 = 202;
		textParams.b1 = 242;
	}
	else if (pRebels->GetScore() > pCombine->GetScore())
	{
		textParams.r1 = 255;
		textParams.g1 = 50;
		textParams.b1 = 50;
	}
	else
	{
		textParams.r1 = 255;
		textParams.g1 = 255;
		textParams.b1 = 255;
	}
}

void CHL2MPRules::DisplayUnassignedTeamStats(hudtextparms_t& textParams, const char* stime)
{
	CTeam* pTeamUnassigned = g_Teams[TEAM_UNASSIGNED];
	if (!pTeamUnassigned)
		return;

	CUtlVector<CBaseMultiplayerPlayer*> unassignedPlayers;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
		CBaseMultiplayerPlayer* pMultiplayerPlayer = ToBaseMultiplayerPlayer(pPlayer);

		if (pMultiplayerPlayer && pMultiplayerPlayer->GetTeamNumber() == TEAM_UNASSIGNED && !pMultiplayerPlayer->IsObserver())
		{
			unassignedPlayers.AddToTail(pMultiplayerPlayer);
		}
	}

	unassignedPlayers.Sort([](CBaseMultiplayerPlayer* const* a, CBaseMultiplayerPlayer* const* b) {
		return (*b)->FragCount() - (*a)->FragCount();
		});

	for (int i = 0; i < unassignedPlayers.Count(); i++)
	{
		CBaseMultiplayerPlayer* pCurrentPlayer = unassignedPlayers[i];
		int playerRank = i + 1;

		char playerStatText[128];

		if (pCurrentPlayer->FragCount() >= 2 || pCurrentPlayer->FragCount() <= -2)
			Q_snprintf(playerStatText, sizeof(playerStatText), "%s\n%d/%d | %d Frags", stime, playerRank, unassignedPlayers.Count(), pCurrentPlayer->FragCount());
		else
			Q_snprintf(playerStatText, sizeof(playerStatText), "%s\n%d/%d | %d Frag", stime, playerRank, unassignedPlayers.Count(), pCurrentPlayer->FragCount());


		UTIL_HudMessage(pCurrentPlayer, textParams, playerStatText);
	}
}

void CHL2MPRules::DisplaySpectatorStats(hudtextparms_t& textParams, const char* stime)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() && pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
		{
			UTIL_HudMessage(pPlayer, textParams, stime);
		}
	}
}

void CHL2MPRules::SendHudMessagesToPlayers(hudtextparms_t& textParams, const char* stime)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot())
		{
			UTIL_HudMessage(pPlayer, textParams, stime);
		}
	}
}

void CHL2MPRules::HandleNewTargetID()
{
	if (!sv_hudtargetid.GetBool())
		return;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));
		if (pPlayer && pPlayer->IsAlive())
		{
			Vector vecDir;
			AngleVectors(pPlayer->EyeAngles(), &vecDir);
			Vector vecAbsStart = pPlayer->EyePosition();
			Vector vecAbsEnd = vecAbsStart + (vecDir * 2048);
			trace_t tr;
			UTIL_TraceLine(vecAbsStart, vecAbsEnd, MASK_ALL, pPlayer, COLLISION_GROUP_NONE, &tr);
			CBasePlayer* pPlayerEntity = dynamic_cast<CBasePlayer*>(tr.m_pEnt);

			if (pPlayerEntity && pPlayerEntity->IsPlayer() && pPlayerEntity->IsAlive())
			{
				// Se estilo for 0 (original), usa o sistema simples do HL2DM
				if (sv_targetid_style.GetInt() == 0)
				{
					// Sistema original - nome simples na parte inferior
					hudtextparms_s tTextParam;
					tTextParam.x = -1;  // Centralizado horizontalmente
					tTextParam.y = 0.73; // Posição inferior como no HL2DM original
					tTextParam.effect = 0;
					tTextParam.r1 = 255;
					tTextParam.g1 = 255;
					tTextParam.b1 = 255;
					tTextParam.a1 = 255;
					tTextParam.r2 = 255;
					tTextParam.g2 = 255;
					tTextParam.b2 = 255;
					tTextParam.a2 = 255;
					tTextParam.fadeinTime = 0.0;
					tTextParam.fadeoutTime = 0.0;
					tTextParam.holdTime = 0.1;
					tTextParam.fxTime = 0;
					tTextParam.channel = 1; // Canal fixo para o sistema original

					// Apenas o nome do jogador
					char playerName[256];
					Q_snprintf(playerName, sizeof(playerName), "%s", pPlayerEntity->GetPlayerName());

					UTIL_HudMessage(pPlayer, tTextParam, playerName);
				}
				// Se estilo for 1 (customizado), usa o sistema avançado
				// Se estilo for 1 (customizado), usa o sistema avançado
				else if (sv_targetid_style.GetInt() == 1)
				{
					static CBasePlayer* pLastTarget[MAX_PLAYERS] = { nullptr };
					bool isNewTarget = (pPlayerEntity != pLastTarget[i]);
					char entity[256];

					if (GameRules()->IsTeamplay())
					{
						if (pPlayerEntity->GetTeamNumber() == pPlayer->GetTeamNumber())
						{
							if (pPlayerEntity->ArmorValue())
								Q_snprintf(entity, sizeof(entity), "%s\nHP: %.0i\nAP: %.0i\n", pPlayerEntity->GetPlayerName(), pPlayerEntity->GetHealth(), pPlayerEntity->ArmorValue());
							else
								Q_snprintf(entity, sizeof(entity), "%s\nHP: %.0i\n", pPlayerEntity->GetPlayerName(), pPlayerEntity->GetHealth());
						}
						else
						{
							Q_snprintf(entity, sizeof(entity), "%s", pPlayerEntity->GetPlayerName());
						}
					}
					else
					{
						Q_snprintf(entity, sizeof(entity), "%s", pPlayerEntity->GetPlayerName());
					}

					// HUD message setup para sistema customizado
					hudtextparms_s tTextParam;
					tTextParam.x = -1; // CORRIGIDO: Usa -1 para centralizar horizontalmente
					tTextParam.y = 0.63;
					tTextParam.effect = 0;
					tTextParam.r1 = 255;
					tTextParam.g1 = 128;
					tTextParam.b1 = 0;
					tTextParam.a1 = 255;
					tTextParam.r2 = 255;
					tTextParam.g2 = 128;
					tTextParam.b2 = 0;
					tTextParam.a2 = 255;
					tTextParam.fadeinTime = 0.008;
					tTextParam.fadeoutTime = 0.008;
					tTextParam.holdTime = 0.5;
					tTextParam.fxTime = 0;
					tTextParam.channel = sv_hudtargetid_channel.GetInt();

					if (isNewTarget || gpGlobals->curtime > pPlayer->GetNextHudUpdate())
					{
						UTIL_HudMessage(pPlayer, tTextParam, entity);
						pPlayer->SetNextHudUpdate(gpGlobals->curtime + 0.5f);
						pLastTarget[i] = pPlayerEntity;
					}
				}
			}
			else
			{
				// Limpa qualquer texto quando não está mirando em ninguém
				if (sv_targetid_style.GetInt() == 0)
				{
					hudtextparms_s tTextParam;
					tTextParam.x = -1;
					tTextParam.y = 0.73;
					tTextParam.effect = 0;
					tTextParam.r1 = 255;
					tTextParam.g1 = 255;
					tTextParam.b1 = 255;
					tTextParam.a1 = 255;
					tTextParam.r2 = 255;
					tTextParam.g2 = 255;
					tTextParam.b2 = 255;
					tTextParam.a2 = 255;
					tTextParam.fadeinTime = 0.0;
					tTextParam.fadeoutTime = 0.0;
					tTextParam.holdTime = 0.1;
					tTextParam.fxTime = 0;
					tTextParam.channel = 1;

					UTIL_HudMessage(pPlayer, tTextParam, "");
				}
			}
		}
	}
}


void CHL2MPRules::RemoveAllPlayersEquipment()
{
	// Forcefully remove suit and weapons here to account for mp_restartgame
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

		if (pPlayer && pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
		{
			pPlayer->RemoveAllItems(true);
		}
	}

	// Fixes a bug where a specator could spectate another spectator
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

		if (!pPlayer || !pPlayer->IsConnected())
			continue;

		if (pPlayer->GetObserverMode() == OBS_MODE_CHASE || pPlayer->GetObserverMode() == OBS_MODE_IN_EYE)
		{
			CBasePlayer* pTarget = ToBasePlayer(pPlayer->GetObserverTarget());

			if (pTarget && pTarget->GetTeamNumber() == TEAM_SPECTATOR)
			{
				pPlayer->SetObserverMode(OBS_MODE_ROAMING);
				pPlayer->SetObserverTarget(NULL);
			}
		}
	}
}
#endif

void CHL2MPRules::GoToIntermission(void)
{
#ifndef CLIENT_DLL
	if (g_fGameOver)
		return;

	g_fGameOver = true;

	m_flIntermissionEndTime = gpGlobals->curtime + mp_chattime.GetInt();

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

		if (!pPlayer)
			continue;

		pPlayer->ShowViewPortPanel(PANEL_SCOREBOARD);
		pPlayer->AddFlag(FL_FROZEN);
	}
#endif

}

bool CHL2MPRules::CheckGameOver()
{
#ifndef CLIENT_DLL
	if (g_fGameOver)   // someone else quit the game already
	{
		// check to see if we should change levels now
		if (m_flIntermissionEndTime < gpGlobals->curtime)
		{
			ChangeLevel(); // intermission is over			
		}

		return true;
	}
#endif

	return false;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHL2MPRules::FlWeaponTryRespawn(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	if (pWeapon && (pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD))
	{
		if (gEntList.NumberOfEntities() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE))
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime(pWeapon);
	}
#endif
	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHL2MPRules::VecWeaponRespawnSpot(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	CWeaponHL2MPBase* pHL2Weapon = dynamic_cast<CWeaponHL2MPBase*>(pWeapon);

	if (pHL2Weapon)
	{
		return pHL2Weapon->GetOriginalSpawnOrigin();
	}
#endif

	return pWeapon->GetAbsOrigin();
}

QAngle CHL2MPRules::DefaultWeaponRespawnAngle(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	CWeaponHL2MPBase* pHL2Weapon = dynamic_cast<CWeaponHL2MPBase*>(pWeapon);

	if (pHL2Weapon)
	{
		return pHL2Weapon->GetOriginalSpawnAngles();
	}
#endif
	return pWeapon->GetAbsAngles();
}

#ifndef CLIENT_DLL

CItem* IsManagedObjectAnItem(CBaseEntity* pObject)
{
	return dynamic_cast<CItem*>(pObject);
}

CWeaponHL2MPBase* IsManagedObjectAWeapon(CBaseEntity* pObject)
{
	return dynamic_cast<CWeaponHL2MPBase*>(pObject);
}

bool GetObjectsOriginalParameters(CBaseEntity* pObject, Vector& vOriginalOrigin, QAngle& vOriginalAngles)
{
	if (CItem* pItem = IsManagedObjectAnItem(pObject))
	{
		if (pItem->m_flNextResetCheckTime > gpGlobals->curtime)
			return false;

		vOriginalOrigin = pItem->GetOriginalSpawnOrigin();
		vOriginalAngles = pItem->GetOriginalSpawnAngles();

		pItem->m_flNextResetCheckTime = gpGlobals->curtime + sv_hl2mp_item_respawn_time.GetFloat();
		return true;
	}
	else if (CWeaponHL2MPBase* pWeapon = IsManagedObjectAWeapon(pObject))
	{
		if (pWeapon->m_flNextResetCheckTime > gpGlobals->curtime)
			return false;

		vOriginalOrigin = pWeapon->GetOriginalSpawnOrigin();
		vOriginalAngles = pWeapon->GetOriginalSpawnAngles();

		pWeapon->m_flNextResetCheckTime = gpGlobals->curtime + sv_hl2mp_weapon_respawn_time.GetFloat();
		return true;
	}

	return false;
}

void CHL2MPRules::ManageObjectRelocation(void)
{
	int iTotal = m_hRespawnableItemsAndWeapons.Count();

	if (iTotal > 0)
	{
		for (int i = 0; i < iTotal; i++)
		{
			CBaseEntity* pObject = m_hRespawnableItemsAndWeapons[i].Get();

			if (pObject)
			{
				Vector vSpawOrigin;
				QAngle vSpawnAngles;

				if (GetObjectsOriginalParameters(pObject, vSpawOrigin, vSpawnAngles) == true)
				{
					float flDistanceFromSpawn = (pObject->GetAbsOrigin() - vSpawOrigin).Length();

					if (flDistanceFromSpawn > WEAPON_MAX_DISTANCE_FROM_SPAWN)
					{
						bool shouldReset = false;
						IPhysicsObject* pPhysics = pObject->VPhysicsGetObject();

						if (pPhysics)
						{
							shouldReset = pPhysics->IsAsleep();
						}
						else
						{
							shouldReset = (pObject->GetFlags() & FL_ONGROUND) ? true : false;
						}

						if (shouldReset)
						{
							pObject->Teleport(&vSpawOrigin, &vSpawnAngles, NULL);
							pObject->EmitSound("AlyxEmp.Charge");

							IPhysicsObject* pPhys = pObject->VPhysicsGetObject();

							if (pPhys)
							{
								pPhys->Wake();
							}
						}
					}
				}
			}
		}
	}
}

//=========================================================
//AddLevelDesignerPlacedWeapon
//=========================================================
void CHL2MPRules::AddLevelDesignerPlacedObject(CBaseEntity* pEntity)
{
	if (m_hRespawnableItemsAndWeapons.Find(pEntity) == -1)
	{
		m_hRespawnableItemsAndWeapons.AddToTail(pEntity);
	}
}

//=========================================================
//RemoveLevelDesignerPlacedWeapon
//=========================================================
void CHL2MPRules::RemoveLevelDesignerPlacedObject(CBaseEntity* pEntity)
{
	if (m_hRespawnableItemsAndWeapons.Find(pEntity) != -1)
	{
		m_hRespawnableItemsAndWeapons.FindAndRemove(pEntity);
	}
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHL2MPRules::VecItemRespawnSpot(CItem* pItem)
{
	return pItem->GetOriginalSpawnOrigin();
}

//=========================================================
// What angles should this item use to respawn?
//=========================================================
QAngle CHL2MPRules::VecItemRespawnAngles(CItem* pItem)
{
	return pItem->GetOriginalSpawnAngles();
}

//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHL2MPRules::FlItemRespawnTime(CItem* pItem)
{
	return sv_hl2mp_item_respawn_time.GetFloat();
}


//=========================================================
// CanHaveWeapon - returns false if the player is not allowed
// to pick up this weapon
//=========================================================
bool CHL2MPRules::CanHavePlayerItem(CBasePlayer* pPlayer, CBaseCombatWeapon* pItem)
{
	if (weaponstay.GetInt() > 0)
	{
		if (pPlayer->Weapon_OwnsThisType(pItem->GetClassname(), pItem->GetSubType()))
			return false;
	}

	return BaseClass::CanHavePlayerItem(pPlayer, pItem);
}

#endif

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHL2MPRules::WeaponShouldRespawn(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	if (pWeapon->HasSpawnFlags(SF_NORESPAWN))
	{
		return GR_WEAPON_RESPAWN_NO;
	}
#endif

	return GR_WEAPON_RESPAWN_YES;
}

//-----------------------------------------------------------------------------
// Purpose: Player has just left the game
//-----------------------------------------------------------------------------
void CHL2MPRules::ClientDisconnected(edict_t* pClient)
{
#ifndef CLIENT_DLL
	// Msg( "CLIENT DISCONNECTED, REMOVING FROM TEAM.\n" );

	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pClient);
	CSaveScores::SavePlayerScore(pPlayer);
	if (pPlayer)
	{
		FireTargets("game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0);
		// Remove the player from his team
		if (pPlayer->GetTeam())
		{
			pPlayer->GetTeam()->RemovePlayer(pPlayer);
		}
	}

	BaseClass::ClientDisconnected(pClient);

#endif
}


//=========================================================
// Deathnotice. 
//=========================================================
void CHL2MPRules::DeathNotice(CBasePlayer* pVictim, const CTakeDamageInfo& info)
{
#ifndef CLIENT_DLL
	// Work out what killed the player, and send a message to all clients about it
	const char* killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_ID = 0;

	// Find the killer & the scorer
	CBaseEntity* pInflictor = info.GetInflictor();
	CBaseEntity* pKiller = info.GetAttacker();
	CBasePlayer* pScorer = GetDeathScorer(pKiller, pInflictor);

	// Custom kill type?
	if (info.GetDamageCustom())
	{
		killer_weapon_name = GetDamageCustomString(info);
		if (pScorer)
		{
			killer_ID = pScorer->GetUserID();
		}
	}
	else
	{
		// Is the killer a client?
		if (pScorer)
		{
			killer_ID = pScorer->GetUserID();

			if (pInflictor)
			{
				if (pInflictor == pScorer)
				{
					// If the inflictor is the killer,  then it must be their current weapon doing the damage
					if (pScorer->GetActiveWeapon())
					{
						killer_weapon_name = pScorer->GetActiveWeapon()->GetClassname();
					}
				}
				else
				{
					killer_weapon_name = pInflictor->GetClassname();  // it's just that easy
				}
			}
		}
		else
		{
			killer_weapon_name = pInflictor->GetClassname();
		}

		// strip the NPC_* or weapon_* from the inflictor's classname
		if (strncmp(killer_weapon_name, "weapon_", 7) == 0)
		{
			killer_weapon_name += 7;
		}
		else if (strncmp(killer_weapon_name, "npc_", 4) == 0)
		{
			killer_weapon_name += 4;
		}
		else if (strncmp(killer_weapon_name, "func_", 5) == 0)
		{
			killer_weapon_name += 5;
		}
		else if (strstr(killer_weapon_name, "physics"))
		{
			killer_weapon_name = "physics";
		}
		if (strstr(killer_weapon_name, "physbox"))
		{
			killer_weapon_name = "physics";
		}
		if (strcmp(killer_weapon_name, "prop_combine_ball") == 0)
		{
			killer_weapon_name = "combine_ball";
		}
		else if (strcmp(killer_weapon_name, "grenade_ar2") == 0)
		{
			killer_weapon_name = "smg1_grenade";
		}
		else if (strcmp(killer_weapon_name, "satchel") == 0 || strcmp(killer_weapon_name, "tripmine") == 0)
		{
			killer_weapon_name = "slam";
		}
		// CÓDIGO DE PONTUAÇÃO REMOVIDO - agora está na PlayerKilled
	}

	IGameEvent* event = gameeventmanager->CreateEvent("player_death");
	if (event)
	{
		event->SetInt("userid", pVictim->GetUserID());
		event->SetInt("attacker", killer_ID);
		event->SetString("weapon", killer_weapon_name);
		event->SetInt("priority", 7);
		gameeventmanager->FireEvent(event);
	}
#endif
}

void CHL2MPRules::ClientSettingsChanged(CBasePlayer* pPlayer)
{
#ifndef CLIENT_DLL

	CHL2MP_Player* pHL2Player = ToHL2MPPlayer(pPlayer);

	if (pHL2Player == NULL)
		return;

	const char* pCurrentModel = modelinfo->GetModelName(pPlayer->GetModel());
	const char* szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(pPlayer->edict()), "cl_playermodel");

	//If we're different.
	if (stricmp(szModelName, pCurrentModel))
	{
		//Too soon, set the cvar back to what it was.
		//Note: this will make this function be called again
		//but since our models will match it'll just skip this whole dealio.
		if (pHL2Player->GetNextModelChangeTime() >= gpGlobals->curtime)
		{
			char szReturnString[512];

			Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", pCurrentModel);
			engine->ClientCommand(pHL2Player->edict(), szReturnString);

			Q_snprintf(szReturnString, sizeof(szReturnString), "Please wait %d more seconds before trying to switch models.\n", (int)(pHL2Player->GetNextModelChangeTime() - gpGlobals->curtime));
			ClientPrint(pHL2Player, HUD_PRINTTALK, szReturnString);
			return;
		}

		if (HL2MPRules()->IsTeamplay() == false)
		{
			pHL2Player->SetPlayerModel();

			const char* pszCurrentModelName = modelinfo->GetModelName(pHL2Player->GetModel());

			char szReturnString[128];
			Q_snprintf(szReturnString, sizeof(szReturnString), "Your player model is: %s\n", pszCurrentModelName);

			ClientPrint(pHL2Player, HUD_PRINTTALK, szReturnString);
		}
		else
		{
			if (!sv_equalizer.GetBool()) {
				if (Q_stristr(szModelName, "models/human"))
				{
					pHL2Player->ChangeTeam(TEAM_REBELS);
				}
				else
				{
					pHL2Player->ChangeTeam(TEAM_COMBINE);
				}
			}
		}
	}
	if (sv_report_client_settings.GetInt() == 1)
	{
		UTIL_LogPrintf("\"%s\" cl_cmdrate = \"%s\"\n", pHL2Player->GetPlayerName(), engine->GetClientConVarValue(pHL2Player->entindex(), "cl_cmdrate"));
	}

	BaseClass::ClientSettingsChanged(pPlayer);
#endif

}

int CHL2MPRules::PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget)
{
#ifndef CLIENT_DLL
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if (!pPlayer || !pTarget || !pTarget->IsPlayer() || IsTeamplay() == false)
		return GR_NOTTEAMMATE;

	if ((*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp(GetTeamID(pPlayer), GetTeamID(pTarget)))
	{
		return GR_TEAMMATE;
	}
#endif

	return GR_NOTTEAMMATE;
}

const char* CHL2MPRules::GetGameDescription(void)
{
	if (IsTeamplay())
		return "Team Deathmatch";

	return "Deathmatch";
}

bool CHL2MPRules::IsConnectedUserInfoChangeAllowed(CBasePlayer* pPlayer)
{
	return true;
}

float CHL2MPRules::GetMapRemainingTime()
{
	// if timelimit is disabled, return 0
	if (mp_timelimit.GetInt() <= 0)
		return 0;

	// timelimit is in minutes

	float timeleft = (m_flGameStartTime + mp_timelimit.GetInt() * 60.0f) - gpGlobals->curtime;

	return timeleft;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPRules::Precache(void)
{
	CBaseEntity::PrecacheScriptSound("AlyxEmp.Charge");
}

#ifdef GAME_DLL
bool CHL2MPRules::IsOfficialMap(void)
{
	static const char* s_OfficialMaps[] =
	{
		"devtest",
		"dm_lockdown",
		"dm_overwatch",
		"dm_powerhouse",
		"dm_resistance",
		"dm_runoff",
		"dm_steamlab",
		"dm_underpass",
		"halls3",
	};

	char szCurrentMap[MAX_MAP_NAME];
	Q_strncpy(szCurrentMap, STRING(gpGlobals->mapname), sizeof(szCurrentMap));

	for (int i = 0; i < ARRAYSIZE(s_OfficialMaps); ++i)
	{
		if (!Q_stricmp(s_OfficialMaps[i], szCurrentMap))
		{
			return true;
		}
	}

	return BaseClass::IsOfficialMap();
}
#endif

bool CHL2MPRules::ShouldCollide(int collisionGroup0, int collisionGroup1)
{
	if (collisionGroup0 > collisionGroup1)
	{
		// swap so that lowest is always first
		V_swap(collisionGroup0, collisionGroup1);
	}

	if ((collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_WEAPON)
	{
		return false;
	}

	return BaseClass::ShouldCollide(collisionGroup0, collisionGroup1);

}

bool CHL2MPRules::ClientCommand(CBaseEntity* pEdict, const CCommand& args)
{
#ifndef CLIENT_DLL
	if (BaseClass::ClientCommand(pEdict, args))
		return true;


	CHL2MP_Player* pPlayer = (CHL2MP_Player*)pEdict;

	if (pPlayer->ClientCommand(args))
		return true;
#endif

	return false;
}

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			3.5
// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)


CAmmoDef* GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if (!bInitted)
	{
		bInitted = true;

		def.AddAmmoType("AR2", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, 60, BULLET_IMPULSE(200, 1225), 0);
		def.AddAmmoType("AR2AltFire", DMG_DISSOLVE, TRACER_NONE, 0, 0, 3, 0, 0);
		def.AddAmmoType("Pistol", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, 150, BULLET_IMPULSE(200, 1225), 0);
		def.AddAmmoType("SMG1", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, 225, BULLET_IMPULSE(200, 1225), 0);
		def.AddAmmoType("357", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, 12, BULLET_IMPULSE(800, 5000), 0);
		def.AddAmmoType("XBowBolt", DMG_BULLET, TRACER_LINE, 0, 0, 10, BULLET_IMPULSE(800, 8000), 0);
		//def.AddAmmoType("Buckshot", DMG_BULLET | DMG_BUCKSHOT, TRACER_LINE, 0, 0, 30, BULLET_IMPULSE(400, 1200), 0);
		def.AddAmmoType("Buckshot", DMG_BULLET | DMG_BUCKSHOT, TRACER_LINE_AND_WHIZ, 0, 0, 30, BULLET_IMPULSE(400, 1200), 0);
		def.AddAmmoType("RPG_Round", DMG_BURN, TRACER_NONE, 0, 0, 3, 0, 0);
		def.AddAmmoType("SMG1_Grenade", DMG_BURN, TRACER_NONE, 0, 0, 3, 0, 0);
		def.AddAmmoType("Grenade", DMG_BURN, TRACER_NONE, 0, 0, 5, 0, 0);
		def.AddAmmoType("slam", DMG_BURN, TRACER_NONE, 0, 0, 5, 0, 0);
	}
	return &def;
}

#ifdef CLIENT_DLL

ConVar cl_autowepswitch(
	"cl_autowepswitch",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Automatically switch to picked up weapons (if more powerful)");

#else

bool CHL2MPRules::FShouldSwitchWeapon(CBasePlayer* pPlayer, CBaseCombatWeapon* pWeapon)
{
	if (pPlayer->GetActiveWeapon() && pPlayer->IsNetClient())
	{
		// Player has an active item, so let's check cl_autowepswitch.
		const char* cl_autowepswitch = engine->GetClientConVarValue(engine->IndexOfEdict(pPlayer->edict()), "cl_autowepswitch");
		if (cl_autowepswitch && atoi(cl_autowepswitch) <= 0)
		{
			return false;
		}
	}

	return BaseClass::FShouldSwitchWeapon(pPlayer, pWeapon);
}

#endif

#ifndef CLIENT_DLL

void CHL2MPRules::RestartGame()
{
	// bounds check
	if (mp_timelimit.GetInt() < 0)
	{
		mp_timelimit.SetValue(0);
	}
	m_flGameStartTime = gpGlobals->curtime;
	if (!IsFinite(m_flGameStartTime.Get()))
	{
		Warning("Trying to set a NaN game start time\n");
		m_flGameStartTime.GetForModify() = 0.0f;
	}

	CleanUpMap();

	// now respawn all players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);

		if (!pPlayer)
			continue;

		if (pPlayer->GetActiveWeapon())
		{
			pPlayer->GetActiveWeapon()->Holster();
		}
		pPlayer->RemoveAllItems(true);
		respawn(pPlayer, false);
		pPlayer->Reset();
	}

	// Respawn entities (glass, doors, etc..)

	CTeam* pRebels = GetGlobalTeam(TEAM_REBELS);
	CTeam* pCombine = GetGlobalTeam(TEAM_COMBINE);

	if (pRebels)
	{
		pRebels->SetScore(0);
	}

	if (pCombine)
	{
		pCombine->SetScore(0);
	}

	m_flIntermissionEndTime = 0;
	m_flRestartGameTime = 0.0;
	m_bCompleteReset = false;

	if (!HL2MPRules()->IsTeamplay())
	{
		IGameEvent* event = gameeventmanager->CreateEvent("round_start");
		if (event)
		{
			event->SetInt("fraglimit", 0);
			event->SetInt("priority", 6); // HLTV event priority, not transmitted
			event->SetString("objective", "DEATHMATCH");
			gameeventmanager->FireEvent(event);
		}
	}
	else
	{
		IGameEvent* event = gameeventmanager->CreateEvent("teamplay_round_start");
		if (event)
		{
			gameeventmanager->FireEvent(event);
		}

	}
}

#ifdef GAME_DLL
void CHL2MPRules::OnNavMeshLoad(void)
{
	TheNavMesh->SetPlayerSpawnName("info_player_deathmatch");
}
#endif

void CHL2MPRules::CleanUpMap()
{
	// Recreate all the map entities from the map data (preserving their indices),
	// then remove everything else except the players.

	// Get rid of all entities except players.
	CBaseEntity* pCur = gEntList.FirstEnt();
	while (pCur)
	{
		CBaseHL2MPCombatWeapon* pWeapon = dynamic_cast<CBaseHL2MPCombatWeapon*>(pCur);
		// Weapons with owners don't want to be removed..
		if (pWeapon)
		{
			if (!pWeapon->GetPlayerOwner())
			{
				UTIL_Remove(pCur);
			}
		}
		// remove entities that has to be restored on roundrestart (breakables etc)
		else if (!FindInList(s_PreserveEnts, pCur->GetClassname()))
		{
			UTIL_Remove(pCur);
		}

		pCur = gEntList.NextEnt(pCur);
	}

	// Really remove the entities so we can have access to their slots below.
	gEntList.CleanupDeleteList();

	// Cancel all queued events, in case a func_bomb_target fired some delayed outputs that
	// could kill respawning CTs
	g_EventQueue.Clear();

	// Now reload the map entities.
	class CHL2MPMapEntityFilter : public IMapEntityFilter
	{
	public:
		virtual bool ShouldCreateEntity(const char* pClassname)
		{
			// Don't recreate the preserved entities.
			if (!FindInList(s_PreserveEnts, pClassname))
			{
				return true;
			}
			else
			{
				// Increment our iterator since it's not going to call CreateNextEntity for this ent.
				if (m_iIterator != g_MapEntityRefs.InvalidIndex())
					m_iIterator = g_MapEntityRefs.Next(m_iIterator);

				return false;
			}
		}


		virtual CBaseEntity* CreateNextEntity(const char* pClassname)
		{
			if (m_iIterator == g_MapEntityRefs.InvalidIndex())
			{
				// This shouldn't be possible. When we loaded the map, it should have used 
				// CCSMapLoadEntityFilter, which should have built the g_MapEntityRefs list
				// with the same list of entities we're referring to here.
				Assert(false);
				return NULL;
			}
			else
			{
				CMapEntityRef& ref = g_MapEntityRefs[m_iIterator];
				m_iIterator = g_MapEntityRefs.Next(m_iIterator);	// Seek to the next entity.

				if (ref.m_iEdict == -1 || engine->PEntityOfEntIndex(ref.m_iEdict))
				{
					// Doh! The entity was delete and its slot was reused.
					// Just use any old edict slot. This case sucks because we lose the baseline.
					return CreateEntityByName(pClassname);
				}
				else
				{
					// Cool, the slot where this entity was is free again (most likely, the entity was 
					// freed above). Now create an entity with this specific index.
					return CreateEntityByName(pClassname, ref.m_iEdict);
				}
			}
		}

	public:
		int m_iIterator; // Iterator into g_MapEntityRefs.
	};
	CHL2MPMapEntityFilter filter;
	filter.m_iIterator = g_MapEntityRefs.Head();

	// DO NOT CALL SPAWN ON info_node ENTITIES!

	MapEntity_ParseAllEntities(engine->GetMapEntitiesString(), &filter, true);
}

void CHL2MPRules::CheckChatForReadySignal(CHL2MP_Player* pPlayer, const char* chatmsg)
{
	if (m_bAwaitingReadyRestart && FStrEq(chatmsg, mp_ready_signal.GetString()))
	{
		if (!pPlayer->IsReady())
		{
			pPlayer->SetReady(true);
		}
	}
}

void CHL2MPRules::CheckRestartGame(void)
{
	// Restart the game if specified by the server
	int iRestartDelay = mp_restartgame.GetInt();

	if (iRestartDelay > 0)
	{
		if (iRestartDelay > 60)
			iRestartDelay = 60;


		// let the players know
		char strRestartDelay[64];
		Q_snprintf(strRestartDelay, sizeof(strRestartDelay), "%d", iRestartDelay);
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Game will restart in %s1 %s2", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS");
		UTIL_ClientPrintAll(HUD_PRINTCONSOLE, "Game will restart in %s1 %s2", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS");

		m_flRestartGameTime = gpGlobals->curtime + iRestartDelay;
		m_bCompleteReset = true;
		mp_restartgame.SetValue(0);
	}

	if (mp_readyrestart.GetBool())
	{
		m_bAwaitingReadyRestart = true;
		m_bHeardAllPlayersReady = false;


		const char* pszReadyString = mp_ready_signal.GetString();


		// Don't let them put anything malicious in there
		if (pszReadyString == NULL || Q_strlen(pszReadyString) > 16)
		{
			pszReadyString = "ready";
		}

		IGameEvent* event = gameeventmanager->CreateEvent("hl2mp_ready_restart");
		if (event)
			gameeventmanager->FireEvent(event);

		mp_readyrestart.SetValue(0);

		// cancel any restart round in progress
		m_flRestartGameTime = -1;
	}
}

void CHL2MPRules::CheckAllPlayersReady(void)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);

		if (!pPlayer)
			continue;
		if (!pPlayer->IsReady())
			return;
	}
	m_bHeardAllPlayersReady = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* CHL2MPRules::GetChatFormat(bool bTeamOnly, CBasePlayer* pPlayer)
{
	if (!pPlayer)  // dedicated server output
	{
		return NULL;
	}

	const char* pszFormat = NULL;

	// team only
	if (bTeamOnly == TRUE)
	{
		if (pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
		{
			pszFormat = "HL2MP_Chat_Spec";
		}
		else
		{
			const char* chatLocation = GetChatLocation(bTeamOnly, pPlayer);
			if (chatLocation && *chatLocation)
			{
				pszFormat = "HL2MP_Chat_Team_Loc";
			}
			else
			{
				pszFormat = "HL2MP_Chat_Team";
			}
		}
	}
	// everyone
	else
	{
		if (pPlayer->GetTeamNumber() != TEAM_SPECTATOR)
		{
			pszFormat = "HL2MP_Chat_All";
		}
		else
		{
			pszFormat = "HL2MP_Chat_AllSpec";
		}
	}

	return pszFormat;
}

#endif
void CHL2MPRules::ChangeTeamplayMode(bool bTeamplay)
{
	m_bTeamPlayEnabled = bTeamplay;

	Msg("Switching to %s mode...\n", bTeamplay ? "Team Deathmatch" : "Deathmatch");

	ResetAllPlayersScores();
	RestartGame();

	if (HL2MPRules()->IsTeamplay())
	{
		ReassignPlayerTeams();
		IGameEvent* event = gameeventmanager->CreateEvent("teamplay_round_start");
		if (event)
		{
			gameeventmanager->FireEvent(event);
		}
	}
	else
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);
			if (pPlayer && !HL2MPRules()->IsTeamplay())
			{
				if (!pPlayer->IsHLTV() && !pPlayer->IsObserver() && pPlayer->IsConnected())
				{
					pPlayer->ChangeTeam(TEAM_UNASSIGNED);
				}
			}
		}
	}

	RecalculateTeamCounts();

	UTIL_ClientPrintAll(HUD_PRINTCENTER, bTeamplay ?
		"Switched to Team Deathmatch!" : "Switched to Deathmatch!");
}

void CHL2MPRules::ResetAllPlayersScores()
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);
		if (pPlayer)
		{
			if (!pPlayer->IsHLTV() && pPlayer->IsConnected())
			{
				pPlayer->ResetFragCount();
				pPlayer->ResetDeathCount();
			}
		}
	}

	for (int team = TEAM_COMBINE; team <= TEAM_REBELS; team++)
	{
		GetGlobalTeam(team)->SetScore(0);
	}
}

void CHL2MPRules::ReassignPlayerTeams()
{
	int iCombineCount = 0, iRebelCount = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);
		if (pPlayer && HL2MPRules()->IsTeamplay())
		{
			if (!pPlayer->IsHLTV() && !pPlayer->IsObserver() && pPlayer->IsConnected())
			{
				int newTeam;
				if (iCombineCount <= iRebelCount)
				{
					newTeam = TEAM_COMBINE;
					iCombineCount++;
				}
				else
				{
					newTeam = TEAM_REBELS;
					iRebelCount++;
				}
				pPlayer->ChangeTeam(newTeam);
			}
		}
	}
}

void CHL2MPRules::RecalculateTeamCounts()
{
	CTeam* pCombine = GetGlobalTeam(TEAM_COMBINE);
	CTeam* pRebels = GetGlobalTeam(TEAM_REBELS);

	if (pCombine && pRebels) 
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);
			if (pPlayer && HL2MPRules()->IsTeamplay())
			{
				if (!pPlayer->IsHLTV() && !pPlayer->IsObserver() && pPlayer->IsConnected() && pPlayer->GetTeamNumber() == TEAM_UNASSIGNED)
				{
					int team = pPlayer->GetTeamNumber();
					if (team == TEAM_COMBINE)
						pCombine->AddPlayer(pPlayer);
					else if (team == TEAM_REBELS)
						pRebels->AddPlayer(pPlayer);
				}
			}
		}
		pCombine->NetworkStateChanged();
		pRebels->NetworkStateChanged();
	}
}

bool CHL2MPRules::WasAirKill(CBasePlayer* pVictim, const CTakeDamageInfo& info)
{
	// This now correctly uses damage_display convars to be fully independent.
	if (!pVictim || !sv_damage_display_airkill_enable.GetBool())
		return false;

	// Condição 1: A vítima tem velocidade vertical alta?
	Vector velocity = pVictim->GetAbsVelocity();
	float verticalSpeed = fabs(velocity.z);
	if (verticalSpeed > sv_damage_display_airkill_velocity_threshold.GetFloat())
	{
		return true;
	}

	// Condição 2: Se não, a vítima está a uma altura mínima do chão?
	trace_t height_tr;
	Vector vecStart = pVictim->GetAbsOrigin();
	Vector vecEnd = vecStart - Vector(0, 0, sv_damage_display_airkill_height_threshold.GetFloat());
	UTIL_TraceLine(vecStart, vecEnd, MASK_PLAYERSOLID, pVictim, COLLISION_GROUP_NONE, &height_tr);

	if (height_tr.fraction > 0.99f)
	{
		return true;
	}

	return false;
}

void CHL2MPRules::ShowDamageDisplay(CBasePlayer* pAttacker, CBasePlayer* pVictim, int damageDealt, bool isKill, int healthLeft, int hitGroup, bool wasAirKill, bool wasBounceKill, int bounceCount)
{
	if (!sv_damage_display.GetBool() || !pAttacker || !pVictim)
		return;

	if (damageDealt <= 0)
		return;

	// << CORREÇÃO DEFINITIVA: Calculamos a vida restante manualmente.
	// A variável 'healthLeft' que a função recebe é a vida ANTES do dano.
	// A vida correta é simplesmente Vida Antes - Dano.
	int vidaCorreta = healthLeft - damageDealt;
	if (vidaCorreta < 0) vidaCorreta = 0; // Garante que não mostre vida negativa.


	// Check friendly fire settings
	if (!sv_damage_display_ff.GetBool() && IsTeamplay() &&
		pAttacker->GetTeamNumber() == pVictim->GetTeamNumber())
		return;

	// Don't show own damage unless enabled
	if (!sv_damage_display_own.GetBool() && pAttacker == pVictim)
		return;

	int displayArea = sv_damage_display_area.GetInt();

	if (displayArea == 1) // Center screen
	{
		// Always show damage info at damage position
		hudtextparms_t damageParams;
		damageParams.x = sv_damage_display_x.GetFloat();
		damageParams.y = sv_damage_display_y.GetFloat();
		damageParams.effect = sv_damage_display_fx_style.GetInt();

		// Determine color based on health left
		int r, g, b;
		// << CORREÇÃO DEFINITIVA: Usar a 'vidaCorreta' para determinar a cor
		if (vidaCorreta >= sv_damage_display_hp_high_min.GetInt())
		{
			ParseRGBColor(sv_damage_display_color_high.GetString(), r, g, b);
		}
		else if (vidaCorreta >= sv_damage_display_hp_medium_min.GetInt())
		{
			ParseRGBColor(sv_damage_display_color_medium.GetString(), r, g, b);
		}
		else if (vidaCorreta >= sv_damage_display_hp_low_min.GetInt())
		{
			ParseRGBColor(sv_damage_display_color_low.GetString(), r, g, b);
		}
		else // 0 HP
		{
			ParseRGBColor(sv_damage_display_color_dead.GetString(), r, g, b);
		}

		damageParams.r1 = r;
		damageParams.g1 = g;
		damageParams.b1 = b;
		damageParams.a1 = 255;
		damageParams.r2 = 255;
		damageParams.g2 = 255;
		damageParams.b2 = 0;
		damageParams.a2 = 255;
		damageParams.fadeinTime = sv_damage_display_fadein_time.GetFloat();
		damageParams.fadeoutTime = sv_damage_display_fadeout_time.GetFloat();
		damageParams.holdTime = sv_damage_display_hold_time.GetFloat();
		damageParams.fxTime = 0.25f;
		damageParams.channel = 2;

		char damageMessage[128];
		if (isKill && !sv_damage_display_show_hp_left_on_kill.GetBool())
		{
			Q_snprintf(damageMessage, sizeof(damageMessage), "-%d HP", damageDealt);
		}
		else
		{
			// << CORREÇÃO DEFINITIVA: Usar a 'vidaCorreta' na mensagem
			Q_snprintf(damageMessage, sizeof(damageMessage), "-%d HP\n[%d HP left]", damageDealt, vidaCorreta);
		}

		UTIL_HudMessage(pAttacker, damageParams, damageMessage);

		// Show kill message above if it's a kill
		if (isKill && sv_damage_display_kill.GetBool())
		{
			// If special kills are disabled, show the old generic message
			if (!sv_damage_display_special_kills.GetBool())
			{
				// ... (insert your old generic "KILL!" message code here) ...
			}
			else
			{
				CUtlVector<const char*> specialKills;
				bool isHeadshot = false;
				bool isAirKill = wasAirKill;
				bool isBounce = wasBounceKill;

				// 1. Build a list of all special kills that occurred
				if (hitGroup == HITGROUP_HEAD)
				{
					specialKills.AddToTail("HEADSHOT");
					isHeadshot = true;
				}
				if (isAirKill)
				{
					specialKills.AddToTail("AIRKILL");
				}
				if (isBounce)
				{
					if (sv_killerinfo_bounce_counter.GetBool() && bounceCount >= sv_killerinfo_bounce_counter_min.GetInt())
					{
						static char bounceText[32];
						Q_snprintf(bounceText, sizeof(bounceText), "BOUNCE KILL x%d", bounceCount);
						specialKills.AddToTail(bounceText);
					}
					else
					{
						specialKills.AddToTail("BOUNCE KILL");
					}
				}

				// Only proceed if at least one special kill happened, otherwise fall back to generic kill message
				if (specialKills.Count() > 0)
				{
					int r, g, b;

					// 2. Prioritize the color (Headshot > Airkill > Bounce)
					if (isHeadshot)
					{
						ParseRGBColor(sv_damage_display_color_headshot.GetString(), r, g, b);
					}
					else if (isAirKill)
					{
						ParseRGBColor(sv_damage_display_color_airkill.GetString(), r, g, b);
					}
					else // isBounce
					{
						ParseRGBColor(sv_damage_display_color_bouncekill.GetString(), r, g, b);
					}

					// 3. Format the final kill string based on combo style
					char finalKillMessage[256] = "";
					if (sv_damage_display_combo_style.GetInt() == 0) // Single Line
					{
						for (int i = 0; i < specialKills.Count(); ++i)
						{
							Q_strncat(finalKillMessage, specialKills[i], sizeof(finalKillMessage), COPY_ALL_CHARACTERS);
							if (i < specialKills.Count() - 1) // Add separator if not the last item
							{
								Q_strncat(finalKillMessage, sv_damage_display_combo_separator.GetString(), sizeof(finalKillMessage), COPY_ALL_CHARACTERS);
							}
						}
						Q_strncat(finalKillMessage, "!", sizeof(finalKillMessage), COPY_ALL_CHARACTERS);
					}
					else // Stacked
					{
						for (int i = 0; i < specialKills.Count(); ++i)
						{
							Q_strncat(finalKillMessage, specialKills[i], sizeof(finalKillMessage), COPY_ALL_CHARACTERS);
							if (i == specialKills.Count() - 1) // Add ! to the very last item
							{
								Q_strncat(finalKillMessage, "!", sizeof(finalKillMessage), COPY_ALL_CHARACTERS);
							}
							else // Add newline for all others
							{
								Q_strncat(finalKillMessage, "\n", sizeof(finalKillMessage), COPY_ALL_CHARACTERS);
							}
						}
					}

					// 4. Setup and send the HUD message
					hudtextparms_t killParams;
					killParams.x = sv_damage_display_kill_x.GetFloat();
					killParams.y = sv_damage_display_kill_y.GetFloat();
					killParams.effect = sv_damage_display_fx_style.GetInt();
					killParams.r1 = r;
					killParams.g1 = g;
					killParams.b1 = b;
					killParams.a1 = 255;
					killParams.r2 = 255;
					killParams.g2 = 255;
					killParams.b2 = 0;
					killParams.a2 = 255;
					killParams.fadeinTime = sv_damage_display_fadein_time.GetFloat();
					killParams.fadeoutTime = sv_damage_display_fadeout_time.GetFloat();
					killParams.holdTime = sv_damage_display_hold_time.GetFloat() + sv_damage_display_special_kills_additional_time.GetFloat();
					killParams.fxTime = 0.25f;
					killParams.channel = 3;

					UTIL_HudMessage(pAttacker, killParams, finalKillMessage);
				}
				else
				{
					// Fallback to the generic KILL message if no special conditions were met
					int kr, kg, kb;
					ParseRGBColor(sv_damage_display_color_kill.GetString(), kr, kg, kb);

					hudtextparms_t killParams;
					killParams.x = sv_damage_display_kill_x.GetFloat();
					killParams.y = sv_damage_display_kill_y.GetFloat();
					killParams.effect = sv_damage_display_fx_style.GetInt();
					killParams.r1 = kr;
					killParams.g1 = kg;
					killParams.b1 = kb;
					killParams.a1 = 255;
					killParams.r2 = 255;
					killParams.g2 = 255;
					killParams.b2 = 0;
					killParams.a2 = 255;
					killParams.fadeinTime = sv_damage_display_fadein_time.GetFloat();
					killParams.fadeoutTime = sv_damage_display_fadeout_time.GetFloat();
					killParams.holdTime = sv_damage_display_hold_time.GetFloat();
					killParams.fxTime = 0.25f;
					killParams.channel = 3;

					UTIL_HudMessage(pAttacker, killParams, "KILL!");
				}
			}
		}
	}
	else if (displayArea == 2) // Hint area
	{
		char message[128];
		if (isKill && sv_damage_display_kill.GetBool())
		{
			Q_snprintf(message, sizeof(message), "-%d HP KILL!", damageDealt);
		}
		else
		{
			Q_snprintf(message, sizeof(message), "-%d HP [%d left]", damageDealt, vidaCorreta);
		}
		ClientPrint(pAttacker, HUD_PRINTTALK, message);
	}
	else if (displayArea == 3) // Chat area
	{
		char message[128];
		if (isKill && sv_damage_display_kill.GetBool())
		{
			Q_snprintf(message, sizeof(message), "-%d HP KILL!", damageDealt);
		}
		else
		{
			Q_snprintf(message, sizeof(message), "-%d HP [%d left]", damageDealt, vidaCorreta);
		}
		ClientPrint(pAttacker, HUD_PRINTCONSOLE, message);
	}
}

bool CHL2MPRules::WasBounceKill(const CTakeDamageInfo& info)
{
	return (info.GetDamageType() & DMG_BOUNCE_KILL);
}

void CHL2MPRules::DisplayKillerInfo(CHL2MP_Player* pVictim, CHL2MP_Player* pKiller, const char* weaponName, int hitGroup, bool wasAirKill, bool wasBounceKill, int bounceCount)
{
	if (!pVictim || !pKiller || !sv_killerinfo_enable.GetBool())
		return;

	// <<< ESTA PARTE ESTAVA FALTANDO
	int killerHealth = pKiller->GetHealth();
	char killerHealthDisplay[16];

	int showNegative = sv_killerinfo_show_health_negative_values.GetInt();
	if (killerHealth < 0) {
		if (showNegative == 0) {
			killerHealth = 0;
			Q_snprintf(killerHealthDisplay, sizeof(killerHealthDisplay), "%d", killerHealth);
		}
		else if (showNegative == 1) {
			Q_snprintf(killerHealthDisplay, sizeof(killerHealthDisplay), "%d", killerHealth);
		}
		else if (showNegative == 2) {
			Q_strncpy(killerHealthDisplay, "{#ff0000}DEAD", sizeof(killerHealthDisplay));
		}
		else {
			killerHealth = 0;
			Q_snprintf(killerHealthDisplay, sizeof(killerHealthDisplay), "%d", killerHealth);
		}
	}
	else {
		Q_snprintf(killerHealthDisplay, sizeof(killerHealthDisplay), "%d", killerHealth);
	}

	int killerArmor = pKiller->ArmorValue();

	Vector killerPos = pKiller->GetAbsOrigin();
	Vector victimPos = pVictim->GetAbsOrigin();
	float distance = killerPos.DistTo(victimPos);
	float displayDistance;
	const char* unitStr;

	if (sv_killerinfo_distance_unit.GetInt() == 1)
	{
		displayDistance = distance * 0.0625f; // pés
		unitStr = "ft";
	}
	else
	{
		displayDistance = distance * 0.01905f; // metros
		unitStr = "m";
	}

	char message[256];
	char weaponDisplay[64] = "";
	char distanceDisplay[32] = "";
	char armorDisplay[32] = "";

	if (sv_killerinfo_show_weapon.GetBool() && weaponName)
	{
		Q_snprintf(weaponDisplay, sizeof(weaponDisplay), CHAT_WHITE " With" CHAT_COMBINE " %s", weaponName);
	}

	if (sv_killerinfo_show_distance.GetBool())
	{
		Q_snprintf(distanceDisplay, sizeof(distanceDisplay), CHAT_WHITE " | Dist: " CHAT_DM "%.1f%s", displayDistance, unitStr);
	}

	if (sv_killerinfo_show_armor.GetBool() && killerArmor > 0)
	{
		Q_snprintf(armorDisplay, sizeof(armorDisplay), CHAT_WHITE " | AP:" CHAT_COMBINE " %d", killerArmor);
	}

	const char* hpColor;
	if (killerHealth >= sv_damage_display_hp_high_min.GetInt())
		hpColor = CHAT_FULLGREEN;
	else if (killerHealth >= sv_damage_display_hp_medium_min.GetInt())
		hpColor = CHAT_FULLYELLOW;
	else if (killerHealth >= sv_damage_display_hp_low_min.GetInt())
		hpColor = CHAT_FULLRED;
	else
		hpColor = CHAT_FULLRED;
	// <<< FIM DA PARTE QUE FALTAVA

	// --- Bloco de Kills Especiais ---
	char specialDisplay[128] = "";

	if (wasAirKill)
	{
		Q_strncat(specialDisplay, " " CHAT_DEEPPINK "[AIRKILL]", sizeof(specialDisplay), COPY_ALL_CHARACTERS);
	}
	if ((hitGroup == HITGROUP_HEAD) && sv_killerinfo_headshot_enable.GetBool())
	{
		Q_strncat(specialDisplay, " " CHAT_PETROL "[HEADSHOT]", sizeof(specialDisplay), COPY_ALL_CHARACTERS);
	}

	if (wasBounceKill && sv_killerinfo_bouncekill_enable.GetBool())
	{
		char bounceText[64];
		if (sv_killerinfo_bounce_counter.GetBool() && bounceCount >= sv_killerinfo_bounce_counter_min.GetInt())
		{
			Q_snprintf(bounceText, sizeof(bounceText), " " CHAT_DEEPORANGE "[BOUNCE KILL " CHAT_FULLYELLOW "x%d" CHAT_DEEPORANGE "]", bounceCount);
		}
		else
		{
			Q_snprintf(bounceText, sizeof(bounceText), " " CHAT_DEEPORANGE "[BOUNCE KILL]");
		}
		Q_strncat(specialDisplay, bounceText, sizeof(specialDisplay), COPY_ALL_CHARACTERS);
	}

	// --- Montagem da Mensagem Final ---
	Q_snprintf(message, sizeof(message),
		CHAT_FULLRED "> " CHAT_WHITE "Killed by: " CHAT_FULLRED "%s%s \n" CHAT_FULLRED "> " CHAT_WHITE "[HP: %s%d" CHAT_WHITE "%s%s" CHAT_WHITE "]%s",
		pKiller->GetPlayerName(), weaponDisplay, hpColor, killerHealth, distanceDisplay, armorDisplay, specialDisplay);

	ClientPrint(pVictim, HUD_PRINTTALK, message);
}


bool CHL2MPRules::ShouldShowKillerInfo(CHL2MP_Player* pPlayer)
{
	return (pPlayer && sv_killerinfo_enable.GetBool());
}

void CHL2MPRules::SendColoredKillerMessage(CHL2MP_Player* pVictim, const char* message)
{
	if (pVictim && message)
		ClientPrint(pVictim, HUD_PRINTTALK, message);
}

//bool CHL2MPRules::WasAirKill(CHL2MP_Player* pVictim, const CTakeDamageInfo& info)
//{
//	if (!pVictim)
//		return false;
//
//	// PASSO 2: O VETO DO CHÃO
//	trace_t ground_tr;
//	Vector vecStart = pVictim->GetAbsOrigin();
//	Vector vecEnd = vecStart - Vector(0, 0, 16);
//
//	UTIL_TraceLine(vecStart, vecEnd, MASK_PLAYERSOLID, pVictim, COLLISION_GROUP_NONE, &ground_tr);
//
//	if (ground_tr.fraction < 1.0f)
//	{
//		// Log para depuração ATIVADO
//		Msg("WasAirKill VETO: Jogador considerado no chao. Fracao do trace: %.2f\n", ground_tr.fraction);
//		return false;
//	}
//
//	// PASSO 3: CONFIRMAÇÃO DO AIRKILL
//	Vector velocity = pVictim->GetAbsVelocity();
//	float verticalSpeed = fabs(velocity.z);
//	if (verticalSpeed > sv_killerinfo_airkill_velocity_threshold.GetFloat())
//	{
//		// Log para depuração ATIVADO
//		Msg("WasAirKill SUCESSO: Velocidade vertical (%.2f) maior que o limite (%.2f)\n", verticalSpeed, sv_killerinfo_airkill_velocity_threshold.GetFloat());
//		return true;
//	}
//
//	trace_t height_tr;
//	float heightThreshold = sv_killerinfo_airkill_height_threshold.GetFloat();
//	vecEnd = vecStart - Vector(0, 0, heightThreshold);
//
//	UTIL_TraceLine(vecStart, vecEnd, MASK_PLAYERSOLID, pVictim, COLLISION_GROUP_NONE, &height_tr);
//
//	if (height_tr.fraction > 0.99f)
//	{
//		// Log para depuração ATIVADO
//		Msg("WasAirKill SUCESSO: Altura maior que o limite (%.2f)\n", heightThreshold);
//		return true;
//	}
//
//	Msg("WasAirKill FALHA: Nao passou em nenhuma condicao de confirmacao.\n");
//	return false;
//}

int CHL2MPRules::GetBounceCount(const CTakeDamageInfo& info)
{
	CBaseEntity* pInflictor = info.GetInflictor();
	if (pInflictor && FClassnameIs(pInflictor, "crossbow_bolt"))
	{
		return pInflictor->GetHealth();
	}
	return 0;
}

void CHL2MPRules::LevelShutdown(void)
{
	// Desliga nosso sistema de SaveScores e limpa os arquivos
	// CORREÇÃO: Usar o nome da classe CSaveScores:: e a função CleanupScoreFiles()
	CSaveScores::CleanupScoreFiles();

	BaseClass::LevelShutdown();
}

void CHL2MPRules::LevelInitPostEntity()
{
	BaseClass::LevelInitPostEntity();
	CSaveScores::Init();

	if (g_pSpawnWeaponsManager)
	{
		g_pSpawnWeaponsManager->LevelInit();
	}
}

void CHL2MPRules::PlayerSpawn(CBasePlayer* pPlayer)
{
	BaseClass::PlayerSpawn(pPlayer); // O jogo reseta o score para 0 aqui

	int restoredKills = 0;
	int restoredDeaths = 0;

	// Pede os dados para o nosso sistema de scores
	if (CSaveScores::LoadPlayerScoreData(pPlayer, restoredKills, restoredDeaths))
	{
		// =======================================================================
		// ESTA É A VERSÃO CORRETA QUE USA O TRUQUE
		// =======================================================================

		// Se houver kills para restaurar (positivos OU negativos)...
		if (restoredKills != 0)
		{
			// ...usamos a mesma função para somar ou subtrair, passando o valor total.
			// Se restoredKills for -2, a função vai somar -2, efetivamente tirando 2 kills.
			pPlayer->IncrementFragCount(restoredKills);
		}

		// Fazemos o mesmo para as mortes.
		if (restoredDeaths != 0)
		{
			pPlayer->IncrementDeathCount(restoredDeaths);
		}
	}
}