#ifndef afc_h
#define afc_h

#include "common.h"
#include "device.h"

typedef uint32_t afc_error_t;
typedef void *afc_file_ref_t;

typedef enum {
	AFC_FOPEN_RDONLY = 1,
	AFC_FOPEN_RW,
	AFC_FOPEN_WRONLY,
	AFC_FOPEN_WR,
	AFC_FOPEN_APPEND,
	AFC_FOPEN_RDAPPEND 
} afc_file_mode_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t handle;
    uint32_t __unk0;
    uint8_t __unk1;
    uint8_t padding[3];
    uint32_t __unk2;
    uint32_t __unk3;
    uint32_t __unk4;
    uint32_t fs_block_size;
    uint32_t sock_block_size;
    uint32_t io_timeout;
    void *afc_lock;
    uint32_t context;
} afc_connection_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t __unk0[12];
} afc_directory_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t __unk0[16];
    CFDictionaryRef dict;
} afc_dictionary_t;
#pragma pack(pop)

typedef struct {
    int service;
    afc_connection_t *connection;
    am_device_t *device;
} afc_info_t;

typedef struct {
	uint8_t *bytes;
	uint32_t size;
} afc_file_data_t;

#if defined(WINDOWS_BUILD)
extern afc_error_t (*AFCConnectionOpen)(int socket_fd, uint32_t io_timeout, afc_connection_t**conn);
extern afc_error_t (*AFCDeviceInfoOpen)(afc_connection_t *conn, afc_dictionary_t **info);
extern afc_error_t (*AFCDirectoryOpen)(afc_connection_t *conn, const char *path, afc_directory_t **dir);
extern afc_error_t (*AFCDirectoryRead)(afc_connection_t *conn, afc_directory_t *dir, char **dirent);
extern afc_error_t (*AFCDirectoryClose)(afc_connection_t *conn, afc_directory_t *dir);
extern afc_error_t (*AFCDirectoryCreate)(afc_connection_t *conn, const char *dirname);
extern afc_error_t (*AFCRemovePath)(afc_connection_t *conn, const char *dirname);
extern afc_error_t (*AFCRenamePath)(afc_connection_t *conn, const char *oldpath, const char *newpath);
extern afc_error_t (*AFCLinkPath)(afc_connection_t *conn, int64_t linktype, const char *target, const char *linkname);
extern afc_error_t (*AFCFileRefOpen)(afc_connection_t *conn, const char *path, uint64_t mode, afc_file_ref_t *ref);
extern afc_error_t (*AFCFileRefRead)(afc_connection_t *conn, afc_file_ref_t ref, void *buf, uint32_t *len);
extern afc_error_t (*AFCFileRefWrite)(afc_connection_t *conn, afc_file_ref_t ref, void *buf, uint32_t len);
extern afc_error_t (*AFCFileRefClose)(afc_connection_t *conn, afc_file_ref_t ref);
extern afc_error_t (*AFCFileInfoOpen)(afc_connection_t *conn, const char *path, afc_dictionary_t **info);
extern afc_error_t (*AFCConnectionClose)(afc_connection_t *conn);
extern afc_error_t (*AFCLinkPath)(afc_connection_t *conn, int64_t linktype, const char *target, const char *linkname);
extern afc_error_t (*AFCFileRefRead)(afc_connection_t *conn, afc_file_ref_t ref, void *buf, unsigned int *len);
#else
extern afc_error_t AFCConnectionOpen(int socket_fd, uint32_t io_timeout, afc_connection_t**conn);
extern afc_error_t AFCDeviceInfoOpen(afc_connection_t *conn, afc_dictionary_t **info);
extern afc_error_t AFCDirectoryOpen(afc_connection_t *conn, const char *path, afc_directory_t **dir);
extern afc_error_t AFCDirectoryRead(afc_connection_t *conn, afc_directory_t *dir, char **dirent);
extern afc_error_t AFCDirectoryClose(afc_connection_t *conn, afc_directory_t *dir);
extern afc_error_t AFCDirectoryCreate(afc_connection_t *conn, const char *dirname);
extern afc_error_t AFCRemovePath(afc_connection_t *conn, const char *dirname);
extern afc_error_t AFCRenamePath(afc_connection_t *conn, const char *oldpath, const char *newpath);
extern afc_error_t AFCFileRefOpen(afc_connection_t *conn, const char *path, uint64_t mode, afc_file_ref_t *ref);
extern afc_error_t AFCFileRefWrite(afc_connection_t *conn, afc_file_ref_t ref, void *buf, uint32_t len);
extern afc_error_t AFCFileRefClose(afc_connection_t *conn, afc_file_ref_t ref);
extern afc_error_t AFCFileInfoOpen(afc_connection_t *conn, const char *path, afc_dictionary_t **info);
extern afc_error_t AFCConnectionClose(afc_connection_t *conn);
extern afc_error_t AFCLinkPath(afc_connection_t *conn, int64_t linktype, const char *target, const char *linkname);
extern afc_error_t AFCFileRefRead(afc_connection_t *conn, afc_file_ref_t ref, void *buf, unsigned int *len);
#endif

afc_info_t *afc_init(am_device_t *device);
afc_info_t *afc2_init(am_device_t *device);
void afc_deinit(afc_info_t *info);
int afc_rename(afc_info_t *info, const char *from, const char *to);
afc_directory_t *afc_open_dir(afc_info_t *info, const char *path);
void afc_close_dir(afc_info_t *info, afc_directory_t *dir);
int afc_create_dir(afc_info_t *info, const char *path);
char *afc_read_dir(afc_info_t *info, afc_directory_t *dir);
afc_file_ref_t afc_open_file(afc_info_t *info, const char *path, uint64_t mode);
void afc_close_file(afc_info_t *info, afc_file_ref_t file);
afc_file_ref_t afc_create_file(afc_info_t *info, const char *path);
int afc_write_file(afc_info_t *info, afc_file_ref_t file, void *data, uint32_t size);
bool afc_file_exists(afc_info_t *info, const char *path);
uint32_t afc_get_file_size(afc_info_t *info, const char *path);
afc_file_data_t *afc_read_file(afc_info_t *info, const char *path);
int afc_create_symlink(afc_info_t *info, const char *from, const char *to);
int afc_delete_item(afc_info_t *info, const char *path);
int afc_upload_embed_file(afc_info_t *info, const char *name, const char *remote_path);
void *load_embed_file(const char *name, size_t *size);

#endif /* afc_h */
