#include "common.h"
#include "util.h"
#include "screen.h"
#include "exploit.h"
#include "memory.h"
#include "utils.h"
#include "install.h"

void prepare_install(void) {
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

        move_file("/Developer/tar", "/bin/tar", false);
        set_file_permissions("/private/var/mobile/Media/aquila/bootstrap.tar", 0777, 501, 501);
        set_file_permissions("/private/var/mobile/Media/aquila/truststore.tar", 0777, 501, 501);
        set_file_permissions("/private/var/mobile/Media/aquila/aquila", 0755, 0, 0);
        set_file_permissions("/private/var/mobile/Media/aquila/amfi_bypass.dylib", 0755, 0, 0);
    } else {
        set_file_permissions("/private/var/aquila/bootstrap.tar", 0777, 501, 501);
        set_file_permissions("/private/var/aquila/truststore.tar", 0777, 501, 501);
        set_file_permissions("/private/var/aquila/aquila", 0755, 0, 0);
        set_file_permissions("/private/var/aquila/amfi_bypass.dylib", 0755, 0, 0);
    }

    chmod("/private", 0777);
    chmod("/private/var", 0777);
    chmod("/private/var/mobile", 0777);
    chmod("/private/var/mobile/Library", 0777);
    chmod("/private/var/mobile/Library/Preferences", 0777);
    set_file_permissions("/bin/tar", 0755, 0, 0);
    sync();
}

void cleanup_install(void) {
    unlink("/private/var/aquila/splashscreen.jp2");
    unlink("/private/var/aquila/bootstrap.tar");
    unlink("/private/var/aquila/truststore.tar");
    unlink("/private/var/aquila/safemode.tar");
    unlink("/private/var/aquila/substrate.tar");
    unlink("/private/var/mobile/Media/aquila/bootstrap.tar");
    unlink("/private/var/mobile/Media/aquila/truststore.tar");
    unlink("/Library/Logs/CrashReporter/Baseband");

    DIR *dir = opendir("Library/LaunchDaemons");
    if (dir != NULL) {
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
    }
}

