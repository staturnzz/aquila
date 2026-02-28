#include "common.h"
#include "device.h"
#include "afc.h"
#include "jailbreak.h"
#include "util.h"

uint16_t global_flags = FLAG_NONE;

void print_help(void) {
    fprintf(stdout, "Usage: aquila [-jaobvnch]\n");
    fprintf(stdout, "   -j, --jailbreak \t\t install jailbreak (default)\n");
    fprintf(stdout, "   -f, --force-install \t\t install if already jailbroken\n");
    fprintf(stdout, "   -v, --verbose \t\t enable verbose logging\n");
    fprintf(stdout, "   -n, --no-color \t\t disable color in logs\n");
    fprintf(stdout, "   -c, --credits \t\t print credits\n");
    fprintf(stdout, "   -h, --help \t\t\t print help\n\n");
    exit(0);
}

void print_credits(void) {
    fprintf(stdout, "Credits:\n");
    fprintf(stdout, "   staturnz - jailbreak and exploit\n");
    fprintf(stdout, "   comex - ddi race condition and sandbox patch\n");
    fprintf(stdout, "   planetbeing - ios-jailbreak-patchfinder\n");
    fprintf(stdout, "   PanguTeam - CVE-2014-4461 (kernel exploit bug)\n");
    fprintf(stdout, "   evad3rs - amfid bypass method\n\n");
    exit(0);
}

void parse_args(int argc, char **argv) {
    const char *short_opts = "jfvnch";
    struct option long_opts[] = {
        {"jailbreak", no_argument, 0, 'j'},
        {"force-install", no_argument, 0, 'f'},
        {"verbose", no_argument, 0, 'v'},
        {"no-color", no_argument, 0, 'n'},
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
            case 'n': global_flags |= FLAG_NO_COLOR; break;
            case 'c': return print_credits();
            case 'h': return print_help();
            case '?': return print_help();
            default: exit(1);
        }
    }

    global_flags |= FLAG_JAILBREAK;
    if (!isatty(1) || !isatty(2)) global_flags |= FLAG_NO_COLOR;
}

int main(int argc, char **argv) {
    print_log(INFO, "aquila jailbreak for iOS 6 (v1.0)\n");
    parse_args(argc, argv);

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

    if (device_info->version[2] == 0) {
        print_log(INFO, "%s on iOS %d.%d connected\n", device_info->product_type, device_info->version[0], device_info->version[1]);
    } else {
        print_log(INFO, "%s on iOS %d.%d.%d connected\n", device_info->product_type, device_info->version[0], device_info->version[1], device_info->version[2]);
    }

    if (device_info->version[0] != 6) {
        print_log(ERROR, "device version is not supported\n");
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
