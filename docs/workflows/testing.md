# NoMoreDay 测试与验证流程

## 验证策略

1. 编辑前确定受影响的测试层级和验收证据。
2. 先运行最窄的相关检查，再按风险扩大到对应 CTest 标签、门禁或手测。
3. 新行为、修复和合同变更应在 `tests/` 的合适层级留下回归防护；`REQUIRE` 用于继续执行所必需的前置条件，`CHECK` 用于值断言。
4. 不得在生产代码加入只为测试服务的硬编码分支、facade mock 或 dummy data；测试替身只存在于测试代码。
5. 失败时记录命令、配置、关键错误和是否属于已知非阻塞问题；不得把失败的检查表述为通过。

## 构建命令

从仓库根目录 `D:\PRJ\NoMoreDay` 执行：

```powershell
./build.bat
./build.bat clean
./build.bat clean-all
./build.bat release
./build.bat debug
./build.bat check
./build.bat analyze
./build.bat asan
./build.bat gate
./build.bat combat-gate
./build.bat notest
```

默认 `./build.bat` 使用 RelWithDebInfo。构建输出较大时重定向到日志文件，再读取关键错误段；不要把完整错误流输出到控制台。

```powershell
./build.bat > build_log.txt 2>&1
Get-Content -Encoding UTF8 build_log.txt -TotalCount 100
```

## CTest 与单例

MSVC 多配置生成器必须显式指定 `-C`：

```powershell
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
ctest --test-dir build -C Release -L performance --output-on-failure
./bin/NoMoreDayTests.exe --list-test-cases
./bin/NoMoreDayTests.exe --test-case="[Unit] ..."
```

优先使用 `bin/NoMoreDayTests.exe`。回退路径为 `build/bin/Release/NoMoreDayTests.exe`（MSVC）和 `build/bin/NoMoreDayTests.exe`（G++）。`build.bat perf` 已弃用，性能测试使用上面的 `ctest` 命令。

## 完成条件

- C++ 或构建文件变更：运行 `./build.bat` 和与改动相称的测试后，才可声明完成。
- 纯文档变更：检查 Markdown 链接、导航和术语；无需构建，但交付时说明跳过构建的原因。
- 性能、渲染或发布任务：按对应规格或 Track 的门禁执行；通过、豁免和已知风险必须明确区分。
- 阶段或发布验收：完成规定的自动化检查后，提供无法由自动化覆盖的手测步骤，并等待用户验收；未经明确授权不得创建提交。
- 评审文档（设计评审、计划评审、实施检查等）存放于 `docs/reviews/`；审查标准与报告格式见 `docs/workflows/review.md`。
