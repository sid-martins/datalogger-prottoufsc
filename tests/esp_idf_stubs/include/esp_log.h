#pragma once

#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LOGE(tag, format, ...) std::printf("[E][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) std::printf("[W][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) std::printf("[I][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) std::printf("[D][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) std::printf("[V][%s] " format "\n", tag, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

