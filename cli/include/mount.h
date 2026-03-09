#ifndef mount_h
#define mount_h

#include "common.h"
#include "afc.h"
#include "device.h"

typedef struct {
    am_device_t *device;
    afc_info_t *afc_info;
    device_info_t *device_info;
#if defined(WINDOWS_BUILD)
    HANDLE log_thread;
#else
    pthread_t log_thread;
#endif
} mount_info_t;

int mount_dmg_payload(am_device_t *device, device_info_t *device_info, afc_info_t *afc_info, int *disk);

#endif /* mount_h */
