/*****************************************************************************
 * $Id: atoms.c,v 1.16 2004/07/28 22:09:12 playfair Exp $
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

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "hymn_private.h"

#include "endianutils.h"
#include "atoms.h"

#include "aes.h"
#include "md5.h"

char* get_homedir( void );
int get_user_key( char *psz_homedir, uint32_t i_user,
                  uint32_t i_key, uint32_t *p_user_key );

void decrypt(uint8_t *input, uint8_t *output, uint32_t len,
             uint8_t *key, uint8_t *iv) {
    aes_context ctx;
    aes_set_key(&ctx, key, 128);
    aes_cbc_decrypt(&ctx, input, output, len, iv);
}

bool is_container(uint32_t atom_id) {
    switch (atom_id) {
    case ATOM_ALB:
    case ATOM_ART:
    case ATOM_CLIP:
    case ATOM_CMT:
    case ATOM_COVR:
    case ATOM_CPIL:
    case ATOM_DAY:
    case ATOM_DINF:
    case ATOM_DISK:
    case ATOM_DRMS:
    case ATOM_EDTS:
    case ATOM_ILST:
    case ATOM_MATT:
    case ATOM_MDIA:
    case ATOM_META:
    case ATOM_MINF:
    case ATOM_MOOV:
    case ATOM_NAM:
    case ATOM_SCHI:
    case ATOM_SINF:
    case ATOM_STBL:
    case ATOM_TMPO:
    case ATOM_TOO:
    case ATOM_TRAK:
    case ATOM_TRKN:
    case ATOM_UDTA:
    case ATOM_WRT:
        return true;
    default:
        return false;
    }
}

void print_atom(hymn_t *ctx,
                uint32_t atom_id, uint32_t len, uint32_t level) {
    /* only print on first loop */
    if (!ctx->writing) {
        int i;
        char indent[32];
        char atom_name[5];
        bytes_from_uint32(atom_id, atom_name);
        atom_name[4] = 0;

        for (i=0; i<(2*level); ++i) {
            indent[i] = ' ';
        }
        indent[i] = 0;
    }
}

int write_block(hymn_t *ctx,
                void *data, uint32_t size) {
    if (ctx->writing && !ctx->write_off) {
        fwrite(data, 1, size, ctx->outfile);
    }
    return 0;
}

int write_header(hymn_t *ctx,
                 uint32_t len, uint32_t atom_id) {
    if (ctx->writing && !ctx->write_off) {
        int err;
        /* If our atom is in the hierarchy containing the sinf atom,
         * we need to subtrace the size of the sinf atom from the size
         * of the containing atom.
         */
        switch(atom_id) {
        case ATOM_DRMS:
            atom_id = ATOM_MP4A;
            /* no break here */
        case ATOM_STSD:
        case ATOM_STBL:
        case ATOM_MINF:
        case ATOM_MDIA:
        case ATOM_TRAK:
        case ATOM_MOOV:
            /* audiobooks have two trak atoms -- skip the one w/o drms info */
            if ( (ctx->drms_trak_idx == ctx->trak_idx) ||
                 atom_id == ATOM_MOOV ) {
                len -= ctx->sinf_size;
            }
            /* no break here */
        default:
            len += 8;
            uint8_t scratch[4];
            bytes_from_uint32_be(len, scratch);
            err = write_block(ctx, scratch, sizeof(scratch) );
            if (err) return err;
            bytes_from_uint32(atom_id, scratch);
            err = write_block(ctx, scratch, sizeof(scratch) );
            if (err) return err;
            break;
        }
    }
    return 0;
}

int write_atom(hymn_t *ctx,
               uint8_t **fp, uint32_t size) {
    return write_block(ctx, (*fp)-8, size+8);
}

int parse_iviv_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    /* just parse during the first pass */
    if (!ctx->writing) {
        ctx->drms_iviv = malloc(size);
        memcpy(ctx->drms_iviv, *fp, size);
        ctx->drms_iviv_size = size;
    }
    (*fp) += size;
    return 0;
}

int parse_name_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    /* just parse during the first pass */
    if (!ctx->writing) {
        ctx->drms_name = malloc(size);
        memcpy(ctx->drms_name, *fp, size);
        ctx->drms_name_size = strlen(ctx->drms_name);
    }
    (*fp) += size;
    return 0;
}

