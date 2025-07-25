/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#include "pch.h"

static const char *engine_akv_id = "e_akv";
static const char *engine_akv_name = "AKV/HSM engine";

#if OPENSSL_VERSION_NUMBER < 0x30000000L
static RSA_METHOD *akv_rsa_method = NULL;
static EC_KEY_METHOD *akv_eckey_method = NULL;
#else
/* OpenSSL 3 uses EVP_PKEY methods instead of low-level methods */
static EVP_PKEY_METHOD *akv_rsa_pkey_meth = NULL;
static EVP_PKEY_METHOD *akv_rsa_pss_pkey_meth = NULL;
#endif

int akv_idx = -1;
int rsa_akv_idx = -1;
int eckey_akv_idx = -1;
int pkey_akv_idx = -1;

/**
 * @brief Free AKV_KEY from EVP_PKEY ex_data, for OpenSSL 3.0 compatibility.
 *
 * @param parent EVP_PKEY parent key
 * @param ptr AKV_KEY pointer
 * @param ad Not Used
 * @param idx EVP_PKEY external data index
 * @param argl Not Used
 * @param argp Not Used
 */
static void destroy_akv_key_ex(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                              int idx, long argl, void *argp)
{
    AKV_KEY *akv_key = (AKV_KEY *)ptr;
    if (akv_key)
    {
        destroy_akv_key(akv_key);
    }
}

/**
 * @brief Free RSA context, paired with RSA_set_ex_data in akv_load_privkey.
 *
 * @param rsa RSA context
 * @return 0 == success, 1 == failure
 */
int akv_rsa_free(RSA *rsa)
{
    AKV_KEY *akv_key = NULL;
    
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    typedef int (*PFN_RSA_meth_finish)(RSA * rsa);
    const RSA_METHOD *ossl_rsa_meth = RSA_PKCS1_OpenSSL();
    PFN_RSA_meth_finish pfn_rsa_meth_finish = RSA_meth_get_finish(ossl_rsa_meth);
    if (pfn_rsa_meth_finish)
    {
        pfn_rsa_meth_finish(rsa);
    }
#endif

    akv_key = RSA_get_ex_data(rsa, rsa_akv_idx);

    if (!akv_key)
    {
        return 1;
    }

    destroy_akv_key(akv_key);
    RSA_set_ex_data(rsa, rsa_akv_idx, NULL);
    return 1;
}

/**
 * @brief Free EC_KEY context, paired with EC_KEY_set_ex_data in akv_load_privkey.
 *
 * @param eckey EC_KEY context
 */
void akv_eckey_free(EC_KEY *eckey)
{
    AKV_KEY *akv_key;
    akv_key = EC_KEY_get_ex_data(eckey, eckey_akv_idx);
    // Not our key. First time we do EC_KEY_set_method
    // actually goes through here.
    if (!akv_key)
    {
        return;
    }

    destroy_akv_key(akv_key);
    EC_KEY_set_ex_data(eckey, eckey_akv_idx, NULL);
}

/**
 * @brief Set up engine for AKV/HSM.
 *
 * @param e Engine
 * @return 1 == success, 0 == failure
 */
static int akv_init(ENGINE *e)
{
    if (akv_idx < 0)
    {
        akv_idx = ENGINE_get_ex_new_index(0, NULL, NULL, NULL, 0);
        if (akv_idx < 0)
            goto err;

        /* Setup RSA index */
        rsa_akv_idx = RSA_get_ex_new_index(0, NULL, NULL, NULL, 0);
        if (rsa_akv_idx < 0)
            goto err;

        /* Setup EVP_PKEY index for OpenSSL 3.0 compatibility */
        /* Temporarily disabled - not currently used
        pkey_akv_idx = EVP_PKEY_get_ex_new_index(0, NULL, NULL, NULL, destroy_akv_key_ex);
        if (pkey_akv_idx < 0)
            goto err;
        */

#if OPENSSL_VERSION_NUMBER < 0x30000000L
        /* Setup RSA_METHOD for OpenSSL 1.1 */
        RSA_meth_set_priv_dec(akv_rsa_method, akv_rsa_priv_dec);
        RSA_meth_set_priv_enc(akv_rsa_method, akv_rsa_priv_enc);
        RSA_meth_set_finish(akv_rsa_method, akv_rsa_free);

        /* Setup EC_METHOD */
        int (*old_eckey_sign_setup)(EC_KEY *, BN_CTX *, BIGNUM **, BIGNUM **) = NULL;
        EC_KEY_METHOD_get_sign(EC_KEY_OpenSSL(), NULL, &old_eckey_sign_setup, NULL);
        if (!old_eckey_sign_setup)
        {
            goto err;
        }

        eckey_akv_idx = EC_KEY_get_ex_new_index(0, NULL, NULL, NULL, 0);
        if (eckey_akv_idx < 0)
            goto err;

        EC_KEY_METHOD_set_init(akv_eckey_method, NULL, akv_eckey_free, NULL, NULL, NULL, NULL);
        EC_KEY_METHOD_set_sign(akv_eckey_method, akv_eckey_sign, old_eckey_sign_setup,
                               akv_eckey_sign_sig);
#else
        /* For OpenSSL 3, we use EVP_PKEY methods instead */
        eckey_akv_idx = EC_KEY_get_ex_new_index(0, NULL, NULL, NULL, 0);
        if (eckey_akv_idx < 0)
            goto err;
#endif
    }

    return 1;

err:
    AKVerr(AKV_F_INIT, AKV_R_ALLOC_FAILURE);
    return 0;
}

