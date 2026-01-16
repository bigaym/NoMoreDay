# 高级调试工具与技巧

当常规的日志阅读无法定位问题时，请使用以下高级工具和技巧。

## 1. Smart Tree 高级搜索

`smart-tree` 是项目内置的强大分析工具，不仅用于列出文件，还能进行智能搜索。

*   **查找最近修改的文件** (寻找引入 Bug 的变更):
    ```powershell
    # 查找过去 3 天内修改过的 C++ 文件
    find {type:'recent', days:3, pattern:'*.cpp'}
    ```

*   **搜索特定模式的代码**:
    ```powershell
    # 查找所有包含 "TODO" 的代码行
    search {keyword:'TODO', include_content:true}
    
    # 查找使用了 reinterpret_cast 的地方 (高风险)
    search {keyword:'reinterpret_cast', context_lines:2}
    ```

*   **分析大文件** (通常是逻辑复杂的重灾区):
    ```powershell
    find {type:'large', min_size:'50KB'}
    ```

## 2. AddressSanitizer (ASan) - 内存错误检测

ASan 是检测 Use-After-Free (UAF)、缓冲区溢出和内存泄漏的最强工具。GCC 和 MSVC 均支持。

### 如何开启 (CMake)

不需要修改 `CMakeLists.txt`，只需在生成构建文件时添加参数：

**GCC (MinGW):**
```powershell
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address -g" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" ..
```

**MSVC:**
```powershell
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="/fsanitize=address /Zi" ..
```

*注意：开启 ASan 后运行速度会变慢，且需要将对应的 ASan DLL 复制到可执行文件目录（MSVC 通常会自动处理，MinGW 可能需要手动复制 `libasan.dll`）。*

## 3. 静态分析 (Cppcheck)

如果你的环境中安装了 `cppcheck`，它可以在不运行代码的情况下发现逻辑错误。

```powershell
# 检查 src 目录，启用所有警告
cppcheck --enable=all --inconclusive --std=c++20 src/
```

## 4. 日志分析 (PowerShell)

使用 PowerShell 快速过滤大量日志：

```powershell
# 提取所有包含 "ERROR" 或 "CRITICAL" 的行，并显示前后 2 行
Get-Content logs/latest.log | Select-String -Pattern "ERROR|CRITICAL" -Context 2
```

## 5. 测试运行器 (Tests Runner)

项目使用统一的测试运行器，支持运行特定标签或套件的测试。

*   **GCC**: `.\build\bin\tests_runner.exe`
*   **MSVC**: `.\build\bin\Release\tests_runner.exe`

**运行特定测试:**
假设测试框架支持命令行参数（如 Doctest/GTest）：
```powershell
# 仅运行包含 "Combat" 的测试
.\build\bin\tests_runner.exe -tc="*Combat*"
```
