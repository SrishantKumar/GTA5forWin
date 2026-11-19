#pragma once
#include <stdint.h>

struct nlist_64 {
    union {
        uint32_t n_strx;
    } n_un;
    uint8_t  n_type;
    uint8_t  n_sect;
    uint16_t n_desc;
    uint64_t n_value;
};
