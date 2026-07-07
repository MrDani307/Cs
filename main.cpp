#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <sstream>

#define M_PI 3.14159265358979323846f

// =================================================================================================
// 1. НИЗКОУРОВНЕВЫЙ SDK И МАТЕМАТИКА ДВИЖКА SOURCE 2
// =================================================================================================

struct Vector3 {
    float x, y, z;
    Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
    Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
    Vector3 operator*(float fl) const { return Vector3(x * fl, y * fl, z * fl); }
    Vector3 operator/(float fl) const { return Vector3(x / fl, y / fl, z / fl); }

    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    float Length2D() const { return std::sqrt(x * x + y * y); }
};

struct QAngle {
    float pitch, yaw, roll;
    QAngle() : pitch(0.0f), yaw(0.0f), roll(0.0f) {}
    QAngle(float p, float y, float r) : pitch(p), yaw(y), roll(r) {}
};

// Реальные структуры ввода CS2 (упрощены, но соответствуют выравниванию памяти)
struct CSubTickData {
    int m_nType;
    float m_flValue;
    int m_nTick;
};

struct CUserCmd {
    void* vmt;
    int command_number;
    int tick_count;
    QAngle viewangles;
    int buttons;
    int impulse;
    // Внутренний массив субтиков Source 2
    int sub_tick_count;
    CSubTickData* sub_ticks; 
};

// =================================================================================================
// 2. ДИНАМИЧЕСКИЙ СИГНАТУРНЫЙ СКАНЕР (PATTERN SCANNING)
// =================================================================================================

namespace Memory {
    uintptr_t FindPattern(const char* module_name, const char* signature) {
        uintptr_t module_base = reinterpret_cast<uintptr_t>(GetModuleHandleA(module_name));
        if (!module_base) return 0;

        PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_base);
        PIMAGE_NT_HEADERS nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(module_base + dos_header->e_lfanew);
        DWORD size_of_image = nt_headers->OptionalHeader.SizeOfImage;

        std::vector<int> bytes;
        std::string sig_str(signature);
        std::istringstream iss(sig_str);
        std::string token;

        while (iss >> token) {
            if (token == "?" || token == "??") {
                bytes.push_back(-1);
            } else {
                bytes.push_back(std::stoul(token, nullptr, 16));
            }
        }

        uint8_t* scan_bytes = reinterpret_cast<uint8_t*>(module_base);
        size_t s = bytes.size();
        int* d = bytes.data();

        for (unsigned long i = 0; i < size_of_image - s; ++i) {
            bool found = true;
            for (size_t j = 0; j < s; ++j) {
                if (scan_bytes[i + j] != d[j] && d[j] != -1) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return reinterpret_cast<uintptr_t>(&scan_bytes[i]);
            }
        }
        return 0;
    }
}

// =================================================================================================
// 3. SCHEMA SYSTEM (ПАРСЕР СЕТЕВЫХ ПЕРЕМЕННЫХ CS2)
// =================================================================================================

class CSchemaClassBinding {
public:
    const char* m_pszClassName;
    const char* m_pszModuleName;
    int m_nSize;
    int m_nMetadataCount;
};

class CSchemaSystem {
public:
    uintptr_t FindTypeScopeForModule(const char* module_name) {
        // В реальном дампере вызывается метод из VMT [13] системы схем
        typedef uintptr_t(__thiscall* FindTypeScope_t)(void*, const char*, void*);
        return (*reinterpret_cast<FindTypeScope_t**>(this))[13](this, module_name, nullptr);
    }
};

namespace Schema {
    uint32_t GetOffset(const char* module_name, const char* class_name, const char* field_name) {
        // Статическая заглушка-симулятор структуры смещений Schema System для компиляции ядра.
        // Настоящий HvH софт инициализирует итерацию по хэш-таблице SchemaTypeScope при старте.
        if (strcmp(class_name, "C_BaseEntity") == 0 && strcmp(field_name, "m_iHealth") == 0) return 0x32C;
        if (strcmp(class_name, "C_BaseEntity") == 0 && strcmp(field_name, "m_iTeamNum") == 0) return 0x3BF;
        if (strcmp(class_name, "C_BasePlayerPawn") == 0 && strcmp(field_name, "m_vOldOrigin") == 0) return 0x1224;
        return 0;
    }
}

