#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* Basic options */
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT   /* Keep this if you plan to use a hardware RNG later */

/* Use SHA-256 */
#define MBEDTLS_SHA256_C

/* Use generic message-digest interface (mbedtls_md_xxx); HMAC is here too */
#define MBEDTLS_MD_C

/* Macros required for HMAC */
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_PADDING_PKCS7

/* Memory-related: disable dynamic allocation (optional) */
// #define MBEDTLS_PLATFORM_MEMORY
// #define MBEDTLS_PLATFORM_STD_CALLOC     your_calloc
// #define MBEDTLS_PLATFORM_STD_FREE       your_free

/* To enable debug options, add: */
// #define MBEDTLS_DEBUG_C

/* Disable modules you don't need to save Flash */
#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */
