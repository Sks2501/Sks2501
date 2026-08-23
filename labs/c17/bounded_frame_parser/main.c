#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FRAME_MAGIC_0 0xA5u
#define FRAME_MAGIC_1 0x5Au
#define FRAME_VERSION 0x01u
#define FRAME_HEADER_SIZE 6u
#define FRAME_CRC_SIZE 4u
#define FRAME_MAX_PAYLOAD 256u
#define FRAME_MAX_SIZE (FRAME_HEADER_SIZE + FRAME_MAX_PAYLOAD + FRAME_CRC_SIZE)

typedef enum {
    PARSE_OK = 0,
    PARSE_NULL_ARGUMENT,
    PARSE_TOO_SHORT,
    PARSE_BAD_MAGIC,
    PARSE_BAD_VERSION,
    PARSE_PAYLOAD_TOO_LARGE,
    PARSE_LENGTH_MISMATCH,
    PARSE_BAD_CRC
} parse_status_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t payload_length;
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint32_t crc32;
} frame_t;

static uint16_t read_u16_be(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8u) | (uint16_t)p[1]);
}

static uint32_t read_u32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) |
           (uint32_t)p[3];
}

static void write_u16_be(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)((value >> 8u) & 0xFFu);
    p[1] = (uint8_t)(value & 0xFFu);
}

static void write_u32_be(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)((value >> 24u) & 0xFFu);
    p[1] = (uint8_t)((value >> 16u) & 0xFFu);
    p[2] = (uint8_t)((value >> 8u) & 0xFFu);
    p[3] = (uint8_t)(value & 0xFFu);
}

static uint32_t crc32_ieee(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint32_t)data[i];
        for (uint32_t bit = 0; bit < 8u; ++bit) {
            const uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

static const char *parse_status_name(parse_status_t status) {
    switch (status) {
        case PARSE_OK: return "ok";
        case PARSE_NULL_ARGUMENT: return "argumento_nulo";
        case PARSE_TOO_SHORT: return "frame_curto";
        case PARSE_BAD_MAGIC: return "magic_invalido";
        case PARSE_BAD_VERSION: return "versao_invalida";
        case PARSE_PAYLOAD_TOO_LARGE: return "payload_excede_limite";
        case PARSE_LENGTH_MISMATCH: return "comprimento_inconsistente";
        case PARSE_BAD_CRC: return "crc_invalido";
        default: return "erro_desconhecido";
    }
}

static parse_status_t parse_frame(const uint8_t *bytes, size_t length, frame_t *out) {
    if (bytes == NULL || out == NULL) {
        return PARSE_NULL_ARGUMENT;
    }

    if (length < FRAME_HEADER_SIZE + FRAME_CRC_SIZE) {
        return PARSE_TOO_SHORT;
    }

    if (bytes[0] != FRAME_MAGIC_0 || bytes[1] != FRAME_MAGIC_1) {
        return PARSE_BAD_MAGIC;
    }

    if (bytes[2] != FRAME_VERSION) {
        return PARSE_BAD_VERSION;
    }

    const uint16_t payload_length = read_u16_be(&bytes[4]);
    if (payload_length > FRAME_MAX_PAYLOAD) {
        return PARSE_PAYLOAD_TOO_LARGE;
    }

    const size_t expected_length = FRAME_HEADER_SIZE + (size_t)payload_length + FRAME_CRC_SIZE;
    if (length != expected_length) {
        return PARSE_LENGTH_MISMATCH;
    }

    const size_t crc_offset = FRAME_HEADER_SIZE + (size_t)payload_length;
    const uint32_t expected_crc = read_u32_be(&bytes[crc_offset]);
    const uint32_t calculated_crc = crc32_ieee(bytes, crc_offset);

    if (expected_crc != calculated_crc) {
        return PARSE_BAD_CRC;
    }

    out->version = bytes[2];
    out->type = bytes[3];
    out->payload_length = payload_length;
    out->crc32 = expected_crc;

    if (payload_length > 0u) {
        memcpy(out->payload, &bytes[FRAME_HEADER_SIZE], payload_length);
    }

    return PARSE_OK;
}

static bool build_frame(
    uint8_t type,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *out,
    size_t out_capacity,
    size_t *out_length
) {
    if (out == NULL || out_length == NULL) {
        return false;
    }

    if (payload_length > FRAME_MAX_PAYLOAD) {
        return false;
    }

    if (payload_length > 0u && payload == NULL) {
        return false;
    }

    const size_t total = FRAME_HEADER_SIZE + (size_t)payload_length + FRAME_CRC_SIZE;
    if (out_capacity < total) {
        return false;
    }

    out[0] = FRAME_MAGIC_0;
    out[1] = FRAME_MAGIC_1;
    out[2] = FRAME_VERSION;
    out[3] = type;
    write_u16_be(&out[4], payload_length);

    if (payload_length > 0u) {
        memcpy(&out[FRAME_HEADER_SIZE], payload, payload_length);
    }

    const size_t crc_offset = FRAME_HEADER_SIZE + (size_t)payload_length;
    const uint32_t crc = crc32_ieee(out, crc_offset);
    write_u32_be(&out[crc_offset], crc);

    *out_length = total;
    return true;
}

static void print_hex(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        printf("%02X", data[i]);
        if (i + 1u < length) {
            putchar(' ');
        }
    }
    putchar('\n');
}