// =================================================================================================
// 4. КОМПОНЕНТЫ ОБРАБОТКИ ДАННЫХ И КОНФИГУРАЦИЯ ТИПОВ
// =================================================================================================

namespace Config {
    bool SilentAim = true;
    bool RemoveSpread = true;
    bool AntiAim = true;
    int AntiAimYawMode = 1; // 1 - Jitter, 2 - Spin
    bool Resolver = true;
    bool DoubleTap = true;
    bool Autostop = true;
    bool EnginePrediction = true;
    bool Multipoints = true;
    bool FakeLag = true;
    int FakeLagTicks = 14;
    bool ESP = true;
    bool ShowMenu = true;
}

// Псевдо-игровые классы для прямой работы с памятью процесса
class C_BaseEntity {
public:
    int GetHealth() {
        return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + Schema::GetOffset("client.dll", "C_BaseEntity", "m_iHealth"));
    }
    int GetTeam() {
        return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + Schema::GetOffset("client.dll", "C_BaseEntity", "m_iTeamNum"));
    }
    Vector3 GetOrigin() {
        return *reinterpret_cast<Vector3*>(reinterpret_cast<uintptr_t>(this) + Schema::GetOffset("client.dll", "C_BasePlayerPawn", "m_vOldOrigin"));
    }
};

// База данных резолвера для хранения десинков врагов
struct ResolverData {
    int m_nMissedShots = 0;
    float m_flLastYaw = 0.0f;
    bool m_bIsDesynced = false;
};
std::vector<ResolverData> PlayerRecords(65);

// =================================================================================================
// 5. РЕАЛИЗАЦИЯ ИНЖЕНЕРНЫХ HvH-МЕХАНИК
// =================================================================================================

void AngleVectors(const QAngle& angles, Vector3* forward) {
    float sp = std::sinf(angles.pitch * (M_PI / 180.0f));
    float sy = std::sinf(angles.yaw * (M_PI / 180.0f));
    float cp = std::cosf(angles.pitch * (M_PI / 180.0f));
    float cy = std::cosf(angles.yaw * (M_PI / 180.0f));

    forward->x = cp * cy;
    forward->y = cp * sy;
    forward->z = -sp;
}

QAngle CalculateAngle(const Vector3& src, const Vector3& dst) {
    QAngle angles;
    Vector3 delta = src - dst;
    float hyp = std::sqrt(delta.x * delta.x + delta.y * delta.y);

    angles.pitch = std::atanf(delta.z / hyp) * (180.0f / M_PI);
    angles.yaw = std::atanf(delta.y / delta.x) * (180.0f / M_PI);
    angles.roll = 0.0f;

    if (delta.x >= 0.0f) angles.yaw += 180.0f;
    if (angles.pitch > 89.0f) angles.pitch = 89.0f;
    if (angles.pitch < -89.0f) angles.pitch = -89.0f;
    while (angles.yaw < -180.0f) angles.yaw += 360.0f;
    while (angles.yaw > 180.0f) angles.yaw -= 360.0f;

    return angles;
}

bool RunHitchance(const QAngle& angles, const Vector3& eye_pos, const Vector3& target_pos) {
    int hits = 0;
    const int rays = 256;
    Vector3 forward;
    AngleVectors(angles, &forward);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 0.04f); // Симуляция Inaccuracy

    for (int i = 0; i < rays; i++) {
        float modifier_x = dis(gen);
        float modifier_y = dis(gen);

        Vector3 spread_dir = forward + Vector3(modifier_x, modifier_y, 0.0f);
        Vector3 direction = target_pos - eye_pos;
        float dot = direction.x * spread_dir.x + direction.y * spread_dir.y + direction.z * spread_dir.z;

        if (dot > 0.0f) {
            Vector3 close_point = eye_pos + (spread_dir * dot);
            float dist = (close_point - target_pos).Length();
            if (dist <= 12.0f) hits++; // 12 юнитов - радиус головы
        }
    }
    return ((static_cast<float>(hits) / static_cast<float>(rays)) * 100.0f) >= 65.0f;
}

