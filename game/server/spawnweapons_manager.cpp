#include "cbase.h"
#include "spawnweapons_manager.h"
#include "hl2mp_player.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "hl2mp_gamerules.h"
#include "ammodef.h"
#include "weapon_hl2mpbase.h"

// CVars
ConVar sv_spawnweapons_enable("sv_spawnweapons_enable", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY,
    "Enable/Disable the custom spawn loadouts system.");
ConVar sv_spawn_loadouts_file("sv_spawn_loadouts_file", "cfg/utils/spawn_weapons/spawn_loadouts.txt", FCVAR_GAMEDLL | FCVAR_NOTIFY,
    "Path to the master spawn loadouts configuration file.");
ConVar sv_spawnweapons_strip_on_spawn("sv_spawnweapons_strip_on_spawn", "1", FCVAR_GAMEDLL | FCVAR_NOTIFY,
    "1 = Remove all player weapons before giving the new loadout.");
ConVar sv_spawnweapons_debug("sv_spawnweapons_debug", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY,
    "Enable debug messages for the spawn loadouts system.");

// Global instance
CSpawnWeaponsManager g_SpawnWeaponsManager;
CSpawnWeaponsManager* g_pSpawnWeaponsManager = &g_SpawnWeaponsManager;

// Console command
CON_COMMAND(sv_spawnweapons_reload, "Reloads the master spawn loadouts configuration file.")
{
    if (g_pSpawnWeaponsManager)
    {
        g_pSpawnWeaponsManager->LevelInit();
        Msg("[SpawnWeapons] Configuration reloaded.\n");
    }
}

CSpawnWeaponsManager::CSpawnWeaponsManager() : m_pLoadoutGroupsKV(NULL)
{
}

CSpawnWeaponsManager::~CSpawnWeaponsManager()
{
    ClearConfig();
}

void CSpawnWeaponsManager::ClearConfig()
{
    if (m_pLoadoutGroupsKV)
    {
        m_pLoadoutGroupsKV->deleteThis();
        m_pLoadoutGroupsKV = NULL;
    }
}

void CSpawnWeaponsManager::ReloadConfigFile()
{
    ClearConfig();

    const char* filePath = sv_spawn_loadouts_file.GetString();
    if (!filePath || !*filePath)
    {
        Warning("[SpawnWeapons] No configuration file path specified.\n");
        return;
    }

    m_pLoadoutGroupsKV = new KeyValues("SpawnLoadoutGroups");
    if (m_pLoadoutGroupsKV->LoadFromFile(filesystem, filePath, "GAME"))
    {
        DebugMsg("[SpawnWeapons] Master loadout config loaded from %s\n", filePath);
    }
    else
    {
        Warning("[SpawnWeapons] FAILED to load master loadout config: %s\n", filePath);
        m_pLoadoutGroupsKV->deleteThis();
        m_pLoadoutGroupsKV = NULL;
    }
}

void CSpawnWeaponsManager::LevelInit()
{
    ReloadConfigFile();
}

void CSpawnWeaponsManager::DebugMsg(const char* fmt, ...)
{
    if (!sv_spawnweapons_debug.GetBool())
        return;

    va_list args;
    va_start(args, fmt);

    char buffer[1024];
    Q_vsnprintf(buffer, sizeof(buffer), fmt, args);
    Msg("[SpawnWeapons DEBUG] %s", buffer);

    va_end(args);
}

void CSpawnWeaponsManager::SafeRemovePlayerWeapons(CHL2MP_Player* pPlayer)
{
    if (!pPlayer)
        return;

    DebugMsg("Safely removing weapons from player %s\n", pPlayer->GetPlayerName());

    // First, holster and clear the active weapon reference
    CBaseCombatWeapon* pActiveWeapon = pPlayer->GetActiveWeapon();
    if (pActiveWeapon)
    {
        pActiveWeapon->Holster();
        pPlayer->SetActiveWeapon(NULL);
        DebugMsg("Holstered active weapon: %s\n", pActiveWeapon->GetClassname());
    }

    // Build a list of weapons to remove (to avoid iterator invalidation)
    CUtlVector<CBaseCombatWeapon*> weaponsToRemove;

    for (int i = 0; i < pPlayer->WeaponCount(); i++)
    {
        CBaseCombatWeapon* pWeapon = pPlayer->GetWeapon(i);
        if (pWeapon)
        {
            weaponsToRemove.AddToTail(pWeapon);
        }
    }

    // Now safely remove each weapon
    for (int i = 0; i < weaponsToRemove.Count(); i++)
    {
        CBaseCombatWeapon* pWeapon = weaponsToRemove[i];
        if (!pWeapon)
            continue;

        DebugMsg("Removing weapon: %s\n", pWeapon->GetClassname());

        // Ensure the weapon is holstered
        if (pWeapon->IsWeaponVisible())
        {
            pWeapon->Holster();
        }

        // Clear ownership before removal
        pWeapon->SetOwner(NULL);
        pWeapon->SetOwnerEntity(NULL);

        // Remove from player's weapon list
        pPlayer->Weapon_Detach(pWeapon);

        // Mark for deletion on next frame
        UTIL_Remove(pWeapon);
    }

    // Clear all ammo
    pPlayer->RemoveAllAmmo();

    DebugMsg("Weapon removal completed for player %s\n", pPlayer->GetPlayerName());
}

