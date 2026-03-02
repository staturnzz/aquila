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


int main(void) {
    setuid(0);
    setgid(0);

    if (run_oob_entry(true) != 0) {
        print_log("[-] exploit failed\n");
        usleep(1000000);
        exit(0);
        return -1;
    }

    print_log("[*] exploit done\n");  
    if (patch_kernel() != 0) {
        print_log("[-] failed to patch kernel\n");
        return -1;
    }
    
    print_log("[*] kernel patched\n");
    start_daemons();

    print_log("[*] done\n");
    return 0;
}