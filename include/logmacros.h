#ifndef _LOGMACROS_H
#define _LOGMACROS_H

#include "spdlog/spdlog.h"

#define LOGD(format, ...)                                                      \
  do {                                                                         \
    spdlog::debug(format, ##__VA_ARGS__);                                      \
  } while (0)
#define LOGI(format, ...)                                                      \
  do {                                                                         \
    spdlog::info(format, ##__VA_ARGS__);                                       \
  } while (0)
#define LOGW(format, ...)                                                      \
  do {                                                                         \
    spdlog::warn(format, ##__VA_ARGS__);                                       \
  } while (0)
#define LOGE(format, ...)                                                      \
  do {                                                                         \
    spdlog::error(format, ##__VA_ARGS__);                                      \
  } while (0)
#define LOGF(format, ...)                                                      \
  do {                                                                         \
    spdlog::critical(format, ##__VA_ARGS__);                                   \
  } while (0)

// this assumes core1 - the "Arduino Core"
#define MEMREPORT(tag)                                                         \
  LOGD("{}: core {}, stack usage {:.1F}%, {} bytes used, heap usage {:.1F}%, " \
       "{} bytes free",                                                        \
       tag, xPortGetCoreID(),                                                  \
       100.0 * (float)uxTaskGetStackHighWaterMark(NULL) /                      \
           (float)ARDUINO_LOOP_STACK_SIZE,                                     \
       (uint32_t)uxTaskGetStackHighWaterMark(NULL),                            \
       100.0 * (((float)ESP.getHeapSize() - (float)ESP.getFreeHeap()) /        \
                (float)ESP.getHeapSize()),                                     \
       ESP.getFreeHeap())

#endif