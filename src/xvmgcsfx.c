/*
 * $Id: xvmgcsfx.c,v 1.23 95/11/27 19:03:36 tin329 Exp Locker: tin329 $
 * xvmgcsfx.c - Use the filters as input and output method.
 *
 * Features
 * ========
 * Use the filters as input and output method for load and save unsupported
 * image format file. The filter command is recognized by definition of
 * magic number or suffix in "~/.xv_mgcsfx" .
 *
 * Bugs
 * ====
 * There are many bugs.
 * Let's go hunting for insects with an insect net. (it's all joke.)
 *
 * Author
 * ======
 * Tetsuya INOUE  <tin329@chino.it.okayama-u.ac.jp>
 */

/*
 * Known Bugs and Todo  /  あれこれ気になること
 *
 *  ~/.xv_mgcsfx 内
 *    ・定義が不完全だとエラー (':'の数)。
 *    ・デリミタとして ':' を使うので、スタートアップファイル内で
 *      ':' を用いて定義はできない。'\:'でもダメ。
 *    ・ magic タイプで、８進数は３桁分０〜７を調べ、１６進数は
 *       isxdigit が真を返す間中処理される。しかし、１ｂｙｔｅと
 *       してしか評価されない。
 *    ・プリプロセッサを使うときは、コメントの書き方に注意しなければな
 *        らない。プリプロセッサによってはコメントがエラーになる。
 *    ・パイプへの入出力のフォーマットの種類が PNM のみ
 *        入力
 *            ファイルポインタを seek してはいけない
 *            ファイルサイズを用いてはいけない
 *        出力
 *            ファイルポインタを seek してはいけない
 *            exec できなくて終了したプロセスに書き込み不可
 *    ・サフィックスとマジックナンバーの使い分けをどうするか。
 *        マジックナンバーが同じで、サフィックスが異なる場合を認めるか？
 *    ・compress(gzip)のファイルはテンポラリでは xvtmp??? という名前な
 *      ので suffix では識別できない。
 *
 *  認識する時に MACBINARY には負ける(in xv.c)。
 *
 *  多重に pipe を通すことができない。(pipe が seek できないから)
 *    ・socketpair で、recv に MSG_PEEK フラグをつかって空読みする。
 *    ・これをやるとファイルの認識がめちゃめちゃ遅くなる。
 *
 *  リソースで設定
 *    ・リソースで設定する方が面倒くさい
 *
 *  マジックナンバーの設定に正規表現
 *
 *  セーブ用プロセスが失敗する場合の対策が今一つ
 *
 *  DEC OSF/1 V3.0 では、パイプにデータがまだない時に読み込もうとすると、
 *  read が不完全になる。(in xvpbm.c)
 *  同様に書き込み時にも問題が生じるかもしれない。
 */

#define  NEEDSDIR               /* for stat() */
#include "xv.h"


#ifdef HAVE_MGCSFX


#ifdef __osf__
#  ifdef __alpha
#    define ARCHITECTURE64 1
#  endif /* __alpha */
#endif /* __osf__ */

#ifdef ARCHITECTURE64
typedef short int16;
typedef int   int32;
typedef long  int64;
#else
typedef short int16;
typedef long  int32;
#endif /* ARCHITECTURE64 */

#ifdef sgi
#  define vfork fork
#endif

#define USE_SIGCHLD
#if 0
#  undef  USE_SIGCHLD
#endif

#ifdef USE_SIGCHLD
#  include <sys/wait.h>
#endif

typedef struct _mgcsfxtab
{
  struct _mgcsfxtab *next;
  char              *description;
  int                mgcsfx_type;
  int                offset;
  union{
    int16            int16_data;
    int32            int32_data;
    char            *string_data;
  }                  dt;
  int                string_len;
  char              *suffix;
  int                input_image_type;
  char              *input_command;
  int                output_image_type;
  char              *output_command;
} mgcsfxtab;


#ifndef MGCSFXDIR
#  define MGCSFXDIR       "/usr/local/lib"
#endif
#ifndef SYSCONFDIR
#  define SYSCONFDIR      MGCSFXDIR
#endif
#ifndef MGCSFX_SITE_RC
#  define MGCSFX_SITE_RC  "xv_mgcsfx"
#endif
#ifndef MGCSFX_RC
#  define MGCSFX_RC       ".xv_mgcsfx"
#endif

#ifdef USE_MGCSFX_PREPROCESSOR
#  ifndef MGCSFX_PREPROCESSOR
#    define MGCSFX_PREPROCESSOR "/usr/lib/cpp"
#  endif
#endif


/* Check type for Magic number and Suffix */
enum {T_UNKNOWN,
      T_MAGIC, T_SUFFIX,
      T_BEINT16, T_BEINT32, T_BEINT64,
      T_LEINT16, T_LEINT32, T_LEINT64};

/* Image Type for input and output format */
enum {IT_UNKNOWN,
#ifdef HAVE_MGCSFX_AUTO
      IT_AUTO,
#endif /* HAVE_MGCSFX_AUTO */
      IT_PNM, IT_PPM, IT_PGM, IT_PBM,
      IT_PNM_RAW, IT_PPM_RAW, IT_PGM_RAW, IT_PBM_RAW,
      IT_PNM_ASCII, IT_PPM_ASCII, IT_PGM_ASCII, IT_PBM_ASCII,
      IT_GIF, IT_JPEG, IT_TIFF, IT_JFIF, /* IT_PS, IT_COMPRESS,*/
      IT_XBM, IT_XPM, IT_BMP, IT_SUNRAS, IT_IRIS, IT_XWD,
      /* IT_TARGA, IT_FITS, IT_PM, IT_UTAHRLE, IT_PCX, IT_PDSVICAR, IT_IFF, */
      IT_MAG, IT_MAKI, IT_PI, IT_PIC, IT_PIC2 /* , IT_PCD */};


/*--------------------------------------------------------------------------*/
void  mgcsfx_handler        PARM((int));
void  mgcsfx_handler_setup  PARM((void));

#ifdef USE_MGCSFX_PREPROCESSOR
static char      *get_tmp_fname          PARM((void));
static char      *make_preprocessed_file PARM((char *));
#endif /* USE_MGCSFX_PREPROCESSOR */

int   is_mgcsfx             PARM((char *, unsigned char *, int));

char *mgcsfx_auto_input_com PARM((char *));


static mgcsfxtab *free_mgcsfx PARM((mgcsfxtab *));
static char      *fgettoken   PARM((FILE*, int));
static int        string_fin  PARM((char *));
static int        type_mgcsfx PARM((char *));
static int        type_image  PARM((char *));

static void       read_mgcsfx PARM((mgcsfxtab **, char *));
static void       init_mgcsfx PARM((void));
static mgcsfxtab *find_mgcsfx PARM((char *, unsigned char *, int));

int   LoadMGCSFX            PARM((char *, PICINFO *));

#ifdef SVR4
typedef void Sigfunc(int);
static Sigfunc   *xv_signal   PARM((int , Sigfunc *));
#endif

/*--------------------------------------------------------------------------*/
mgcsfxtab *mgcsfx_table = NULL;
int       mgcsfx_setup_flag = 0;

int       nitem_mgcsfx = 0;
int       desc_width = 0;

int       max_offset_mgcsfx = 0;
int       max_length_mgcsfx = 0;
int       need_buf_size = 0;

static char input_command_ex[1024];
static int  input_command_ex_flag = 0;

#ifdef USE_SIGCHLD
static int  w_p_fail=0;
#endif

/*--------------------------------------------------------------------------*/

/***************************************************/
void mgcsfx_handler(sig)
     int sig;
{
#ifdef USE_SIGCHLD
  int pid, pst;
#endif

#if defined(SYSV) || defined(SVR4)
  sighold(sig);
#else
  sigblock(sigmask(sig));
#endif

#ifdef USE_SIGCHLD
  if(w_p_fail == 1){
    /*
     * At this point, process write to broken pipe.
     * Probably external command was can't exec.
     */
    w_p_fail = 2;
    pid = wait(&pst);
  }
#endif

  return;

  /* Quit(1); */ /*exit(1);*/
}

void mgcsfx_handler_setup()
{
#ifdef SVR4
  xv_signal(SIGPIPE, (void (*)PARM((int))) mgcsfx_handler);
  xv_signal(SIGCHLD, (void (*)PARM((int))) mgcsfx_handler);
#else
# ifdef SYSV
  sigset(SIGPIPE, (void (*)PARM((int))) mgcsfx_handler);
  sigset(SIGCHLD, (void (*)PARM((int))) mgcsfx_handler);
# else
  signal(SIGPIPE, (void (*)PARM((int))) mgcsfx_handler);
  signal(SIGCHLD, (void (*)PARM((int))) mgcsfx_handler);
# endif
#endif
}

/***************************************************/
#ifdef USE_MGCSFX_PREPROCESSOR
static char *get_tmp_fname()
{
  static char tmp[MAXPATHLEN+1];

#ifndef VMS
  sprintf(tmp, "%s/xvmgcsfxXXXXXX",tmpdir);
#else
  /* sprintf(tmp, "Sys$Scratch:xvmgcsfxXXXXXX"); */
  strcpy(tmp, "[]xvmgcsfxXXXXXX");
#endif /* VMS */

#ifdef USE_MKSTEMP
  close(mkstemp(tmp));
#else
  mktemp(tmp);
#endif

  return tmp;
}

