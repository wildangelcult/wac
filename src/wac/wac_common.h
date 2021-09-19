#ifndef __WAC_COMMON_H
#define __WAC_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef WAC_DEBUG_ALL
#define WAC_DEBUG_PRINT_CODE
#define WAC_DEBUG_TRACE_EXEC
#define WAC_DEBUG_STACK_CHECK
#define WAC_DEBUG_GC_STRESS
#define WAC_DEBUG_GC_LOG
#endif //WAC_DEBUG_ALL

#endif //__WAC_COMMON_H