void CSpawnWeaponsManager::OnPlayerSpawn(CHL2MP_Player* pPlayer)
{
    if (!pPlayer)
        return;

    DebugMsg("OnPlayerSpawn called for player %s\n", pPlayer->GetPlayerName());

    // Small delay to ensure player is fully spawned
    pPlayer->SetThink(&CHL2MP_Player::Spawn);
    pPlayer->SetNextThink(gpGlobals->curtime + 0.1f);
}

void CSpawnWeaponsManager::OnPlayerDeath(CHL2MP_Player* pPlayer)
{
    if (!pPlayer)
        return;

    DebugMsg("OnPlayerDeath called for player %s\n", pPlayer->GetPlayerName());

    // Clear active weapon reference immediately to prevent crashes
    CBaseCombatWeapon* pActiveWeapon = pPlayer->GetActiveWeapon();
    if (pActiveWeapon)
    {
        pActiveWeapon->Holster();
        pPlayer->SetActiveWeapon(NULL);
    }
}

void CSpawnWeaponsManager::ApplyPlayerLoadout(CHL2MP_Player* pPlayer)
{
    if (!pPlayer || !m_pLoadoutGroupsKV)
    {
        DebugMsg("ApplyPlayerLoadout: Invalid player or no config loaded\n");
        return;
    }

    if (!sv_spawnweapons_enable.GetBool())
    {
        DebugMsg("ApplyPlayerLoadout: System disabled\n");
        return;
    }

    const char* pMapName = gpGlobals->mapname.ToCStr();
    DebugMsg("Applying loadout for player %s on map %s\n", pPlayer->GetPlayerName(), pMapName);

    // Find the appropriate loadout group for the current map
    KeyValues* pFinalWeaponsSection = nullptr;

    FOR_EACH_SUBKEY(m_pLoadoutGroupsKV, pLoadoutGroup)
    {
        const char* pGroupName = pLoadoutGroup->GetName();
        if (FStrEq(pGroupName, "__DEFAULT__"))
            continue;

        KeyValues* pMapsSection = pLoadoutGroup->FindKey("maps");
        if (pMapsSection && pMapsSection->FindKey(pMapName))
        {
            pFinalWeaponsSection = pLoadoutGroup->FindKey("weapons");
            DebugMsg("Found specific loadout group for map: %s\n", pGroupName);
            break;
        }
    }

    // Fall back to default if no specific map loadout found
    if (!pFinalWeaponsSection)
    {
        KeyValues* pDefaultGroup = m_pLoadoutGroupsKV->FindKey("__DEFAULT__");
        if (pDefaultGroup)
        {
            pFinalWeaponsSection = pDefaultGroup->FindKey("weapons");
            DebugMsg("Using default loadout group\n");
        }
    }

    if (!pFinalWeaponsSection)
    {
        DebugMsg("No valid weapons section found\n");
        return;
    }

    // Determine which loadout to use based on game mode and team
    KeyValues* pLoadoutKV = nullptr;

    if (HL2MPRules()->IsTeamplay())
    {
        const char* teamName = (pPlayer->GetTeamNumber() == TEAM_COMBINE) ? "COMBINE" : "REBELS";
        pLoadoutKV = pFinalWeaponsSection->FindKey(teamName);
        DebugMsg("Teamplay mode - looking for %s loadout\n", teamName);
    }
    else
    {
        pLoadoutKV = pFinalWeaponsSection->FindKey("DEATHMATCH");
        DebugMsg("Deathmatch mode - looking for DEATHMATCH loadout\n");
    }

    if (!pLoadoutKV)
    {
        DebugMsg("No loadout configuration found for current game mode\n");
        return;
    }

    // Remove existing weapons if configured to do so
    if (sv_spawnweapons_strip_on_spawn.GetBool())
    {
        SafeRemovePlayerWeapons(pPlayer);
    }

    // Ensure player has suit
    pPlayer->EquipSuit();

    CBaseCombatWeapon* pBestWeapon = nullptr;
    int bestSlot = 999;

    // Give weapons from loadout
    FOR_EACH_SUBKEY(pLoadoutKV, pWeaponKV)
    {
        const char* weaponName = pWeaponKV->GetName();
        int desiredAmmoPrimary = pWeaponKV->GetInt("ammo_primary", 0);
        int desiredAmmoSecondary = pWeaponKV->GetInt("ammo_secondary", 0);

        DebugMsg("Giving weapon: %s (primary ammo: %d, secondary ammo: %d)\n",
            weaponName, desiredAmmoPrimary, desiredAmmoSecondary);

        CBaseCombatWeapon* pWeapon = static_cast<CBaseCombatWeapon*>(pPlayer->GiveNamedItem(weaponName));

        if (pWeapon)
        {
            DebugMsg("Successfully gave weapon: %s\n", weaponName);

            // Handle special weapons that need custom ammo management
            if (FStrEq(weaponName, "weapon_slam"))
            {
                if (desiredAmmoPrimary > 0)
                {
                    int iAmmoIndex = GetAmmoDef()->Index("slam");
                    if (iAmmoIndex != -1)
                    {
                        pPlayer->SetAmmoCount(desiredAmmoPrimary, iAmmoIndex);
                        DebugMsg("Set SLAM ammo to %d\n", desiredAmmoPrimary);
                    }
                }
            }
            else if (FStrEq(weaponName, "weapon_rpg"))
            {
                if (desiredAmmoPrimary > 0)
                {
                    int iAmmoIndex = GetAmmoDef()->Index("RPG_Round");
                    if (iAmmoIndex != -1)
                    {
                        pPlayer->SetAmmoCount(desiredAmmoPrimary, iAmmoIndex);
                        DebugMsg("Set RPG ammo to %d\n", desiredAmmoPrimary);
                    }
                }
            }
            else
            {
                // Handle normal weapons
                int ammoIdx1 = pWeapon->GetPrimaryAmmoType();
                if (ammoIdx1 != -1 && desiredAmmoPrimary > 0)
                {
                    int currentAmmo = pPlayer->GetAmmoCount(ammoIdx1);
                    if (desiredAmmoPrimary > currentAmmo)
                    {
                        pPlayer->GiveAmmo(desiredAmmoPrimary - currentAmmo, ammoIdx1, true);
                        DebugMsg("Gave %d primary ammo for %s\n", desiredAmmoPrimary - currentAmmo, weaponName);
                    }
                }

                int ammoIdx2 = pWeapon->GetSecondaryAmmoType();
                if (ammoIdx2 != -1 && desiredAmmoSecondary > 0)
                {
                    int currentAmmo2 = pPlayer->GetAmmoCount(ammoIdx2);
                    if (desiredAmmoSecondary > currentAmmo2)
                    {
                        pPlayer->GiveAmmo(desiredAmmoSecondary - currentAmmo2, ammoIdx2, true);
                        DebugMsg("Gave %d secondary ammo for %s\n", desiredAmmoSecondary - currentAmmo2, weaponName);
                    }
                }
            }

            // Select the best weapon to equip (prefer lower slot numbers, avoid problematic weapons)
            int weaponSlot = pWeapon->GetSlot();
            if (weaponSlot > 0 && // Don't select crowbar as default
                !FStrEq(weaponName, "weapon_rpg") &&
                !FStrEq(weaponName, "weapon_slam") &&
                weaponSlot < bestSlot)
            {
                pBestWeapon = pWeapon;
                bestSlot = weaponSlot;
                DebugMsg("Selected %s as best weapon (slot %d)\n", weaponName, weaponSlot);
            }
        }
        else
        {
            Warning("[SpawnWeapons] Failed to give weapon: %s\n", weaponName);
        }
    }

    // Equip the best weapon found
    if (pBestWeapon)
    {
        pPlayer->Weapon_Equip(pBestWeapon);
        pPlayer->Weapon_Switch(pBestWeapon);
        DebugMsg("Equipped best weapon: %s\n", pBestWeapon->GetClassname());
    }
    else
    {
        DebugMsg("No suitable weapon found to equip\n");
    }

    DebugMsg("Loadout application completed for player %s\n", pPlayer->GetPlayerName());
}