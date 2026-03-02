#include "util.h"

#if defined(WINDOWS_BUILD)
#include "afc.h"
#include "device.h"

static HMODULE cf_handle = NULL;
static HMODULE md_handle = NULL;

CFTypeRef kCFBooleanTrue = NULL;
CFTypeRef kCFBooleanFalse = NULL;
void (*CFRunLoopStop)(CFRunLoopRef rl) = NULL;
void (*CFRunLoopRun)(void) = NULL;
CFRunLoopRef (*CFRunLoopGetCurrent)(void) = NULL;
CFTypeID (*CFStringGetTypeID)(void) = NULL;
CFTypeID (*CFGetTypeID)(CFTypeRef cf) = NULL;
void (*CFRelease)(CFTypeRef cf) = NULL;
CFTypeRef (*CFRetain)(CFTypeRef cf) = NULL;
CFStringRef (*CFStringCreateWithCString)(CFAllocatorRef alloc, const char *cStr, CFStringEncoding encoding) = NULL;
bool (*CFStringGetCString)(CFStringRef theString, char *buffer, CFIndex bufferSize, CFStringEncoding encoding) = NULL;
CFMutableDictionaryRef (*CFDictionaryCreateMutable)(CFAllocatorRef allocator, CFIndex capacity, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks) = NULL;
void (*CFDictionaryAddValue)(CFMutableDictionaryRef theDict, const void *key, const void *value) = NULL;
const void *(*CFDictionaryGetValue)(CFDictionaryRef theDict, const void *key) = NULL;
bool (*CFEqual)(CFTypeRef cf1, CFTypeRef cf2) = NULL;
CFDataRef (*CFDataCreate)(CFAllocatorRef allocator, const uint8_t *bytes, CFIndex length) = NULL;
void (*CFDictionarySetValue)(CFMutableDictionaryRef theDict, const void *key, const void *value) = NULL;
CFIndex (*CFStringGetLength)(CFStringRef theString) = NULL;
bool (*CFNumberGetValue)(CFNumberRef number, uint32_t theType, void *valuePtr) = NULL;
CFNumberRef (*CFNumberCreate)(CFAllocatorRef allocator, uint32_t theType, const void *valuePtr) = NULL;
CFTypeID (*CFArrayGetTypeID)(void) = NULL;
CFIndex (*CFArrayGetCount)(CFArrayRef theArray) = NULL;
const void *(*CFArrayGetValueAtIndex)(CFArrayRef theArray, CFIndex idx) = NULL;
CFTypeID (*CFNumberGetTypeID)(void) = NULL;
CFMutableArrayRef (*CFArrayCreateMutable)(CFAllocatorRef allocator, CFIndex capacity, void *callBacks) = NULL;
void (*CFArrayAppendValue)(CFMutableArrayRef theArray, const void *value) = NULL;
CFDataRef (*CFPropertyListCreateData)(CFAllocatorRef allocator, CFPropertyListRef propertyList, CFPropertyListFormat format, uint32_t options, void *error) = NULL;
uint8_t *(*CFDataGetBytePtr)(CFDataRef theData) = NULL;
CFIndex (*CFDataGetLength)(CFDataRef theData) = NULL;
CFTypeID (*CFDictionaryGetTypeID)(void) = NULL;
void (*CFShow)(CFTypeRef object) = NULL;

static int load_symbol(const char *name, void **ptr) {
    void *symbol = (void *)GetProcAddress(cf_handle, name);
    if (symbol == NULL) symbol = (void *)GetProcAddress(md_handle, name);
    if (symbol == NULL) return -1;

    *ptr = symbol;
    return 0;
}

void usleep(int16_t usec) { 
    LARGE_INTEGER due = {0};
    due.QuadPart = -(10 * usec);

    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &due, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
}

char *cfstr_to_cstr(CFStringRef str) {
    if (str == NULL) return NULL;
    __cfstring_t *str_data = (__cfstring_t *)str;
    if (str_data->isa == 0 || str_data->len == 0) return NULL;

    char *buf = calloc(1, str_data->len+1);
    if (buf == NULL) return NULL;

    memcpy(buf, str_data->data, str_data->len);
    return buf;
}

CFStringRef cstr_to_cfstr(const char *str) {
    if (str == NULL) return NULL;
    CFStringRef cf_str = CFStringCreateWithCString(NULL, str, kCFStringEncodingASCII);

    if (cf_str == NULL) return NULL;
    CFRetain(cf_str);
    return cf_str;
}

