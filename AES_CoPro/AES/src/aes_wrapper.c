#include "aes/aes.h"
#include "string_utils.h"

#include "aes_wrapper.h"
/* --------------------------------------------------------------------------
 * Simple wrapper functions
 * -------------------------------------------------------------------------- */

static const uint8_t aes_key[AES_KEYLEN] = {
    0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
    0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
    0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,
    0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4
};

static const uint8_t aes_iv[AES_BLOCKLEN] = {
    0x00,0x01,0x02,0x03,
    0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,
    0x0C,0x0D,0x0E,0x0F
};

void encrypt_message(const uint8_t *plaintext,
                     uint8_t *ciphertext,
                     size_t length)
{
    struct AES_ctx ctx;

    AES_init_ctx_iv(&ctx, aes_key, aes_iv);

    memcpy(ciphertext, plaintext, length);

    AES_CBC_encrypt_buffer(&ctx, ciphertext, length);
}

void decrypt_message(const uint8_t *ciphertext,
                     uint8_t *plaintext,
                     size_t length)
{
    struct AES_ctx ctx;

    AES_init_ctx_iv(&ctx, aes_key, aes_iv);

    memcpy(plaintext, ciphertext, length);

    AES_CBC_decrypt_buffer(&ctx, plaintext, length);
}
