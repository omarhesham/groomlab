// SPDX-License-Identifier: Apache-2.0
#include "crash_log.h"

#ifdef _WIN32

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <string>
#include <typeinfo>

// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on

#include <maya/MGlobal.h>
#include <maya/MSceneMessage.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

#include "groomlab/version.h"

namespace groomlab::maya {

namespace {

using SignalHandler = void (*)(int);
SignalHandler gPreviousAbort = SIG_DFL;
std::terminate_handler gPreviousTerminate = nullptr;
_purecall_handler gPreviousPurecall = nullptr;
_invalid_parameter_handler gPreviousInvalid = nullptr;
LPTOP_LEVEL_EXCEPTION_FILTER gPreviousFilter = nullptr;
bool gInstalled = false;
bool gFilterInChain = false;   // onUnhandledException is in the process's filter chain
volatile LONG gEnabled = 0;    // it logs (else it only hands on: the plugin was unloaded, see removeCrashLog)
volatile LONG gWriting = 0;    // one report per crash
thread_local int tInFilter = 0;  // a filter chain that leads back here hands on instead of looping
// Read while Maya runs (nothing of Maya is asked once it is going down): Maya's version when the
// plugin loads, MtoA's when it is loaded (before or after GroomLab).
char gMayaVersion[96] = "Maya ?";
char gMtoaVersion[64] = "";
MCallbackId gPluginLoaded = 0;

void readMtoaVersion() {
    int loaded = 0;
    MString version;
    if (MGlobal::executeCommand("pluginInfo -query -loaded mtoa", loaded, false, false) && loaded &&
        MGlobal::executeCommand("pluginInfo -query -version mtoa", version, false, false)) {
        std::snprintf(gMtoaVersion, sizeof gMtoaVersion, "%s", version.asChar());
    }
}

void onPluginLoaded(const MStringArray& names, void*) {
    if (names.length() > 1 && names[1] == "mtoa") readMtoaVersion();
}

// The module holding `address`, its file path and its base; false when none does.
bool moduleOf(const void* address, HMODULE* module, char* path, DWORD size) {
    *module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCSTR>(address), module)) {
        std::snprintf(path, size, "?");
        return false;
    }
    if (GetModuleFileNameA(*module, path, size) == 0) std::snprintf(path, size, "?");
    return true;
}

const char* baseName(const char* path) {
    const char* slash = std::strrchr(path, '\\');
    return slash ? slash + 1 : path;
}

// GroomLab's own binaries (the plugin, the MtoA translator, the procedural): the only modules
// whose symbols the log looks up, from the .pdb beside each. Other modules' symbols are not
// looked for: that could reach a symbol server (_NT_SYMBOL_PATH) and hold Maya's own crash
// handler, which saves the scene, for minutes; their module and offset are enough.
bool ourModule(const char* path) {
    const char* name = baseName(path);
    return _strnicmp(name, "GroomLab", 8) == 0;  // GroomLab.mll, groomLabTranslator.dll, groomlab_procedural.dll
}

