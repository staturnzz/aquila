#include "common.h"
#include "device.h"
#include "afc.h"
#include "jailbreak.h"
#include "syslog.h"
#include "util.h"

uint16_t global_flags = FLAG_NONE;

void print_help(void) {
    fprintf(stdout, "Usage: aquila [-jfvisnrch]\n");
    fprintf(stdout, "   -j, --jailbreak \t\t install jailbreak (default)\n");
    fprintf(stdout, "   -f, --force-install \t\t allow (re)install if already jailbroken\n");
    fprintf(stdout, "   -v, --verbose \t\t enable verbose logging\n");
    fprintf(stdout, "   -i, --install-log \t\t view install log\n");
    fprintf(stdout, "   -s, --system-log \t\t view system log\n");
    fprintf(stdout, "   -n, --no-color \t\t disable color in logs\n");
    fprintf(stdout, "   -r, --reboot \t\t reboot the connected device\n");
    fprintf(stdout, "   -c, --credits \t\t print credits\n");
    fprintf(stdout, "   -h, --help \t\t\t print help\n\n");
    exit(0);
}

void print_credits(void) {
    fprintf(stdout, "Credits:\n");
    fprintf(stdout, "   staturnz - jailbreak and exploit\n");
    fprintf(stdout, "   TaiG Jailbreak Team - CVE-2014-4480 (afc race condition)\n");
    fprintf(stdout, "   Kaspersky - CVE-2023-32434 (kernel exploit bug)\n");
    fprintf(stdout, "   comex - ddi race condition and sandbox patch\n");
    fprintf(stdout, "   planetbeing - ios-jailbreak-patchfinder\n");
    fprintf(stdout, "   evad3rs - amfid bypass method\n");
    fprintf(stdout, "   g1lbertJB team - patched substrate for ios 5\n\n");
    exit(0);
}

#if defined(WINDOWS_BUILD)
void set_color_mode(void) {
    if ((global_flags & FLAG_NO_COLOR) == FLAG_NO_COLOR) return;
    if (getenv("ANSICON") || getenv("MSYSTEM")) {
        global_flags |= FLAG_ANSI_COLOR;
        return;
    }

    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdout_handle != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;

        if (GetConsoleMode(stdout_handle, &mode)) {
            if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
                global_flags |= FLAG_ANSI_COLOR;
                return;
            }
        }
    }

    const char *ps_version = getenv("PSVersionTable");
    if (ps_version != NULL && strstr(ps_version, "7") != NULL) {
        global_flags |= FLAG_ANSI_COLOR;
        return;
    }
    global_flags |= FLAG_WINCONSOLE_COLOR;
}
#endif

void parse_args(int argc, char **argv) {
#if defined(WINDOWS_BUILD)
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (arg[0] == '-' && arg[1] != '-') {
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'j': global_flags |= FLAG_JAILBREAK; break;
                    case 'f': global_flags |= FLAG_FORCE_INSTALL; break;
                    case 'v': global_flags |= FLAG_VERBOSE_LOGGING; break;
                    case 'i': global_flags |= FLAG_INSTALL_LOG; break;
                    case 's': global_flags |= FLAG_SYSTEM_LOG; break;
                    case 'n': global_flags |= FLAG_NO_COLOR; break;
                    case 'r': global_flags |= FLAG_REBOOT; break;
                    case 'c': return print_credits();
                    default: return print_help();
                }
            }
        } else if (strcmp(arg, "--jailbreak") == 0) {
            global_flags |= FLAG_JAILBREAK;
        } else if (strcmp(arg, "--force-install") == 0) {
            global_flags |= FLAG_FORCE_INSTALL;
        } else if (strcmp(arg, "--verbose") == 0) {
            global_flags |= FLAG_VERBOSE_LOGGING;
        } else if (strcmp(arg, "--install-log") == 0) {
            global_flags |= FLAG_INSTALL_LOG;
        } else if (strcmp(arg, "--system-log") == 0) {
            global_flags |= FLAG_SYSTEM_LOG;
        } else if (strcmp(arg, "--no-color") == 0) {
            global_flags |= FLAG_NO_COLOR;
        } else if (strcmp(arg, "--reboot") == 0) {
            global_flags |= FLAG_REBOOT;
        } else if (strcmp(arg, "--credits") == 0) {
            return print_credits();
        } else {
            return print_help();
        }
    }

    global_flags |= FLAG_JAILBREAK;
    set_color_mode();
#else
    const char *short_opts = "jfvisnrch";
    struct option long_opts[] = {
        {"jailbreak", no_argument, 0, 'j'},
        {"force-install", no_argument, 0, 'f'},
        {"verbose", no_argument, 0, 'v'},
        {"install-log", no_argument, 0, 'i'},
        {"system-log", no_argument, 0, 's'},
        {"no-color", no_argument, 0, 'n'},
        {"reboot", no_argument, 0, 'r'},
        {"credits", no_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0},
    };

    while (1) {
        int idx = 0;
        int opt = getopt_long(argc, argv, short_opts, long_opts, &idx);
        if (opt == -1) break;

        switch (opt) {
            case 'j': global_flags |= FLAG_JAILBREAK; break;
            case 'f': global_flags |= FLAG_FORCE_INSTALL; break;
            case 'v': global_flags |= FLAG_VERBOSE_LOGGING; break;
            case 'i': global_flags |= FLAG_INSTALL_LOG; break;
            case 's': global_flags |= FLAG_SYSTEM_LOG; break;
            case 'n': global_flags |= FLAG_NO_COLOR; break;
            case 'r': global_flags |= FLAG_REBOOT; break;
            case 'c': return print_credits();
            case 'h': return print_help();
            case '?': return print_help();
            default: exit(1);
        }
    }

    global_flags |= FLAG_JAILBREAK;
    if (!isatty(1) || !isatty(2)) global_flags |= FLAG_NO_COLOR;
    if ((global_flags & FLAG_NO_COLOR) == 0) global_flags |= FLAG_ANSI_COLOR;
