/*
 * xvwebp.c - load and write routines for 'WEBP' format pictures
 *
 * callable functions
 *
 *    WriteWEBP(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle)
 *    VersionInfoWEBP()
 *    LoadWebP(fname, pinfo)
 *
 * Uses Google's WebP library
 *
 * https://developers.google.com/speed/webp/docs/api
 *
 * Installed on ubuntu with: apt-get install libwebp-dev
 */

/*
 * Assembled by Stephen Conley - @tanabi on GitHub
 *
 * I blatantly re-used code from xvpng.c -- credit and ownership to those
 * original authors (see that file for details).  My contribution is
 * provided freely.
 *
 */

#include "xv.h"

#ifdef HAVE_WEBP

#include "webp/decode.h"
#include "webp/encode.h"

/* Used for xv to hand off save info to our 'library' */
static char *filename;
static int   colorType;

/*
 * This is how xv hands off save parameters to us.
 *
 * Takes a file name and the color type.
 */
void WEBPSaveParams(fname, col)
     char *fname;
     int col;
{
  filename = fname;
  colorType = col;
}

/*
 * Method to handle writing ('encoding') a WEBP file.
 *
 * So there's a lot of potential options here.  At first, I was going to
 * use the dialog box approach used by PNG, JPEG, TIFF and some others to
 * offer these options.
 *
 * However, the amount of work involved there -- and believe me, I know,
 * I went pretty far down the rabbit hole -- is a magnitude more than
 * doing it the easy way.  Not only is the XV UI side much more complex,
 * but to expose the options is way more complex on the libwebp side as
 * well.  It goes from a single well-documented function call, to a bunch
 * of not well documented function calls and a need to write callbacks and
 * such.
 *
 * I came to realize that I, personally, am unlikely to ever save a webp
 * file and that I just don't care enough to put that level of work into
 * it.
 *
 * I'm sorry! :)
 *
 * Thus, this just saves lossless format with default settings.  I figure
 * that is the most useful webp to save.  We could make a seperate
 * webp (lossless) vs. webp (lossy) save list items and treat them like
 * 2 different formats to avoid the dialog box.  Or if someone comes along
 * later and cares to implement the dialog box, more power to you.
 *
 * xvpng.c has a very good example of how to do it, and you'll have to
 * remove this WriteWEBP call from xvdir.c and grep around for HAVE_PNG
 * in the rest of the code -- particularly xv.c, xvdir.c and xvevent.c -- to do
 * it.  It's not super hard, but it'll take this from an afternoon project
 * to a "few days" project if I do that, and that's more than I can do
 * right now.
 *
 * Sorry for writing a diary journal in comments!  But hopefully this is
 * helpful for someone else who may come along and care about this problem
 * more than I do.
 *
 * - @tanabi on GITHUB
 */
int
WriteWEBP(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle)
     FILE *fp;
     byte *pic;
     int   ptype,w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
{
  uint8_t*  webp_in = NULL;
  uint8_t*  webp_out = NULL;
  byte      mono_byte = 0;
  int       free_me = 0; /* if 1, we need to free webp_in */
  int       linesize = w*3;
  int       i = 0;
  int       final_size = 0;

  /*
   * First, we have to prepare our data in such a fashion that we can
   * write it out.  webp only supports 24 or 32 bit formats, so any
   * other formats will have to be massaged into 24/32 bit.
   *
   * Greyscale is a bit of a pill.  WEBP doesn't have a 'greyscale' format
   * that just uses intensities, so we'll have to make our own.
   */
  if (ptype == PIC24) {
    if (colorstyle == F_GREYSCALE) {
        // Convert the pic to greyscale
        webp_in = (uint8_t*)malloc(linesize*h);
        free_me = 1;

        for (i = 0; i < linesize*h; i+=3) {
          mono_byte = MONO(pic[i], pic[i+1], pic[i+2]);

          webp_in[i] = webp_in[i+1] = webp_in[i+2] = mono_byte;
        }
    } else {
      /* nothing to do, we're already good to go */
      webp_in = (uint8_t*)pic;
    }
  } else {
    webp_in = (uint8_t*)malloc(linesize*h);
    free_me = 1;

    for (i = 0; i < (w*h); i++) {
      if (colorstyle == F_GREYSCALE) {
        mono_byte = MONO(rmap[pic[i]], gmap[pic[i]], bmap[pic[i]]);
        webp_in[i*3] = mono_byte;
        webp_in[(i*3)+1] = mono_byte;
        webp_in[(i*3)+2] = mono_byte;
      } else {
        webp_in[i*3] = rmap[pic[i]];
        webp_in[(i*3)+1] = gmap[pic[i]];
        webp_in[(i*3)+2] = bmap[pic[i]];
      }
    }
  }

  /* Try encoding it */
  final_size = WebPEncodeLosslessRGB(webp_in, w, h, linesize, &webp_out);

  if (!final_size) {
    if (free_me) {
      free(webp_in);
    }

    /* I'm not sure if this is necessary */
    if (webp_out) {
      WebPFree(webp_out);
    }

    return -1;
  }

  /* Write it out */
  fwrite(webp_out, final_size, 1, fp);

  /* Clean up */
  if (free_me) {
    free(webp_in);
  }

  WebPFree(webp_out);

  if (ferror(fp)) {
    return -1;
  }

  return 0;
}