// A loaded module's file version ("7.4.2.1"), from its version resource; false when it is not loaded.
bool moduleVersion(const char* moduleName, char* text, std::size_t size) {
    HMODULE module = GetModuleHandleA(moduleName);
    if (!module) return false;
    char path[MAX_PATH] = {};
    if (GetModuleFileNameA(module, path, MAX_PATH) == 0) return false;
    static BYTE data[64 * 1024];  // no heap: the process is going down
    DWORD ignored = 0;
    const DWORD bytes = GetFileVersionInfoSizeA(path, &ignored);
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (bytes == 0 || bytes > sizeof data || !GetFileVersionInfoA(path, 0, sizeof data, data) ||
        !VerQueryValueA(data, "\\", reinterpret_cast<void**>(&info), &length) || !info) {
        std::snprintf(text, size, "loaded, version unknown");
        return true;
    }
    std::snprintf(text, size, "%u.%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                  HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
    return true;
}

// The log's first lines: what happened, the versions of GroomLab, Maya and (when loaded) MtoA
// and Arnold, and the thread.
FILE* openLog(const char* reason, const char* detail) {
    char temp[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, temp);
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    char path[MAX_PATH + 64] = {};
    std::snprintf(path, sizeof path, "%sGroomLabCrash_%04d%02d%02d_%02d%02d%02d.log", temp, local.tm_year + 1900,
                  local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
    FILE* out = nullptr;
    if (fopen_s(&out, path, "w") != 0 || !out) return nullptr;
    std::fprintf(out, "Maya is going down: %s\n", reason);
    if (detail && *detail) std::fprintf(out, "%s\n", detail);
    std::fprintf(out, "GroomLab %s (built for Maya API %d), running in %s\n", groomlab::kVersionString, MAYA_API_VERSION,
                 gMayaVersion);
    if (gMtoaVersion[0]) std::fprintf(out, "MtoA %s\n", gMtoaVersion);
    char version[64];
    if (moduleVersion("ai.dll", version, sizeof version)) std::fprintf(out, "Arnold (ai.dll) %s\n", version);
    std::fprintf(out, "thread %lu\n\n", GetCurrentThreadId());
    return out;
}

// Symbols for GroomLab's modules only (ourModule), loaded on first use from the folder of each.
struct Symbols {
    HANDLE process = GetCurrentProcess();
    bool ready = false;
    DWORD64 loaded[8] = {};
    int loadedCount = 0;

    void name(FILE* out, DWORD64 address, HMODULE module, const char* path) {
        if (!module || !ourModule(path)) return;
        if (!ready) {
            char dir[MAX_PATH] = {};
            std::snprintf(dir, sizeof dir, "%s", path);
            if (char* slash = std::strrchr(dir, '\\')) *slash = '\0';
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
            ready = SymInitialize(process, dir, FALSE) != FALSE;  // the search path: this folder, never a server
            if (!ready) return;
        }
        const auto base = reinterpret_cast<DWORD64>(module);
        bool known = false;
        for (int i = 0; i < loadedCount; ++i) known = known || loaded[i] == base;
        if (!known && loadedCount < 8) {
            SymLoadModuleEx(process, nullptr, path, nullptr, base, 0, nullptr, 0);
            loaded[loadedCount++] = base;
        }
        alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 511;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol)) std::fprintf(out, "  %s", symbol->Name);
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof line;
        DWORD column = 0;
        if (SymGetLineFromAddr64(process, address, &column, &line)) std::fprintf(out, "  (%s:%lu)", line.FileName, line.LineNumber);
    }
    ~Symbols() {
        if (ready) SymCleanup(process);
    }
};

void writeFrame(FILE* out, Symbols& symbols, unsigned index, DWORD64 address, bool returnAddress) {
    char module[MAX_PATH] = "?";
    HMODULE handle = nullptr;
    moduleOf(reinterpret_cast<const void*>(address), &handle, module, MAX_PATH);
    std::fprintf(out, "%2u  %s+0x%llx", index, baseName(module),
                 static_cast<unsigned long long>(address - reinterpret_cast<DWORD64>(handle)));
    // A return address points past its call: the call's line is the byte before.
    symbols.name(out, returnAddress ? address - 1 : address, handle, module);
    std::fprintf(out, "\n");
}

// This thread's stack from here (abort, terminate and the like: the failing frames are below).
void writeOwnStack(FILE* out) {
    void* frames[96] = {};
    const USHORT count = CaptureStackBackTrace(1, 96, frames, nullptr);
    Symbols symbols;
    for (USHORT i = 0; i < count; ++i) writeFrame(out, symbols, i, reinterpret_cast<DWORD64>(frames[i]), true);
}

// Writes the reason and this thread's stack to a new log in %TEMP%. Once.
void writeCrashLog(const char* reason, const std::string& detail = std::string()) {
    if (gEnabled == 0 || InterlockedExchange(&gWriting, 1) != 0) return;
    FILE* out = openLog(reason, detail.c_str());
    if (!out) return;
    writeOwnStack(out);
    std::fclose(out);
}

