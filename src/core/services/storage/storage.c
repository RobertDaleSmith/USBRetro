// storage.c - Storage Subsystem
//
// Unified storage control for persistent settings.

#include "storage.h"
#include "flash.h"

void storage_init(void)
{
    flash_init();
}

void storage_task(void)
{
    flash_task();
}
