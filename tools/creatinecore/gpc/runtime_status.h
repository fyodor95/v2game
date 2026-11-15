#pragma once

#include <stdint.h>

// C ABI диагностики: DTO не содержит указателей, bool, size_t и padding ABI.
// Нулевая дата означает отсутствие завершённого/наблюдавшегося callback.
#define V2_GPC_RUNTIME_STATUS_VERSION 1u
#define V2_GPC_RUNTIME_STATUS_EXPORT "V2Gpc_GetRuntimeStatus"

typedef struct V2GpcRuntimeStatus {
    uint32_t size;
    uint32_t version;
    uint32_t enabled;
    uint32_t failed;
    uint32_t busy;
    uint32_t has_schedule;
    uint32_t last_completed_raw_date;
    uint32_t last_callback_raw_date;
    uint32_t pass;
    uint32_t rng;
    uint32_t core_digest;
    uint32_t state_digest;
} V2GpcRuntimeStatus;

#if defined(_MSC_VER)
#define V2_GPC_STATUS_CDECL __cdecl
#else
#define V2_GPC_STATUS_CDECL
#endif

typedef int (V2_GPC_STATUS_CDECL *V2GpcGetRuntimeStatusFn)(
    V2GpcRuntimeStatus* output, uint32_t output_size);

#ifdef __cplusplus
static_assert(sizeof(V2GpcRuntimeStatus) == 48, "GPC runtime status ABI v1");
extern "C" {
#endif

// 1: целый snapshot скопирован; 0: неверный output/размер. При 0 результат
// использовать нельзя. Getter не читает движок и не выполняет действий GPC.
int V2_GPC_STATUS_CDECL V2Gpc_GetRuntimeStatus(
    V2GpcRuntimeStatus* output, uint32_t output_size);

#ifdef __cplusplus
}
#endif