static char *make_preprocessed_file(fname)
     char *fname;
{
  char buf[512];
  char *tmp_name;

  tmp_name = get_tmp_fname();

#ifndef VMS
  sprintf(buf,"%s %s > %s", MGCSFX_PREPROCESSOR, fname, tmp_name);
#else
  sprintf(buf,"%s %s > %s", MGCSFX_PREPROCESSOR, fname, tmp_name); /* really OK? */
#endif

  SetISTR(ISTR_INFO, "Preprocessing '%s'...", BaseName(fname));
#ifndef VMS
  if (system(buf))
#else
  if (!system(buf))
#endif
  {
    SetISTR(ISTR_INFO, "Unable to preprocess '%s'.", BaseName(fname));
    Warning();
    return NULL;
  }

  return tmp_name;
}
#endif /* USE_MGCSFX_PREPROCESSOR */

/***************************************************/
/* 認識できるファイルかどうか調べる */
int is_mgcsfx(fname,buffer,size)
     char          *fname;
     unsigned char *buffer;
     int            size;
{
  mgcsfxtab          *magic;
  FILE               *fp;
  unsigned char      *buf;
  int                 s;

  if(nomgcsfx){
    return 0;
  }else{
    if(size < need_buf_size){
      if((buf = (unsigned char *)calloc(need_buf_size, sizeof(char)))==NULL){
	fprintf(stderr,"Can't allocate memory\n");
	return 0;
      }
      if((fp = xv_fopen(fname, "r"))==NULL){
	fprintf(stderr,"Can't open file %s\n", fname);
	free(buf);
	return 0;
      }
      s = fread(buf, 1, need_buf_size, fp);
      if((magic = find_mgcsfx(fname, buf, s))!=NULL &&
	magic->input_command != NULL){
	free(buf);
	fclose(fp);
	return 1;
      }else{
	free(buf);
	fclose(fp);
	return 0;
      }
    }else{
      if((magic = find_mgcsfx(fname, buffer, size))!=NULL &&
	magic->input_command != NULL){
	return 1;
      }else{
	return 0;
      }
    }
  }
}

#ifdef HAVE_MGCSFX_AUTO
char *mgcsfx_auto_input_com(fname)
char *fname;
{
  static char command[1024];
  mgcsfxtab       *magic;
  char *ptr;

  FILE *fp;
  unsigned char *buf;
  int                 s;

  if((buf = (unsigned char *)calloc(need_buf_size, sizeof(char)))==NULL){
    fprintf(stderr,"Can't allocate memory\n");
    return NULL;
  }
  if((fp = xv_fopen(fname, "r"))==NULL){
    fprintf(stderr,"Can't open file %s\n", fname);
    free(buf);
    return NULL;
  }
  s = fread(buf, 1, need_buf_size, fp);
  if((magic = find_mgcsfx(fname, buf, s))!=NULL &&
     magic->input_command != NULL && magic->input_image_type == IT_AUTO){
    if ((ptr = strstr(magic->input_command, "%s"))){
      sprintf(command, magic->input_command, fname);
    }else{
      sprintf(command, "%s < %s", magic->input_command, fname);
    }
    free(buf);
    fclose(fp);
    return command;
  }else{
    free(buf);
    fclose(fp);
    return NULL;
  }
}
#endif /* HAVE_MGCSFX_AUTO */

/***************************************************/
static mgcsfxtab *free_mgcsfx(m)
     mgcsfxtab *m;
{
  mgcsfxtab *next;
  if(m == NULL) return NULL;
  next = m->next;
  if(m->description != NULL) free(m->description);
  if(m->mgcsfx_type == T_MAGIC && m->dt.string_data != NULL)
    free(m->dt.string_data);
  if(m->suffix != NULL) free(m->suffix);
  if(m->input_command != NULL) free(m->input_command);
  if(m->output_command != NULL) free(m->output_command);
  free(m);
  return next;
}



/***************************************************/
/* char c または '\n' で区切られた文字列を取り出す
 *  ファイルの最後まで読んだら NULL を返す
 *  改行なら改行を返す(改行で区切られた場合は '\n' をストリームに戻す)
 */
#define CBUF_SIZE 1024
static char *fgettoken(fp, c)
     FILE *fp;
     int   c; /* Real mean is char */
{
  char *buf;
  char *buf2;
  int   i;
  int   n=0;
  int   max=0;
  int   count = 1;

  char *ss;
  char *se;

  if((buf = (char *)calloc(CBUF_SIZE, sizeof(char))) == NULL){
    fprintf(stderr,"Can't allocate memory\n");
    exit(1);
  }
  max = CBUF_SIZE;
  count = 2;

  do{
    if((i = getc(fp))==EOF || i == '\n' || i == c) break;

    buf[n] = (char)i;

    if(i != c && n == max-1){
      buf[max] = '\0';
      if((buf2 = (char *)calloc(CBUF_SIZE * count, sizeof(char))) == NULL){
	fprintf(stderr,"Can't allocate memory\n");
	exit(1);
      }
      strcpy(buf2, buf);
      free(buf);
      buf = buf2;
      buf2 = NULL;
      max = CBUF_SIZE * count;
      count++;
    }

    n++;
  }while(i != c);

  buf[n] = '\0';

  /* 最初と最後の空白文字を切り詰める */
  ss = buf + strspn(buf, " \t\b\r\n"); /* find the first non-white space */
  se = buf + strlen(buf);              /* find the end of the string */

  /* strip from the end first */
  while ((--se >= ss) && strchr(" \t\b\r\n", *se));
  *(++se) = '\0';

  if(i == EOF && strlen(ss)==0){        /* EOF なら NULL を返す */
    free(buf);
    return NULL;
  }else if(i == '\n' && strlen(ss)==0){ /* 改行のみの場合 */
    static char cr[2] = {'\n','\0'};
    buf2 = strdup(cr);
    free(buf);
    return buf2;
  }else{                                /* 通常 */
    if(i == '\n' && strlen(ss)>0) ungetc(i,fp);
    buf2 = strdup(ss);
    free(buf);
    return buf2;
  }
}



/***************************************************/
/* 文字列中の特殊記号(\)を正しいものにする
 */
static int string_fin(string_data)
     char *string_data;
{
  char *cptr;
  char *ptr;
  int   length;

  /* Change all the \xx sequences into a single character */
  cptr = string_data;

  for (ptr = cptr; *ptr; ++ptr){
    if (*ptr != '\\'){
      *cptr = *ptr;
    }else{
      switch (*(++ptr)){
#if defined(__STDC__)
      case 'a': /* Audible alert (terminal bell) */
	*cptr = '\007';
	break;
      case '?': /* Question mark */
	*cptr = '\?';
	break;
#endif
      case 'b': /* Backspace */
	*cptr = '\b';
	break;
      case 'f': /* Form feed */
	*cptr = '\f';
	break;
      case 'n': /* Line feed */
	*cptr = '\n';
	break;
      case 'r': /* Carriage return */
	*cptr = '\r';
	break;
      case 't': /* Horizontal tab */
	*cptr = '\t';
	break;
      case 'v': /* Vertical tab */
	*cptr = '\v';
	break;
      case '\\': /* Backslash */
	*cptr = '\\';
	break;
      case '\'': /* Single quote */
	*cptr = '\'';
	break;
      case '"': /* Double quote */
	*cptr = '\"';
	break;
      case '0': /* Octal constant  \0 ... \377 */
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
	if ((ptr[1] >= '0') && (ptr[1] <= '7')){
	  if ((ptr[2] >= '0') && (ptr[2] <= '7')){ /* \000 ...\377 */
	    *cptr = ((*ptr - '0') * 64) +((ptr[1] - '0') * 8) +(ptr[1] - '0');
	    ptr += 2;
	  }else{ /* \00 ...\77 */
	    *cptr = ((*ptr - '0') * 8) + (ptr[1] - '0');
	    ++ptr;
	  }
	}else{ /* \0 ...\7 */
	  *cptr = *ptr - '0';
	}
	break;
      case 'x': /* Hexadecimal constant  \x0 .. \xff */
	if (isxdigit (ptr[1])){
	  *cptr = 0;
	  while (isxdigit (*(++ptr)))
	    *cptr = (*cptr * 16) +
	      (*ptr > '9' ? tolower (*ptr) - ('a' - 10) : *ptr - '0');
	  --ptr;
	  break;
	}
      default:
	/*  *(cptr++) = '\\';  No use for treat '\z' as 'z' */
	*cptr = *ptr;
	break;
      }
    }
    ++cptr;
  }
  *cptr = '\0';
  length = cptr - string_data;
  return length;
}

