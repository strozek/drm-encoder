/*****************************************************************************
* $Id: hymn.c,v 1.9 2004/07/24 07:32:09 playfair Exp $
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

/* functions */
int hymn_init(hymn_t *ctx, const char *infilename) {
    FILE *infile;

    /* drms info */
    ctx->drms_user = 0;
    ctx->drms_key = 0;
    ctx->drms_iviv = 0;
    ctx->drms_iviv_size = 0;
    ctx->drms_name = 0;
    ctx->drms_name_size = 0;
    ctx->drms_priv = 0;
    ctx->drms_priv_size = 0;
    ctx->drms_trak_idx = 0;

    /* mp4 / trak info */
    ctx->sample_chunk_table = 0;
    ctx->chunk_table_size = 0;
    ctx->sample_table_sizes = 0;
    ctx->num_sample_sizes = 0;
    ctx->sample_table_offsets_trak1 = 0;
    ctx->num_sample_offsets_trak1 = 0;
    ctx->sample_table_offsets_trak2 = 0;
    ctx->num_sample_offsets_trak2 = 0;
    ctx->sinf_size = 0;
    ctx->trak_idx = 0;

    /* internal context */
    ctx->writing = false;
    ctx->write_off = false;

    /* infile */
    infile = fopen(infilename, "r");
    if (infile) {
        struct stat instat;
        int statok = stat(infilename, &instat);
        if (!statok) {
            ctx->inbuf_size = instat.st_size;
            ctx->inbuf = malloc(ctx->inbuf_size);
            if (ctx->inbuf) {
                int bytes_read = fread(ctx->inbuf, 1, ctx->inbuf_size, infile);
                if (bytes_read != ctx->inbuf_size) {
                    return -1;
                }
                fclose(infile);
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}

int hymn_init_write(hymn_t *ctx, const char *outfile) {
    /* mp4 / trak info */
    ctx->trak_idx = 0;

    /* outfile */
    ctx->outfile = fopen(outfile, "w+");
    if (!ctx->outfile) {
        return -1;
    }

    return 0;
}

void hymn_free(hymn_t *ctx) {
    if (ctx->inbuf) free(ctx->inbuf);
    if (ctx->drms_iviv) free(ctx->drms_iviv);
    if (ctx->drms_name) free(ctx->drms_name);
    if (ctx->drms_priv) free(ctx->drms_priv);
    if (ctx->sample_chunk_table) {
        uint32_t i;
        for (i=0; i<ctx->chunk_table_size; ++i) {
            free(ctx->sample_chunk_table[i]);
        }
        free(ctx->sample_chunk_table);
    }
    if (ctx->sample_table_sizes) free(ctx->sample_table_sizes);
    if (ctx->sample_table_offsets_trak1)
        free(ctx->sample_table_offsets_trak1);
    if (ctx->sample_table_offsets_trak2)
        free(ctx->sample_table_offsets_trak2);
    if (ctx->outfile) fclose(ctx->outfile);
}

void hymn_failed(hymn_t *ctx) {
    if (ctx->outfile) {
        /* if the outfile was created, delete it */
        unlink(ctx->outfile);
    }
    hymn_free(ctx);
}

int decrypt_keys(hymn_t *ctx) {
    int err;
    char *homedir = "/Users/strozek/";
    uint8_t user_key[16];
    uint8_t name_iviv_hash[16];
    md5_context md5;

    if ( get_user_key(homedir, ctx->drms_user, ctx->drms_key,
                      (uint32_t*)user_key) ) {
        return -1;
    }
    /* drms code is assuming the opposite endian-ness */
    REVERSE((uint32_t*)user_key, 4);

    /* hash the name and iviv */
    md5_starts(&md5);
    md5_update( &md5, ctx->drms_name, strlen(ctx->drms_name) );
    md5_update(&md5, ctx->drms_iviv, ctx->drms_iviv_size);
    md5_finish(&md5, name_iviv_hash);

    decrypt(ctx->drms_priv, ctx->drms_priv, ctx->drms_priv_size,
            user_key, name_iviv_hash);

    if (ctx->drms_priv[0] != 'i' ||
        ctx->drms_priv[1] != 't' ||
        ctx->drms_priv[2] != 'u' ||
        ctx->drms_priv[3] != 'n') {
        return -1;
    }

    return 0;
}

int convert(char *infile, char *outfile) {
    int ret;

    hymn_t ctx;
    if ( ( ret = hymn_init(&ctx, infile) ) ) {
        hymn_failed(&ctx);
    } else {
        /* We will read through our input file two times.  The first
         * time we are parsing.
         */
        uint8_t *fp = ctx.inbuf;
        if ( ( ret = parse_atoms(&ctx, &fp, ctx.inbuf_size, 0) ) ) {
            hymn_failed(&ctx);
            return ret;
        }

        /* After the first pass, we need to do all of the decryption
         * of the keys.
         */
        if ( ( ret = decrypt_keys(&ctx) ) ) {
            hymn_failed(&ctx);
            return ret;
        }

        /* The second time we are writing out our output file after
         * having collected all of the necessary information from the
         * input file.
         */
        if ( ( ret = hymn_init_write(&ctx, outfile) ) ) {
            hymn_failed(&ctx);
            return ret;
        } else {
            ctx.writing = true;
            uint8_t *fp = ctx.inbuf;
            if ( ( ret =
                   parse_atoms(&ctx, &fp, ctx.inbuf_size, 0) ) ) {
                hymn_failed(&ctx);
                return ret;
            }
        }
    }

    hymn_free(&ctx);
    return 0;
}

int main(int argc, char *argv[]) {
    convert(argv[1], argv[2]);
    return 0;
}

/* EOF */
