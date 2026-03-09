#ifndef installer_util_h
#define installer_util_h

#include "common.h"
#include <libkern/OSByteOrder.h>

typedef int lockdown_t;

extern int secure_lockdown_checkin(lockdown_t *conn, int unknown, int unknown1);
extern lockdown_t lockdown_connect(void);
extern void lockdown_disconnect(lockdown_t conn);
extern int lockdown_send_message(lockdown_t conn, CFPropertyListRef message, int flags);
extern int lockdown_receive_message(lockdown_t conn, CFPropertyListRef* message);
extern int lockdown_get_socket(lockdown_t conn);
extern void *lockdown_get_securecontext(lockdown_t conn);

int create_file(const char *path, mode_t mode, uid_t uid, gid_t gid);
int write_file(const char *path, void *data, uint32_t size, mode_t mode, uid_t uid, gid_t gid);
void *load_file(const char *path, size_t *size);
int move_file(const char *from, void *to, bool same_partition);
int set_file_permissions(const char *path, mode_t mode, uid_t uid, gid_t gid);
char *get_hw_model(void);
int edit_plist(const char *path, void (^action)(CFMutableDictionaryRef plist));
int run_tar(const char *tar_path, const char *output_path);
int uicache(void);
int clear_mobile_installation_cache(void);
int show_non_default_apps(void);
int move_launch_daemons(void);

#endif /* installer_util_h */