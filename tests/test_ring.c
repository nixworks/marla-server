#include "prepare.h"
#include "rainback.h"
#include <string.h>

AP_DECLARE(void) ap_log_perror_(const char *file, int line, int module_index,
                                int level, apr_status_t status, apr_pool_t *p,
                                const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char exp[512];
    memset(exp, 0, sizeof(exp));
    vsprintf(exp, fmt, args);
    dprintf(3, exp);
    va_end(args);
}

int test_ring_read()
{
    parsegraph_Ring* ring = parsegraph_Ring_new(parsegraph_BUFSIZE);

    const char* line = "Hello, world!";
    int linelen = strlen(line) + 1;
    parsegraph_Ring_write(ring, line, linelen);

    char out[parsegraph_BUFSIZE];
    int nread = parsegraph_Ring_read(ring, out, parsegraph_BUFSIZE);
    if(nread != linelen) {
        fprintf(stderr, "nread(%d) must be equal to the linelen(%d)\n", nread, linelen);
        return 1;
    }
    if(strcmp(line, out)) {
        fprintf(stderr, "Strings are not equal");
        return 1;
    }

    parsegraph_Ring_free(ring);
    return 0;
}

int test_ring_write()
{
    parsegraph_Ring* ring = parsegraph_Ring_new(parsegraph_BUFSIZE);

    void* slotData;
    size_t slotLen;
    parsegraph_Ring_writeSlot(ring, &slotData, &slotLen);

    if(slotLen != parsegraph_BUFSIZE) {
        fprintf(stderr, "slotLen must be equal to the BUFSIZE for an empty ring.\n");
        return 1;
    }
    parsegraph_Ring_writeSlot(ring, &slotData, &slotLen);
    if(slotLen != 0) {
        fprintf(stderr, "writeSlot must not treat full rings as empty.\n");
        return 1;
    }

    parsegraph_Ring_free(ring);
    return 0;
}

int test_ring_readSlot()
{
    parsegraph_Ring* ring = parsegraph_Ring_new(parsegraph_BUFSIZE);
    for(int i = 0; i < parsegraph_BUFSIZE; ++i) {
        parsegraph_Ring_write(ring, "A", 1);
    }

    void* slotData;
    size_t slotLen;
    parsegraph_Ring_readSlot(ring, &slotData, &slotLen);
    if(slotLen != parsegraph_BUFSIZE) {
        fprintf(stderr, "slotLen must be equal to the BUFSIZE for a full ring.\n");
        return 1;
    }
    parsegraph_Ring_readSlot(ring, &slotData, &slotLen);
    if(slotLen != 0) {
        fprintf(stderr, "readSlot must not treat empty rings as full.\n");
        return 1;
    }

    parsegraph_Ring_free(ring);
    return 0;
}

int test_ring_nearFullReads()
{
    const int CAP = 16;
    parsegraph_Ring* ring = parsegraph_Ring_new(CAP);
    for(int i = 0; i < CAP-1; ++i) {
        parsegraph_Ring_writec(ring, 1+i);
    }
    if(parsegraph_Ring_size(ring) != CAP-1) {
        fprintf(stderr, "Ring's size must match the number of bytes written.\n");
        return 1;
    }
    void* bytes;
    size_t slotLen;
    parsegraph_Ring_readSlot(ring, &bytes, &slotLen);
    if(slotLen != CAP-1) {
        fprintf(stderr, "Ring must return the same number of bytes as written.\n");
        return 1;
    }
    for(int i = 0; i < CAP-1; ++i) {
        char c = ((char*)bytes)[i];
        if(c != 1+i) {
            fprintf(stderr, "Ring must return bytes in the order written. (%d != %d\n", c, 1+i);
            return 1;
        }
    }
    parsegraph_Ring_readSlot(ring, &bytes, &slotLen);
    if(slotLen != 0) {
        fprintf(stderr, "Ring must, once all bytes are written, return empty read slots.\n");
        return 1;
    }
    return 0;
}

int main()
{
    return test_ring_read() ||
        test_ring_write() ||
        test_ring_readSlot() ||
        test_ring_nearFullReads();
}
