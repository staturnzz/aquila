#include "common.h"
#include "util.h"
#include "exploit.h"
#include "memory.h"
#include "patchfinder.h"
#include "patches.h"
#include "install.h"
#include "screen.h"
#include "utils.h"

static lockdown_t ld_connection = -1;
static int ld_socket = -1;

int load_run_commands(void) {
    DIR *dir = opendir("/etc/rc.d");
    if (dir == NULL) return -1;

    struct dirent *entry = NULL;
    char path_buf[PATH_MAX] = {0};
    char *args[] = {path_buf, NULL};

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        bzero(path_buf, PATH_MAX);
        snprintf(path_buf, PATH_MAX-1, "/etc/rc.d/%s", entry->d_name);

        pid_t pid = -1;
        int rv = posix_spawn(&pid, path_buf, NULL, NULL, args, environ);
        if (rv != 0 || pid == -1) {
            print_log("[WARNING] failed to start: %s\n", path_buf);
        }
    }

    closedir(dir);
    return 0;
}

int load_user_daemons(void) {
    char *args[] = {"/bin/launchctl", "load", "/Library/LaunchDaemons", NULL};
    pid_t pid = -1;
    int status = -1;

    int rv = posix_spawn(&pid, args[0], NULL, NULL, args, environ);
    if (rv != 0 || pid == -1) {
        print_log("[ERROR] failed to start user daemons\n");
        return rv;
    }
    
    do { if (waitpid(pid, &status, 0) == -1) return status; }
    while (!WIFEXITED(status) && !WIFSIGNALED(status));
    return status;
}

int launchctl_unsetenv(void) {
    char **args = calloc(1, sizeof(char *) * 4);
    args[0] = "/bin/launchctl";
    args[1] = "unsetenv";
    args[2] = "DYLD_INSERT_LIBRARIES";
    args[3] = NULL;

    pid_t pid = -1;
    int status = -1;
    int rv = posix_spawn(&pid, "/bin/launchctl", NULL, NULL, args, NULL);
    if (rv != 0 || pid == -1) goto done;
    
    do { if (waitpid(pid, &status, 0) == -1) goto done; }
    while (!WIFEXITED(status) && !WIFSIGNALED(status));

done:
    free(args);
    return status;
}

int start_daemons(void) {
    usleep(50000);
    launchctl_unsetenv();
    DIR *dir = opendir("/Library/LaunchDaemons");
    if (dir == NULL) return -1;

    struct dirent *entry = NULL;
    char path_buf[PATH_MAX] = {0};

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (strstr(entry->d_name, ".plist") == NULL) continue;

        bzero(path_buf, PATH_MAX);
        snprintf(path_buf, PATH_MAX, "/Library/LaunchDaemons/%s", entry->d_name);
        
        chmod(path_buf, 0644);
        chown(path_buf, 0, 0);
    }

    closedir(dir);
    sync();

    load_run_commands();
    load_user_daemons();

    if (access("/usr/libexec/substrate", F_OK) == 0) {
        char *args[] = {"/usr/libexec/substrate", NULL};
        pid_t pid = -1;
        int rv = posix_spawn(&pid, args[0], NULL, NULL, args, environ);
        if (rv != 0 || pid == -1) {
            print_log("[WARNING] failed to start substrate\n");
        }
    }
    return 0;
}

static void send_status_msg(const char *msg) {
    if (ld_connection < 0) {
        secure_lockdown_checkin(&ld_connection, 0, 0);
        if (ld_connection < 0) return;

        ld_socket = lockdown_get_socket(ld_connection);
        if (ld_socket < 0) return;
    }

    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef status_str = CFStringCreateWithCString(NULL, msg, kCFStringEncodingUTF8);
    CFDictionarySetValue(dict, CFSTR("Status"), status_str);
    CFRelease(status_str);

    CFDataRef msg_data = CFPropertyListCreateData(NULL, dict, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    CFRelease(dict);

    uint32_t msg_size = htonl(CFDataGetLength(msg_data));
    send(ld_socket, &msg_size, 4, 0);

    send(ld_socket, CFDataGetBytePtr(msg_data), CFDataGetLength(msg_data), 0);
    CFRelease(msg_data);
}

int main(void) {
    setuid(0);
    setgid(0);

    uint32_t version[3] = {0};
    get_ios_version(&version[0]);
    if (version[0] == 7) {
        send_status_msg("checkin");
        usleep(1000000);
    }

    char *image_path = NULL;
    if (access("/private/var/aquila/splashscreen.jp2", F_OK) == 0) image_path = "/private/var/aquila/splashscreen.jp2";
    else if (access("/private/var/mobile/Media/aquila/splashscreen.jp2", F_OK) == 0) image_path = "/private/var/mobile/Media/aquila/splashscreen.jp2";
    if (image_path != NULL) draw_splash_screen(image_path);
    
    uint32_t cpu_family = 0;
    size_t size = sizeof(cpu_family);
    sysctlbyname("hw.cpufamily", &cpu_family, &size, NULL, 0);

    if (run_oob_entry((cpu_family != CPUFAMILY_ARM_SWIFT)) != 0) {
        print_log("[-] exploit failed\n");
        usleep(100000);
    }

    if (patch_kernel() != 0) {
        print_log("[-] failed to patch kernel\n");
        return -1;
    }

    if (kinfo->version[0] == 7) {
        unmount("/System/Library/Caches", 0x80000);
        unmount("/usr/lib", 0x80000);
        usleep(100000);
        sync();
    }

    if (install_jailbreak() != 0) {
        return -1;
    }

    if (version[0] == 7) {
        send_status_msg("install_sucess");
        usleep(100000);
        reboot(0);
    } else {
        start_daemons();
    }
    return 0;
}