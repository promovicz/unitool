
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unicode/uchar.h>
#include <unicode/ustdio.h>

typedef int (*command_cb_t)(int argc, char **argv);

struct command {
  char *name;
  char *help;
  command_cb_t handler;
};

typedef struct command command_t;

UChar *u_stringFromUTF8(const char *inbuf, int32_t inlen) {
  UErrorCode ue;
  UChar *res = NULL;
  int32_t len = 0;
  /* default input length */
  if(inlen == -1) {
    inlen = strlen(inbuf);
  }
  /* determine length */
  ue = U_ZERO_ERROR;
  u_strFromUTF8(NULL, 0, &len, inbuf, inlen, &ue);
  if(U_FAILURE(ue) && ue != U_BUFFER_OVERFLOW_ERROR) {
    fprintf(stderr,
	    "Failed to estimate string length: %s\n",
	    u_errorName(ue));
    return NULL;
  }
  /* space for null termination */
  len = len + 1;
  /* allocate memory */
  res = calloc(len, sizeof(UChar));
  /* perform actual conversion */
  ue = U_ZERO_ERROR;
  u_strFromUTF8(res, len, NULL, inbuf, inlen, &ue);
  if(U_FAILURE(ue)) {
    fprintf(stderr,
	    "Failed to convert string: %s\n",
	    u_errorName(ue));
    free(res);
    return NULL;
  }
  /* return result */
  return res;
}

static int cmd_dump(int argc, char **argv) {
  UFILE *ui;
  UChar32 ch;
  char chname[256];
  UErrorCode ue = U_ZERO_ERROR;
  /* open input */
  ui = u_finit(stdin, NULL, NULL);
  if(!ui) {
    puts("Failed to open input");
    abort();
  }
  /* read uchar32 */
  while ((ch = u_fgetcx(ui)) != U_EOF) {
    /* get char name */
    u_charName(ch, U_UNICODE_CHAR_NAME, chname, sizeof(chname), &ue);
    if(U_FAILURE(ue)) {
      snprintf(chname, sizeof(chname), "Error: %s", u_errorName(ue));
    }
    /* format dump line */
    if(ch <= 0xFFFF) {
      printf("U+%04x %s\n", (uint32_t)ch, chname);
    } else {
      printf("U+%06x %s\n", (uint32_t)ch, chname);
    }
  }
  /* close input */
  u_fclose(ui);
  /* done */
  return 0;
}

struct infoprop {
  char *name;
  UProperty prop;
};

static struct infoprop info_props_b[] = {
  {"Deprecated", UCHAR_DEPRECATED},

  {"Alphabetic", UCHAR_ALPHABETIC},
  {"Ideographic", UCHAR_IDEOGRAPHIC},
  {"Cased", UCHAR_CASED},
  {"Lowercase", UCHAR_LOWERCASE},
  {"Uppercase", UCHAR_UPPERCASE},
  {"Diacritic", UCHAR_DIACRITIC},
  {"Extender", UCHAR_EXTENDER},
  {"Radical", UCHAR_RADICAL},

  {"Dash", UCHAR_DASH},
  {"Quotation mark", UCHAR_QUOTATION_MARK},
  {"Sentence terminal", UCHAR_S_TERM},
  {"White space", UCHAR_WHITE_SPACE},

  {"Math", UCHAR_MATH},

  {"Regional indicator", UCHAR_REGIONAL_INDICATOR},

  {"Emoji", UCHAR_EMOJI},
  {"Emoji component", UCHAR_EMOJI_COMPONENT},
  {"Emoji modifier", UCHAR_EMOJI_MODIFIER},
  {"Emoji modifier base", UCHAR_EMOJI_MODIFIER_BASE},
  {"Emoji presentation", UCHAR_EMOJI_PRESENTATION},

  {"Default ignorable", UCHAR_DEFAULT_IGNORABLE_CODE_POINT},

  {NULL, 0},
};

