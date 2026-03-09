#include "device.h"
#include "afc.h"
#include "util.h"

#if defined(WINDOWS_BUILD)
afc_error_t (*AFCConnectionOpen)(int socket_fd, uint32_t io_timeout, afc_connection_t **conn) = NULL;
afc_error_t (*AFCDeviceInfoOpen)(afc_connection_t *conn, afc_dictionary_t **info) = NULL;
afc_error_t (*AFCDirectoryOpen)(afc_connection_t *conn, const char *path, afc_directory_t **dir) = NULL;
afc_error_t (*AFCDirectoryRead)(afc_connection_t *conn, afc_directory_t *dir, char **dirent) = NULL;
afc_error_t (*AFCDirectoryClose)(afc_connection_t *conn, afc_directory_t *dir) = NULL;
afc_error_t (*AFCDirectoryCreate)(afc_connection_t *conn, const char *dirname) = NULL;
afc_error_t (*AFCRemovePath)(afc_connection_t *conn, const char *dirname) = NULL;
afc_error_t (*AFCRenamePath)(afc_connection_t *conn, const char *oldpath, const char *newpath) = NULL;
afc_error_t (*AFCFileRefOpen)(afc_connection_t *conn, const char *path, uint64_t mode, afc_file_ref_t *ref) = NULL;
afc_error_t (*AFCFileRefRead)(afc_connection_t *conn, afc_file_ref_t ref, void *buf, uint32_t *len) = NULL;
afc_error_t (*AFCFileRefWrite)(afc_connection_t *conn, afc_file_ref_t ref, void *buf, uint32_t len) = NULL;
afc_error_t (*AFCFileRefClose)(afc_connection_t *conn, afc_file_ref_t ref) = NULL;
afc_error_t (*AFCFileInfoOpen)(afc_connection_t *conn, const char *path, afc_dictionary_t **info) = NULL;
afc_error_t (*AFCConnectionClose)(afc_connection_t *conn) = NULL;
afc_error_t (*AFCLinkPath)(afc_connection_t *conn, int64_t linktype, const char *target, const char *linkname) = NULL;
#endif

static afc_info_t *afc_init_service(am_device_t *device, const char *name) {
    int service = md_open_service(device, name, false);
    if (service < 0) return NULL;

    afc_connection_t *connection = NULL;
    if (AFCConnectionOpen(service, 0, &connection) != 0) {
        md_close_service(service);
        return NULL;
    }

    afc_dictionary_t *dict = NULL;
    if (AFCDeviceInfoOpen(connection, &dict) != 0) {
        AFCConnectionClose(connection);
        md_close_service(service);
        return NULL;
    }

    afc_info_t *info = calloc(1, sizeof(afc_info_t));
    info->service = service;
    info->connection = connection;
    info->device = device;
    return info;
}

afc_info_t *afc_init(am_device_t *device) {
    return afc_init_service(device, "com.apple.afc");
}

afc_info_t *afc2_init(am_device_t *device) {
    afc_info_t *info = afc_init_service(device, "com.apple.afc2");
    if (info != NULL) return info;
    return afc_init_service(device, "haxx.aquila.afc2");
}

void afc_deinit(afc_info_t *info) {
    if (info == NULL) return;
    if (info->connection != NULL) AFCConnectionClose(info->connection);
    if (info->service >= 0) md_close_service(info->service);
    free(info);
}

int afc_rename(afc_info_t *info, const char *from, const char *to) {
    afc_delete_item(info, to);
    return AFCRenamePath(info->connection, from, to);
}

afc_directory_t *afc_open_dir(afc_info_t *info, const char *path) {
    afc_directory_t *dir = NULL;
    if (AFCDirectoryOpen(info->connection, path, &dir) != 0) return NULL;
    return dir;
}

void afc_close_dir(afc_info_t *info, afc_directory_t *dir) {
    AFCDirectoryClose(info->connection, dir);
}

int afc_create_dir(afc_info_t *info, const char *path) {
    return AFCDirectoryCreate(info->connection, path);
}

char *afc_read_dir(afc_info_t *info, afc_directory_t *dir) {
    char *entry = NULL;
    if (AFCDirectoryRead(info->connection, dir, &entry) != 0) return NULL;
    return entry;
}

afc_file_ref_t afc_open_file(afc_info_t *info, const char *path, uint64_t mode) {
    afc_file_ref_t file = NULL;
    if (AFCFileRefOpen(info->connection, path, mode, &file) != 0) return NULL;
    return file;
}