int parse_priv_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    /* just parse during the first pass */
    if (!ctx->writing) {
        ctx->drms_priv = malloc(size);
        memcpy(ctx->drms_priv, *fp, size);
        ctx->drms_priv_size = size;
    }

    (*fp) += size;
    return 0;
}

int parse_drms_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    int err;
    uint32_t atom_id, len;

    err = write_block( ctx, *fp, (6+2+8+2+2+4+4) ); /* see below */
    if (err) return err;

    (*fp) += 6; /* reserved */
    (*fp) += 2; /* data reference index */
    (*fp) += 8; /* version, revision, vendor */
    (*fp) += 2; /* channel count */
    (*fp) += 2; /* sample size */
    (*fp) += 4; /* compression id, packet size */
    (*fp) += 4; /* sample rate */

    /* iTunes audiobooks contain two 'trak' atoms.  After we find the
     * drms info, we know that we've read the right trak atom and
     * should ignore the second one. */
    ctx->drms_trak_idx = ctx->trak_idx;

    /* esds */
    err = parse_atom(ctx, fp, size, level+1);
    if (err) return err;

    /* sinf */
    err = parse_atom(ctx, fp, size, level+1);
    if (err) return err;

    return 0;
}

int parse_stsd_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    int err;
    uint32_t entry_count, len, atom_id, i;

    err = write_block( ctx, *fp, (1+3+4) );
    if (err) return err;

    (*fp) += 1; /* version */
    (*fp) += 3; /* flags */
    entry_count = uint32_from_bytes_be(*fp);
    (*fp) += 4;

    /* deal w/ entry count */
    for (i=0; i<entry_count; ++i) {
        uint32_t len = uint32_from_bytes_be(*fp) - 8;
        (*fp) += 4;
        uint32_t atom_id = uint32_from_bytes(*fp);
        (*fp) += 4;

        switch(atom_id) {
        case ATOM_DRMS:
            print_atom(ctx, atom_id, len, level+1);
            err = write_header(ctx, len, atom_id);
            if (err) return err;
            err = parse_drms_atom(ctx, fp, len, level+1);
            if (err) return err;
            break;
        default:
            /* copy atom to outfile */
            print_atom(ctx, atom_id, len, level+1);
            err = write_atom(ctx, fp, len);
            if (err) return err;
            (*fp) += len;
            break;
        }
    }

    return 0;
}

int parse_stsc_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    int err;

    if (!ctx->writing) {
        /* audiobooks have two tracks -- we only need the sample chunk
         * table for the track with the drms info */
        if (ctx->drms_trak_idx == ctx->trak_idx) {
            uint32_t i;

            (*fp) += 1; /* version */
            (*fp) += 3; /* flags */
            ctx->chunk_table_size = uint32_from_bytes_be(*fp);
            (*fp) += 4;

            /* deal w/ entry count */
            ctx->sample_chunk_table = malloc( ctx->chunk_table_size *
                                              sizeof(uint32_t) );
            for (i=0; i<ctx->chunk_table_size; ++i) {
                ctx->sample_chunk_table[i] = malloc( 3 * sizeof(uint32_t) );
                ctx->sample_chunk_table[i][0] = uint32_from_bytes_be(*fp);
                (*fp) += 4;
                ctx->sample_chunk_table[i][1] = uint32_from_bytes_be(*fp);
                (*fp) += 4;
                ctx->sample_chunk_table[i][2] = uint32_from_bytes_be(*fp);
                (*fp) += 4;
            }
        } else {
            (*fp) += size;
        }
    } else {
        /* copy atom to outfile */
        err = write_atom(ctx, fp, size);
        if (err) return err;
        (*fp) += size;

    }

    return 0;
}

int parse_stsz_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    if (!ctx->writing) {
        /* audiobooks have two tracks -- we only need the sample table
         * sizes for the track with the drms info */
        if (ctx->drms_trak_idx == ctx->trak_idx) {
            /* get the sample table sizes */
            uint32_t i;

            /* first 12 bytes is some sort of header */
            (*fp) += 12;
            size -= 12;
            ctx->num_sample_sizes = size / 4;
            ctx->sample_table_sizes = malloc(size);
            for (i=0; i<ctx->num_sample_sizes; ++i) {
                ctx->sample_table_sizes[i] = uint32_from_bytes_be(*fp);
                (*fp) += 4;
            }
        } else {
            (*fp) += size;
        }
    } else {
        /* copy atom to outfile */
        int err;
        err = write_atom(ctx, fp, size);
        if (err) return err;
        (*fp) += size;
    }

    return 0;
}

