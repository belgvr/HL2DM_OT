#include "cbase.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"
#include "hl2mp_player.h"
#include "hl2mp_savescores.h"

// --- Inicialização das variáveis estáticas da nossa classe ---
// O erro 'm_szWelcomeBackMsg is not a member' acontecia porque isso estava faltando no .h
bool CSaveScores::m_bShowLoadMessage = true;
char CSaveScores::m_szWelcomeBackMsg[256] = "{#ffffff}Your score has been {#ff8000}reloaded{white}.";


// --- ConVars para controle do sistema ---
ConVar sv_savescores_enabled("sv_savescores_enabled", "1", FCVAR_NOTIFY, "Enables the score saving system. 1=On, 0=Off");
ConVar sv_savescores_method("sv_savescores_method", "0", FCVAR_NOTIFY, "Score cleanup method. 0=On map change");
ConVar sv_savescores_reloaded_msg("sv_savescores_reloaded_msg", "1", FCVAR_NOTIFY, "Show a message when a player's score is restored. 1=On, 0=Off");


// Esta função é chamada uma vez por mapa pelo gamerules
void CSaveScores::Init()
{
	if (!filesystem->IsDirectory("cfg/savescores", "MOD"))
	{
		filesystem->CreateDirHierarchy("cfg/savescores", "MOD");
	}

	KeyValues* kv = new KeyValues("SaveScoresSettings");
	if (kv->LoadFromFile(filesystem, "cfg/savescores/savescores_settings.cfg", "MOD"))
	{
		// A CVar agora controla a exibição da mensagem, então não precisamos mais ler do .cfg
		Q_strncpy(m_szWelcomeBackMsg, kv->GetString("messages/score_reloaded", "{#ffffff}Your score has been {#0080ff}reloaded{#ffffff}."), sizeof(m_szWelcomeBackMsg));
		Msg("SaveScores: Custom settings loaded from savescores_settings.cfg\n");
	}
	else
	{
		Msg("SaveScores: savescores_settings.cfg not found. Using default settings.\n");
	}
	kv->deleteThis();
}

// Chamado pelo gamerules quando um jogador desconecta
void CSaveScores::SavePlayerScore(CBasePlayer* pPlayer)
{
	if (!sv_savescores_enabled.GetBool() || !pPlayer || pPlayer->IsFakeClient()) return;

	CSteamID steamID;
	if (!pPlayer->GetSteamID(&steamID)) return;
	uint64 steamID64 = steamID.ConvertToUint64();
	if (steamID64 == 0) return;

	char szFilePath[MAX_PATH];
	Q_snprintf(szFilePath, sizeof(szFilePath), "cfg/savescores/%llu.kv", steamID64);

	KeyValues* kv = new KeyValues("PlayerScore");
	kv->SetInt("kills", pPlayer->FragCount());
	kv->SetInt("deaths", pPlayer->DeathCount());
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
		kills = kv->GetInt("kills", 0);
		deaths = kv->GetInt("deaths", 0);

		if (sv_savescores_reloaded_msg.GetBool())
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
	filesystem->RemoveFile(szFilePath, "MOD");
	return bSuccess;
}

// Chamado pelo gamerules no final do mapa
void CSaveScores::CleanupScoreFiles()
{
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