void afc_close_file(afc_info_t *info, afc_file_ref_t file) {
    if (file != NULL) AFCFileRefClose(info->connection, file);
}

afc_file_ref_t afc_create_file(afc_info_t *info, const char *path) {
    AFCRemovePath(info->connection, path);
    return afc_open_file(info, path, AFC_FOPEN_RW);
}

int afc_write_file(afc_info_t *info, afc_file_ref_t file, void *data, uint32_t size) {
    return AFCFileRefWrite(info->connection, file, data, size);
}

bool afc_file_exists(afc_info_t *info, const char *path) {
    afc_file_ref_t file = NULL;
    if (AFCFileRefOpen(info->connection, path, AFC_FOPEN_RDONLY, &file) != 0 || file == NULL) return false;
    AFCFileRefClose(info->connection, file);
    return true;
}

uint32_t afc_get_file_size(afc_info_t *info, const char *path) {
    afc_dictionary_t *file_info = NULL;
    if (AFCFileInfoOpen(info->connection, path, &file_info) != 0) return 0;

    CFTypeRef cf_size = CFDictionaryGetValue(file_info->dict, CFSTR("st_size"));
    if (cf_size == NULL) return 0;

    if (CFGetTypeID(cf_size) == CFNumberGetTypeID()) {
        uint32_t size = 0;
        CFNumberGetValue((CFNumberRef)cf_size, kCFNumberIntType, &size);
        return size;
    } else if (CFGetTypeID(cf_size) == CFStringGetTypeID()) {
        char buf[32] = {0};
        CFStringGetCString((CFStringRef)cf_size, buf, 32-1, kCFStringEncodingUTF8);
        return atoi(buf);
    }
    return 0;
}

afc_file_data_t *afc_read_file(afc_info_t *info, const char *path) {
#if defined(WINDOWS_BUILD)
    uint64_t len = afc_get_file_size(info, path);
#else
    uint32_t len = afc_get_file_size(info, path);
#endif
    if (len == 0) return NULL;

    afc_file_data_t *data = calloc(1, sizeof(afc_file_data_t));
    data->bytes = calloc(1, len);
    data->size = len;

    afc_file_ref_t file = afc_open_file(info, path, AFC_FOPEN_RDONLY);
    if (file == NULL) {
        free(data->bytes);
        free(data);
        return NULL;
    }

    if (AFCFileRefRead(info->connection, file, data->bytes, &len) != 0) {
        free(data->bytes);
        free(data);
        return NULL;
    }
    return data;
}

int afc_create_symlink(afc_info_t *info, const char *from, const char *to) {
    return AFCLinkPath(info->connection, 2, from, to);
}

int afc_delete_item(afc_info_t *info, const char *path) {
    afc_dictionary_t *file_info = NULL;
    if (AFCFileInfoOpen(info->connection, path, &file_info) != 0) return -1;

    if (file_info == NULL || file_info->dict == NULL) return -1;
    CFStringRef st_ifmt = CFDictionaryGetValue(file_info->dict, CFSTR("st_ifmt"));
    if (st_ifmt == NULL) return -1;

    if (!CFEqual(st_ifmt, CFSTR("S_IFDIR"))) {
        AFCRemovePath(info->connection, path);
        return 0;
    }

    afc_directory_t *dir = afc_open_dir(info, path);
    if (dir == NULL) return -1;
    bool is_root = strcmp(path, "/") == 0;

    char *entry = NULL;
    char path_buf[1024] = {0};
    while ((entry = afc_read_dir(info, dir)) != NULL) {
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        memset(path_buf, 0, 1024);

        if (is_root) {
            snprintf(path_buf, 1024-1, "%s", entry);
        } else {
            snprintf(path_buf, 1024-1, "%s/%s", path, entry);
        }
        afc_delete_item(info, path_buf);
    }

    afc_close_dir(info, dir);
    AFCRemovePath(info->connection, path);
    return 0;
}

int afc_upload_embed_file(afc_info_t *info, const char *name, const char *remote_path) {
    size_t size = 0;
    void *data = load_embed_file(name, &size);
    if (data == NULL) return -1;
    
    afc_delete_item(info, remote_path);
    afc_file_ref_t file = afc_create_file(info, remote_path);
    if (file == NULL) return -1;
    
    afc_write_file(info, file, data, size);
    afc_close_file(info, file);
    return 0;
}
