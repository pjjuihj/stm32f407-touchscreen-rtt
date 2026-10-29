#include "iot.h"

#include <stddef.h>

_Static_assert(sizeof(IoT_Status) == 44U, "IoT_Status must match recovered ABI layout");
_Static_assert(sizeof(IoT_Command) == 8U, "IoT_Command must use byte enums and aligned param");
_Static_assert(offsetof(IoT_Command, module) == 0U, "module offset mismatch");
_Static_assert(offsetof(IoT_Command, action) == 1U, "action offset mismatch");
_Static_assert(offsetof(IoT_Command, param) == 4U, "param offset mismatch");

int main(void)
{
    return 0;
}
