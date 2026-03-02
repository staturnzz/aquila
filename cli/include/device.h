#ifndef device_h
#define device_h

#include "common.h"

#define ADNCI_MSG_CONNECTED         1
#define ADNCI_MSG_DISCONNECTED      2
#define ADNCI_MSG_UNSUBSCRIBED      3

typedef uint32_t service_conn_t;

typedef enum {
    INTERFACE_TYPE_UNKNOWN = 0,
    INTERFACE_TYPE_WIRED,
    INTERFACE_TYPE_WIRELESS,
    INTERFACE_TYPE_PROXY
} md_interface_type_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t __unk0[16];
    uint32_t device_id;
    uint32_t product_id;
    char *serial;
    uint32_t __unk1;
    uint32_t __unk2;
    uint32_t lockdown_conn;
    uint8_t __unk3[8];
    uint32_t __unk4;
    uint8_t __unk5[24];
} am_device_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint32_t __unk0;
    uint32_t __unk1;
    uint32_t __unk2;
    void *callback;
    uint32_t cookie;
} am_device_notification_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    am_device_t *dev;
    uint32_t msg;
    am_device_notification_t *subscription;
} am_device_notification_callback_info_t;
#pragma pack(pop)

typedef struct {
    char *cpu_arch;
    char *hw_model;
    char *uuid;
    char *product_type;
    char *activation;
    int version[3];
} device_info_t;


#if defined(WINDOWS_BUILD)
extern int (*AMDeviceNotificationSubscribe)(void *callback, uint32_t __unk0, uint32_t __unk1, uint32_t cookie, am_device_notification_t **subscription);
extern int (*AMDeviceNotificationUnsubscribe)(am_device_notification_t* subscription);
extern int (*AMDeviceConnect)(am_device_t *device);
extern int (*AMDeviceIsPaired)(am_device_t *device);
extern int (*AMDevicePair)(am_device_t *device);
extern int (*AMDeviceValidatePairing)(am_device_t *device);
extern int (*AMDeviceStartSession)(am_device_t *device);
extern void *(*AMDeviceCopyValue)(am_device_t *device, CFStringRef domain, CFStringRef cfstring);
extern int (*AMDeviceStartService)(am_device_t *device, CFStringRef service_name, int *socket_fd);
extern int (*AMDeviceStartServiceWithOptions)(am_device_t *device, CFStringRef service_name, CFDictionaryRef options, int *socket_fd);
extern int (*AMDeviceStopSession)(am_device_t *device);
extern int (*USBMuxConnectByPort)(int connectionID, int iPhone_port_network_byte_order, int* outHandle);
extern int (*AMDeviceGetConnectionID)(am_device_t *device);
extern int (*AMDeviceSecureStartService)(am_device_t *device, CFStringRef service, CFDictionaryRef flags, void *handle);
extern int (*AMDServiceConnectionReceiveMessage)(void *service, CFPropertyListRef message, CFPropertyListFormat *format);
extern int (*AMDServiceConnectionReceive)(void *service, char *buf, size_t size);
extern int (*AMDServiceConnectionSendMessage)(void *service, CFPropertyListRef message, CFPropertyListFormat format);
extern int (*AMDServiceConnectionSend)(void *service, const void *message, size_t length);
extern int (*AMDeviceLookupApplications)(am_device_t *device, CFDictionaryRef options, CFDictionaryRef *result);
#else
extern int AMDeviceNotificationSubscribe(void *callback, uint32_t __unk0, uint32_t __unk1, uint32_t cookie, am_device_notification_t **subscription);
extern int AMDeviceNotificationUnsubscribe(am_device_notification_t* subscription);
extern int AMDeviceConnect(am_device_t *device);
extern int AMDeviceIsPaired(am_device_t *device);
extern int AMDevicePair(am_device_t *device);
extern int AMDeviceValidatePairing(am_device_t *device);
extern int AMDeviceStartSession(am_device_t *device);
extern void *AMDeviceCopyValue(am_device_t *device, CFStringRef domain, CFStringRef cfstring);
extern mach_error_t AMDeviceStartService(am_device_t *device, CFStringRef service_name, int *socket_fd);
extern mach_error_t AMDeviceStartServiceWithOptions(am_device_t *device, CFStringRef service_name, CFDictionaryRef options, int *socket_fd);
extern mach_error_t AMDeviceStopSession(am_device_t *device);
extern int AMDeviceSecureStartService(am_device_t *device, CFStringRef service, CFDictionaryRef flags, void *handle);
extern int AMDServiceConnectionReceiveMessage(void *service, CFPropertyListRef message, CFPropertyListFormat *format);
extern int AMDServiceConnectionSendMessage(void *service, CFPropertyListRef message, CFPropertyListFormat format);
extern md_interface_type_t AMDeviceGetInterfaceType(am_device_t *device);
extern am_device_t *AMDeviceCopyPairedCompanion(am_device_t *device);
extern mach_error_t AMDeviceLookupApplications(am_device_t *device, CFDictionaryRef options, CFDictionaryRef *result);
extern mach_error_t AMDeviceRelease(am_device_t *device);
extern void AMDSetLogLevel(int level);
extern void AFCSetLogLevel(int level);
extern void AFCPlatformInitialize(void);
extern void USBMuxListenerSetDebug(int level);
extern char *AMDErrorString(uint32_t err);
extern char *AFCErrorString(uint32_t err);
#endif


int md_init(void);
void md_deinit(void);
am_device_t *md_await_device(void);
device_info_t *md_device_info(am_device_t *device);
int md_open_service(am_device_t *device, const char *name, bool timeout);
void *md_open_secure_service(am_device_t *device, const char *name);
void md_close_service(int service);
int md_reboot_device(am_device_t *device);

#endif /* device_h */