char *get_mobile_device_path(void) {
    HKEY key = NULL;
    const char *key_name = "SOFTWARE\\Apple Inc.\\Apple Mobile Device Support";
    const char *entry_name = "InstallDir";

    char value[MAX_PATH] = {0};
    DWORD size = sizeof(value);
    DWORD type = 0;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key_name, 0, KEY_READ, &key) != ERROR_SUCCESS) goto fallback;
    if (RegQueryValueExA(key, entry_name, NULL, &type, (LPBYTE)&value, &size) != ERROR_SUCCESS) {
        RegCloseKey(key);
        goto fallback;
    }

    RegCloseKey(key);
    if (type != REG_SZ) goto fallback;

    if (value[strlen(value)-1] == '\\') {
        value[strlen(value)-1] = '\0';
    }
    return _strdup(value);

fallback:
    if (_access("C:\\Program Files\\Common Files\\Apple\\Mobile Device Support", 0) == 0) {
        return _strdup("C:\\Program Files\\Common Files\\Apple\\Mobile Device Support");
    }
    return NULL;
}

void add_search_path(char *path) {
    if (path == NULL) return;
    wchar_t wide_path[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path, MAX_PATH);
    AddDllDirectory(wide_path);
}

bool is_app_installed(const char *name) {
    if (system("powershell exit 0 > nul 2>&1") != 0) return false;
    char cmd[1024] = {0};
    snprintf(cmd, 1024-1, "powershell exit (Get-AppxPackage -Name %s).Length > nul 2>&1", name);
    return (system(cmd) != 0);
}

