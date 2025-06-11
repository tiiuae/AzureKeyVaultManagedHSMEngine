cmake_minimum_required(VERSION 3.10)

# set the project name
project(e_akv)

# Check OpenSSL version
include(CheckIncludeFile)
include(CheckSymbolExists)
check_include_file("openssl/opensslv.h" HAVE_OPENSSLV_H)

if(HAVE_OPENSSLV_H)
  set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
  check_symbol_exists(OPENSSL_VERSION_NUMBER "openssl/opensslv.h" HAVE_OPENSSL_VERSION)
endif()

# Add OpenSSL 3 compatibility
add_definitions(-DOPENSSL_API_COMPAT=0x10100000L)

set(eakv_src
  ctrl.c
  dllmain.c
  e_akv_err.c
  key.c
  rsa.c
  ec.c
  base64.c
  curl.c
  pch.c
  )
add_library(eakv_obj OBJECT ${eakv_src})
set_property(TARGET eakv_obj PROPERTY POSITION_INDEPENDENT_CODE 1)
add_library(eakv SHARED $<TARGET_OBJECTS:eakv_obj>)
set_target_properties(eakv
  PROPERTIES OUTPUT_NAME e_akv PREFIX "" SUFFIX ".so"
  )
find_library(SSL_LIB libssl.so REQUIRED)
find_library(CRYPTO_LIB libcrypto.so REQUIRED)
find_library(CURL_LIB libcurl.so REQUIRED)
find_library(JSONC_LIB libjson-c.so REQUIRED)
target_link_libraries(eakv
  ${CRYPTO_LIB}
  ${SSL_LIB}
  ${CURL_LIB}
  ${JSONC_LIB}
  )

# Determine the correct installation path based on OpenSSL version
execute_process(
  COMMAND bash -c "pkg-config --modversion libcrypto | cut -d. -f1"
  OUTPUT_VARIABLE OPENSSL_MAJOR_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(OPENSSL_MAJOR_VERSION EQUAL 3)
  install(TARGETS eakv DESTINATION "/usr/lib/x86_64-linux-gnu/engines-3")
else()
  install(TARGETS eakv DESTINATION "/usr/lib/x86_64-linux-gnu/engines-1.1")
endif()
