/*

This is an implementation of the AES algorithm, specifically ECB, CTR and CBC mode.
Block size can be chosen in aes.h - available choices are AES128, AES192, AES256.

The implementation is verified against the test vectors in:
  National Institute of Standards and Technology Special Publication 800-38A 2001 ED

ECB-AES128
----------

  plain-text:
    6bc1bee22e409f96e93d7e117393172a
    ae2d8a571e03ac9c9eb76fac45af8e51
    30c81c46a35ce411e5fbc1191a0a52ef
    f69f2445df4f9b17ad2b417be66c3710

  key:
    2b7e151628aed2a6abf7158809cf4f3c

  resulting cipher
    3ad77bb40d7a3660a89ecaf32466ef97
    f5d3d58503b9699de785895a96fdbaaf
    43b1cd7f598ece23881b00e3ed030688
    7b0c785e27e8ad3f8223207104725dd4


NOTE:   String length must be evenly divisible by 16byte (str_len % 16 == 0)
        You should pad the end of the string with zeros if this is not the case.
        For AES192/256 the key size is proportionally larger.

*/


/*****************************************************************************/
/* Includes:                                                                 */
/*****************************************************************************/
// #include <string.h> // CBC mode, for memset
#include "aes.h"
#include "string_utils.h"
/*****************************************************************************/
/* Defines:                                                                  */
/*****************************************************************************/
// The number of columns comprising a state in AES. This is a constant in AES. Value=4
#define Nb 4

#if defined(AES256) && (AES256 == 1)
    #define Nk 8
    #define Nr 14
#elif defined(AES192) && (AES192 == 1)
    #define Nk 6
    #define Nr 12
#else
    #define Nk 4        // The number of 32 bit words in a key.
    #define Nr 10       // The number of rounds in AES Cipher.
#endif


