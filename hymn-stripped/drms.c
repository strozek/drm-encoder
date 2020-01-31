
/*****************************************************************************
 * drms.c: DRMS
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: drms.c,v 1.7 2004/07/05 22:43:24 playfair Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Sam Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdlib.h>                                      /* malloc(), free() */
#include <stdio.h>
#include "drmsvl.h"
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <limits.h>

#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CFNumber.h>

#include "drms.h"
#include "drmstables.h"

/*****************************************************************************
 * aes_s: AES keys structure
 *****************************************************************************
 * This structure stores a set of keys usable for encryption and decryption
 * with the AES/Rijndael algorithm.
 *****************************************************************************/
struct aes_s
{
    uint32_t pp_enc_keys[ AES_KEY_COUNT + 1 ][ 4 ];
    uint32_t pp_dec_keys[ AES_KEY_COUNT + 1 ][ 4 ];
};

/*****************************************************************************
 * md5_s: MD5 message structure
 *****************************************************************************
 * This structure stores the static information needed to compute an MD5
 * hash. It has an extra data buffer to allow non-aligned writes.
 *****************************************************************************/
struct md5_s
{
    uint64_t i_bits;      /* Total written bits */
    uint32_t p_digest[4]; /* The MD5 digest */
    uint32_t p_data[16];  /* Buffer to cache non-aligned writes */
};

/*****************************************************************************
 * shuffle_s: shuffle structure
 *****************************************************************************
 * This structure stores the static information needed to shuffle data using
 * a custom algorithm.
 *****************************************************************************/
struct shuffle_s
{
    uint32_t i_version;
    uint32_t p_commands[ 20 ];
    uint32_t p_bordel[ 16 ];
};

#define SWAP( a, b ) { (a) ^= (b); (b) ^= (a); (a) ^= (b); }

/*****************************************************************************
 * drms_s: DRMS structure
 *****************************************************************************
 * This structure stores the static information needed to decrypt DRMS data.
 *****************************************************************************/
struct drms_s
{
    uint32_t i_user;
    uint32_t i_key;
    uint8_t  p_iviv[ 16 ];
    uint8_t *p_name;

    uint32_t p_key[ 4 ];
    struct aes_s aes;

    char     psz_homedir[ PATH_MAX ];
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void InitAES       ( struct aes_s *, uint32_t * );
static void DecryptAES    ( struct aes_s *, uint32_t *, const uint32_t * );

static void InitMD5       ( struct md5_s * );
static void AddMD5        ( struct md5_s *, const uint8_t *, uint32_t );
static void EndMD5        ( struct md5_s * );
static void Digest        ( struct md5_s *, uint32_t * );

static int GetSystemKey   ( uint32_t *, vlc_bool_t );

/*****************************************************************************
 * Reverse: reverse byte order
 *****************************************************************************/
static inline void Reverse( uint32_t *p_buffer, int n )
{
    int i;

    for( i = 0; i < n; i++ )
    {
        p_buffer[ i ] = GetDWLE(&p_buffer[ i ]);
    }
}
#    define REVERSE( p, n ) Reverse( p, n )

/*****************************************************************************
 * BlockXOR: XOR two 128 bit blocks
 *****************************************************************************/
static inline void BlockXOR( uint32_t *p_dest, uint32_t *p_s1, uint32_t *p_s2 )
{
    int i;

    for( i = 0; i < 4; i++ )
    {
        p_dest[ i ] = p_s1[ i ] ^ p_s2[ i ];
    }
}

/*****************************************************************************
 * drms_alloc: allocate a DRMS structure
 *****************************************************************************/
void *drms_alloc( char *psz_homedir )
{
    struct drms_s *p_drms;

    p_drms = malloc( sizeof(struct drms_s) );

    if( p_drms == NULL )
    {
        return NULL;
    }

    memset( p_drms, 0, sizeof(struct drms_s) );

    strncpy( p_drms->psz_homedir, psz_homedir, PATH_MAX );
    p_drms->psz_homedir[ PATH_MAX - 1 ] = '\0';

    return (void *)p_drms;
}

/*****************************************************************************
 * drms_free: free a previously allocated DRMS structure
 *****************************************************************************/
void drms_free( void *_p_drms )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;

