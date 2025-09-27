#include "cbase.h"
#include "Chatsounds/hl2mp_chatsounds.h"
#include "gamerules.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "player.h"

// =================================================================================================
// Implementação do Construtor de Cópia (necessário para o CUtlMap)
// =================================================================================================
SoundData_t::SoundData_t(const SoundData_t& src)
{
    for (int i = 0; i < src.m_SoundFiles.Count(); i++)
    {
        int len = Q_strlen(src.m_SoundFiles[i]) + 1;
        char* newPath = new char[len];
        Q_strncpy(newPath, src.m_SoundFiles[i], len);
        m_SoundFiles.AddToTail(newPath);
    }
}

// =================================================================================================
// Variável Global
// =================================================================================================
CHL2MPChatSounds* g_pChatSounds = NULL;


// =================================================================================================
// Implementação da Classe Principal
// =================================================================================================
CHL2MPChatSounds::CHL2MPChatSounds()
{
    Msg("Sistema de ChatSounds INICIADO!\n");
}

CHL2MPChatSounds::~CHL2MPChatSounds() {}

void CHL2MPChatSounds::LevelInit()
{
    ClearSoundData();
    ParseSoundsFile();
}

void CHL2MPChatSounds::LevelShutdown()
{
    ClearSoundData();
}

void CHL2MPChatSounds::ClearSoundData()
{
    FOR_EACH_MAP_FAST(m_Sounds, i)
    {
        m_Sounds[i].m_SoundFiles.PurgeAndDeleteElements();
    }
    m_Sounds.Purge();
}

void CHL2MPChatSounds::ParseSoundsFile()
{
    KeyValues* kv = new KeyValues("ChatSounds");

    // Usando o caminho que você pediu para eu lembrar
    if (!kv->LoadFromFile(filesystem, "cfg/chatsounds/chatsounds.cfg", "MOD"))
    {
        Warning("[ChatSounds] FALHA! Nao achei o arquivo cfg/chatsounds/chatsounds.cfg!\n");
        kv->deleteThis();
        return;
    }

    Msg("[ChatSounds] Arquivo chatsounds.cfg carregado! Processando sons...\n");

    FOR_EACH_SUBKEY(kv, pSoundKey)
    {
        const char* command = pSoundKey->GetName();
        KeyValues* pFiles = pSoundKey->FindKey("files");
        if (pFiles)
        {
            SoundData_t soundData;
            FOR_EACH_SUBKEY(pFiles, pSoundPath)
            {
                if (FStrEq(pSoundPath->GetName(), "sound"))
                {
                    const char* soundPath = pSoundPath->GetString();
                    int len = Q_strlen(soundPath) + 1;
                    char* newPath = new char[len];
                    Q_strncpy(newPath, soundPath, len);
                    soundData.m_SoundFiles.AddToTail(newPath);
                }
            }

            if (soundData.m_SoundFiles.Count() > 0)
            {
                char* newCommand = new char[Q_strlen(command) + 1];
                Q_strcpy(newCommand, command);
                m_Sounds.Insert(newCommand, soundData);
                Msg("  -> Comando '%s' carregado com %d som(ns).\n", command, soundData.m_SoundFiles.Count());
            }
        }
    }
    kv->deleteThis();
}

void CHL2MPChatSounds::PrecacheSounds()
{
    FOR_EACH_MAP_FAST(m_Sounds, i)
    {
        SoundData_t& soundData = m_Sounds[i];
        for (int j = 0; j < soundData.m_SoundFiles.Count(); j++)
        {
            CBaseEntity::PrecacheSound(soundData.m_SoundFiles[j]);
        }
    }
}

bool CHL2MPChatSounds::OnPlayerSay(CBasePlayer* pPlayer, const char* pText)
{
    int soundIndex = m_Sounds.Find(pText);
    if (soundIndex == m_Sounds.InvalidIndex())
    {
        return true;
    }

    SoundData_t& soundData = m_Sounds.Element(soundIndex);
    if (soundData.m_SoundFiles.Count() == 0)
    {
        return true;
    }

    const char* soundToPlay = soundData.m_SoundFiles[0];
    CRecipientFilter filter;
    filter.AddAllPlayers();
    CBaseEntity::EmitSound(filter, pPlayer->entindex(), soundToPlay);

    return true;
}