static inline uint32_t aes32esi0(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32esi %0, %1, %2, 0"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32esi1(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32esi %0, %1, %2, 1"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32esi2(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32esi %0, %1, %2, 2"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32esi3(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32esi %0, %1, %2, 3"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32esmi0(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32esmi %0, %1, %2, 0"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32esmi1(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32esmi %0, %1, %2, 1"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32esmi2(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32esmi %0, %1, %2, 2"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32esmi3(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32esmi %0, %1, %2, 3"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32dsi0(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32dsi %0, %1, %2, 0"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32dsi1(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32dsi %0, %1, %2, 1"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32dsi2(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32dsi %0, %1, %2, 2"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32dsi3(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32dsi %0, %1, %2, 3"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32dsmi0(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32dsmi %0, %1, %2, 0"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32dsmi1(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32dsmi %0, %1, %2, 1"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32dsmi2(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32dsmi %0, %1, %2, 2"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

static inline uint32_t aes32dsmi3(uint32_t rs1, uint32_t rs2)
{
    uint32_t rd;
    asm volatile ("aes32dsmi %0, %1, %2, 3"
                  : "=r"(rd)
                  : "r"(rs1), "r"(rs2));
    return rd;
}

typedef uint8_t state_t[4][4];

static inline uint32_t pack_word(
    uint8_t b0,
    uint8_t b1,
    uint8_t b2,
    uint8_t b3)
{
    return ((uint32_t)b0)       |
           ((uint32_t)b1 << 8)  |
           ((uint32_t)b2 << 16) |
           ((uint32_t)b3 << 24);
}

static inline void unpack_word(
    uint32_t w,
    uint8_t *b0,
    uint8_t *b1,
    uint8_t *b2,
    uint8_t *b3)
{
    *b0 = (uint8_t)(w);
    *b1 = (uint8_t)(w >> 8);
    *b2 = (uint8_t)(w >> 16);
    *b3 = (uint8_t)(w >> 24);
}

static inline uint32_t aes_zkne_middle(
    uint32_t state_word,
    uint32_t round_key_word)
{
    uint32_t r;

    r = aes32esmi0(round_key_word, state_word);
    r = aes32esmi1(r,             state_word);
    r = aes32esmi2(r,             state_word);
    r = aes32esmi3(r,             state_word);

    return r;
}

static inline uint32_t aes_zkne_final(
    uint32_t state_word,
    uint32_t round_key_word)
{
    uint32_t r;

    r = aes32esi0(round_key_word, state_word);
    r = aes32esi1(r,              state_word);
    r = aes32esi2(r,              state_word);
    r = aes32esi3(r,              state_word);

    return r;
}

static inline uint32_t aes_zknd_middle(
    uint32_t state_word,
    uint32_t inv_mixed_round_key_word)
{
    uint32_t r;

    r = aes32dsmi0(inv_mixed_round_key_word, state_word);
    r = aes32dsmi1(r,                          state_word);
    r = aes32dsmi2(r,                          state_word);
    r = aes32dsmi3(r,                          state_word);

    return r;
}

static inline uint32_t aes_zknd_final(
    uint32_t state_word,
    uint32_t round_key_word)
{
    uint32_t r;

    r = aes32dsi0(round_key_word, state_word);
    r = aes32dsi1(r,              state_word);
    r = aes32dsi2(r,              state_word);
    r = aes32dsi3(r,              state_word);

    return r;
}

static uint32_t InvMixColumnWord(uint32_t word);
static void PrepareInvMixRoundKeys(struct AES_ctx* ctx);

/*****************************************************************************/
/* Private variables:                                                        */
/*****************************************************************************/
// state - array holding the intermediate results during decryption.
typedef uint8_t state_t[4][4];



// The lookup-tables are marked const so they can be placed in read-only storage instead of RAM
// The numbers below can be computed dynamically trading ROM for RAM -
// This can be useful in (embedded) bootloader applications, where ROM is often limited.
static const uint8_t sbox[256] = {
  //0     1    2      3     4    5     6     7      8    9     A      B    C     D     E     F
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };

#if (defined(CBC) && CBC == 1) || (defined(ECB) && ECB == 1)
static const uint8_t rsbox[256] = {
  0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d };
#endif

// The round constant word array, Rcon[i], contains the values given by
// x to the power (i-1) being powers of x (x is denoted as {02}) in the field GF(2^8)
static const uint8_t Rcon[11] = {
  0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

/*
 * Jordan Goulder points out in PR #12 (https://github.com/kokke/tiny-AES-C/pull/12),
 * that you can remove most of the elements in the Rcon array, because they are unused.
 *
 * From Wikipedia's article on the Rijndael key schedule @ https://en.wikipedia.org/wiki/Rijndael_key_schedule#Rcon
 *
 * "Only the first some of these constants are actually used – up to rcon[10] for AES-128 (as 11 round keys are needed),
 *  up to rcon[8] for AES-192, up to rcon[7] for AES-256. rcon[0] is not used in AES algorithm."
 */


/*****************************************************************************/
/* Private functions:                                                        */
/*****************************************************************************/
/*
static uint8_t getSBoxValue(uint8_t num)
{
  return sbox[num];
}
*/
#define getSBoxValue(num) (sbox[(num)])

// This function produces Nb(Nr+1) round keys. The round keys are used in each round to decrypt the states.
static void KeyExpansion(uint8_t* RoundKey, const uint8_t* Key)
{
  unsigned i, j, k;
  uint8_t tempa[4]; // Used for the column/row operations

  // The first round key is the key itself.
  for (i = 0; i < Nk; ++i)
  {
    RoundKey[(i * 4) + 0] = Key[(i * 4) + 0];
    RoundKey[(i * 4) + 1] = Key[(i * 4) + 1];
    RoundKey[(i * 4) + 2] = Key[(i * 4) + 2];
    RoundKey[(i * 4) + 3] = Key[(i * 4) + 3];
  }

  // All other round keys are found from the previous round keys.
  for (i = Nk; i < Nb * (Nr + 1); ++i)
  {
    {
      k = (i - 1) * 4;
      tempa[0]=RoundKey[k + 0];
      tempa[1]=RoundKey[k + 1];
      tempa[2]=RoundKey[k + 2];
      tempa[3]=RoundKey[k + 3];

    }

    if (i % Nk == 0)
    {
      // This function shifts the 4 bytes in a word to the left once.
      // [a0,a1,a2,a3] becomes [a1,a2,a3,a0]

      // Function RotWord()
      {
        const uint8_t u8tmp = tempa[0];
        tempa[0] = tempa[1];
        tempa[1] = tempa[2];
        tempa[2] = tempa[3];
        tempa[3] = u8tmp;
      }

      // SubWord() is a function that takes a four-byte input word and
      // applies the S-box to each of the four bytes to produce an output word.

      // Function Subword()
      {
        tempa[0] = getSBoxValue(tempa[0]);
        tempa[1] = getSBoxValue(tempa[1]);
        tempa[2] = getSBoxValue(tempa[2]);
        tempa[3] = getSBoxValue(tempa[3]);
      }

      tempa[0] = tempa[0] ^ Rcon[i/Nk];
    }
#if defined(AES256) && (AES256 == 1)
    if (i % Nk == 4)
    {
      // Function Subword()
      {
        tempa[0] = getSBoxValue(tempa[0]);
        tempa[1] = getSBoxValue(tempa[1]);
        tempa[2] = getSBoxValue(tempa[2]);
        tempa[3] = getSBoxValue(tempa[3]);
      }
    }
#endif
    j = i * 4; k=(i - Nk) * 4;
    RoundKey[j + 0] = RoundKey[k + 0] ^ tempa[0];
    RoundKey[j + 1] = RoundKey[k + 1] ^ tempa[1];
    RoundKey[j + 2] = RoundKey[k + 2] ^ tempa[2];
    RoundKey[j + 3] = RoundKey[k + 3] ^ tempa[3];
  }
}

void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key)
{
  KeyExpansion(ctx->RoundKey, key);
}

void AES_ctx_prepare_decrypt(struct AES_ctx* ctx)
{
  PrepareInvMixRoundKeys(ctx);
}
#if (defined(CBC) && (CBC == 1)) || (defined(CTR) && (CTR == 1))
void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv)
{
  AES_init_ctx(ctx, key);
  memcpy (ctx->Iv, iv, AES_BLOCKLEN);
}
void AES_ctx_set_iv(struct AES_ctx* ctx, const uint8_t* iv)
{
  memcpy (ctx->Iv, iv, AES_BLOCKLEN);
}
#endif

// This function adds the round key to state.
// The round key is added to the state by an XOR function.
static void AddRoundKey(uint8_t round, state_t* state, const uint8_t* RoundKey)
{
  uint8_t i,j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[i][j] ^= RoundKey[(round * Nb * 4) + (i * Nb) + j];
    }
  }
}

// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
static void SubBytes(state_t* state)
{
  uint8_t i, j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[j][i] = getSBoxValue((*state)[j][i]);
    }
  }
}

// The ShiftRows() function shifts the rows in the state to the left.
// Each row is shifted with different offset.
// Offset = Row number. So the first row is not shifted.
static void ShiftRows(state_t* state)
{
  uint8_t temp;

  // Rotate first row 1 columns to left
  temp           = (*state)[0][1];
  (*state)[0][1] = (*state)[1][1];
  (*state)[1][1] = (*state)[2][1];
  (*state)[2][1] = (*state)[3][1];
  (*state)[3][1] = temp;

  // Rotate second row 2 columns to left
  temp           = (*state)[0][2];
  (*state)[0][2] = (*state)[2][2];
  (*state)[2][2] = temp;

  temp           = (*state)[1][2];
  (*state)[1][2] = (*state)[3][2];
  (*state)[3][2] = temp;

  // Rotate third row 3 columns to left
  temp           = (*state)[0][3];
  (*state)[0][3] = (*state)[3][3];
  (*state)[3][3] = (*state)[2][3];
  (*state)[2][3] = (*state)[1][3];
  (*state)[1][3] = temp;
}
static inline uint8_t xtime8(uint8_t x)
{
    return (uint8_t)((x << 1) ^ ((uint8_t)(-(int8_t)(x >> 7)) & 0x1b));
}
// Apply the fixed AES inverse MixColumns transform to one little-endian
// 32-bit state/key column.  Only the four fixed coefficients 09, 0b, 0d,
// and 0e are required, so no general GF(2^8) multiplier is needed.
static inline uint32_t InvMixColumnWord(uint32_t word)
{
  uint8_t a0 = (uint8_t) word;
  uint8_t a1 = (uint8_t)(word >> 8);
  uint8_t a2 = (uint8_t)(word >> 16);
  uint8_t a3 = (uint8_t)(word >> 24);

  uint8_t a0_2 = xtime8(a0);
  uint8_t a1_2 = xtime8(a1);
  uint8_t a2_2 = xtime8(a2);
  uint8_t a3_2 = xtime8(a3);

  uint8_t a0_4 = xtime8(a0_2);
  uint8_t a1_4 = xtime8(a1_2);
  uint8_t a2_4 = xtime8(a2_2);
  uint8_t a3_4 = xtime8(a3_2);

  uint8_t a0_8 = xtime8(a0_4);
  uint8_t a1_8 = xtime8(a1_4);
  uint8_t a2_8 = xtime8(a2_4);
  uint8_t a3_8 = xtime8(a3_4);

  // 09 = 08 ^ 01
  // 0b = 08 ^ 02 ^ 01
  // 0d = 08 ^ 04 ^ 01
  // 0e = 08 ^ 04 ^ 02
  const uint8_t r0 = (a0_8 ^ a0_4 ^ a0_2) ^
                     (a1_8 ^ a1_2 ^ a1) ^
                     (a2_8 ^ a2_4 ^ a2) ^
                     (a3_8 ^ a3);

  const uint8_t r1 = (a0_8 ^ a0) ^
                     (a1_8 ^ a1_4 ^ a1_2) ^
                     (a2_8 ^ a2_2 ^ a2) ^
                     (a3_8 ^ a3_4 ^ a3);

  const uint8_t r2 = (a0_8 ^ a0_4 ^ a0) ^
                     (a1_8 ^ a1) ^
                     (a2_8 ^ a2_4 ^ a2_2) ^
                     (a3_8 ^ a3_2 ^ a3);

  const uint8_t r3 = (a0_8 ^ a0_2 ^ a0) ^
                     (a1_8 ^ a1_4 ^ a1) ^
                     (a2_8 ^ a2) ^
                     (a3_8 ^ a3_4 ^ a3_2);

  return pack_word(r0, r1, r2, r3);
}

// aes32dsmi combines InvSubBytes and InvMixColumns.  By linearity, the
// middle-round AddRoundKey value must therefore also be inverse-mixed.
// Do this once when the key is installed, rather than once for every block.
static void PrepareInvMixRoundKeys(struct AES_ctx* ctx)
{
  for (uint8_t round = 1; round < Nr; ++round) {
    for (uint8_t column = 0; column < Nb; ++column) {
      const uint8_t offset = (uint8_t)(round * AES_BLOCKLEN + column * 4);
      const uint32_t key_word =
          pack_word(ctx->RoundKey[offset],
                    ctx->RoundKey[offset + 1],
                    ctx->RoundKey[offset + 2],
                    ctx->RoundKey[offset + 3]);

      ctx->InvMixRoundKey[(round - 1) * Nb + column] =
          InvMixColumnWord(key_word);
    }
  }
}


#if (defined(CBC) && CBC == 1) || (defined(ECB) && ECB == 1)
/*
static uint8_t getSBoxInvert(uint8_t num)
{
  return rsbox[num];
}
*/
#define getSBoxInvert(num) (rsbox[(num)])

// MixColumns function mixes the columns of the state matrix.
// The method used to multiply may be difficult to understand for the inexperienced.
// Please use the references to gain more information.
#endif // #if (defined(CBC) && CBC == 1) || (defined(ECB) && ECB == 1)

// Cipher is the main function that encrypts the PlainText.
// static void Cipher(state_t* state, const uint8_t* RoundKey)
// {
//   uint8_t round = 0;

//   // Add the First round key to the state before starting the rounds.
//   AddRoundKey(0, state, RoundKey);

//   // There will be Nr rounds.
//   // The first Nr-1 rounds are identical.
//   // These Nr rounds are executed in the loop below.
//   // Last one without MixColumns()
//   for (round = 1; ; ++round)
//   {
//     SubBytes(state);
//     ShiftRows(state);
//     if (round == Nr) {
//       break;
//     }
//     MixColumns(state);
//     AddRoundKey(round, state, RoundKey);
//   }
//   // Add round key to last round
//   AddRoundKey(Nr, state, RoundKey);
// }

static void Cipher(state_t* state, const uint8_t* RoundKey)
{
    uint32_t s0, s1, s2, s3;
    uint32_t t0, t1, t2, t3;
    uint32_t rk0, rk1, rk2, rk3;

    /*
     * Initial AddRoundKey
     *
     * RoundKey layout is already:
     *   [column0 bytes][column1 bytes][column2 bytes][column3 bytes]
     */
    s0 = pack_word(
        (*state)[0][0],
        (*state)[0][1],
        (*state)[0][2],
        (*state)[0][3]);

    s1 = pack_word(
        (*state)[1][0],
        (*state)[1][1],
        (*state)[1][2],
        (*state)[1][3]);

    s2 = pack_word(
        (*state)[2][0],
        (*state)[2][1],
        (*state)[2][2],
        (*state)[2][3]);

    s3 = pack_word(
        (*state)[3][0],
        (*state)[3][1],
        (*state)[3][2],
        (*state)[3][3]);

    rk0 = pack_word(
        RoundKey[0], RoundKey[1],
        RoundKey[2], RoundKey[3]);

    rk1 = pack_word(
        RoundKey[4], RoundKey[5],
        RoundKey[6], RoundKey[7]);

    rk2 = pack_word(
        RoundKey[8], RoundKey[9],
        RoundKey[10], RoundKey[11]);

    rk3 = pack_word(
        RoundKey[12], RoundKey[13],
        RoundKey[14], RoundKey[15]);

    s0 ^= rk0;
    s1 ^= rk1;
    s2 ^= rk2;
    s3 ^= rk3;

    /*
     * Middle rounds
     *
     * Zkne aes32esmi performs:
     *
     *   SBox + partial MixColumns + rotate + XOR
     *
     * The ShiftRows operation is incorporated by selecting
     * bytes from four different state columns.
     */
    for (unsigned round = 1; round < Nr; ++round)
    {
        /*
         * After ShiftRows:
         *
         * output column 0:
         *   s0.byte0
         *   s1.byte1
         *   s2.byte2
         *   s3.byte3
         *
         * output column 1:
         *   s1.byte0
         *   s2.byte1
         *   s3.byte2
         *   s0.byte3
         *
         * output column 2:
         *   s2.byte0
         *   s3.byte1
         *   s0.byte2
         *   s1.byte3
         *
         * output column 3:
         *   s3.byte0
         *   s0.byte1
         *   s1.byte2
         *   s2.byte3
         */

        t0 =
            ((s0       & 0x000000FF)) |
            ((s1       & 0x0000FF00)) |
            ((s2       & 0x00FF0000)) |
            ((s3       & 0xFF000000));

        t1 =
            ((s1       & 0x000000FF)) |
            ((s2       & 0x0000FF00)) |
            ((s3       & 0x00FF0000)) |
            ((s0       & 0xFF000000));

        t2 =
            ((s2       & 0x000000FF)) |
            ((s3       & 0x0000FF00)) |
            ((s0       & 0x00FF0000)) |
            ((s1       & 0xFF000000));

        t3 =
            ((s3       & 0x000000FF)) |
            ((s0       & 0x0000FF00)) |
            ((s1       & 0x00FF0000)) |
            ((s2       & 0xFF000000));

        rk0 = pack_word(
            RoundKey[round * 16 + 0],
            RoundKey[round * 16 + 1],
            RoundKey[round * 16 + 2],
            RoundKey[round * 16 + 3]);

        rk1 = pack_word(
            RoundKey[round * 16 + 4],
            RoundKey[round * 16 + 5],
            RoundKey[round * 16 + 6],
            RoundKey[round * 16 + 7]);

        rk2 = pack_word(
            RoundKey[round * 16 + 8],
            RoundKey[round * 16 + 9],
            RoundKey[round * 16 + 10],
            RoundKey[round * 16 + 11]);

        rk3 = pack_word(
            RoundKey[round * 16 + 12],
            RoundKey[round * 16 + 13],
            RoundKey[round * 16 + 14],
            RoundKey[round * 16 + 15]);

        s0 = aes_zkne_middle(t0, rk0);
        s1 = aes_zkne_middle(t1, rk1);
        s2 = aes_zkne_middle(t2, rk2);
        s3 = aes_zkne_middle(t3, rk3);
    }

    /*
     * Final round
     *
     * Same ShiftRows byte selection, but aes32esi instead
     * of aes32esmi because the final AES round has no MixColumns.
     */
    t0 =
        ((s0 & 0x000000FF)) |
        ((s1 & 0x0000FF00)) |
        ((s2 & 0x00FF0000)) |
        ((s3 & 0xFF000000));

    t1 =
        ((s1 & 0x000000FF)) |
        ((s2 & 0x0000FF00)) |
        ((s3 & 0x00FF0000)) |
        ((s0 & 0xFF000000));

    t2 =
        ((s2 & 0x000000FF)) |
        ((s3 & 0x0000FF00)) |
        ((s0 & 0x00FF0000)) |
        ((s1 & 0xFF000000));

    t3 =
        ((s3 & 0x000000FF)) |
        ((s0 & 0x0000FF00)) |
        ((s1 & 0x00FF0000)) |
        ((s2 & 0xFF000000));

    rk0 = pack_word(
        RoundKey[Nr * 16 + 0],
        RoundKey[Nr * 16 + 1],
        RoundKey[Nr * 16 + 2],
        RoundKey[Nr * 16 + 3]);

    rk1 = pack_word(
        RoundKey[Nr * 16 + 4],
        RoundKey[Nr * 16 + 5],
        RoundKey[Nr * 16 + 6],
        RoundKey[Nr * 16 + 7]);

    rk2 = pack_word(
        RoundKey[Nr * 16 + 8],
        RoundKey[Nr * 16 + 9],
        RoundKey[Nr * 16 + 10],
        RoundKey[Nr * 16 + 11]);

    rk3 = pack_word(
        RoundKey[Nr * 16 + 12],
        RoundKey[Nr * 16 + 13],
        RoundKey[Nr * 16 + 14],
        RoundKey[Nr * 16 + 15]);

    s0 = aes_zkne_final(t0, rk0);
    s1 = aes_zkne_final(t1, rk1);
    s2 = aes_zkne_final(t2, rk2);
    s3 = aes_zkne_final(t3, rk3);

    /*
     * Write the four resulting columns back into the
     * original tiny-AES state representation.
     */
    unpack_word(s0,
                &(*state)[0][0],
                &(*state)[0][1],
                &(*state)[0][2],
                &(*state)[0][3]);

    unpack_word(s1,
                &(*state)[1][0],
                &(*state)[1][1],
                &(*state)[1][2],
                &(*state)[1][3]);

    unpack_word(s2,
                &(*state)[2][0],
                &(*state)[2][1],
                &(*state)[2][2],
                &(*state)[2][3]);

    unpack_word(s3,
                &(*state)[3][0],
                &(*state)[3][1],
                &(*state)[3][2],
                &(*state)[3][3]);
}

#if (defined(CBC) && CBC == 1) || (defined(ECB) && ECB == 1)
static void InvCipher(state_t* state, const uint8_t* RoundKey,
                      const uint32_t* InvMixRoundKey)
{
  uint32_t s0, s1, s2, s3;
  uint32_t t0, t1, t2, t3;
  uint32_t rk0, rk1, rk2, rk3;

  s0 = pack_word((*state)[0][0], (*state)[0][1], (*state)[0][2], (*state)[0][3]);
  s1 = pack_word((*state)[1][0], (*state)[1][1], (*state)[1][2], (*state)[1][3]);
  s2 = pack_word((*state)[2][0], (*state)[2][1], (*state)[2][2], (*state)[2][3]);
  s3 = pack_word((*state)[3][0], (*state)[3][1], (*state)[3][2], (*state)[3][3]);

  rk0 = pack_word(RoundKey[Nr * 16 + 0], RoundKey[Nr * 16 + 1],
                  RoundKey[Nr * 16 + 2], RoundKey[Nr * 16 + 3]);
  rk1 = pack_word(RoundKey[Nr * 16 + 4], RoundKey[Nr * 16 + 5],
                  RoundKey[Nr * 16 + 6], RoundKey[Nr * 16 + 7]);
  rk2 = pack_word(RoundKey[Nr * 16 + 8], RoundKey[Nr * 16 + 9],
                  RoundKey[Nr * 16 + 10], RoundKey[Nr * 16 + 11]);
  rk3 = pack_word(RoundKey[Nr * 16 + 12], RoundKey[Nr * 16 + 13],
                  RoundKey[Nr * 16 + 14], RoundKey[Nr * 16 + 15]);

  // Initial AddRoundKey with the encryption final-round key.
  s0 ^= rk0;
  s1 ^= rk1;
  s2 ^= rk2;
  s3 ^= rk3;

  // AES32DSMI includes InvMixColumns. Its accumulator must therefore start
  // with InvMixColumns(round_key), rather than the unmodified round key.
  for (unsigned round = Nr - 1; round > 0; --round) {
    // InvShiftRows: select the source byte for each output state column.
    t0 = (s0 & 0x000000FF) | (s3 & 0x0000FF00) |
         (s2 & 0x00FF0000) | (s1 & 0xFF000000);
    t1 = (s1 & 0x000000FF) | (s0 & 0x0000FF00) |
         (s3 & 0x00FF0000) | (s2 & 0xFF000000);
    t2 = (s2 & 0x000000FF) | (s1 & 0x0000FF00) |
         (s0 & 0x00FF0000) | (s3 & 0xFF000000);
    t3 = (s3 & 0x000000FF) | (s2 & 0x0000FF00) |
         (s1 & 0x00FF0000) | (s0 & 0xFF000000);

    rk0 = InvMixRoundKey[(round - 1) * Nb + 0];
    rk1 = InvMixRoundKey[(round - 1) * Nb + 1];
    rk2 = InvMixRoundKey[(round - 1) * Nb + 2];
    rk3 = InvMixRoundKey[(round - 1) * Nb + 3];

    s0 = aes_zknd_middle(t0, rk0);
    s1 = aes_zknd_middle(t1, rk1);
    s2 = aes_zknd_middle(t2, rk2);
    s3 = aes_zknd_middle(t3, rk3);
  }

  // The final decryption round omits InvMixColumns and uses round key zero.
  t0 = (s0 & 0x000000FF) | (s3 & 0x0000FF00) |
       (s2 & 0x00FF0000) | (s1 & 0xFF000000);
  t1 = (s1 & 0x000000FF) | (s0 & 0x0000FF00) |
       (s3 & 0x00FF0000) | (s2 & 0xFF000000);
  t2 = (s2 & 0x000000FF) | (s1 & 0x0000FF00) |
       (s0 & 0x00FF0000) | (s3 & 0xFF000000);
  t3 = (s3 & 0x000000FF) | (s2 & 0x0000FF00) |
       (s1 & 0x00FF0000) | (s0 & 0xFF000000);

  rk0 = pack_word(RoundKey[0], RoundKey[1], RoundKey[2], RoundKey[3]);
  rk1 = pack_word(RoundKey[4], RoundKey[5], RoundKey[6], RoundKey[7]);
  rk2 = pack_word(RoundKey[8], RoundKey[9], RoundKey[10], RoundKey[11]);
  rk3 = pack_word(RoundKey[12], RoundKey[13], RoundKey[14], RoundKey[15]);

  s0 = aes_zknd_final(t0, rk0);
  s1 = aes_zknd_final(t1, rk1);
  s2 = aes_zknd_final(t2, rk2);
  s3 = aes_zknd_final(t3, rk3);

  unpack_word(s0, &(*state)[0][0], &(*state)[0][1], &(*state)[0][2], &(*state)[0][3]);
  unpack_word(s1, &(*state)[1][0], &(*state)[1][1], &(*state)[1][2], &(*state)[1][3]);
  unpack_word(s2, &(*state)[2][0], &(*state)[2][1], &(*state)[2][2], &(*state)[2][3]);
  unpack_word(s3, &(*state)[3][0], &(*state)[3][1], &(*state)[3][2], &(*state)[3][3]);

}
#endif // #if (defined(CBC) && CBC == 1) || (defined(ECB) && ECB == 1)

/*****************************************************************************/
/* Public functions:                                                         */
/*****************************************************************************/
#if defined(ECB) && (ECB == 1)


void AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf)
{
  // The next function call encrypts the PlainText with the Key using AES algorithm.
  Cipher((state_t*)buf, ctx->RoundKey);
}

void AES_ECB_decrypt(const struct AES_ctx* ctx, uint8_t* buf)
{
  // The next function call decrypts the PlainText with the Key using AES algorithm.
  InvCipher((state_t*)buf, ctx->RoundKey, ctx->InvMixRoundKey);
}


#endif // #if defined(ECB) && (ECB == 1)





#if defined(CBC) && (CBC == 1)


static void XorWithIv(uint8_t* buf, const uint8_t* Iv)
{
  uint8_t i;
  for (i = 0; i < AES_BLOCKLEN; ++i) // The block in AES is always 128bit no matter the key size
  {
    buf[i] ^= Iv[i];
  }
}

void AES_CBC_encrypt_buffer(struct AES_ctx *ctx, uint8_t* buf, size_t length)
{
  size_t i;
  uint8_t *Iv = ctx->Iv;
  for (i = 0; i < length; i += AES_BLOCKLEN)
  {
    XorWithIv(buf, Iv);
    Cipher((state_t*)buf, ctx->RoundKey);
    Iv = buf;
    buf += AES_BLOCKLEN;
  }
  /* store Iv in ctx for next call */
  memcpy(ctx->Iv, Iv, AES_BLOCKLEN);
}

void AES_CBC_decrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length)
{
  size_t i;
  uint8_t storeNextIv[AES_BLOCKLEN];
  for (i = 0; i < length; i += AES_BLOCKLEN)
  {
    memcpy(storeNextIv, buf, AES_BLOCKLEN);
    InvCipher((state_t*)buf, ctx->RoundKey, ctx->InvMixRoundKey);
    XorWithIv(buf, ctx->Iv);
    memcpy(ctx->Iv, storeNextIv, AES_BLOCKLEN);
    buf += AES_BLOCKLEN;
  }

}

#endif // #if defined(CBC) && (CBC == 1)



#if defined(CTR) && (CTR == 1)

/* Symmetrical operation: same function for encrypting as for decrypting. Note any IV/nonce should never be reused with the same key */
void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t buffer[AES_BLOCKLEN];

  size_t i;
  int bi;
  for (i = 0, bi = AES_BLOCKLEN; i < length; ++i, ++bi)
  {
    if (bi == AES_BLOCKLEN) /* we need to regen xor compliment in buffer */
    {

      memcpy(buffer, ctx->Iv, AES_BLOCKLEN);
      Cipher((state_t*)buffer,ctx->RoundKey);

      /* Increment Iv and handle overflow */
      for (bi = (AES_BLOCKLEN - 1); bi >= 0; --bi)
      {
	/* inc will overflow */
        if (ctx->Iv[bi] == 255)
	{
          ctx->Iv[bi] = 0;
          continue;
        }
        ctx->Iv[bi] += 1;
        break;
      }
      bi = 0;
    }

    buf[i] = (buf[i] ^ buffer[bi]);
  }
}

#endif // #if defined(CTR) && (CTR == 1)
