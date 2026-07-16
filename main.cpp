#include <windows.h>
#include <thread>
#include <chrono>
#include <stdint.h>

// Относительные смещения (офсеты) для актуальной версии CS2
namespace Offsets {
    constexpr uintptr_t dwEntityList = 0x183FCE8;       // Смещение списка сущностей
    constexpr uintptr_t dwLocalPlayerController = 0x1824A18; // Локальный игрок
    constexpr uintptr_t m_hPlayerPawn = 0x7E4;          // Связь контроллера с пешкой игрока
    constexpr uintptr_t m_iTeamNum = 0x3E3;             // Номер команды
    constexpr uintptr_t m_iHealth = 0x344;              // Здоровье
    constexpr uintptr_t m_pGlowSceneObject = 0x1100;    // Объект свечения в памяти сцены
    constexpr uintptr_t m_bIsGlowing = 0x1110;          // Флаг активации свечения
}

struct GlowColor_t {
    float r, g, b, a;
};

// Функция безопасного чтения памяти внутри процесса
template <typename T>
T ReadMemory(uintptr_t address) {
    if (address < 0x10000 || address > 0x7FFFFFFFFFFF) return T{};
    return *reinterpret_cast<T*>(address);
}

// Функция безопасной записи памяти внутри процесса
template <typename T>
void WriteMemory(uintptr_t address, T value) {
    if (address < 0x10000 || address > 0x7FFFFFFFFFFF) return;
    *reinterpret_cast<T*>(address) = value;
}

void ExecuteGlowESP() {
    uintptr_t client_base = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    if (!client_base) return;

    uintptr_t entity_list = ReadMemory<uintptr_t>(client_base + Offsets::dwEntityList);
    uintptr_t local_controller = ReadMemory<uintptr_t>(client_base + Offsets::dwLocalPlayerController);
    if (!entity_list || !local_controller) return;

    int local_team = ReadMemory<int>(local_controller + Offsets::m_iTeamNum);

    // Проход по списку сущностей (максимум 64 игрока)
    for (int i = 1; i < 64; ++i) {
        uintptr_t list_entry = ReadMemory<uintptr_t>(entity_list + (8 * (i & 0x7FFF) >> 9) + 16);
        if (!list_entry) continue;

        uintptr_t controller = ReadMemory<uintptr_t>(list_entry + 120 * (i & 0x1FF));
        if (!controller || controller == local_controller) continue;

        // Получаем пешку (pawn) игрока из контроллера
        uint32_t player_pawn_handle = ReadMemory<uint32_t>(controller + Offsets::m_hPlayerPawn);
        if (!player_pawn_handle) continue;

        uintptr_t list_entry2 = ReadMemory<uintptr_t>(entity_list + (8 * (player_pawn_handle & 0x7FFF) >> 9) + 16);
        if (!list_entry2) continue;

        uintptr_t pawn = ReadMemory<uintptr_t>(list_entry2 + 120 * (player_pawn_handle & 0x1FF));
        if (!pawn) continue;

        int health = ReadMemory<int>(pawn + Offsets::m_iHealth);
        int team = ReadMemory<int>(pawn + Offsets::m_iTeamNum);

        if (health <= 0 || health > 100) continue;

        // Если это противник — включаем Glow-подсветку
        if (team != local_team) {
            // Активируем статус свечения в объекте сцены
            WriteMemory<bool>(pawn + Offsets::m_bIsGlowing, true);

            // Настраиваем цвет (красный для врагов)
            uintptr_t glow_scene_object = ReadMemory<uintptr_t>(pawn + Offsets::m_pGlowSceneObject);
            if (glow_scene_object) {
                GlowColor_t color = { 1.0f, 0.0f, 0.0f, 0.8f }; // RGBA (Красный с прозрачностью 80%)
                WriteMemory<GlowColor_t>(glow_scene_object + 0x8, color);
            }
        }
    }
}

DWORD WINAPI CoreExecutionStream(LPVOID lpParam) {
    HMODULE module_instance = static_cast<HMODULE>(lpParam);

    // Бесконечный цикл работы Glow ESP в отдельном потоке
    while (!GetAsyncKeyState(VK_END)) {
        ExecuteGlowESP();
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Частота обновления ~100 Гц
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