#endif
}

bool is_device_supported(device_info_t *device_info) {
    if (device_info->version[0] >= 8 || device_info->version[0] <= 3) return false;
    if (device_info->version[0] == 4 && device_info->version[1] != 3) return false;
    return !(strcmp(device_info->cpu_arch, "arm64") == 0 || strcmp(device_info->cpu_arch, "arm64e") == 0 || strcmp(device_info->cpu_arch, "armv6") == 0);
}

int main(int argc, char **argv) {
    parse_args(argc, argv);
    print_log(INFO, "aquila jailbreak for iOS 4.3 - 7.1.2 by @staturnzdev\n");
#if defined(WINDOWS_BUILD)
    print_log(INFO, "version 2.1.1 (Windows)\n");
#else
    print_log(INFO, "version 2.1.1 (macOS)\n");
#endif

    int err = platform_init();
    if (err != 0) {
        switch (err) {
#if defined(WINDOWS_BUILD)
            case -1: print_log(ERROR, "failed to find require symbols, update or reinstall iTunes and try again\n"); break;
            case 1: print_log(ERROR, "failed to find iTunes files, update or install iTunes (64bit) and try again\n"); break;
            case 2: print_log(ERROR, "Apple TV from the Microsoft Store is installed, it must be removed before running aquila\n"); break;
            case 3: print_log(ERROR, "iTunes from the Microsoft Store is installed, it must be removed before running aquila\n"); break;
            case 4: print_log(ERROR, "Apple Devices from the Microsoft Store is installed, it must be removed before running aquila\n"); break;
            case 5: print_log(ERROR, "Apple Music from the Microsoft Store is installed, it must be removed before running aquila\n"); break;
            case 6: print_log(ERROR, "CoreFoundation.dll not found, update or reinstall iTunes and try again\n"); break;
            case 7: print_log(ERROR, "MobileDevice.dll not found, update or reinstall iTunes and try again\n"); break;
#endif
            default: print_log(ERROR, "failed to initialize platform: unknown error\n"); break;
        }
    }

    if (md_init() != 0) {
        print_log(ERROR, "failed to initialize MobileDevice API\n");
        return -1;
    }

    print_log(INFO, "waiting for device...\n");
    am_device_t *device = md_await_device();
    if (device == NULL) {
        print_log(ERROR, "failed to connect to device\n");
        return -1;
    }

    device_info_t *device_info = md_device_info(device);
    if (device_info == NULL) {
        print_log(ERROR, "failed to get device info\n");
        return -1;
    }

    if (device_info->version[0] == 7) {
        global_flags &= ~FLAG_FORCE_INSTALL;
    }

    if (device_info->version[2] == 0) {
        print_log(INFO, "%s on iOS %d.%d connected\n", device_info->product_type, device_info->version[0], device_info->version[1]);
    } else {
        print_log(INFO, "%s on iOS %d.%d.%d connected\n", device_info->product_type, device_info->version[0], device_info->version[1], device_info->version[2]);
    }

    if (has_flag(FLAG_INSTALL_LOG) || has_flag(FLAG_SYSTEM_LOG) || has_flag(FLAG_REBOOT)) {
        if (has_flag(FLAG_INSTALL_LOG)) {
            afc_info_t *afc_info = afc_init(device);
            if (afc_info == NULL) {
                print_log(ERROR, "failed to connect to AFC\n");
                return -1;
            }
            
            afc_file_data_t *file = afc_read_file(afc_info, "aquila_log.txt");
            if (file == NULL) file = afc_read_file(afc_info, "/aquila_log.txt");
            if (file == NULL) {
                print_log(INFO, "no install log found\n");
            } else {
                fprintf(stdout, "%.*s\n", (int)file->size, (char *)file->bytes);
            }
        } else if (has_flag(FLAG_SYSTEM_LOG)) {
            syslog_info_t *syslog_info = syslog_init(device);
            if (syslog_info == NULL) return 0;

            syslog_print_logs(syslog_info);
            syslog_deinit(syslog_info);
        } else if (has_flag(FLAG_REBOOT)) {
            print_log(INFO, "rebooting device...\n");
            md_reboot_device(device);
        }
        return 0;
    }

    if (!is_device_supported(device_info)) {
        print_log(ERROR, "device version or architecture unsupported\n");
        return -1;
    }

    if (strcmp(device_info->activation, "Unactivated") == 0) {
        print_log(ERROR, "device is not activated, unable to continue\n");
        return -1;
    }

    afc_info_t *afc_info = afc_init(device);
    if (afc_info == NULL) {
        print_log(ERROR, "failed to connect to AFC\n");
        return -1;
    }

    if (jailbreak(device, device_info, afc_info) != 0) {
        print_log(ERROR, "failed to jailbreak device\n");
        return -1;
    }

    print_log(INFO, "done\n");
    return 0;
}