    if( p_drms->p_name != NULL )
    {
        free( (void *)p_drms->p_name );
    }

    free( p_drms );
}

/*****************************************************************************
 * drms_decrypt: unscramble a chunk of data
 *****************************************************************************/
void drms_decrypt( void *_p_drms, uint32_t *p_buffer, uint32_t i_bytes )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    uint32_t p_key[ 4 ];
    unsigned int i_blocks;

    /* AES is a block cypher, round down the byte count */
    i_blocks = i_bytes / 16;
    i_bytes = i_blocks * 16;

    /* Initialise the key */
    memcpy( p_key, p_drms->p_key, 16 );

    /* Unscramble */
    while( i_blocks-- )
    {
        uint32_t p_tmp[ 4 ];

        REVERSE( p_buffer, 4 );
        DecryptAES( &p_drms->aes, p_tmp, p_buffer );
        BlockXOR( p_tmp, p_key, p_tmp );

        /* Use the previous scrambled data as the key for next block */
        memcpy( p_key, p_buffer, 16 );

        /* Copy unscrambled data back to the buffer */
        memcpy( p_buffer, p_tmp, 16 );
        REVERSE( p_buffer, 4 );

        p_buffer += 4;
    }
}

/*****************************************************************************
 * drms_init: initialise a DRMS structure
 *****************************************************************************/
int drms_init( void *_p_drms, uint32_t i_type,
               uint8_t *p_info, uint32_t i_len )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    int i_ret = 0;

    switch( i_type )
    {
        case FOURCC_user:
            if( i_len < sizeof(p_drms->i_user) )
            {
                i_ret = -1;
                break;
            }

            p_drms->i_user = U32_AT( p_info );
            break;

        case FOURCC_key:
            if( i_len < sizeof(p_drms->i_key) )
            {
                i_ret = -1;
                break;
            }

            p_drms->i_key = U32_AT( p_info );
            break;

        case FOURCC_iviv:
            if( i_len < sizeof(p_drms->p_key) )
            {
                i_ret = -1;
                break;
            }

            memcpy( p_drms->p_iviv, p_info, 16 );
            break;

        case FOURCC_name:
            p_drms->p_name = strdup( p_info );

            if( p_drms->p_name == NULL )
            {
                i_ret = -1;
            }
            break;

        case FOURCC_priv:
        {
            uint32_t p_priv[ 64 ];
            struct md5_s md5;

            if( i_len < 64 )
            {
                i_ret = -1;
                break;
            }

            InitMD5( &md5 );
            AddMD5( &md5, p_drms->p_name, strlen( p_drms->p_name ) );
            AddMD5( &md5, p_drms->p_iviv, 16 );
            EndMD5( &md5 );

            if( p_drms->i_user == 0 && p_drms->i_key == 0 )
            {
                static char const p_secret[] = "tr1-th3n.y00_by3";
                memcpy( p_drms->p_key, p_secret, 16 );
                REVERSE( p_drms->p_key, 4 );
            }
            else
            {
                if( GetUserKey( p_drms, p_drms->p_key ) )
                {
                    i_ret = -1;
                    break;
                }
            }

            InitAES( &p_drms->aes, p_drms->p_key );

            memcpy( p_priv, p_info, 64 );
            memcpy( p_drms->p_key, md5.p_digest, 16 );
            drms_decrypt( p_drms, p_priv, 64 );
            REVERSE( p_priv, 64 );

            if( p_priv[ 0 ] != 0x6e757469 ) /* itun */
            {
                i_ret = -1;
                break;
            }

            InitAES( &p_drms->aes, p_priv + 6 );
            memcpy( p_drms->p_key, p_priv + 12, 16 );

            free( (void *)p_drms->p_name );
            p_drms->p_name = NULL;
        }
        break;
    }

    return i_ret;
}

/*****************************************************************************
 * get_user_key: get the user key
 *****************************************************************************/
int get_user_key( char *psz_homedir, uint32_t i_user,
                  uint32_t i_key, uint32_t *p_user_key )
{
    struct drms_s drms;

    drms.i_user = i_user;
    drms.i_key = i_key;
    strcpy(drms.psz_homedir, psz_homedir);
    drms.psz_homedir[ PATH_MAX - 1 ] = '\0';
    return GetUserKey(&drms, p_user_key);
}

