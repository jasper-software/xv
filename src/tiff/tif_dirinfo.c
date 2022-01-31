/* $Header: /usr/people/sam/tiff/libtiff/RCS/tif_dirinfo.c,v 1.29 1994/09/17 23:41:33 sam Exp $ */

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library.
 *
 * Known Directory Tag Support.
 */
#include "tiffiop.h"

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif

/*
 * NB: THIS ARRAY IS ASSUMED TO BE SORTED BY TAG.
 *     Also, if a tag can have both LONG and SHORT types
 *     then the LONG must be placed before the SHORT for
 *     writing to work properly.
 */
const TIFFFieldInfo tiffFieldInfo[] = {
    { TIFFTAG_SUBFILETYPE,	 1, 1, TIFF_LONG,	FIELD_SUBFILETYPE,
      TRUE,	FALSE,	"SubfileType" },
/* XXX SHORT for compatibility w/ old versions of the library */
    { TIFFTAG_SUBFILETYPE,	 1, 1, TIFF_SHORT,	FIELD_SUBFILETYPE,
      TRUE,	FALSE,	"SubfileType" },
    { TIFFTAG_OSUBFILETYPE,	 1, 1, TIFF_SHORT,	FIELD_SUBFILETYPE,
      TRUE,	FALSE,	"OldSubfileType" },
    { TIFFTAG_IMAGEWIDTH,	 1, 1, TIFF_LONG,	FIELD_IMAGEDIMENSIONS,
      FALSE,	FALSE,	"ImageWidth" },
    { TIFFTAG_IMAGEWIDTH,	 1, 1, TIFF_SHORT,	FIELD_IMAGEDIMENSIONS,
      FALSE,	FALSE,	"ImageWidth" },
    { TIFFTAG_IMAGELENGTH,	 1, 1, TIFF_LONG,	FIELD_IMAGEDIMENSIONS,
      TRUE,	FALSE,	"ImageLength" },
    { TIFFTAG_IMAGELENGTH,	 1, 1, TIFF_SHORT,	FIELD_IMAGEDIMENSIONS,
      TRUE,	FALSE,	"ImageLength" },
    { TIFFTAG_BITSPERSAMPLE,	-1,-1, TIFF_SHORT,	FIELD_BITSPERSAMPLE,
      FALSE,	FALSE,	"BitsPerSample" },
    { TIFFTAG_COMPRESSION,	-1, 1, TIFF_SHORT,	FIELD_COMPRESSION,
      FALSE,	FALSE,	"Compression" },
    { TIFFTAG_PHOTOMETRIC,	 1, 1, TIFF_SHORT,	FIELD_PHOTOMETRIC,
      FALSE,	FALSE,	"PhotometricInterpretation" },
    { TIFFTAG_THRESHHOLDING,	 1, 1, TIFF_SHORT,	FIELD_THRESHHOLDING,
      TRUE,	FALSE,	"Threshholding" },
    { TIFFTAG_CELLWIDTH,	 1, 1, TIFF_SHORT,	FIELD_IGNORE,
      TRUE,	FALSE,	"CellWidth" },
    { TIFFTAG_CELLLENGTH,	 1, 1, TIFF_SHORT,	FIELD_IGNORE,
      TRUE,	FALSE,	"CellLength" },
    { TIFFTAG_FILLORDER,	 1, 1, TIFF_SHORT,	FIELD_FILLORDER,
      FALSE,	FALSE,	"FillOrder" },
    { TIFFTAG_DOCUMENTNAME,	-1,-1, TIFF_ASCII,	FIELD_DOCUMENTNAME,
      TRUE,	FALSE,	"DocumentName" },
    { TIFFTAG_IMAGEDESCRIPTION,	-1,-1, TIFF_ASCII,	FIELD_IMAGEDESCRIPTION,
      TRUE,	FALSE,	"ImageDescription" },
    { TIFFTAG_MAKE,		-1,-1, TIFF_ASCII,	FIELD_MAKE,
      TRUE,	FALSE,	"Make" },
    { TIFFTAG_MODEL,		-1,-1, TIFF_ASCII,	FIELD_MODEL,
      TRUE,	FALSE,	"Model" },
    { TIFFTAG_STRIPOFFSETS,	-1,-1, TIFF_LONG,	FIELD_STRIPOFFSETS,
      FALSE,	FALSE,	"StripOffsets" },
    { TIFFTAG_STRIPOFFSETS,	-1,-1, TIFF_SHORT,	FIELD_STRIPOFFSETS,
      FALSE,	FALSE,	"StripOffsets" },
    { TIFFTAG_ORIENTATION,	 1, 1, TIFF_SHORT,	FIELD_ORIENTATION,
      FALSE,	FALSE,	"Orientation" },
    { TIFFTAG_SAMPLESPERPIXEL,	 1, 1, TIFF_SHORT,	FIELD_SAMPLESPERPIXEL,
      FALSE,	FALSE,	"SamplesPerPixel" },
    { TIFFTAG_ROWSPERSTRIP,	 1, 1, TIFF_LONG,	FIELD_ROWSPERSTRIP,
      FALSE,	FALSE,	"RowsPerStrip" },
    { TIFFTAG_ROWSPERSTRIP,	 1, 1, TIFF_SHORT,	FIELD_ROWSPERSTRIP,
      FALSE,	FALSE,	"RowsPerStrip" },
    { TIFFTAG_STRIPBYTECOUNTS,	-1,-1, TIFF_LONG,	FIELD_STRIPBYTECOUNTS,
      FALSE,	FALSE,	"StripByteCounts" },
    { TIFFTAG_STRIPBYTECOUNTS,	-1,-1, TIFF_SHORT,	FIELD_STRIPBYTECOUNTS,
      FALSE,	FALSE,	"StripByteCounts" },
    { TIFFTAG_MINSAMPLEVALUE,	-2,-1, TIFF_SHORT,	FIELD_MINSAMPLEVALUE,
      TRUE,	FALSE,	"MinSampleValue" },
    { TIFFTAG_MAXSAMPLEVALUE,	-2,-1, TIFF_SHORT,	FIELD_MAXSAMPLEVALUE,
      TRUE,	FALSE,	"MaxSampleValue" },
    { TIFFTAG_XRESOLUTION,	 1, 1, TIFF_RATIONAL,	FIELD_RESOLUTION,
      FALSE,	FALSE,	"XResolution" },
    { TIFFTAG_YRESOLUTION,	 1, 1, TIFF_RATIONAL,	FIELD_RESOLUTION,
      FALSE,	FALSE,	"YResolution" },
    { TIFFTAG_PLANARCONFIG,	 1, 1, TIFF_SHORT,	FIELD_PLANARCONFIG,
      FALSE,	FALSE,	"PlanarConfiguration" },
    { TIFFTAG_PAGENAME,		-1,-1, TIFF_ASCII,	FIELD_PAGENAME,
      TRUE,	FALSE,	"PageName" },
    { TIFFTAG_XPOSITION,	 1, 1, TIFF_RATIONAL,	FIELD_POSITION,
      TRUE,	FALSE,	"XPosition" },
    { TIFFTAG_YPOSITION,	 1, 1, TIFF_RATIONAL,	FIELD_POSITION,
      TRUE,	FALSE,	"YPosition" },
    { TIFFTAG_FREEOFFSETS,	-1,-1, TIFF_LONG,	FIELD_IGNORE,
      FALSE,	FALSE,	"FreeOffsets" },
    { TIFFTAG_FREEBYTECOUNTS,	-1,-1, TIFF_LONG,	FIELD_IGNORE,
      FALSE,	FALSE,	"FreeByteCounts" },
    { TIFFTAG_GRAYRESPONSEUNIT,	 1, 1, TIFF_SHORT,	FIELD_IGNORE,
      TRUE,	FALSE,	"GrayResponseUnit" },
    { TIFFTAG_GRAYRESPONSECURVE,-1,-1, TIFF_SHORT,	FIELD_IGNORE,
      TRUE,	FALSE,	"GrayResponseCurve" },
    { TIFFTAG_GROUP3OPTIONS,	 1, 1, TIFF_LONG,	FIELD_GROUP3OPTIONS,
      FALSE,	FALSE,	"Group3Options" },
    { TIFFTAG_GROUP4OPTIONS,	 1, 1, TIFF_LONG,	FIELD_GROUP4OPTIONS,
      FALSE,	FALSE,	"Group4Options" },
    { TIFFTAG_RESOLUTIONUNIT,	 1, 1, TIFF_SHORT,	FIELD_RESOLUTIONUNIT,
      FALSE,	FALSE,	"ResolutionUnit" },
    { TIFFTAG_PAGENUMBER,	 2, 2, TIFF_SHORT,	FIELD_PAGENUMBER,
      TRUE,	FALSE,	"PageNumber" },
    { TIFFTAG_COLORRESPONSEUNIT, 1, 1, TIFF_SHORT,	FIELD_IGNORE,
      TRUE,	FALSE,	"ColorResponseUnit" },
#ifdef COLORIMETRY_SUPPORT
    { TIFFTAG_TRANSFERFUNCTION,	-1,-1, TIFF_SHORT,	FIELD_TRANSFERFUNCTION,
      TRUE,	FALSE,	"TransferFunction" },
#endif
    { TIFFTAG_SOFTWARE,		-1,-1, TIFF_ASCII,	FIELD_SOFTWARE,
      TRUE,	FALSE,	"Software" },
    { TIFFTAG_DATETIME,		20,20, TIFF_ASCII,	FIELD_DATETIME,
      TRUE,	FALSE,	"DateTime" },
    { TIFFTAG_ARTIST,		-1,-1, TIFF_ASCII,	FIELD_ARTIST,
      TRUE,	FALSE,	"Artist" },
    { TIFFTAG_HOSTCOMPUTER,	-1,-1, TIFF_ASCII,	FIELD_HOSTCOMPUTER,
      TRUE,	FALSE,	"HostComputer" },
    { TIFFTAG_PREDICTOR,	 1, 1, TIFF_SHORT,	FIELD_PREDICTOR,
      FALSE,	FALSE,	"Predictor" },
#ifdef COLORIMETRY_SUPPORT
    { TIFFTAG_WHITEPOINT,	 2, 2, TIFF_RATIONAL,FIELD_WHITEPOINT,
      TRUE,	FALSE,	"WhitePoint" },
    { TIFFTAG_PRIMARYCHROMATICITIES,6,6,TIFF_RATIONAL,FIELD_PRIMARYCHROMAS,
      TRUE,	FALSE,	"PrimaryChromaticities" },
#endif
    { TIFFTAG_COLORMAP,		-1,-1, TIFF_SHORT,	FIELD_COLORMAP,
      TRUE,	FALSE,	"ColorMap" },
    { TIFFTAG_HALFTONEHINTS,	 2, 2, TIFF_SHORT,	FIELD_HALFTONEHINTS,
      TRUE,	FALSE,	"HalftoneHints" },
    { TIFFTAG_TILEWIDTH,	 1, 1, TIFF_LONG,	FIELD_TILEDIMENSIONS,
      FALSE,	FALSE,	"TileWidth" },
    { TIFFTAG_TILEWIDTH,	 1, 1, TIFF_SHORT,	FIELD_TILEDIMENSIONS,
      FALSE,	FALSE,	"TileWidth" },
    { TIFFTAG_TILELENGTH,	 1, 1, TIFF_LONG,	FIELD_TILEDIMENSIONS,
      FALSE,	FALSE,	"TileLength" },
    { TIFFTAG_TILELENGTH,	 1, 1, TIFF_SHORT,	FIELD_TILEDIMENSIONS,
      FALSE,	FALSE,	"TileLength" },
    { TIFFTAG_TILEOFFSETS,	-1, 1, TIFF_LONG,	FIELD_STRIPOFFSETS,
      FALSE,	FALSE,	"TileOffsets" },
    { TIFFTAG_TILEBYTECOUNTS,	-1, 1, TIFF_LONG,	FIELD_STRIPBYTECOUNTS,
      FALSE,	FALSE,	"TileByteCounts" },
    { TIFFTAG_TILEBYTECOUNTS,	-1, 1, TIFF_SHORT,	FIELD_STRIPBYTECOUNTS,
      FALSE,	FALSE,	"TileByteCounts" },
    { TIFFTAG_BADFAXLINES,	 1, 1, TIFF_LONG,	FIELD_BADFAXLINES,
      TRUE,	FALSE,	"BadFaxLines" },
    { TIFFTAG_BADFAXLINES,	 1, 1, TIFF_SHORT,	FIELD_BADFAXLINES,
      TRUE,	FALSE,	"BadFaxLines" },
    { TIFFTAG_CLEANFAXDATA,	 1, 1, TIFF_SHORT,	FIELD_CLEANFAXDATA,
      TRUE,	FALSE,	"CleanFaxData" },
    { TIFFTAG_CONSECUTIVEBADFAXLINES,1,1, TIFF_LONG,FIELD_BADFAXRUN,
      TRUE,	FALSE,	"ConsecutiveBadFaxLines" },
    { TIFFTAG_CONSECUTIVEBADFAXLINES,1,1, TIFF_SHORT,FIELD_BADFAXRUN,
      TRUE,	FALSE,	"ConsecutiveBadFaxLines" },
#ifdef TIFFTAG_SUBIFD
    { TIFFTAG_SUBIFD,		-1,-1, TIFF_LONG,	FIELD_SUBIFD,
      TRUE,	TRUE,	"SubIFD" },
#endif
#ifdef CMYK_SUPPORT		/* 6.0 CMYK tags */
    { TIFFTAG_INKSET,		 1, 1, TIFF_SHORT,	FIELD_INKSET,
      FALSE,	FALSE,	"InkSet" },
    { TIFFTAG_INKNAMES,		-1,-1, TIFF_ASCII,	FIELD_INKNAMES,
      TRUE,	FALSE,	"InkNames" },
    { TIFFTAG_DOTRANGE,		 2, 2, TIFF_SHORT,	FIELD_DOTRANGE,
      FALSE,	FALSE,	"DotRange" },
    { TIFFTAG_DOTRANGE,		 2, 2, TIFF_BYTE,	FIELD_DOTRANGE,
      FALSE,	FALSE,	"DotRange" },
    { TIFFTAG_TARGETPRINTER,	-1,-1, TIFF_ASCII,	FIELD_TARGETPRINTER,
      TRUE,	FALSE,	"TargetPrinter" },
#endif
    { TIFFTAG_EXTRASAMPLES,	-1,-1, TIFF_SHORT,	FIELD_EXTRASAMPLES,
      FALSE,	FALSE,	"ExtraSamples" },
/* XXX for bogus Adobe Photoshop v2.5 files */
    { TIFFTAG_EXTRASAMPLES,	-1,-1, TIFF_BYTE,	FIELD_EXTRASAMPLES,
      FALSE,	FALSE,	"ExtraSamples" },
    { TIFFTAG_SAMPLEFORMAT,	-1,-1, TIFF_SHORT,	FIELD_SAMPLEFORMAT,
      FALSE,	FALSE,	"SampleFormat" },
#ifdef notdef
    { TIFFTAG_SMINSAMPLEVALUE,	-2,-1, TIFF_ANY,	FIELD_SMINSAMPLEVALUE,
      TRUE,	FALSE,	"SMinSampleValue" },
    { TIFFTAG_SMAXSAMPLEVALUE,	-2,-1, TIFF_ANY,	FIELD_SMAXSAMPLEVALUE,
      TRUE,	FALSE,	"SMaxSampleValue" },
#endif
#ifdef JPEG_SUPPORT		/* 6.0 JPEG tags */
    { TIFFTAG_JPEGPROC,		 1, 1, TIFF_SHORT,	FIELD_JPEGPROC,
      FALSE,	FALSE,	"JPEGProc" },
    { TIFFTAG_JPEGIFOFFSET,	 1, 1, TIFF_LONG,	FIELD_IGNORE,
      FALSE,	FALSE,	"JPEGInterchangeFormat" },
    { TIFFTAG_JPEGIFBYTECOUNT,	 1, 1, TIFF_LONG,	FIELD_IGNORE,
      FALSE,	FALSE,	"JPEGInterchangeFormatLength" },
    { TIFFTAG_JPEGRESTARTINTERVAL,1,1, TIFF_SHORT,FIELD_JPEGRESTARTINTERVAL,
      FALSE,	FALSE,	"JPEGRestartInterval" },
    { TIFFTAG_JPEGQTABLES,	-2,-1, TIFF_LONG,	FIELD_JPEGQTABLES,
      FALSE,	FALSE,	"JPEGQTables" },
    { TIFFTAG_JPEGDCTABLES,	-2,-1, TIFF_LONG,	FIELD_JPEGDCTABLES,
      FALSE,	FALSE,	"JPEGDCTables" },
    { TIFFTAG_JPEGACTABLES,	-2,-1, TIFF_LONG,	FIELD_JPEGACTABLES,
      FALSE,	FALSE,	"JPEGACTables" },
#endif
#ifdef YCBCR_SUPPORT		/* 6.0 YCbCr tags */
    { TIFFTAG_YCBCRCOEFFICIENTS, 3, 3, TIFF_RATIONAL,	FIELD_YCBCRCOEFFICIENTS,
      FALSE,	FALSE,	"YCbCrCoefficients" },
    { TIFFTAG_YCBCRSUBSAMPLING,	 2, 2, TIFF_SHORT,	FIELD_YCBCRSUBSAMPLING,
      FALSE,	FALSE,	"YCbCrSubsampling" },
    { TIFFTAG_YCBCRPOSITIONING,	 1, 1, TIFF_SHORT,	FIELD_YCBCRPOSITIONING,
      FALSE,	FALSE,	"YCbCrPositioning" },
#endif
#ifdef COLORIMETRY_SUPPORT
    { TIFFTAG_REFERENCEBLACKWHITE,6,6,TIFF_RATIONAL,	FIELD_REFBLACKWHITE,
      FALSE,	FALSE,	"ReferenceBlackWhite" },
/* XXX temporarily accept LONG for backwards compatibility */
    { TIFFTAG_REFERENCEBLACKWHITE,6,6,TIFF_LONG,	FIELD_REFBLACKWHITE,
      FALSE,	FALSE,	"ReferenceBlackWhite" },
#endif
/* begin SGI tags */
    { TIFFTAG_MATTEING,		 1, 1, TIFF_SHORT,	FIELD_EXTRASAMPLES,
      FALSE,	FALSE,	"Matteing" },
    { TIFFTAG_DATATYPE,		-2,-1, TIFF_SHORT,	FIELD_SAMPLEFORMAT,
      FALSE,	FALSE,	"DataType" },
    { TIFFTAG_IMAGEDEPTH,	 1, 1, TIFF_LONG,	FIELD_IMAGEDEPTH,
      FALSE,	FALSE,	"ImageDepth" },
    { TIFFTAG_IMAGEDEPTH,	 1, 1, TIFF_SHORT,	FIELD_IMAGEDEPTH,
      FALSE,	FALSE,	"ImageDepth" },
    { TIFFTAG_TILEDEPTH,	 1, 1, TIFF_LONG,	FIELD_TILEDEPTH,
      FALSE,	FALSE,	"TileDepth" },
    { TIFFTAG_TILEDEPTH,	 1, 1, TIFF_SHORT,	FIELD_TILEDEPTH,
      FALSE,	FALSE,	"TileDepth" },
/* end SGI tags */
    { 0 }
};

