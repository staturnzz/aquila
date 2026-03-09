
#include "exploit.h"
#include "util.h"

void *(*__IOSurfaceCreate)(CFDictionaryRef) = NULL;
void *(*__IOSurfaceGetBaseAddress)(void *) = NULL;
int (*__IOServiceOpen)(mach_port_t, mach_port_t, uint32_t, mach_port_t *) = NULL;
CFMutableDictionaryRef (*__IOServiceMatching)(const char *) = NULL;
mach_port_t (*__IOServiceGetMatchingService)(mach_port_t, CFDictionaryRef) = NULL;
int (*__IOMobileFramebufferOpen)(mach_port_t, mach_port_t, uint32_t, void *) = NULL;
int (*__IOMobileFramebufferGetLayerDefaultSurface)(mach_port_t, int, void *) = NULL;

static bool first_log = true;
static FILE *console_file = NULL;

void print_log(const char *fmt, ...) {
    static bool log_opened;
    if (!log_opened) {
        openlog("aquila", LOG_PID | LOG_CONS, LOG_USER);
        log_opened = true;
    }

    if (first_log) {
        unlink("/private/var/mobile/Media/aquila_log.txt");
        first_log = false;
    }

    FILE *log_file = NULL;
    if (access("/private/var/mobile/Media/aquila_log.txt", F_OK) != 0) {
        log_file = fopen("/private/var/mobile/Media/aquila_log.txt", "w+");
    } else {
        log_file = fopen("/private/var/mobile/Media/aquila_log.txt", "a");
    }
    
    va_list va;
    va_start(va, fmt);
    vsyslog(LOG_CRIT, fmt, va);
    vfprintf(stderr, fmt, va);

    if (log_file != NULL) {
        vfprintf(log_file, fmt, va);
        fflush(log_file);
        fclose(log_file);
        sync();
    }

    if (console_file == NULL) {
        if ((console_file = fopen("/dev/console", "a")) == NULL) return;
    }

    if (console_file != NULL) {
        vfprintf(log_file, fmt, va);
        fflush(log_file);
        sync();
    }
    va_end(va);
}

int init_io(void) {
    void *io_handle = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_NOW);
    if (io_handle == NULL) {
        io_handle = dlopen("/System/Library/PrivateFrameworks/IOSurface.framework/IOSurface", RTLD_NOW);
        if (io_handle == NULL) return -1;
    }

    if ((__IOSurfaceCreate = dlsym(io_handle, "IOSurfaceCreate")) == NULL) return -1;
    if ((__IOSurfaceGetBaseAddress = dlsym(io_handle, "IOSurfaceGetBaseAddress")) == NULL) return -1;
    if ((__IOServiceGetMatchingService = dlsym(io_handle, "IOServiceGetMatchingService")) == NULL) return -1;
    if ((__IOServiceMatching = dlsym(io_handle, "IOServiceMatching")) == NULL) return -1;
    if ((__IOServiceOpen = dlsym(io_handle, "IOServiceOpen")) == NULL) return -1;

    void *fb_handle = dlopen("/System/Library/Frameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", RTLD_NOW);
    if (fb_handle == NULL) {
        fb_handle = dlopen("/System/Library/PrivateFrameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", RTLD_NOW);
        if (fb_handle == NULL) return -1;
    }

    if ((__IOMobileFramebufferOpen = dlsym(fb_handle, "IOMobileFramebufferOpen")) == NULL) return -1;
    if ((__IOMobileFramebufferGetLayerDefaultSurface = dlsym(fb_handle, "IOMobileFramebufferGetLayerDefaultSurface")) == NULL) return -1;
    return 0;
}

void get_ios_version(uint32_t *output) {
    char str[32] = {0};
    CFDictionaryRef dict = _CFCopySystemVersionDictionary();
    CFStringRef version = CFDictionaryGetValue(dict, CFSTR("ProductVersion"));
    CFStringGetCString(version, str, 32, kCFStringEncodingUTF8);
    
    sscanf(str, "%d.%d.%d", &output[0], &output[1], &output[2]);
    CFRelease(dict);
}

CFNumberRef CFNUM(uint32_t value) {
    return CFNumberCreate(NULL, kCFNumberIntType, (void *)&value);
}

uint64_t timer_start(void) {
    return mach_absolute_time();
}

uint64_t timer_end(uint64_t start) {
    uint64_t end = mach_absolute_time();
    mach_timebase_info_data_t info = {0};
    mach_timebase_info(&info);
    return (uint64_t)(((double)(end - start) * info.numer / info.denom) / 1000000);
}

