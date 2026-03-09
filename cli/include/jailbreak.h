#ifndef jailbreak_h
#define jailbreak_h

#include "common.h"
#include "device.h"
#include "afc.h"

#define SERVICE_HOUSEKEEPING "haxx.aquila.housekeeping"
#define SERVICE_REMOUNT "haxx.aquila.remount"
#define SERVICE_AFC2 "haxx.aquila.afc2"

bool is_jailbroken(afc_info_t *afc_info);
int jailbreak(am_device_t *device, device_info_t *device_info, afc_info_t *afc_info);

#endif /* jailbreak_h */
