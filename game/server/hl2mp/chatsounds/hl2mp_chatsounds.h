#ifndef HL2MP_CHATSOUNDS_H
#define HL2MP_CHATSOUNDS_H
#pragma once

#include "utlmap.h"
#include "utlvector.h"

struct SoundData_t
{
    // ... (Mantenha o construtor padr�o e o de c�pia que j� fizemos) ...
    SoundData_t() {}
    SoundData_t(const SoundData_t& src); // S� a declara��o, a defini��o est� no .cpp

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

    // A �NICA fonte de dados. N�o precisamos mais da lista de precache separada.
    CUtlMap<const char*, SoundData_t> m_Sounds;
};

extern CHL2MPChatSounds* g_pChatSounds;

#endif // HL2MP_CHATSOUNDS_H