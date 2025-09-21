#include "cbase.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"
#include "hl2mp_player.h"
#include "hl2mp_savescores.h"

// --- Inicialização das variáveis estáticas da nossa classe ---
bool CSaveScores::m_bShowLoadMessage = true;
char CSaveScores::m_szWelcomeBackMsg[256] = "{#ffffff}Your score has been {#ff8000}reloaded{#ffffff}.";
bool CSaveScores::m_bMapChanging = false;


// --- ConVars para controle do sistema ---
ConVar sv_savescores_enabled("sv_savescores_enabled", "1", FCVAR_NOTIFY, "Enables the score saving system. 1=On, 0=Off");
ConVar sv_savescores_method("sv_savescores_method", "0", FCVAR_NOTIFY, "Score cleanup method. 0=On map change, 1=By timeout");
ConVar sv_savescores_timeout("sv_savescores_timeout", "10", FCVAR_NOTIFY, "Minutes before score files expire (only for method 1). Default: 10");
ConVar sv_savescores_reloaded_msg("sv_savescores_reloaded_msg", "1", FCVAR_NOTIFY, "Show a message when a player's score is restored. 1=On, 0=Off");


// Esta função é chamada uma vez por mapa pelo gamerules
void CSaveScores::Init()
{
	// IMPORTANTE: Resetar a flag de mudança de mapa
	m_bMapChanging = false;

	if (!filesystem->IsDirectory("cfg/savescores", "MOD"))
	{
		filesystem->CreateDirHierarchy("cfg/savescores", "MOD");
	}

	KeyValues* kv = new KeyValues("SaveScoresSettings");
	if (kv->LoadFromFile(filesystem, "cfg/savescores/savescores_settings.cfg", "MOD"))
	{
		Q_strncpy(m_szWelcomeBackMsg, kv->GetString("messages/score_reloaded", "{#ffffff}Your score has been {#0080ff}reloaded{#ffffff}."), sizeof(m_szWelcomeBackMsg));
		Msg("SaveScores: Custom settings loaded from savescores_settings.cfg\n");
	}
	else
	{
		Msg("SaveScores: savescores_settings.cfg not found. Using default settings.\n");
	}
	kv->deleteThis();

	// Se o método for por tempo, limpa arquivos expirados na inicialização
	if (sv_savescores_method.GetInt() == 1)
	{
		CleanupExpiredScoreFiles();
	}
}

// Chamado pelo gamerules quando um jogador desconecta
void CSaveScores::SavePlayerScore(CBasePlayer* pPlayer)
{
	// PROTEÇÃO CRÍTICA: Não salva se o mapa está mudando!
	if (!sv_savescores_enabled.GetBool() || !pPlayer || pPlayer->IsFakeClient() || m_bMapChanging)
	{
		if (m_bMapChanging && pPlayer && !pPlayer->IsFakeClient())
		{
			Msg("SaveScores: Blocked save for player %s - map is changing\n", pPlayer->GetPlayerName());
		}
		return;
	}

	CSteamID steamID;
	if (!pPlayer->GetSteamID(&steamID)) return;
	uint64 steamID64 = steamID.ConvertToUint64();
	if (steamID64 == 0) return;

	char szFilePath[MAX_PATH];
	Q_snprintf(szFilePath, sizeof(szFilePath), "cfg/savescores/%llu.kv", steamID64);

	KeyValues* kv = new KeyValues("PlayerScore");
	kv->SetInt("kills", pPlayer->FragCount());
	kv->SetInt("deaths", pPlayer->DeathCount());

	// Se método for por tempo, salva o timestamp atual
	if (sv_savescores_method.GetInt() == 1)
	{
		kv->SetInt("timestamp", (int)gpGlobals->curtime);
	}

	kv->SaveToFile(filesystem, szFilePath, "MOD");
	kv->deleteThis();

	Msg("SaveScores: Saved score for player %s (%llu)\n", pPlayer->GetPlayerName(), steamID64);
}