const int tiffDataWidth[] = {
    1,	/* nothing */
    1,	/* TIFF_BYTE */
    1,	/* TIFF_ASCII */
    2,	/* TIFF_SHORT */
    4,	/* TIFF_LONG */
    8,	/* TIFF_RATIONAL */
    1,	/* TIFF_SBYTE */
    1,	/* TIFF_UNDEFINED */
    2,	/* TIFF_SSHORT */
    4,	/* TIFF_SLONG */
    8,	/* TIFF_SRATIONAL */
    4,	/* TIFF_FLOAT */
    8,	/* TIFF_DOUBLE */
};

const TIFFFieldInfo *
TIFFFindFieldInfo(ttag_t tag, TIFFDataType dt)
{
	static const TIFFFieldInfo *last = NULL;
	register const TIFFFieldInfo *fip;

	if (last && last->field_tag == tag &&
	    (dt == TIFF_ANY || dt == last->field_type))
		return (last);
	/* NB: if table gets big, use sorted search (e.g. binary search) */
	for (fip = tiffFieldInfo; fip->field_tag; fip++)
		if (fip->field_tag == tag &&
		    (dt == TIFF_ANY || fip->field_type == dt))
			return (last = fip);
	return ((const TIFFFieldInfo *)0);
}

#include <assert.h>
#include <stdio.h>

const TIFFFieldInfo *
TIFFFieldWithTag(ttag_t tag)
{
	const TIFFFieldInfo *fip = TIFFFindFieldInfo(tag, TIFF_ANY);
	if (!fip) {
		TIFFError("TIFFFieldWithTag",
		    "Internal error, unknown tag 0x%x", (u_int) tag);
		assert(fip != NULL);
		/*NOTREACHED*/
	}
	return (fip);
}