/***************************************************/
static int type_mgcsfx(str)
     char *str;
{
  if(str == NULL){
    return T_UNKNOWN;
  }else if(!strcmp(str, "magic")   || !strcmp(str, "MAGIC")){
    return T_MAGIC;
  }else if(!strcmp(str, "string")  || !strcmp(str, "STRING")){
    return T_MAGIC;
  }else if(!strcmp(str, "suffix")  || !strcmp(str, "SUFFIX")){
    return T_SUFFIX;
  }else if(!strcmp(str, "beint16") || !strcmp(str, "BEINT16")){
    return T_BEINT16;
  }else if(!strcmp(str, "leint16") || !strcmp(str, "LEINT16")){
    return T_LEINT16;
  }else if(!strcmp(str, "beint32") || !strcmp(str, "BEINT32")){
    return T_BEINT32;
  }else if(!strcmp(str, "leint32") || !strcmp(str, "LEINT32")){
    return T_LEINT32;
  }else{
    return T_UNKNOWN;
  }
}

/***************************************************/
static int type_image(str)
     char *str;
{
  if(str == NULL){
    return IT_UNKNOWN;
#ifdef HAVE_MGCSFX_AUTO
  }else if(!strcmp(str, "auto") || !strcmp(str, "AUTO")){
    return IT_AUTO;
#endif /* HAVE_MGCSFX_AUTO */
  }else if(!strcmp(str, "pnm") || !strcmp(str, "PNM")){
    return IT_PNM;
  }else if(!strcmp(str, "ppm") || !strcmp(str, "PPM")){
    return IT_PPM;
  }else if(!strcmp(str, "pgm") || !strcmp(str, "PGM")){
    return IT_PGM;
  }else if(!strcmp(str, "pbm") || !strcmp(str, "PBM")){
    return IT_PBM;
  }else if(!strcmp(str, "pnm_raw") || !strcmp(str, "PNM_RAW")){
    return IT_PNM_RAW;
  }else if(!strcmp(str, "ppm_raw") || !strcmp(str, "PPM_RAW")){
    return IT_PPM_RAW;
  }else if(!strcmp(str, "pgm_raw") || !strcmp(str, "PGM_RAW")){
    return IT_PGM_RAW;
  }else if(!strcmp(str, "pbm_raw") || !strcmp(str, "PBM_RAW")){
    return IT_PBM_RAW;
  }else if(!strcmp(str, "pnm_ascii") || !strcmp(str, "PNM_ASCII")){
    return IT_PNM_ASCII;
  }else if(!strcmp(str, "ppm_ascii") || !strcmp(str, "PPM_ASCII")){
    return IT_PPM_ASCII;
  }else if(!strcmp(str, "pgm_ascii") || !strcmp(str, "PGM_ASCII")){
    return IT_PGM_ASCII;
  }else if(!strcmp(str, "pbm_ascii") || !strcmp(str, "PBM_ASCII")){
    return IT_PBM_ASCII;

  }else if(!strcmp(str, "gif")  || !strcmp(str, "GIF")){
    return IT_GIF;
  }else if(!strcmp(str, "jpeg") || !strcmp(str, "JPEG")){
    return IT_JPEG;
  }else if(!strcmp(str, "tiff") || !strcmp(str, "TIFF")){
    return IT_TIFF;
  }else if(!strcmp(str, "jfif") || !strcmp(str, "JFIF")){
    return IT_JFIF;

  }else if(!strcmp(str, "xbm") || !strcmp(str, "XBM")){
    return IT_XBM;
  }else if(!strcmp(str, "xpm") || !strcmp(str, "XPM")){
    return IT_XPM;
  }else if(!strcmp(str, "bmp") || !strcmp(str, "BMP")){
    return IT_BMP;
  }else if(!strcmp(str, "sunras") || !strcmp(str, "SUNRAS")){
    return IT_SUNRAS;
  }else if(!strcmp(str, "iris") || !strcmp(str, "IRIS")){
    return IT_IRIS;
  }else if(!strcmp(str, "xwd") || !strcmp(str, "XWD")){
    return IT_XWD;

  }else if(!strcmp(str, "mag") || !strcmp(str, "MAG")){
    return IT_MAG;
  }else if(!strcmp(str, "maki") || !strcmp(str, "MAKI")){
    return IT_MAKI;
  }else if(!strcmp(str, "pi") || !strcmp(str, "PI")){
    return IT_PI;
  }else if(!strcmp(str, "pic") || !strcmp(str, "PIC")){
    return IT_PIC;
  }else if(!strcmp(str, "pic2") || !strcmp(str, "PIC2")){
    return IT_PIC2;

  }else{
    return IT_UNKNOWN;
  }
}

/*--------------------------------------------------------------------------*/
#define mgcsfx_read_error(FILENAME, LINENUM, AFTERFIELD) \
fprintf (stderr,\
"%s: line %d: missing fields of %s field\n",\
FILENAME, LINENUM, AFTERFIELD);

#define magic_type_error(FILENAME, LINENUM, MAGICNUMBER) \
fprintf (stderr,\
"%s: line %d: invalid <magic type> field '%s'\n",\
FILENAME, LINENUM, MAGICNUMBER);
/*--------------------------------------------------------------------------*/

