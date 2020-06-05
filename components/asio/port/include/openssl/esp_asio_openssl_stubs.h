// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _ESP_ASIO_OPENSSL_STUBS_H
#define _ESP_ASIO_OPENSSL_STUBS_H

#include "internal/ssl_x509.h"
#include "internal/ssl_pkey.h"
#include "mbedtls/pem.h"
#include <stdint.h>

/**
 * @note This header contains openssl API which are NOT implemented, and are only provided
 * as stubs or no-operations to get the ASIO library compiled and working with most
 * practical use cases as an embedded application on ESP platform
 */

#ifdef __cplusplus
extern "C" {
#endif

// The most applicable OpenSSL version wrtt ASIO usage
#define OPENSSL_VERSION_NUMBER 0x10100001L
// SSLv2 methods not supported
// OpenSSL port supports: TLS_ANY, TLS_1, TLS_1_1, TLS_1_2, SSL_3
#define OPENSSL_NO_SSL2
#define SSL2_VERSION 0x0002

#define SSL_R_SHORT_READ 219
#define SSL_OP_ALL 0
#define SSL_OP_SINGLE_DH_USE 0
//#define OPENSSL_VERSION_NUMBER 0x10001000L
#define SSL_OP_NO_COMPRESSION 0
//#define LIBRESSL_VERSION_NUMBER 1
//#define PEM_R_NO_START_LINE 110
// Translates mbedTLS PEM parse error, used by ASIO
#define PEM_R_NO_START_LINE -MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT

#define SSL_OP_NO_SSLv2					0x01000000L
#define SSL_OP_NO_SSLv3					0x02000000L
#define SSL_OP_NO_TLSv1					0x04000000L

#define X509_FILETYPE_PEM	1
#define X509_FILETYPE_ASN1	2
#define SSL_FILETYPE_ASN1	X509_FILETYPE_ASN1
#define SSL_FILETYPE_PEM	X509_FILETYPE_PEM

#define NID_subject_alt_name 85

#define SSL_MODE_RELEASE_BUFFERS            0x00000000L
#define SSL_MODE_ENABLE_PARTIAL_WRITE       0x00000001L
#define SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER 0x00000002L

#define GEN_DNS		2
#define GEN_IPADD	7
#define V_ASN1_OCTET_STRING		4
#define V_ASN1_IA5STRING		22
#define NID_commonName 13

#define SSL_CTX_get_app_data(ctx) ((void*)SSL_CTX_get_ex_data(ctx, 0))

/**
* @brief Frees DH object -- not implemented
*
* Current implementation calls SSL_ASSERT
*
* @param r DH object
*/
void DH_free(DH *r);

/**
 * @brief Frees GENERAL_NAMES -- not implemented
 *
 * Current implementation calls SSL_ASSERT
 *
 * @param r GENERAL_NAMES object
 */
void GENERAL_NAMES_free(GENERAL_NAMES * gens);

/**
 * @brief Returns subject name from X509 -- not implemented
 *
 * Current implementation calls SSL_ASSERT
 *
 * @param r X509 object
 */
X509_NAME *X509_get_subject_name(X509 *a);

/**
 * @brief API provaded as declaration only
 *
 */
int	X509_STORE_CTX_get_error_depth(X509_STORE_CTX *ctx);

/**
 * @brief API provaded as declaration only
 *
 */
int X509_NAME_get_index_by_NID(X509_NAME *name, int nid, int lastpos);

/**
 * @brief API provaded as declaration only
 *
 */
X509_NAME_ENTRY *X509_NAME_get_entry(X509_NAME *name, int loc);

/**
 * @brief API provaded as declaration only
 *
 */
ASN1_STRING *X509_NAME_ENTRY_get_data(X509_NAME_ENTRY *ne);

/**
 * @brief API provaded as declaration only
 *
 */
void *X509_get_ext_d2i(X509 *x, int nid, int *crit, int *idx);

/**
 * @brief API provaded as declaration only
 *
 */
X509 *	X509_STORE_CTX_get_current_cert(X509_STORE_CTX *ctx);

/**
 * @brief Reads DH params from a bio object -- not implemented
 *
 * Current implementation calls SSL_ASSERT
 */
DH *PEM_read_bio_DHparams(BIO *bp, DH **x, pem_password_cb *cb, void *u);

/**
 * @brief API provaded as declaration only
 *
 */
void *	X509_STORE_CTX_get_ex_data(X509_STORE_CTX *ctx,int idx);

/**
 * @brief Sets DH params to ssl ctx -- not implemented
 *
 * Current implementation calls SSL_ASSERT
 */
int SSL_CTX_set_tmp_dh(SSL_CTX *ctx, const DH *dh);

/**
 * @brief Sets SSL mode -- not implemented
 *
 * Current implementation is no-op
 */
uint32_t SSL_set_mode(SSL *ssl, uint32_t mode);

/**
 * @brief API provaded as declaration only
 *
 */
void SSL_CTX_set_default_passwd_cb_userdata(SSL_CTX *ctx, void *data);

/**
 * @brief API provaded as declaration only
 *
 */
void SSL_CTX_set_default_passwd_cb(SSL_CTX *ctx, pem_password_cb *cb);

/**
 * @brief Clears any existing chain associated with the current certificate of ctx.
 *
 */
int SSL_CTX_clear_chain_certs(SSL_CTX *ctx);

#if defined(__cplusplus)
} /* extern C */
#endif

#endif /* _ESP_ASIO_OPENSSL_STUBS_H */