/**
 * @brief Free any resouces associated with AKV/HSM.
 *
 * @param e Engine
 * @return 1 == success, 0 == failure
 */
static int akv_finish(ENGINE *e)
{
    return 1;
}

/**
 * @brief Free engine methods
 *
 * @param e Engine
 * @return 1 == success, 0 == failure
 */
static int akv_destroy(ENGINE *e)
{
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    if (akv_rsa_method)
    {
        RSA_meth_free(akv_rsa_method);
        akv_rsa_method = NULL;
    }

    if (akv_eckey_method)
    {
        EC_KEY_METHOD_free(akv_eckey_method);
        akv_eckey_method = NULL;
    }
#endif

    /* Only free these methods if they were actually created */
    if (akv_rsa_pkey_meth != NULL)
    {
        EVP_PKEY_meth_free(akv_rsa_pkey_meth);
        akv_rsa_pkey_meth = NULL;
    }
    
    if (akv_rsa_pss_pkey_meth != NULL)
    {
        EVP_PKEY_meth_free(akv_rsa_pss_pkey_meth);
        akv_rsa_pss_pkey_meth = NULL;
    }

    ERR_unload_AKV_strings();
    return 1;
}

/**
 * @brief Load public key from AKV/HSM.
 *
 * @param key_id Key ID to load, e.g. "<vault type>:<keyvault name>:<key name>"
 * @param pevpkey Public key
 * @return 1 == success, 0 == failure
 */
