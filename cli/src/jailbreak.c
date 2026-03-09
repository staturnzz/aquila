#include "common.h"
#include "device.h"
#include "afc.h"
#include "mount.h"
#include "jailbreak.h"
#include "util.h"

bool is_jailbroken(afc_info_t *afc_info) {
    int service = md_open_service(afc_info->device, "com.apple.afc2", false);
    if (service != -1) {
#if defined(MACOS_BUILD)
        close(service);
#endif
        return true;
    }

    service = md_open_service(afc_info->device, "haxx.aquila.afc2", false);
    if (service != -1) {
#if defined(MACOS_BUILD)
        close(service);
#endif
        return true;
    }

    CFDictionaryRef dict = NULL;
    AMDeviceLookupApplications(afc_info->device, 0, &dict);
    if (dict != NULL) {
        CFDictionaryRef cydia = CFDictionaryGetValue(dict, CFSTR("com.saurik.Cydia"));
        if (cydia != NULL) return true;
    }
    return false;
}

int remount_rootfs(afc_info_t *afc_info) {
    afc_file_ref_t test_file = NULL;
    if ((test_file = afc_create_file(afc_info, "/.test_file")) != NULL) {
        print_log(VERBOSE, "rootfs already remounted\n");

        afc_close_file(afc_info, test_file);
        afc_delete_item(afc_info, "/.test_file");
        return 0;
    }

    md_open_service(afc_info->device, SERVICE_REMOUNT, true);
    usleep(100000);

    if ((test_file = afc_create_file(afc_info, "/.test_file")) != NULL) {
        afc_close_file(afc_info, test_file);
        afc_delete_item(afc_info, "/.test_file");
        return 0;
    }
    return -1;
}

