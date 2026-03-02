#ifndef util_h
#define util_h

#include "common.h"

#if defined(WINDOWS_BUILD)
#define INFO 0
#define ERROR 1
#define WARNING 2
#define VERBOSE 3

#define CC_SHA1_DIGEST_LENGTH 20

typedef uint32_t log_type_t;
typedef struct {
    HCRYPTPROV hCryptProv;
    HCRYPTHASH hHash;
} CC_SHA1_CTX;

void usleep(int16_t usec);
char *cfstr_to_cstr(CFStringRef str);
CFStringRef cstr_to_cfstr(const char *str);
char *get_mobile_device_path(void);
int CC_SHA1_Init(CC_SHA1_CTX *ctx);
int CC_SHA1_Update(CC_SHA1_CTX *ctx, const void *data, size_t len);
int CC_SHA1_Final(unsigned char *hash, CC_SHA1_CTX *ctx);
unsigned char *CC_SHA1(const void *data, size_t len, unsigned char *hash);
#else
typedef enum {
    INFO = 0,
    ERROR,
    WARNING,
    VERBOSE
} log_type_t;
#endif

int platform_init(void);
void *load_embed_file(const char *name, size_t *size);
void print_log(log_type_t type, const char *format, ...);
CFNumberRef CFNUM(int32_t val);
void *sha1_init(void);
void sha1_update(void *ctx, void *data, size_t size);
uint8_t *sha1_final(void *ctx);
uint8_t *sha1_calculate(void *data, size_t size);
char *sha1_calculate_to_str(void *data, size_t size);

#endif /* util_h */