static int load_key(const char *key_id, EVP_PKEY **pevpkey)
{
    *pevpkey = NULL;

    AKV_KEY *key = NULL;
    EVP_PKEY *pkey = NULL;

    char keyvault_type[KEY_ID_MAX_SIZE + 1];
    char keyvault_name[KEY_ID_MAX_SIZE + 1];
    char key_name[KEY_ID_MAX_SIZE + 1];
#ifdef _WIN32
    int scanned = sscanf_s(key_id,
                           "%[^:]:%[^:]:%[^:]",
                           keyvault_type, KEY_ID_MAX_SIZE,
                           keyvault_name, KEY_ID_MAX_SIZE,
                           key_name, KEY_ID_MAX_SIZE);
#else
    int scanned = sscanf(key_id,
                         "%[^:]:%[^:]:%[^:]",
                         keyvault_type,
                         keyvault_name,
                         key_name);
#endif
    if (scanned != 3)
    {
        AKVerr(AKV_F_LOAD_KEY_CERT, AKV_R_PARSE_KEY_ID_ERROR);
        goto err;
    }

    if (strcasecmp(keyvault_type, "managedHsm") != 0 && strcasecmp(keyvault_type, "vault") != 0)
    {
        AKVerr(AKV_F_LOAD_KEY_CERT, AKV_R_PARSE_KEY_ID_ERROR);
        goto err;
    }

    key = acquire_akv_key(keyvault_type, keyvault_name, key_name);
    if (!key)
    {
        AKVerr(AKV_F_LOAD_KEY_CERT, AKV_R_CANT_GET_KEY);
        goto err;
    }

    MemoryStruct accessToken = {0};
    if (!GetAccessTokenFromIMDS(keyvault_type, &accessToken))
    {
        goto err;
    }

    pkey = AkvGetKey(keyvault_type, keyvault_name, key_name, &accessToken);
    if (!pkey)
    {
        AKVerr(AKV_F_LOAD_KEY_CERT, AKV_R_LOAD_PUBKEY_ERROR);
        goto err;
    }

    if (EVP_PKEY_id(pkey) == EVP_PKEY_RSA)
    {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        // In OpenSSL 3.0, get non-const RSA for method setting
        RSA *rsa = (RSA *)EVP_PKEY_get0_RSA(pkey);
#else
        RSA *rsa = EVP_PKEY_get0_RSA(pkey);
#endif
        if (!rsa)
        {
            AKVerr(AKV_F_LOAD_KEY_CERT, AKV_R_INVALID_RSA);
            goto err;
        }

        // Set RSA method and ex_data for both OpenSSL 1.1.x and 3.0
        // This ensures certificate generation works properly
#if OPENSSL_VERSION_NUMBER < 0x30000000L
        RSA_set_method(rsa, akv_rsa_method);
#endif
        RSA_set_ex_data(rsa, rsa_akv_idx, key);
        key = NULL; // Key is now owned by RSA object
    }
    else if (EVP_PKEY_id(pkey) == EVP_PKEY_EC)
    {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        // In OpenSSL 3.0, get non-const EC_KEY for method setting
        EC_KEY *ec = (EC_KEY *)EVP_PKEY_get0_EC_KEY(pkey);
#else
        EC_KEY *ec = EVP_PKEY_get0_EC_KEY(pkey);
#endif
        if (!ec)
        {
            AKVerr(AKV_F_LOAD_KEY_CERT, AKV_R_INVALID_EC_KEY);
            goto err;
        }

#if OPENSSL_VERSION_NUMBER < 0x30000000L
        EC_KEY_set_method(ec, akv_eckey_method);
#endif
        EC_KEY_set_ex_data(ec, eckey_akv_idx, key);
        key = NULL; // Key is now owned by EC object
    }
    else
    {
        AKVerr(AKV_F_LOAD_KEY_CERT, AKV_R_UNSUPPORTED_KEY_ALGORITHM);
        goto err;
    }

    *pevpkey = pkey;
    free(accessToken.memory);
    return 1;
err:
    // Unref key if we're not keeping it.
    if (pkey)
        EVP_PKEY_free(pkey);
    if (key)
        destroy_akv_key(key);
    if (accessToken.memory)
        free(accessToken.memory);
    return 0;
}

/**
 * @brief Load public key from AKV/HSM.
 *
 * @param eng Engine
 * @param key_id Key ID to load, e.g. "<vault type>:<keyvault name>:<key name>"
 * @param ui_method Not used
 * @param callback_data  Not used
 * @return Public key == success, NULL == failure
 */
static EVP_PKEY *akv_load_pubkey(ENGINE *eng, const char *key_id,
                                 UI_METHOD *ui_method, void *callback_data)
{
    EVP_PKEY *pkey = NULL;

    load_key(key_id, &pkey);
    return pkey;
}

/**
 * @brief Load private key from AKV/HSM.
 *
 * @param eng Engine
 * @param key_id Key ID to load, e.g. "<vault type>:<keyvault name>:<key name>"
 * @param ui_method Not used
 * @param callback_data  Not used
 * @return Private key == success, NULL == failure
 */
static EVP_PKEY *akv_load_privkey(ENGINE *eng, const char *key_id,
                                  UI_METHOD *ui_method, void *callback_data)
{
    EVP_PKEY *pkey = NULL;

    load_key(key_id, &pkey);
    return pkey;
}

// This function returns either nids table or methods table.

/**
 * @brief Setup engine methods
 *
 * @param e Engine
 * @param pmeth methods table
 * @param nids nids table
 * @param nid RSA or EC
 * @return 1 ==
 */
