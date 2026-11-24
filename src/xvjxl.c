#include "xv.h"
#include "copyright.h"

#ifdef HAVE_JXL
//syntax error

#include "jxl/decode.h"
#include "jxl/encode.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/cms.h"
#include <stdbool.h>

/*** Stuff for JXL Dialog box ***/
#define JWIDE 400
#define JHIGH 200
#define J_NBUTTS 2
#define J_BOK    0
#define J_BCANC  1
#define BUTTH    24

/*** local variables ***/
static char *filename;
static int   colorType;

static const char *fbasename;

static DIAL  qDial, smDial;
static BUTT  jbut[J_NBUTTS];


/***************************************************/
void CreateJXLW()
{
  jxlW = CreateWindow("xv jxl","XVjxl",NULL,JWIDE,JHIGH,infofg,infobg,FALSE);
  if (!jxlW) FatalError("can't create jxl window!");

  XSelectInput(theDisp, jxlW, ExposureMask | ButtonPressMask | KeyPressMask);

  DCreate(&qDial, jxlW, 10, 10, 80, 100, 0.0, 100.0, 80.0, 1.0, 5.0,
	  infofg, infobg, hicol, locol, "Quality", "%");

  DCreate(&smDial, jxlW, 120, 10, 80, 100, 1.0, 10.0, 7.0, 1.0, 3.0,
	  infofg, infobg, hicol, locol, "Effort", "");

  BTCreate(&jbut[J_BOK], jxlW, JWIDE-180-1, JHIGH-10-BUTTH-1, 80, BUTTH,
	   "Ok", infofg, infobg, hicol, locol);

  BTCreate(&jbut[J_BCANC], jxlW, JWIDE-90-1, JHIGH-10-BUTTH-1, 80, BUTTH,
	   "Cancel", infofg, infobg, hicol, locol);

  XMapSubwindows(theDisp, jxlW);
}


/***************************************************/
void JXLDialog(int vis)
{
  if (vis) {
    CenterMapWindow(jxlW, jbut[J_BOK].x + (int) jbut[J_BOK].w/2,
		    jbut[J_BOK].y + (int) jbut[J_BOK].h/2, JWIDE, JHIGH);
  }
  else     XUnmapWindow(theDisp, jxlW);
  jxlUp = vis;
}

/***************************************************/
static void drawJD(int x,int y,int w,int h)
{
  const char *title  = "Save JPEG XL file...";
  const char *title1 = "Quality 100 is lossless,";
  const char *title2 = "~ 90 visually lossless";
  const char *title3 = "";
  const char *title4 = "";
  const char *title5 = "";

  const char *qtitle1 = "Default is 80.";
  const char *qtitle2 = "Recommended";
  const char *qtitle3 = "is 68 - 96.";

  const char *smtitle1 = "Default = 7.";
  const char *smtitle2 = "Higher effort means";
  const char *smtitle3 = "more computation.";

  int  i;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  for (i=0; i<J_NBUTTS; i++) BTRedraw(&jbut[i]);

  DrawString(jxlW, 220, 10+ASCENT,                   title);
  DrawString(jxlW, 230, 10+ASCENT+LINEHIGH*1,        title1);
  DrawString(jxlW, 230, 10+ASCENT+LINEHIGH*2,        title2);
  DrawString(jxlW, 230, 10+ASCENT+LINEHIGH*3,        title3);
  DrawString(jxlW, 230, 10+ASCENT+LINEHIGH*4,        title4);
  DrawString(jxlW, 230, 10+ASCENT+LINEHIGH*5,        title5);

  DrawString(jxlW,  15, 10+100+10+ASCENT,            qtitle1);
  DrawString(jxlW,  15, 10+100+10+ASCENT+LINEHIGH,   qtitle2);
  DrawString(jxlW,  15, 10+100+10+ASCENT+LINEHIGH*2, qtitle3);

  DrawString(jxlW, 115, 10+100+10+ASCENT+LINEHIGH*0, smtitle1);
  DrawString(jxlW, 115, 10+100+10+ASCENT+LINEHIGH*1, smtitle2);
  DrawString(jxlW, 115, 10+100+10+ASCENT+LINEHIGH*2, smtitle3);

  XSetClipMask(theDisp, theGC, None);
}

