#include "common.h"
#include "util.h"
#include "screen.h"
#include "exploit.h"
#include "memory.h"
#include "utils.h"

int create_file(const char *path, mode_t mode, uid_t uid, gid_t gid) {
    if (access(path, F_OK) == 0) {
        if (chmod(path, mode) != 0) return -1;
        if (chown(path, uid, gid) != 0) return -1;
        sync();
        return 0;
    }

    int fd = open(path, O_RDWR|O_CREAT);
    if (fd < 0) return -1;
    close(fd);
    sync();

    if (chmod(path, mode) != 0) return -1;
    if (chown(path, uid, gid) != 0) return -1;
    sync();
    return 0;
}

int write_file(const char *path, void *data, uint32_t size, mode_t mode, uid_t uid, gid_t gid) {
    if (access(path, F_OK) == 0) {
        unlink(path);
        sync();
    }

    int fd = open(path, O_RDWR|O_CREAT);
    if (fd < 0) return -1;

    write(fd, data, size);
    close(fd);
    sync();

    if (chmod(path, mode) != 0) return -1;
    if (chown(path, uid, gid) != 0) return -1;
    sync();
    return 0;
}

void *load_file(const char *path, size_t *size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return NULL;
    
    *size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    void *data = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) {
        *size = 0;
        return NULL;
    }
    return data;
}

int move_file(const char *from, void *to, bool same_partition) {
    if (same_partition) return rename(from, to);
    struct stat st = {0};
    if (stat(from, &st) != 0) return -1;

    size_t size = 0;
    void *data = load_file(from, &size);
    if (data == NULL) return -1;

    if (access(to, F_OK) == 0) {
        unlink(to);
        sync();
    }

    int fd = open(to, O_RDWR|O_CREAT);
    if (fd < 0) {
        munmap(data, size);
        return -1;
    }

    write(fd, data, size);
    close(fd);
    sync();

    munmap(data, size);
    chmod(to, st.st_mode);
    chown(to, st.st_uid, st.st_gid);

    unlink(from);
    sync();
    return 0;
}

int set_file_permissions(const char *path, mode_t mode, uid_t uid, gid_t gid) {
    if (access(path, F_OK) != 0) return -1;
    if (chmod(path, mode) != 0) return -1;
    if (chown(path, uid, gid) != 0) return -1;
    sync();
    return 0;
}

char *get_hw_model(void) {
    size_t size = 0;
    sysctlbyname("hw.model", NULL, &size, NULL, 0);
    char *model = calloc(1, size+1);
    sysctlbyname("hw.model", model, &size, NULL, 0);
    return model;
}