#if defined(_M_X64)
// The faulting thread's stack, unwound from the exception's context: frame 0 is the faulting
// instruction. Unwinding reads the stack, which may be the broken thing: the caller guards it.
void writeFaultStack(FILE* out, Symbols& symbols, const CONTEXT& fault) {
    CONTEXT context = fault;
    for (unsigned i = 0; i < 64 && context.Rip != 0; ++i) {
        writeFrame(out, symbols, i, context.Rip, i > 0);
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
        if (!function) {  // a leaf function: the return address is on top of the stack
            context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
            context.Rsp += 8;
        } else {
            void* handlerData = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData, &establisher, nullptr);
        }
    }
}

// writeFaultStack with an SEH frame of its own (no C++ object here): a stack too broken to walk
// ends the walk, not the log.
void guardedFaultStack(FILE* out, Symbols* symbols, const CONTEXT* fault) {
    __try {
        writeFaultStack(out, *symbols, *fault);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::fprintf(out, "(the stack could not be walked further)\n");
    }
}
#endif

void describeException(const EXCEPTION_RECORD* record, char* detail, std::size_t size) {
    std::snprintf(detail, size, "unknown exception");
    if (!record) return;
    const char* what = record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? "access violation"
                     : record->ExceptionCode == EXCEPTION_STACK_OVERFLOW ? "stack overflow"
                     : record->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION ? "illegal instruction"
                     : record->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO ? "integer division by zero"
                     : record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR ? "in-page error"
                     : "exception";
    std::snprintf(detail, size, "%s 0x%08lx at 0x%p", what, record->ExceptionCode, record->ExceptionAddress);
    if ((record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        record->NumberParameters >= 2) {
        const std::size_t used = std::strlen(detail);
        std::snprintf(detail + used, size - used, " (%s 0x%llx)",
                      record->ExceptionInformation[0] == 0   ? "reading"
                      : record->ExceptionInformation[0] == 1 ? "writing"
                                                             : "executing",
                      static_cast<unsigned long long>(record->ExceptionInformation[1]));
    }
    // Which module faulted, named on its own line: the stack's first frame, without reading it.
    char module[MAX_PATH] = "?";
    HMODULE handle = nullptr;
    const std::size_t used = std::strlen(detail);
    if (moduleOf(record->ExceptionAddress, &handle, module, MAX_PATH)) {
        std::snprintf(detail + used, size - used, "\nfaulting module: %s+0x%llx (%s)%s", baseName(module),
                      static_cast<unsigned long long>(reinterpret_cast<DWORD64>(record->ExceptionAddress) -
                                                      reinterpret_cast<DWORD64>(handle)),
                      module,
                      _stricmp(baseName(module), "ai.dll") == 0
                          ? "\n(the fault is inside Arnold: send the Arnold log of the render too, if there is one)"
                          : "");
    } else {
        std::snprintf(detail + used, size - used, "\nfaulting module: none (a jump to a bad address)");
    }
}

// The exception's log (guardedExceptionLog runs it under an SEH frame).
void writeExceptionLog(EXCEPTION_POINTERS* pointers) {
    if (InterlockedExchange(&gWriting, 1) != 0) return;
    char detail[1024];
    describeException(pointers ? pointers->ExceptionRecord : nullptr, detail, sizeof detail);
    FILE* out = openLog("unhandled exception", detail);
    if (!out) return;
#if defined(_M_X64)
    if (pointers && pointers->ContextRecord) {
        std::fprintf(out, "stack of the faulting thread, from the fault:\n");
        Symbols symbols;
        guardedFaultStack(out, &symbols, pointers->ContextRecord);
    }
#else
    writeOwnStack(out);
#endif
    std::fclose(out);
}

void guardedExceptionLog(EXCEPTION_POINTERS* pointers) {
    __try {
        writeExceptionLog(pointers);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Writing the log failed: Maya's handler still runs.
    }
}

void onAbort(int signal) {
    writeCrashLog("abort()");
    std::signal(SIGABRT, gPreviousAbort == SIG_ERR ? SIG_DFL : gPreviousAbort);
    std::raise(signal);
}

void onTerminate() {
    std::string detail;
    if (std::exception_ptr current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception& e) {
            detail = std::string("uncaught ") + typeid(e).name() + ": " + e.what();
        } catch (...) {
            detail = "uncaught exception of an unknown type";
        }
    }
    writeCrashLog("std::terminate", detail);
    if (gPreviousTerminate) gPreviousTerminate();
    std::abort();
}