/***************************************************/
static void read_mgcsfx(mgcsfx_table, fname)
     mgcsfxtab **mgcsfx_table;
     char       *fname;
{
  FILE *fp;
  char *s;
  int   line_number = 0;
  int   str_len;
  int   reach_end;
  int   def_err;

  char *description;
  char *mgcsfx_type;
  char *offset;
  char *magic;
  char *suffix;
  char *i_img;
  char *i_com;
  char *o_img;
  char *o_com;

  mgcsfxtab  *ent;
  mgcsfxtab **entry;


  if((fp=fopen(fname, "r"))==NULL){
    /* fprintf(stderr, "Can't open %s\n",fname); */
    return;
  }

  while(1){
retry:
    line_number++;
    def_err = 0;

    s= NULL;
    description = mgcsfx_type = offset = magic = suffix
      = i_img = i_com = o_img = o_com = NULL;
    reach_end = 0;

    if((s = fgettoken(fp, ':'))==NULL) break; /* EOF なら終り */
    if(*s == '#'){/* 先頭が '#' なら読みとばす */
      while((s = fgettoken(fp, '\n'))!=NULL){
	if(*s == '\n'){
	  free(s);
	  goto retry;
	}
	free(s);
      }
      if(s == NULL) break;
    }else if(*s == '\n'){/* 空行は無視 */
      free(s);
      goto retry;
    }
    if(strlen(s) > 0) description = s;
    else free(s);

    if((s = fgettoken(fp, ':'))==NULL || *s == '\n'){/* 何もないなら設定ミス */
      if(s != NULL) free(s);
      mgcsfx_read_error(fname, line_number, "data type");
      goto next;
    }
    if(strlen(s) > 0) mgcsfx_type = s;
    else free(s);

    if((s = fgettoken(fp, ':'))==NULL || *s == '\n'){/* 何もないなら設定ミス */
      if(s != NULL) free(s);
      mgcsfx_read_error(fname, line_number, "byte offset");
      goto next;
    }
    if(strlen(s) > 0) offset = s;
    else free(s);

    if((s = fgettoken(fp, ':'))==NULL || *s == '\n'){/* 何もないなら設定ミス */
      if(s != NULL) free(s);
      mgcsfx_read_error(fname, line_number, "magic number");
      goto next;
    }
    if(strlen(s) > 0) magic = s;
    else free(s);

    if((s = fgettoken(fp, ':'))==NULL || *s == '\n'){/* 何もないなら設定ミス */
      if(s != NULL) free(s);
      mgcsfx_read_error(fname, line_number, "suffix");
      goto next;
    }
    if(strlen(s) > 0) suffix = s;
    else free(s);

    if((s = fgettoken(fp, ':'))==NULL || *s == '\n'){/* 何もないなら設定ミス */
      if(s != NULL) free(s);
      mgcsfx_read_error(fname, line_number, "input image type");
      goto next;
    }
    if(strlen(s) > 0) i_img = s;
    else free(s);

    if((s = fgettoken(fp, ':'))==NULL || *s == '\n'){/* 何もないなら設定ミス */
      if(s != NULL) free(s);
      mgcsfx_read_error(fname, line_number, "input command");
      goto next;
    }
    if(strlen(s) > 0) i_com = s;
    else free(s);

    if((s = fgettoken(fp, ':'))==NULL || *s == '\n'){/* 何もないなら設定ミス */
      if(s != NULL) free(s);
      mgcsfx_read_error(fname, line_number, "output image type");
      goto next;
    }
    if(strlen(s) > 0) o_img = s;
    else free(s);

    if((s = fgettoken(fp, '#'))==NULL || *s == '\n'){/* 何もないなら設定ミス */
    /*
      free(s);
      mgcsfx_read_error(fname, line_number, "output command");
      goto next;
     */
      if(s != NULL){
	*s = '\0';
	reach_end = 1;
      }
    }
    if(s != NULL){
      if(strlen(s) > 0) o_com = s;
      else free(s);
    }

    if(reach_end == 0){
      while((s = fgettoken(fp, '\n'))!=NULL){/* 行末のゴミを捨てる */
	if(*s == '\n'){
	  free(s);
	  break; /* goto next; */
	}
	free(s);
      }
    }else{
      reach_end = 0;
    }



    /* --------------------------------------------------------------------- */
next:;

    if(DEBUG){
      fprintf(stderr,"Read:  file %s:  line %d.\n", fname, line_number);
      fprintf(stderr,"Description : %s\n",
	      description ? description : "-- error --");
      fprintf(stderr,"Type        : %s\n",
	      mgcsfx_type ? mgcsfx_type : "-- error --");
      fprintf(stderr,"Offset      : %s\n", offset ? offset : "--+--");
      fprintf(stderr,"Magic       : %s\n", magic ? magic : "--+--");
      fprintf(stderr,"Suffix      : %s\n", suffix ? suffix : "--+--");
      fprintf(stderr,"i Image     : %s\n", i_img ? i_img : "--+--");
      fprintf(stderr,"i Command   : %s\n", i_com ? i_com : "--+--");
      fprintf(stderr,"o Image     : %s\n", o_img ? o_img : "--+--");
      fprintf(stderr,"o Command   : %s\n", o_com ? o_com : "--+--");
      fprintf(stderr,"\n");
    }

    /* create mgcsfxtab */
    if((ent = (mgcsfxtab *) malloc (sizeof (mgcsfxtab)))==NULL){
      fprintf(stderr,"Can't allocate memory\n");
      exit(1);
    }
    ent->next              = NULL;
    ent->description       = NULL;
    ent->mgcsfx_type       = T_UNKNOWN;
    ent->offset            = 0;
    ent->string_len        = 0;
    ent->suffix            = NULL;
    ent->input_image_type  = IT_UNKNOWN;
    ent->input_command     = NULL;
    ent->output_image_type = IT_UNKNOWN;
    ent->output_command    = NULL;

    if(description != NULL){
      ent->description = description;
      description = NULL;
    }else{
      fprintf (stderr,"%s: line %d: undefined <description> field.\n",
	       fname, line_number);
      def_err ++;
      goto next2;
    }

    if(mgcsfx_type == NULL){
      fprintf (stderr,"%s: line %d: undefined <mgcsfx type> field.\n",
	       fname, line_number);
      def_err ++;
      goto next2;
    }
    ent->mgcsfx_type = type_mgcsfx(mgcsfx_type);
    switch(ent->mgcsfx_type){
    case T_SUFFIX:
      if(suffix == NULL){
	fprintf (stderr,
              "%s: line %d: conflict definition : undefined <suffix> field.\n",
		 fname, line_number);
	def_err ++;
	goto next2;
      }
      break;
    case T_BEINT16:
      if (sscanf(magic, "%hi", &(ent->dt.int16_data)) != 1){
	magic_type_error(fname, line_number, magic);
	def_err ++;
	goto next2;
      }
      break;
    case T_LEINT16:
      if (sscanf(magic, "%hi", &(ent->dt.int16_data)) != 1){
	magic_type_error(fname, line_number, magic);
	def_err ++;
	goto next2;
      }
      break;
#ifdef ARCHITECTURE64
    case T_BEINT32:
      if (sscanf(magic, "%i", &(ent->dt.int32_data)) != 1){
	magic_type_error(fname, line_number, magic);
	def_err ++;
	goto next2;
      }
      break;
    case T_LEINT32:
      if (sscanf(magic, "%i", &(ent->dt.int32_data)) != 1){
	magic_type_error(fname, line_number, magic);
	def_err ++;
	goto next2;
      }
      break;
#else
    case T_BEINT32:
      if (sscanf(magic, "%li", &(ent->dt.int32_data)) != 1){
	magic_type_error(fname, line_number, magic);
	def_err ++;
	goto next2;
      }
      break;
    case T_LEINT32:
      if (sscanf(magic, "%li", &(ent->dt.int32_data)) != 1){
	magic_type_error(fname, line_number, magic);
	def_err ++;
	goto next2;
      }
      break;
#endif /* ARCHITECTURE64 */
    case T_MAGIC:
      if(magic == NULL){
	fprintf (stderr,"%s: line %d: undefined <magic> field.\n",
		 fname, line_number);
	def_err ++;
	goto next2;
      }
      if((str_len = string_fin(magic))<=0){
	fprintf (stderr,"%s: line %d: invalid <magic> field.\n",
		 fname, line_number);
	def_err ++;
	goto next2;
      }

      ent->string_len = str_len;
      if((ent->dt.string_data = (char *)malloc(str_len + 1))==NULL){
	fprintf(stderr,"Can't allocate memory\n");
	exit(1);
      }
      memcpy(ent->dt.string_data, magic, str_len + 1);
      break;
    case T_UNKNOWN:
    default:
      fprintf (stderr,"%s: line %d: invalid <mgcsfx type> field.\n",
	       fname, line_number);
      def_err ++;
      goto next2;
      break;
    };


    if(offset == NULL){
      if(ent->mgcsfx_type == T_MAGIC ||
	 ent->mgcsfx_type == T_BEINT16 ||
	 ent->mgcsfx_type == T_LEINT16 ||
	 ent->mgcsfx_type == T_BEINT32 ||
	 ent->mgcsfx_type == T_LEINT32){
	fprintf (stderr,
	      "%s: line %d: conflict definition : undefined <offset> field.\n",
		 fname, line_number);
	def_err ++;
	goto next2;
      }
    }else{
      if(ent->mgcsfx_type != T_SUFFIX) sscanf(offset, "%i", &(ent->offset));
    }

    if(suffix != NULL){
      ent->suffix = suffix;
      suffix = NULL;
    }

    if((i_img == NULL && i_com == NULL) && (o_img == NULL || o_com == NULL)){
      fprintf (stderr,"%s: line %d: invalid definition.\n",
	       fname, line_number);
      def_err ++;
      goto next2;
    }
    if((o_img == NULL && o_com == NULL) && (i_img == NULL || i_com == NULL)){
      fprintf (stderr,"%s: line %d: invalid definition.\n",
	       fname, line_number);
      def_err ++;
      goto next2;
    }

    if(i_img != NULL && i_com != NULL){
      ent->input_image_type  = type_image(i_img);
      ent->input_command = i_com;
      i_com = NULL;
    }else{
      ent->input_image_type  = IT_UNKNOWN;
      ent->input_command = NULL;
    }

    if(o_img != NULL && o_com != NULL){
      ent->output_image_type = type_image(o_img);
      ent->output_command = o_com;
      o_com = NULL;
    }else{
      ent->output_image_type = IT_UNKNOWN;
      ent->output_command = NULL;
    }
    /* end of create mgcsfxtab */


next2:;

    if(def_err != 0 || DEBUG){
      fprintf(stderr,"Description : %s \t -> %s\n",
	      description ? description : "--+--",
	      ent->description ? ent->description : "-- error --");
      fprintf(stderr,"Type        : %s \t -> %d\n",
	      mgcsfx_type ? mgcsfx_type : "--+--",
	      ent->mgcsfx_type);
      fprintf(stderr,"Offset      : %s \t -> %d\n",
	      offset ? offset : "--+--",
	      ent->offset);

      fprintf(stderr,"Magic       : %s", magic ? magic : "--+--");
      switch(ent->mgcsfx_type){
      case T_BEINT16:
      case T_LEINT16:
	fprintf(stderr," \t -> %d\n",ent->dt.int16_data);
	break;
      case T_BEINT32:
      case T_LEINT32:
	fprintf(stderr," \t -> %ld\n",ent->dt.int32_data);
	break;
      case T_MAGIC:
	fprintf(stderr," \t -> %s\n",ent->dt.string_data);
	break;
      default:
	fprintf(stderr,"\n");
	break;
      };

      fprintf(stderr,"Suffix      : %s \t -> %s\n",
	      suffix ? suffix : "--+--",
	      ent->suffix ? ent->suffix : "--+--");
      fprintf(stderr,"i Image     : %s \t -> %d\n",
	      i_img ? i_img : "--+--",
	      ent->input_image_type);
      fprintf(stderr,"i Command   : %s \t -> %s\n",
	      i_com ? i_com : "--+--",
	      ent->input_command ? ent->input_command : "--+--");
      fprintf(stderr,"o Image     : %s \t -> %d\n",
	      o_img ? o_img : "--+--",
	      ent->output_image_type);
      fprintf(stderr,"o Command   : %s \t -> %s\n",
	      o_com ? o_com : "--+--",
	      ent->output_command ? ent->output_command : "--+--");
      fprintf(stderr,"\n");
    }

    if(description != NULL) free(description);
    if(mgcsfx_type != NULL) free(mgcsfx_type);
    if(offset      != NULL) free(offset);
    if(magic       != NULL) free(magic);
    if(suffix      != NULL) free(suffix);
    if(i_img       != NULL) free(i_img);
    if(i_com       != NULL) free(i_com);
    if(o_img       != NULL) free(o_img);
    if(o_com       != NULL) free(o_com);


    if(def_err != 0) goto next3;

    /* Override any existing entry for this magic number/file type */
    for(entry = mgcsfx_table; *entry; entry = &((*entry)->next)){
      if((ent->mgcsfx_type == (*entry)->mgcsfx_type) &&
	 (
	   ((ent->offset == (*entry)->offset) &&
	    (((ent->mgcsfx_type == T_BEINT16) &&
	      (ent->dt.int16_data == (*entry)->dt.int16_data)) ||
	     ((ent->mgcsfx_type == T_BEINT32) &&
	      (ent->dt.int32_data == (*entry)->dt.int32_data)) ||
	     ((ent->mgcsfx_type == T_LEINT16) &&
	      (ent->dt.int16_data == (*entry)->dt.int16_data)) ||
	     ((ent->mgcsfx_type == T_LEINT32) &&
	      (ent->dt.int32_data == (*entry)->dt.int32_data)) ||

	     ((ent->mgcsfx_type == T_MAGIC) &&
	      !memcmp(ent->dt.string_data, (*entry)->dt.string_data,
		      ent->string_len))
	      )) ||
	  ((ent->mgcsfx_type == T_SUFFIX) &&
	   !strcmp(ent->suffix, (*entry)->suffix))
	 )
	){

	free ((*entry)->description);
	(*entry)->description = ent->description;
	ent->description = NULL;

	(*entry)->input_image_type = ent->input_image_type;
	if ((*entry)->input_command) free ((*entry)->input_command);
	(*entry)->input_command = ent->input_command;
	ent->input_command = NULL;

	(*entry)->output_image_type = ent->output_image_type;
	if ((*entry)->output_command) free ((*entry)->output_command);
	(*entry)->output_command = ent->output_command;
	ent->output_command = NULL;

	free_mgcsfx(ent);
	break;
      }
    }
    if (!*entry){
      ent->next = NULL;
      *entry = ent;
    }

    /* if(s == NULL) break; */
next3:;
    if(def_err != 0) free_mgcsfx(ent);
  } /* end of while(1) */
}