int jailbreak(am_device_t *device, device_info_t *device_info, afc_info_t *afc_info) {
    print_log(INFO, "starting jailbreak...\n");
    if (is_jailbroken(afc_info)) {
        if (has_flag(FLAG_FORCE_INSTALL)) {
            print_log(WARNING, "device already jailbroken, continuing anyways\n");
        } else {
            print_log(INFO, "device already jailbroken\n");
            return 0;
        }
    }

    print_log(INFO, "uploading dmg payload...\n");
    int mount_disk = -1;
    if (mount_dmg_payload(device, device_info, afc_info, &mount_disk) != 0) {
        print_log(ERROR, "failed to upload dmg payload\n");
        return -1;
    }

    if (device_info->version[0] == 7) {
        if (mount_disk == -1) {
            print_log(ERROR, "failed to upload dmg payload\n");
            return -1;
        }

        char cache_service[128] = {0};
        char lib_service[128] = {0};
        snprintf(cache_service, sizeof(cache_service)-1, "com.apple.mount_%d", mount_disk);
        snprintf(lib_service, sizeof(lib_service)-1, "com.apple.union_%d", mount_disk);

        print_log(INFO, "mounting /dev/disk%ds3 at /System/Library/Caches...\n", mount_disk);
        if (md_open_secure_service(device, cache_service) == NULL) {
            print_log(ERROR, "failed to mount /dev/disk%ds3\n", mount_disk);
            return -1;
        }

        print_log(INFO, "union mounting /dev/disk%ds2 at /usr/lib...\n", mount_disk);
        if (md_open_secure_service(device, lib_service) == NULL) {
            print_log(ERROR, "failed to mount /dev/disk%ds2\n", mount_disk);
            return -1;
        }

        print_log(INFO, "uploading jailbreak files...\n");
        afc_delete_item(afc_info, "aquila");
        afc_create_dir(afc_info, "aquila");

        afc_upload_embed_file(afc_info, "__bootstrap", "aquila/bootstrap.tar");
        afc_upload_embed_file(afc_info, "__truststore", "aquila/truststore.tar");
        afc_upload_embed_file(afc_info, "__splashscreen", "aquila/splashscreen.jp2");
        afc_upload_embed_file(afc_info, "__aquila", "aquila/aquila");
        afc_upload_embed_file(afc_info, "__amfi_7", "aquila/amfi_bypass.dylib");
        if (has_flag(FLAG_FORCE_INSTALL)) afc_close_file(afc_info, afc_create_file(afc_info, "aquila/.force_install"));

        for (uint32_t i = 0; i < 5; i++) {
            md_open_secure_service(device, "com.apple.unload_amfid"); usleep(50000);
            md_open_service(device, "com.apple.unload_amfid", true); usleep(50000);
            md_open_secure_service(device, "com.apple.unload_amfid"); usleep(50000);
            usleep(100000);

            md_open_secure_service(device, "com.apple.load_amfid"); usleep(50000);
            md_open_service(device, "com.apple.load_amfid", true); usleep(50000);
            md_open_secure_service(device, "com.apple.load_amfid"); usleep(50000);
            usleep(100000);

            md_open_service(device, "com.apple.haxx", true);
            usleep(100000);
        }

        print_log(INFO, "installing jailbreak files...\n");
        void *service = md_open_secure_service(device, "com.apple.aquila_install");
        if (service == NULL) return -1;
        usleep(5000000);

        print_log(WARNING, "your device should now show the 'installing jailbreak' screen\n");
        print_log(WARNING, "if you don't see this screen within 15 seconds, reboot and try again\n");
        while (1) {
            afc_info_t *afc_test = afc_init(device);
            if (afc_test == NULL) break;
            
            afc_deinit(afc_test);
            usleep(2000000);
        }
    } else {
        afc_info_t *afc2_info = afc2_init(device);
        if (afc2_info == NULL) {
            print_log(ERROR, "failed to connect to AFC2\n");
            return -1;
        }

        print_log(INFO, "remounting rootfs...\n");
        if (remount_rootfs(afc2_info) != 0) {
            print_log(ERROR, "failed to remount rootfs\n");
            return -1;
        }

        print_log(INFO, "rootfs remounted\n");
        print_log(INFO, "uploading jailbreak files...\n");
        afc_delete_item(afc2_info, "/private/var/aquila");
        afc_create_dir(afc2_info, "/private/var/aquila");

        afc_delete_item(afc2_info, "/sbin/reboot");
        afc_delete_item(afc2_info, "/Library/Logs/CrashReporter/Baseband");

        afc_upload_embed_file(afc2_info, "__bootstrap", "/private/var/aquila/bootstrap.tar");
        afc_upload_embed_file(afc2_info, "__truststore", "/private/var/aquila/truststore.tar");
        afc_upload_embed_file(afc2_info, "__splashscreen", "/private/var/aquila/splashscreen.jp2");
        afc_upload_embed_file(afc2_info, "__launchd_conf", "/private/etc/launchd.conf");
        if (has_flag(FLAG_FORCE_INSTALL)) afc_close_file(afc2_info, afc_create_file(afc2_info, "/private/var/aquila/.force_install"));

        if (device_info->version[0] == 4) {
            afc_upload_embed_file(afc2_info, "__aquila_4", "/private/var/aquila/aquila");
            afc_upload_embed_file(afc2_info, "__installer_4", "/Library/Logs/CrashReporter/Baseband");
            afc_upload_embed_file(afc2_info, "__tar_4", "/bin/tar");
        } else {
            afc_upload_embed_file(afc2_info, "__aquila", "/private/var/aquila/aquila");
            afc_upload_embed_file(afc2_info, "__installer", "/Library/Logs/CrashReporter/Baseband");
            afc_upload_embed_file(afc2_info, "__tar", "/bin/tar");
        }

        if (device_info->version[0] == 6) {
            afc_upload_embed_file(afc2_info, "__amfi_6", "/private/var/aquila/amfi_bypass.dylib");
        } else {
            afc_upload_embed_file(afc2_info, "__amfi_4_5", "/private/var/aquila/amfi_bypass.dylib");
            afc_upload_embed_file(afc2_info, "__safemode", "/private/var/aquila/safemode.deb");
            afc_upload_embed_file(afc2_info, "__substrate", "/private/var/aquila/substrate.deb");
        
        }

        md_open_service(device, "haxx.aquila.housekeeping", true);
        usleep(500000);

        print_log(INFO, "rebooting device...\n");
        md_reboot_device(device);
    }
    return 0;
}
