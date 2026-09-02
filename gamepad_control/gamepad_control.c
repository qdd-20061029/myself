#include <windows.h>
#include <XInput.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>  // 用于 _kbhit()

// 手柄状态结构体
typedef struct {
    BOOL connected;          // 是否连接
    XINPUT_STATE state;      // 当前状态
    XINPUT_STATE lastState;  // 上一帧状态
} GamepadState;

// 定义死区阈值 (XInput 推荐值)
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 7849
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30

// 全局变量：最多支持 4 个手柄
static GamepadState g_Gamepads[XUSER_MAX_COUNT];

// 初始化手柄状态
void init_gamepad_states() {
    for (int i = 0; i < XUSER_MAX_COUNT; i++) {
        g_Gamepads[i].connected = FALSE;
        ZeroMemory(&g_Gamepads[i].state, sizeof(XINPUT_STATE));
        ZeroMemory(&g_Gamepads[i].lastState, sizeof(XINPUT_STATE));
    }
}

// 检测所有手柄的连接状态
void detect_gamepads() {
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(XINPUT_STATE));
        
        DWORD result = XInputGetState(i, &state);
        
        if (result == ERROR_SUCCESS) {
            if (!g_Gamepads[i].connected) {
                printf("[系统] 手柄 %d 已连接\n", i + 1);
                g_Gamepads[i].connected = TRUE;
            }
            // 保存当前状态用于比较
            g_Gamepads[i].lastState = g_Gamepads[i].state;
            g_Gamepads[i].state = state;
        } else {
            if (g_Gamepads[i].connected) {
                printf("[系统] 手柄 %d 已断开\n", i + 1);
                g_Gamepads[i].connected = FALSE;
                ZeroMemory(&g_Gamepads[i].state, sizeof(XINPUT_STATE));
                ZeroMemory(&g_Gamepads[i].lastState, sizeof(XINPUT_STATE));
            }
        }
    }
}

// 应用死区过滤
SHORT apply_deadzone(SHORT value, SHORT deadzone) {
    if (value > deadzone || value < -deadzone) {
        return value;
    }
    return 0;
}

// 处理单个手柄的事件
void handle_gamepad_input(int index) {
    GamepadState *gp = &g_Gamepads[index];
    if (!gp->connected) return;
    
    XINPUT_GAMEPAD *gamepad = &gp->state.Gamepad;
    XINPUT_GAMEPAD *last = &gp->lastState.Gamepad;
    
    // --- 检测按键变化 ---
    WORD changedButtons = gamepad->wButtons ^ last->wButtons;
    if (changedButtons != 0) {
        for (int bit = 0; bit < 16; bit++) {
            WORD mask = 1 << bit;
            if (changedButtons & mask) {
                BOOL pressed = (gamepad->wButtons & mask) != 0;
                // 按键名称映射
                const char* name = "未知";
                switch (mask) {
                    case XINPUT_GAMEPAD_DPAD_UP: name = "方向键上"; break;
                    case XINPUT_GAMEPAD_DPAD_DOWN: name = "方向键下"; break;
                    case XINPUT_GAMEPAD_DPAD_LEFT: name = "方向键左"; break;
                    case XINPUT_GAMEPAD_DPAD_RIGHT: name = "方向键右"; break;
                    case XINPUT_GAMEPAD_START: name = "Start"; break;
                    case XINPUT_GAMEPAD_BACK: name = "Back"; break;
                    case XINPUT_GAMEPAD_LEFT_THUMB: name = "左摇杆按下"; break;
                    case XINPUT_GAMEPAD_RIGHT_THUMB: name = "右摇杆按下"; break;
                    case XINPUT_GAMEPAD_LEFT_SHOULDER: name = "LB"; break;
                    case XINPUT_GAMEPAD_RIGHT_SHOULDER: name = "RB"; break;
                    case XINPUT_GAMEPAD_A: name = "A"; break;
                    case XINPUT_GAMEPAD_B: name = "B"; break;
                    case XINPUT_GAMEPAD_X: name = "X"; break;
                    case XINPUT_GAMEPAD_Y: name = "Y"; break;
                }
                printf("[手柄 %d 按键] %s: %s\n", index + 1, name, pressed ? "按下" : "释放");
            }
        }
    }
    
    // --- 检测摇杆变化 (带死区过滤) ---
    SHORT lx = apply_deadzone(gamepad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    SHORT ly = apply_deadzone(gamepad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    SHORT rx = apply_deadzone(gamepad->sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    SHORT ry = apply_deadzone(gamepad->sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    
    static SHORT last_lx[XUSER_MAX_COUNT] = {0}, last_ly[XUSER_MAX_COUNT] = {0};
    static SHORT last_rx[XUSER_MAX_COUNT] = {0}, last_ry[XUSER_MAX_COUNT] = {0};
    
    if (lx != last_lx[index] || ly != last_ly[index]) {
        printf("[手柄 %d 左摇杆] X: %6d, Y: %6d\n", index + 1, lx, ly);
        last_lx[index] = lx;
        last_ly[index] = ly;
    }
    
    if (rx != last_rx[index] || ry != last_ry[index]) {
        printf("[手柄 %d 右摇杆] X: %6d, Y: %6d\n", index + 1, rx, ry);
        last_rx[index] = rx;
        last_ry[index] = ry;
    }
    
    // --- 检测扳机 (0-255) ---
    BYTE lt = gamepad->bLeftTrigger;
    BYTE rt = gamepad->bRightTrigger;
    static BYTE last_lt[XUSER_MAX_COUNT] = {0}, last_rt[XUSER_MAX_COUNT] = {0};
    
    if (lt != last_lt[index]) {
        printf("[手柄 %d 左扳机] %d\n", index + 1, lt);
        last_lt[index] = lt;
    }
    
    if (rt != last_rt[index]) {
        printf("[手柄 %d 右扳机] %d\n", index + 1, rt);
        last_rt[index] = rt;
    }
}

// 主程序
int main() {
    printf("Windows 手柄检测程序 (XInput API)\n");
    printf("====================================\n");
    printf("支持: Xbox 360, Xbox One 及兼容手柄\n");
    printf("最多支持 4 个手柄\n\n");
    
    init_gamepad_states();
    
    printf("正在检测手柄...\n");
    detect_gamepads();
    
    printf("\n开始监听手柄输入...\n");
    printf("按 Q 键退出程序\n\n");
    
    while (1) {
        // 检测键盘输入 (退出)
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                break;
            }
        }
        
        // 更新手柄状态
        detect_gamepads();
        
        // 处理每个已连接的手柄
        for (int i = 0; i < XUSER_MAX_COUNT; i++) {
            if (g_Gamepads[i].connected) {
                handle_gamepad_input(i);
            }
        }

        // 简单的帧率控制
        Sleep(20); // 20ms ~ 50fps
    }
    
    printf("\n程序已退出。\n");
    return 0;
}