/*
 * xvorient.c - handle the orientation flag in images (probably from EXIF)
 */

#include "copyright.h"
#include "xv.h"

#ifdef HAVE_EXIF
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
#endif /* HAVE_EXIF */

/**************************************************************/
/**************************************************************/

unsigned int get_exif_orientation(byte *buf, unsigned int bufsize)
{
#ifndef HAVE_EXIF
	return ORIENT_NONE;
#else
	ExifData *exif_data;
	ExifEntry *exif_entry;
	ExifByteOrder exif_byte_order;

	exif_data = exif_data_new_from_data(buf, bufsize);
	if (exif_data == NULL)
	{
		return ORIENT_NONE;
	}

	exif_entry = exif_data_get_entry(exif_data, EXIF_TAG_ORIENTATION);
	if (exif_entry == NULL)
	{
		return ORIENT_NONE;
	}

	exif_byte_order = exif_data_get_byte_order(exif_data);

	/* If the EXIF IFD is as expected, get the orientation from it */
	if (
			exif_entry->components == 1 &&
			exif_entry->size == 2 &&
			exif_entry->format == EXIF_FORMAT_SHORT)
	{
		return exif_get_short(exif_entry->data, exif_byte_order);
	}

	return ORIENT_NONE;
#endif
}

/**************************************************************/
/**************************************************************/

void reorient_image(PICINFO *pinfo)
{
	byte *tmppic;
	byte *orientpic;
	int dst_row, dst_col, dst_row_width;
	int bperpix, pixel_bytes, swap_xy, h, w;

	if (pinfo->orientation == ORIENT_NONE || pinfo->orientation == ORIENT_NORMAL)
	{
		/*** We don't need to do anything - the image is fine as it is ***/
		return;
	}

	switch(pinfo->type)
	{
		case PIC8:
			bperpix = 1;
			break;
		case PIC24:
			bperpix = 3;
			break;

		/* We don't know how many bytes we have per pixel,
		** so just give up here and leave the image as it is.
		**/

		default:
			return;
	}

	/* If we need to transform the image, allocate a new image and populate
	** it with the correct pixel values. Then swap the two images.
	*/

	h = pinfo->h;
	w = pinfo->w;

	pixel_bytes = h * w * bperpix;

	if ((orientpic = (byte *)malloc((size_t) pixel_bytes)) == NULL)
	{
		FatalError("unable to reorient image - out of memory");
	}

	if (pinfo->orientation == ORIENT_VFLIP)
	{
		/* We can copy entire lines for VFLIP images, which will
		** be quicker than doing it pixel by pixel, so special
		** case this one.
		*/
		for (int row=0; row < h; row++)
		{
			memcpy(&orientpic[(h-row-1) * w * bperpix],
					&pinfo->pic[row * w * bperpix],
					w * bperpix);
		}
	} else
	{
		int src_offset, dst_offset;

		for (int row=0; row < h; row++)
		{
			for (int col=0; col < w; col++)
			{
				switch(pinfo->orientation)
				{
					case ORIENT_ROT90:
						dst_col = h-row-1;
						dst_row = col;
						swap_xy = 1;
						break;
					case ORIENT_ROT180:
						dst_col = w-col-1;
						dst_row = h-row-1;
						break;
					case ORIENT_ROT270:
						dst_col = row;
						dst_row = w-col-1;
						swap_xy = 1;
						break;
					case ORIENT_HFLIP:
						dst_col = w-col-1;
						dst_row = row;
						break;
					case ORIENT_TRANSPOSE:
						dst_col = row;
						dst_row = col;
						swap_xy = 1;
						break;
					case ORIENT_TRANSVERSE:
						dst_col = h-row-1;
						dst_row = w-col-1;
						swap_xy = 1;
						break;
					default:
						dst_offset = src_offset;
						break;
				}

				dst_row_width = swap_xy ? h : w;

				src_offset = (row * w) + col;
				dst_offset = (dst_row * dst_row_width) + dst_col;

				memcpy(&orientpic[dst_offset * bperpix],
					&pinfo->pic[src_offset * bperpix],
					bperpix);
			}
		}
	}

	/* Swap the images - make the reoriented image the
	** actual image and free the memory from the old image.
	*/
	tmppic = pinfo->pic;
	pinfo->pic = orientpic;
	free(tmppic);

	/* If we rotated the image in any way,
	** swap the w & h values in the pinfo.
	*/
	if (swap_xy)
	{
		pinfo->w = h;
		pinfo->h = w;
	}
}
