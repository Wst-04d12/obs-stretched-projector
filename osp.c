#include <windows.h>
#include <stdio.h>

#include <iso646.h>
#include <dbghelp.h>

#pragma comment(lib, "Dbghelp.lib")
extern SIZE_T(*PatternScan(LPBYTE pData, SIZE_T dataSize, LPCSTR pattern, _Out_ PSIZE_T pMatchesCount, _Out_ PSIZE_T pPatternLength))[];


static void MsgErr(const char* prefix) {
    char buf[256];
    wsprintfA(buf, "%s (%lu)", prefix, GetLastError());
    MessageBoxA(NULL, buf, "osp", MB_OK);
}

static void* LocateFunction(const char* funcName, const char* const module) {

    void* ret = NULL;
    const static HANDLE sSymSession = 0x1124112411241124;

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);

    if (not SymInitialize(sSymSession, NULL, FALSE)) {
        MsgErr("SymInitialize failed");
        return ret; // no need for clean up
    }

    HMODULE hModule = GetModuleHandleA(module);

    if (hModule == NULL) {
        MsgErr("GetModuleHandle failed");
        goto clean_up;
    }

    char path[MAX_PATH];
    GetModuleFileNameA(hModule, path, MAX_PATH);

    DWORD64 base = SymLoadModuleEx(sSymSession, NULL, path, NULL, hModule, NULL, NULL, NULL);

    if (base == NULL) {
        MsgErr("SymLoadModuleEx failed");
        goto clean_up;
    }


    char buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {0};
    PSYMBOL_INFO sym = (PSYMBOL_INFO)buf;

    sym -> SizeOfStruct = sizeof(SYMBOL_INFO);
    sym -> MaxNameLen = MAX_SYM_NAME;

    if (not SymFromName(sSymSession, funcName, sym)) {
        MsgErr("SymFromName failed");
        goto clean_up;
    }

    ret = sym -> Address;

    clean_up:
    SymCleanup(sSymSession);

    return ret;

}


static unsigned char op[64];

static unsigned char bBackedUp = FALSE;

static ptrdiff_t pBase = 'Myon';

const static ptrdiff_t wSize = 0x23;

__declspec(dllexport) void projector_patch_enable() {

    if (bBackedUp) goto w;

    for (unsigned char offset = 0; offset < wSize; ++offset) {
        op[offset] = *(unsigned char*)(pBase + offset);
    }

    bBackedUp = TRUE;

    w:

    DWORD old;
    VirtualProtect(pBase, wSize, PAGE_EXECUTE_READWRITE, &old);

    for (unsigned char offset = 0; offset < wSize; ++offset) {
        ((char*)pBase)[offset] = 0x90;
    }

    //31 C9 31 D2 45 89 E8 45 89 E1
    *(UINT64*)pBase = 0x45e88945d231c931;
    *(WORD*)(pBase + 8) = 0xe189;


    FlushInstructionCache(GetCurrentProcess(), pBase, wSize);

    VirtualProtect(pBase, wSize, old, &old);

}

__declspec(dllexport) void projector_patch_disable() {

    DWORD old;
    VirtualProtect(pBase, wSize, PAGE_EXECUTE_READWRITE, &old);

    for (unsigned char offset = 0; offset < wSize; ++offset) {
        ((char*)pBase)[offset] = op[offset];
    }

    FlushInstructionCache(GetCurrentProcess(), pBase, wSize);

    VirtualProtect(pBase, wSize, old, &old);

}

static unsigned char lop[64];
__declspec(dllexport) uintptr_t init_(void) {
    /*return pBase = (uintptr_t)LocateFunction("OBSProjector::OBSRender", NULL) + 0x19B, pBase;*/
    uintptr_t gs_set_viewport = LocateFunction("gs_set_viewport", "obs.dll");
    uintptr_t p = LocateFunction("OBSProjector::OBSRender", NULL);

    for (ptrdiff_t offset = 0; offset < 0x400; offset = offset + 1) {
        uintptr_t ip = p + offset;
        WORD inst = *(WORD*)ip;
        if (inst != 0x15FF) {
            continue;
        }
        else {
            INT32 disp = *(INT32*)(ip + 2);
            if (*(uintptr_t*)(disp + ip + 6) == gs_set_viewport) {
                pBase = ip;
                break;
            }
        }
        
    }

    return pBase;
    //return pBase = (uintptr_t)LocateFunction("gs_set_viewport", "obs.dll"), pBase;
}

__declspec(dllexport) uintptr_t init(void) {
    return pBase = (uintptr_t)LocateFunction("OBSProjector::OBSRender", NULL) + 0x19B, pBase;
}