/*
 * Output version information to stderr
 */
void
VersionInfoWEBP()
{
  int decoder_version;
  int encoder_version;

  decoder_version = WebPGetDecoderVersion();
  encoder_version = WebPGetEncoderVersion();

  fprintf(stderr, "   Compiled with libwebp; decoder version: %hhu.%hhu.%hhu\n",
    (unsigned char)decoder_version,
    ((unsigned char*)&decoder_version)[1],
    ((unsigned char*)&decoder_version)[2]
  );
  fprintf(stderr, "                          encoder version: %hhu.%hhu.%hhu\n",
    (unsigned char)encoder_version,
    ((unsigned char*)&encoder_version)[1],
    ((unsigned char*)&encoder_version)[2]
  );
}

/*
 * Loader for WebP Images.
 *
 * Takes file name and PICINFO structure.
 *
 * Returns 1 on success, 0 on failure
 *
 */
int LoadWebP(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
{
  FILE                  *fp;
  int                   filesize;
  int                   bufsize;
  int                   linesize;
  uint8_t               *raw_data;
  WebPBitstreamFeatures features;
  VP8StatusCode         status;

  /* open the file */
  fp = xv_fopen(fname,"r");
  if (!fp) {
    SetISTR(ISTR_WARNING,"%s:  can't open file", fname);
    return 0;
  }

  /* find the size of the file */
  fseek(fp, 0L, SEEK_END);
  filesize = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  /* Try to make our buffer */
  raw_data = (uint8_t*)malloc(filesize);

  if (!raw_data) {
    FatalError("malloc failure in LoadWebP");
    fclose(fp);
    return 0;
  }

  /* Load it up */
  if (!fread(raw_data, filesize, 1, fp)) {
    free(raw_data);
    fclose(fp);
    SetISTR(ISTR_WARNING, "%s:  couldn't read entire file", fname);
    return 0;
  }

  /* Shouldn't need the file anymore */
  fclose(fp);

  /* Get basic information about the image */
  status = WebPGetFeatures(raw_data, filesize, &features);

  if (status != VP8_STATUS_OK) {
    free(raw_data);
    SetISTR(ISTR_WARNING, "failed to load WebP Features");
    return 0;
  }

  /* Null out some default stuff */
  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;

  /* Set sizing information */
  pinfo->w = pinfo->normw = features.width;
  pinfo->h = pinfo->normh = features.height;

  /* Basic sanity checking */
  if (pinfo->w <= 0 || pinfo->h <= 0) {
    free(raw_data);
    SetISTR(ISTR_WARNING, "%s:  image dimensions out of range (%dx%d)",
      fname, pinfo->w, pinfo->h);
    return 0;
  }

  /* Set file type if we've gotten this far, along with other type
   * informations
   */
  pinfo->frmType = F_WEBP;
  pinfo->colType = F_FULLCOLOR;
  pinfo->type = PIC24;

  /* Compile fullInfo - note that this is a 128 byte buffer */
  strcpy(pinfo->fullInfo, "WebP");

  if (features.has_alpha) {
    strcat(pinfo->fullInfo, ", with alpha channel");
  }

  if (features.has_animation) {
    strcat(pinfo->fullInfo, ", animated");
  }

  switch (features.format) {
    case 1:
      strcat(pinfo->fullInfo, ", lossy");
      break;

    case 2:
      strcat(pinfo->fullInfo, ", lossless");
      break;

    default:
      strcat(pinfo->fullInfo, ", mixed loss/lossless");
  }

  /* Set short info (128 byte buffer as well) */
  sprintf(pinfo->shrtInfo, "%dx%d WEBP", pinfo->w, pinfo->h);

  /* Calculate datasize and load */
  linesize = 3 * pinfo->w;
  bufsize = linesize * pinfo->h;

  /* Sanity check */
  if (((linesize / 3) < pinfo->w) || ((bufsize / linesize) < pinfo->h)) {
    SetISTR(ISTR_WARNING, "image dimensions too large (%dx%d)",
      pinfo->w, pinfo->h);
    free(raw_data);
    return 0;
  }

  /*
   * Getting data seems to fail on animations -- probably because I"m
   * not allocating enough memory, or maybe WebPDecodeRGBInto doesn't
   * support it.
   *
   * xv doesn't really support animations anyway I don't think, so we
   * will give a bummer message.
   */
  if (features.has_animation) {
    free(raw_data);
    SetISTR(ISTR_WARNING, "xv doesn't support animations (yet)");
    return 0;
  }

  pinfo->pic = (byte*)malloc(bufsize);

  if (!pinfo->pic) {
    free(raw_data);
    FatalError("malloc failure in LoadWebP");
    return 0;
  }

  /*
   * Get data.  Note that there's an RGBA version of this call if we want
   * to get a fourth (32 bit) alpha channel byte, but I don't think xv
   * supports that (at least not in a modern and smart way) so I'm just
   * loading RGB right now.
   */
  if (!WebPDecodeRGBInto(raw_data, filesize, (uint8_t*)pinfo->pic,
                         bufsize, linesize)) {
    free(raw_data);
    free(pinfo->pic);
    pinfo->pic = NULL;
    SetISTR(ISTR_WARNING, "failed to load WebP");
    return 0;
  }

  free(raw_data);
  return 1;
}


#endif