// SPDX-License-Identifier: Apache-2.0
// groomlab-helper: GroomLab's core in a process of its own, for hosts whose plug-ins may not load it
// (Blender: its add-ons are GPL, GroomLab is not; docs/BLENDER.md). The host spawns it, writes one
// JSON request per line to its stdin and reads one JSON response per line from its stdout; big
// arrays travel in shared memory. It ends on "shutdown", at the end of stdin (the host closed the
// pipe or died), or when the process named by --parent-pid ends.
//
// stdout carries the protocol alone: anything else the process prints (a library's stray output)
// goes to stderr, which the host may read as a log. No window and no dialog ever opens: the host
// spawns it without a console (CREATE_NO_WINDOW), and a crash or an abort exits silently with a
// code the host sees (it can start another helper).

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "groomlab/graph.h"
#include "groomlab/version.h"

#if GROOMLAB_HAS_ALEMBIC
#include "groomlab/alembic_operators.h"
#endif

#include "helper.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

namespace {

using nlohmann::json;
namespace fs = std::filesystem;

FILE* g_protocol = nullptr;  // the real stdout

void send(const json& response) {
    // Invalid UTF-8 (a path the OS gave in another code page) is replaced, never a broken line.
    const std::string line = response.dump(-1, ' ', false, json::error_handler_t::replace) + "\n";
    std::fwrite(line.data(), 1, line.size(), g_protocol);
    std::fflush(g_protocol);
}

// stdout becomes the protocol's alone; fd 1 (printf, std::cout) goes to stderr.
bool takeStdout() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    const int protocol = _dup(_fileno(stdout));
    if (protocol < 0) return false;
    _setmode(protocol, _O_BINARY);
    std::fflush(stdout);
    if (_fileno(stderr) >= 0 && _dup2(_fileno(stderr), _fileno(stdout)) == 0) {
        SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE));
    } else {
        std::FILE* null = nullptr;
        freopen_s(&null, "NUL", "w", stdout);
    }
    g_protocol = _fdopen(protocol, "wb");
#else
    signal(SIGPIPE, SIG_IGN);  // a host gone: writes fail, the loop ends at stdin's end
    const int protocol = dup(STDOUT_FILENO);
    if (protocol < 0) return false;
    std::fflush(stdout);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    g_protocol = fdopen(protocol, "wb");
#endif
    return g_protocol != nullptr;
}

// No dialog for a crash, an abort or a missing DLL: the process ends and the host sees it.
void quietFailures() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    std::set_terminate([] {
        std::fputs("groomlab-helper: terminated\n", stderr);
        std::_Exit(70);
    });
}

std::string executableDir(const char* argv0) {
#ifdef _WIN32
    (void)argv0;
    std::wstring path(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(n);
    return fs::path(path).parent_path().u8string();
#else
    std::error_code ec;
    const fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return self.parent_path().u8string();
    return fs::absolute(fs::u8path(argv0 ? argv0 : ""), ec).parent_path().u8string();
#endif
}

// Ends the helper when the host's process does, should stdin stay open (a handle inherited by
// another child of the host).
void watchParent(long pid) {
#ifdef _WIN32
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!parent) return;
    std::thread([parent] {
        WaitForSingleObject(parent, INFINITE);
        std::_Exit(0);
    }).detach();
#else
    std::thread([pid] {
        while (getppid() == static_cast<pid_t>(pid)) std::this_thread::sleep_for(std::chrono::seconds(1));
        std::_Exit(0);
    }).detach();
#endif
}

}  // namespace

int main(int argc, char** argv) {
    quietFailures();
    long parentPid = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--version") {
            std::printf("groomlab-helper %s (protocol %d)\n", groomlab::kVersionString, groomlab::helper::kProtocolVersion);
            return 0;
        }
        if (arg == "--parent-pid" && i + 1 < argc) {
            parentPid = std::strtol(argv[++i], nullptr, 10);
        } else {
            std::fprintf(stderr, "usage: groomlab-helper [--version] [--parent-pid <pid>]\n"
                                 "Speaks GroomLab's helper protocol on stdin/stdout (docs/BLENDER.md).\n");
            return 2;
        }
    }
    if (!takeStdout()) {
        std::fputs("groomlab-helper: cannot take stdout\n", stderr);
        return 1;
    }
    if (parentPid > 0) watchParent(parentPid);

#if GROOMLAB_HAS_ALEMBIC
    groomlab::registerAlembicOperators(groomlab::OperatorRegistry::instance());
#endif
    groomlab::helper::setExecutableDir(executableDir(argc > 0 ? argv[0] : nullptr));

    groomlab::helper::Helper helper;
    std::string line;
    while (!helper.done() && std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.find_first_not_of(" \t") == std::string::npos) continue;
        json request = json::parse(line, nullptr, false);
        if (request.is_discarded()) {
            send(groomlab::helper::Helper::malformed("not a JSON line"));
            continue;
        }
        send(helper.handle(request));
    }
    std::fflush(g_protocol);
    return 0;
}