static int cmd_info(int argc, char **argv) {
  UFILE *ui;
  UChar32 ch;
  UErrorCode ue = U_ZERO_ERROR;
  int pc;
  struct infoprop *pp;
  /* open unicode input */
  ui = u_finit(stdin, NULL, NULL);
  if(!ui) {
    puts("Failed to open input");
    abort();
  }
  /* read single uchar32 */
  while ((ch = u_fgetcx(ui)) != U_EOF) {
    char chcode[16];
    char chname[256];
    UBlockCode chblock;
    /* format char code */
    if(ch <= 0xFFFF) {
      snprintf(chcode, sizeof(chcode), "U+%04x", ch);
    } else {
      snprintf(chcode, sizeof(chcode), "U+%06x", ch);
    }
    /* get char name */
    u_charName(ch, U_UNICODE_CHAR_NAME, chname, sizeof(chname), &ue);
    if(U_FAILURE(ue)) {
      snprintf(chname, sizeof(chname), "Error: %s", u_errorName(ue));
    }
    /* determine block of char */
    chblock = ublock_getCode(ch);
    /* print header */
    printf("%s %s\n", chcode, chname);
    /* print flags */
    pc = 0;
    pp = &info_props_b[0];
    while(pp->name) {
      if(u_hasBinaryProperty(ch, pp->prop)) {
	if(pc > 0) {
	  printf(", ");
	} else {
	  printf("  Flags: ");
	}
	printf("%s", pp->name);
	pc++;
      }
      pp++;
    }
    if(pc > 0) {
      printf("\n");
    }
    /* separating newline */
    printf("\n");
  }
  /* close input */
  u_fclose(ui);
  /* done */
  return 0;
}

static int cmd_transform(int argc, char **argv) {
  int ret = 1;
  UErrorCode ue = U_ZERO_ERROR;
  UParseError upe;
  UChar *utspec = NULL;
  UTransliterator *ut;
  UFILE *ui, *uo;
  UChar uc;
  /* need at least one argument */
  if(argc < 2) {
    fputs("Usage: %s <transform>", stderr);
    goto err_args;
  }
  /* convert transform spec to UTF-8 */
  utspec = u_stringFromUTF8(argv[1], -1);
  if(!utspec) {
    fputs("Could not convert transform spec", stderr);
    goto err_args;
  }
  /* build the transform */
  ut = utrans_openU(utspec, -1, UTRANS_FORWARD,
		    NULL, -1, &upe, &ue);
  if (U_FAILURE(ue)) {
    fprintf(stderr, "utrans_open(%s): %s\n",
	    argv[1], u_errorName(ue));
    goto err_stdin;
  }
  /* establish input and output */
  uo = u_get_stdout();
  if (!(ui = u_finit(stdin, NULL, NULL))) {
    fputs("Error opening stdin as UFILE\n", stderr);
    goto err_stdin;
  }
  /* set up transform on output stream */
  u_fsettransliterator(uo, U_WRITE, ut, &ue);
  if (U_FAILURE(ue)) {
    fprintf(stderr,
	    "Failed to set transliterator on stdout: %s\n",
	    u_errorName(ue));
    goto err_other;
  }
  /* perform transformation */
  while ((uc = u_fgetcx(ui)) != U_EOF) {
    u_fputc(uc, uo);
  }
  /* flush output */
  u_fflush(uo);
  /* disable transform on output stream */
  u_fsettransliterator(uo, U_WRITE, NULL, &ue);
  if (U_FAILURE(ue)) {
    fprintf(stderr,
	    "Failed to set transliterator on stdout: %s\n",
	    u_errorName(ue));
    goto err_other;
  }
  /* success */
  ret = 0;
  /* done */
 err_other:
  /* close output */
  u_fclose(ui);
 err_stdin:
  /* free the spec */
  free(utspec);
 err_args:
  /* return result */
  return ret;
}

command_t commands[] = {
{ "dump", "Show terse information about characters.", cmd_dump },
{ "info", "Show detailed information about characters.", cmd_info },
{ "transform", "Apply an ICU transform.", cmd_transform },
{ NULL, NULL, NULL },
};

int main(int argc, char **argv) {
  char *cmd;
  command_t *ci;
  /* need at least one argument */
  if(argc < 2) {
    fprintf(stderr, "Usage: %s <command>\n", argv[0]);
    return 1;
  }
  /* dispatch appropriate command */
  cmd = argv[1];
  ci = &commands[0];
  while(ci->name) {
    if(!strcmp(ci->name, cmd)) {
      return ci->handler(argc - 1, argv + 1);
    }
    ci++;
  }
  /* unknown command */
  fprintf(stderr, "Unknown command \"%s\"\n", cmd);
  return 1;
}
