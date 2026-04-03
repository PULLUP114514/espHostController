#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "NowMode.h"
#include "F1CUart.h"
void app_main(void)
{
    StartUart();
    StartEspNow();
}
