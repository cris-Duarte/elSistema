#pragma once
#include "FreeRTOS.h"
#include <chrono>
#include <thread>
inline TickType_t xTaskGetTickCount() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void vTaskDelay(TickType_t ticks) {std::this_thread::sleep_for(std::chrono::milliseconds(ticks));}
