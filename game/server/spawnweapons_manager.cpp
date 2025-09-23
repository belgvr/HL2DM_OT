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

// SafeRemovePlayerWeapons - versão robusta e segura
void CSpawnWeaponsManager::SafeRemovePlayerWeapons(CHL2MP_Player* pPlayer)
{
    if (!pPlayer)
        return;

    // Se o player já está marcado para deleção, não mexemos.
    if (pPlayer->IsMarkedForDeletion())
        return;

    // NÃO chamar Holster() em players mortos — isso costuma causar crash.
    // Vamos aceitar que a função pode ser chamada em qualquer momento:
    // - se o player está vivo, tratamos normalmente;
    // - se está morto, ainda precisamos limpar referências cliente-side (viewmodels) e agendar remoção.
    // Primeiro: força limpeza do viewmodel no cliente (se disponível)
    // (DestroyViewModels é seguro em muitas builds HL2MP; se não existir, ignore a chamada)
#ifdef _DEBUG
    // nada de debug
#endif
    // Se o método existir no seu CHL2MP_Player, chama; caso contrário só ignore.
    // Use um cast + checagem por ponteiro de função se sua versão precisar — aqui assumimos que existe.
    pPlayer->DestroyViewModels();

    // Garantir que o jogador não tem arma ativa apontada no momento
    CBaseCombatWeapon* pActiveWeapon = pPlayer->GetActiveWeapon();
    if (pActiveWeapon && !pActiveWeapon->IsMarkedForDeletion())
    {
        // primeiro, detach da lista do jogador para evitar que iterações subsequentes falhem
        pPlayer->Weapon_Detach(pActiveWeapon);

        // limpar owner/owner entity para evitar callbacks através do owner
        pActiveWeapon->SetOwner(nullptr);
        pActiveWeapon->SetOwnerEntity(nullptr);

        // remover referência do player para a arma imediatamente
        pPlayer->SetActiveWeapon(nullptr);

        // agendar remoção segura no próximo tick (evita use-after-free)
        pActiveWeapon->SetThink(&CBaseEntity::SUB_Remove);
        pActiveWeapon->SetNextThink(gpGlobals->curtime + 0.01f);
    }

    // Agora remover todas as outras armas de maneira segura.
    // Importante: usar while(WeaponCount() > 0) com GetWeapon(0) evita invalidar índices.
    while (pPlayer->WeaponCount() > 0)
    {
        CBaseCombatWeapon* pWeapon = pPlayer->GetWeapon(0);
        if (!pWeapon)
            break;

        // se a arma já está marcada para remoção, apenas detach e continue
        if (pWeapon->IsMarkedForDeletion())
        {
            pPlayer->Weapon_Detach(pWeapon);
            continue;
        }

        // Detach para remover da lista do jogador
        pPlayer->Weapon_Detach(pWeapon);

        // Limpa owner/owner entity
        pWeapon->SetOwner(nullptr);
        pWeapon->SetOwnerEntity(nullptr);

        // Agendar remoção segura no próximo frame
        pWeapon->SetThink(&CBaseEntity::SUB_Remove);
        pWeapon->SetNextThink(gpGlobals->curtime + 0.01f);

        // loop reinicia e pega o próximo item (sempre index 0)
    }

    // Limpar munição do jogador (evita referências pendentes)
    pPlayer->RemoveAllAmmo();

    // Assegura novamente que não há arma ativa
    pPlayer->SetActiveWeapon(nullptr);
}




void CSpawnWeaponsManager::OnPlayerSpawn(CHL2MP_Player* pPlayer)
{
    if (!pPlayer)
        return;

    DebugMsg("OnPlayerSpawn called for player %s\n", pPlayer->GetPlayerName());

    /* Small delay to ensure player is fully spawned
    pPlayer->SetThink(&CHL2MP_Player::Spawn);
    pPlayer->SetNextThink(gpGlobals->curtime + 0.1f);*/
}

void CSpawnWeaponsManager::OnPlayerDeath(CHL2MP_Player* pPlayer)
{
    if (!pPlayer)
        return;

    // Se houver arma ativa, desanexar/agendar remoção (forçado porque o jogador está morto)
    CBaseCombatWeapon* pActiveWeapon = pPlayer->GetActiveWeapon();
    if (pActiveWeapon)
    {
        pPlayer->Weapon_Detach(pActiveWeapon);
        pActiveWeapon->SetOwner(NULL);
        pActiveWeapon->SetOwnerEntity(NULL);
        pActiveWeapon->SetThink(&CBaseEntity::SUB_Remove);
        pActiveWeapon->SetNextThink(gpGlobals->curtime + 0.01f);

        pPlayer->SetActiveWeapon(NULL);
    }

    // Forçar remoção de todas as outras armas mesmo que o jogador esteja morto
    SafeRemovePlayerWeapons(pPlayer);

    // Garantir que o cliente limpe viewmodels: SetActiveWeapon(NULL) já ajuda.
    // Alguns builds têm DestroyViewModels(); se disponível, seria ideal chamá-la:
    // pPlayer->DestroyViewModels();  // opcional, dependendo da versão do SDK
}

// EM spawnweapons_manager.cpp