/***************************************************/
/* マジックナンバー定義ファイル名を得て、読み込ませる */
static void init_mgcsfx ()
{
  extern char *getenv ();

  char      *home_dir;
  char       fname[1024];
  mgcsfxtab *entry;
  int        len;
  struct stat st;

#ifdef USE_MGCSFX_PREPROCESSOR
  char      *pp_fname;
#endif /* USE_MGCSFX_PREPROCESSOR */

  mgcsfx_table = NULL;

  mgcsfx_handler_setup();

  if(nomgcsfx){
    mgcsfx_setup_flag = 1;
    nitem_mgcsfx = 0;
    desc_width = 0;
  }else{
    sprintf (fname, "%s/%s", SYSCONFDIR, MGCSFX_SITE_RC);
    if(stat(fname, &st) == 0 && S_ISREG(st.st_mode)){
      /* Read the site MagicSuffix table into a linked list */
#ifdef USE_MGCSFX_PREPROCESSOR
      if((pp_fname = make_preprocessed_file(fname)) != NULL){
	read_mgcsfx (&mgcsfx_table, pp_fname);
      }
      unlink(pp_fname);
#else
      read_mgcsfx (&mgcsfx_table, fname);
#endif /* USE_MGCSFX_PREPROCESSOR */
    }

    /* Read the personal MgcSfx table into the list overriding site entries */
    if ((home_dir = getenv ("HOME"))){
      sprintf (fname, "%s/%s", home_dir, MGCSFX_RC);
      if(stat(fname, &st) == 0 && S_ISREG(st.st_mode)){
#ifdef USE_MGCSFX_PREPROCESSOR
	if((pp_fname = make_preprocessed_file(fname)) != NULL){
	  read_mgcsfx (&mgcsfx_table, pp_fname);
	}
	unlink(pp_fname);
#else
	read_mgcsfx (&mgcsfx_table, fname);
#endif /* USE_MGCSFX_PREPROCESSOR */
      }
    }

    mgcsfx_setup_flag = 1;

    nitem_mgcsfx = 0;
    desc_width = 0;
    for (entry = mgcsfx_table; entry; entry = entry->next){
      nitem_mgcsfx ++;
      len = strlen(entry->description);
      if(len > desc_width) desc_width = len;
      if(max_offset_mgcsfx < entry->offset) max_offset_mgcsfx = entry->offset;
      if(entry->mgcsfx_type == T_MAGIC &&
	 max_length_mgcsfx < entry->string_len)
	max_length_mgcsfx = entry->string_len;
    }
    if(max_length_mgcsfx == 0) max_length_mgcsfx = sizeof(int32);
    need_buf_size = max_offset_mgcsfx + max_length_mgcsfx + 1;/* 1 is safety */
  }
}

/***************************************************/
/* マジックナンバーを調べて、定義しているテーブルを検索する
   マジックナンバーのテーブルを読み込んでいないなら読み込む */
static mgcsfxtab *find_mgcsfx (fname, buffer, buffer_size)
     char           *fname;
     unsigned char  *buffer;
     int             buffer_size;
{
  mgcsfxtab *entry;
  int16      buf16;
  int32      buf32;
  char      *suf;

  if (mgcsfx_setup_flag == 0) init_mgcsfx ();

  for (entry = mgcsfx_table; entry; entry = entry->next){
    switch (entry->mgcsfx_type){
    case T_BEINT16:
      if ((buffer_size > 0) &&
	  ((entry->offset + sizeof (int16)) <= buffer_size)){
	buf16 = ((char)*(buffer + entry->offset) << 8) |
	  ((char)*(buffer + entry->offset +1));
	if(entry->dt.int16_data == buf16) return entry;
      }
      break;
    case T_LEINT16:
      if ((buffer_size > 0) &&
	  ((entry->offset + sizeof (int16)) <= buffer_size)){
	buf16 = ((char)*(buffer + entry->offset +1) << 8) |
	  ((char)*(buffer + entry->offset));
	if(entry->dt.int16_data == buf16) return entry;
      }
      break;
    case T_BEINT32:
      if ((buffer_size > 0) &&
	  ((entry->offset + sizeof (int32)) <= buffer_size)){
	buf32 = ((char)*(buffer + entry->offset) << 24) |
	  ((char)*(buffer + entry->offset +1) << 16) |
	  ((char)*(buffer + entry->offset +2) << 8) |
	  ((char)*(buffer + entry->offset +3));
	if(entry->dt.int32_data == buf32) return entry;
      }
      break;
    case T_LEINT32:
      if ((buffer_size > 0) &&
	  ((entry->offset + sizeof (int32)) <= buffer_size)){
	buf32 = ((char)*(buffer + entry->offset +3) << 24) |
	  ((char)*(buffer + entry->offset +2) << 16) |
	  ((char)*(buffer + entry->offset +1) << 8) |
	  ((char)*(buffer + entry->offset));
	if(entry->dt.int32_data == buf32) return entry;
      }
      break;
    case T_MAGIC:
      if ((buffer_size > 0) &&
	  ((entry->offset + entry->string_len)
	   <= buffer_size) &&
	  !memcmp (entry->dt.string_data, buffer + entry->offset,
		   entry->string_len ))
	return entry;
      break;
    case T_SUFFIX:
      if(fname != NULL && entry->suffix != NULL){
	if(strlen(fname) - strlen(entry->suffix) > 0){
	  suf = fname + (strlen(fname) - strlen(entry->suffix));
	  if(!strcmp(suf, entry->suffix)) return entry;
	}
      }
      break;
    case T_UNKNOWN:
    default:
      return NULL;
      break;
    }
  }
  return NULL;
}





