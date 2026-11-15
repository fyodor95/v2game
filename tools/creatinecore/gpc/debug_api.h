#pragma once
#include <stdint.h>

#define V2_GPC_DEBUG_VERSION 1u
#define V2_GPC_DEBUG_EXPORT "V2Gpc_Debug"
#define V2_GPC_DEBUG_PREVIEW 1u
#define V2_GPC_DEBUG_SNAPSHOT 2u
#define V2_GPC_DEBUG_RUN 3u

// code=0 означает успех. committed отличает выполненный run от отказа до
// мутаций, в том числе если после commit не удалось закончить запись JSON.
#define V2_GPC_DEBUG_OK 0u
#define V2_GPC_DEBUG_ARGUMENT 1u
#define V2_GPC_DEBUG_GUARD 2u
#define V2_GPC_DEBUG_OUTPUT 3u
#define V2_GPC_DEBUG_NATIVE 4u
#define V2_GPC_DEBUG_FAILED 5u

typedef struct V2GpcDebugRequest {
    uint32_t size;
    uint32_t version;
    uint32_t operation;
    char name[64]; // 1..48 символов [A-Za-z0-9_-], без расширения.
} V2GpcDebugRequest;

typedef struct V2GpcDebugResult {
    uint32_t size;
    uint32_t version;
    uint32_t code;
    uint32_t committed;
    uint32_t pairs;
    uint32_t added;
    uint32_t raw_date;
    uint32_t pass;
    uint32_t rng;
    uint32_t state_digest;
    uint32_t core_digest;
    char path[512]; // UTF-8; новый JSON внутри собственного userdata/gpc-debug.
    char error[96]; // Стабильный ASCII-код, подробности — в runtime log.
} V2GpcDebugResult;

#if defined(_MSC_VER)
#define V2_GPC_DEBUG_CDECL __cdecl
#else
#define V2_GPC_DEBUG_CDECL
#endif

typedef int (V2_GPC_DEBUG_CDECL *V2GpcDebugFn)(const V2GpcDebugRequest*, uint32_t,
                                            V2GpcDebugResult*, uint32_t);
#ifdef __cplusplus
static_assert(sizeof(V2GpcDebugRequest) == 76, "GPC debug request ABI v1");
static_assert(sizeof(V2GpcDebugResult) == 652, "GPC debug result ABI v1");
extern "C" {
#endif

// Возвращает 1 для корректно скопированного result (проверить code/committed).
// 0 означает неверный caller buffer/размер, не подтверждённый исход команды.
int V2_GPC_DEBUG_CDECL V2Gpc_Debug(const V2GpcDebugRequest* request, uint32_t request_size,
                                V2GpcDebugResult* output, uint32_t output_size);
#ifdef __cplusplus
}
#endif
