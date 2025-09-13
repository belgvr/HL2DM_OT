//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:      $
// $Date:          $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef HL2MP_GAMERULES_H
#define HL2MP_GAMERULES_H
#pragma once

#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "gamevars_shared.h"

#ifndef CLIENT_DLL
#include "hl2mp_player.h"
#endif

#define VEC_CROUCH_TRACE_MIN	HL2MPRules()->GetHL2MPViewVectors()->m_vCrouchTraceMin
#define VEC_CROUCH_TRACE_MAX	HL2MPRules()->GetHL2MPViewVectors()->m_vCrouchTraceMax

enum
{
	TEAM_COMBINE = 2,
	TEAM_REBELS,
};

#ifdef CLIENT_DLL
#define CHL2MPRules C_HL2MPRules
#define CHL2MPGameRulesProxy C_HL2MPGameRulesProxy
#endif

class CHL2MPGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS(CHL2MPGameRulesProxy, CGameRulesProxy);
	DECLARE_NETWORKCLASS();
};

class HL2MPViewVectors : public CViewVectors
{
public:
	HL2MPViewVectors(
		Vector vView,
		Vector vHullMin,
		Vector vHullMax,
		Vector vDuckHullMin,
		Vector vDuckHullMax,
		Vector vDuckView,
		Vector vObsHullMin,
		Vector vObsHullMax,
		Vector vDeadViewHeight,
		Vector vCrouchTraceMin,
		Vector vCrouchTraceMax) :
		CViewVectors(
			vView,
			vHullMin,
			vHullMax,
			vDuckHullMin,
			vDuckHullMax,
			vDuckView,
			vObsHullMin,
			vObsHullMax,
			vDeadViewHeight)
	{
		m_vCrouchTraceMin = vCrouchTraceMin;
		m_vCrouchTraceMax = vCrouchTraceMax;
	}

	Vector m_vCrouchTraceMin;
	Vector m_vCrouchTraceMax;
};

class CHL2MPRules : public CTeamplayRules
{
public:
	DECLARE_CLASS(CHL2MPRules, CTeamplayRules);

#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.
#else
	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
#endif

	CHL2MPRules();
	virtual ~CHL2MPRules();
	virtual	void ChangeTeamplayMode(bool bTeamplay);
	virtual void RecalculateTeamCounts();
	virtual void ResetAllPlayersScores();
	virtual void ReassignPlayerTeams();
	virtual void Precache(void);
	virtual bool ShouldCollide(int collisionGroup0, int collisionGroup1);
	virtual bool ClientCommand(CBaseEntity* pEdict, const CCommand& args);

	virtual float FlWeaponRespawnTime(CBaseCombatWeapon* pWeapon);
	virtual float FlWeaponTryRespawn(CBaseCombatWeapon* pWeapon);
	virtual Vector VecWeaponRespawnSpot(CBaseCombatWeapon* pWeapon);
	virtual QAngle DefaultWeaponRespawnAngle(CBaseCombatWeapon* pWeapon);
	virtual int WeaponShouldRespawn(CBaseCombatWeapon* pWeapon);
	virtual void Think(void);
	virtual void CreateStandardEntities(void);
	virtual void ClientSettingsChanged(CBasePlayer* pPlayer);
	virtual int PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget);
	virtual void GoToIntermission(void);
	virtual void DeathNotice(CBasePlayer* pVictim, const CTakeDamageInfo& info);
	virtual const char* GetGameDescription(void);
	// derive this function if you mod uses encrypted weapon info files
	virtual const unsigned char* GetEncryptionKey(void) { return (unsigned char*)"x9Ke0BY7"; }
	virtual const CViewVectors* GetViewVectors() const;
	const HL2MPViewVectors* GetHL2MPViewVectors() const;

	float GetMapRemainingTime();
	void CleanUpMap();
	void CheckRestartGame();
	void RestartGame();

	void OnNavMeshLoad(void);