static const char* saveJXL(FILE* fp, void* buffer, size_t buffer_size,
			   int is_gray,
			   JxlEncoder* encoder, void* parallel_runner,
			   JxlBasicInfo* output_info, JxlPixelFormat* pixel_format)
{
  if (!parallel_runner || !encoder) {
    return "Creation of the JXL encoder failed";
  }

  JxlResizableParallelRunnerSetThreads(
    parallel_runner, JxlResizableParallelRunnerSuggestThreads(
      output_info->xsize, output_info->ysize));

  JxlEncoderStatus status =
    JxlEncoderSetParallelRunner(encoder, JxlResizableParallelRunner, parallel_runner);
  if (status != JXL_ENC_SUCCESS) {
    return "JxlEncoderSetParallelRunner failed";
  }

  double distance;
  if (qDial.val > 99.0) {
    output_info->uses_original_profile = JXL_TRUE;
    distance = 0;
  } else {
    output_info->uses_original_profile = JXL_FALSE;
    distance = JxlEncoderDistanceFromQuality((float)qDial.val);
  }

  status = JxlEncoderSetBasicInfo(encoder, output_info);
  if (status != JXL_ENC_SUCCESS) {
    return "JxlEncoderSetBasicInfo failed";
  }

  JxlColorEncoding color_profile;
  JxlColorEncodingSetToLinearSRGB(&color_profile, is_gray);
  status = JxlEncoderSetColorEncoding(encoder, &color_profile);
  if (status != JXL_ENC_SUCCESS) {
    return "JxlEncoderSetColorEncoding failed";
  }

  if (picExifInfo) {
    if (DEBUG) { fprintf(stderr, "DBGJXL: write exif: %d bytes\n", picExifInfoSize); }

    byte* exif = malloc(picExifInfoSize + 4);

    // mark 6 byte offset to compensate Exif\0\0 start
    // taken from image magick, not sure why the 6 bytes cannot just be omitted
    exif[0] = exif[1] = exif[2] = 0;
    exif[3] = 6;
    memcpy(exif + 4, picExifInfo, picExifInfoSize);

    status = JxlEncoderUseBoxes(encoder);
    if (status != JXL_ENC_SUCCESS) {
      fprintf(stderr, "JxlEncoderUseBoxes for exif data failed %d\n", status);
    }
    else {
      status = JxlEncoderAddBox(encoder, "Exif", exif, picExifInfoSize - 6, JXL_FALSE);
      if (status != JXL_ENC_SUCCESS) {
	fprintf(stderr, "JxlEncoderAddBox for exif data failed %d\n", status);
      }
    }
    JxlEncoderCloseBoxes(encoder);
    free(exif);
  }

  JxlEncoderFrameSettings* frame_settings = JxlEncoderFrameSettingsCreate(encoder, NULL);
  if (frame_settings == NULL) {
    fprintf(stderr, "JxlEncoderFrameSettingsCreate failed");
  }
  JxlEncoderSetFrameDistance(frame_settings, distance);
  JxlEncoderSetFrameLossless(frame_settings, output_info->uses_original_profile);

  status = JxlEncoderAddImageFrame(frame_settings, pixel_format,
				   buffer,
				   buffer_size);
  if (status != JXL_ENC_SUCCESS) {
    return "JxlEncoderAddImageFrame failed";
  }

  JxlEncoderCloseInput(encoder);

  size_t outlen = 65536;
  byte* compressed = malloc(outlen);
  size_t avail_out;
  byte* next_out;

  do {
    next_out = compressed;
    avail_out = outlen;
    status = JxlEncoderProcessOutput(encoder, &next_out, &avail_out);
    fwrite(compressed, 1, (next_out-compressed), fp);

    if (status == JXL_ENC_ERROR) {
      free(compressed);
      return "JxlEncoderProcessOutput failed";
    }
    else if (status != JXL_ENC_SUCCESS && status != JXL_ENC_NEED_MORE_OUTPUT) {
      fprintf(stderr, "unexpected status %d\n", status);
    }
  } while (status != JXL_ENC_SUCCESS);

  free(compressed);
  return NULL;
}