static int akv_pkey_meths(ENGINE *e, EVP_PKEY_METHOD **pmeth,
                          const int **nids, int nid)
{
    if (!pmeth)
    {
        static int akv_pkey_nids[] = {
            EVP_PKEY_RSA,
            EVP_PKEY_RSA_PSS,
            EVP_PKEY_EC,
        };

        *nids = akv_pkey_nids;
        return sizeof(akv_pkey_nids) / sizeof(akv_pkey_nids[0]);
    }

    if (nid == EVP_PKEY_RSA)
    {
        /* Create the method only once and store it in the static variable */
        if (!akv_rsa_pkey_meth)
        {
            akv_rsa_pkey_meth = EVP_PKEY_meth_new(EVP_PKEY_RSA, 0);
            EVP_PKEY_meth_copy(akv_rsa_pkey_meth,
                               EVP_PKEY_meth_find(EVP_PKEY_RSA));
            EVP_PKEY_meth_set_sign(akv_rsa_pkey_meth, 0,
                                   akv_pkey_rsa_sign);
        }

        *pmeth = akv_rsa_pkey_meth;
        return 1;
    }
    else if (nid == EVP_PKEY_RSA_PSS)
    {
        /* Create the method only once and store it in the static variable */
        if (!akv_rsa_pss_pkey_meth)
        {
            akv_rsa_pss_pkey_meth = EVP_PKEY_meth_new(EVP_PKEY_RSA, 0);
            EVP_PKEY_meth_copy(akv_rsa_pss_pkey_meth,
                               EVP_PKEY_meth_find(EVP_PKEY_RSA_PSS));

            int (*old_sign_init)(EVP_PKEY_CTX *) = NULL;
            EVP_PKEY_meth_get_sign(akv_rsa_pss_pkey_meth,
                                   &old_sign_init, NULL);
            EVP_PKEY_meth_set_sign(akv_rsa_pss_pkey_meth,
                                   old_sign_init, akv_pkey_rsa_sign);
        }

        *pmeth = akv_rsa_pss_pkey_meth;
        return 1;
    }
    else if (nid == EVP_PKEY_EC)
    {
        // Unchanged.
        *pmeth = (EVP_PKEY_METHOD *)EVP_PKEY_meth_find(nid);
        return 1;
    }

    *pmeth = NULL;
    return 0;
}

/**
 * @brief Bind engine to OpenSSL
 *
 * @param e Engine
 * @return 1 == success, 0 == failure
 */
static int bind_akv(ENGINE *e)
{
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    akv_rsa_method = RSA_meth_dup(RSA_PKCS1_OpenSSL());
    if (!akv_rsa_method)
        goto memerr;
    RSA_meth_set1_name(akv_rsa_method, "AKV RSA method");

    akv_eckey_method = EC_KEY_METHOD_new(EC_KEY_OpenSSL());
    if (!akv_eckey_method)
        goto memerr;
#endif

    int ret = 1;
    ret = ENGINE_set_id(e, engine_akv_id) &&
          ENGINE_set_name(e, engine_akv_name) &&
          ENGINE_set_flags(e, ENGINE_FLAGS_NO_REGISTER_ALL) &&
          ENGINE_set_init_function(e, akv_init) &&
          ENGINE_set_finish_function(e, akv_finish) &&
          ENGINE_set_destroy_function(e, akv_destroy);

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    ret = ret && ENGINE_set_RSA(e, akv_rsa_method) &&
               ENGINE_set_EC(e, akv_eckey_method);
#endif

    ret = ret && ENGINE_set_load_privkey_function(e, akv_load_privkey) &&
               ENGINE_set_load_pubkey_function(e, akv_load_pubkey) &&
               ENGINE_set_pkey_meths(e, akv_pkey_meths) &&
               ENGINE_set_cmd_defns(e, akv_cmd_defns) &&
               ENGINE_set_ctrl_function(e, akv_ctrl);
    
    if (!ret)
        goto memerr;

    ERR_load_AKV_strings();
    return 1;
memerr:
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    if (akv_rsa_method)
    {
        RSA_meth_free(akv_rsa_method);
        akv_rsa_method = NULL;
    }
    
    if (akv_eckey_method)
    {
        EC_KEY_METHOD_free(akv_eckey_method);
        akv_eckey_method = NULL;
    }
#endif

    return 0;
}

/**
 * @brief Helper function to load engine
 *
 * @param e Engine
 * @param id Engine ID
 * @return 1 == success, 0 == failure
 */
static int bind_helper(ENGINE *e, const char *id)
{
    if (id && (strcmp(id, engine_akv_id) != 0))
        return 0;
    if (!bind_akv(e))
        return 0;
    return 1;
}

#if OPENSSL_VERSION_NUMBER < 0x30000000L
IMPLEMENT_DYNAMIC_CHECK_FN()
IMPLEMENT_DYNAMIC_BIND_FN(bind_helper)
#else
/* OpenSSL 3.0 dynamic engine loading */
static int bind_fn(ENGINE *e, const char *id)
{
    if (id && (strcmp(id, engine_akv_id) != 0))
        return 0;
    if (!bind_akv(e))
        return 0;
    return 1;
}

IMPLEMENT_DYNAMIC_BIND_FN(bind_fn)
IMPLEMENT_DYNAMIC_CHECK_FN()
#endif

#ifdef _WIN32
/**
 * @brief DLL entry point
 *
 * @param hModule Module handle
 * @param ul_reason_for_call unused
 * @param lpReserved unused
 * @return TRUE == success, FALSE == failure
 */
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
#endif
