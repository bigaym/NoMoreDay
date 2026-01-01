#pragma once

namespace NoMoreDay {
    class CrashHandler {
    public:
        // 初始化崩溃捕获器 (设置 Unhandled Exception Filter)
        static void Init();
    };
}
