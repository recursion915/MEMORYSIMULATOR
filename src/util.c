#include "util.h"


uint32_t getIndex(uint32_t address, uint8_t offsetSize, uint8_t indexSize) {
    uint32_t index = 0;
    index = (address >> offsetSize) & (~(0xffffffff << indexSize));

    return index;
}


uint32_t getTag(uint32_t address, uint32_t offsetSize, uint8_t indexSize , uint8_t tagSize) {
    uint32_t tag = 0;
    tag = (address >> (offsetSize + indexSize)) & (~(0xffffffff << tagSize));

    return tag;
}
