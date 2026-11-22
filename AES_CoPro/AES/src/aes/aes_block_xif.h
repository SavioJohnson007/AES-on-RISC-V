#ifndef AES_BLOCK_XIF_H
#define AES_BLOCK_XIF_H

#include <stdint.h>

#define AES_XIF_OPCODE 0x2b
#define AES_XIF_KEY    0
#define AES_XIF_BLOCK  1
#define AES_XIF_START  2
#define AES_XIF_GET    3

static inline void aes_xif_key_word(uint32_t word, uint32_t value,
                                    uint32_t key_size)
{
    if (word == 0) {
        switch (key_size) {
        case 0: asm volatile (".insn r 0x2b, 0, 0, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        case 1: asm volatile (".insn r 0x2b, 0, 1, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        case 2: asm volatile (".insn r 0x2b, 0, 2, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        default: break;
        }
    } else {
        switch (word) {
        case 1: asm volatile (".insn r 0x2b, 0, 4, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        case 2: asm volatile (".insn r 0x2b, 0, 8, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        case 3: asm volatile (".insn r 0x2b, 0, 12, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        case 4: asm volatile (".insn r 0x2b, 0, 16, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        case 5: asm volatile (".insn r 0x2b, 0, 20, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        case 6: asm volatile (".insn r 0x2b, 0, 24, x0, %0, x0"
                              :: "r" (value) : "memory"); break;
        default: asm volatile (".insn r 0x2b, 0, 28, x0, %0, x0"
                               :: "r" (value) : "memory"); break;
        }
    }
}

static inline void aes_xif_block_word(uint32_t word, uint32_t value)
{
    switch (word) {
    case 0: asm volatile (".insn r 0x2b, 0, 32, x0, %0, x0" :: "r" (value) : "memory"); break;
    case 1: asm volatile (".insn r 0x2b, 0, 36, x0, %0, x0" :: "r" (value) : "memory"); break;
    case 2: asm volatile (".insn r 0x2b, 0, 40, x0, %0, x0" :: "r" (value) : "memory"); break;
    default: asm volatile (".insn r 0x2b, 0, 44, x0, %0, x0" :: "r" (value) : "memory"); break;
    }
}

static inline void aes_xif_start(int decrypt)
{
    if (decrypt) {
        asm volatile (".insn r 0x2b, 0, 66, x0, x0, x0" ::: "memory");
    } else {
        asm volatile (".insn r 0x2b, 0, 64, x0, x0, x0" ::: "memory");
    }
}

static inline uint32_t aes_xif_result_word(uint32_t word)
{
    uint32_t result;
    switch (word) {
    case 0: asm volatile (".insn r 0x2b, 0, 96, %0, x0, x0" : "=r" (result) :: "memory"); break;
    case 1: asm volatile (".insn r 0x2b, 0, 100, %0, x0, x0" : "=r" (result) :: "memory"); break;
    case 2: asm volatile (".insn r 0x2b, 0, 104, %0, x0, x0" : "=r" (result) :: "memory"); break;
    default: asm volatile (".insn r 0x2b, 0, 108, %0, x0, x0" : "=r" (result) :: "memory"); break;
    }
    return result;
}

static inline void aes_xif_key(const uint32_t *words, uint32_t key_size)
{
    if (key_size > 2) {
        return;
    }
    uint32_t word_count = (key_size == 0) ? 4 : (key_size == 1 ? 6 : 8);

    /* KEY0 establishes the key size in the coprocessor. */
    aes_xif_key_word(0, words[0], key_size);

    /* KEY1..KEY(N-1) complete the selected AES key. */
    for (uint32_t i = 1; i < word_count; ++i) {
        aes_xif_key_word(i, words[i], key_size);
    }
}

static inline void aes_xif_block(const uint32_t words[4])
{
    for (uint32_t i = 0; i < 4; ++i) {
        aes_xif_block_word(i, words[i]);
    }
}

static inline void aes_xif_crypt(const uint32_t input[4], uint32_t output[4],
                                 int decrypt)
{
    aes_xif_block(input);
    aes_xif_start(decrypt);
    for (uint32_t i = 0; i < 4; ++i) {
        output[i] = aes_xif_result_word(i);
    }
}

/*
 * Byte-oriented interface used by tiny-AES.  The coprocessor transfers words
 * most-significant byte first, whereas Ibex memory words are little-endian;
 * do the conversion explicitly instead of casting a uint8_t buffer.
 */
static inline uint32_t aes_xif_load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static inline void aes_xif_store_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static inline void aes_xif_crypt_block(const uint8_t *key, uint32_t key_size,
                                       uint8_t block[16], int decrypt)
{
    uint32_t input[4];
    uint32_t output[4];
    uint32_t key_words[8] = {0};
    uint32_t words = key_size == 0 ? 4 : (key_size == 1 ? 6 : 8);
    uint32_t i;

    for (i = 0; i < words; ++i) key_words[i] = aes_xif_load_be32(key + 4 * i);
    for (i = 0; i < 4; ++i) input[i] = aes_xif_load_be32(block + 4 * i);

    /*
     * KEY0 carries the AES key-size field, so it must be issued first.
     * The coprocessor latches the key size from KEY0 and accepts the
     * remaining key words using that latched size.  Key expansion is
     * triggered only when the final required word arrives.
     */
    aes_xif_key(key_words, key_size);
    aes_xif_crypt(input, output, decrypt);

    for (i = 0; i < 4; ++i) aes_xif_store_be32(block + 4 * i, output[i]);
}

#endif
