// Copyright lowRISC contributors.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

#include "../vivado/ibex-demo-system/sw/c/common/demo_system.h"
#include "../vivado/ibex-demo-system/sw/c/common/gpio.h"
#include "../vivado/ibex-demo-system/sw/c/common/uart.h"


#include <stdio.h>

#include <string.h>
#include "aes/aes.h"

/* 32-byte AES-256 key */
static const uint8_t key[32] = {
    0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
    0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
    0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,
    0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4
};

/* 16-byte IV */
static const uint8_t iv[16] = {
    0x00,0x01,0x02,0x03,
    0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,
    0x0C,0x0D,0x0E,0x0F
};

/*------------------------------------------------------------------
 * Encrypt
 * input  : plaintext
 * output : ciphertext
 * len    : must be a multiple of 16
 *------------------------------------------------------------------*/
void encrypt_message(const uint8_t *plaintext,
                     uint8_t *ciphertext,
                     uint32_t len)
{
    struct AES_ctx ctx;

    /* Copy because the AES library encrypts in-place */
    memcpy(ciphertext, plaintext, len);

    AES_init_ctx_iv(&ctx, key, iv);
    AES_CBC_encrypt_buffer(&ctx, ciphertext, len);
}

/*------------------------------------------------------------------
 * Decrypt
 * input  : ciphertext
 * output : plaintext
 * len    : must be a multiple of 16
 *------------------------------------------------------------------*/
void decrypt_message(const uint8_t *ciphertext,
                     uint8_t *plaintext,
                     uint32_t len)
{
    struct AES_ctx ctx;

    memcpy(plaintext, ciphertext, len);

    AES_init_ctx_iv(&ctx, key, iv);
    AES_CBC_decrypt_buffer(&ctx, plaintext, len);
}


// int main(void)
// {
//     uint8_t plaintext[16] = "Hello AES Demo!";
//     uint8_t ciphertext[16];
//     uint8_t decrypted[16];

//     encrypt_message(plaintext, ciphertext, 16);

//     decrypt_message(ciphertext, decrypted, 16);

//     printf("Original : %s\n", plaintext);
//     printf("Recovered: %s\n", decrypted);

//     return 0;
// }