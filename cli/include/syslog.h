#ifndef aquila_syslog_h
#define aquila_syslog_h

#include "common.h"

typedef struct {
    char *data;
    uint32_t size;
    void *service;
    am_device_t *device;
} syslog_info_t;

syslog_info_t *syslog_init(am_device_t *device);
void syslog_deinit(syslog_info_t *info);
char *syslog_get_message(syslog_info_t *info);
int syslog_print_logs(syslog_info_t *info);

#endif /* aquila_syslog_h */