/***************************************************/
/* マジックナンバーの定義を調べて、それにあわせたコマンドを実行する */
/* if OK return 1, else if ERROR return 0 */
int
LoadMGCSFX(file_name, pinfo)
     char    *file_name;
     PICINFO *pinfo;
{
  unsigned char *buffer;
  int            size;
  mgcsfxtab     *magic;
  mgcsfxtab     *magic_cur;
  char          *ptr;
  char           command[1024];
  int            fd[2];
  int            pid = -2;
  int            file;
  char          *fname;
  int            rv;
  int            pst;

  int            i_it;
  char          *i_com;

  WaitCursor();

  fname = file_name;
  if((file = open (fname, O_RDONLY))<0){
    SetISTR(ISTR_WARNING, "Can't open %s",fname);
    return 0;
  }

  if((buffer = (unsigned char *)calloc(need_buf_size, sizeof(char))) == NULL){
    SetISTR(ISTR_WARNING, "Can't allocate memory");
    return 0;
  }

  magic_cur = NULL;

/*  do{ */
    size = read (file, buffer, need_buf_size);

    if (lseek (file, 0L, 0) < 0){ /* can't seek pipe !! */
      fprintf (stderr, "Can't lseek %s\n", file_name);
      close(file);
      return 0;
    }

    magic = find_mgcsfx (fname, buffer, size);

    if ((magic != NULL && magic->input_command) ||
        (magic == NULL && mgcsfx && input_command_ex_flag)){

      if(magic == NULL){
	if (fname != NULL && (ptr = strstr(input_command_ex, "%s"))){
	  sprintf (command, input_command_ex, fname);
	}else{
	  strcpy (command, input_command_ex);
	  fname=NULL;
	}
      }else{
	/* Use stdin or give file name */
	if (fname != NULL && (ptr = strstr(magic->input_command, "%s"))){
	  sprintf (command, magic->input_command, fname);
	}else{
	  strcpy (command, magic->input_command);
	  fname=NULL;
	}
      }

      /* Do the pipe/fork/exec here */
      if (pipe (fd) < 0){
	fprintf (stderr, "Can't pipe : %s\n", file_name);
	close(file);
	return 0;
      }

      if ((pid = vfork ()) < 0){
	fprintf (stderr, "Can't vfork : %s\n", file_name);
	close (fd[0]);
	close (fd[1]);
	close(file);
	return 0;
      }

      if (!pid){
	 close(0);
	if (fname == NULL || (open ("/dev/null", O_RDONLY) < 0)){
	   dup(file);
	}
	 close(file);
	 close(1);
	 dup(fd[1]);
	 close(2);
	 open("/dev/null", O_WRONLY);
	 close(fd[0]);
	 execl("/bin/sh", "/bin/sh", "-c", command, 0);
	_exit(127);
      }

      close (fd[1]);
      dup2(fd[0], file);
      close (fd[0]);
      fname = NULL;
      magic_cur = magic;
    }
/*  } while(magic != NULL); */

  free(buffer);

  if(magic_cur == NULL && mgcsfx && input_command_ex_flag){
    i_it  = IT_PNM;
    i_com = input_command_ex;
  }else{
    i_it  = magic_cur->input_image_type;
    i_com = magic_cur->input_command;
  }

  if((magic_cur != NULL && i_com) ||
    (magic_cur == NULL && mgcsfx && input_command_ex_flag)){
    switch(i_it){
    case IT_PNM:
    case IT_PPM:
    case IT_PGM:
    case IT_PBM:
    case IT_PNM_RAW:
    case IT_PPM_RAW:
    case IT_PGM_RAW:
    case IT_PBM_RAW:
    case IT_PNM_ASCII:
    case IT_PPM_ASCII:
    case IT_PGM_ASCII:
    case IT_PBM_ASCII:
      rv = LoadPBM(file_name, pinfo, file);
      break;
    case IT_GIF:
    case IT_JPEG:
    case IT_TIFF:
    case IT_JFIF:
    case IT_XBM:
    case IT_XPM:
    case IT_BMP:
    case IT_SUNRAS:
    case IT_IRIS:
    case IT_XWD:
    case IT_MAG:
    case IT_MAKI:
    case IT_PI:
    case IT_PIC:
    case IT_PIC2:
      SetISTR(ISTR_WARNING, "Yet supported input image type (from filter output)");
      rv = 0;
      break;
    case IT_UNKNOWN:
      SetISTR(ISTR_WARNING, "Unknown input image type (from filter output)");
      rv = 0;
      break;
#ifdef HAVE_MGCSFX_AUTO
    case IT_AUTO:
#endif
    default:
      SetISTR(ISTR_WARNING, "Error in input image type (from filter output)");
      rv = 0;
      break;
    }
  }else{
    rv = 0;
  }

  /* fail if pid still == -2? */
  while(wait(&pst) != pid);  /* FIXME?  pid isn't necessarily initialized... */
  if( *((char *)&pst) != 0 ) rv = 0;

  input_command_ex_flag = 0;

  return rv;

  /* fclose(fp);  close in Load??? */
  /* return 0; error */
  /* return 1; ok */
}





/*--------------------------------------------------------------------------*/
#ifndef MGCSFX_DEFAULT_INPUT_COMMAND
#  define MGCSFX_DEFAULT_INPUT_COMMAND  "tifftopnm"
#endif
#ifndef MGCSFX_DEFAULT_OUTPUT_COMMAND
#  define MGCSFX_DEFAULT_OUTPUT_COMMAND "pnmtotiff"
#endif

int MSWIDE  =  0;
int MSHIGH  =  0;

#define MS_NBUTTS 2
#define MS_BOK    0
#define MS_BCANC  1
#define BUTTW    60  /* width of buttons (OK or Cancel) */
#define BUTTH    24  /* height of buttons (OK or Cancel) */
#define RBSIZE   15  /* width and height of RB button (select, ON or OFF)*/
#define CWIDE    8   /* width of character */
/* #define CHIGH        height of character      defined in xv.h */
#define MARGIN    3  /* margin of button and label     SPACING */

#define MSD_TITLE       "Save file with external command..."
#define MSD_RBTITLE     "Type of Magic and Suffix"
#define MSD_IC_TITLE    "input command"

static BUTT  msbut[MS_NBUTTS];
static RBUTT *typeRB;

static char output_command_ex[1024];
static int  output_command_ex_flag = 0;

static int   colorType;

static int   w_pid;
static int   w_pstatus;

#define MSNAMWIDE 252               /* width of 'file name' entry window */
#define MAXFNLEN 256               /* max len of filename being entered */
static char   DialogFileName[MAXFNLEN+100];   /* filename being entered */
static int    curPos, stPos, enPos;     /* filename textedit stuff */


static mgcsfxtab *get_mgcsfx PARM((int));
static void changeSuffix PARM((int));

static int WriteMGCSFX  PARM((FILE**,byte*,int,int,int,
			    byte*,byte*,byte*,int,int,char*,
			    int, int, char*));
void  CreateMGCSFXW         PARM((void));
void  MGCSFXDialog          PARM((int));
int   MGCSFXCheckEvent      PARM((XEvent *));
int   MGCSFXSaveParams      PARM((char *, int));

static void drawMSD     PARM((int,int,int,int));
static void clickMSD    PARM((int,int));
static void doCmd       PARM((int));
static int writeMGCSFX  PARM((void));

static void changeSuffix   PARM((int));
static void redrawNamMSD   PARM((void));
static void showFNamMSD    PARM((void));
static int keyinMSD        PARM((int));

int getInputCom  PARM((void));
int getOutputCom PARM((void));
/*--------------------------------------------------------------------------*/

/***************************************************/
/* どれを選んだか調べる。０はコマンドを入力するものとする */
static mgcsfxtab *get_mgcsfx(ms_type)
     int ms_type;
{
  mgcsfxtab *magic;
  int        i;

  magic = NULL;
  if(ms_type != 0){
    i = 1;
    for(magic = mgcsfx_table; (magic && i<ms_type); magic = magic->next){i++;}
  }
  return magic;
}

/***************************************************/
/* 外部コマンドを実行して、それに出力する */
/* if OK return 0, else if ERROR return -1 */
static
int WriteMGCSFX(fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,file_name,
		ms_type, file, comment)
     FILE **fp;
     byte  *pic;
     int    ptype, w,h;
     byte  *rmap, *gmap, *bmap;
     int    numcols, colorstyle;
     char  *file_name;
     int    ms_type;
     int    file; /* file descriptor */
     char  *comment;
{
  mgcsfxtab *magic;

  int        fd[2];
  int        pid;
  int        rv;

  WaitCursor();

#ifdef USE_SIGCHLD
  w_p_fail = 1;
#endif

  magic = get_mgcsfx(ms_type);
  if(ms_type != 0 && magic == NULL) return -1;

  if ((ms_type == 0 && output_command_ex_flag) ||
      (ms_type !=0 && magic != NULL && magic->output_command)){

    /* Do the pipe/fork/exec here */
    if (pipe (fd) < 0){
      fprintf (stderr, "Can't pipe : %s\n", file_name);
      return -1;
    }

    if ((pid = vfork ()) < 0){
      fprintf (stderr, "Can't vfork : %s\n", file_name);
      close (fd[0]);
      close (fd[1]);
      return -1;
    }

    if (!pid){
      close(1);
      dup(file);
      close(file);
      close(0);
      dup(fd[0]);
      close(2);
      open("/dev/null", O_WRONLY);
      close(fd[1]);
      if(ms_type == 0){
	execl("/bin/sh", "/bin/sh", "-c", output_command_ex, 0);
      }else{
	execl("/bin/sh", "/bin/sh", "-c", magic->output_command, 0);
      }
      _exit(127);
    }

    close (fd[0]);
    dup2(fd[1], file);
    close (fd[1]);

  }else{
    return -1;
  }


  *fp = fdopen(file, "w");

  /* sleep(1); Best way is wait for checking SIGCHLD, but it's feel waist.*/

#ifdef USE_SIGCHLD
  if(w_p_fail != 2){
#endif
    if(ms_type == 0){
      rv = WritePBM(*fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,
		    1, comment);
    }else{
      switch(magic -> output_image_type){
      case IT_PNM:
      case IT_PPM:
      case IT_PGM:
      case IT_PBM:
      case IT_PNM_RAW:
      case IT_PPM_RAW:
      case IT_PGM_RAW:
      case IT_PBM_RAW:
	rv = WritePBM(*fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,
		      1, comment);
	break;
      case IT_PNM_ASCII:
      case IT_PPM_ASCII:
      case IT_PGM_ASCII:
      case IT_PBM_ASCII:
	rv = WritePBM(*fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,
		      0, comment);
	break;
      case IT_GIF:
      case IT_JPEG:
      case IT_TIFF:
      case IT_JFIF:
      case IT_XBM:
      case IT_XPM:
      case IT_BMP:
      case IT_SUNRAS:
      case IT_IRIS:
      case IT_XWD:
      case IT_MAG:
      case IT_MAKI:
      case IT_PI:
      case IT_PIC:
      case IT_PIC2:
	SetISTR(ISTR_WARNING, "Yet supported output image type (to filter input)");
	rv = -1;
	break;
      case IT_UNKNOWN:
	SetISTR(ISTR_WARNING, "Unknown output image type (to filter input)");
	rv = -1;
	break;
#ifdef HAVE_MGCSFX_AUTO
      case IT_AUTO:
#endif
      default:
	SetISTR(ISTR_WARNING, "Error in output image type (to filter input)");
	rv = -1;
	break;
      }
    }
#ifdef USE_SIGCHLD
  }else{
    rv = -1;
  }
#endif

#ifdef USE_SIGCHLD
  if(w_p_fail != 2){
#endif
    w_pid = pid;
#ifdef USE_SIGCHLD
    w_p_fail = 0;
  }else{
    rv = -1;
  }
#endif

  output_command_ex_flag = 0;

  return rv;

  /* fclose(*fp);   close in CloseOutFile in writeMGCSFX */
  /* return  0; ok */
  /* return -1; error */
}