// Chamado pelo gamerules quando um jogador spawna
bool CSaveScores::LoadPlayerScoreData(CBasePlayer* pPlayer, int& kills, int& deaths)
{
	if (!sv_savescores_enabled.GetBool() || !pPlayer || pPlayer->IsFakeClient()) return false;

	CSteamID steamID;
	if (!pPlayer->GetSteamID(&steamID)) return false;
	uint64 steamID64 = steamID.ConvertToUint64();
	if (steamID64 == 0) return false;

	char szFilePath[MAX_PATH];
	Q_snprintf(szFilePath, sizeof(szFilePath), "cfg/savescores/%llu.kv", steamID64);

	if (!filesystem->FileExists(szFilePath, "MOD")) return false;

	bool bSuccess = false;
	KeyValues* kv = new KeyValues("PlayerScore");
	if (kv->LoadFromFile(filesystem, szFilePath, "MOD"))
	{
		// Se método for por tempo, verifica se não expirou
		if (sv_savescores_method.GetInt() == 1)
		{
			int savedTime = kv->GetInt("timestamp", 0);
			int timeoutMinutes = sv_savescores_timeout.GetInt();
			int currentTime = (int)gpGlobals->curtime;

			// Se expirou, deleta o arquivo e retorna false
			if (savedTime > 0 && (currentTime - savedTime) > (timeoutMinutes * 60))
			{
				filesystem->RemoveFile(szFilePath, "MOD");
				kv->deleteThis();
				Msg("SaveScores: Score file expired for player %llu\n", steamID64);
				return false;
			}
		}

		kills = kv->GetInt("kills", 0);
		deaths = kv->GetInt("deaths", 0);

		// SÓ MOSTRA A MENSAGEM SE O SCORE NÃO FOR 0/0
		if ((kills > 0 || deaths > 0) && sv_savescores_reloaded_msg.GetBool())
		{
			char szUnformattedMessage[512];
			Q_snprintf(szUnformattedMessage, sizeof(szUnformattedMessage), m_szWelcomeBackMsg, pPlayer->GetPlayerName(), kills, deaths);

			char szFormattedMessage[512];
			UTIL_FormatColors(szUnformattedMessage, szFormattedMessage, sizeof(szFormattedMessage));

			ClientPrint(pPlayer, HUD_PRINTTALK, szFormattedMessage);
		}
		bSuccess = true;
	}
	kv->deleteThis();

	// Remove o arquivo depois de carregar (método 0) ou deixa para expirar (método 1)
	if (sv_savescores_method.GetInt() == 0)
	{
		filesystem->RemoveFile(szFilePath, "MOD");
	}

	return bSuccess;
}

// Chamado pelo gamerules no final do mapa (método 0)
void CSaveScores::CleanupScoreFiles()
{
	// IMPORTANTE: Marcar que o mapa está mudando
	m_bMapChanging = true;

	if (sv_savescores_method.GetInt() != 0) return;

	FileFindHandle_t findHandle;
	const char* pFilename = filesystem->FindFirst("cfg/savescores/*.kv", &findHandle);
	while (pFilename)
	{
		char szFilePath[MAX_PATH];
		Q_snprintf(szFilePath, sizeof(szFilePath), "cfg/savescores/%s", pFilename);
		filesystem->RemoveFile(szFilePath, "MOD");
		pFilename = filesystem->FindNext(findHandle);
	}
	filesystem->FindClose(findHandle);
	Msg("SaveScores: All score files have been cleaned up.\n");
}

// Nova função para limpar arquivos expirados (método 1)
void CSaveScores::CleanupExpiredScoreFiles()
{
	if (sv_savescores_method.GetInt() != 1) return;

	int timeoutMinutes = sv_savescores_timeout.GetInt();
	int currentTime = (int)gpGlobals->curtime;
	int filesRemoved = 0;

	FileFindHandle_t findHandle;
	const char* pFilename = filesystem->FindFirst("cfg/savescores/*.kv", &findHandle);
	while (pFilename)
	{
		char szFilePath[MAX_PATH];
		Q_snprintf(szFilePath, sizeof(szFilePath), "cfg/savescores/%s", pFilename);

		KeyValues* kv = new KeyValues("PlayerScore");
		if (kv->LoadFromFile(filesystem, szFilePath, "MOD"))
		{
			int savedTime = kv->GetInt("timestamp", 0);
			if (savedTime > 0 && (currentTime - savedTime) > (timeoutMinutes * 60))
			{
				filesystem->RemoveFile(szFilePath, "MOD");
				filesRemoved++;
			}
		}
		kv->deleteThis();

		pFilename = filesystem->FindNext(findHandle);
	}
	filesystem->FindClose(findHandle);

	if (filesRemoved > 0)
	{
		Msg("SaveScores: %d expired score files have been cleaned up.\n", filesRemoved);
	}
}

// NOVA FUNÇÃO: Salva todos os players conectados antes do mapa mudar
void CSaveScores::SaveAllPlayersBeforeMapChange()
{
	if (!sv_savescores_enabled.GetBool()) return;

	Msg("SaveScores: Saving all players before map change...\n");

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer && !pPlayer->IsFakeClient() && pPlayer->IsConnected())
		{
			// Salva sem verificar a flag m_bMapChanging (chamada antes dela ser ativada)
			CSteamID steamID;
			if (!pPlayer->GetSteamID(&steamID)) continue;
			uint64 steamID64 = steamID.ConvertToUint64();
			if (steamID64 == 0) continue;

			char szFilePath[MAX_PATH];
			Q_snprintf(szFilePath, sizeof(szFilePath), "cfg/savescores/%llu.kv", steamID64);

			KeyValues* kv = new KeyValues("PlayerScore");
			kv->SetInt("kills", pPlayer->FragCount());
			kv->SetInt("deaths", pPlayer->DeathCount());

			if (sv_savescores_method.GetInt() == 1)
			{
				kv->SetInt("timestamp", (int)gpGlobals->curtime);
			}

			kv->SaveToFile(filesystem, szFilePath, "MOD");
			kv->deleteThis();

			Msg("SaveScores: Pre-saved score for player %s (%llu): %d/%d\n",
				pPlayer->GetPlayerName(), steamID64, pPlayer->FragCount(), pPlayer->DeathCount());
		}
	}
}

// NOVA FUNÇÃO: Retorna a mensagem de reset
const char* CSaveScores::GetResetMessage()
{
	return "{#ffffff}Your score has been {#ff0000}reset{#ffffff}.";
}