void onPurecall() {
    writeCrashLog("pure virtual function call");
    if (gPreviousPurecall) gPreviousPurecall();
    std::abort();
}

void onInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    writeCrashLog("invalid parameter to a C runtime function");
    std::abort();
}

// A hardware fault nobody handled (an access violation, say): the log names it, the faulting
// module, the versions and the faulting thread's stack, then the filter before this one runs as
// it would have (Maya's, which saves the scene to %TEMP%, or Arnold's, which hands on to it).
// Nothing in writing the log can keep that from happening: a fault inside it is caught, and a
// filter chain that leads back here hands on.
LONG WINAPI onUnhandledException(EXCEPTION_POINTERS* pointers) {
    if (tInFilter != 0) return EXCEPTION_CONTINUE_SEARCH;
    tInFilter = 1;
    if (gEnabled != 0) guardedExceptionLog(pointers);
    const LONG result = gPreviousFilter ? gPreviousFilter(pointers) : EXCEPTION_CONTINUE_SEARCH;
    tInFilter = 0;
    return result;
}

}  // namespace

void installCrashLog() {
    if (gInstalled) return;
    gInstalled = true;
    MString version;
    if (MGlobal::executeCommand("about -installedVersion", version, false, false) && version.length() > 0) {
        std::snprintf(gMayaVersion, sizeof gMayaVersion, "%s", version.asChar());  // "Autodesk MAYA 2022.5"
    } else {
        std::snprintf(gMayaVersion, sizeof gMayaVersion, "Maya %s", MGlobal::mayaVersion().asChar());
    }
    readMtoaVersion();
    MStatus status;
    gPluginLoaded = MSceneMessage::addStringArrayCallback(MSceneMessage::kAfterPluginLoad, onPluginLoaded, nullptr, &status);
    if (!status) gPluginLoaded = 0;
    gPreviousAbort = std::signal(SIGABRT, onAbort);
    gPreviousTerminate = std::set_terminate(onTerminate);
    gPreviousPurecall = _set_purecall_handler(onPurecall);
    gPreviousInvalid = _set_invalid_parameter_handler(onInvalidParameter);
    // Loaded again after an unload that had to leave the filter in the chain (removeCrashLog):
    // it is still there, so it is only switched back on.
    if (!gFilterInChain) {
        gPreviousFilter = SetUnhandledExceptionFilter(onUnhandledException);
        gFilterInChain = true;
    }
    InterlockedExchange(&gEnabled, 1);
}

void removeCrashLog() {
    if (!gInstalled) return;
    gInstalled = false;
    InterlockedExchange(&gEnabled, 0);
    if (gPluginLoaded != 0) MMessage::removeCallback(gPluginLoaded);
    gPluginLoaded = 0;
    std::signal(SIGABRT, gPreviousAbort == SIG_ERR ? SIG_DFL : gPreviousAbort);
    std::set_terminate(gPreviousTerminate);
    _set_purecall_handler(gPreviousPurecall);
    _set_invalid_parameter_handler(gPreviousInvalid);
    // The filter comes out of the chain only when it is still the top one. A filter set after it
    // (Arnold sets one when MtoA starts it) calls it when its turn comes: then it stays, only
    // handing on, and the plugin's code stays loaded (pinned) so that call never lands in
    // freed memory.
    const LPTOP_LEVEL_EXCEPTION_FILTER top = SetUnhandledExceptionFilter(gPreviousFilter);
    if (top == onUnhandledException) {
        gFilterInChain = false;
        return;
    }
    SetUnhandledExceptionFilter(top);
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                       reinterpret_cast<LPCSTR>(&onUnhandledException), &self);
}

}  // namespace groomlab::maya

#else

namespace groomlab::maya {
void installCrashLog() {}
void removeCrashLog() {}
}  // namespace groomlab::maya

#endif
