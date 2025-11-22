#include "hid_utils.h"

// check if different than 2
bool diff_than_n(uint16_t x, uint16_t y, uint8_t n)
{
  return (x - y > n) || (y - x > n);
}

//
void ensureNonZero(uint8_t* value) {
    if (!*value) {
        *value = 1;
    }
}

void ensureAllNonZero(uint8_t* axis_1x, uint8_t* axis_1y, uint8_t* axis_2x, uint8_t* axis_2y) {
    ensureNonZero(axis_1x);
    ensureNonZero(axis_1y);
    ensureNonZero(axis_2x);
    ensureNonZero(axis_2y);
}