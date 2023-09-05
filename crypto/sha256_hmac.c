/*******************************************************************************
* Copyright (C) 2013 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/
//  SHA256_HMAC - HMAC using SHA_256

#include <stdio.h>
#include "string.h"

#include "sha256.h"

#define SHA256_HMAC
#include "sha256_hmac.h"

int sha256_hmac(uchar *key, int key_len, uchar *message, int msg_len, uchar *mac);

//extern int dprintf(char *format, ...);
//extern int sha_debug;

//---------------------------------------------------------------------------
/// Compute HMAC using SHA-256.
///
/// @param[in] key
/// buffer for key
/// @param[in] ken_len
/// length of key
/// @param[in] message
/// buffer for message
/// @param[in] msg_len
/// length of message
/// @param[out] mac
/// 32 byte output mac
///
/// Restrictions:
///  Key length limited to 32 bytes.
///  Message Length is limited to 512 bytes.
///
/// @return
/// TRUE - command successful @n
/// FALSE - command failed
///
int sha256_hmac(uchar *key, int key_len, uchar *message, int msg_len, uchar *mac)
{
    int i;
    uchar thash[64];
    uchar cat_input_thash[1024];
    uchar cat_input_final[1024];

    //   opad = [0x5c * blocksize] // Where blocksize is that of the underlying hash function
    int blocksize = 64;
    uchar opad[64];
    //   ipad = [0x36 * blocksize]
    uchar ipad[64];

    memset(opad, 0x5C, blocksize);
    memset(ipad, 0x36, blocksize);

    // Compute an HMAC

    //   if (length(key) > blocksize) then
    //     key = hash(key) // Where 'hash' is the underlying hash function
    //   end if
    if (key_len > blocksize)
        return 0;  // this will never happen with our device

    // check for blocks too big
    if (msg_len > 512)
        return 0;

    //   for i from 0 to length(key) - 1 step 1
    for (i = 0; i < key_len; i++) {
        //     ipad[i] = ipad[i] XOR key[i]
        ipad[i] ^= key[i];
        //     opad[i] = opad[i] XOR key[i]
        opad[i] ^= key[i];
        //   end for
    }

    // Where || is concatenation
    // thash = hash(ipad || message)
    memcpy(cat_input_thash, ipad, 64);
    memcpy(&cat_input_thash[64], message, msg_len);

    // int ucl_sha256(u8 *hash, u8 *message, u32 byteLength)
    // ComputeSHA256(cat_input_thash, 64 + msg_len, FALSE, FALSE, thash);
    sha256(cat_input_thash, 64 + msg_len, thash);

    //   return hash(opad || thash)
    memcpy(cat_input_final, opad, 64);
    memcpy(&cat_input_final[64], thash, 32);

    // ComputeSHA256(cat_input_final, 64 + 32, FALSE, FALSE, mac);
    sha256(cat_input_final, 64 + 32, mac);

    //end function

    return 1;
}