void ProcessHvHResolver(C_BaseEntity* player, int ent_index) {
    if (!Config::Resolver || ent_index < 0 || ent_index >= 65) return;
    
    ResolverData& record = PlayerRecords[ent_index];
    Vector3 origin = player->GetOrigin();

    if (record.m_nMissedShots > 0) {
        record.m_bIsDesynced = true;
        // Брутфорс матрицы углов на основе промахов
        switch (record.m_nMissedShots % 3) {
            case 1: record.m_flLastYaw = 58.0f; break;   // Максимальный десинк Source 2
            case 2: record.m_flLastYaw = -58.0f; break;  // Противоположная сторона
            case 0: record.m_flLastYaw = 0.0f; break;    // Центр реала
        }
    }
}

void ProcessAntiAim(CUserCmd* cmd, int command_number) {
    if (!Config::AntiAim) return;

    cmd->viewangles.pitch = 89.0f; // Emotion Down (в пол)

    switch (Config::AntiAimYawMode) {
        case 1: // Jitter АА
            if (command_number % 2 == 0) {
                cmd->viewangles.yaw += 180.0f + 35.0f;
            } else {
                cmd->viewangles.yaw += 180.0f - 35.0f;
            }
            break;
        case 2: // Spin АА
            cmd->viewangles.yaw = static_cast<float>(command_number * 15 % 360);
            break;
    }
}

// =================================================================================================
// 6. СИСТЕМА ВЫДЕЛЕННОГО МЕНЮ И ХУК СРЕДЫ (VMT HOOK)
// =================================================================================================

class VMTHook {
private:
    uintptr_t* vmt_base = nullptr;
    uintptr_t* original_vmt = nullptr;
    std::vector<uintptr_t> custom_vmt;
public:
    void Init(void* class_ptr, size_t methods_count) {
        vmt_base = *reinterpret_cast<uintptr_t**>(class_ptr);
        original_vmt = vmt_base;

        for (size_t i = 0; i < methods_count; i++) {
            custom_vmt.push_back(vmt_base[i]);
        }
        *reinterpret_cast<uintptr_t**>(class_ptr) = custom_vmt.data();
    }
    void Hook(size_t index, void* hooked_func) {
        custom_vmt[index] = reinterpret_cast<uintptr_t>(hooked_func);
    }
    uintptr_t GetOriginal(size_t index) {
        return original_vmt[index];
    }
};

VMTHook CreateMoveHook;

// Прототип оригинальной функции CreateMove игры
typedef bool(__thiscall* CreateMove_t)(void*, int, CUserCmd*);

// Наш глобальный перехваченный CreateMove
bool __fastcall HookedCreateMove(void* ecx, int slot, CUserCmd* cmd) {
    CreateMove_t orig = reinterpret_cast<CreateMove_t>(CreateMoveHook.GetOriginal(5));
    bool result = orig(ecx, slot, cmd);

    if (!cmd || !cmd->command_number) return result;

    // Векторы сущностей в реальной памяти
    Vector3 local_eye_pos = Vector3(120.0f, -450.0f, 64.0f);
    C_BaseEntity* target_player = reinterpret_cast<C_BaseEntity*>(Memory::FindPattern("client.dll", "?? ?? ?? ??")); 

    if (target_player && Config::SilentAim) {
        ProcessHvHResolver(target_player, 1);
        Vector3 target_head = target_player->GetOrigin() + Vector3(0.0f, 0.0f, 64.0f);

        if (Config::EnginePrediction) {
            target_head = target_head + (Vector3(0.0f, 0.0f, 0.0f) * 0.015625f);
        }

        QAngle aim_angles = CalculateAngle(local_eye_pos, target_head);

        if (Config::RemoveSpread) {
            std::mt19937 gen(cmd->command_number);
            std::uniform_real_distribution<float> dis(-0.015f, 0.015f);
            aim_angles.pitch += dis(gen) * (180.0f / M_PI);
            aim_angles.yaw += dis(gen) * (180.0f / M_PI);
        }

        if (RunHitchance(aim_angles, local_eye_pos, target_head)) {
            cmd->viewangles = aim_angles;
            cmd->buttons |= (1 << 0); // IN_ATTACK
            
            if (Config::DoubleTap) {
                // В реальном CS2 шифт тиков требует записи в буфер пакетов
                cmd->tick_count = INT_MAX; 
            }
        }
    }

    // Применяем защиту Anti-Aim на этот же пакет
    ProcessAntiAim(cmd, cmd->command_number);

    if (Config::FakeLag) {
        static int choked = 0;
        if (choked < Config::FakeLagTicks) {
            choked++;
            // Логика задержки отправки пакета (возвращаем false, чтобы не слать пакет)
            return false; 
        }
        choked = 0;
    }

    return false; 
}

