#include <windows.h>
#include <stdio.h>

#include <iso646.h>
#include <dbghelp.h>

#pragma comment(lib, "Dbghelp.lib")

#pragma section(".text")

__declspec(noinline) static void MsgErr(const char* prefix) {
    char buf[256];
    wsprintfA(buf, "%s (%lu)", prefix, GetLastError());
    MessageBoxA(NULL, buf, "OBS Stretched Projector Error Message", MB_OK);
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

__declspec(allocate(".text")) static unsigned char op[] = {
        0x54, 0x55, 0x56, 0x57, 0x50, 0x51, 0x52, 0x53,             // push rsp bp si di ax cx dx bx 8 .. 15
        0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53,
        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x89, 0xE1,                                           // mov rcx, rsp
        0x48, 0x83, 0xEC, 0x20,                                     // sub rsp, 20h
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, &SetupRegThisPtrObsProjector
        0xFF, 0xD0,                                                 // call rax
        0x48, 0x83, 0xC4, 0x20,                                     // add rsp, 20h
        0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C,             // pop ...
        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58,
        0x5B, 0x5A, 0x59, 0x58, 0x5F, 0x5E, 0x5D,
        0x48, 0x83, 0xC4, 0x08,          /*  <- 0x4A(74)   */       // add rsp, 8
/*0x00*/0x8A, 0x05, 0x00, 0x00, 0x00, 0x00,                         // mov al, byte ptr [&bOnlyFullscreenProjector]
/*0x06*/0x3C, 0x00,                                                 // cmp al, 0
/*0x08*/0x74, 0x11,                                                 // je +11               ;force reg manipulate
/*0x0A*/0x4C, 0x89, 0xD8,                                           // mov rax, r11
/*0x0D*/0x48, 0x8B, 0x40, 0x20,                                     // mov rax, qword ptr [rax + 20]
/*0x11*/0x8B, 0x40, 0x10,                                           // mov eax, dword ptr [rax + 10]
/*0x14*/0xC1, 0xE8, 0x02,                                           // shr eax, 2
/*0x17*/0x3C, 0x01,                                                 // cmp al, 1
/*0x19*/0x75, 0x0A,                                                 // jne +0A              ;direct to `call`, skip reg manipulate
/*0x1B*/0x31, 0xC9,                                                 // xor ecx, ecx
/*0x1D*/0x31, 0xD2,                                                 // xor edx, edx
/*0x1F*/0x45, 0x89, 0xE8,                                           // mov r8d, r13d
/*0x22*/0x45, 0x89, 0xE1,                                           // mov r9d, r12d
/*0x25*/0xFF, 0x15, 0x00, 0x00, 0x00, 0x00,                         // call qword ptr [<&gs_set_viewport>]
/*0x2B*/0xFF, 0x25, 0x00, 0x00, 0x00, 0x00                          // jmp qword ptr [&(pBase+6)]
};

#define OP op+74


static CRITICAL_SECTION g_cs;
static unsigned char cRegPrepareFailedState = 255;
struct Reg { unsigned __int64 r15, r14, r13, r12, r11, r10, r9, r8, rbx, rdx, rcx, rax, rdi, rsi, rbp, rsp; };
struct mov_inst { unsigned char i0, i1, i2; };
static const struct mov_inst mov_insts[16] = { // mov rax, <reg>
    0x49, 0x8B, 0xC7, 0x49, 0x8B, 0xC6, 0x49, 0x8B, 0xC5, 0x49, 0x8B, 0xC4,
    0x49, 0x8B, 0xC3, 0x49, 0x8B, 0xC2, 0x49, 0x8B, 0xC1, 0x49, 0x8B, 0xC0,
    0x48, 0x8B, 0xC3, 0x48, 0x8B, 0xC2, 0x48, 0x8B, 0xC1, 0x48, 0x8B, 0xC0,
    0x48, 0x8B, 0xC7, 0x48, 0x8B, 0xC6, 0x48, 0x8B, 0xC5, 0x48, 0x8B, 0xC4
};
struct COL {
    UINT32 signature;
    UINT32 offset;
    UINT32 cdOffset;
    INT32  pTypeDescriptor;
    INT32  pClassHierarchy;
    INT32  pSelf;
};

static void SetupRegThisPtrObsProjector(const struct Reg* reg) {

    static unsigned char thisRegister = 0xFF;

    EnterCriticalSection(&g_cs);

    if (cRegPrepareFailedState == TRUE or thisRegister < 16) {
        goto ret;
    }

    DWORD old_protect;

    unsigned __int64* regs = (unsigned __int64*)reg;

    for (unsigned char i = 0; i < 16; i = i + 1) {

        __try {

            unsigned __int64 obj = regs[i];
            unsigned __int64 vptr = *(unsigned __int64*)obj;
            struct COL* col = *(struct COL**)(vptr - 8);

            if (col->signature != 1) {
                continue;
            }

            char* image = (char*)col - col->pSelf;
            char* type = image + col->pTypeDescriptor + 16;

            if (strstr(type, "OBSProjector")) {

                thisRegister = i;
                *(struct mov_inst*)(OP + 0x0A) = mov_insts[i];
                FlushInstructionCache(GetCurrentProcess(), OP + 0x0A, 3);

                VirtualProtect(pJ, 8, PAGE_READWRITE, &old_protect);
                *(uintptr_t*)pJ = OP;
                VirtualProtect(pJ, 8, old_protect, &old_protect);

                cRegPrepareFailedState = FALSE;

                goto finalize;

            }

        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

    }

    cRegPrepareFailedState = TRUE;
    (OP)[0x08] = '\xEB'; // je -> jmp

    finalize: VirtualProtect(op, sizeof op, PAGE_EXECUTE_READ, &old_protect);
    ret: return LeaveCriticalSection(&g_cs);

}

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

    VirtualProtect(op, sizeof op, PAGE_EXECUTE_READWRITE, &old);

    *(uintptr_t*)(op + 33) = &SetupRegThisPtrObsProjector;

    *(INT32*)(OP + 2) = &bOnlyFullscreenProjector - (OP + 6);

    *(uintptr_t*)mem = gs_set_viewport;
    
    *(INT32*)(OP + 0x25 + 2) = mem - (OP + 0x25 + 6);

    *((uintptr_t*)mem + 1) = pBase + 6;

    *(INT32*)(OP + 0x2B + 2) = mem + 8 - (OP + 0x2B + 6);

#undef OP

    //backup the original disp to [&gs_set_viewport]

    *(int*)b_disp = *(int*)(pBase + 2);

    InitializeCriticalSection(&g_cs);

    return pJ = pj, pBase;

}

__declspec(dllexport) extern void enable_only_fs_projector() {
    bOnlyFullscreenProjector = TRUE;
}

__declspec(dllexport) extern void disable_only_fs_projector() {
    bOnlyFullscreenProjector = FALSE;
}

__declspec(dllexport) extern void uninit(void) {
    DeleteCriticalSection(&g_cs);
}

__declspec(dllexport) extern unsigned int get_reg_setup_status() {
    return cRegPrepareFailedState;
}

__declspec(dllexport) extern void lua_msgerr(const char* msg) {
    MsgErr(msg);
}