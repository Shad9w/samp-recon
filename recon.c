/*
 * recon.asi - adds /recon [seconds] to SA-MP 0.3.7 R1.
 * Disconnects and rejoins the current server without closing the game.
 * No CRT, no SAMPFUNCS/CLEO dependency. Does nothing on other samp.dll versions.
 */
#include <windows.h>

/* samp.dll 0.3.7 R1 */
#define R1_ENTRYPOINT        0x31DF13
#define OFF_NETGAME          0x21A0F8   /* CNetGame**            */
#define OFF_CHAT             0x21A0E4   /* CChat**               */
#define OFF_INPUT            0x21A0E8   /* CInput**              */
#define FN_ADDCOMMAND        0x65AD0    /* CInput::AddCommand    */
#define FN_ADDCHATENTRY      0x64010    /* CChat::AddEntry       */
#define FN_RESTARTGAME       0xA060     /* CNetGame::ShutdownForRestart */
#define NG_GAMESTATE         0x3BD
#define NG_RAKCLIENT         0x3C9
#define GAMESTATE_WAIT_CONNECT 9

#define DEFAULT_DELAY_MS     3000
#define MAX_DELAY_SEC        120

typedef void (__cdecl *CmdProc)(const char *);
typedef void (__thiscall *AddCommand_t)(void *self, const char *name, CmdProc proc);
typedef void (__thiscall *AddEntry_t)(void *self, int type, const char *text,
                                      const char *prefix, DWORD color, DWORD prefixColor);
typedef void (__thiscall *RestartGame_t)(void *self);
typedef void (__thiscall *RakDisconnect_t)(void *self, unsigned int blockMs, unsigned char channel);

static BYTE *g_samp;                 /* samp.dll base */
static volatile LONG g_rejoinAt;     /* tick at which to rejoin, 0 = idle */

static void *NetGame(void) { return *(void **)(g_samp + OFF_NETGAME); }

static void Chat(const char *text)
{
    void *chat = *(void **)(g_samp + OFF_CHAT);
    if (chat)
        ((AddEntry_t)(g_samp + FN_ADDCHATENTRY))(chat, 8, text, NULL, 0xFF88AA62, 0);
}

/* Runs on the game thread when the player types /recon [seconds] */
static void __cdecl CmdRecon(const char *params)
{
    void *ng = NetGame();
    DWORD delay = DEFAULT_DELAY_MS, at;
    char msg[64];

    if (!ng)
        return;

    if (params) {
        DWORD n = 0;
        int digits = 0;
        while (*params == ' ') params++;
        while (*params >= '0' && *params <= '9' && digits < 4) {
            n = n * 10 + (DWORD)(*params++ - '0');
            digits++;
        }
        if (digits) {
            if (n > MAX_DELAY_SEC) n = MAX_DELAY_SEC;
            delay = n * 1000;
        }
    }

    /* Tear down the session, tell the server we left, then rejoin after the delay. */
    ((RestartGame_t)(g_samp + FN_RESTARTGAME))(ng);
    {
        void **rak = *(void ***)((BYTE *)ng + NG_RAKCLIENT);
        if (rak) {
            void **vtbl = *(void ***)rak;
            ((RakDisconnect_t)vtbl[2])(rak, 500, 0);
        }
    }

    wsprintfA(msg, "Reconnecting in %u second%s...", (unsigned)(delay / 1000),
              delay == 1000 ? "" : "s");
    Chat(msg);

    at = GetTickCount() + delay;
    if (at == 0) at = 1;
    InterlockedExchange(&g_rejoinAt, (LONG)at);
}

static DWORD WINAPI Worker(LPVOID unused)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    void *input;
    (void)unused;

    /* Wait for samp.dll; give up quietly if it never shows (single player). */
    for (int i = 0; !(g_samp = (BYTE *)GetModuleHandleA("samp.dll")); i++) {
        if (i > 600) return 0;
        Sleep(100);
    }

    dos = (IMAGE_DOS_HEADER *)g_samp;
    nt = (IMAGE_NT_HEADERS *)(g_samp + dos->e_lfanew);
    if (nt->OptionalHeader.AddressOfEntryPoint != R1_ENTRYPOINT)
        return 0;   /* not 0.3.7 R1: offsets would be wrong, stay inert */

    /* Wait for SA-MP to create its net game and chat input objects. */
    while (!NetGame() || !(input = *(void **)(g_samp + OFF_INPUT)))
        Sleep(100);
    Sleep(500);

    ((AddCommand_t)(g_samp + FN_ADDCOMMAND))(input, "recon", CmdRecon);

    for (;;) {
        LONG at = g_rejoinAt;
        if (at && (LONG)(GetTickCount() - (DWORD)at) >= 0) {
            void *ng = NetGame();
            InterlockedExchange(&g_rejoinAt, 0);
            if (ng)
                *(int *)((BYTE *)ng + NG_GAMESTATE) = GAMESTATE_WAIT_CONNECT;
        }
        Sleep(50);
    }
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE h;
        DisableThreadLibraryCalls(inst);
        h = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