/* The following functions are local */

/*****************************************************************************
 * InitAES: initialise AES/Rijndael encryption/decryption tables
 *****************************************************************************
 * The Advanced Encryption Standard (AES) is described in RFC 3268
 *****************************************************************************/
static void InitAES( struct aes_s *p_aes, uint32_t *p_key )
{
    unsigned int i, t;
    uint32_t i_key, i_seed;

    memset( p_aes->pp_enc_keys[1], 0, 16 );
    memcpy( p_aes->pp_enc_keys[0], p_key, 16 );

    /* Generate the key tables */
    i_seed = p_aes->pp_enc_keys[ 0 ][ 3 ];

    for( i_key = 0; i_key < AES_KEY_COUNT; i_key++ )
    {
        uint32_t j;

        i_seed = AES_ROR( i_seed, 8 );

        j = p_aes_table[ i_key ];

        j ^= p_aes_encrypt[ (i_seed >> 24) & 0xff ]
              ^ AES_ROR( p_aes_encrypt[ (i_seed >> 16) & 0xff ], 8 )
              ^ AES_ROR( p_aes_encrypt[ (i_seed >> 8) & 0xff ], 16 )
              ^ AES_ROR( p_aes_encrypt[ i_seed & 0xff ], 24 );

        j ^= p_aes->pp_enc_keys[ i_key ][ 0 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 0 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 1 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 1 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 2 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 2 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 3 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 3 ] = j;

        i_seed = j;
    }

    memcpy( p_aes->pp_dec_keys[ 0 ],
            p_aes->pp_enc_keys[ 0 ], 16 );

    for( i = 1; i < AES_KEY_COUNT; i++ )
    {
        for( t = 0; t < 4; t++ )
        {
            uint32_t j, k, l, m, n;

            j = p_aes->pp_enc_keys[ i ][ t ];

            k = (((j >> 7) & 0x01010101) * 27) ^ ((j & 0xff7f7f7f) << 1);
            l = (((k >> 7) & 0x01010101) * 27) ^ ((k & 0xff7f7f7f) << 1);
            m = (((l >> 7) & 0x01010101) * 27) ^ ((l & 0xff7f7f7f) << 1);

            j ^= m;

            n = AES_ROR( l ^ j, 16 ) ^ AES_ROR( k ^ j, 8 ) ^ AES_ROR( j, 24 );

            p_aes->pp_dec_keys[ i ][ t ] = k ^ l ^ m ^ n;
        }
    }
}

/*****************************************************************************
 * DecryptAES: decrypt an AES/Rijndael 128 bit block
 *****************************************************************************/
static void DecryptAES( struct aes_s *p_aes,
                        uint32_t *p_dest, const uint32_t *p_src )
{
    uint32_t p_wtxt[ 4 ]; /* Working cyphertext */
    uint32_t p_tmp[ 4 ];
    unsigned int i_round, t;

    for( t = 0; t < 4; t++ )
    {
        /* FIXME: are there any endianness issues here? */
        p_wtxt[ t ] = p_src[ t ] ^ p_aes->pp_enc_keys[ AES_KEY_COUNT ][ t ];
    }

    /* Rounds 0 - 8 */
    for( i_round = 0; i_round < (AES_KEY_COUNT - 1); i_round++ )
    {
        for( t = 0; t < 4; t++ )
        {
            p_tmp[ t ] = AES_XOR_ROR( p_aes_itable, p_wtxt );
        }

        for( t = 0; t < 4; t++ )
        {
            p_wtxt[ t ] = p_tmp[ t ]
                    ^ p_aes->pp_dec_keys[ (AES_KEY_COUNT - 1) - i_round ][ t ];
        }
    }

    /* Final round (9) */
    for( t = 0; t < 4; t++ )
    {
        p_dest[ t ] = AES_XOR_ROR( p_aes_decrypt, p_wtxt );
        p_dest[ t ] ^= p_aes->pp_dec_keys[ 0 ][ t ];
    }
}

