#ifndef __ENDIANUTILS_H__
#define __ENDIANUTILS_H__
/*****************************************************************************
 * $Id: endianutils.h,v 1.1.1.1 2004/05/07 23:33:55 playfair Exp $
 *****************************************************************************
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111,
 * USA.
 *****************************************************************************/

static inline uint32_t uint32_from_bytes_be( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
             | ((uint32_t)p[2] << 8) | p[3] );
}
static inline uint64_t uint64_from_bytes_be( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
             | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
             | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
             | ((uint64_t)p[6] << 8) | p[7] );
}
static inline uint32_t uint32_from_bytes_le( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16)
             | ((uint32_t)p[1] << 8) | p[0] );
}
static inline uint64_t uint64_from_bytes_le( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint64_t)p[7] << 56) | ((uint64_t)p[6] << 48)
             | ((uint64_t)p[5] << 40) | ((uint64_t)p[4] << 32)
             | ((uint64_t)p[3] << 24) | ((uint64_t)p[2] << 16)
             | ((uint64_t)p[1] << 8) | p[0] );
}
static inline void bytes_from_uint32_be(uint32_t from, uint8_t *to)
{
    to[0] = (uint8_t)(from >> 24);
    to[1] = (uint8_t)(from >> 16);
    to[2] = (uint8_t)(from >> 8);
    to[3] = (uint8_t)(from);
}
static inline void bytes_from_uint32_le(uint32_t from, uint8_t *to)
{
    to[0] = (uint8_t)(from);
    to[1] = (uint8_t)(from >> 8);
    to[2] = (uint8_t)(from >> 16);
    to[3] = (uint8_t)(from >> 24);
}

#   define UINT32_FROM_BYTES(a, b, c, d)                        \
    ( ((uint32_t)d) | ( ((uint32_t)c) << 8 )                    \
      | ( ((uint32_t)b) << 16 ) | ( ((uint32_t)a) << 24 ) )
#   define uint32_from_bytes(a) uint32_from_bytes_be(a)
#   define uint64_from_bytes(a) uint64_from_bytes_be(a)
#   define bytes_from_uint32(a, b) bytes_from_uint32_be((a),(b))
/*****************************************************************************
 * reverse_bytes: reverse byte order
 *****************************************************************************/
static inline void reverse_bytes(uint32_t *p_buffer, int n)
{
    int i;

    for(i = 0; i < n; i++)
    {
        p_buffer[ i ] = uint32_from_bytes_le(&p_buffer[ i ]);
    }
}
#   define REVERSE(p, n) reverse_bytes(p, n)

#endif /* __ENDIANUTILS_H__ */