static bool run_negative_tests(const uint8_t *valid_frame, size_t valid_length) {
    uint8_t scratch[FRAME_MAX_SIZE];
    frame_t parsed = {0};

    memcpy(scratch, valid_frame, valid_length);
    scratch[0] ^= 0xFFu;
    if (parse_frame(scratch, valid_length, &parsed) != PARSE_BAD_MAGIC) {
        return false;
    }

    memcpy(scratch, valid_frame, valid_length);
    scratch[2] = 0x7Fu;
    if (parse_frame(scratch, valid_length, &parsed) != PARSE_BAD_VERSION) {
        return false;
    }

    if (parse_frame(valid_frame, valid_length - 1u, &parsed) != PARSE_LENGTH_MISMATCH) {
        return false;
    }

    memcpy(scratch, valid_frame, valid_length);
    scratch[FRAME_HEADER_SIZE] ^= 0x01u;
    if (parse_frame(scratch, valid_length, &parsed) != PARSE_BAD_CRC) {
        return false;
    }

    return true;
}

int main(void) {
    static const uint8_t payload[] = {
        0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x70u, 0x80u
    };

    uint8_t encoded[FRAME_MAX_SIZE] = {0};
    size_t encoded_length = 0u;

    if (!build_frame(
            0x11u,
            payload,
            (uint16_t)sizeof(payload),
            encoded,
            sizeof(encoded),
            &encoded_length)) {
        fputs("falha_ao_construir_frame\n", stderr);
        return 1;
    }

    printf("frame_codificado_bytes=%zu\n", encoded_length);
    print_hex(encoded, encoded_length);

    frame_t parsed = {0};
    const parse_status_t status = parse_frame(encoded, encoded_length, &parsed);

    printf("parse_status=%s\n", parse_status_name(status));
    if (status != PARSE_OK) {
        return 2;
    }

    printf("versao=%" PRIu8 "\n", parsed.version);
    printf("tipo=%" PRIu8 "\n", parsed.type);
    printf("payload_length=%" PRIu16 "\n", parsed.payload_length);
    printf("crc32=%08" PRIX32 "\n", parsed.crc32);
    printf("payload=");
    print_hex(parsed.payload, parsed.payload_length);

    if (!run_negative_tests(encoded, encoded_length)) {
        fputs("teste_negativo_falhou\n", stderr);
        return 3;
    }

    puts("testes_negativos=ok");
    return 0;
}