/***************************************************/
void CreateMGCSFXW()
{
  int	     y;
  int        type_num;
  mgcsfxtab *entry;

  if (mgcsfx_setup_flag == 0) init_mgcsfx ();

  if(desc_width < strlen(MSD_IC_TITLE))  desc_width = strlen(MSD_IC_TITLE);
  nitem_mgcsfx ++;

  MSWIDE = desc_width * CWIDE + RBSIZE + 36; /* 36 is start of RB button */
  MSHIGH = nitem_mgcsfx * (RBSIZE + MARGIN);

  if(MSWIDE < strlen(MSD_TITLE) + 20) MSWIDE = strlen(MSD_TITLE) + 20;
  if(MSWIDE < strlen(MSD_RBTITLE) + 16) MSWIDE = strlen(MSD_RBTITLE) + 16;
  if(MSWIDE < MSNAMWIDE + 10) MSWIDE = MSNAMWIDE + 10;
  if(MSWIDE <  BUTTW * 2 + 10) MSWIDE = BUTTW * 2 + 10;

  MSHIGH += 55 + LINEHIGH + 10 + BUTTH + 10;

  MSWIDE += 20; /* right side margin */
  MSHIGH += 10; /* RB buttun down side margin */


  mgcsfxW = CreateWindow("xv mgcsfx", "XVmgcsfx", NULL,
			 MSWIDE, MSHIGH, infofg, infobg, 0);
  if (!mgcsfxW) FatalError("can't create mgcsfx window!");

  XSelectInput(theDisp, mgcsfxW,
	       ExposureMask | ButtonPressMask | KeyPressMask);

  mgcsfxNameW = XCreateSimpleWindow(theDisp, mgcsfxW,
				    10,  MSHIGH-LINEHIGH-10-BUTTH-10-1,
				    (u_int) MSNAMWIDE+6, (u_int) LINEHIGH+5,
				    1, infofg, infobg);
  if (!mgcsfxNameW) FatalError("can't create mgcsfx name window");
  XSelectInput(theDisp, mgcsfxNameW, ExposureMask);

  /* Ok ボタン */
  BTCreate(&msbut[MS_BOK], mgcsfxW,
	   MSWIDE-BUTTW-10-BUTTW-10-1, MSHIGH-BUTTH-10-1,
	   BUTTW, BUTTH,
	   "Ok", infofg, infobg, hicol, locol);
  /* Cancel ボタン*/
  BTCreate(&msbut[MS_BCANC], mgcsfxW,
	   MSWIDE-BUTTW-10-1, MSHIGH-BUTTH-10-1,
	   BUTTW, BUTTH,
	   "Cancel", infofg, infobg, hicol, locol);

  y = 55;
  /* User should input command to exec external command */
  typeRB = RBCreate(NULL, mgcsfxW, 36, y,          MSD_IC_TITLE,
		    infofg, infobg,hicol,locol);
  y += (RBSIZE + MARGIN); /* 18 */

  type_num = 1;
  for (entry = mgcsfx_table; entry; entry = entry->next){
    RBCreate(typeRB, mgcsfxW, 36, y,            entry->description,
	     infofg, infobg,hicol,locol);
    y += (RBSIZE + MARGIN); /* 18 */
    if(entry->output_command == NULL){
      RBSetActive(typeRB, type_num, 0); /* if no command, off */
    }
    type_num++;
  }

  XMapSubwindows(theDisp, mgcsfxW);
}


/***************************************************/
void MGCSFXDialog(vis)
     int vis;
{
  if (vis) {
    CenterMapWindow(mgcsfxW, msbut[MS_BOK].x + msbut[MS_BOK].w/2,
		    msbut[MS_BOK].y + msbut[MS_BOK].h/2, MSWIDE, MSHIGH);
  }
  else     XUnmapWindow(theDisp, mgcsfxW);
  mgcsfxUp = vis;
}


/***************************************************/
int MGCSFXCheckEvent(xev)
     XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!mgcsfxUp) return (0);

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    if (e->window == mgcsfxW)       drawMSD(x, y, w, h);
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == mgcsfxW)     clickMSD(x,y);
      else rv = 0;
    }  /* button1 */
    else rv = 0;
  }  /* button press */

  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;  XComposeStatus status;
    int stlen;

    stlen = XLookupString(e,buf,128,&ks,&status);
    buf[stlen] = '\0';

    if (e->window == mgcsfxW) {
      if (stlen) {
	keyinMSD(buf[0]);
      }
    }
    else rv = 0;
  }
  else rv = 0;

  if (rv == 0 && (xev->type == ButtonPress || xev->type == KeyPress)) {
    XBell(theDisp, 50);
    rv = 1;   /* eat it */
  }

  return (rv);
}


/***************************************************/
int MGCSFXSaveParams(fname, col)
     char *fname;
     int col;
{
  colorType = col;
  strcpy(DialogFileName, GetDirFName());
  return (0);
}

/***************************************************/
/* ダイアログを表示するときの処理 */
static void drawMSD(x,y,w,h)
     int x,y,w,h;
{
  int        i;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  for (i = 0; i < MS_NBUTTS; i++) BTRedraw(&msbut[i]);

  ULineString(mgcsfxW, typeRB->x-16, typeRB->y-3-DESCENT,
	      MSD_RBTITLE);
  RBRedraw(typeRB, -1);

  DrawString(mgcsfxW, 20, 29, MSD_TITLE);

  XSetClipMask(theDisp, theGC, None);

  showFNamMSD();
}

/***************************************************/
/* ダイアログをクリックしたときの処理 */
static void clickMSD(x,y)
     int x,y;
{
  int   i;
  BUTT *bp;

  /* check BUTTs */

  /* check the RBUTTS first, since they don't DO anything */
  if ((i = RBClick(typeRB, x,y)) >= 0) { /* 選択(type)ボタンの処理 */
    (void) RBTrack(typeRB, i);  /* 選択(type)ボタンを押したとき */
    changeSuffix(i);
    return;
  }

  for (i = 0; i < MS_NBUTTS; i++) { /* Ok,Cancel ボタンの処理 */
    bp = &msbut[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h))
      break;
  }
  if (i < MS_NBUTTS)  /* found one */ /* Ok,Cancel ボタンを押したとき */
    if (BTTrack(bp)) doCmd(i);
}

/***************************************************/
/* ボタン(Ok, Cancel) の処理 */
static void doCmd(cmd)
     int cmd;
{
  int rv;

  switch (cmd) {
  case MS_BOK: /* Ok button */ {
    char *fullname;

    rv = writeMGCSFX(); /* Save with filter(MGCSFX) */
    MGCSFXDialog(0);

    fullname = GetDirFullName();
    if (!ISPIPE(fullname[0])) {
      XVCreatedFile(fullname);
      if(!rv) StickInCtrlList(0);
    }
  }
    break;
  case MS_BCANC: /* Cancel button */
    DialogFileName[0] = '\0';
    curPos = stPos = enPos = 0;
    MGCSFXDialog(0);
    break;
  default:
    break;
  }
}

/*******************************************/
static int writeMGCSFX()
{
  int   rv, type;
  int   ptype, w, h, pfree, nc;
  byte *inpix, *rmap, *gmap, *bmap;

  FILE *fp = NULL;
  int   file;
  char *fullname;

  rv = -1;
  type = RBWhich(typeRB);

  SetDirFName(DialogFileName); /* change filename in dir dialog */
  fullname = GetDirFullName();

  if(type == 0){
    if(getOutputCom() == 0) return rv;
  }

  file = OpenOutFileDesc(fullname);
  if(file < 0) return rv;

  WaitCursor();
  inpix = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rmap, &gmap, &bmap);

  rv = WriteMGCSFX(&fp, inpix, ptype, w, h,
		   rmap, gmap, bmap, nc, colorType, fullname,
		   type, file, picComments);

  SetCursors(-1);

  if (CloseOutFile(fp, fullname, rv) == 0) DirBox(0);

  WaitCursor();
#ifdef USE_SIGCHLD
  if(w_p_fail == 0){
#endif
    while(wait(&w_pstatus) != w_pid);  /* if( *((char *)&w_pstatus) != 0 ) ; */
#ifdef USE_SIGCHLD
  }else{
    w_p_fail = 0;
  }
#endif
  w_pid = 0;
  w_pstatus = 0;

  if (pfree) free(inpix);
  return rv;
}