static void writeJXL(void)
{
  FILE          *fp;
  IMAGE_SAVE_DATA imageData = { 0 };

  fp = OpenOutFile(filename);
  if (!fp) return;

  fbasename = BaseName(filename);

  WaitCursor();

  if (prepareJpgSave(&imageData, colorType, fbasename)) {
    return;
  }

  void* buffer = NULL;
  size_t buffer_size = 0;

  if (colorType == F_GREYSCALE) {   /* build an 8-bit Greyscale image */
    buffer = imageData.image8;
    buffer_size = imageData.npixels;
  }
  else {  /* PIC24 */
    buffer = imageData.image24;
    buffer_size = 3 * imageData.npixels;
  }

  if (DEBUG) {
    fprintf(stderr, "DBGJXL: save %s\n", fbasename);
    fprintf(stderr, "        %dx%d %s\n", imageData.w, imageData.h, colorType == F_GREYSCALE ? "grey" : "color");
    fprintf(stderr, "        %d %d\n", (int)qDial.val, (int)smDial.val);
  }

  JxlBasicInfo output_info;
  JxlEncoderInitBasicInfo(&output_info);
  output_info.have_container = JXL_FALSE;
  output_info.xsize = imageData.w;
  output_info.ysize = imageData.h;
  output_info.bits_per_sample = 8;
  output_info.orientation = JXL_ORIENT_IDENTITY;
  output_info.num_color_channels = colorType == F_GREYSCALE ? 1 : 3;
  output_info.num_extra_channels = 0;
  output_info.alpha_bits = 0;

  JxlPixelFormat pixel_format;
  pixel_format.data_type = JXL_TYPE_UINT8;
  pixel_format.endianness = JXL_NATIVE_ENDIAN;
  pixel_format.align = 0;
  pixel_format.num_channels = colorType == F_GREYSCALE ? 1 : 3;

  JxlEncoder* encoder = JxlEncoderCreate(NULL);
  void* parallel_runner = JxlResizableParallelRunnerCreate(NULL);

  const char* error = saveJXL(fp, buffer, buffer_size, colorType == F_GREYSCALE,  encoder, parallel_runner, &output_info, &pixel_format);
  if (parallel_runner) { JxlResizableParallelRunnerDestroy(parallel_runner); }
  if (encoder) { JxlEncoderDestroy(encoder); }

  if      (colorType == F_GREYSCALE) free(imageData.image8);
  else if (imageData.ptype == PIC8)  free(imageData.image24);

  if (imageData.pfree) free(imageData.inpix);

  if (CloseOutFileWhy(fp, filename, !!error, error) == 0) DirBox(0);
  SetCursors(-1);
}

/***************************************************/
static void doCmd(int cmd)
{

  switch (cmd) {
  case J_BOK: {
    char *fullname;

    writeJXL();
    JXLDialog(0);

    fullname = GetDirFullName();
    if (!ISPIPE(fullname[0])) {
      XVCreatedFile(fullname);
      StickInCtrlList(0);
    }
  }
    break;

  case J_BCANC:  JXLDialog(0);  break;

  default:        break;
  }
}

/***************************************************/
static void clickJD(int x, int y)
{
  int i;
  BUTT *bp;

  /* check BUTTs */

  for (i=0; i<J_NBUTTS; i++) {
    bp = &jbut[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  if (i<J_NBUTTS) {  /* found one */
    if (BTTrack(bp)) doCmd(i);
  }
}

/***************************************************/
int JXLCheckEvent(XEvent* xev)
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!jxlUp) return 0;

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    /* throw away excess expose events for 'dumb' windows */
    if (e->count > 0 && (e->window == qDial.win ||
			 e->window == smDial.win)) {}

    else if (e->window == jxlW)       drawJD(x, y, w, h);
    else if (e->window == qDial.win)   DRedraw(&qDial);
    else if (e->window == smDial.win)  DRedraw(&smDial);
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == jxlW)      clickJD(x,y);
      else if (e->window == qDial.win)  DTrack(&qDial,  x,y);
      else if (e->window == smDial.win) DTrack(&smDial, x,y);
      else rv = 0;
    }  /* button1 */
    else rv = 0;
  }  /* button press */


  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;
    int stlen;

    stlen = XLookupString(e,buf,128,&ks,(XComposeStatus *) NULL);
    buf[stlen] = '\0';

    RemapKeyCheck(ks, buf, &stlen);

    if (e->window == jxlW) {
      if (stlen) {
	if (buf[0] == '\r' || buf[0] == '\n') { /* enter */
	  FakeButtonPress(&jbut[J_BOK]);
	}
	else if (buf[0] == '\033') {            /* ESC */
	  FakeButtonPress(&jbut[J_BCANC]);
	}
      }
    }
    else rv = 0;
  }
  else rv = 0;

  if (rv==0 && (xev->type == ButtonPress || xev->type == KeyPress)) {
    XBell(theDisp, 50);
    rv = 1;   /* eat it */
  }

  return rv;
}

