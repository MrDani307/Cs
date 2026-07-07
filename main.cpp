#include <windows.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <string>

namespace Config {
    bool SilentAim = true;
    bool RemoveSpread = true;
    bool AntiAim = true;
    bool Resolver = true;
    bool DoubleTap = true;
    bool Autostop = true;
    bool EnginePrediction = true;
    bool Multipoints = true;
    bool FakeLag = true;
    bool ESP = true;
}

// Функция для отрисовки меню прямо в консоли
void RenderConsoleMenu() {
    // Очищаем консоль
    system("cls");

    std::cout << "==================================================\n";
    std::cout << "        PROJECT FUTURE HvH - CONSOLE MENU         \n";
    std::cout << "==================================================\n\n";

    auto PrintItem = [](const char* label, bool active) {
        std::cout << "  " << label << " -> " << (active ? "[\x1B[32mENABLED\x1B[0m]" : "[\x1B[31mDISABLED\x1B[0m]") << "\n";
    };

    PrintItem("[F1] Silent Aim Mode    ", Config::SilentAim);
    PrintItem("[F2] Server No-Spread   ", Config::RemoveSpread);
    PrintItem("[F3] Desync Anti-Aim    ", Config::AntiAim);
    PrintItem("[F4] Animation Resolver ", Config::Resolver);
    PrintItem("[F5] DoubleTap Shift    ", Config::DoubleTap);
    PrintItem("[F6] Fast Autostop      ", Config::Autostop);
    PrintItem("[F7] Engine Prediction  ", Config::EnginePrediction);
    PrintItem("[F8] Hitbox Multipoints ", Config::Multipoints);
    PrintItem("[F9] FakeLag (14 Ticks) ", Config::FakeLag);
    PrintItem("[F10] Visuals / ESP     ", Config::ESP);

    std::cout << "\n==================================================\n";
    std::cout << " Use keys [F1 - F10] to toggle features\n";
    std::cout << " Press [END] to Uninject\n";
    std::cout << "==================================================\n";
}

DWORD WINAPI CoreExecutionStream(LPVOID lpParam) {
    HMODULE module_instance = static_cast<HMODULE>(lpParam);

    // Выделяем отдельное окно консоли для процесса игры
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONIN$", "r", stdin);

    // Устанавливаем заголовок консоли
    SetConsoleTitleA("HvH Core Loader");

    // Сразу же при инжекте выводим меню на экран
    RenderConsoleMenu();

    // Цикл отслеживания горячих клавиш в игре
    while (!GetAsyncKeyState(VK_END)) {
        bool changed = false;

        if (GetAsyncKeyState(VK_F1) & 1) { Config::SilentAim = !Config::SilentAim; changed = true; }
        if (GetAsyncKeyState(VK_F2) & 1) { Config::RemoveSpread = !Config::RemoveSpread; changed = true; }
        if (GetAsyncKeyState(VK_F3) & 1) { Config::AntiAim = !Config::AntiAim; changed = true; }
        if (GetAsyncKeyState(VK_F4) & 1) { Config::Resolver = !Config::Resolver; changed = true; }
        if (GetAsyncKeyState(VK_F5) & 1) { Config::DoubleTap = !Config::DoubleTap; changed = true; }
        if (GetAsyncKeyState(VK_F6) & 1) { Config::Autostop = !Config::Autostop; changed = true; }
        if (GetAsyncKeyState(VK_F7) & 1) { Config::EnginePrediction = !Config::EnginePrediction; changed = true; }
        if (GetAsyncKeyState(VK_F8) & 1) { Config::Multipoints = !Config::Multipoints; changed = true; }
        if (GetAsyncKeyState(VK_F9) & 1) { Config::FakeLag = !Config::FakeLag; changed = true; }
        if (GetAsyncKeyState(VK_F10) & 1) { Config::ESP = !Config::ESP; changed = true; }

        // Если какая-то настройка изменилась, мгновенно перерисовываем интерфейс
        if (changed) {
            RenderConsoleMenu();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Закрываем консоль перед выгрузкой
    if (fDummy) fclose(fDummy);
    FreeConsole();
    FreeLibraryAndExitThread(module_instance, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0, CoreExecutionStream, hModule, 0, nullptr));
    }
    return TRUE;
}
