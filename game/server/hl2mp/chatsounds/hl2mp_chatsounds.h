#ifndef HL2MP_CHATSOUNDS_H
#define HL2MP_CHATSOUNDS_H
#pragma once

#include "utlmap.h"
#include "utlvector.h"

struct SoundData_t
{
    // ... (Mantenha o construtor padrão e o de cópia que já fizemos) ...
    SoundData_t() {}
    SoundData_t(const SoundData_t& src); // Só a declaração, a definição está no .cpp

    CUtlVector<char*> m_SoundFiles;
};

class CHL2MPChatSounds
{
public:
    CHL2MPChatSounds();
    ~CHL2MPChatSounds();

    void LevelInit();
    void LevelShutdown();
    void PrecacheSounds();
    bool OnPlayerSay(CBasePlayer* pPlayer, const char* pText);

private:
    void ParseSoundsFile();
    void ClearSoundData();

    // A ÚNICA fonte de dados. Não precisamos mais da lista de precache separada.
    CUtlMap<const char*, SoundData_t> m_Sounds;
};

extern CHL2MPChatSounds* g_pChatSounds;

#endif // HL2MP_CHATSOUNDS_H