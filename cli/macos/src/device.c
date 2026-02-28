#include "device.h"
#include "util.h"

static am_device_notification_t *md_notification = NULL;
static bool wait_for_device = false;
static am_device_t *last_device = NULL;

static bool is_wireless_device(am_device_t *device) {
    if (device == NULL) return false;
    md_interface_type_t type = AMDeviceGetInterfaceType(device);
    if (type == INTERFACE_TYPE_WIRELESS) return true;

    if (type == INTERFACE_TYPE_PROXY) {
        am_device_t *companion_device = AMDeviceCopyPairedCompanion(device);
        if (companion_device == NULL) return false;

        type = AMDeviceGetInterfaceType(companion_device);
        AMDeviceRelease(companion_device);
        if (type == INTERFACE_TYPE_WIRELESS) return true;
    }
    return false;
}

static void md_connect_handler(am_device_notification_callback_info_t *info, int cookie) {
    if (info->msg == ADNCI_MSG_CONNECTED) {
        if (is_wireless_device(info->dev)) {
            print_log(VERBOSE, "found wireless device, skipping...\n");
            return;
        }
        
        int err = AMDeviceConnect(info->dev);
        if (err != 0) {
            print_log(VERBOSE, "AMDeviceConnect failed: %d\n", err);
            return;
        }

        err = AMDeviceIsPaired(info->dev);
        if (err != 1) {
            print_log(VERBOSE, "AMDeviceIsPaired failed: %d\n", err);
            return;
        }

        err = AMDeviceValidatePairing(info->dev);
        if (err != 0) {
            print_log(VERBOSE, "AMDeviceValidatePairing failed: %d\n", err);
            return;
        }

        err = AMDeviceStartSession(info->dev);
        if (err != 0) {
            print_log(VERBOSE, "AMDeviceStartSession failed: %d\n", err);
            return;
        }

        last_device = info->dev;
        print_log(VERBOSE, "device connection: %p\n", last_device);

        if (wait_for_device) {
            CFRunLoopStop(CFRunLoopGetCurrent());
            wait_for_device = false;
        }
    }
}

int md_init(void) {
    if (has_flag(FLAG_VERBOSE_LOGGING)) {
        setenv("AFCDEBUG", "1", true);
        AFCSetLogLevel(6);
        AFCPlatformInitialize();
        AMDSetLogLevel(5);
    }

    int err = AMDeviceNotificationSubscribe(md_connect_handler, 0, 0, 0, &md_notification);
    if (err != 0) {
        print_log(VERBOSE, "AMDeviceNotificationSubscribe failed: %d\n", err);
        return -1;
    }
    return 0;
}

void md_deinit(void) {
    if (md_notification != NULL) {
        AMDeviceNotificationUnsubscribe(md_notification);
        md_notification = NULL;
    }

    CFRunLoopStop(CFRunLoopGetCurrent());
    last_device = NULL;
    wait_for_device = false;
}

am_device_t *md_await_device(void) {
    wait_for_device = true;
    CFRunLoopRun();
    return last_device;
}

char *md_get_device_value(am_device_t *device, const char *key) {
    print_log(VERBOSE, "AMDeviceCopyValue: %s\n", key);
    CFStringRef cf_key = CFStringCreateWithCString(NULL, key, kCFStringEncodingASCII);
    CFStringRef value = (CFStringRef)AMDeviceCopyValue(device, 0, cf_key);
    CFRelease(cf_key);

    if (value == NULL) return NULL;
    if (CFGetTypeID(value) != CFStringGetTypeID()) {
        CFRelease(value);
        return NULL;
    }

    CFIndex len = CFStringGetLength(value);
    if (len == 0) {
        CFRelease(value);
        return NULL;
    }

    char *buf = calloc(1, len+2);
    CFStringGetCString(value, buf, len+1, kCFStringEncodingUTF8);
    CFRelease(value);
    return buf;
}

device_info_t *md_device_info(am_device_t *device) {
    char *cpu_arch = NULL;
    char *hw_model = NULL;
    char *uuid = NULL;
    char *product_type = NULL;
    char *version = NULL;
    char *activation = NULL;

    if ((cpu_arch = md_get_device_value(device, "CPUArchitecture")) == NULL) goto err;
    if ((hw_model = md_get_device_value(device, "HardwareModel")) == NULL) goto err;
    if ((uuid = md_get_device_value(device, "UniqueDeviceID")) == NULL) goto err;
    if ((product_type = md_get_device_value(device, "ProductType")) == NULL) goto err;
    if ((version = md_get_device_value(device, "ProductVersion")) == NULL) goto err;
    if ((activation = md_get_device_value(device, "ActivationState")) == NULL) goto err;

    device_info_t *info = calloc(1, sizeof(device_info_t));
    info->activation = activation;
    info->cpu_arch = cpu_arch;
    info->hw_model = hw_model;
    info->uuid = uuid;
    info->product_type = product_type;

    sscanf(version, "%d.%d.%d", &info->version[0], &info->version[1], &info->version[2]);
    free(version);
    version = NULL;
    return info;

err:
    if (cpu_arch != NULL) free(cpu_arch);
    if (hw_model != NULL) free(hw_model);
    if (uuid != NULL) free(uuid);
    if (product_type != NULL) free(product_type);
    if (version != NULL) free(version);
    return NULL;
}

int md_open_service(am_device_t *device, const char *name, bool timeout) {
    print_log(VERBOSE, "AMDeviceStartService: %s\n", name);
    CFStringRef cf_name = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);
    int service = -1;
    int rv = -1;

    if (timeout) {
        CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
        CFDictionaryAddValue(dict, CFSTR("TimeoutConnection"), kCFBooleanTrue);
        CFDictionaryAddValue(dict, CFSTR("CloseOnInvalidate"), kCFBooleanTrue);
        rv = AMDeviceStartServiceWithOptions(device, cf_name, dict, &service);
        CFRelease(dict);
    } else {
        rv = AMDeviceStartService(device, cf_name, &service);
    }
    
    CFRelease(cf_name);
    return (rv == 0) ? service : -1;
}

void *md_open_secure_service(am_device_t *device, const char *name) {
    print_log(VERBOSE, "AMDeviceSecureStartService: %s\n", name);
    CFStringRef cf_name = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);
    void *service = NULL;

    int rv = AMDeviceSecureStartService(device, cf_name, NULL, &service);
    CFRelease(cf_name);
    return (rv == 0) ? service : NULL;
}

void md_close_service(int service) {
    if (service >= 0) close(service);
}

int md_reboot_device(am_device_t *device) {
    void *service = md_open_secure_service(device, "com.apple.mobile.diagnostics_relay");
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(dict, CFSTR("Request"), CFSTR("Restart"));

    AMDServiceConnectionSendMessage(service, dict, kCFPropertyListXMLFormat_v1_0);
    CFRelease(dict);

    CFDictionaryRef status = NULL;
    AMDServiceConnectionReceiveMessage(service, &status, NULL);

    dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(dict, CFSTR("Request"), CFSTR("Goodbye"));
    AMDServiceConnectionSendMessage(service, dict, kCFPropertyListXMLFormat_v1_0);
    CFRelease(dict);
    return 0;
}
