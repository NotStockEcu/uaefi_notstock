# Install script for directory: /tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/root/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aes.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aria.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/asn1.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/asn1write.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/base64.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/bignum.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/block_cipher.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/build_info.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/camellia.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ccm.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/chacha20.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/chachapoly.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/check_config.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cipher.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cmac.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/compat-2.x.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_x509.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_psa.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/constant_time.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ctr_drbg.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/debug.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/des.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/dhm.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecdh.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecdsa.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecjpake.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecp.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/entropy.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/error.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/gcm.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/hkdf.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/hmac_drbg.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/lms.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/mbedtls_config.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md5.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/net_sockets.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/nist_kw.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/oid.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pem.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pk.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs12.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs5.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs7.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform_time.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform_util.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/poly1305.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/private_access.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/psa_util.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ripemd160.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/rsa.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha1.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha256.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha3.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha512.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_cache.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_cookie.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_ticket.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/threading.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/timing.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/version.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_crl.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_crt.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/build_info.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_dependencies.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_composites.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_primitives.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_compat.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_config.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_common.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_extra.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_legacy.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_platform.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_se_driver.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_sizes.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_struct.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_types.h"
    "/tmp/claude-0/-home-user-uaefi-notstock/2507f29d-757d-5082-b9e8-bbbd97507528/scratchpad/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_values.h"
    )
endif()

