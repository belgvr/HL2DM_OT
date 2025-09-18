#ifndef HL2MP_SERVERADMIN_H
#define HL2MP_SERVERADMIN_H
#include "cbase.h"
#include <utlstring.h>
#include <utldict.h>

// Definição da estrutura para os grupos de admin
struct AdminGroup_t
{
    CUtlString name;
    CUtlString flags;
    CUtlString tag;        // "[A]", "[MOD]", etc.
    CUtlString tagColor;   // Color for the tag
    CUtlString nameColor;  // Color for admin's name
    CUtlString textColor;  // Color for admin's message text
    CUtlString rebelTeamColor;   // Color for (Rebel) team prefix
    CUtlString combineTeamColor; // Color for (Combine) team prefix
    bool isDefault;        // True if this is the default group for non-admins
};

struct AdminChatFormat
{
    CUtlString tag;
    const char* tagColor;
    const char* nameColor;
    const char* textColor;
    const char* rebelTeamColor;    
    const char* combineTeamColor;  
};

// Variável global para armazenar os grupos, visível para todos os arquivos
extern CUtlDict<AdminGroup_t, unsigned short> g_AdminGroups;

// admin permission flags
#define ADMIN_UNDEFINED      'a'
#define ADMIN_GENERIC        'b' 
#define ADMIN_KICK           'c' 
#define ADMIN_BAN            'd' 
#define ADMIN_UNBAN          'e' 
#define ADMIN_SLAY           'f' 
#define ADMIN_CHANGEMAP      'g' 
#define ADMIN_CVAR           'h' 
#define ADMIN_CONFIG         'i' 
#define ADMIN_CHAT           'j' 
#define ADMIN_VOTE           'k' 
#define ADMIN_PASSWORD       'l' 
#define ADMIN_RCON           'm' 
#define ADMIN_CHEATS         'n' 
#define ADMIN_WHO            'w' // Nova flag para o comando 'sa who'
#define ADMIN_ROOT           'z' 

class CHL2MP_Admin
{
public:
    // Construtor aceita SteamID e nome do grupo
    CHL2MP_Admin(const char* steamID, const char* groupName);
    ~CHL2MP_Admin();

    // Função para verificar permissões
    bool HasPermission(char flag) const;
    const char* GetSteamID() const { return m_steamID; }
    const char* GetGroupName() const { return m_groupName; }

    static void InitAdminSystem();
    static bool IsPlayerAdmin(CBasePlayer* pPlayer, const char* requiredFlags);
    static void ClearAllAdmins();
    static CHL2MP_Admin* GetAdmin(const char* steamID);
    static void AddAdmin(const char* steamID, const char* groupName);
    bool FindSpecialTargetGroup(const char* targetSpecifier);
    static void CheckChatText(char* p, int bufsize);
    static void LogAction(CBasePlayer* pAdmin, CBasePlayer* pTarget, const char* action, const char* details = "", const char* groupTarget = nullptr);


    // Funções para verificar grupos especiais
    bool IsAllPlayers() const { return bAll; }
    bool IsAllBluePlayers() const { return bBlue; }
    bool IsAllRedPlayers() const { return bRed; }
    bool IsAllButMePlayers() const { return bAllButMe; }
    bool IsMe() const { return bMe; }
    bool IsAllAlivePlayers() const { return bAlive; }
    bool IsAllDeadPlayers() const { return bDead; }
    bool IsAllBotsPlayers() const { return bBots; }
    bool IsAllHumanPlayers() const { return bHumans; }

private:
    const char* m_steamID;
    const char* m_groupName;

    // Flags para grupos especiais
    bool bAll;
    bool bBlue;
    bool bRed;
    bool bAllButMe;
    bool bMe;
    bool bAlive;
    bool bDead;
    bool bBots;
    bool bHumans;
};

// --- DECLARAÇÃO DE FUNÇÕES NOVAS ---
static void WhoPlayerCommand(const CCommand& args); // Adicionado aqui para resolver o erro

// Function declarations outside the class
AdminChatFormat GetPlayerChatFormat(CBasePlayer* pPlayer);
const char* GetColorConstant(const char* colorName);
void LoadCustomColors(); 

CUtlString ProcessChatColors(const char* message, const char* defaultTextColor);

extern CUtlVector<CHL2MP_Admin*> g_AdminList;
extern CHL2MP_Admin* g_pHL2MPAdmin;
extern CUtlDict<CUtlString, unsigned short> g_CustomColors;

inline CHL2MP_Admin* HL2MPAdmin()
{
    return static_cast<CHL2MP_Admin*>(g_pHL2MPAdmin);
}

#endif // HL2MP_SERVERADMIN_H