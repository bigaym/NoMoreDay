# Raylib 强制开启并加载 OpenGL 4.3 方案总结

在本项目中，我们需要使用 OpenGL 4.3 的高级特性（如 Compute Shader 和 SSBO），但 Raylib 5.5 在 Windows 平台默认请求 OpenGL 3.3 上下文。即使强制请求 4.3，如果函数加载器不完善，也会导致高级函数指针为空（NULL），从而引发 `0xC0000005` 访问违例崩溃。

以下是成功的完整修复方案：

## 1. CMake 构建配置优化

在 `CMakeLists.txt` 中，必须在 `add_subdirectory(third_party/raylib)` 之前强制设置缓存变量，确保 Raylib 内部的 `rlgl` 模块编译时开启 4.3 的宏定义。

```cmake
# 强制 Raylib 使用 OpenGL 4.3
set(OPENGL_VERSION "4.3" CACHE STRING "Force OpenGL 4.3" FORCE)
set(GRAPHICS "GRAPHICS_API_OPENGL_43" CACHE STRING "Force OpenGL 4.3" FORCE)

# 由于我们需要手动调用 GLFW 函数，需确保链接 glfw 库
target_link_libraries(NoMoreDayCore PUBLIC raylib glfw ...)

# 添加 GLFW 头文件路径
target_include_directories(NoMoreDayCore PUBLIC 
    third_party/raylib/src/external/glfw/include
)
```

## 2. 健壮的 GLAD 函数加载器

在 Windows 上，`glfwGetProcAddress` 可能无法返回 1.1 版本的核心函数（如 `glGetString`），而 `wglGetProcAddress` 也不一定能直接获取所有 4.3 函数。我们需要一个“组合式”加载器。

在 `Game.cpp` 初始化时：

```cpp
InitWindow(m_screenWidth, m_screenHeight, m_title);

// 定义鲁棒的加载器 lambda
auto glad_loader = [](const char* name) -> void* {
    void* p = (void*)glfwGetProcAddress(name);
#ifdef _WIN32
    // 如果 GLFW 加载失败，回退到系统原生的 opengl32.dll
    if (p == nullptr || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
        static HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
        if (opengl32 == nullptr) opengl32 = LoadLibraryA("opengl32.dll");
        p = (void*)GetProcAddress(opengl32, name);
    }
#endif
    return p;
};

// 初始化 GLAD
// 注意：即使返回值是 0 也不一定要中断，因为只要我们需要的功能函数加载成功即可
gladLoadGL((GLADloadfunc)+glad_loader);
```

## 3. 代码层面的安全降级

由于 `gladLoadGL` 只要有一个不重要的函数加载失败就会返回 0，因此不能仅靠返回值判断。在调用高级功能前，必须进行函数指针判空。

在 `GPUUtils.hpp` 中：

```cpp
if (info.computeShaderSupported) {
    // 关键：检查 4.3 特有的函数指针是否真的加载成功了
    if (glGetIntegeri_v == nullptr || glDispatchCompute == nullptr) {
        LOG_WARN("Compute shaders reported as supported, but function pointers are NULL!");
        info.computeShaderSupported = false; // 安全降级，防止崩溃
    } else {
        // 执行高级功能...
    }
}
```

## 4. 解决文件锁定问题

在频繁崩溃和重启过程中，`NoMoreDay.exe` 容易被系统（如调试器或僵尸进程）锁定。
*   **方案**：构建脚本中使用 `taskkill` 强制结束进程，或者在 `cmake` 链接失败时尝试重命名已有的 `.exe` 文件（Windows 允许重命名打开的文件）。

## 5. 结论

通过以上三位一体的修复（CMake 编译参数 + 组合式加载器 + 函数指针判空），我们解决了游戏启动即崩溃的问题，并确保了在不同驱动环境下都能安全运行。
