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
    if (mount_dmg_payload(device, device_info, afc_info) != 0) {
        print_log(ERROR, "failed to upload dmg payload\n");
        return -1;
    }

    print_log(INFO, "dmg payload uploaded\n");
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
    afc_upload_embed_file(afc2_info, "__aquila", "/private/var/aquila/aquila");
    afc_upload_embed_file(afc2_info, "__installer", "/Library/Logs/CrashReporter/Baseband");

    if (device_info->version[0] == 6) {
        afc_upload_embed_file(afc2_info, "__tar", "/bin/tar");
        afc_upload_embed_file(afc2_info, "__amfi_6", "/private/var/aquila/amfi_bypass.dylib");
    } else {
        afc_upload_embed_file(afc2_info, "__amfi_4_5", "/private/var/aquila/amfi_bypass.dylib");
        if (device_info->version[0] == 5) {
            afc_upload_embed_file(afc2_info, "__tar", "/bin/tar");
            afc_upload_embed_file(afc2_info, "__safemode_5", "/private/var/aquila/safemode5.deb");
            afc_upload_embed_file(afc2_info, "__substrate_5", "/private/var/aquila/substrate5.deb");
        } else {
            afc_upload_embed_file(afc2_info, "__tar_4", "/bin/tar");
        }
    }

    md_open_service(device, "haxx.aquila.housekeeping", true);
    usleep(500000);

    print_log(INFO, "rebooting device...\n");
    md_reboot_device(device);
    return 0;
}
