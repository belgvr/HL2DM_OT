#ifndef HL2MP_SAVESCORES_H
#define HL2MP_SAVESCORES_H
#ifdef _WIN32
#pragma once
#endif
#include "tier1/strtools.h" // Necessário para as funções de string (Q_strncpy, etc.)
// ===================================================================================
// A FUNÇÃO DE COR AGORA VIVE AQUI DENTRO DO HEADER, MARCADA COMO 'inline'
// Isso resolve TODOS os erros de "já definido" e "não encontrado" de uma vez por todas.
// ===================================================================================
inline void UTIL_FormatColors(const char* pMessage, char* pFormatted, int maxlen)
{
	char pTempMessage[512];
	Q_strncpy(pTempMessage, pMessage, sizeof(pTempMessage));
	pFormatted[0] = '\0';
	char* pNext = pTempMessage;
	while (pNext && *pNext)
	{
		char* pStart = strstr(pNext, "{#");
		if (!pStart)
		{
			Q_strncat(pFormatted, pNext, maxlen, COPY_ALL_CHARACTERS);
			break;
		}
		*pStart = '\0';
		Q_strncat(pFormatted, pNext, maxlen, COPY_ALL_CHARACTERS);
		*pStart = '{';
		pNext = pStart;
		char* pEnd = strstr(pNext, "}");
		if (!pEnd || (pEnd - pStart) != 8) // Formato {#RRGGBB} tem 9 chars
		{
			Q_strncat(pFormatted, pNext, maxlen, COPY_ALL_CHARACTERS);
			break;
		}
		char szColor[7];
		Q_strncpy(szColor, pStart + 2, sizeof(szColor));
		Q_strncat(pFormatted, "\x07", maxlen, COPY_ALL_CHARACTERS);
		Q_strncat(pFormatted, szColor, maxlen, COPY_ALL_CHARACTERS);
		pNext = pEnd + 1;
		if (Q_strnicmp(pNext, "{white}", 7) == 0)
		{
			Q_strncat(pFormatted, "\x01", maxlen, COPY_ALL_CHARACTERS);
			pNext += 7;
		}
	}
}
class CBasePlayer; // Forward declaration
class CSaveScores
{
public:
	static void Init();
	static void SavePlayerScore(CBasePlayer* pPlayer);
	static bool LoadPlayerScoreData(CBasePlayer* pPlayer, int& kills, int& deaths);
	static void CleanupScoreFiles();
	static void CleanupExpiredScoreFiles(); // Nova função para método 1
	static void SaveAllPlayersBeforeMapChange(); // NOVA: Salva todos antes do mapa mudar
	static const char* GetResetMessage(); // NOVA: Retorna mensagem de reset
private:
	static bool m_bShowLoadMessage;
	static char m_szWelcomeBackMsg[256];
	static bool m_bMapChanging; // NOVA: Flag para proteger contra save pós-mudança
};
#endif // HL2MP_SAVESCORES_H