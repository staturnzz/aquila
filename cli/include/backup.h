#ifndef aquila_backup_h
#define aquila_backup_h

#include "common.h"

CFTypeRef mb_send_msg(void *service, CFTypeRef data);
void *mb_open_service(am_device_t *device);
void create_manifest_file(CFMutableDictionaryRef file_list, CFStringRef path, uint32_t uid, uint32_t gid);
CFDictionaryRef create_backup_manifest(am_device_t *device);
CFArrayRef create_backup_payload(am_device_t *device);
CFArrayRef create_file_payload(CFStringRef path, CFStringRef target);
int send_backup_payload(am_device_t *device, void *service);
int send_file_payload(am_device_t *device, void *service, const char *path, const char *target);

#endif /* aquila_backup_h */
