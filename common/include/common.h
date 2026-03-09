#ifndef common_h
#define common_h

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/dir.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>
#include <device/device_types.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <spawn.h>

typedef uint32_t io_object_t;
typedef io_object_t io_connect_t;
typedef io_object_t io_service_t;
typedef io_object_t io_registry_entry_t;
typedef uint32_t IOSurfaceLockOptions;
typedef struct __IOSurface * IOSurfaceRef;
typedef uint32_t IOOptionBits;

extern char **environ;
extern void vsyslog(int, const char *, __darwin_va_list);

#endif /* common_h */