int parse_stco_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    /* The first time through, just skip over the stco atom.  The
     * second time through, we need to subtract the sinf atom size
     * from each sample offset.
     */
    if (!ctx->writing) {
        uint32_t i;
        uint32_t *sample_table_offsets;
        uint32_t num_sample_offsets;

        /* first 8 bytes is some sort of header */
        (*fp) += 8;
        size -= 8;

        /* get the sample table offsets */
        num_sample_offsets = size / 4;
        sample_table_offsets = malloc(size);
        for (i=0; i<num_sample_offsets; ++i) {
            sample_table_offsets[i] = uint32_from_bytes_be(*fp);
            (*fp) += 4;
        }

        /* audiobooks have two tracks */
        if (ctx->trak_idx == 0) {
            ctx->sample_table_offsets_trak1 = sample_table_offsets;
            ctx->num_sample_offsets_trak1 = num_sample_offsets;
        } else {
            ctx->sample_table_offsets_trak2 = sample_table_offsets;
            ctx->num_sample_offsets_trak2 = num_sample_offsets;
        }
    } else {
        uint32_t i, tmp;
        uint32_t *sample_table_offsets;
        uint32_t num_sample_offsets;
        uint8_t block[4];
        int err;

        /* audiobooks have two tracks */
        if (ctx->trak_idx == 0) {
            sample_table_offsets = ctx->sample_table_offsets_trak1;
            num_sample_offsets = ctx->num_sample_offsets_trak1;
        } else {
            sample_table_offsets = ctx->sample_table_offsets_trak2;
            num_sample_offsets = ctx->num_sample_offsets_trak2;
        }

        /* first 8 bytes is some sort of header */
        err = write_block(ctx, (*fp)-8, 16);
        if (err) return err;
        (*fp) += 8;
        size -= 8;

        for (i=0; i<num_sample_offsets; ++i) {
            /* subtract out the 'sinf' atom size */
            tmp = sample_table_offsets[i] - ctx->sinf_size;
            bytes_from_uint32_be(tmp, block);
            err = write_block(ctx, block, 4);
            if (err) return err;
            (*fp) += 4;
        }
    }

    /* 'stco' should be the final 'trak' sub-atom */
    ctx->trak_idx++;

    return 0;
}

int parse_mdat_atom(hymn_t *ctx,
                    uint8_t **fp, uint32_t size, uint32_t level) {
    /* The first time through, just skip over the mdat atom.  The
     * second time through, decrypt and write it out to the output
     * file.
     */
    if (!ctx->writing) {
        (*fp) += size;
    } else {
        int i, j, k, l, err;
        uint32_t samples_total_size = 0;
        uint8_t key[16], iv[16], block[4096];
        uint32_t wide;

        /* header */
        err = write_header(ctx, size, ATOM_MDAT);
        if (err) return err;

        /* the 'mdat' atom may have a 'wide' atom, in which case we
         * need to copy it to the output file. */
        wide = uint32_from_bytes(*fp + 4);
        if (wide == ATOM_WIDE) {
            err = write_block(ctx, *fp, 16);
            if (err) return err;
            (*fp) += 16;
        }

        /* data */
        memcpy(key, ctx->drms_priv+24, 16);
        memcpy(iv, ctx->drms_priv+48, 16);

        k=0;
        l=0;
        samples_total_size = 0;
        for (i=0; i<ctx->num_sample_offsets_trak1; ++i) {
            /* if our file pointer's current location is in between
             * chunks of sound samples, write out the in-between
             * data */
            if ( (*fp) - ctx->inbuf < ctx->sample_table_offsets_trak1[i] ) {
                uint32_t exbytes = ctx->sample_table_offsets_trak1[i] -
                    ( (*fp) - ctx->inbuf );
                err = write_block(ctx, fp, exbytes);
                if (err) return err;
            }
            /* if our sample offset number is equal to the next chunk
             * number, use the next chunk number */
            if (k+1 < ctx->chunk_table_size &&
                i+1 == ctx->sample_chunk_table[k+1][0]) {
                ++k;
            }
            (*fp) = ctx->inbuf + ctx->sample_table_offsets_trak1[i];
            for (j=0; j<ctx->sample_chunk_table[k][1]; ++j, ++l) {
                decrypt(*fp, block, ctx->sample_table_sizes[l], key, iv);
                err = write_block(ctx, block, ctx->sample_table_sizes[l]);
                if (err) return err;
                samples_total_size += ctx->sample_table_sizes[l];
                (*fp) += ctx->sample_table_sizes[l];
            }
        }
    }

    return 0;
}