/*****************************************************************************
 * InitMD5: initialise an MD5 message
 *****************************************************************************
 * The MD5 message-digest algorithm is described in RFC 1321
 *****************************************************************************/
static void InitMD5( struct md5_s *p_md5 )
{
    p_md5->p_digest[ 0 ] = 0x67452301;
    p_md5->p_digest[ 1 ] = 0xefcdab89;
    p_md5->p_digest[ 2 ] = 0x98badcfe;
    p_md5->p_digest[ 3 ] = 0x10325476;

    memset( p_md5->p_data, 0, 64 );
    p_md5->i_bits = 0;
}

/*****************************************************************************
 * AddMD5: add i_len bytes to an MD5 message
 *****************************************************************************/
static void AddMD5( struct md5_s *p_md5, const uint8_t *p_src, uint32_t i_len )
{
    unsigned int i_current; /* Current bytes in the spare buffer */
    unsigned int i_offset = 0;

    i_current = (p_md5->i_bits / 8) & 63;

    p_md5->i_bits += 8 * i_len;

    /* If we can complete our spare buffer to 64 bytes, do it and add the
     * resulting buffer to the MD5 message */
    if( i_len >= (64 - i_current) )
    {
        memcpy( ((uint8_t *)p_md5->p_data) + i_current, p_src,
                (64 - i_current) );
        Digest( p_md5, p_md5->p_data );

        i_offset += (64 - i_current);
        i_len -= (64 - i_current);
        i_current = 0;
    }

    /* Add as many entire 64 bytes blocks as we can to the MD5 message */
    while( i_len >= 64 )
    {
        uint32_t p_tmp[ 16 ];
        memcpy( p_tmp, p_src + i_offset, 64 );
        Digest( p_md5, p_tmp );
        i_offset += 64;
        i_len -= 64;
    }

    /* Copy our remaining data to the message's spare buffer */
    memcpy( ((uint8_t *)p_md5->p_data) + i_current, p_src + i_offset, i_len );
}

/*****************************************************************************
 * EndMD5: finish an MD5 message
 *****************************************************************************
 * This function adds adequate padding to the end of the message, and appends
 * the bit count so that we end at a block boundary.
 *****************************************************************************/
static void EndMD5( struct md5_s *p_md5 )
{
    unsigned int i_current;

    i_current = (p_md5->i_bits / 8) & 63;

    /* Append 0x80 to our buffer. No boundary check because the temporary
     * buffer cannot be full, otherwise AddMD5 would have emptied it. */
    ((uint8_t *)p_md5->p_data)[ i_current++ ] = 0x80;

    /* If less than 8 bytes are available at the end of the block, complete
     * this 64 bytes block with zeros and add it to the message. We'll add
     * our length at the end of the next block. */
    if( i_current > 56 )
    {
        memset( ((uint8_t *)p_md5->p_data) + i_current, 0, (64 - i_current) );
        Digest( p_md5, p_md5->p_data );
        i_current = 0;
    }

    /* Fill the unused space in our last block with zeroes and put the
     * message length at the end. */
    memset( ((uint8_t *)p_md5->p_data) + i_current, 0, (56 - i_current) );
    p_md5->p_data[ 14 ] = p_md5->i_bits & 0xffffffff;
    p_md5->p_data[ 15 ] = (p_md5->i_bits >> 32);
    REVERSE( &p_md5->p_data[ 14 ], 2 );

    Digest( p_md5, p_md5->p_data );
}

#define F1( x, y, z ) ((z) ^ ((x) & ((y) ^ (z))))
#define F2( x, y, z ) F1((z), (x), (y))
#define F3( x, y, z ) ((x) ^ (y) ^ (z))
#define F4( x, y, z ) ((y) ^ ((x) | ~(z)))

#define MD5_DO( f, w, x, y, z, data, s ) \
    ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*****************************************************************************
 * Digest: update the MD5 digest with 64 bytes of data
 *****************************************************************************/