void CSpawnWeaponsManager::ApplyPlayerLoadout(CHL2MP_Player* pPlayer)
{
    // Usando Warning() para ter certeza de que a mensagem aparecerá no console em laranja.
    Warning("======= INICIANDO DEBUG DE APPLYPLAYERLOADOUT PARA %s =======\n", pPlayer ? pPlayer->GetPlayerName() : "NULL Player");

    if (!pPlayer || !m_pLoadoutGroupsKV)
    {
        Warning("DEBUG: ERRO - Jogador inválido ou arquivo de configuração não carregado. Abortando.\n");
        return;
    }

    if (!sv_spawnweapons_enable.GetBool())
    {
        Warning("DEBUG: Sistema desabilitado pela CVar. Abortando.\n");
        return;
    }

    Warning("DEBUG: Procurando loadout para o mapa %s...\n", gpGlobals->mapname.ToCStr());

    // ... (A lógica para encontrar a seção de armas correta permanece a mesma) ...
    KeyValues* pFinalWeaponsSection = nullptr;
    FOR_EACH_SUBKEY(m_pLoadoutGroupsKV, pLoadoutGroup)
    {
        KeyValues* pMapsSection = pLoadoutGroup->FindKey("maps");
        if (pMapsSection && pMapsSection->FindKey(gpGlobals->mapname.ToCStr()))
        {
            pFinalWeaponsSection = pLoadoutGroup->FindKey("weapons");
            Warning("DEBUG: Encontrado grupo de loadout específico para o mapa: %s\n", pLoadoutGroup->GetName());
            break;
        }
    }
    if (!pFinalWeaponsSection)
    {
        KeyValues* pDefaultGroup = m_pLoadoutGroupsKV->FindKey("__DEFAULT__");
        if (pDefaultGroup)
        {
            pFinalWeaponsSection = pDefaultGroup->FindKey("weapons");
            Warning("DEBUG: Usando grupo de loadout __DEFAULT__.\n");
        }
    }
    if (!pFinalWeaponsSection)
    {
        Warning("DEBUG: ERRO - Nenhuma seção 'weapons' válida foi encontrada. Abortando.\n");
        return;
    }

    // ... (A lógica para encontrar o loadout de DM ou Teamplay permanece a mesma) ...
    KeyValues* pLoadoutKV = nullptr;
    if (HL2MPRules()->IsTeamplay())
    {
        const char* teamName = (pPlayer->GetTeamNumber() == TEAM_COMBINE) ? "COMBINE" : "REBELS";
        pLoadoutKV = pFinalWeaponsSection->FindKey(teamName);
        Warning("DEBUG: Modo Teamplay, procurando por loadout '%s'.\n", teamName);
    }
    else
    {
        pLoadoutKV = pFinalWeaponsSection->FindKey("DEATHMATCH");
        Warning("DEBUG: Modo Deathmatch, procurando por loadout 'DEATHMATCH'.\n");
    }
    if (!pLoadoutKV)
    {
        Warning("DEBUG: ERRO - Nenhum loadout encontrado para o modo de jogo atual. Abortando.\n");
        return;
    }

    Warning("DEBUG: Loadout encontrado. Preparando para dar itens...\n");

    // Lembre-se, o bloco SafeRemovePlayerWeapons deve estar comentado!
    /*
    if (sv_spawnweapons_strip_on_spawn.GetBool())
    {
        SafeRemovePlayerWeapons(pPlayer);
    }
    */

    pPlayer->EquipSuit();

    CBaseCombatWeapon* pBestWeapon = nullptr;
    int bestSlot = 999;

    // Loop para dar as armas
    FOR_EACH_SUBKEY(pLoadoutKV, pWeaponKV)
    {
        const char* weaponName = pWeaponKV->GetName();
        Warning("  -> Loop: Processando arma '%s'\n", weaponName);

        CBaseCombatWeapon* pWeapon = static_cast<CBaseCombatWeapon*>(pPlayer->GiveNamedItem(weaponName));

        if (pWeapon)
        {
            Warning("    -> SUCESSO ao dar o item '%s'.\n", weaponName);

            // Lógica de munição
            int desiredAmmoPrimary = pWeaponKV->GetInt("ammo_primary", 0);
            if (desiredAmmoPrimary > 0)
            {
                int ammoIdx1 = pWeapon->GetPrimaryAmmoType();
                if (ammoIdx1 != -1)
                {
                    Warning("      -> Dando %d de munição primária.\n", desiredAmmoPrimary);
                    pPlayer->GiveAmmo(desiredAmmoPrimary, ammoIdx1, true);
                }
            }

            int desiredAmmoSecondary = pWeaponKV->GetInt("ammo_secondary", 0);
            if (desiredAmmoSecondary > 0)
            {
                int ammoIdx2 = pWeapon->GetSecondaryAmmoType();
                if (ammoIdx2 != -1)
                {
                    Warning("      -> Dando %d de munição secundária.\n", desiredAmmoSecondary);
                    pPlayer->GiveAmmo(desiredAmmoSecondary, ammoIdx2, true);
                }
            }

            // Lógica de melhor arma
            int weaponSlot = pWeapon->GetSlot();
            if (weaponSlot > 0 && weaponSlot < bestSlot)
            {
                pBestWeapon = pWeapon;
                bestSlot = weaponSlot;
            }
        }
        else
        {
            Warning("    -> FALHA ao dar o item '%s'.\n", weaponName);
        }
    }

    Warning("DEBUG: Fim do loop de armas.\n");

    if (pBestWeapon)
    {
        Warning("DEBUG: Tentando equipar a melhor arma: %s\n", pBestWeapon->GetClassname());
        pPlayer->Weapon_Equip(pBestWeapon);
        pPlayer->Weapon_Switch(pBestWeapon);
        Warning("DEBUG: Melhor arma equipada com sucesso.\n");
    }
    else
    {
        Warning("DEBUG: Nenhuma arma adequada encontrada para equipar.\n");
    }

    Warning("======= FIM DO DEBUG DE APPLYPLAYERLOADOUT =======\n");
}