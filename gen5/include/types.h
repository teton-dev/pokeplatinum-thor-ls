#ifndef GEN5_TYPES_H
#define GEN5_TYPES_H

// pokebw2's types.h for the port: the SDK's fixed-size types (pokebw2's u32 is unsigned long, 64 bits here)

#include <nitro/types.h>
#include <stddef.h>
#include <stdint.h>

// An address held as an integer, which a native port widens to the size of a pointer
typedef uintptr_t uptr;

#ifndef NELEMS
#define NELEMS(array) (sizeof(array) / sizeof((array)[0]))
#endif

#ifndef ATTRIBUTE_ALIGN
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))
#endif

#endif // GEN5_TYPES_H
