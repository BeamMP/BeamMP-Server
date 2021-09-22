//
// Created by Anonymous275 on 9/9/2021.
//

#include <windows.h>
#include <imagehlp.h>
#include <strsafe.h>
#include <cstdint>
#include "stack_string.h"

struct function_info {
    void* func_address;
    uint32_t thread_id;
    bool enter;
};

fst::stack_string<1024> crash_file;

template <typename I>
fst::stack_string<(sizeof(I)<<1)+1> HexString(I w) {
    static const char* digits = "0123456789ABCDEF";
    const size_t hex_len = sizeof(I)<<1;
    fst::stack_string<hex_len+1> rc;
    rc.resize(hex_len+1);
    memset(rc.get(), '0', hex_len);
    memset(rc.get() + hex_len, 0, 1);
    for (size_t i=0, j=(hex_len-1)*4 ; i<hex_len; ++i,j-=4)
        rc[i] = digits[(w>>j) & 0x0f];
    return rc;
}

template<class T_>
class heap_array {
public:
    heap_array() noexcept {
        Data = (T_*)(GlobalAlloc(GPTR, Cap * sizeof(T_)));
        init = true;
    }
    explicit heap_array(size_t Cap_) noexcept {
        Cap = Cap_;
        Data = (T_*)(GlobalAlloc(GPTR, Cap * sizeof(T_)));
        init = true;
    }
    ~heap_array() {
        free(Data);
    }
    inline T_* get() noexcept {
        return Data;
    }
    inline const T_* cget() noexcept {
        return Data;
    }
    inline void insert(const T_& T) {
        if(!init)return;
        if(Size >= Cap) {
            Grow();
        }
        Data[Size++] = T;
    }
    inline void string_insert(const T_* T, size_t len = 0) {
        if(len == 0)len = strlen(T);
        if(Size+len >= Cap) {
            Grow(len);
        }
        memcpy(&Data[Size], T, len);
        Size += len;
    }
    inline T_ at(size_t idx) {
        return Data[idx];
    }
    inline size_t size() const noexcept {
        return Size;
    }
    const T_& operator[](size_t idx) {
        if (idx >= Size) {
            throw std::exception("out of boundaries operator[]");
        }
        return Data[idx];
    }
private:
    inline void Grow(size_t add = 0) {
        Cap = (Cap*2) + add;
        auto* NewData = (T_*)(GlobalAlloc(GPTR, Cap * sizeof(T_)));
        for(size_t C = 0; C < Size; C++) {
            NewData[C] = Data[C];
        }
        GlobalFree(Data);
        Data = NewData;
    }
    size_t Size{0}, Cap{5};
    bool init{false};
    T_* Data;
};

heap_array<function_info>* watch_data;

struct watchdog_mutex {
    static void Create() noexcept {
        hMutex = CreateMutex(nullptr, FALSE, nullptr);
    }
    static void Lock() {
        WaitForSingleObject(hMutex, INFINITE);
    }
    static void Unlock() {
        ReleaseMutex(hMutex);
    }
    struct [[nodiscard]] ScopedLock {
        ScopedLock() {
            if(hMutex)
            watchdog_mutex::Lock();
        }
        ~ScopedLock() {
            if(hMutex)
            watchdog_mutex::Unlock();
        }
    };
private:
    static HANDLE hMutex;
};
HANDLE watchdog_mutex::hMutex{nullptr};
std::atomic<bool> Init{false}, Sym;
std::atomic<int64_t> Offset{0};

void watchdog_setOffset(int64_t Off) {
    Offset.store(Off);
}

void notify(const char* msg) {
    HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdOut != nullptr && stdOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteConsoleA(stdOut, "[WATCHDOG] ", 11, &written, nullptr);
        WriteConsoleA(stdOut, msg, DWORD(strlen(msg)), &written, nullptr);
        WriteConsoleA(stdOut, "\n", 1, &written, nullptr);
    }
}

fst::stack_string<MAX_SYM_NAME> FindFunction(void* Address) {
    if(!Sym.load()) {
        fst::stack_string<MAX_SYM_NAME> undName;
        return undName;
    }
    static HANDLE process = GetCurrentProcess();
    DWORD64 symDisplacement = 0;
    fst::stack_string<MAX_SYM_NAME> undName;
    TCHAR buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    memset(&buffer,0, sizeof(buffer));
    auto pSymbolInfo = (PSYMBOL_INFO)buffer;
    pSymbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbolInfo->MaxNameLen	= MAX_SYM_NAME;
    if (SymFromAddr(process, DWORD64(Address) + Offset, &symDisplacement, pSymbolInfo)) {
        undName.push_back(pSymbolInfo->Name);
    }
    return undName;
}


