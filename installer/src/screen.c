#include "common.h"
#include "util.h"
#include "screen.h"
#include "utils.h"

#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>

void *(*IOSurfaceCreate)(const struct __CFDictionary *) = NULL;
kern_return_t (*IOSurfaceLock)(void *, int, uint32_t *) = NULL;
kern_return_t (*IOSurfaceUnlock)(void *, int, uint32_t *) = NULL;
size_t (*IOSurfaceGetBytesPerRow)(void *) = NULL;
void *(*IOSurfaceGetBaseAddress)(void *) = NULL;

pid_t process_get_pid(const char *name) {
    if (name == NULL) return -1;
    char current_name[4*MAXCOMLEN] = {0};
    pid_t target_pid = -1;

    int count = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (count <= 0) return -1;

    uint32_t list_size = sizeof(pid_t) * (count+20);
    pid_t *pid_list = calloc(1, list_size);
    if (pid_list == NULL) return -1;

    count = proc_listpids(PROC_ALL_PIDS, 0, pid_list, list_size);
    for (int i = 0; i < count; i++) {
        int current_pid = pid_list[i];
        if (current_pid <= 1) continue;
        bzero(current_name, sizeof(current_name));
        
        if (proc_name(current_pid, current_name, sizeof(current_name)-1) <= 0) continue;
        if (strncmp(name, current_name, MAXCOMLEN) == 0) {
            target_pid = current_pid;
            break;
        }
    }

    free(pid_list);
    return target_pid;
}

int draw_splash_screen(const char *path) {
    uint32_t version[3] = {0};
    float rotation = 0.0f;

    get_ios_version(&version[0]);
    char *model = get_hw_model();
    if (model != NULL && strstr(model, "iPad") != NULL) rotation = 90.0f;

    void *handle = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_NOW);
    if (handle == NULL) {
        handle = dlopen("/System/Library/PrivateFrameworks/IOSurface.framework/IOSurface", RTLD_NOW);
        if (handle == NULL) return -1;
    }

    if ((IOSurfaceCreate = dlsym(handle, "IOSurfaceCreate")) == NULL) return -1;
    if ((IOSurfaceLock = dlsym(handle, "IOSurfaceLock")) == NULL) return -1;
    if ((IOSurfaceUnlock = dlsym(handle, "IOSurfaceUnlock")) == NULL) return -1;
    if ((IOSurfaceGetBytesPerRow = dlsym(handle, "IOSurfaceGetBytesPerRow")) == NULL) return -1;
    if ((IOSurfaceGetBaseAddress = dlsym(handle, "IOSurfaceGetBaseAddress")) == NULL) return -1;

    IOMobileFramebufferRef display = NULL;
    IOMobileFramebufferDisplaySize size = CGSizeMake(0, 0);
    __block IOSurfaceRef surface = NULL;
    CFMutableDictionaryRef dict = NULL;
    CFDataRef image_data = NULL;
    CGImageSourceRef image_src = NULL;
    CGImageRef cg_image = NULL;
    CGContextRef ctx = NULL;
    void *jp2_data = NULL;
    size_t jp2_size = 0;
    pid_t backboardd_pid = -1;
    pid_t springboard_pid = -1;
    int fd = -1;
    int rv = -1;

    if (IOMobileFramebufferGetMainDisplay(&display) != 0) goto done;
    if (IOMobileFramebufferGetDisplaySize(display, &size) != 0) goto done;
    if (IOMobileFramebufferGetLayerDefaultSurface(display, 0, &surface) != 0) goto done;
    if ((dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL)) == NULL) goto done;

    CFDictionarySetValue(dict, CFSTR("IOSurfaceIsGlobal"), kCFBooleanFalse);
    CFDictionarySetValue(dict, CFSTR("IOSurfaceWidth"), CFNUM(size.width));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceHeight"), CFNUM(size.height));
    CFDictionarySetValue(dict, CFSTR("IOSurfacePixelFormat"), CFNUM(0x42475241));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerElement"), CFNUM(4));
    
    int token = 0;
    CGRect frame = CGRectMake(0, 0, size.width, size.height);
    if ((surface = IOSurfaceCreate(dict)) == NULL) goto done;
    IOSurfaceLock(surface, 0, 0);
    
    if (version[0] == 7) {
        kill(process_get_pid("backboardd"), SIGTERM);
        usleep(500000);

        for (uint32_t i = 0; i < 100; i++) {
            backboardd_pid = process_get_pid("backboardd");
            if (backboardd_pid != -1) break;
            usleep(10000);
        }

        springboard_pid = process_get_pid("SpringBoard");
        if (backboardd_pid != -1) kill(backboardd_pid, SIGSTOP);
        if (springboard_pid != -1) kill(springboard_pid, SIGSTOP);
        usleep(100000);
    }

    if (IOMobileFramebufferSwapBegin(display, &token) != 0) goto done;
    if (IOMobileFramebufferSwapSetLayer(display, 0, surface, frame, frame, 0) != 0) goto done;
    if (IOMobileFramebufferSwapEnd(display) != 0) goto done;
    
    if ((fd = open(path, O_RDONLY)) == -1) goto done;
    jp2_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if ((jp2_data = mmap(NULL, jp2_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) goto done;
    if ((image_data = CFDataCreateWithBytesNoCopy(NULL, jp2_data, jp2_size, kCFAllocatorNull)) == NULL) goto done;
    if ((image_src = CGImageSourceCreateWithData(image_data, NULL)) == NULL) goto done;
    if ((cg_image = CGImageSourceCreateImageAtIndex(image_src, 0, NULL)) == NULL) goto done;

    uint32_t flags = (kCGImageAlphaPremultipliedFirst | kCGImageByteOrder32Little);
    void *base = IOSurfaceGetBaseAddress(surface);
    size_t bytes = IOSurfaceGetBytesPerRow(surface);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();

    if ((ctx = CGBitmapContextCreate(base, size.width, size.height, 8, bytes, colorspace, flags)) == NULL) goto done;
    CGRect final_frame = CGRectZero;

    if (rotation != 0) {
        CGContextClearRect(ctx, CGRectMake(0, 0, size.width, size.height));
        CGContextTranslateCTM(ctx, size.width/2.0, size.height/2.0);
        CGContextRotateCTM(ctx, (rotation * M_PI) / 180.0f);
        
        final_frame.size.width = (rotation == 90 || rotation == 270) ? size.height : size.width;
        final_frame.size.height = (rotation == 90 || rotation == 270) ? size.width : size.height;
        final_frame.origin.x = (-final_frame.size.width) / 2.0;
        final_frame.origin.y = (-final_frame.size.height) / 2.0;
    } else {
        final_frame.size.width = size.width;
        final_frame.size.height = size.height;
        final_frame.origin.x = 0;
        final_frame.origin.y = 0;
    }

    CGContextDrawImage(ctx, final_frame, cg_image);
    CGColorSpaceRelease(colorspace);
    usleep(1000000);
    sync();
    rv = 0;

done:
    if (dict != NULL) CFRelease(dict);
    if (ctx != NULL) CGContextRelease(ctx);
    if (cg_image != NULL) CGImageRelease(cg_image);
    if (jp2_data != NULL) munmap(jp2_data, jp2_size);
    if (image_src != NULL) CFRelease(image_src);
    if (surface != NULL && version[0] != 7) CFRelease(surface);
    if (fd != -1) close(fd);

    if (rv != 0) {
        if (backboardd_pid != -1) kill(backboardd_pid, SIGCONT);
        if (springboard_pid != -1) kill(springboard_pid, SIGCONT);
    }
    return rv;
}
