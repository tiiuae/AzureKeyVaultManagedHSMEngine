/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#ifndef OPENSSL3_COMPAT_H
#define OPENSSL3_COMPAT_H

#include <openssl/opensslv.h>

/*
 * This header provides compatibility macros and functions
 * to help bridge between OpenSSL 1.1.x and OpenSSL 3.x APIs
 */

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
/* OpenSSL 3.0 compatibility functions */

/* 
 * In OpenSSL 3.0, many low-level APIs are deprecated in favor of
 * the provider interface. This file helps maintain compatibility
 * with both versions.
 */

#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

#endif /* OPENSSL3_COMPAT_H */