fst::stack_string<512> getCrashInfo(void* Address){
    if(!Sym.load()){
        fst::stack_string<512> Value;
        Value.push_back("unknown", 7);
        return Value;
    }
    DWORD pdwDisplacement = 0;
    IMAGEHLP_LINE64 line{sizeof(IMAGEHLP_LINE64)};
    SymGetLineFromAddr64(GetCurrentProcess(), DWORD64(Address) + Offset, &pdwDisplacement, &line);
    char* Name = nullptr;
    if(line.FileName) {
        Name = strrchr(line.FileName, '\\');
    }
    fst::stack_string<512> Value;
    if(Name)Value.push_back(Name+1);
    else Value.push_back("unknown", 7);
    char buffer[20];
    auto n = sprintf(buffer, ":%lu", line.LineNumber);
    Value.push_back(buffer, n);
    return Value;
}
const char* getFunctionDetails(void* Address) {
    return FindFunction(Address).c_str();
}
const char* getCrashLocation(void* Address) {
    return getCrashInfo(Address).c_str();
}

void InitSym(const char* PDBLocation) {
    SymInitialize(GetCurrentProcess(), PDBLocation, TRUE);
    Sym.store(true);
}

void write_report(const char* report, size_t size) {
    HANDLE hFile = CreateFile(crash_file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        notify("Failed to open crash file for writing!");
        return;
    }
    DWORD dwBytesWritten = 0;
    auto Flag = WriteFile(hFile, report, DWORD(size), &dwBytesWritten, nullptr);
    if (Flag == FALSE) {
        notify("Failed to write to crash file!");
    }
    CloseHandle(hFile);
}

void generate_crash_report(uint32_t Code, void* Address) {
    watchdog_mutex::ScopedLock guard;
    notify("generating crash report, please wait");
    Init.store(false);
    heap_array<char> Report(watch_data->size() * sizeof(function_info));
    Report.string_insert("crash code ");
    Report.string_insert(HexString(Code).c_str());
    Report.string_insert(" at ");
    Report.string_insert(HexString(size_t(Address) + Offset).c_str());
    Report.string_insert("\n");
    if(Address) {
        Report.string_insert("origin and line number -> ");
        Report.string_insert(getCrashInfo(Address).c_str());
        Report.string_insert("\n");
    }
    Report.string_insert("Call history: \n");
    char buff[20];
    for(size_t C = 0; C < watch_data->size(); C++){
        auto entry = watch_data->at(C);
        auto Name = FindFunction(entry.func_address);
        if(entry.enter){
            Report.string_insert("[Entry] ");
        }
        else {
            Report.string_insert("[Exit ] ");
        }
        auto n = sprintf(buff, "(%d) ", entry.thread_id);
        Report.string_insert(buff, n);
        if(Name.size() > 0){
            Report.string_insert(Name.c_str(), Name.size());
            Report.string_insert("  |  ");
            auto location = getCrashInfo(entry.func_address);
            Report.string_insert(location.c_str(), location.size());
        }
        else {
            Report.string_insert(HexString(size_t(entry.func_address) + Offset).c_str());
        }
        Report.string_insert("\n");
    }
    write_report(Report.cget(), Report.size());
    notify("crash report generated");
    Init.store(true);
}

LONG WINAPI CrashHandler(EXCEPTION_POINTERS* p) {
    Init.store(false);
    notify("CAUGHT EXCEPTION!");
    generate_crash_report(p->ExceptionRecord->ExceptionCode, p->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

void watchdog_init(const char* crashFile, const char* SpecificPDBLocation, bool Symbols) {
    if(Symbols)SymInitialize(GetCurrentProcess(), SpecificPDBLocation, TRUE);
    Sym.store(Symbols);
    SetUnhandledExceptionFilter(CrashHandler);
    watch_data = new heap_array<function_info>();
    watchdog_mutex::Create();
    crash_file.push_back(crashFile);
    notify("initialized!");
    Init.store(true);
}

inline void AddEntry(void* func_address, uint32_t thread_id, bool entry) {
    watchdog_mutex::ScopedLock guard;
    if(Init.load()) {
        watch_data->insert({func_address, thread_id, entry});
    }
}

extern "C" {
    void FuncEntry(void* func) {
        AddEntry(func, GetCurrentThreadId(), true);
    }
    void FuncExit(void* func) {
        AddEntry(func, GetCurrentThreadId(), false);
    }
}