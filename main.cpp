#include <windows.h>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

// Твики для полной автономности графического ядра без внешних зависимостей ImGui
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
    bool ShowMenu = true;
}

// Указатели на оригинальные функции
typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

Present_t oPresent = nullptr;
WNDPROC oWndProc = nullptr;

ID3D11Device* pDevice = nullptr;
ID3D11DeviceContext* pContext = nullptr;
ID3D11RenderTargetView* mainRenderTargetView = nullptr;
HWND window = nullptr;

// Наш кастомный обработчик ввода внутри игры (блокирует клики по игре, когда открыто меню)
LRESULT __stdcall WndProcHook(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        Config::ShowMenu = !Config::ShowMenu;
        return 0;
    }

    if (Config::ShowMenu) {
        // Логика переключения элементов меню кнопками 0-9 прямо во время игры
        if (uMsg == WM_KEYDOWN) {
            switch (wParam) {
                case '1': Config::SilentAim = !Config::SilentAim; break;
                case '2': Config::RemoveSpread = !Config::RemoveSpread; break;
                case '3': Config::AntiAim = !Config::AntiAim; break;
                case '4': Config::Resolver = !Config::Resolver; break;
                case '5': Config::DoubleTap = !Config::DoubleTap; break;
                case '6': Config::Autostop = !Config::Autostop; break;
                case '7': Config::EnginePrediction = !Config::EnginePrediction; break;
                case '8': Config::Multipoints = !Config::Multipoints; break;
                case '9': Config::FakeLag = !Config::FakeLag; break;
                case '0': Config::ESP = !Config::ESP; break;
            }
        }
        return 1; // Поглощаем ввод, чтобы игра не реагировала на клики при открытом меню
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

// Отрендерить кастомный графический интерфейс поверх кадра CS2 встроенными средствами GDI/D3D текстовых буферов
void DrawMenuOverlay() {
    if (!Config::ShowMenu) return;

    // В полноценных читах здесь инициализируется ImGui::NewFrame()
    // Для легковесной компиляции через GitHub Actions выводим элементы через внутренний контекст окна
    HDC hdc = GetDC(window);
    if (hdc) {
        HBRUSH hBrush = CreateSolidBrush(RGB(20, 20, 25));
        RECT rect = { 50, 50, 450, 400 };
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        SetTextColor(hdc, RGB(255, 75, 75));
        SetBkMode(hdc, TRANSPARENT);
        TextOutA(hdc, 70, 70, "=== FUTURE project HvH inside CS2 ===", 37);

        auto DrawMenuRow = [&](int y, const char* label, bool state) {
            std::string text = std::string(label) + (state ? " [ON]" : " [OFF]");
            SetTextColor(hdc, state ? RGB(0, 255, 128) : RGB(160, 160, 160));
            TextOutA(hdc, 80, y, text.c_str(), static_cast<int>(text.length()));
        };

        DrawMenuRow(100, "1. Silent Aim Mode", Config::SilentAim);
        DrawMenuRow(120, "2. Server No-Spread", Config::RemoveSpread);
        DrawMenuRow(140, "3. Desync Anti-Aim", Config::AntiAim);
        DrawMenuRow(160, "4. Animation Resolver", Config::Resolver);
        DrawMenuRow(180, "5. DoubleTap Shift", Config::DoubleTap);
        DrawMenuRow(200, "6. Fast Autostop", Config::Autostop);
        DrawMenuRow(220, "7. Engine Prediction", Config::EnginePrediction);
        DrawMenuRow(240, "8. Hitbox Multipoints", Config::Multipoints);
        DrawMenuRow(260, "9. FakeLag Engine", Config::FakeLag);
        DrawMenuRow(280, "0. Visuals / ESP Chams", Config::ESP);

        SetTextColor(hdc, RGB(200, 200, 200));
        TextOutA(hdc, 70, 320, "Press keys [1-0] to toggle hacks", 32);
        TextOutA(hdc, 70, 340, "Press [INS] to close menu", 25);

        ReleaseDC(window, hdc);
    }
}

// Наш перехваченный графический поток Present
HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!pDevice) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice))) {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;
            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
        }
    }

    // Вызываем отрисовку нашего меню поверх кадра игры
    DrawMenuOverlay();

    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI CoreExecutionStream(LPVOID lpParam) {
    HMODULE module_instance = static_cast<HMODULE>(lpParam);

    // Симуляция поиска SwapChain vtable для хука DirectX 11
    // В реальной CS2 адрес извлекается через сигнатурный паттерн dxgi.dll
    HWND hWnd = CreateWindowA("BUTTON", "Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    IDXGISwapChain* pSwapChain;
    D3D_FEATURE_LEVEL featureLevel;
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, &featureLevel, &pContext);
    
    if (SUCCEEDED(hr) && pSwapChain) {
        void** pVMT = *(void***)pSwapChain;
        oPresent = (Present_t)pVMT[8]; // 8-й индекс в таблице DXGI - это функция Present
        
        // В продакшн коде подмена адреса делается через MinHook, переписывая указатель в защищенной памяти
        DWORD oldProtect;
        VirtualProtect(&pVMT[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        pVMT[8] = &HookedPresent;
        VirtualProtect(&pVMT[8], sizeof(void*), oldProtect, &oldProtect);

        pSwapChain->Release();
        DestroyWindow(hWnd);
    }

    while (!GetAsyncKeyState(VK_END)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Восстановление оригинального WndProc при выгрузке
    if (oWndProc) {
        SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
    }

    FreeLibraryAndExitThread(module_instance, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0, CoreExecutionStream, hModule, 0, nullptr));
    }
    return TRUE;
}
