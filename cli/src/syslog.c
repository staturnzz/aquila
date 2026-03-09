#include "device.h"
#include "util.h"
#include "syslog.h"

syslog_info_t *syslog_init(am_device_t *device) {
    if (device == NULL) return NULL;
    syslog_info_t *info = calloc(1, sizeof(syslog_info_t));
    if (info == NULL) return NULL;

    info->data = calloc(1, 0x4000);
    if (info->data == NULL) {
        free(info);
        return NULL;
    }

    info->service = md_open_secure_service(device, "com.apple.syslog_relay");
    if (info->service == NULL) {
        free(info->data);
        free(info);
        return NULL;
    }

    info->device = device;
    info->size = 0x4000;
    return info;
}

void syslog_deinit(syslog_info_t *info) {
    if (info == NULL) return;
    if (info->data != NULL) free(info->data);
    if (info->service != NULL) md_close_secure_service(info->service);
    free(info);
}

char *syslog_get_message(syslog_info_t *info) {
    int status = AMDServiceConnectionReceive(info->service, info->data, info->size-1);
    if (status <= 0 || info->data[0] == '\0') return NULL;
    return info->data;    
}

int syslog_print_logs(syslog_info_t *info) {
    device_info_t *device_info = md_device_info(info->device);
    if (device_info == NULL) return -1;

    while (1) {
        char *data = syslog_get_message(info);
        if (data == NULL) break;

        bool is_warning = (strstr(data, "<Warning>") != NULL);
        bool is_error = (strstr(data, "<Error>") != NULL);
        bool is_debug = (strstr(data, "<Debug>") != NULL);

        char *base = strstr(data, device_info->name);
        if (base == NULL) {
            base = strstr(data, " unknown");
            if (base != NULL) {
                base += strlen(" unknown") + 1;
            }
        } else {
            base += strlen(device_info->name) + 1;
        }

        if (base == NULL) continue;
        char *process = base;
        char *process_end = strstr(process, "[");
        if (process_end == NULL) continue;

        process_end[0] = '\0';
        char *pid = process_end + 1;
        char *pid_end = strstr(pid, "]");
        if (pid_end == NULL) continue;

        pid_end[0] = '\0';
        char *message = strstr(pid_end + 1, ">: ");
        if (message == NULL) continue;
        message += strlen(">: ");

#if defined(MACOS_BUILD)
            if (is_warning) {
                fprintf(stdout, "<\e[1;33mWarning\e[0m:\e[1;35m%s\e[0m>[\e[0;34m%s\e[0m]>: %s", process, pid, message);
            } else if (is_error) {
                fprintf(stdout, "<\e[1;31mError\e[0m:\e[1;35m%s\e[0m[\e[0;34m%s\e[0m]>: %s", process, pid, message);
            } else if (is_debug) {
                fprintf(stdout, "<\e[1;32mDebug\e[0m:\e[1;35m%s\e[0m>[\e[0;34m%s\e[0m]>: %s", process, pid, message);
            } else {
                fprintf(stdout, "<\e[1;36mNotice\e[0m:\e[1;35m%s\e[0m>[\e[0;34m%s\e[0m]>: %s", process, pid, message);
            }
#else
            if (is_warning) {
                fprintf(stdout, "<Warning:%s[%s]>: %s", process, pid, message);
            } else if (is_error) {
                fprintf(stdout, "<Error:%s[%s]>: %s", process, pid, message);
            } else if (is_debug) {
                fprintf(stdout, "<Debug:%s[%s]>: %s", process, pid, message);
            } else {
                fprintf(stdout, "<Notice:%s[%s]>: %s", process, pid, message);
            } 
#endif

    }
    return 0;
}