#ifndef CLIENT_DLL
	virtual Vector VecItemRespawnSpot(CItem* pItem);
	virtual QAngle VecItemRespawnAngles(CItem* pItem);
	virtual float	FlItemRespawnTime(CItem* pItem);
	virtual bool	CanHavePlayerItem(CBasePlayer* pPlayer, CBaseCombatWeapon* pItem);
	virtual bool FShouldSwitchWeapon(CBasePlayer* pPlayer, CBaseCombatWeapon* pWeapon);

	void	AddLevelDesignerPlacedObject(CBaseEntity* pEntity);
	void	RemoveLevelDesignerPlacedObject(CBaseEntity* pEntity);
	void	ManageObjectRelocation(void);
	void    CheckChatForReadySignal(CHL2MP_Player* pPlayer, const char* chatmsg);
	const char* GetChatFormat(bool bTeamOnly, CBasePlayer* pPlayer);

	// NEW: Damage Display System
	void ShowDamageDisplay(CBasePlayer* pAttacker, CBasePlayer* pVictim, int damageDealt, bool isKill, int healthLeft);

	// NEW: Killer Info System   
	void DisplayKillerInfo(CHL2MP_Player* pVictim, CHL2MP_Player* pKiller, const char* weaponName, int hitGroup, bool wasAirKill, bool wasBounceKill, int bounceCount); // MODIFICADO

	bool WasBounceKill(const CTakeDamageInfo& info); // Bounce Kill detection
	int GetBounceCount(const CTakeDamageInfo& info);

	bool ShouldShowKillerInfo(CHL2MP_Player* pPlayer);
	void SendColoredKillerMessage(CHL2MP_Player* pVictim, const char* message);
	//bool WasAirKill(CHL2MP_Player* pVictim);
	bool WasAirKill(CHL2MP_Player* pVictim, const CTakeDamageInfo& info);


#endif

	bool IsOfficialMap(void);

	virtual void ClientDisconnected(edict_t* pClient);

	bool CheckGameOver(void);
	bool IsIntermission(void);

	void PlayerKilled(CBasePlayer* pVictim, const CTakeDamageInfo& info);

	bool	IsTeamplay(void) { return m_bTeamPlayEnabled; }
	void	CheckAllPlayersReady(void);

	virtual bool IsConnectedUserInfoChangeAllowed(CBasePlayer* pPlayer);

#ifndef CLIENT_DLL
	// NEW: Enhanced HUD and Display Systems
	void HandleNewTargetID();
	void HandleTimeleft();
	hudtextparms_t CreateTextParams();
	void FormatStandardTime(int iTimeRemaining, char* buffer, size_t bufferSize);
	void UpdateTeamScoreColors(hudtextparms_t& textParams);
	void DisplayUnassignedTeamStats(hudtextparms_t& textParams, const char* stime);
	void DisplaySpectatorStats(hudtextparms_t& textParams, const char* stime);
	void SendHudMessagesToPlayers(hudtextparms_t& textParams, const char* stime);
	void FormatTimeRemaining(int iTimeRemaining, char* buffer, size_t bufferSize);
	void HandlePlayerNetworkCheck();

	// NEW: Map Voting System
	void SetMapChangeOnGoing(bool enabled) { bMapChangeOnGoing = enabled; }
	void SetMapChange(bool enabled) { bMapChange = enabled; }
	bool IsMapChangeOnGoing() const { return bMapChangeOnGoing; }
	bool IsMapChange() const { return bMapChange; }
	void SetScheduledMapName(const char* mapName) { Q_strncpy(m_scheduledMapName, mapName, sizeof(m_scheduledMapName)); }
	void HandleMapVotes();

	// NEW: Equipment and spectator management
	void RemoveAllPlayersEquipment();
#endif

private:

	CNetworkVar(bool, m_bTeamPlayEnabled);
	CNetworkVar(float, m_flGameStartTime);
	CUtlVector<EHANDLE> m_hRespawnableItemsAndWeapons;
	float m_tmNextPeriodicThink;
	float m_flRestartGameTime;
	bool m_bCompleteReset;
	bool m_bAwaitingReadyRestart;
	bool m_bHeardAllPlayersReady;

#ifndef CLIENT_DLL
	bool m_bChangelevelDone;

	// NEW: Map change system variables
	bool bMapChangeOnGoing;
	bool bMapChange;
	float m_flMapChangeTime;
	char m_scheduledMapName[64];  // The map name to change to
#endif
};

inline CHL2MPRules* HL2MPRules()
{
	return static_cast<CHL2MPRules*>(g_pGameRules);
}

//-----------------------------------------------------------------------------
// External utility function declarations
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
// Color parsing utility used by multiple systems
void ParseRGBColor(const char* colorString, int& r, int& g, int& b);

// External globals for map voting system
extern bool bAdminMapChange;
extern int g_voters;
extern int g_votes;
extern int g_votesneeded;
extern bool g_votebegun;
extern bool g_votehasended;
extern int g_votetime;
extern int g_timetortv;
extern bool g_rtvbooted;

extern ConVar sv_killerinfo_airkill_velocity_threshold; 
extern ConVar sv_killerinfo_airkill_height_threshold;
extern ConVar sv_killerinfo_bouncekill_enable;
extern ConVar sv_killerinfo_bounce_counter; 

// Map voting utility
extern void StartMapVote();

#endif

#endif //HL2MP_GAMERULES_H