int init_offsets(void) {
    get_ios_version(kinfo->version);
    if (kinfo->version[0] < 3 || kinfo->version[0] > 10) return -1;
    
    size_t size = sizeof(kinfo->mem_size);
    sysctlbyname("hw.physmem", &kinfo->mem_size, &size, NULL, 0);
    kinfo->mem_size &= 0xfff00000;

    uint32_t cpu_family = 0;
    size = sizeof(cpu_family);
    sysctlbyname("hw.cpufamily", &cpu_family, &size, NULL, 0);

    if (cpu_family == CPUFAMILY_ARM_14 || cpu_family == CPUFAMILY_ARM_SWIFT) {
        kinfo->kernel_static_base = 0x80001000;
        kinfo->kernel_phys_base = 0x80001000;
        kinfo->mem_base = 0x80000000;
    } else {
        if (kinfo->version[0] == 3 && kinfo->version[1] <= 1) {
            kinfo->kernel_static_base = 0xc0008000;
            kinfo->kernel_phys_base = 0x40008000;
            kinfo->mem_base = 0x40000000;
        } else {
            kinfo->kernel_static_base = 0x80001000;
            kinfo->kernel_phys_base = 0x40001000;
            kinfo->mem_base = 0x40000000;
        }
    }

    switch (kinfo->version[0]) {
        case 10:
            kinfo->offsets.task.ref_count = 0x8;
            kinfo->offsets.task.itk_self = 0x9c;
            kinfo->offsets.task.itk_seatbelt = 0x1c8;
            kinfo->offsets.proc.next = 0x4;
            kinfo->offsets.proc.pid = 0x8;
            kinfo->offsets.proc.task = 0xc;
            kinfo->offsets.proc.lock_type = 0x48;
            kinfo->offsets.proc.p_stat = 0x4c;
            kinfo->offsets.ipc_port.ip_references = 0x4;
            kinfo->offsets.ipc_port.ip_kobject = 0x48;
            break;
        case 9:
            kinfo->offsets.task.ref_count = 0xc;
            kinfo->offsets.task.itk_self = 0xa4;
            kinfo->offsets.task.itk_seatbelt = 0x198;
            kinfo->offsets.proc.next = 0x4;
            kinfo->offsets.proc.pid = 0x8;
            kinfo->offsets.proc.task = 0xc;
            kinfo->offsets.proc.lock_type = 0x4c;
            kinfo->offsets.proc.p_stat = 0x50;
            kinfo->offsets.ipc_port.ip_references = 0x4;
            kinfo->offsets.ipc_port.ip_kobject = 0x50;
            break;
        case 8:
            kinfo->offsets.task.ref_count = 0xc;
            kinfo->offsets.task.itk_self = 0xa4;
            kinfo->offsets.task.itk_seatbelt = 0x188;
            kinfo->offsets.proc.next = 0x4;
            kinfo->offsets.proc.pid = 0x8;
            kinfo->offsets.proc.task = 0xc;
            kinfo->offsets.proc.lock_type = 0x4c;
            kinfo->offsets.proc.p_stat = 0x50;
            kinfo->offsets.ipc_port.ip_references = 0x4;
            kinfo->offsets.ipc_port.ip_kobject = 0x44;
            break;
        case 7:
            kinfo->offsets.task.ref_count = 0xc;
            kinfo->offsets.task.itk_self = 0xa0;
            kinfo->offsets.task.itk_seatbelt = 0x184;
            kinfo->offsets.proc.next = 0x4;
            kinfo->offsets.proc.pid = 0x8;
            kinfo->offsets.proc.task = 0xc;
            kinfo->offsets.proc.lock_type = 0x4c;
            kinfo->offsets.proc.p_stat = 0x50;
            kinfo->offsets.ipc_port.ip_references = 0x4;
            kinfo->offsets.ipc_port.ip_kobject = 0x44;
            break;
        case 6:
            kinfo->offsets.task.ref_count = 0xc;
            kinfo->offsets.task.itk_self = 0x9c;
            kinfo->offsets.task.itk_seatbelt = 0x180;
            kinfo->offsets.proc.next = 0x4;
            kinfo->offsets.proc.pid = 0x8;
            kinfo->offsets.proc.task = 0xc;
            kinfo->offsets.proc.lock_type = 0x44;
            kinfo->offsets.proc.p_stat = 0x48;
            kinfo->offsets.ipc_port.ip_references = 0x4;
            kinfo->offsets.ipc_port.ip_kobject = 0x44;
            break;
        case 5:
            kinfo->offsets.task.ref_count = 0xc;
            kinfo->offsets.task.itk_self = 0x9c;
            kinfo->offsets.task.itk_seatbelt = 0x160;
            kinfo->offsets.proc.next = 0x4;
            kinfo->offsets.proc.pid = 0x8;
            kinfo->offsets.proc.task = 0xc;
            kinfo->offsets.proc.lock_type = 0x44;
            kinfo->offsets.proc.p_stat = 0x48;
            kinfo->offsets.ipc_port.ip_references = 0x4;
            kinfo->offsets.ipc_port.ip_kobject = 0x40;
            break;
        case 4:
            kinfo->offsets.task.ref_count = 0xc;
            kinfo->offsets.task.itk_self = 0x9c;
            kinfo->offsets.task.itk_seatbelt = 0x160;
            kinfo->offsets.proc.next = 0x4;
            kinfo->offsets.proc.pid = 0x8;
            kinfo->offsets.proc.task = 0xc;
            kinfo->offsets.proc.lock_type = 0x44;
            kinfo->offsets.proc.p_stat = 0x48;
            kinfo->offsets.ipc_port.ip_references = 0x4;
            kinfo->offsets.ipc_port.ip_kobject = 0x40;
            break;
        case 3:
            kinfo->offsets.task.ref_count = 0xc;
            kinfo->offsets.task.itk_self = 0x98;
            kinfo->offsets.task.itk_seatbelt = 0x15c;
            kinfo->offsets.proc.next = 0x4;
            kinfo->offsets.proc.pid = 0x8;
            kinfo->offsets.proc.task = 0xc;
            kinfo->offsets.proc.lock_type = 0x44;
            kinfo->offsets.proc.p_stat = 0x48;
            kinfo->offsets.ipc_port.ip_references = 0x0;
            kinfo->offsets.ipc_port.ip_kobject = 0x40;
            break;
        default:
            break;
    }
    return 0;
}