int install_jailbreak(void) {
    bool skip_bootstrap = false;
    if (access("/.aquila_installed", F_OK) == 0 || access("/Applications/Cydia.app/Cydia", F_OK) == 0) {
        if (access("/private/var/aquila/.force_install", F_OK) == 0 || access("/private/var/mobile/Mediaaquila/.force_install", F_OK) == 0) {
            unlink("/private/var/mobile/Media/aquila/.force_install");
            unlink("/private/var/aquila/.force_install");
            skip_bootstrap = true;
        } else {
            cleanup_install();
            return 0;
        }
    }

    prepare_install();
    if (!skip_bootstrap) {
        if (kinfo->version[0] == 7) run_tar("/private/var/mobile/Media/aquila/bootstrap.tar", "/");
        else run_tar("/private/var/aquila/bootstrap.tar", "/");
        print_log("[*] bootstrap installed\n");

        if (kinfo->version[0] == 7) run_tar("/private/var/mobile/Media/aquila/truststore.tar", "/");
        else run_tar("/private/var/aquila/truststore.tar", "/");
        print_log("[*] truststore installed\n");
    }

    if (kinfo->version[0] == 7) {
        mkdir("/System/Library/Caches/com.apple.dyld", 0755);
        create_file("/System/Library/Caches/com.apple.dyld/enable-dylibs-to-override-cache", 0644, 0, 0);

        move_file("/private/var/mobile/Media/aquila/aquila", "/aquila", false);
        set_file_permissions("/aquila", 0777, 0, 0);

        move_file("/private/var/mobile/Media/aquila/amfi_bypass.dylib", "/usr/lib/libmis.dylib", false);
        set_file_permissions("/usr/lib/libmis.dylib", 0755, 0, 0);
        move_launch_daemons();
    } else if (kinfo->version[0] == 5 || kinfo->version[0] == 4) {
        if (!skip_bootstrap) {
            if (access("/private/var/root/Media/Cydia/AutoInstall", F_OK) != 0) {
                mkdir("/private/var/root/Media/Cydia/AutoInstall", 0777);
                chown("/private/var/root/Media/Cydia/AutoInstall", 501, 501);
            }

            move_file("/private/var/aquila/safemode.deb", "/private/var/root/Media/Cydia/AutoInstall/safemode.deb", true);
            move_file("/private/var/aquila/substrate.deb", "/private/var/root/Media/Cydia/AutoInstall/substrate.deb", true);
            set_file_permissions("/private/var/root/Media/Cydia/AutoInstall/safemode.deb", 0777, 501, 501);
            set_file_permissions("/private/var/root/Media/Cydia/AutoInstall/substrate.deb", 0777, 501, 501);
        }
    }

    FILE *file = NULL;
    if (!skip_bootstrap) {
        file = fopen("/etc/apt/sources.list.d/aquila.list", "w+");
        if (file != NULL) {
            fprintf(file, "deb https://lukezgd.github.io/repo ./\n");
            fflush(file);
            fclose(file);
            sync();
        }
    }

    clear_mobile_installation_cache();
    show_non_default_apps();
    if (kinfo->version[0] == 7) uicache();
    print_log("[*] non default apps set\n");

    file = NULL;
    if (kinfo->version[0] != 7) {
        FILE *file = fopen("/etc/launchd.conf", "wb+");
        if (file == NULL) return -1;
        if (kinfo->version[0] == 4) {
            fprintf(file, "bsexec .. /sbin/mount -u -o rw,suid,dev /\n");
            fprintf(file, "setenv DYLD_INSERT_LIBRARIES /private/var/aquila/amfi_bypass.dylib\n");
            fprintf(file, "unload /System/Library/LaunchDaemons/com.apple.MobileFileIntegrity.plist\n");
            fprintf(file, "load /System/Library/LaunchDaemons/com.apple.MobileFileIntegrity.plist\n");
            fprintf(file, "start com.apple.MobileFileIntegrity\n");
            fprintf(file, "bsexec .. /private/var/aquila/aquila\n");
            fprintf(file, "unsetenv DYLD_INSERT_LIBRARIES\n");
        } else {
            fprintf(file, "unload /System/Library/LaunchDaemons/com.apple.MobileFileIntegrity.plist\n");
            fprintf(file, "bsexec .. /sbin/mount -u -o rw,suid,dev /\n");
            fprintf(file, "setenv DYLD_INSERT_LIBRARIES /private/var/aquila/amfi_bypass.dylib\n");
            fprintf(file, "load /System/Library/LaunchDaemons/com.apple.MobileFileIntegrity.plist\n");
            fprintf(file, "bsexec .. /private/var/aquila/aquila\n");
            fprintf(file, "unsetenv DYLD_INSERT_LIBRARIES\n");
        }

        fflush(file);
        fclose(file);
        set_file_permissions("/etc/launchd.conf", 0644, 0, 0);
    }

    size_t fstab_size = 0;
    char *fstab_data = load_file("/etc/fstab", &fstab_size);
    if (fstab_data == NULL) return -1;

    CFStringRef temp = CFStringCreateWithBytes(NULL, (const uint8_t *)fstab_data, fstab_size, kCFStringEncodingUTF8, false);
    CFMutableStringRef fstab_str = CFStringCreateMutableCopy(NULL, 0, temp);
    CFRelease(temp);

    CFStringFindAndReplace(fstab_str, CFSTR(",nosuid,nodev"), CFSTR(""), CFRangeMake(0, CFStringGetLength(fstab_str)), 0);
    if (kinfo->version[0] != 7) {
        CFStringFindAndReplace(fstab_str, CFSTR(" ro "), CFSTR(" rw "), CFRangeMake(0, CFStringGetLength(fstab_str)), 0);
    }

    char new_fstab[512] = {0};
    CFStringGetCString(fstab_str, new_fstab, 512-1, kCFStringEncodingUTF8);
    CFRelease(fstab_str);

    file = fopen("/etc/fstab", "wb+");
    if (file == NULL) return -1;

    fwrite(new_fstab, strlen(new_fstab), 1, file);
    fflush(file);
    fclose(file);

    set_file_permissions("/etc/fstab", 0644, 0, 0);
    create_file("/.aquila_installed", 0644, 0, 0);
    create_file("/.cydia_no_stash", 0644, 0, 0);
    cleanup_install();
    return 0;
}
