#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) {
    (void)data;
    (void)size;
    return 0;
}