/***************************************************/
void JXLSaveParams(char* fname, int col)
{
  if (DEBUG) {
    fprintf(stderr, "DBGJXL: %s %d\n", fname, col);
  }
  filename = fname;
  colorType = col;
}

static void JXLDebug(const char *message) {
  if (DEBUG) {
    fprintf(stderr, "DBGJXL: %s\n", message);
  }
}

static void JXLDebugDecoderStatus(JxlDecoderStatus status, const char *message) {
  char *status_text = "";

  if (!DEBUG) {
    return;
  }

  if (status == JXL_DEC_SUCCESS) {
    status_text = "success";
  }
  else if (status == JXL_DEC_ERROR) {
    status_text = "error";
  }
  else if (status == JXL_DEC_NEED_MORE_INPUT) {
    status_text = "need more input";
  }
  else if (status == JXL_DEC_NEED_PREVIEW_OUT_BUFFER) {
    status_text = "need preview out buffer";
  }
    else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
	status_text = "need image out buffer";
    }
    else if (status == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
	status_text = "jpeg need more output";
    }
    else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
	status_text = "box need more output";
    }
    else if (status == JXL_DEC_BASIC_INFO) {
	status_text = "basic info";
    }
    else if (status == JXL_DEC_COLOR_ENCODING) {
	status_text = "color encoding";
    }
    else if (status == JXL_DEC_PREVIEW_IMAGE) {
	status_text = "preview image";
    }
    else if (status == JXL_DEC_FRAME) {
	status_text = "frame";
    }
    else if (status == JXL_DEC_FULL_IMAGE) {
	status_text = "full image";
    }
    else if (status == JXL_DEC_JPEG_RECONSTRUCTION) {
	status_text = "jpeg reconstruction";
    }
    else if (status == JXL_DEC_BOX) {
	status_text = "box";
    }
    else if (status == JXL_DEC_FRAME_PROGRESSION) {
	status_text = "frame progression";
    }
    else if (status == JXL_DEC_BOX_COMPLETE) {
	status_text = "jpeg box complete";
    }

    fprintf(stderr, "DBGJXL: STATE: %s: %s (%d)\n", message, status_text, status);
}

static int checkGrayscaleColor(JxlDecoder* decoder) {
  JxlColorEncoding color_encoding;

  JxlDecoderStatus stat = JxlDecoderGetColorAsEncodedProfile(decoder, JXL_COLOR_PROFILE_TARGET_DATA, &color_encoding);
  if (DEBUG) {
    JXLDebugDecoderStatus(stat, "decoder get color as encoded profile");
    fprintf(stderr, "        color space %d, transfer function %d, gamma fct %d, gamma %lf\n", color_encoding.color_space, color_encoding.transfer_function, JXL_TRANSFER_FUNCTION_GAMMA , color_encoding.gamma);
    fprintf(stderr, "        primaries %d, white point %d\n", color_encoding.primaries, color_encoding.white_point);
  }
  if (stat == JXL_DEC_SUCCESS) {
    return color_encoding.color_space == JXL_COLOR_SPACE_GRAY;
  }

  size_t color_size;
  JxlColorProfileTarget target = JXL_COLOR_PROFILE_TARGET_DATA;
  stat = JxlDecoderGetICCProfileSize(decoder, target, &color_size);
  if (DEBUG) {
    JXLDebugDecoderStatus(stat, "decoder get icc profile size");
    fprintf(stderr, "        color size: %ld\n", color_size);
  }
  uint8_t* icc_profile = malloc(color_size);
  stat = JxlDecoderGetColorAsICCProfile(decoder, target, icc_profile, color_size);
  if (DEBUG) {
    JXLDebugDecoderStatus(stat, "decoder get color as icc profile");
    printf("CMM type: \"%.4s\", ", icc_profile + 4);
    printf("Color space: \"%.4s\", ", icc_profile + 16);
    printf("Rendering intent: %d\n", (int)(icc_profile[67]));
  }
  int gray = !memcmp(icc_profile + 16, "GRAY", 4);
  free(icc_profile);
  return gray;
}

