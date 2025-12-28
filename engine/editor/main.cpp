#define CHIKA_DEBUG
#include "debug/assert.h"

int main()
{
    int x = 0;
    CHIKA_ASSERT(x != 0);
    // CHIKA_ASSERT(true);
    return 0;
}