#include "user_utils.h"

uint32_t StrToHex(uint8_t *pbDest, const char *pbSrc)
{
    char h1, h2;
    unsigned char s1, s2;
    int i;
    for (i = 0;; i++) {
        h1 = pbSrc[2 * i];
        h2 = pbSrc[2 * i + 1];
        if (h1 == 0 || h2 == 0) {
            break;
        }
        s1 = toupper(h1) - 0x30;
        if (s1 > 9) {
            s1 -= 7;
        }
        s2 = toupper(h2) - 0x30;
        if (s2 > 9) {
            s2 -= 7;
        }
        pbDest[i] = s1 * 16 + s2;
    }
    return i;
}


/// @brief Simply Check the entropy of an array.
/// @param[in] array Array to be checked.
/// @param[in] len Array length.
/// @return True if the array entropy is fine.
bool CheckEntropy(const uint8_t *array, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        if (array[i] != 0 && array[i] != 0xFF) {
            return true;
        }
    }
    return false;
}


/// @brief Check the array if all data are 0xFF.
/// @param[in] array Array to be checked.
/// @param[in] len Array length.
/// @return True if the array data are all 0xFF.
bool CheckAllFF(const uint8_t *array, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        if (array[i] != 0xFF) {
            return false;
        }
    }
    return true;
}


/// @brief Check the array if all data are zero.
/// @param[in] array Array to be checked.
/// @param[in] len Array length.
/// @return True if the array data are all zero.
bool CheckAllZero(const uint8_t *array, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        if (array[i] != 0) {
            return false;
        }
    }
    return true;
}