/***************************************/
static void changeSuffix(ms_type)
     int ms_type;
{
  /* see if there's a common suffix at the end of the DialogFileName.
     if there is, remember what case it was (all caps or all lower), lop
     it off, and replace it with a new appropriate suffix, in the
     same case */

  int allcaps;
  char *suffix, *sp, *dp, lowsuf[512];
  mgcsfxtab *magic;

  /* find the last '.' in the DialogFileName */
  suffix = (char *) rindex(DialogFileName, '.');
  if (!suffix) return;
  suffix++;  /* point to first letter of the suffix */

  /* check for all-caposity */
  for (sp = suffix, allcaps=1; *sp; sp++)
    if (islower(*sp)) allcaps = 0;

  /* copy the suffix into an all-lower-case buffer */
  for (sp=suffix, dp=lowsuf; *sp; sp++, dp++) {
    *dp = (isupper(*sp)) ? tolower(*sp) : *sp;
  }
  *dp = '\0';


  magic = get_mgcsfx(ms_type);
  if(magic != NULL && magic->suffix != NULL){
    strcpy(lowsuf,(magic->suffix)+1);

    if (allcaps) {  /* upper-caseify lowsuf */
      for (sp=lowsuf; *sp; sp++)
	*sp = (islower(*sp)) ? toupper(*sp) : *sp;
    }

    /* one other case:  if the original suffix started with a single
       capital letter, make the new suffix start with a single cap */
    if (isupper(suffix[0])) lowsuf[0] = toupper(lowsuf[0]);

    strcpy(suffix, lowsuf);   /* tack onto DialogFileName */
    showFNamMSD();
  }
}

/***************************************************/
/* ダイアログ内にファイルネームを表示するときの処理 (下請け)*/
static void redrawNamMSD()
{
  int cpos;

  /* draw substring DialogFileName[stPos:enPos] and cursor */

  Draw3dRect(mgcsfxNameW, 0, 0, (u_int) MSNAMWIDE+5, (u_int) LINEHIGH+4, R3D_IN, 2,
	     hicol, locol, infobg);

  XSetForeground(theDisp, theGC, infofg);

  if (stPos>0) {  /* draw a "there's more over here" doowah */
    XDrawLine(theDisp, mgcsfxNameW, theGC, 0,0,0,LINEHIGH+5);
    XDrawLine(theDisp, mgcsfxNameW, theGC, 1,0,1,LINEHIGH+5);
    XDrawLine(theDisp, mgcsfxNameW, theGC, 2,0,2,LINEHIGH+5);
  }

  if ((size_t) enPos < strlen(DialogFileName)) {
    /* draw a "there's more over here" doowah */
    XDrawLine(theDisp, mgcsfxNameW, theGC, MSNAMWIDE+5,0,MSNAMWIDE+5,LINEHIGH+5);
    XDrawLine(theDisp, mgcsfxNameW, theGC, MSNAMWIDE+4,0,MSNAMWIDE+4,LINEHIGH+5);
    XDrawLine(theDisp, mgcsfxNameW, theGC, MSNAMWIDE+3,0,MSNAMWIDE+3,LINEHIGH+5);
  }

  XDrawString(theDisp, mgcsfxNameW, theGC,3,ASCENT+3,DialogFileName+stPos, enPos-stPos);

  cpos = XTextWidth(mfinfo, &DialogFileName[stPos], curPos-stPos);
  XDrawLine(theDisp, mgcsfxNameW, theGC, 3+cpos, 2, 3+cpos, 2+CHIGH+1);
  XDrawLine(theDisp, mgcsfxNameW, theGC, 3+cpos, 2+CHIGH+1, 5+cpos, 2+CHIGH+3);
  XDrawLine(theDisp, mgcsfxNameW, theGC, 3+cpos, 2+CHIGH+1, 1+cpos, 2+CHIGH+3);
}

/***************************************************/
/* ダイアログ内にファイルネームを表示する */
static void showFNamMSD()
{
  int len;

  len = strlen(DialogFileName);

  if (curPos<stPos) stPos = curPos;
  if (curPos>enPos) enPos = curPos;

  if (stPos>len) stPos = (len>0) ? len-1 : 0;
  if (enPos>len) enPos = (len>0) ? len-1 : 0;

  /* while substring is shorter than window, inc enPos */

  while (XTextWidth(mfinfo, &DialogFileName[stPos], enPos-stPos) < MSNAMWIDE
	 && enPos<len) { enPos++; }

  /* while substring is longer than window, dec enpos, unless enpos==curpos,
     in which case, inc stpos */

  while (XTextWidth(mfinfo, &DialogFileName[stPos], enPos-stPos) > MSNAMWIDE) {
    if (enPos != curPos) enPos--;
    else stPos++;
  }


  if (ctrlColor) XClearArea(theDisp, mgcsfxNameW, 2,2, (u_int) MSNAMWIDE+5-3,
			    (u_int) LINEHIGH+4-3, False);
  else XClearWindow(theDisp, mgcsfxNameW);

  redrawNamMSD();
  BTSetActive(&msbut[MS_BOK], strlen(DialogFileName)!=0);
}

/***************************************************/
/* キー入力したときの処理 */
static int keyinMSD(c)
     int c;
{
  /* got keypress in dirW.  stick on end of DialogFileName */
  int len;

  len = strlen(DialogFileName);

  if (c>=' ' && c<'\177') {             /* printable characters */
    /* note: only allow 'piped commands' in savemode... */

    /* only allow spaces in 'piped commands', not filenames */
    if (c==' ' && (!ISPIPE(DialogFileName[0]) || curPos==0)) return (-1);

    /* only allow vertbars in 'piped commands', not filenames */
    if (c=='|' && curPos!=0 && !ISPIPE(DialogFileName[0])) return(-1);

    if (len >= MAXFNLEN-1) return(-1);  /* max length of string */
    xvbcopy(&DialogFileName[curPos], &DialogFileName[curPos+1], (size_t) (len-curPos+1));
    DialogFileName[curPos]=c;  curPos++;
  }

  else if (c=='\010' || c=='\177') {    /* BS or DEL */
    if (curPos==0) return(-1);          /* at beginning of str */
    xvbcopy(&DialogFileName[curPos], &DialogFileName[curPos-1], (size_t) (len-curPos+1));
    curPos--;
  }

  else if (c=='\025') {                 /* ^U: clear entire line */
    DialogFileName[0] = '\0';
    curPos = 0;
  }

  else if (c=='\013') {                 /* ^K: clear to end of line */
    DialogFileName[curPos] = '\0';
  }

  else if (c=='\001') {                 /* ^A: move to beginning */
    curPos = 0;
  }

  else if (c=='\005') {                 /* ^E: move to end */
    curPos = len;
  }

  else if (c=='\004') {                 /* ^D: delete character at curPos */
    if (curPos==len) return(-1);
    xvbcopy(&DialogFileName[curPos+1], &DialogFileName[curPos], (size_t) (len-curPos));
  }

  else if (c=='\002') {                 /* ^B: move backwards char */
    if (curPos==0) return(-1);
    curPos--;
  }

  else if (c=='\006') {                 /* ^F: move forwards char */
    if (curPos==len) return(-1);
    curPos++;
  }

  else if (c=='\012' || c=='\015') {    /* CR(\r) or LF(\n) */
    FakeButtonPress(&msbut[MS_BOK]);
  }

  else if (c=='\033') {                  /* ESC = Cancel */
    FakeButtonPress(&msbut[MS_BCANC]);
  }

  else if (c=='\011') {                  /* tab = filename expansion */
    if (1 /* !autoComplete() */) XBell(theDisp, 0);
    else {
      curPos = strlen(DialogFileName);
    }
  }

  else return(-1);                      /* unhandled character */

  showFNamMSD();

  return(0);
}


/*******************************************/
int getInputCom()
{
  static const char *labels[] = { "\nOk", "\033Cancel" };
  int                i;

  strcpy(input_command_ex, MGCSFX_DEFAULT_INPUT_COMMAND);
  i = GetStrPopUp("Input External Command (Input is PNM):", labels, 2,
		  input_command_ex, 1024, "",0);
  if (i == 0 && strlen(input_command_ex) != 0){
    input_command_ex_flag = 1;
    return 1;
  }else{
    input_command_ex_flag = 0;
    return 0;
  }
}

int getOutputCom()
{
  static const char *labels[] = { "\nOk", "\033Cancel" };
  int                i;

  strcpy(output_command_ex, MGCSFX_DEFAULT_OUTPUT_COMMAND);
  i = GetStrPopUp("Input External Command (Output is PNM_RAW):", labels, 2,
		  output_command_ex, 1024, "",0);
  if (i == 0 && strlen(output_command_ex) != 0){
    output_command_ex_flag = 1;
    return 1;
  }else{
    output_command_ex_flag = 0;
    return 0;
  }
}

#ifdef SVR4
Sigfunc *
xv_signal(signo, func)
     int signo;
     Sigfunc *func;
{
  struct sigaction act, oact;

  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_flags |= SA_RESTART;

  if (sigaction(signo, &act, &oact) < 0)
    return SIG_ERR;

  return oact.sa_handler;
}
#endif

#endif /* HAVE_MGCSFX */
