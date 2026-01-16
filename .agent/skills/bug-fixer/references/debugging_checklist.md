# 调试与安全检查清单

在宣布 Bug 修复完成之前，请逐项核对以下内容：

## 1. 分析阶段
- [ ] 我是否已经定位到导致失败的确切代码行或逻辑？
- [ ] 我是否检索了代码库中是否存在类似的模式（以防复制粘贴引入的重复 Bug）？
- [ ] 我是否仔细阅读了所有相关日志和堆栈跟踪？

## 2. 实现安全性
- [ ] **EnTT 安全性**: 在调用 `registry.create()`、`registry.destroy()` 或 `registry.emplace()` 等修改注册表的操作时，我是否**没有**持有任何组件指针 (`component*`)？（这是导致 Use-After-Free 的常见原因）。
- [ ] **指针安全**: 在解引用之前，我是否检查了 `nullptr`？
- [ ] **内存管理**: 我是否使用了智能指针（`unique_ptr`, `shared_ptr`）？严禁使用原始的 `new`/`delete`。
- [ ] **锁机制**: 如果涉及多线程，我是否使用了 `std::scoped_lock`？
- [ ] **常量管理**: 我是否避免了使用魔术数字？（已移至 `Common.hpp` 或使用 `constexpr`）。

## 3. 验证阶段
- [ ] **编译**: `.\build.bat` 是否编译成功？
- [ ] **测试**: 我是否运行了相关的测试可执行文件？（例如：`.\build\bin\tests\CombatSystemTests.exe`）
- [ ] **新增测试**: 如果可能，我是否添加了能够复现该 Bug 的测试用例？

## 4. 规范一致性
- [ ] 代码是否符合 `conductor/code_standard.md` 的规范？
- [ ] 格式化（Clang-format）是否与项目保持一致？