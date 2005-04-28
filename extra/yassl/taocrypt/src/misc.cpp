/* misc.cpp                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* based on Wei Dai's misc.cpp from CryptoPP */


#include "runtime.hpp"
#include "misc.hpp"
#include <new>        // for NewHandler


void* operator new(size_t sz, TaoCrypt::new_t)
{
    void* ptr = ::operator new(sz);

    if (!ptr) abort();

    return ptr;
}

void* operator new[](size_t sz, TaoCrypt::new_t tc)
{
#if defined(_MSC_VER) && (_MSC_VER < 1300)
    void* ptr = ::operator new(sz);         // no ::operator new[]
#else
    void* ptr = ::operator new[](sz);
#endif

    if (!ptr) abort();

    return ptr;
}



namespace TaoCrypt {


new_t tc;   // for library new


inline void XorWords(word* r, const word* a, unsigned int n)
{
    for (unsigned int i=0; i<n; i++)
        r[i] ^= a[i];
}


void xorbuf(byte* buf, const byte* mask, unsigned int count)
{
    if (((size_t)buf | (size_t)mask | count) % WORD_SIZE == 0)
        XorWords((word *)buf, (const word *)mask, count/WORD_SIZE);
    else
    {
        for (unsigned int i=0; i<count; i++)
            buf[i] ^= mask[i];
    }
}


unsigned int BytePrecision(unsigned long value)
{
    unsigned int i;
    for (i=sizeof(value); i; --i)
        if (value >> (i-1)*8)
            break;

    return i;
}


unsigned int BitPrecision(unsigned long value)
{
    if (!value)
        return 0;

    unsigned int l = 0,
                 h = 8 * sizeof(value);

    while (h-l > 1)
    {
        unsigned int t = (l+h)/2;
        if (value >> t)
            l = t;
        else
            h = t;
    }

    return h;
}


unsigned long Crop(unsigned long value, unsigned int size)
{
    if (size < 8*sizeof(value))
        return (value & ((1L << size) - 1));
    else
        return value;
}


#if !(defined(_MSC_VER) && (_MSC_VER < 1300))
using std::new_handler;
using std::set_new_handler;
#endif

void CallNewHandler()
{
    abort();
}


}  // namespace

