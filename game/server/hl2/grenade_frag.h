//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef GRENADE_FRAG_H
#define GRENADE_FRAG_H
#pragma once

#include "basegrenade_shared.h"

class CBaseGrenade;
class CBasePlayer;
struct edict_t;

// Forward declarations for enhanced frag grenade system
class CSprite;
class CSpriteTrail;

//-----------------------------------------------------------------------------
// Factory and utility functions
//-----------------------------------------------------------------------------
CBaseGrenade* Fraggrenade_Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner, float timer, bool combineSpawned);
bool	Fraggrenade_WasPunted(const CBaseEntity* pEntity);
bool	Fraggrenade_WasCreatedByCombine(const CBaseEntity* pEntity);

//-----------------------------------------------------------------------------
// External ConVar declarations for frag grenade customization
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL

// Core damage and radius CVars
extern ConVar sk_plr_dmg_fraggrenade;
extern ConVar sk_npc_dmg_fraggrenade;
extern ConVar sk_fraggrenade_radius;

// NEW: Sprite customization CVars
extern ConVar sv_grenade_frag_glow_sprite;
extern ConVar sv_grenade_frag_trail_sprite;
extern ConVar sv_grenade_frag_glow_color;
extern ConVar sv_grenade_frag_trail_color;

// NEW: Trail appearance CVars
extern ConVar sv_grenade_frag_trail_lifetime;
extern ConVar sv_grenade_frag_trail_startwidth;
extern ConVar sv_grenade_frag_trail_endwidth;

// NEW: Team-based color system CVars
extern ConVar sv_grenade_frag_byteams;
extern ConVar sv_grenade_frag_rebels_trail_color;
extern ConVar sv_grenade_frag_rebels_glow_color;
extern ConVar sv_grenade_frag_combine_trail_color;
extern ConVar sv_grenade_frag_combine_glow_color;

#endif

#endif // GRENADE_FRAG_H