#include <windows.h>
#include <stdio.h>

#include <iso646.h>
#include <dbghelp.h>

#pragma comment(lib, "Dbghelp.lib")

static void MsgErr(const char* prefix) {
    char buf[256];
    wsprintfA(buf, "%s (%lu)", prefix, GetLastError());
    MessageBoxA(NULL, buf, "osp", MB_OK);
}

static void* LocateFunction(const char* funcName, const char* module) {

    void* ret = NULL;
    const static HANDLE sSymSession = 0x1124112411241124;

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);

    if (not SymInitialize(sSymSession, NULL, FALSE)) {
        MsgErr("SymInitialize failed");
        return ret; //no need for clean up
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

static ptrdiff_t pBase = 'Myon';

static uintptr_t pJ =    'Myon';

static unsigned char b_disp[4] = {'M', 'y', 'o', 'n'};

__declspec(dllexport) extern void projector_patch_enable() {

    DWORD old;

    //overwrite instruction at patch point from `call qword ptr [&gs_set_viewport]` to `jmp qword ptr [jumper]`

    VirtualProtect(pBase, 6, PAGE_EXECUTE_READWRITE, &old);

    *(unsigned char*)(pBase + 1) = 0x25; //call -> jmp

    *(int*)(pBase + 2) = (uintptr_t)pJ - (pBase + 6); //[&gs_set_viewport] -> [jumper]

    FlushInstructionCache(GetCurrentProcess(), pBase, 6);

    VirtualProtect(pBase, 6, old, &old);

}

__declspec(dllexport) extern void projector_patch_disable() {

    DWORD old;

    VirtualProtect(pBase, 6, PAGE_EXECUTE_READWRITE, &old);

    *(unsigned char*)(pBase + 1) = 0x15;

    *(int*)(pBase + 2) = *(int*)b_disp;

    FlushInstructionCache(GetCurrentProcess(), pBase, 6);

    VirtualProtect(pBase, 6, old, &old);

}

static unsigned char bOnlyFullscreenProjector = FALSE;
static unsigned char mem[16]; // qword ptr[2] = {&gs_set_viewport, pBase + 6}
static unsigned char op[0x40] = {
/*0x00*/0x8A, 0x05, 0x00, 0x00, 0x00, 0x00, // mov al, byte ptr [&bOnlyFullscreenProjector]
/*0x06*/0xA8, 0x01,                         // test al, 1
/*0x08*/0x74, 0x0E,                         // je +0E               ;force reg manipulate
/*0x0A*/0x49, 0x8b, 0x47, 0x20,             // mov rax, qword ptr [r15 + 20]
/*0x0E*/0x8B, 0x40, 0x10,                   // mov eax, dword ptr [rax + 10]
/*0x11*/0xC1, 0xE8, 0x02,                   // shr eax, 2
/*0x14*/0xA8, 0x01,                         // test al, 1
/*0x16*/0x74, 0x0A,                         // je +0A               ;direct to `call`, skip reg manipulate
/*0x18*/0x31, 0xC9,                         // xor ecx, ecx
/*0x1A*/0x31, 0xD2,                         // xor edx, edx
/*0x1C*/0x45, 0x89, 0xE8,                   // mov r8d, r13d
/*0x1F*/0x45, 0x89, 0xE1,                   // mov r9d, r12d
/*0x22*/0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, // call qword ptr [<&gs_set_viewport>]
/*0x28*/0xFF, 0x25, 0x00, 0x00, 0x00, 0x00  // jmp qword ptr [&(pBase+6)]
};

__declspec(dllexport) extern uintptr_t init(void) {

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

    if (pBase == 'Myon') {
        MsgErr("Incompatible OBS version.");
        return NULL;
    }

    //construct jumper

    intptr_t* const pj = (uintptr_t)GetModuleHandleA(NULL) + 0x4E;

    DWORD old;
    VirtualProtect(pj, 8, PAGE_READWRITE, &old);

    *pj = op;

    VirtualProtect(pj, 8, old, &old);

    //setup assemblies

    *(uintptr_t*)mem = gs_set_viewport;
    
    *(INT32*)(op + 12 + 24) = mem - (op + 16 + 24);

    *((uintptr_t*)mem + 1) = pBase + 6;

    *(INT32*)(op + 18 + 24) = mem + 8 - (op + 22 + 24);

    *(INT32*)(op + 2) = &bOnlyFullscreenProjector - (op + 6);

    VirtualProtect(op, sizeof op, PAGE_EXECUTE_READWRITE, &old);

    //backup the original disp to [&gs_set_viewport]

    *(int*)b_disp = *(int*)(pBase + 2);

    return pJ = pj, pBase;

}

__declspec(dllexport) extern void enable_only_fs_projector() {
    bOnlyFullscreenProjector = TRUE;
}

__declspec(dllexport) extern void disable_only_fs_projector() {
    bOnlyFullscreenProjector = FALSE;
}