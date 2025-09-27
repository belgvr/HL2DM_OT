//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== tf_client.cpp ========================================================

  HL2 client/server game specific stuff

*/

#include "cbase.h"
#include "hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "team.h"
#include "viewport_panel_names.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Host_Say( edict_t *pEdict, bool teamonly );

ConVar sv_motd_unload_on_dismissal( "sv_motd_unload_on_dismissal", "0", 0, "If enabled, the MOTD contents will be unloaded when the player closes the MOTD." );
ConVar sv_displaymotd( "sv_displaymotd", "1", 0, "If enabled, display the MOTD to players after server entry." );

ConVar sv_connect_msg_mode("sv_connect_msg_mode", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Connection message mode: 0 = legacy, 1 = colored text");
ConVar sv_connect_new_mode_sound("sv_connect_new_mode_sound", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Play a sound when a player connects in new mode (sv_connect_msg_mode 1)");
ConVar sv_connect_new_mode_sound_file("sv_connect_new_mode_sound_file", "npc/turret_floor/deploy.wav", FCVAR_GAMEDLL | FCVAR_NOTIFY, "Sound to play when a player connects in new mode (sv_connect_msg_mode 1)");

extern CBaseEntity*	FindPickerEntityClass( CBasePlayer *pPlayer, char *classname );
extern bool			g_fGameOver;
extern int g_voters;

void CBasePlayer::DelayedSpawn()
{
	Spawn();
}

void FinishClientPutInServer( CHL2MP_Player *pPlayer )
{
	g_voters++;
	pPlayer->InitialSpawn();
	// Peter: I can't seem to find anything that would suggest 
	// this would be broken after connecting to a server, but clearly, 
	// delaying this fixes: 
	// 
	// 1) The spawning angles of 0, 0, 0 
	// 2) Always spawning in showers on lockdown
	pPlayer->SetContextThink( &CBasePlayer::DelayedSpawn, gpGlobals->curtime + 0.01f, "DelayedSnapEyeAngles" );

	UTIL_SendConVarValue( pPlayer->edict(), "sv_wpn_sway_pred_legacy", "1" );

	char sName[128];
	Q_strncpy( sName, pPlayer->GetPlayerName(), sizeof( sName ) );
	
	// First parse the name and remove any %'s
	for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
	{
		// Replace it with a space
		if ( *pApersand == '%' )
				*pApersand = ' ';
	}

	// notify other clients of player joining the game
	if (sv_connect_msg_mode.GetInt() == 1)
	{
		char rawMessage[256];
		Q_snprintf(rawMessage, sizeof(rawMessage), "{#00ff00}> {#ffffff}Player {#00ff00}%s {#ffffff}is Connected", sName[0] != 0 ? sName : "<unconnected>");

		char formattedMessage[256];
		UTIL_FormatColors(rawMessage, formattedMessage, sizeof(formattedMessage));

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex(i);
			if (pLoopPlayer && pLoopPlayer->IsConnected())
			{
				ClientPrint(pLoopPlayer, HUD_PRINTTALK, formattedMessage);

				if (sv_connect_new_mode_sound.GetBool())
				{
					enginesound->PrecacheSound(sv_connect_new_mode_sound_file.GetString(), true);
					pLoopPlayer->EmitSound(sv_connect_new_mode_sound_file.GetString());
				}
			}
		}
	}
	else
	{
		UTIL_ClientPrintAll(HUD_PRINTNOTIFY, "#Game_connected", sName[0] != 0 ? sName : "<unconnected>");
	}

	if ( HL2MPRules()->IsTeamplay() == true )
	{
		ClientPrint( pPlayer, HUD_PRINTTALK, "You are on team %s1/n", pPlayer->GetTeam()->GetName() );
	}

	if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
	{
		pPlayer->RemoveAllItems( true );
	}

	if ( sv_displaymotd.GetBool() )
	{
		const ConVar *hostname = cvar->FindVar( "hostname" );
		const char *title = ( hostname ) ? hostname->GetString() : "MESSAGE OF THE DAY";

		KeyValues *data = new KeyValues( "data" );
		data->SetString( "title", title );		// info panel title
		data->SetString( "type", "1" );			// show userdata from stringtable entry
		data->SetString( "msg", "motd" );		// use this stringtable entry
		data->SetBool( "unload", sv_motd_unload_on_dismissal.GetBool() );

		pPlayer->ShowViewPortPanel( PANEL_INFO, true, data );

		data->deleteThis();
	}
}

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBaseTFPlayer for pev, and call spawn
	CHL2MP_Player *pPlayer = CHL2MP_Player::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
}


void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	// Can't load games in CS!
	Assert( !bLoadGame );

	CHL2MP_Player *pPlayer = ToHL2MPPlayer( CBaseEntity::Instance( pEdict ) );
	pPlayer->CompensateScoreOnTeamSwitch( false );
	pPlayer->CompensateTeamScoreOnTeamSwitch( false );
	FinishClientPutInServer( pPlayer );
}


/*
===============
const char *GetGameDescription()

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Half-Life 2 Deathmatch";
}

//-----------------------------------------------------------------------------
// Purpose: Given a player and optional name returns the entity of that 
//			classname that the player is nearest facing
//			
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* FindEntity( edict_t *pEdict, char *classname)
{
	// If no name was given set bits based on the picked
	if (FStrEq(classname,"")) 
	{
		return (FindPickerEntityClass( static_cast<CBasePlayer*>(GetContainingEntity(pEdict)), classname ));
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	CBaseEntity::PrecacheModel("models/player.mdl");
	CBaseEntity::PrecacheModel( "models/gibs/agibs.mdl" );
	CBaseEntity::PrecacheModel ("models/weapons/v_hands.mdl");

	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowAmmo" );
	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowHealth" );

	CBaseEntity::PrecacheScriptSound( "FX_AntlionImpact.ShellImpact" );
	CBaseEntity::PrecacheScriptSound( "Missile.ShotDown" );
	CBaseEntity::PrecacheScriptSound( "Bullets.DefaultNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.GunshipNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.StriderNearmiss" );
	
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepHigh" );
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepLow" );
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( pEdict );

	if ( pPlayer )
	{
		if ( gpGlobals->curtime > pPlayer->GetDeathTime() + DEATH_ANIMATION_TIME )
		{		
			// respawn player
			pPlayer->Spawn();			
		}
		else
		{
			pPlayer->SetNextThink( gpGlobals->curtime + 0.1f );
		}
	}
}

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");
	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = (teamplay.GetInt() != 0);
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	// vanilla deathmatch
	CreateGameRulesObject( "CHL2MPRules" );
}