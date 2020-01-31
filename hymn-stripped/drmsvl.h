#ifndef __DRMSVL_H__
#define __DRMSVL_H__
/*****************************************************************************
* $Id: drmsvl.h,v 1.1 2004/06/16 22:09:14 playfair Exp $
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
*n Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111,
* USA.
*****************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#define VLC_TRUE true
#define VLC_FALSE false
#define vlc_bool_t bool

#include "hymn_private.h"

#include "endianutils.h"
#define U32_AT(p) uint32_from_bytes_be(p)
#define U64_AT(p) uint64_from_bytes_be(p)
#define GetDWLE(p) uint32_from_bytes_le(p)
#define __MIN(a, b)  (((a) < (b)) ? (a) : (b))

#include "atoms.h"
#define FOURCC_user ATOM_USER
#define FOURCC_key ATOM_KEY
#define FOURCC_iviv ATOM_IVIV
#define FOURCC_name ATOM_NAME
#define FOURCC_priv ATOM_PRIV

#endif /* __DRMSVL_H__ */
