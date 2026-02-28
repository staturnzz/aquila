#include "common.h"
#include "util.h"
#include "screen.h"

#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>

void *(*IOSurfaceCreate)(const struct __CFDictionary *) = NULL;
kern_return_t (*IOSurfaceLock)(void *, int, uint32_t *) = NULL;
kern_return_t (*IOSurfaceUnlock)(void *, int, uint32_t *) = NULL;
size_t (*IOSurfaceGetBytesPerRow)(void *) = NULL;
void *(*IOSurfaceGetBaseAddress)(void *) = NULL;

int draw_splash_screen(const char *path) {
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

    CGRect final_frame = CGRectZero;
    final_frame.size.width = size.width;
    final_frame.size.height = size.height;
    final_frame.origin.x = 0;
    final_frame.origin.y = 0;

    uint32_t flags = (kCGImageAlphaPremultipliedFirst | kCGImageByteOrder32Little);
    void *base = IOSurfaceGetBaseAddress(surface);
    size_t bytes = IOSurfaceGetBytesPerRow(surface);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();

    if ((ctx = CGBitmapContextCreate(base, size.width, size.height, 8, bytes, colorspace, flags)) == NULL) goto done;
    CGContextDrawImage(ctx, final_frame, cg_image);
    CGColorSpaceRelease(colorspace);
    rv = 0;

done:
    if (dict != NULL) CFRelease(dict);
    if (ctx != NULL) CGContextRelease(ctx);
    if (cg_image != NULL) CGImageRelease(cg_image);
    if (jp2_data != NULL) munmap(jp2_data, jp2_size);
    if (image_src != NULL) CFRelease(image_src);
    if (surface != NULL) CFRelease(surface);
    if (fd != -1) close(fd);
    return rv;
}
