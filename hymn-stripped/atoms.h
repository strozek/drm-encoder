#ifndef __ATOMS_H__
#define __ATOMS_H__
/*****************************************************************************
* $Id: atoms.h,v 1.3 2004/07/22 22:16:48 playfair Exp $
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

/* container atoms */
#define ATOM_ALB  UINT32_FROM_BYTES(0xa9,'a','l','b')
#define ATOM_ART  UINT32_FROM_BYTES(0xa9,'A','R','T')
#define ATOM_CLIP UINT32_FROM_BYTES('c','l','i','p')
#define ATOM_CMT  UINT32_FROM_BYTES(0xa9,'c','m','t')
#define ATOM_COVR UINT32_FROM_BYTES('c','o','v','r')
#define ATOM_CPIL UINT32_FROM_BYTES('c','p','i','l')
#define ATOM_DAY  UINT32_FROM_BYTES(0xa9,'d','a','y')
#define ATOM_DINF UINT32_FROM_BYTES('d','i','n','f')
#define ATOM_DISK UINT32_FROM_BYTES('d','i','s','k')
#define ATOM_DRMS UINT32_FROM_BYTES('d','r','m','s')
#define ATOM_EDTS UINT32_FROM_BYTES('e','d','t','s')
#define ATOM_ILST UINT32_FROM_BYTES('i','l','s','t')
#define ATOM_MATT UINT32_FROM_BYTES('m','a','t','t')
#define ATOM_MDIA UINT32_FROM_BYTES('m','d','i','a')
#define ATOM_MINF UINT32_FROM_BYTES('m','i','n','f')
#define ATOM_MOOV UINT32_FROM_BYTES('m','o','o','v')
#define ATOM_NAM  UINT32_FROM_BYTES(0xa9,'n','a','m')
#define ATOM_SCHI UINT32_FROM_BYTES('s','c','h','i')
#define ATOM_SINF UINT32_FROM_BYTES('s','i','n','f')
#define ATOM_STBL UINT32_FROM_BYTES('s','t','b','l')
#define ATOM_STSD UINT32_FROM_BYTES('s','t','s','d')
#define ATOM_TMPO UINT32_FROM_BYTES('t','m','p','o')
#define ATOM_TOO  UINT32_FROM_BYTES(0xa9,'t','o','o')
#define ATOM_TRAK UINT32_FROM_BYTES('t','r','a','k')
#define ATOM_TRKN UINT32_FROM_BYTES('t','r','k','n')
#define ATOM_UDTA UINT32_FROM_BYTES('u','d','t','a')
#define ATOM_WRT  UINT32_FROM_BYTES(0xa9,'w','r','t')
#define ATOM_META UINT32_FROM_BYTES('m','e','t','a')

/* standalone atoms */
#define ATOM_CTTS UINT32_FROM_BYTES('c','t','t','s')
#define ATOM_DATA UINT32_FROM_BYTES('d','a','t','a')
#define ATOM_DRMS UINT32_FROM_BYTES('d','r','m','s')
#define ATOM_ESDS UINT32_FROM_BYTES('e','s','d','s')
#define ATOM_FREE UINT32_FROM_BYTES('f','r','e','e')
#define ATOM_FRMA UINT32_FROM_BYTES('f','r','m','a')
#define ATOM_FTYP UINT32_FROM_BYTES('f','t','y','p')
#define ATOM_GEN  UINT32_FROM_BYTES(0xa9,'g','e','n')
#define ATOM_GNRE UINT32_FROM_BYTES('g','n','r','e')
#define ATOM_HMHD UINT32_FROM_BYTES('h','m','h','d')
#define ATOM_IVIV UINT32_FROM_BYTES('i','v','i','v')
#define ATOM_KEY  UINT32_FROM_BYTES('k','e','y',' ')
#define ATOM_MDAT UINT32_FROM_BYTES('m','d','a','t')
#define ATOM_MDHD UINT32_FROM_BYTES('m','d','h','d')
#define ATOM_MP4A UINT32_FROM_BYTES('m','p','4','a')
#define ATOM_MP4S UINT32_FROM_BYTES('m','p','4','s')
#define ATOM_MP4V UINT32_FROM_BYTES('m','p','4','v')
#define ATOM_MVHD UINT32_FROM_BYTES('m','v','h','d')
#define ATOM_NAME UINT32_FROM_BYTES('n','a','m','e')
#define ATOM_PRIV UINT32_FROM_BYTES('p','r','i','v')
#define ATOM_SINF UINT32_FROM_BYTES('s','i','n','f')
#define ATOM_SKIP UINT32_FROM_BYTES('s','k','i','p')
#define ATOM_SMHD UINT32_FROM_BYTES('s','m','h','d')
#define ATOM_STBL UINT32_FROM_BYTES('s','t','b','l')
#define ATOM_STCO UINT32_FROM_BYTES('s','t','c','o')
#define ATOM_STSC UINT32_FROM_BYTES('s','t','s','c')
#define ATOM_STSZ UINT32_FROM_BYTES('s','t','s','z')
#define ATOM_STTS UINT32_FROM_BYTES('s','t','t','s')
#define ATOM_STZ2 UINT32_FROM_BYTES('s','t','z','2')
#define ATOM_TKHD UINT32_FROM_BYTES('t','k','h','d')
#define ATOM_TREF UINT32_FROM_BYTES('t','r','e','f')
#define ATOM_USER UINT32_FROM_BYTES('u','s','e','r')
#define ATOM_VMHD UINT32_FROM_BYTES('v','m','h','d')
#define ATOM_APID UINT32_FROM_BYTES('a','p','I','D')
#define ATOM_GEID UINT32_FROM_BYTES('g','e','I','D')
#define ATOM_WIDE UINT32_FROM_BYTES('w','i','d','e')


/* interface */
int parse_atoms(hymn_t *ctx,
                uint8_t **fp, uint32_t len, uint32_t level);

#endif /* __ATOMS_H__ */
