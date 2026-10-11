#pragma once
inline void audio_test_log(const char*, const char*, ...) {}
#define ESP_LOGW(...) audio_test_log(__VA_ARGS__)
#define ESP_LOGI(...) audio_test_log(__VA_ARGS__)
