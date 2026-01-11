#include "core/logging/CrashHandler.hpp"
#include "core/logging/Logger.hpp"
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <time.h>
#include <vector>
#include <string>
#include <filesystem>

#ifdef _MSC_VER
#pragma comment(lib, "dbghelp.lib")
#endif

namespace NoMoreDay {

    const char* GetExceptionName(DWORD code) {
        switch(code) {
            case EXCEPTION_ACCESS_VIOLATION: return "Access Violation (非法内存访问)";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Array Bounds Exceeded (数组越界)";
            case EXCEPTION_BREAKPOINT: return "Breakpoint (断点)";
            case EXCEPTION_DATATYPE_MISALIGNMENT: return "Datatype Misalignment (数据未对齐)";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "Float Divide By Zero (浮点除零)";
            case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Integer Divide By Zero (整数除零)";
            case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal Instruction (非法指令)";
            case EXCEPTION_PRIV_INSTRUCTION: return "Privileged Instruction (特权指令)";
            case EXCEPTION_STACK_OVERFLOW: return "Stack Overflow (栈溢出)";
            default: return "Unknown Exception (未知异常)";
        }
    }

    // 生成增强型 Minidump
    void WriteEnhancedMiniDump(EXCEPTION_POINTERS* pExceptionInfo) {
        if (!std::filesystem::exists("crashes")) {
            std::filesystem::create_directory("crashes");
        }

        char timeBuf[64];
        time_t now = time(NULL);
        struct tm ltm;
        localtime_s(&ltm, &now);
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d_%H-%M-%S", &ltm);
        
        std::string dumpPath = std::string("crashes/crash_") + timeBuf + ".dmp";
        
        HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        
        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mdei;
            mdei.ThreadId = GetCurrentThreadId();
            mdei.ExceptionPointers = pExceptionInfo;
            mdei.ClientPointers = FALSE;

            // 使用更高级别的 Dump 类型
            // MiniDumpWithIndirectlyReferencedMemory: 非常有用于分析指针崩溃，它会记录指针指向的内存块
            // MiniDumpWithDataSegs: 包含全局变量段
            MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(
                MiniDumpNormal | 
                MiniDumpWithIndirectlyReferencedMemory | 
                MiniDumpWithDataSegs | 
                MiniDumpWithHandleData |
                MiniDumpWithUnloadedModules |
                MiniDumpWithProcessThreadData
            );

            // 如果你需要极其详细的信息（文件会很大，数百MB），可以使用 MiniDumpWithFullMemory
            // dumpType = MiniDumpWithFullMemory;

            BOOL success = MiniDumpWriteDump(
                GetCurrentProcess(), 
                GetCurrentProcessId(), 
                hFile, 
                dumpType, 
                &mdei, 
                NULL, 
                NULL
            );

            if (success) {
                fprintf(stderr, "[CrashHandler] 增强型 Minidump 已写入: %s\n", dumpPath.c_str());
            } else {
                fprintf(stderr, "[CrashHandler] 写入 Minidump 失败, 错误码: %lu\n", GetLastError());
            }
            CloseHandle(hFile);
        }
    }

    void PrintStackSafe(const CONTEXT* originalCtx) {
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        // 必须复制一份 Context，因为 StackWalk64 会修改它
        CONTEXT ctx = *originalCtx;

        // 初始化符号处理器，尝试设置当前目录为符号路径
        SymInitialize(process, NULL, TRUE);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);

        STACKFRAME64 stack;
        memset(&stack, 0, sizeof(stack));
        
        DWORD machineType;
#ifdef _M_X64
        machineType = IMAGE_FILE_MACHINE_AMD64;
        stack.AddrPC.Offset = ctx.Rip;
        stack.AddrPC.Mode = AddrModeFlat;
        stack.AddrFrame.Offset = ctx.Rbp;
        stack.AddrFrame.Mode = AddrModeFlat;
        stack.AddrStack.Offset = ctx.Rsp;
        stack.AddrStack.Mode = AddrModeFlat;
#else
        machineType = IMAGE_FILE_MACHINE_I386;
        stack.AddrPC.Offset = ctx.Eip;
        stack.AddrPC.Mode = AddrModeFlat;
        stack.AddrFrame.Offset = ctx.Ebp;
        stack.AddrFrame.Mode = AddrModeFlat;
        stack.AddrStack.Offset = ctx.Esp;
        stack.AddrStack.Mode = AddrModeFlat;
#endif

        fprintf(stderr, "\n--- Stack Trace (副本回溯) ---\n");

        int frame = 0;
        // 注意：这里使用 &ctx 副本进行迭代
        while (StackWalk64(machineType, process, thread, &stack, &ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            if (stack.AddrPC.Offset == 0 || frame > 32) break;

            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;
            
            DWORD64 displacement = 0;
            if (SymFromAddr(process, stack.AddrPC.Offset, &displacement, pSymbol)) {
                fprintf(stderr, "[%d] 0x%llX : %s + 0x%llx\n", frame, stack.AddrPC.Offset, pSymbol->Name, displacement);
            } else {
                fprintf(stderr, "[%d] 0x%llX : (未知函数)\n", frame, stack.AddrPC.Offset);
            }

            IMAGEHLP_LINE64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD displacementLine = 0;
            if (SymGetLineFromAddr64(process, stack.AddrPC.Offset, &displacementLine, &line)) {
                fprintf(stderr, "    at %s:%d\n", line.FileName, line.LineNumber);
            }
            
            frame++;
        }
        SymCleanup(process);
    }

    LONG WINAPI UnhandledHandler(EXCEPTION_POINTERS* pExceptionInfo) {
        // 如果检测到调试器 (如 GDB/VS)，直接返回让调试器处理
        // 避免 CrashHandler 捕获异常导致调试器无法断在崩溃点
        if (IsDebuggerPresent()) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
        
        fprintf(stderr, "\n\n========================================\n");
        fprintf(stderr, "!!! 捕获到程序崩溃 !!!\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, "异常类型: %s (0x%lX)\n", GetExceptionName(code), code);
        fprintf(stderr, "崩溃地址: 0x%p\n", pExceptionInfo->ExceptionRecord->ExceptionAddress);

        // 步骤 1: 立即写入 Dump (此时 pExceptionInfo 还是干净的)
        WriteEnhancedMiniDump(pExceptionInfo);
        
        // 步骤 2: 打印堆栈 (使用副本，不破坏原始上下文)
        PrintStackSafe(pExceptionInfo->ContextRecord);

        fprintf(stderr, "========================================\n");
        
        tools::Logger::Shutdown(); 
        return EXCEPTION_EXECUTE_HANDLER; 
    }

    void CrashHandler::Init() {
        SetUnhandledExceptionFilter(UnhandledHandler);
        // 禁用 Windows 默认的“程序已停止响应”对话框，加速崩溃处理过程
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    }
}