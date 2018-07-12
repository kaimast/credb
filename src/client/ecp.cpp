#include "util/ecp.h"

#include <cstring>

errno_t memcpy_s(void *dest, size_t numberOfElements, const void *src, size_t count)
{
    if(numberOfElements < count)
    {
        return -1;
    }

    memcpy(dest, src, count);
    return 0;
}
