#ifndef __HYMN_PRIVATE_H__
#define __HYMN_PRIVATE_H__
/*****************************************************************************
* $Id: hymn_private.h,v 1.3 2004/07/22 22:16:48 playfair Exp $
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

/* internal data structures */
typedef struct {
    /* i/o info */
    uint8_t *inbuf;
    uint32_t inbuf_size;
    FILE* outfile;

    /* drms info */
    uint32_t drms_user;
    uint32_t drms_key;
    uint8_t *drms_iviv;
    uint32_t drms_iviv_size;
    uint8_t *drms_name;
    uint32_t drms_name_size;
    uint8_t *drms_priv;
    uint32_t drms_priv_size;
    uint32_t drms_trak_idx;

    /* mp4 / trak info */
    uint32_t **sample_chunk_table;
    uint32_t chunk_table_size;
    uint32_t *sample_table_sizes;
    uint32_t num_sample_sizes;
    uint32_t *sample_table_offsets_trak1;
    uint32_t num_sample_offsets_trak1;
    uint32_t *sample_table_offsets_trak2;
    uint32_t num_sample_offsets_trak2;
    uint32_t sinf_size;
    uint32_t trak_idx;

    /* internal context */
    bool writing;   /* first we parse, then we write */
    bool write_off; /* writing can be suspended for certain atoms */
} hymn_t;

#endif /* __HYMN_PRIVATE_H__ */
