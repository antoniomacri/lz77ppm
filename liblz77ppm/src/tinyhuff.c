/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <tinyhuff.h>

/** Represents an entry in the encoding table. */
struct enc_entry {
    /** The (right-aligned) encoding of the length of the match. */
    uint8_t code;
    /** The length in bits of the code. */
    uint8_t nbits;
};

/** Represents an entry in the decoding table. */
struct dec_entry {
    /** The actual value of the length of the match. */
    uint8_t value;
    /** The number of bits to consume from the input stream. */
    uint8_t nbits;
};

/*
 * We create a code for lengths up to M (=8) symbols. If the length to be
 * encoded is greater, we just use a prefix (the same used for M) and add a
 * fixed number of bits to represent an unsigned integer containing the
 * difference between the actual length and M.
 */

static struct enc_entry encoding_table[] = {
    { .code = 0, .nbits = 6 },  // 0:             000 000
    { .code = 3, .nbits = 2 },  // min_value:     11
    { .code = 2, .nbits = 2 },  // min_value+1:   10
    { .code = 1, .nbits = 2 },  // min_value+2:   01
    { .code = 1, .nbits = 3 },  // min_value+3:   001
    { .code = 1, .nbits = 4 },  // min_value+4:   000 1
    { .code = 1, .nbits = 5 },  // min_value+5:   000 01
    { .code = 1, .nbits = 6 },  // min_value+6+:  000 001
};

#define ENCODING_TABLE_SIZE (sizeof(encoding_table) / sizeof(encoding_table[0]))

#define MAX_CODE_BITS 6

static struct dec_entry decoding_table[] = {
    { .value = 0, .nbits = 6 },
    { .value = 6, .nbits = 6 },
    { .value = 5, .nbits = 5 },
    { .value = 5, .nbits = 5 },
    { .value = 4, .nbits = 4 },
    { .value = 4, .nbits = 4 },
    { .value = 4, .nbits = 4 },
    { .value = 4, .nbits = 4 },
    { .value = 3, .nbits = 3 },
    { .value = 3, .nbits = 3 },
    { .value = 3, .nbits = 3 },
    { .value = 3, .nbits = 3 },
    { .value = 3, .nbits = 3 },
    { .value = 3, .nbits = 3 },
    { .value = 3, .nbits = 3 },
    { .value = 3, .nbits = 3 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 2, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 1, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
    { .value = 0, .nbits = 2 },
};

static uint8_t number_of_bits(uint16_t value);

void tinyhuff_init(lz77_tinyhuff *encoder, uint16_t min_value, uint16_t max_value)
{
    encoder->min_value = min_value;
    encoder->max_value = max_value;
    encoder->max_encoded_value = min_value + sizeof(encoding_table) / sizeof(encoding_table[0]) - 2;
    int max_diff = encoder->max_value - encoder->max_encoded_value;
    encoder->diff_nbits = (max_diff >= 0) ? number_of_bits(max_diff) : 0;
}

uint8_t tinyhuff_encode(lz77_tinyhuff *enc, uint16_t value, uint16_t *code)
{
    assert(enc != NULL);
    assert(value == 0 || value >= enc->min_value);
    assert(value <= enc->max_value);
    assert(code != NULL);

    unsigned int index = 0;
    if (value != 0) {
        index = 1 + value - enc->min_value;
        if (index > ENCODING_TABLE_SIZE - 1) {
            index = ENCODING_TABLE_SIZE - 1;
        }
    }
    *code = encoding_table[index].code;
    uint8_t nbits = encoding_table[index].nbits;

    if (value >= enc->max_encoded_value) {
        assert(enc->diff_nbits > 0);
        uint16_t diff_code = value - enc->max_encoded_value;
        assert(diff_code == (diff_code & ((1 << enc->diff_nbits) - 1)));
        *code = (*code << enc->diff_nbits) | diff_code;
        nbits += enc->diff_nbits;
    }
    return nbits;
}

uint8_t tinyhuff_can_encode(lz77_tinyhuff *enc, uint16_t value)
{
    assert(enc != NULL);

    return value == 0 || (enc->min_value <= value && value <= enc->max_value);
}

uint8_t tinyhuff_decode(lz77_tinyhuff *enc,
                        const uint16_t *peeked_data,
                        uint16_t peeked_length,
                        uint16_t *value)
{
    assert(enc != NULL);
    assert(peeked_length <= sizeof(*peeked_data) * 8);
    assert(value != NULL);

    if (peeked_length < LZ77_TINYHUFF_MIN_CODE_BITS) {
        return 0;
    }

    uint16_t index = *peeked_data;
    uint16_t tpos = sizeof(index) * 8 - MAX_CODE_BITS;
    uint16_t mask = (1 << MAX_CODE_BITS) - 1;
    index = (index >> tpos) & mask;

    *value = decoding_table[index].value;
    uint8_t to_consume = decoding_table[index].nbits;

    if (peeked_length < to_consume) {
        return 0;
    }

    if (index > 0) {
        *value += enc->min_value;
    }

    if (*value == enc->max_encoded_value) {
        assert(enc->diff_nbits > 0);
        if (peeked_length < to_consume + enc->diff_nbits) {
            return 0;
        }
        tpos -= enc->diff_nbits;
        uint16_t diff = *peeked_data;
        uint16_t mask = (1 << enc->diff_nbits) - 1;
        diff = (diff >> tpos) & mask;
        *value += diff;
        to_consume += enc->diff_nbits;
    }
    assert(*value <= enc->max_value);
    assert(to_consume <= peeked_length);

    return to_consume;
}

static uint8_t number_of_bits(uint16_t value)
{
    uint8_t r = 1;
    while (value >>= 1) {
        r++;
    }
    return r;
}