int CC_SHA1_Init(CC_SHA1_CTX *ctx) {
    if (!CryptAcquireContext(&ctx->hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) return -1;
    if (!CryptCreateHash(ctx->hCryptProv, CALG_SHA1, 0, 0, &ctx->hHash)) {
        CryptReleaseContext(ctx->hCryptProv, 0);
        return -1;
    }
    return 0;
}

int CC_SHA1_Update(CC_SHA1_CTX *ctx, const void *data, size_t len) {
    return (!CryptHashData(ctx->hHash, (const BYTE*)data, (DWORD)len, 0)) ? -1 : 0;
}

int CC_SHA1_Final(unsigned char* hash, CC_SHA1_CTX* ctx) {
    DWORD len = 20;
    if (!CryptGetHashParam(ctx->hHash, HP_HASHVAL, hash, &len, 0)) return -1;
    
    CryptDestroyHash(ctx->hHash);
    CryptReleaseContext(ctx->hCryptProv, 0);
    return 0;
}

unsigned char *CC_SHA1(const void *data, size_t len, unsigned char *hash) {
    CC_SHA1_CTX ctx = {0};
    if (CC_SHA1_Init(&ctx) != 0) return NULL;
    if (CC_SHA1_Update(&ctx, data, len) != 0) return NULL;
    if (CC_SHA1_Final(hash, &ctx) != 0) return NULL;
    return hash;
}
#endif

int platform_init(void) {
#if defined(WINDOWS_BUILD)
    char *md_path = get_mobile_device_path();
    if (md_path == NULL) return 1;
    add_search_path(md_path);
    free(md_path);

    if (is_app_installed("AppleInc.AppleTVWin")) return 2;
    if (is_app_installed("AppleInc.iTunes")) return 3;
    if (is_app_installed("AppleInc.AppleDevices")) return 4;
    if (is_app_installed("AppleInc.AppleMusicWin")) return 5;

    if (!SetDefaultDllDirectories(LOAD_FLAGS)) return -1;
    if ((cf_handle = LoadLibraryExA("CoreFoundation.dll", NULL, LOAD_FLAGS)) == NULL) return 6;
    if ((md_handle = LoadLibraryExA("MobileDevice.dll", NULL, LOAD_FLAGS)) == NULL) return 7;
    
    if (load_symbol("AMDeviceNotificationSubscribe", (void **)&AMDeviceNotificationSubscribe) != 0) return -1;
    if (load_symbol("AMDeviceNotificationUnsubscribe", (void **)&AMDeviceNotificationUnsubscribe) != 0) return -1;
    if (load_symbol("AMDeviceConnect", (void **)&AMDeviceConnect) != 0) return -1;
    if (load_symbol("AMDeviceIsPaired", (void **)&AMDeviceIsPaired) != 0) return -1;
    if (load_symbol("AMDevicePair", (void **)&AMDevicePair) != 0) return -1;
    if (load_symbol("AMDeviceValidatePairing", (void **)&AMDeviceValidatePairing) != 0) return -1;
    if (load_symbol("AMDeviceStartSession", (void **)&AMDeviceStartSession) != 0) return -1;
    if (load_symbol("AMDeviceCopyValue", (void **)&AMDeviceCopyValue) != 0) return -1;
    if (load_symbol("AMDeviceStartService", (void **)&AMDeviceStartService) != 0) return -1;
    if (load_symbol("AMDeviceStartServiceWithOptions", (void **)&AMDeviceStartServiceWithOptions) != 0) return -1;
    if (load_symbol("AMDeviceStopSession", (void **)&AMDeviceStopSession) != 0) return -1;
    if (load_symbol("USBMuxConnectByPort", (void **)&USBMuxConnectByPort) != 0) return -1;
    if (load_symbol("AMDeviceGetConnectionID", (void **)&AMDeviceGetConnectionID) != 0) return -1;
    if (load_symbol("AMDeviceSecureStartService", (void **)&AMDeviceSecureStartService) != 0) return -1;
    if (load_symbol("AMDServiceConnectionReceiveMessage", (void **)&AMDServiceConnectionReceiveMessage) != 0) return -1;
    if (load_symbol("AMDServiceConnectionReceive", (void **)&AMDServiceConnectionReceive) != 0) return -1;
    if (load_symbol("AMDServiceConnectionSendMessage", (void **)&AMDServiceConnectionSendMessage) != 0) return -1;
    if (load_symbol("AMDServiceConnectionSend", (void **)&AMDServiceConnectionSend) != 0) return -1;
    if (load_symbol("AMDeviceLookupApplications", (void **)&AMDeviceLookupApplications) != 0) return -1;

    void **kCFBooleanTrue_ptr = NULL;
    void **kCFBooleanFalse_ptr = NULL;
    if (load_symbol("kCFBooleanTrue", (void **)&kCFBooleanTrue_ptr) != 0) return -1;
    if (load_symbol("kCFBooleanFalse", (void **)&kCFBooleanFalse_ptr) != 0) return -1;
    kCFBooleanTrue = *kCFBooleanTrue_ptr;
    kCFBooleanFalse = *kCFBooleanFalse_ptr;

    if (load_symbol("CFRunLoopStop", (void **)&CFRunLoopStop) != 0) return -1;
    if (load_symbol("CFRunLoopRun", (void **)&CFRunLoopRun) != 0) return -1;
    if (load_symbol("CFRunLoopGetCurrent", (void **)&CFRunLoopGetCurrent) != 0) return -1;
    if (load_symbol("CFStringGetTypeID", (void **)&CFStringGetTypeID) != 0) return -1;
    if (load_symbol("CFGetTypeID", (void **)&CFGetTypeID) != 0) return -1;
    if (load_symbol("CFRelease", (void **)&CFRelease) != 0) return -1;
    if (load_symbol("CFRetain", (void **)&CFRetain) != 0) return -1;
    if (load_symbol("CFStringCreateWithCString", (void **)&CFStringCreateWithCString) != 0) return -1;
    if (load_symbol("CFStringGetCString", (void **)&CFStringGetCString) != 0) return -1;
    if (load_symbol("CFDictionaryCreateMutable", (void **)&CFDictionaryCreateMutable) != 0) return -1;
    if (load_symbol("CFDictionaryAddValue", (void **)&CFDictionaryAddValue) != 0) return -1;
    if (load_symbol("CFDictionaryGetValue", (void **)&CFDictionaryGetValue) != 0) return -1;
    if (load_symbol("CFEqual", (void **)&CFEqual) != 0) return -1;
    if (load_symbol("CFDataCreate", (void **)&CFDataCreate) != 0) return -1;
    if (load_symbol("CFDictionarySetValue", (void **)&CFDictionarySetValue) != 0) return -1;
    if (load_symbol("CFStringGetLength", (void **)&CFStringGetLength) != 0) return -1;
    if (load_symbol("CFNumberGetValue", (void **)&CFNumberGetValue) != 0) return -1;
    if (load_symbol("CFArrayGetTypeID", (void **)&CFArrayGetTypeID) != 0) return -1;
    if (load_symbol("CFArrayGetCount", (void **)&CFArrayGetCount) != 0) return -1;
    if (load_symbol("CFArrayGetValueAtIndex", (void **)&CFArrayGetValueAtIndex) != 0) return -1;
    if (load_symbol("CFNumberGetTypeID", (void **)&CFNumberGetTypeID) != 0) return -1;
    if (load_symbol("CFNumberCreate", (void **)&CFNumberCreate) != 0) return -1;
    if (load_symbol("CFArrayCreateMutable", (void **)&CFArrayCreateMutable) != 0) return -1;
    if (load_symbol("CFArrayAppendValue", (void **)&CFArrayAppendValue) != 0) return -1;
    if (load_symbol("CFPropertyListCreateData", (void **)&CFPropertyListCreateData) != 0) return -1;
    if (load_symbol("CFDataGetBytePtr", (void **)&CFDataGetBytePtr) != 0) return -1;
    if (load_symbol("CFDataGetLength", (void **)&CFDataGetLength) != 0) return -1;
    if (load_symbol("CFDictionaryGetTypeID", (void **)&CFDictionaryGetTypeID) != 0) return -1;
    if (load_symbol("CFShow", (void **)&CFShow) != 0) return -1;

    if (load_symbol("AFCConnectionOpen", (void **)&AFCConnectionOpen) != 0) return -1;
    if (load_symbol("AFCDeviceInfoOpen", (void **)&AFCDeviceInfoOpen) != 0) return -1;
    if (load_symbol("AFCDirectoryOpen", (void **)&AFCDirectoryOpen) != 0) return -1;
    if (load_symbol("AFCDirectoryRead", (void **)&AFCDirectoryRead) != 0) return -1;
    if (load_symbol("AFCDirectoryClose", (void **)&AFCDirectoryClose) != 0) return -1;
    if (load_symbol("AFCDirectoryCreate", (void **)&AFCDirectoryCreate) != 0) return -1;
    if (load_symbol("AFCRemovePath", (void **)&AFCRemovePath) != 0) return -1;
    if (load_symbol("AFCRenamePath", (void **)&AFCRenamePath) != 0) return -1;
    if (load_symbol("AFCLinkPath", (void **)&AFCLinkPath) != 0) return -1;
    if (load_symbol("AFCFileRefOpen", (void **)&AFCFileRefOpen) != 0) return -1;
    if (load_symbol("AFCFileRefRead", (void **)&AFCFileRefRead) != 0) return -1;
    if (load_symbol("AFCFileRefWrite", (void **)&AFCFileRefWrite) != 0) return -1;
    if (load_symbol("AFCFileRefClose", (void **)&AFCFileRefClose) != 0) return -1;
    if (load_symbol("AFCFileInfoOpen", (void **)&AFCFileInfoOpen) != 0) return -1;
    if (load_symbol("AFCConnectionClose", (void **)&AFCConnectionClose) != 0) return -1;
    if (load_symbol("AFCLinkPath", (void **)&AFCLinkPath) != 0) return -1;
    if (load_symbol("AFCFileRefRead", (void **)&AFCFileRefRead) != 0) return -1;
#endif
    return 0;
}

void *load_embed_file(const char *name, size_t *size) {
#if defined(WINDOWS_BUILD)
    char *rc_name = strdup(name);
    for (uint32_t i = 0; i < strlen(rc_name); i++) {
        rc_name[i] = toupper(rc_name[i]);
    }
    
    HMODULE module = GetModuleHandle(NULL);
    HRSRC resource = FindResourceA(NULL, rc_name, RT_RCDATA);
    free(rc_name);
    if (resource == NULL) return NULL;

    HGLOBAL global_data = LoadResource(module, resource);
    *size = (size_t)SizeofResource(module, resource);
    return LockResource(global_data);
#else
    struct mach_header_64 *hdr = &_mh_execute_header;
    struct load_command *load_cmd = (struct load_command *)(hdr + 1);

    for (int i = 0; i < hdr->ncmds; i++) {
        if (load_cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg_cmd = (struct segment_command_64 *)load_cmd;
            if (strcmp(seg_cmd->segname, "__DATA") == 0) {
                struct section_64 *sect = (struct section_64 *)(seg_cmd + 1);

                for (uint32_t j = 0; j < seg_cmd->nsects; j++) {
                    if (strcmp(sect->sectname, name) == 0) {
                        *size = sect->size;
                        return (void *)((uint8_t *)hdr + sect->offset);
                    }
                    sect++;
                }
            }
        }
        load_cmd = (struct load_command *)((uint64_t)load_cmd + load_cmd->cmdsize);
    }
#endif
    return NULL;
}

void print_prefix(uint32_t type, FILE *file, void *handle) {
    bool ansi_color = ((global_flags & FLAG_ANSI_COLOR) == FLAG_ANSI_COLOR);
    bool win_color = ((global_flags & FLAG_WINCONSOLE_COLOR) == FLAG_WINCONSOLE_COLOR);

    switch (type) {
        case INFO: {
            if (ansi_color) fprintf(file, "[\x1B[36mINFO\x1B[0m] ");
#if defined(WINDOWS_BUILD)
            else if (win_color) {fprintf(file, "[");SetTextColor((HANDLE)handle, 0xB);fprintf(file, "INFO");SetTextColor((HANDLE)handle, 0x7);fprintf(file, "] ");}
#endif
            else fprintf(file, "[INFO] ");
        } break;
        case ERROR: {
            if (ansi_color) fprintf(file, "[\x1B[31mERROR\x1B[0m] ");
#if defined(WINDOWS_BUILD)
            else if (win_color) {fprintf(file, "[");SetTextColor((HANDLE)handle, 0x4);fprintf(file, "ERROR");SetTextColor((HANDLE)handle, 0x7);fprintf(file, "] ");}
#endif
            else fprintf(file, "[INFO] ");
        } break;
        case WARNING: {
            if (ansi_color) fprintf(file, "[\x1B[33mWARNING\x1B[0m] ");
#if defined(WINDOWS_BUILD)
            else if (win_color) {fprintf(file, "[");SetTextColor((HANDLE)handle, 0xE);fprintf(file, "WARNING");SetTextColor((HANDLE)handle, 0x7);fprintf(file, "] ");}
#endif
            else fprintf(file, "[INFO] ");
        } break;
        case VERBOSE: {
            if (ansi_color) fprintf(file, "[\x1B[35mVERBOSE\x1B[0m] ");
#if defined(WINDOWS_BUILD)
            else if (win_color) {fprintf(file, "[");SetTextColor((HANDLE)handle, 0xD);fprintf(file, "VERBOSE");SetTextColor((HANDLE)handle, 0x7);fprintf(file, "] ");}
#endif
            else fprintf(file, "[INFO] ");
        } break;
        default: break;
    }
}

void print_log(log_type_t type, const char *format, ...) {    
    va_list va;
    FILE *file = NULL;
    void *handle = NULL;

    switch (type) {
        case INFO: 
            file = stdout;
#if defined(WINDOWS_BUILD)
            handle = (void *)GetStdHandle(STD_OUTPUT_HANDLE);
#endif
            break;
        case ERROR: 
            file = stderr;
#if defined(WINDOWS_BUILD)
            handle = (void *)GetStdHandle(STD_ERROR_HANDLE);
#endif
            break;
        case WARNING: 
            file = stderr;
#if defined(WINDOWS_BUILD)
            handle = (void *)GetStdHandle(STD_ERROR_HANDLE);
#endif
            break;
        case VERBOSE:
            if ((global_flags & FLAG_VERBOSE_LOGGING) == 0) return;
            file = stdout;
#if defined(WINDOWS_BUILD)
            handle = (void *)GetStdHandle(STD_OUTPUT_HANDLE);
#endif
            break;
        default:
            return;
    }

    if (file != NULL) {
        print_prefix(type, file, handle);
        va_start(va, format);
        vfprintf(file, format, va);
        va_end(va);
    }
}

CFNumberRef CFNUM(int32_t val) {
    return CFNumberCreate(NULL, 3, &val);
}

void *sha1_init(void) {
    CC_SHA1_CTX *ctx = calloc(1, sizeof(CC_SHA1_CTX));
    CC_SHA1_Init(ctx);
    return ctx;
}

void sha1_update(void *ctx, void *data, size_t size) {
    CC_SHA1_Update(ctx, data, size);
}

uint8_t *sha1_final(void *ctx) {
    uint8_t *hash = calloc(1, CC_SHA1_DIGEST_LENGTH);
    CC_SHA1_Final(hash, ctx);
    free(ctx);
    return hash;
}

uint8_t *sha1_calculate(void *data, size_t size) {
    uint8_t *hash = calloc(1, CC_SHA1_DIGEST_LENGTH);
    CC_SHA1(data, size, hash);
    return hash;
}

char *sha1_calculate_to_str(void *data, size_t size) {
    uint8_t hash[CC_SHA1_DIGEST_LENGTH] = {0};
    CC_SHA1(data, size, hash);

    char *hash_str = calloc(1, 64);
    for (int i = 0; i < CC_SHA1_DIGEST_LENGTH; ++i){
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    return hash_str;
}