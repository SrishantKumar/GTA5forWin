#pragma once
#include <stdint.h>
#include <stddef.h>

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
#define Z_BLOCK         5
#define Z_TREES         6

typedef void *(*alloc_func)(void *opaque, unsigned items, unsigned size);
typedef void (*free_func)(void *opaque, void *address);

typedef struct z_stream_s {
    const uint8_t *next_in;
    unsigned int avail_in;
    unsigned long total_in;

    uint8_t *next_out;
    unsigned int avail_out;
    unsigned long total_out;

    const char *msg;
    struct internal_state *state;

    alloc_func zalloc;
    free_func zfree;
    void *opaque;

    int data_type;
    unsigned long adler;
    unsigned long reserved;
} z_stream;

typedef z_stream *z_streamp;

#define inflateInit(strm) inflateInit_((strm), "1.2.11", (int)sizeof(z_stream))
#define inflateInit2(strm, windowBits) inflateInit2_((strm), (windowBits), "1.2.11", (int)sizeof(z_stream))

static inline int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size) {
    (void)strm; (void)windowBits; (void)version; (void)stream_size;
    return Z_STREAM_ERROR; // Native zlib inactive on Windows; game falls back to compiled wasm zlib
}
static inline int inflateReset(z_streamp strm) { (void)strm; return Z_STREAM_ERROR; }
static inline int inflateEnd(z_streamp strm) { (void)strm; return Z_OK; }
static inline int inflate(z_streamp strm, int flush) { (void)strm; (void)flush; return Z_STREAM_ERROR; }
