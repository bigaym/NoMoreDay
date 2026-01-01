#include "CrashHandler.hpp"
#include "Logger.hpp"
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <time.h>
#include <vector>
#include <string>
#include <filesystem>

// 确保链接 dbghelp 库
// 在 MinGW 中，您可能需要在编译指令中添加 -ldbghelp
#ifdef _MSC_VER
#pragma comment(lib, "dbghelp.lib")
#endif

namespace NoMoreDay {

    // 获取异常名称
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

    // 生成 Minidump 文件 (专业调试用)
    void WriteMiniDump(EXCEPTION_POINTERS* pExceptionInfo) {
        // 创建 crashes 目录
        if (!std::filesystem::exists("crashes")) {
            std::filesystem::create_directory("crashes");
        }

        char timeBuf[64];
        time_t now = time(NULL);
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d_%H-%M-%S", localtime(&now));
        
        std::string dumpPath = std::string("crashes/crash_") + timeBuf + ".dmp";
        
        HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        
        if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE)) {
            MINIDUMP_EXCEPTION_INFORMATION mdei;
            mdei.ThreadId = GetCurrentThreadId();
            mdei.ExceptionPointers = pExceptionInfo;
            mdei.ClientPointers = FALSE;

            // 写入 Dump
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &mdei, NULL, NULL);
            CloseHandle(hFile);
            fprintf(stderr, "[CrashHandler] Minidump written to: %s\n", dumpPath.c_str());
        }
    }

    // 打印堆栈跟踪
    void PrintStack(CONTEXT* ctx) {
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        // 初始化符号处理器
        SymInitialize(process, NULL, TRUE);

        STACKFRAME64 stack;
        memset(&stack, 0, sizeof(stack));
        
        DWORD machineType;
#ifdef _M_X64
        machineType = IMAGE_FILE_MACHINE_AMD64;
        stack.AddrPC.Offset = ctx->Rip;
        stack.AddrPC.Mode = AddrModeFlat;
        stack.AddrFrame.Offset = ctx->Rbp;
        stack.AddrFrame.Mode = AddrModeFlat;
        stack.AddrStack.Offset = ctx->Rsp;
        stack.AddrStack.Mode = AddrModeFlat;
#else
        machineType = IMAGE_FILE_MACHINE_I386;
        stack.AddrPC.Offset = ctx->Eip;
        stack.AddrPC.Mode = AddrModeFlat;
        stack.AddrFrame.Offset = ctx->Ebp;
        stack.AddrFrame.Mode = AddrModeFlat;
        stack.AddrStack.Offset = ctx->Esp;
        stack.AddrStack.Mode = AddrModeFlat;
#endif

        fprintf(stderr, "\n--- Stack Trace (调用栈) ---\n");

        int frame = 0;
        while (StackWalk64(machineType, process, thread, &stack, ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            if (stack.AddrPC.Offset == 0) break;

            // 解析函数名
            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;
            
            DWORD64 displacement = 0;
            std::string symName = "Unknown Function";
            
            if (SymFromAddr(process, stack.AddrPC.Offset, &displacement, pSymbol)) {
                symName = pSymbol->Name;
            }

            // 解析文件名和行号
            IMAGEHLP_LINE64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD displacementLine = 0;
            std::string fileName = "Unknown File";
            int lineNum = 0;

            if (SymGetLineFromAddr64(process, stack.AddrPC.Offset, &displacementLine, &line)) {
                fileName = line.FileName;
                lineNum = line.LineNumber;
            }

            fprintf(stderr, "[%d] 0x%llX : %s\n", frame, stack.AddrPC.Offset, symName.c_str());
            if (fileName != "Unknown File") {
                fprintf(stderr, "    at %s:%d\n", fileName.c_str(), lineNum);
            } else {
                // MinGW 提示：如果无法解析行号，提示使用 addr2line
                fprintf(stderr, "    (MinGW Hint: addr2line -e NoMoreDay.exe 0x%llX)\n", stack.AddrPC.Offset);
            }
            
            frame++;
        }

        SymCleanup(process);
    }

    // 异常处理回调函数
    LONG WINAPI UnhandledHandler(EXCEPTION_POINTERS* pExceptionInfo) {
        DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
        
        // 使用 stderr 确保直接输出到控制台，绕过可能已损坏的 Logger 缓冲区
        fprintf(stderr, "\n\n========================================\n");
        fprintf(stderr, "!!! CRASH DETECTED (游戏崩溃) !!!\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, "Exception: %s (0x%lX)\n", GetExceptionName(code), code);
        fprintf(stderr, "Address: 0x%p\n", pExceptionInfo->ExceptionRecord->ExceptionAddress);

        // 1. 打印堆栈
        PrintStack(pExceptionInfo->ContextRecord);
        
        // 2. 写入 Minidump
        WriteMiniDump(pExceptionInfo);

        fprintf(stderr, "========================================\n");
        
        // 尝试刷新日志
        tools::Logger::Shutdown(); 

        // 返回 EXCEPTION_EXECUTE_HANDLER 表示我们已经处理了异常（通常会导致进程终止）
        // 返回 EXCEPTION_CONTINUE_SEARCH 会调用系统默认的崩溃对话框
        return EXCEPTION_EXECUTE_HANDLER; 
    }

    void CrashHandler::Init() {
        // 设置 Windows 未处理异常过滤器
        SetUnhandledExceptionFilter(UnhandledHandler);
        LOG_INFO("CrashHandler initialized. (System exceptions will be captured)");
    }
}