int LoadJXL(char* fname, PICINFO* pinfo) {
  JxlDecoder* decoder;
  JxlBasicInfo info;
  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};

  uint8_t* image_buffer = NULL;

  size_t bufsize = 256 * 1024;
  uint8_t *buf = malloc(bufsize);

  if (!buf) {
    fprintf(stderr, "JXL: could not allocate exif buffer\n");
    return 0;
  }

  size_t box_buf_size = 64 * 1024;
  uint8_t *box_buf = malloc(box_buf_size);
  size_t exif_box = 0;

  if (!box_buf) {
    fprintf(stderr, "JXL: could not allocate exif buffer\n");
    free(buf);
    return 0;
  }

  decoder = JxlDecoderCreate(NULL);

  JxlDecoderSubscribeEvents(decoder, JXL_DEC_BASIC_INFO|JXL_DEC_COLOR_ENCODING |JXL_DEC_FULL_IMAGE|JXL_DEC_BOX|JXL_DEC_BOX_COMPLETE);

  FILE* fh = fopen(fname, "r");

  size_t bufread = fread(buf, 1, bufsize, fh);
  size_t filesize = bufread;

  byte* exif = (byte*) NULL;
  size_t exifSize = 0;


  JXLDebug(fname);

  JxlDecoderSetCms(decoder, *JxlGetDefaultCms());
  JxlDecoderSetKeepOrientation(decoder, JXL_FALSE);

  JxlDecoderSetInput(decoder, buf, bufread);
  JxlDecoderSetDecompressBoxes(decoder, JXL_TRUE);

  while (bufread > 0) {
    JxlDecoderStatus stat = JxlDecoderProcessInput(decoder);
    JXLDebugDecoderStatus(stat, "process input");

    if (stat == JXL_DEC_NEED_MORE_INPUT) {
      size_t processed = JxlDecoderReleaseInput(decoder);
      if (DEBUG) {
	fprintf(stderr, "        more input: processed %ld\n", processed);
      }
      if (processed > 0) {
	memmove(buf, buf + bufsize - processed, processed);
      }
      bufread = fread(buf + processed, 1, bufsize - processed, fh);
      filesize += bufread;
      JxlDecoderSetInput(decoder, buf, bufread + processed);
    }
    else if (stat == JXL_DEC_BASIC_INFO) {
      stat = JxlDecoderGetBasicInfo(decoder, &info);
      if (DEBUG) {
	JXLDebugDecoderStatus(stat, "get basic info");
	fprintf(stderr, "        size: %d x %d\n", info.xsize, info.ysize);
	fprintf(stderr, "        depth %d color channels %d extra %d orientation %d\n", info.bits_per_sample, info.num_color_channels, info.num_extra_channels, info.orientation);
	fprintf(stderr, "        %s, ", info.have_preview ? "preview" : "no preview");
	fprintf(stderr, "%s, ", info.uses_original_profile ? "org profile" : "no org profile");
	fprintf(stderr, "%s\n ", info.have_animation ? "animation" : "no animation");
	fprintf(stderr, "       format: channels %d, data type %d, endianness %d, align %ld\n",
		format.num_channels,
		format.data_type,
		format.endianness,
		format.align);
      }
      if ( info.have_animation ) {
	SetISTR(ISTR_WARNING, "cannot handle animations");
      }
      if ( info.num_extra_channels ) {
	SetISTR(ISTR_WARNING, "cannot handle extra channels (e.g. alpha channel)");
      }
    }
    else if (stat == JXL_DEC_COLOR_ENCODING) {
      int gray = checkGrayscaleColor(decoder);

      format.num_channels = gray ? 1 : 3;
    }
    else if (stat == JXL_DEC_BOX) {
      if ( exif_box ) {
	fprintf(stderr, "exif not handled, too large?\n");
	JxlDecoderReleaseBoxBuffer(decoder);
	exif_box = 0;
      }
      JxlBoxType type;

      size_t size;
      size_t size_contents;
      stat = JxlDecoderGetBoxType(decoder, type, JXL_TRUE);
      stat = JxlDecoderGetBoxSizeRaw(decoder, &size);
      if (DEBUG) {
	stat = JxlDecoderGetBoxSizeContents(decoder, &size_contents);
	fprintf(stderr, "BOX: %.4s raw %ld content %ld\n", type, size, size_contents);
      }
      if ( !memcmp(type, "Exif", 4) ) {
	JxlDecoderSetBoxBuffer(decoder, box_buf, box_buf_size);
	exif_box = size;
      }
    }
    else if (stat == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
      // exif > 64k is not handled
      fprintf(stderr, "exif need more output, too large!\n");
      JxlDecoderReleaseBoxBuffer(decoder);
      exif_box = 0;
    }
    else if (stat == JXL_DEC_BOX_COMPLETE && exif_box) {
      size_t size_contents;
      size_t box_size;
      stat = JxlDecoderGetBoxSizeContents(decoder, &size_contents);
      box_size = JxlDecoderReleaseBoxBuffer(decoder);
      exif_box = 0;

      size_t offset = 0;
      // taken from image magick jxl code...
      offset|=(unsigned int)box_buf[0] << 24;
      offset|=(unsigned int)box_buf[1] << 16;
      offset|=(unsigned int)box_buf[2] << 8;
      offset|=(unsigned int)box_buf[3] << 0;
      if (offset < box_size - 4 ) {
	exifSize = box_size + 2 - offset;
	exif = malloc(exifSize);
	memcpy(exif, "Exif\x0\x0", 6);
	memcpy(exif + 6, box_buf + 4 + offset, exifSize - 6);
      }
    }
    else if (stat == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      size_t size;
      stat = JxlDecoderImageOutBufferSize(decoder, &format, &size);
      if (DEBUG) {
	JXLDebugDecoderStatus(stat, "decoder image out buffer size");
	fprintf(stderr, "        %ld\n", size);
      }
      image_buffer = malloc(size);
      stat = JxlDecoderSetImageOutBuffer(decoder, &format, image_buffer, size);
      JXLDebugDecoderStatus(stat, "set image output buffer");
    }
    else if (stat == JXL_DEC_FULL_IMAGE) {
      break;
    }
  }

  if ( exif_box ) {
    fprintf(stderr, "exif not handled\n");
    JxlDecoderReleaseBoxBuffer(decoder);
    exif_box = 0;
  }

  // set xv info
  if ( format.num_channels == 1 ) {
    pinfo->type = PIC8;
    pinfo->colType = F_GREYSCALE;
    if (info.num_color_channels == 1) {
      for (int i=0; i<256; i++) pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;
    }
  } else {
    pinfo->type = PIC24;
    pinfo->colType = F_FULLCOLOR;
  }
  pinfo->pic = (byte*) image_buffer;
  pinfo->comment = (char*) NULL;
  pinfo->exifInfo = exif;
  pinfo->exifInfoSize = exifSize;
  pinfo->w = pinfo->normw = info.xsize;
  pinfo->h = pinfo->normh = info.ysize;
  pinfo->frmType = F_JXL;

  pinfo->orientation = 0; // image is rotated by libjxl...

  if (DEBUG) {
    fprintf(stderr, "DBGJXL: picinfo %d x %d %s %s\n",
	    pinfo->normw, pinfo->normh,
	    pinfo->type == PIC8 ? "pic8" : "pic24",
	    pinfo->colType == F_GREYSCALE ? "greyscale" : "fullcolor");
  }
  sprintf(pinfo->fullInfo, "JXL. (%ld bytes)", filesize);
  sprintf(pinfo->shrtInfo, "%dx%d JPEG XL. ", info.xsize, info.ysize);

  fclose(fh);
  free(buf);
  free(box_buf);

  return 1;
}

void VersionInfoJXL()
{
  fprintf(stderr, "   Compiled with libjxl %d.%d.%d.\n", JPEGXL_MAJOR_VERSION, JPEGXL_MINOR_VERSION, JPEGXL_PATCH_VERSION);
}
#endif
