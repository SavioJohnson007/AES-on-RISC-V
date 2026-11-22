#ifndef AES_WRAPPER_H
#define AES_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

void encrypt_message(const uint8_t *plaintext,
                     uint8_t *ciphertext,
                     size_t length);

void decrypt_message(const uint8_t *ciphertext,
                     uint8_t *plaintext,
                     size_t length);

#endif