#ifndef SPAWNWEAPONS_MANAGER_H
#define SPAWNWEAPONS_MANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "convar.h"

class KeyValues;
class CHL2MP_Player;

// External ConVar declarations
extern ConVar sv_spawnweapons_enable;
extern ConVar sv_spawnweapons_strip_on_spawn;
extern ConVar sv_spawn_loadouts_file;
extern ConVar sv_spawnweapons_debug;

class CSpawnWeaponsManager
{
public:
    CSpawnWeaponsManager();
    ~CSpawnWeaponsManager();

    void LevelInit();
    void ApplyPlayerLoadout(CHL2MP_Player* pPlayer);
    void OnPlayerSpawn(CHL2MP_Player* pPlayer);  // Better name than OnPlayerDeath
    void OnPlayerDeath(CHL2MP_Player* pPlayer);  // Keep for compatibility

private:
    void ReloadConfigFile();
    void ClearConfig();
    void SafeRemovePlayerWeapons(CHL2MP_Player* pPlayer);
    void DebugMsg(const char* fmt, ...);

    KeyValues* m_pLoadoutGroupsKV;
};

extern CSpawnWeaponsManager* g_pSpawnWeaponsManager;

#endif // SPAWNWEAPONS_MANAGER_H