int edit_plist(const char *path, void (^action)(CFMutableDictionaryRef plist)) {
    size_t size = 0;
    void *data = load_file(path, &size);
    if (data == NULL) return -1;

    if (size < 8) {
        munmap(data, size);
        return -1;
    }

    CFDataRef cf_data = CFDataCreate(NULL, data, size);
    CFPropertyListFormat format = kCFPropertyListXMLFormat_v1_0;
    bool use_bplist = false;

    if (strncmp((char *)data, "bplist00", strlen("bplist00")) == 0) {
        format = kCFPropertyListBinaryFormat_v1_0;
        use_bplist = true;
    }

    munmap(data, size);
    CFMutableDictionaryRef dict = (CFMutableDictionaryRef)CFPropertyListCreateWithData(NULL, cf_data, kCFPropertyListMutableContainersAndLeaves, &format, NULL);
    if (dict == NULL) {
        CFRelease(cf_data);
        return -1;
    }

    action(dict);
    FILE *file = fopen(path, "wb+");

    if (file != NULL) {
        CFDataRef plist_data = NULL;
        if (use_bplist) {
            plist_data = CFPropertyListCreateData(NULL, dict, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
        } else {
            plist_data = CFPropertyListCreateData(NULL, dict, kCFPropertyListXMLFormat_v1_0, 0, NULL);
        }
        
        if (plist_data != NULL) {
            fwrite((void *)CFDataGetBytePtr(plist_data), CFDataGetLength(plist_data), 1, file);
            fflush(file);
            CFRelease(plist_data);
        }

        fclose(file);
        sync();
    }

    CFRelease(cf_data);
    CFRelease(dict);
    return 0;
}

int run_tar(const char *tar_path, const char *output_path) {
    char *args[] = {"/bin/tar", "-xf", (char *)tar_path, "-C", (char *)output_path, "--preserve-permissions", "--no-overwrite-dir", NULL};
    pid_t pid = -1;
    int status = -1;

    int rv = posix_spawn(&pid, "/bin/tar", NULL, NULL, args, NULL);
    if (rv != 0 || pid == -1) return -1;
    
    do { if (waitpid(pid, &status, 0) == -1) return status; }
    while (!WIFEXITED(status) && !WIFSIGNALED(status));
    return status;
}

int uicache(void) {
    char *args[] = {"/usr/bin/su", "mobile", "-c", "/usr/bin/uicache", NULL};
    pid_t pid = -1;
    int status = -1;

    int rv = posix_spawn(&pid, "/usr/bin/su", NULL, NULL, args, NULL);
    if (rv != 0 || pid == -1) return -1;

    do { if (waitpid(pid, &status, 0) == -1) return status; }
    while (!WIFEXITED(status) && !WIFSIGNALED(status));
    return status;
}

int clear_mobile_installation_cache(void) {
    if (access("/var/mobile/Library/Caches/com.apple.mobile.installation.plist", F_OK) == 0) {
        unlink("/var/mobile/Library/Caches/com.apple.mobile.installation.plist");
        sync();
    }

    DIR *dir = opendir("/var/mobile/Library/Caches");
    if (dir == NULL) return 1;
    struct dirent *entry = NULL;

    while ((entry = readdir(dir)) != NULL) {
        char *item = (char *)(entry->d_name);
        if (strncmp(item, "com.apple.LaunchServices", strlen("com.apple.LaunchServices")) == 0) {
            if (strstr(item, ".csstore") != NULL) {
                char *full_path = calloc(1, PATH_MAX);
                snprintf(full_path, PATH_MAX, "/var/mobile/Library/Caches/%s", item);
                unlink(full_path);
                free(full_path);
                sync();
            }
        }
    }

    closedir(dir);
    return 0;
}

int show_non_default_apps(void) {
    char *model = get_hw_model();
    char path_buf[PATH_MAX] = {0};
    snprintf(path_buf, PATH_MAX-1, "/System/Library/CoreServices/SpringBoard.app/%s.plist", model);
    free(model);

    edit_plist(path_buf,  ^void (CFMutableDictionaryRef dict) {
        CFMutableDictionaryRef capabilities = (CFMutableDictionaryRef)CFDictionaryGetValue(dict, CFSTR("capabilities"));
        if (capabilities == NULL) {
            CFMutableDictionaryRef temp = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
            CFDictionaryAddValue(dict, CFSTR("capabilities"), temp);
            capabilities = (CFMutableDictionaryRef)CFDictionaryGetValue(dict, CFSTR("capabilities"));
            if (capabilities == NULL) return;
        }

        CFDictionaryAddValue(capabilities, CFSTR("hide-non-default-apps"), kCFBooleanFalse);
    });

    edit_plist("/var/mobile/Library/Preferences/com.apple.springboard.plist",  ^void (CFMutableDictionaryRef dict) {
        CFDictionaryAddValue(dict, CFSTR("SBShowNonDefaultSystemApps"), kCFBooleanTrue);
    });

    usleep(100000);
    return 0;
}

int move_launch_daemons(void) {
    if (access("/Library/LaunchDaemons", F_OK) != 0) {
        mkdir("/Library/LaunchDaemons", 0755);
        sync();
    } else {
        chmod("/Library/LaunchDaemons", 0755);
    }

    DIR *dir = opendir("/System/Library/LaunchDaemons");
    if (dir == NULL) return -1;

    const char *ignore_list[] = {
        "com.apple.CrashHousekeeping.plist",
        "com.apple.MobileFileIntegrity.plist",
        "com.apple.mobile.softwareupdated.plist",
        "com.apple.softwareupdateservicesd.plist",
        "com.apple.jetsamproperties*",
        "com.saurik.Cydia.Startup.plist",
        "com.apple.sandboxd.plist",
        NULL,
    };

    struct dirent *entry = NULL;
    char from_path[PATH_MAX] = {0};
    char to_path[PATH_MAX] = {0};

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strstr(entry->d_name, ".plist") == NULL) continue;
        bool skip_daemon = false;
        for (uint32_t i = 0; ignore_list[i] != NULL; i++) {
            if (strstr(entry->d_name, ignore_list[i]) != NULL) {
                skip_daemon = true;
                break;
            }
        }

        if (skip_daemon) continue;
        bzero(from_path, PATH_MAX);
        bzero(to_path, PATH_MAX);
        snprintf(from_path, PATH_MAX-1, "/System/Library/LaunchDaemons/%s", entry->d_name);
        snprintf(to_path, PATH_MAX-1, "/Library/LaunchDaemons/%s", entry->d_name);

        move_file(from_path, to_path, true);
        chmod(to_path, 0644);
        chown(to_path, 0, 0);
    }

    closedir(dir);
    move_file("/System/Library/LaunchDaemons/com.apple.mobile.softwareupdated.plist", "/Library/LaunchDaemons/com.apple.mobile.softwareupdated.plist.backup", true);
    move_file("/System/Library/LaunchDaemons/com.apple.softwareupdateservicesd.plist", "/Library/LaunchDaemons/com.apple.softwareupdateservicesd.plist.backup", true);
    move_file("/usr/libexec/CrashHousekeeping", "/usr/libexec/CrashHousekeeping.backup", true);

    symlink("/aquila", "/usr/libexec/CrashHousekeeping");
    chmod("/usr/libexec/CrashHousekeeping", 0755);
    chown("/usr/libexec/CrashHousekeeping", 0, 0);
 
    usleep(100000);
    sync();
    return 0;
}
