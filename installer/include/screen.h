#ifndef screen_h
#define screen_h

#include "common.h"

#define PROC_ALL_PIDS               1
#define PROC_PIDPATHINFO            11
#define PROC_PIDPATHINFO_SIZE       (MAXPATHLEN)
#define PROC_PIDPATHINFO_MAXSIZE    (4*MAXPATHLEN)

typedef kern_return_t IOReturn;
typedef IOReturn IOMobileFramebufferReturn;
typedef struct __IOMobileFramebuffer *IOMobileFramebufferRef;
typedef CGSize IOMobileFramebufferDisplaySize;

extern IOReturn IOMobileFramebufferGetMainDisplay(IOMobileFramebufferRef *);
extern IOReturn IOMobileFramebufferGetDisplaySize(IOMobileFramebufferRef, IOMobileFramebufferDisplaySize *);
extern IOReturn IOMobileFramebufferGetLayerDefaultSurface(IOMobileFramebufferRef, int, IOSurfaceRef *);
extern IOReturn IOMobileFramebufferSwapBegin(IOMobileFramebufferRef, int *);
extern IOReturn IOMobileFramebufferSwapEnd(IOMobileFramebufferRef);
extern IOReturn IOMobileFramebufferSwapSetLayer(IOMobileFramebufferRef, int, IOSurfaceRef, CGRect, CGRect, int);
extern IOReturn IOMobileFramebufferOpen(mach_port_t, mach_port_t, uint32_t, void *);

extern int csops(pid_t pid, uint32_t ops, void *addr, size_t size);
extern int proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize);
extern int proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer, int buffersize);
extern int proc_pidpath(int pid, void * buffer, uint32_t  buffersize);
extern int proc_name(int pid, void * buffer, uint32_t buffersize);

int draw_splash_screen(const char *path);

#endif /* screen_h */