int parse_meta_atom(hymn_t *ctx, uint8_t **fp,
                    uint32_t size, uint32_t atom_id, uint32_t level) {
    int err;
    uint32_t len;

    err = write_block( ctx, *fp, (1+3) );
    if (err) return err;
    (*fp) += 1; /* version */
    (*fp) += 3; /* flags */

    err = parse_atoms(ctx, fp, size-4, level);
    if (err) return err;

    return 0;
}

int parse_atom(hymn_t *ctx,
               uint8_t **fp, uint32_t size, uint32_t level) {
    int err;
    uint32_t len, atom_id;

    len = uint32_from_bytes_be(*fp) - 8;
    (*fp) += 4;
    atom_id = uint32_from_bytes(*fp);
    (*fp) += 4;

    print_atom(ctx, atom_id, len, level);

    switch(atom_id) {
    case ATOM_STSD:
        err = write_header(ctx, len, atom_id);
        if (err) return err;
        err = parse_stsd_atom(ctx, fp, len, level);
        if (err) return err;
        break;
    case ATOM_USER:
        ctx->drms_user = uint32_from_bytes_be(*fp);
        (*fp) += 4;
        break;
    case ATOM_KEY:
        ctx->drms_key = uint32_from_bytes_be(*fp);
        (*fp) += 4;
        break;
    case ATOM_IVIV:
        err = parse_iviv_atom(ctx, fp, len, level+1);
        if (err) return err;
        break;
    case ATOM_NAME:
        err = parse_name_atom(ctx, fp, len, level+1);
        if (err) return err;
        break;
    case ATOM_PRIV:
        err = parse_priv_atom(ctx, fp, len, level+1);
        if (err) return err;
        break;
    case ATOM_STSC:
        err = parse_stsc_atom(ctx, fp, len, level+1);
        if (err) return err;
        break;
    case ATOM_STSZ:
        err = parse_stsz_atom(ctx, fp, len, level+1);
        if (err) return err;
        break;
    case ATOM_MDAT:
        err = parse_mdat_atom(ctx, fp, len, level+1);
        if (err) return err;
        break;
    case ATOM_STCO:
        err = parse_stco_atom(ctx, fp, len, level+1);
        if (err) return err;
        break;
    case ATOM_META:
        err = write_header(ctx, len, atom_id);
        if (err) return err;
        err = parse_meta_atom(ctx, fp, len, atom_id, level+1);
        if (err) return err;
        break;
    case ATOM_SINF:
        ctx->write_off = true;
        /* audiobooks have two tracks -- we only need the sinf size
         * for the trak with the drms info */
        if (ctx->drms_trak_idx == ctx->trak_idx) {
            ctx->sinf_size = len + 8;
        }
        err = parse_atoms(ctx, fp, len, level+1);
        if (err) return err;
        ctx->write_off = false;
        break;
    case ATOM_APID:
    case ATOM_GEID:
        err = write_block(ctx, (*fp)-8, 4);
        if (err) return err;
        err = write_block(ctx, "free", 4);
        if (err) return err;
        err = write_block(ctx, *fp, len);
        if (err) return err;
        (*fp) += len;
        break;
    default:
        if ( is_container(atom_id) ) {
            /* copy header to outfile */
            err = write_header(ctx, len, atom_id);
            if (err) return err;
            err = parse_atoms(ctx, fp, len, level+1);
            if (err) return err;
        } else {
            /* copy atom to outfile */
            err = write_atom(ctx, fp, len);
            if (err) return err;
            (*fp) += len;
        }
        break;
    }

    return 0;
}

int parse_atoms(hymn_t *ctx,
                uint8_t **fp, uint32_t len, uint32_t level) {
    int err;
    uint8_t *eob = (*fp) + len;
    while ( (*fp) < eob ) {
        err = parse_atom(ctx, fp, len, level);
        if (err) return err;
    }

    return 0;
}

/* EOF */