static void Digest( struct md5_s *p_md5, uint32_t *p_input )
{
    uint32_t a, b, c, d;

    REVERSE( p_input, 16 );

    a = p_md5->p_digest[ 0 ];
    b = p_md5->p_digest[ 1 ];
    c = p_md5->p_digest[ 2 ];
    d = p_md5->p_digest[ 3 ];

    MD5_DO( F1, a, b, c, d, p_input[  0 ] + 0xd76aa478,  7 );
    MD5_DO( F1, d, a, b, c, p_input[  1 ] + 0xe8c7b756, 12 );
    MD5_DO( F1, c, d, a, b, p_input[  2 ] + 0x242070db, 17 );
    MD5_DO( F1, b, c, d, a, p_input[  3 ] + 0xc1bdceee, 22 );
    MD5_DO( F1, a, b, c, d, p_input[  4 ] + 0xf57c0faf,  7 );
    MD5_DO( F1, d, a, b, c, p_input[  5 ] + 0x4787c62a, 12 );
    MD5_DO( F1, c, d, a, b, p_input[  6 ] + 0xa8304613, 17 );
    MD5_DO( F1, b, c, d, a, p_input[  7 ] + 0xfd469501, 22 );
    MD5_DO( F1, a, b, c, d, p_input[  8 ] + 0x698098d8,  7 );
    MD5_DO( F1, d, a, b, c, p_input[  9 ] + 0x8b44f7af, 12 );
    MD5_DO( F1, c, d, a, b, p_input[ 10 ] + 0xffff5bb1, 17 );
    MD5_DO( F1, b, c, d, a, p_input[ 11 ] + 0x895cd7be, 22 );
    MD5_DO( F1, a, b, c, d, p_input[ 12 ] + 0x6b901122,  7 );
    MD5_DO( F1, d, a, b, c, p_input[ 13 ] + 0xfd987193, 12 );
    MD5_DO( F1, c, d, a, b, p_input[ 14 ] + 0xa679438e, 17 );
    MD5_DO( F1, b, c, d, a, p_input[ 15 ] + 0x49b40821, 22 );

    MD5_DO( F2, a, b, c, d, p_input[  1 ] + 0xf61e2562,  5 );
    MD5_DO( F2, d, a, b, c, p_input[  6 ] + 0xc040b340,  9 );
    MD5_DO( F2, c, d, a, b, p_input[ 11 ] + 0x265e5a51, 14 );
    MD5_DO( F2, b, c, d, a, p_input[  0 ] + 0xe9b6c7aa, 20 );
    MD5_DO( F2, a, b, c, d, p_input[  5 ] + 0xd62f105d,  5 );
    MD5_DO( F2, d, a, b, c, p_input[ 10 ] + 0x02441453,  9 );
    MD5_DO( F2, c, d, a, b, p_input[ 15 ] + 0xd8a1e681, 14 );
    MD5_DO( F2, b, c, d, a, p_input[  4 ] + 0xe7d3fbc8, 20 );
    MD5_DO( F2, a, b, c, d, p_input[  9 ] + 0x21e1cde6,  5 );
    MD5_DO( F2, d, a, b, c, p_input[ 14 ] + 0xc33707d6,  9 );
    MD5_DO( F2, c, d, a, b, p_input[  3 ] + 0xf4d50d87, 14 );
    MD5_DO( F2, b, c, d, a, p_input[  8 ] + 0x455a14ed, 20 );
    MD5_DO( F2, a, b, c, d, p_input[ 13 ] + 0xa9e3e905,  5 );
    MD5_DO( F2, d, a, b, c, p_input[  2 ] + 0xfcefa3f8,  9 );
    MD5_DO( F2, c, d, a, b, p_input[  7 ] + 0x676f02d9, 14 );
    MD5_DO( F2, b, c, d, a, p_input[ 12 ] + 0x8d2a4c8a, 20 );

    MD5_DO( F3, a, b, c, d, p_input[  5 ] + 0xfffa3942,  4 );
    MD5_DO( F3, d, a, b, c, p_input[  8 ] + 0x8771f681, 11 );
    MD5_DO( F3, c, d, a, b, p_input[ 11 ] + 0x6d9d6122, 16 );
    MD5_DO( F3, b, c, d, a, p_input[ 14 ] + 0xfde5380c, 23 );
    MD5_DO( F3, a, b, c, d, p_input[  1 ] + 0xa4beea44,  4 );
    MD5_DO( F3, d, a, b, c, p_input[  4 ] + 0x4bdecfa9, 11 );
    MD5_DO( F3, c, d, a, b, p_input[  7 ] + 0xf6bb4b60, 16 );
    MD5_DO( F3, b, c, d, a, p_input[ 10 ] + 0xbebfbc70, 23 );
    MD5_DO( F3, a, b, c, d, p_input[ 13 ] + 0x289b7ec6,  4 );
    MD5_DO( F3, d, a, b, c, p_input[  0 ] + 0xeaa127fa, 11 );
    MD5_DO( F3, c, d, a, b, p_input[  3 ] + 0xd4ef3085, 16 );
    MD5_DO( F3, b, c, d, a, p_input[  6 ] + 0x04881d05, 23 );
    MD5_DO( F3, a, b, c, d, p_input[  9 ] + 0xd9d4d039,  4 );
    MD5_DO( F3, d, a, b, c, p_input[ 12 ] + 0xe6db99e5, 11 );
    MD5_DO( F3, c, d, a, b, p_input[ 15 ] + 0x1fa27cf8, 16 );
    MD5_DO( F3, b, c, d, a, p_input[  2 ] + 0xc4ac5665, 23 );

    MD5_DO( F4, a, b, c, d, p_input[  0 ] + 0xf4292244,  6 );
    MD5_DO( F4, d, a, b, c, p_input[  7 ] + 0x432aff97, 10 );
    MD5_DO( F4, c, d, a, b, p_input[ 14 ] + 0xab9423a7, 15 );
    MD5_DO( F4, b, c, d, a, p_input[  5 ] + 0xfc93a039, 21 );
    MD5_DO( F4, a, b, c, d, p_input[ 12 ] + 0x655b59c3,  6 );
    MD5_DO( F4, d, a, b, c, p_input[  3 ] + 0x8f0ccc92, 10 );
    MD5_DO( F4, c, d, a, b, p_input[ 10 ] + 0xffeff47d, 15 );
    MD5_DO( F4, b, c, d, a, p_input[  1 ] + 0x85845dd1, 21 );
    MD5_DO( F4, a, b, c, d, p_input[  8 ] + 0x6fa87e4f,  6 );
    MD5_DO( F4, d, a, b, c, p_input[ 15 ] + 0xfe2ce6e0, 10 );
    MD5_DO( F4, c, d, a, b, p_input[  6 ] + 0xa3014314, 15 );
    MD5_DO( F4, b, c, d, a, p_input[ 13 ] + 0x4e0811a1, 21 );
    MD5_DO( F4, a, b, c, d, p_input[  4 ] + 0xf7537e82,  6 );
    MD5_DO( F4, d, a, b, c, p_input[ 11 ] + 0xbd3af235, 10 );
    MD5_DO( F4, c, d, a, b, p_input[  2 ] + 0x2ad7d2bb, 15 );
    MD5_DO( F4, b, c, d, a, p_input[  9 ] + 0xeb86d391, 21 );

    p_md5->p_digest[ 0 ] += a;
    p_md5->p_digest[ 1 ] += b;
    p_md5->p_digest[ 2 ] += c;
    p_md5->p_digest[ 3 ] += d;
}


#define DRMS_DIRNAME ".drms"

/*****************************************************************************
 * GetUserKey: get the user key
 *****************************************************************************
 * Retrieve the user key from the hard disk if available, otherwise err
 *****************************************************************************/
static int GetUserKey( void *_p_drms, uint32_t *p_user_key )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    FILE *file;
    int i_ret = -1;
    char psz_path[ PATH_MAX ];

    snprintf( psz_path, PATH_MAX - 1,
              "%s/" DRMS_DIRNAME "/%08X.%03d", p_drms->psz_homedir,
              p_drms->i_user, p_drms->i_key );

    file = fopen( psz_path, "rb" );
    if( file != NULL )
    {
        i_ret = fread( p_user_key, sizeof(uint32_t),
                       4, file ) == 4 ? 0 : -1;
        fclose( file );
    }
    if(i_ret)
    {
        return i_ret;
    }
    REVERSE( p_user_key, 4 );
    return 0;
}