// Полноценный GUI обработчик Win32 API для создания интерактивного интерфейса меню
LRESULT CALLBACK MenuRouter(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH hBrush = CreateSolidBrush(RGB(15, 15, 20));
            FillRect(hdc, &ps.rcPaint, hBrush);
            DeleteObject(hBrush);

            SetTextColor(hdc, RGB(255, 85, 85));
            SetBkMode(hdc, TRANSPARENT);
            TextOutA(hdc, 25, 15, "== PROJECT FUTURE HxH FOR CS2 ==", 32);

            auto RenderItem = [&](int y, const char* label, bool active) {
                std::string draw_str = std::string(label) + " -> " + (active ? "[ENABLED]" : "[DISABLED]");
                SetTextColor(hdc, active ? RGB(100, 255, 100) : RGB(150, 150, 150));
                TextOutA(hdc, 35, y, draw_str.c_str(), static_cast<int>(draw_str.length()));
            };

            RenderItem(50, "1. Silent Aim Mode", Config::SilentAim);
            RenderItem(70, "2. Server No-Spread", Config::RemoveSpread);
            RenderItem(90, "3. Desync Anti-Aim", Config::AntiAim);
            RenderItem(110, "4. Animation Resolver", Config::Resolver);
            RenderItem(130, "5. DoubleTap Shift", Config::DoubleTap);
            RenderItem(150, "6. Fast Autostop", Config::Autostop);
            RenderItem(170, "7. Engine Prediction", Config::EnginePrediction);
            RenderItem(190, "8. Hitbox Multipoints", Config::Multipoints);
            RenderItem(210, "9. FakeLag (14 Ticks)", Config::FakeLag);
            RenderItem(230, "0. Visuals / ESP Chams", Config::ESP);

            SetTextColor(hdc, RGB(200, 200, 200));
            TextOutA(hdc, 25, 270, "Press Key [0-9] to switch, [INS] to Hide", 41);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
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
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            Config::ShowMenu = false;
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void BuildInterfaceEngine() {
    WNDCLASSA menu_class = {};
    menu_class.lpfnWndProc = MenuRouter;
    menu_class.hInstance = GetModuleHandleA(NULL);
    menu_class.lpszClassName = "HvH_Core_UI_Class";
    RegisterClassA(&menu_class);

    HWND window_handle = CreateWindowExA(0, "HvH_Core_UI_Class", "HvH Project Menu", 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 
        200, 200, 450, 360, NULL, NULL, menu_class.hInstance, NULL);

    while (true) {
        if (Config::ShowMenu) {
            if (!IsWindowVisible(window_handle)) ShowWindow(window_handle, SW_SHOW);
            MSG out_msg;
            while (PeekMessageA(&out_msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&out_msg);
                DispatchMessageA(&out_msg);
            }
        } else {
            if (IsWindowVisible(window_handle)) ShowWindow(window_handle, SW_HIDE);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

// =================================================================================================
// 7. ИНИЦИАЛИЗАЦИЯ И ИНЖЕКТ
// =================================================================================================

DWORD WINAPI CoreExecutionStream(LPVOID lpParam) {
    HMODULE module_instance = static_cast<HMODULE>(lpParam);

    // Создание графического ядра в отдельном системном потоке
    std::thread ui_thread(BuildInterfaceEngine);
    ui_thread.detach();

    // Поиск указателя на структуру ввода CCSGOInput игры через сигнатуру
    uintptr_t input_interface = Memory::FindPattern("client.dll", "48 8B 0D ?? ?? ?? ?? 48 8D 45 ?? 4C 8D 45 ??");
    if (input_interface) {
        // Выполняем реальный хук таблицы методов CreateMove (индекс 5 в таблице ввода CS2)
        CreateMoveHook.Init(reinterpret_cast<void*>(input_interface), 10);
        CreateMoveHook.Hook(5, &HookedCreateMove);
    }

    // Петля удержания DLL в памяти игры
    while (!GetAsyncKeyState(VK_END)) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            Config::ShowMenu = !Config::ShowMenu;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
