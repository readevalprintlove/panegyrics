#define VERSION "2.06 (9th September 1996)"

/* 3col.c
 * (c) 1995 Gareth McCaughan
 * You may distribute this file freely, as long as it remains unchanged.
 * If you want to do anything else with it, e-mail gjm11@pmms.cam.ac.uk.
 *
 * This program converts a text file or files to PostScript which will print
 * several columns to the page (3, by default), and optionally interprets
 * some simple mark-up codes. Highly flexible.
 *
 * Call as     3col [options] [filenames]
 * Output goes to stdout.
 *
 * Some relevant facts about A4 paper:
 *   It's 11.75 by 8.25 inches, near enough.
 *   That works out as 846 by 594 points.
 *   A typical printer doesn't cope well with the 1/4 inch around the edges.
 *
 * The program actually copes with paper that isn't A4 too. But A4 is typical.
 *
 * We use the paper in landscape format.
 *
 * Horizontally the arrangement is
 * mgap COL-1 cgap COL-2 ... cgap COL-n mgap
 * where mgap,cgap are the "margin gap" and "centre gap".
 * In the middle of each cgap is a vertical line.
 *
 * Vertically, the arrangement is
 *   mgap
 *   TITLE-BAR
 *   mgap
 *   TEXT
 *   mgap.
 * The title bar contains the title and page number.
 *
 * The text itself is output in a condensed Courier type. (Courier looks
 * really horrible unless it's condensed a bit; condensing it makes it look
 * merely quite horrible.)
 *
 * Almost everything in sight is configurable.
 */

/* Some standard header files, and a declaration that sometimes isn't there.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern double strtod(const char *, char **);

/* ============================= Configurables ============================= */

/*****************************************************************************
**                                                                          **
**  The following sections detail things that the user can change,          **
**  either by command-line options or by entries in a configuration file.   **
**                                                                          **
*****************************************************************************/


/* --------------------------------- Paper --------------------------------- */

/* We can cope with various different sizes of paper.
 * Here's all the information we need for each paper type.
 */
typedef struct Paper_desc {
  char *name;
  double Xsize;		/* in points */
  double Ysize;		/* in points */
  double margin;	/* in points. Unusable amount at each edge */
  int rotated;		/* do we need to rotate through 90 degrees? */
  struct Paper_desc *next;	/* for linking them together */
} Paper_desc;

/* Here, for instance, is information about A4 and A5 paper:
 */
static Paper_desc paper_A5p = { "A5-portrait", 297,423,18, 0, 0 };
static Paper_desc paper_A4p = { "A4-portrait", 594,846,18, 0, &paper_A5p };
static Paper_desc paper_A5  = { "A5", 423,297,18, 1, &paper_A4p };
static Paper_desc paper_A4  = { "A4", 846,594,18, 1, &paper_A5 };

/* The start of the list:
 */
static Paper_desc *paper_descs=&paper_A4;

/* The actual paper we're using is in this variable:
 */
static Paper_desc paper_desc= { "A4", 846,594,18, 1, &paper_A5 };


/* --------------------------------- Fonts --------------------------------- */

/* Similarly, we can vary the main output font.
 * Here's the information we need for one of those.
 */
typedef struct Font_desc {
  char *normal;		/* this & 3 following are names of fonts */
  char *bold;
  char *italic;
  char *bolditalic;
  double aspect;	/* recommended x/y shrinkage, as a PERCENTAGE */
  double width;		/* width of character as fraction of point size,
			 * uncondensed. (The font MUST be monospaced.) */
  struct Font_desc *next;	/* for linking them together */
} Font_desc;

/* Here is suitable information for the default (Courier).
 */
Font_desc font_Courier = {
  "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
  0.9, 0.6,
  0
};

/* The start of the list:
 */
static Font_desc *font_descs=&font_Courier;

/* And the font we're really using:
 */
static Font_desc font_desc = {
  "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
  90, 0.6,
  0
};


/* ------------------------------ Dimensions ------------------------------ */

/* The gap sizes are variable, of course.
 */
static double mgap=20;
static double cgap=24;

/* Nominal font size:
 */
static double font_size=5;

/* Line spacing, as a fraction (usually >=1) of nominal font size:
 */
static double leading=1;

/* So are some things about the title bar:
 */
static double title_height=33;
static double title_grey=0.8;	/* background. 0 = black, 1 = white */
static double title_rule=1.5;	/* width of surrounding rule */
static char *title_font="Helvetica-Bold";

/* And some other things:
 */
static double divider_width=0.4;	/* thickness of rule dividing columns */
static double divider_grey=0;		/* 0 = black, 1 = white */


/* --------------------------------- Flags --------------------------------- */

/* What should we do with form-feeds?
 */
enum {
  ignore=0,	/* Ignore them altogether, */
  as_newline=1,	/* treat them exactly as newlines, */
  new_column=2,	/* start a new column, */
  new_page=3	/* or start a new page? */
};
static int ff_behaviour=new_column;

/* Should we display page numbers in the title bar? (Why on earth not?)
 */
static int show_page_numbers=1;

/* Should we display page numbers as "2 of 23"?
 * *Not* doing this means we don't have to scan the file twice.
 */
static int show_n_pages=1;

/* Should we try to grok our special mark-up commands?
 */
static int mark_up=0;

/* Should we truncate lines when they get too long? (Nononononooo...)
 */
static int truncating=0;

/* Should we display line numbers in the margin?
 * If so, at what interval?
 * And should line numbers be continuous across files?
 * And what font should we use?
 */
static int show_line_numbers=0;
static int line_number_interval=10;
static int line_number_continuously=0;
static char *line_number_font="Times-Italic";
static double line_number_font_size=4;

/* If we have more than one input file, what should we do between files?
 */
static int new_file_action=new_column;	/* see |enum| above */

/* And should we display the name of the new file?
 * If so, in what font?
 * And how many lines of output should it take up?
 */
static int new_file_title=0;
static char *file_name_font="Times-Bold";
static double file_name_font_size=9;
static int file_name_skip_lines=3;

/* How many characters per tab position?
 */
static int tab_width=8;

/* How many columns should we use?
 */
static int n_columns=3;

/* Should we convert fonts to ISO-Latin-1?
 */
static int latinise=0;

/* Should we print the date and time of printout?
 * If so, in what font?
 */
static int show_date=1;
static char *date_font="Times-Roman";
static double date_font_size=6;


/* ----------------------------- Miscellaneous ----------------------------- */

static char *title=0;

/* Format for date, suitable for input to strftime().
 */
static char date_format[256]="Printed %d %b %Y";


/* ============================= Consequences ============================= */

/*****************************************************************************
**                                                                          **
**  The following sections are concerned with consequences of those         **
**  configuration options.                                                  **
**                                                                          **
*****************************************************************************/


/* ------------------------------ Dimensions ------------------------------ */

static double char_width;	/* actual width of one character */
static double line_spacing;	/* actual distance between baselines */

static int chars_per_line;	/* number of characters per line */
static int lines_per_col;	/* number of lines per column */

static double col_width;	/* total width of one column */
static double col_text_width;	/* width of text portion of one column */
static double col1_left;	/* xpos of start of text in first column */

static double col_bottom;	/* bottom of each column */
static double col_top;		/* top of each column */

static double title_bar_bottom;
static double title_bar_top;
static double title_bar_left;
static double title_bar_right;

static double title_start_x;	/* start of title text */
static double title_start_y;

static double title_font_size;

static double pageno_end_x;	/* RH end of page number */
#define pageno_end_y title_start_y


/* -------------------------------- Others -------------------------------- */

static char the_date[256];


/* ======================== Other global variables ======================== */

static int n_pages;	/* total number of pages in the whole printout */
static char *user_name;

static int err_status=0;	/* becomes non-0 if an error happens */

/* Are we really producing output at the moment, or are we just
 * trying to see how big the output will be?
 */
static int for_real;

/* We accumulate characters to be output into |current_line|.
 * When there are enough that we need a new line, or when we
 * encounter a markup directive, we spit out the relevant
 * characters and start accumulating again.
 * At any given moment |current_pos| is the number of characters
 * that would be on the current line if we were to emit all the
 * contents of |current_line|. Note that there may in fact be
 * things on the current line that aren't in |current_ine|, if
 * there have been formatting directives.
 * Since one character of output may actually require more than one
 * character in |current_line|, we also need |next_char|, which points
 * to the character after the last valid one in |current_line|.
 */
static char *current_line;
static int current_pos;
static char *next_char;

/* Where are we in the output?
 */
static int line_num;	/* in column: 0..n-1 */
static int col_num;	/* on page: 1..m */
static int page_num;	/* 1.. */

/* Where are we in the input file?
 */
static int input_line_num;	/* 0.. */

/* Indeed, what *is* the input file?
 */
static FILE *input_file=0;

/* Which output font are we using?
 * (Bit 0 governs boldness, bit 1 governs italicity.)
 */
static int output_font;

/* Are we outputting underlined characters?
 */
static int underlining;


/* ========================== Utility functions ========================== */

/*****************************************************************************
**                                                                          **
**  The following sections contain some useful functions which are used     **
**  in many places in the program. They might as well go here.              **
**                                                                          **
*****************************************************************************/


/* --------------------------------- Errors --------------------------------- */

/* Report an error, but don't actually die.
 */
static void error(char *s, ...) {
  va_list ap;
  va_start(ap,s);
  fprintf(stderr,"! ");
  vfprintf(stderr,s,ap);
  va_end(ap);
  fprintf(stderr,".\n");
  err_status=1;
}

/* Report a fatal error, and die.
 */
static void fatal(char *s, ...) {
  va_list ap;
  va_start(ap,s);
  fprintf(stderr,"!!! ");
  vfprintf(stderr,s,ap);
  va_end(ap);
  fprintf(stderr,".\n\nI'm afraid that was a fatal error. Bye.\n");
  exit(1);
}


/* -------------------------- Memory allocation -------------------------- */

/* Allocate |n| bytes of memory and return a pointer thereto;
 * fall over and die if this doesn't work out.
 */
static void *xmalloc(size_t n, char *what_for) {
  void *p=malloc(n);
  if (!p) fatal("Out of memory: needed %d bytes for %s",n,what_for);
  return p;
}


/* -------------------------------- Strings -------------------------------- */

/* Copy a string into a newly allocated piece of memory.
 */
static char *copy_string(const char *s) {
  int l=strlen(s)+1;
  char *t=xmalloc(l,"a copied string");
  memcpy(t,s,l);
  return t;
}

/* Append two strings, making a new string.
 */
static char *append_string(const char *s, const char *t) {
  int m=strlen(s);
  int n=strlen(t)+1;
  char *u=xmalloc(m+n,"a constructed string");
  memcpy(u,s,m);
  memcpy(u+m,s,n);
  return u;
}

/* Compare two strings, ignoring case differences.
 * We only require that this return 0 iff they compare equal.
 */
static int stricmp(const char *s, const char *t) {
  while (*s) if (tolower(*s++)!=tolower(*t++)) return 1;
  return *t;	/* equal iff it's 0, you see. Sorry. */
}

/* Compare two strings, treating them as config file keywords.
 * This means that space, _, - are treated as identical,
 * as well as case being ignored.
 */
static int strccmp(const char *s, const char *t) {
  char a,b;
  while ((a=*s++)!=0) {
    b=*t++;
    if (tolower(a)==tolower(b)) continue;
    if (a==' ' || a=='-') a='_';
    if (b==' ' || b=='-') b='_';
    if (a!=b) return 1;
  }
  return *t;
}

/* Emit the string |s|, escaping characters in it that are special
 * to PostScript. Don't emit the surrounding '(' and ')'.
 * Don't try to be clever about tabs and things -- we'll deal
 * with that later.
 */
static void emit_string(const char *s) {
  char c;
  while ((c=*s++)!=0) {
    if (c=='(' || c==')' || c=='\\') putchar('\\');
    putchar(c);
  }
}

/* ============================= Config files ============================= */

/*****************************************************************************
**                                                                          **
**  The following sections are concerned with reading in the config files.  **
**                                                                          **
*****************************************************************************/


/* ------------------------------- Locations ------------------------------- */

#ifndef GLOBAL_CONFIG_FILE
#define GLOBAL_CONFIG_FILE 0
#endif

#ifndef USER_CONFIG_FILE
#define USER_CONFIG_FILE 0
#endif

/* ------------------------------- Variables ------------------------------- */

static int config_line_no=0;
static const char *config_file_name="<unknown>";


/* --------------------------------- Errors --------------------------------- */

/* Report an error, together with the file name and line number.
 */
static void config_err(char *s, ...) {
  va_list ap;
  va_start(ap,s);
  if (config_line_no)
    fprintf(stderr,"%s (line %d): ",config_file_name,config_line_no);
  else fprintf(stderr,"<command line>: ");
  vfprintf(stderr,s,ap);
  va_end(ap);
  fprintf(stderr,".\n");
}


/* ------------------------ Parsing one config line ------------------------ */

/* A typical line of a config file looks like this:
 *    Paper: A4
 * or
 *    Paper_def: A4 846 594 18 yes
 * When we see one of these, we call a suitable function, having set up
 * the arguments in a suitable place.
 */
typedef struct {
  char *name;	/* "Paper" or "Paper_def" or ... */
  int n_args;	/* 1 or 5 or ... */
  char *desc;	/* what types the arguments have */
  void (*func)(void *);	/* function to call with the relevant args */
  void *p;	/* sometimes used... */
} Config_option;

/* The values for these functions get put in the following
 * arrays:
 */
static double double_vals[8];
static int int_vals[8];
static char * str_vals[8];

/* The next few functions are for setting single variables of known
 * types.
 */

static void c_double(void *place) {
  *((double *)place) = double_vals[0];
}

static void c_integer(void *place) {
  *((int *)place) = int_vals[0];
}

static void c_boolean(void *place) {
  char *s=str_vals[0];
  char *t;
  int n;
  if (!stricmp(s,"yes")
      || !stricmp(s,"true")
      || !stricmp(s,"on")) { *((int *)place)=1; return; }
  if (!stricmp(s,"no")
      || !stricmp(s,"false")
      || !stricmp(s,"off")) { *((int *)place)=0; return; }
  n=(int)strtol(s,&t,10);
  if (*t) config_err("I expected to find a boolean value, but found `%s'",s);
  else *((int*)place)=n;
}

static void c_string256(void *place) {
  char *s=str_vals[0];
  int l=strlen(s)+1;
  if (l>256) config_err("`%s' is too long (max. 255 chars)",s);
  memcpy((char *)place,s,l);
}

/* The next few functions are for dealing with particular sorts
 * of config line.
 */

/* Parse a Paper_def item:
 * Paper_def: <name> <X-size> <Y-size> <margin> <rotated?>
 */
static void c_paper_def(void *place) {
  Paper_desc *p;
  place=place;	/* pacify compiler */
  p=xmalloc(sizeof(Paper_desc),"a paper-type descriptor");
  p->name=copy_string(str_vals[0]);
  p->Xsize=double_vals[0];
  p->Ysize=double_vals[1];
  p->margin=double_vals[2];
  if (!stricmp(str_vals[1],"Yes")) p->rotated=1;
  else if (!stricmp(str_vals[1],"No")) p->rotated=0;
  else {
    config_err("I don't know whether `%s' means rotated or non-rotated",
               str_vals[1]);
    p->rotated=(p->Xsize>p->Ysize);
  }
  p->next=paper_descs;
  paper_descs=p;
}

/* Parse a Paper item.
 */
static void c_paper(void *place) {
  Paper_desc *p;
  place=place;	/* pacify compiler */
  p=paper_descs;
  while (p) {
    if (!strccmp(str_vals[0],p->name)) { paper_desc=*p; return; }
    p=p->next;
  }
  config_err("I don't know what `%s' paper is",str_vals[0]);
}

/* Parse a Font_def item:
 * Font_def <name> <bold> <italic> <bolditalic> <aspect> <width>
 */
static void c_font_def(void *place) {
  Font_desc *f;
  place=place;	/* pacify compiler */
  f=xmalloc(sizeof(Font_desc),"a font descriptor");
  f->normal=copy_string(str_vals[0]);
  f->bold=copy_string(str_vals[1]);
  f->italic=copy_string(str_vals[2]);
  f->bolditalic=copy_string(str_vals[3]);
  f->aspect=double_vals[0];
  f->width=double_vals[1];
  f->next=font_descs;
  font_descs=f;
}

/* Parse a Font item.
 * If it refers to a font that doesn't exist, make a guess at the names
 * of the bold & italic variants, and at all the other stuff.
 */
static void c_font(void *place) {
  Font_desc *f;
  place=place;	/* pacify compiler */
  f=font_descs;
  while (f) {
    if (!strccmp(str_vals[0],f->normal)) { font_desc=*f; return; }
    f=f->next;
  }
  config_err("I've never heard of a font called `%s'",str_vals[0]);
  f=xmalloc(sizeof(Font_desc),"a font descriptor");
  f->normal=copy_string(str_vals[0]);
  f->bold=append_string(str_vals[0],"-Bold");
  f->italic=append_string(str_vals[0],"-Oblique");
  f->bolditalic=append_string(str_vals[0],"-BoldOblique");
  f->aspect=90; f->width=0.6;
  f->next=font_descs;
  font_descs=f;
  font_desc=*f;
}

/* Parse a Title_font item.
 * This *doesn't* take a size.
 */
static void c_title_font(void *place) {
  place=place;	/* pacify compiler */
  title_font=copy_string(str_vals[0]);
}

/* Parse a Form_feed item:
 * allowable values are "Ignore", "As-Newline", "New-Column", "New-Page".
 */
static void c_form_feed(void *place) {
  place=place;	/* pacify compiler */
  if (!strccmp(str_vals[0],"Ignore")) { ff_behaviour=ignore; return; }
  if (!strccmp(str_vals[0],"As_newline")) { ff_behaviour=as_newline; return; }
  if (!strccmp(str_vals[0],"New_column")) { ff_behaviour=new_column; return; }
  if (!strccmp(str_vals[0],"New_page")) { ff_behaviour=new_page; return; }
  config_err("I don't know what `%s' means for form-feeds",str_vals[0]);
}

/* Parse a Page_numbers item:
 * allowable values are "No[ne]", "Yes", "NofM"
 */
static void c_page_numbers(void *place) {
  place=place;	/* pacify compiler */
  if (!strccmp(str_vals[0],"None")) { show_page_numbers=0; return; }
  if (!strccmp(str_vals[0],"No")) { show_page_numbers=0; return; }
  if (!strccmp(str_vals[0],"Yes")) {
    show_page_numbers=1; show_n_pages=0; return; }
  if (!strccmp(str_vals[0],"NofM")) {
    show_page_numbers=1; show_n_pages=1; return; }
  config_err("I don't know what `%s' means for page numbers",str_vals[0]);
}

/* Parse a LN_font item: font-name, then size.
 */
static void c_ln_font(void *place) {
  place=place;	/* pacify compiler */
  line_number_font=copy_string(str_vals[0]);
  line_number_font_size=double_vals[0];
}

/* Parse a Date_font item: font-name, then size.
 */
static void c_date_font(void *place) {
  place=place;	/* pacify compiler */
  date_font=copy_string(str_vals[0]);
  date_font_size=double_vals[0];
}

/* Parse a New_file item:
 * allowable values are "New-Column" and "New-Page".
 */
static void c_new_file(void *place) {
  place=place;	/* pacify compiler */
  if (!strccmp(str_vals[0],"Ignore")) {
    new_file_action=ignore; return; }
  if (!strccmp(str_vals[0],"As_newline")) {
    new_file_action=as_newline; return; }
  if (!strccmp(str_vals[0],"New_column")) {
    new_file_action=new_column; return; }
  if (!strccmp(str_vals[0],"New_page")) {
    new_file_action=new_page; return; }
  config_err("I don't know what `%s' means for new files",str_vals[0]);
}

/* Parse a New_file_font item: font name, then size.
 */
static void c_nf_font(void *place) {
  place=place;	/* pacify compiler */
  file_name_font=copy_string(str_vals[0]);
  file_name_font_size=double_vals[0];
}

/* The options we understand.
 * The meaning of all this stuff should be obvious by now.
 */
static Config_option config_options[] = {
  { "Paper_def",     5, "SDDDS",   &c_paper_def,    0 },
  { "Paper",         1, "S",       &c_paper,        0 },
  { "XSize",         1, "D",       &c_double,       &paper_desc.Xsize },
  { "YSize",         1, "D",       &c_double,       &paper_desc.Ysize },
  { "Margin",        1, "D",       &c_double,       &paper_desc.margin },
  { "Font_def",      6, "SSSSDD",  &c_font_def,     0 },
  { "Font",          1, "S",       &c_font,         0 },
  { "Size",          1, "D",       &c_double,       &font_size },
  { "Condense",      1, "D",       &c_double,       &font_desc.aspect },
  { "Leading",       1, "D",       &c_double,       &leading },
  { "MGap",          1, "D",       &c_double,       &mgap },
  { "CGap",          1, "D",       &c_double,       &cgap },
  { "Title_height",  1, "D",       &c_double,       &title_height },
  { "Title_grey",    1, "D",       &c_double,       &title_grey },
  { "Title_rule",    1, "D",       &c_double,       &title_rule },
  { "Title_font",    1, "S",       &c_title_font,   0 },
  { "Divider_width", 1, "D",       &c_double,       &divider_width },
  { "Divider_grey",  1, "D",       &c_double,       &divider_grey },
  { "Form_feed",     1, "S",       &c_form_feed,    0 },
  { "Page_numbers",  1, "S",       &c_page_numbers, 0 },
  { "Mark_up",       1, "S",       &c_boolean,      &mark_up },
  { "Truncate",      1, "S",       &c_boolean,      &truncating },
  { "Line_numbers",  1, "S",       &c_boolean,      &show_line_numbers },
  { "LN_interval",   1, "I",       &c_integer,      &line_number_interval },
  { "LN_ctsly",      1, "S",       &c_boolean,      &line_number_continuously },
  { "LN_font",       2, "SD",      &c_ln_font,      0 },
  { "New_file",      1, "S",       &c_new_file,     0 },
  { "New_file_title",1, "S",       &c_boolean,      &new_file_title },
  { "New_file_font", 2, "SD",      &c_nf_font,      0 },
  { "New_file_skip", 1, "I",       &c_integer,      &file_name_skip_lines },
  { "Tab_width",     1, "I",       &c_integer,      &tab_width },
  { "Columns",       1, "I",       &c_integer,      &n_columns },
  { "ISO_Latin_1",   1, "S",       &c_boolean,      &latinise },
  { "Date",          1, "S",       &c_boolean,      &show_date },
  { "Date_format",   1, "S",       &c_string256,    &date_format },
  { "Date_font",     2, "SD",      &c_date_font,    0 },
  { 0,               0, 0,         0,               0 }
};

/* A config line has the form <key>: <values> -- deal with it.
 */
static void parse_config_item(const char *key, char *values) {
  Config_option *t=config_options;
  int i;
  char *cp=values;
  int n_doubles=0, n_ints=0, n_strs=0;
  while (*cp && isspace(*cp)) ++cp;
  while (t->name) {
    /* name n_args desc func p */
    if (!strccmp(t->name,key)) {
      for (i=0;i<t->n_args;++i) {
        switch(t->desc[i]) {
          case 'S':
            str_vals[n_strs++]=cp;
            while (*cp && !isspace(*cp)) ++cp;
            if (*cp) *cp++=0;
            break;
          case 'D':
            double_vals[n_doubles++]=strtod(cp,&cp);
            break;
          case 'I':
            int_vals[n_ints++]=(int)strtol(cp,&cp,10);
            break;
          default: fatal("Gareth screwed up with config item `%s'",t->name);
        }
        while (*cp && isspace(*cp)) ++cp;
      }
      if (*cp) config_err("Extra stuff on line: `%s'",cp);
      (t->func)(t->p);
      return;
    }
    ++t;
  }
  config_err("I don't recognise `%s'",key);
}

/* Actually deal with a raw config line.
 */

static void parse_config_line(char *line) {
  char *cp;
  /* skip whitespace */
  while (*line && isspace(*line)) ++line;
  /* ignore comments */
  if (!*line || *line=='#') return;
  cp=strchr(line,':'); if (!cp) cp=strchr(line,'=');
  if (!cp) {
    config_err("Config line with no colon: `%s'",line);
    return;
  }
  *cp=0;
  parse_config_item(line,cp+1);
}


/* --------------------- Processing the config files --------------------- */

/* Read a line from file |f|. The line may actually extend over several
 * physical lines of the config file: lines ending with "\" followed by
 * only whitespace are treated in this way.
 * Return the address of a buffer containing the line, or 0 for EOF.
 * If the last line in the file ends with "\", 3col is at liberty to
 * misbehave.
 * This character is to help Emacs with its syntax colouring: --> " <--
 */
char *get_line(FILE *f) {
  static char *buf=0;
  static buf_size=0;
  int spare;
  int offset;
  int l;
  if (!buf) { buf=xmalloc(1024,"an input buffer"); buf_size=1024; }
  offset=0; spare=buf_size;
again:
  ++config_line_no;
  if (!fgets(buf+offset,spare,f)) return 0;
  while ((l=strlen(buf+offset))==spare-1) {
    buf=realloc(buf,buf_size*=2); spare+=buf_size;
    if (!buf) { fprintf(stderr,"! Out of memory.\n"); exit(1); }
    offset+=l; spare-=l;
    fgets(buf+offset,spare,f);
  }
  while (--l>=0 && isspace(buf[offset+l])) ;
  if (l>=0 && buf[offset+l]=='\\') {
    buf[offset+l]=' '; offset+=l+1; spare-=l+1;
    goto again;
  }
  return buf;
}

/* Try to open the config file named |name|; if we can't then return
 * quietly (it probably just doesn't exist); if we can, then go through
 * procesing all its lines. If |name==0| then return quietly.
 */
static void process_config_file(const char *name) {
  FILE *f;
  char *l;
  if (!name) return;
  f=fopen(name,"r");
  if (!f) return;
  config_file_name=name; config_line_no=0;
  while ((l=get_line(f))!=0) parse_config_line(l);
  config_file_name="<unknown>";
}


/* Try to read in first the global config file, and then the user's
 * config file.
 */
static void do_config_files(void) {
  char *t;
  t=getenv("3COL_GLOBAL_CONFIG");
  process_config_file(t ? t : GLOBAL_CONFIG_FILE);
  t=getenv("3COL_CONFIG");
  process_config_file(t ? t : USER_CONFIG_FILE);
}


/* =========================== The command line =========================== */

/*****************************************************************************
**                                                                          **
**  The following sections are concerned with parsing the command line.     **
**  That is, with options and with filenames.                               **
**                                                                          **
*****************************************************************************/


/* ------------------------------- Wildcards ------------------------------- */

/* Under Unix, filenames arrive with wildcards expanded.
 * On many other systems, however, this isn't the case:
 * we need to expand them ourselves.
 * On such a system, you might want to compile this with
 * NEED_EXPANSION defined.
 */
#ifdef NEED_EXPANSION
extern void expand_args(int *argcp, char ***argvp);
#else
#define expand_args(x,y)
#endif


/* ------------------------------ Filenames ------------------------------ */

/* Now seems like as good a time as any to set up variables for the
 * input filenames.
 */
static int n_input_files;
static char **input_filenames;

/* For reasons that will shortly become apparent, we also need a temporary
 * file sometimes. Here's where we put its name; the value here is non-0
 * iff we actually did need a temporary file.
 */
static char *tempfile_name;

/* Here's how to make a temporary file name:
 */
static void make_tempfile(void) {
  tempfile_name=copy_string(tmpnam(0));
}

/* --------------------------- The command line --------------------------- */

/* Go through the command line arguments.
 * Anything beginning with a '-' is treated as an option, EXCEPT
 * that a dash on its own means "read from stdin". What we actually
 * do, since we sometimes need to scan the file twice, is to create
 * a temporary file and copy stdin to that.
 * If '-' appears several times, we use the SAME file each time:
 * thus we only read stdin once.
 */
#define Next2 ++argv; --argc; continue
static void do_command_line(int argc, char *argv[]) {
  char *s;
  int i;
  Config_option *t;
  int n_doubles, n_ints, n_strs;
  config_file_name="(command line)"; config_line_no=0;
  expand_args(&argc,&argv);
  input_filenames=xmalloc(argc*sizeof(char*),"input filenames");
  while (++argv,--argc>0) {
    s=argv[0];
    if (s[0]=='-' && s[1]==0) {
      if (!tempfile_name) make_tempfile();
      input_filenames[n_input_files++]=tempfile_name;
      continue; }
    if (s[0]!='-') {
      input_filenames[n_input_files++]=s;
      continue; }
    ++s;
    /* Deal with special options, mostly for back-compatibility */
    if (!stricmp(s,"title")) { title=argv[1]; Next2; }
    if (!stricmp(s,"number")) {
      show_line_numbers=1; line_number_interval=atoi(argv[1]); Next2; }
    if (!stricmp(s,"ignore-FF")) { ff_behaviour=as_newline; continue; }
    if (!stricmp(s,"fname-font")) { file_name_font=argv[1]; Next2; }
    if (!stricmp(s,"fname-size")) { file_name_font_size=atof(argv[1]); Next2; }
    if (!stricmp(s,"fname-skip")) { file_name_skip_lines=atoi(argv[1]); Next2; }
    if (!stricmp(s,"truncate")) { truncating=1; continue; }
    if (!stricmp(s,"notruncate")) { truncating=0; continue; }
    if (!stricmp(s,"format")) { mark_up=1; continue; }
    if (!stricmp(s,"noformat")) { mark_up=0; continue; }
    if (!stricmp(s,"latin1")) { latinise=1; continue; }
    if (!stricmp(s,"help") || !stricmp(s,"h") || !stricmp(s,"?")) {
      fprintf(stderr,"3col, version " VERSION ".\n");
      fprintf(stderr,"Useful options:  -title <string>  -size <points>"
                     "  -condense <percent>\n");
      fprintf(stderr,"-number <interval>  -format  -paper <name>"
                     "  -columns <n>\n");
      fprintf(stderr,"For other options, see the documentation in " DOCS
                     ".\n");
      exit(0);
    }
    /* Deal with regular options; i.e., ones from the config list */
    t=config_options;
    n_doubles=0; n_ints=0; n_strs=0;
    while (t->name) {
      if (!strccmp(s,t->name)) {
        if (argc<=t->n_args) {
          error("Not enough args for option `%s': ignoring it",t->name);
          goto done; }
        for (i=1;i<=t->n_args;++i) {
          switch(t->desc[i-1]) {
            case 'S':
              str_vals[n_strs++]=argv[i];
              break;
            case 'D':
              double_vals[n_doubles++]=atof(argv[i]);
              break;
            case 'I':
              int_vals[n_ints++]=atoi(argv[i]);
              break;
            default:
              fatal("Gareth screwed up with config item `%s'",t->name);
          }
        }
        (t->func)(t->p);
        argv+=t->n_args; argc-=t->n_args;
        t=0; break;
      }
      ++t;
    }
    if (t) error("Unknown option `%s'",s);
  }
done:
  if (!n_input_files) {
    /* No files given? Read from stdin, of course. */
    make_tempfile();
    input_filenames[n_input_files++]=tempfile_name;
  }
}


/* ============================= Consequences ============================= */

/*****************************************************************************
**                                                                          **
**  The following sections are concerned with consequences of those         **
**  configuration options: working them out, this time, not just naming     **
**  the variables.                                                          **
**                                                                          **
*****************************************************************************/


/* ------------------------------ Dimensions ------------------------------ */

/* Work out dimensions whose values depend on the config options we
 * were given.
 */
static void grok_dimensions(void) {
  double col_height;
  if (mgap<paper_desc.margin) mgap=paper_desc.margin;
  col1_left=mgap;
  col_width=(paper_desc.Xsize-2*col1_left+cgap)/n_columns;
  col_text_width=col_width-cgap;
  col_height=paper_desc.Ysize-3*mgap-title_height;
  col_bottom=mgap;
  col_top=col_bottom+col_height;
  title_bar_bottom=col_top+mgap;
  title_bar_top=title_bar_bottom+title_height;
  title_bar_left=mgap;
  title_bar_right=paper_desc.Xsize-title_bar_left;
  title_font_size=title_height/1.6;
  title_start_x=title_bar_left+mgap;
  pageno_end_x=title_bar_right-mgap;
  title_start_y=title_bar_bottom+title_height*.27;
  char_width=font_size*font_desc.aspect*font_desc.width/100;
  line_spacing=font_size*leading;
  chars_per_line=(int)(col_text_width/char_width);
  lines_per_col=(int)(col_height/line_spacing);
  if (chars_per_line<10 || lines_per_col<10)
    fatal("Silly sizes: you only get %d lines of %d characters per column",
          lines_per_col,chars_per_line);
  fprintf(stderr,"%dx%d characters per column.\n",chars_per_line,lines_per_col);
}


/* ------------------------------ Other stuff ------------------------------ */

/* Invent a suitable title, if necessary.
 */
static void grok_title(void) {
  if (title) return;
  if (n_input_files==1) {
    if (input_filenames[0]==tempfile_name) title="<standard input>";
    else title=input_filenames[0]; }
  else {
    int l=strlen(input_filenames[0])+1;
    title=xmalloc(l+40,"the title");
    sprintf(title,"%s and %d other file%s",
      input_filenames[0],n_input_files-1,n_input_files==2?"":"s");
  }
}

/* Put the current time and date, in the requested format,
 * into |the_date|. This won't be called if |show_date==0|.
 */
static void grok_date(void) {
  time_t the_time;
  int l;
  time(&the_time);
  l=strftime(the_date, 256, date_format, localtime(&the_time));
  if (!l || l>=256) {
    fprintf(stderr,"! Date is too long (256 characters maximum)\n");
    show_date=0;
  }
}

/* Work out everything we can on the basis of the
 * config options etc.
 * This includes setting up that temporary file if necessary.
 */
static void grok_things(void) {
  grok_dimensions();
  grok_title();
  if (show_date) grok_date();
  if (tempfile_name) {
    char *b=xmalloc(BUFSIZ,"a buffer for copying");
    int n;
    FILE *f=fopen(tempfile_name,"w");
    if (!f) fatal("I couldn't make a temporary file I needed");
    while ((n=fread(b,1,BUFSIZ,stdin))>0) fwrite(b,1,n,f);
    if (ferror(f))
      error("Something went wrong copying stdin to a temporary file.");
    fclose(f);
  }
  current_line=xmalloc(chars_per_line*2+1,"the current-line buffer");
  next_char=current_line;
  user_name=getenv("USER");
  if (!user_name) user_name="<unknown>";
  if (tab_width<1) tab_width=1;
}


/* ============================= The prologue ============================= */

/*****************************************************************************
**                                                                          **
**  The following sections are concerned with the PostScript prologue that  **
**  goes at the start of the output file.                                   **
**                                                                          **
*****************************************************************************/


/* ----------------------------- Little bits ----------------------------- */

/* Emit the initial DSC comments.
 */
static void prologue_DSC(void) {
  printf("%%!PS-Adobe-2.0\n");
  printf("%%%%Title: %s\n",title);
  if (show_n_pages) printf("%%%%Pages: %d\n",n_pages);
  else printf("%%%%Pages: (atend)\n");
  printf("%%%%PageOrder: Ascend\n");
  if (paper_desc.rotated) printf("%%%%Orientation: Landscape\n");
  else printf("%%%%Orientation: Portrait\n");
  printf("%%%%EndComments\n\n");
  printf("%%%%BeginProlog\n\n");
}

/* Emit definition of <ff>, which is the same as <findfont> if |latinise==0|,
 * and does the necessary conversions if |latinise!=0|.
 */
static void prologue_findfont(void) {
  if (!latinise) { printf("/ff { findfont } bind def\n"); return; }
  printf(
"/ISO-8859-1-encoding [\n"
"\n"
" /ring /circumflex /tilde /dotlessi\n"
" /.notdef /.notdef /.notdef /.notdef\n"
" /.notdef /.notdef /.notdef /.notdef\n"
" /.notdef /.notdef /.notdef /.notdef\n"
" /.notdef /.notdef /.notdef /.notdef\n"
" /.notdef /.notdef /.notdef /.notdef\n"
" /.notdef /.notdef /.notdef /.notdef\n"
" /.notdef /.notdef /.notdef /.notdef\n"
"\n"
" /space /exclam /quotedbl /numbersign /dollar\n"
" /percent /ampersand /quotesingle /parenleft /parenright\n"
" /asterisk /plus /comma /hyphen /period /slash\n"
" /zero /one /two /three /four /five /six /seven /eight /nine\n"
" /colon /semicolon /less /equal /greater /question\n"
"\n"
" /at/A/B/C/D/E/F/G/H/I/J/K/L/M/N/O/P/Q/R/S/T/U/V/W/X/Y/Z\n"
" /bracketleft /backslash /bracketright /asciicircum /underscore\n"
" /grave/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z\n"
" /braceleft /bar /braceright /asciitilde /.notdef\n"
"\n"
" %% The following chunk is not strictly speaking ISO-Latin-1.\n"
" %% The characters here are sometimes useful, though, and as\n"
" %% they happen to correspond to the extensions to Latin-1 used\n"
" %% by my machine at home, I'm keeping them :-)\n"
" /.notdef /Wcircumflex /wcircumflex /.notdef\n"
" /.notdef /Ycircumflex /ycircumflex\n"
" /special1 /special2 /special3 /special4 /special5\n"
" /ellipsis /trademark /perthousand /bullet\n"
" /quoteleft /quoteright /guilsinglleft /guilsinglright\n"
" /quotedblleft /quotedblright /quotedblbase\n"
" /endash /emdash /minus\n"
" /OE /oe /dagger /daggerdbl /fi /fl\n"
"\n"
" /space /exclamdown /cent /sterling /currency /yen /brokenbar /section\n"
" /dieresis /copyright /ordfeminine /guillemotleft /logicalnot /hyphen\n"
" /registered /macron /degree /plusminus /twosuperior /threesuperior\n"
" /acute /mu /paragraph /periodcentered /cedilla /onesuperior /ordmasculine\n"
" /guillemotright /onequarter /onehalf /threequarters /questiondown\n"
"\n"
" /Agrave /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n"
" /Egrave /Eacute /Ecircumflex /Edieresis\n"
" /Igrave /Iacute /Icircumflex /Idieresis\n"
" /Eth /Ntilde\n"
" /Ograve /Oacute /Ocircumflex /Otilde /Odieresis\n"
" /multiply\n"
" /Oslash\n"
" /Ugrave /Uacute /Ucircumflex /Udieresis\n"
" /Yacute /Thorn /germandbls\n"
"\n"
" /agrave /aacute /acircumflex /atilde /adieresis /aring /ae /ccedilla\n"
" /egrave /eacute /ecircumflex /edieresis\n"
" /igrave /iacute /icircumflex /idieresis\n"
" /eth /ntilde\n"
" /ograve /oacute /ocircumflex /otilde /odieresis\n"
" /divide\n"
" /oslash\n"
" /ugrave /uacute /ucircumflex /udieresis\n"
" /yacute /thorn /ydieresis\n"
"\n"
"] def\n"
"\n"
"/ff {\n"
"  /foo exch findfont dup maxlength 1 add dict begin\n"
"    %% Install characters\n"
"    { 1 index /FID ne {def} {pop pop} ifelse } forall\n"
"    %% Install encoding\n"
"    /Encoding ISO-8859-1-encoding def\n"
/* It would be nice to deal with non-existent chars here,
 * but there seems to be no standardisation between Decoding
 * and CharStrings. Or am I stupid?
 */
"  currentdict end\n"
"  definefont\n"
"} bind def\n"
"\n"
);
}

/* Emit definitions of operators we need.
 * Non-obvious ones:
 *   <f0..f3> are fonts: normal, bold, italic, bold-italic
 *   <F0..F3> set the font to f0..f3
 *   <ti> is the font for the title bar
 *   <fn> is the font for filenames
 *   <del> moves left by one character
 *   <col1..colN> move to the start of columns
 *   <l> displays a single line and moves on to the next.
 *   <shu> is like <show>, but underlines what it displays.
 *   <bar> displays a double bar in the LH margin to indicate an overrun.
 */
static void prologue_procset(void) {
  int i;
  printf("%%%%BeginProcSet: 3col 2.0 1\n");
  printf("%% Fonts:\n");
  prologue_findfont();
  printf("/sf { [%lg 0 0 %lg 0 0] makefont } bind def\n",
         font_size*font_desc.aspect/100,font_size);
  printf("/f0 /%s ff sf def /F0 { f0 setfont } bind def\n",font_desc.normal);
  printf("/f1 /%s ff sf def /F1 { f1 setfont } bind def\n",font_desc.bold);
  printf("/f2 /%s ff sf def /F2 { f2 setfont } bind def\n",font_desc.italic);
  printf("/f3 /%s ff sf def /F3 { f3 setfont } bind def\n",font_desc.bolditalic);
  printf("/fn /%s ff %lg scalefont def\n",file_name_font,file_name_font_size);
  printf("/ti /%s ff %lg scalefont def\n",title_font,title_font_size);
  printf("/lf /%s ff %lg scalefont def\n",line_number_font,line_number_font_size);
  if (show_date)
    printf("/df /%s ff %lg scalefont def\n",date_font,date_font_size);
  printf("%% Other things:\n");
  printf("/mt {moveto} bind def /s {show} bind def /rmt {rmoveto} bind def\n");
  printf("/sw {stringwidth} bind def /st {stroke} bind def /np {newpath} bind def\n");
  printf("/slw {setlinewidth} bind def /sg {setgray} bind def\n");
  printf("/del { %lg 0 rmoveto } bind def\n",-char_width);
  printf("/xym { x y moveto } bind def\n");
  for (i=0;i<n_columns;++i)
    printf("/col%d { /x %lg def /y %lg def xym } bind def\n",
           i+1,col1_left+i*col_width,col_top-line_spacing);
  printf("/l { show /y y %lg sub def xym } bind def\n",line_spacing);
  printf("/nl { /y y %lg sub def xym } bind def\n",line_spacing);
  printf("/shu { dup show length dup %lg mul 0 rmoveto -1 1 { pop (_) show } for } bind def\n",
         -char_width);
  printf("/lu { shu /y y %lg sub def xym } bind def\n",line_spacing);
  printf("/nlu { /nl } bind def\n");
  printf("/bar { 0.4 setlinewidth x 2 sub y %lg add mt 0 %lg rlineto stroke\n",
         line_spacing*.5,line_spacing);
  printf("                        x 3 sub y %lg add mt 0 %lg rlineto stroke\n",
         line_spacing*.5,line_spacing);
  printf("                        xym } bind def\n");
  printf("/rbar { 0.8 setlinewidth x %lg add y mt 0 %lg rlineto stroke\n",
         col_text_width+2,line_spacing);
  printf("        xym } bind def\n");
  printf("/lnum { /cf currentfont def lf setfont\n");
  printf("        dup stringwidth pop neg %lg rmoveto show\n",line_spacing);
  printf("        xym cf setfont } bind def\n");
  printf("%% The newpage operator -- (1 of 3) newpage :\n");
  printf(
"/newpage {\n"
"  dup ( of) search pop print pop pop (...) print flush\n"
"  /cf currentfont def\n"
"  currentscreen 3 -1 roll 2 mul 3 1 roll setscreen\n"	/* Que? */
);
  if (paper_desc.rotated) printf("  %lg 0 translate [0 1 -1 0 0 0] concat\n",
                                 paper_desc.Ysize);
  /* Column-separating rules */
  printf("  %lg setlinewidth %lg setgray newpath\n",divider_width,divider_grey);
  for (i=1;i<n_columns;++i)
    printf("  %lg %lg mt 0 %lg rlineto\n",
           col1_left-cgap/2+i*col_width,col_bottom,col_top-col_bottom);
  printf("  stroke\n");
  /* Title bar */
  printf("  %lg setlinewidth 0 setgray newpath\n",title_rule);
  printf("  %lg %lg mt %lg %lg lineto %lg %lg lineto %lg %lg lineto closepath\n",
         title_bar_left,title_bar_bottom, title_bar_right,title_bar_bottom,
         title_bar_right,title_bar_top, title_bar_left,title_bar_top);
  printf("  gsave %lg setgray fill grestore stroke newpath\n",title_grey);
  printf("  ti setfont %lg %lg mt (",title_start_x,title_start_y);
  emit_string(title); printf(") show\n");
  if (show_page_numbers) {
    if (!show_n_pages) printf("  ( of) search pop 3 1 roll pop pop\n");
    printf("  dup stringwidth pop %lg exch sub %lg mt show\n",pageno_end_x,pageno_end_y); }
  else printf("  pop\n");
  /* Date, if necessary */
  if (show_date) {
    printf("  df setfont ("); emit_string(the_date); printf(") dup stringwidth pop\n");
    printf("  %lg exch sub %lg moveto show\n",
           title_bar_right,title_bar_bottom-date_font_size);
  }
  printf("  cf setfont\n");
  printf("} bind def\n");
  printf("%%%%EndProcSet\n");
}

/* Emit the rest of the prologue.
 */
static void prologue_end(void) {
  printf("%%%%EndProlog\n\n");
  if (show_n_pages)
    printf("(Output from 3COL, user %s, total %d pages...\n) print flush\n",
           user_name,n_pages);
  else
    printf("(Output from 3COL, user %s...\n) print flush\n",user_name);
  printf("\n%%%%Page: 1 1\n");
  printf("save\n");
}


/* -------------------------- Putting it together -------------------------- */

/* Output the whole prologue.
 */
static void emit_prologue(void) {
  prologue_DSC();
  prologue_procset();
  prologue_end();
}


/* ============================= The trailer ============================= */

/*****************************************************************************
**                                                                          **
**  The following sections are concerned with the PostScript trailer that   **
**  goes at the end of the output file.                                     **
**                                                                          **
*****************************************************************************/

/* Emit the trailer.
 */
static void emit_trailer(void) {
  printf("\n%%%%Trailer\n");
  if (!show_n_pages) printf("%%%%Pages: %d\n",n_pages);
  printf("(done.\n) print flush\n");
  printf("%%%%EOF\n");
}


/* ============================ The main loop ============================ */

/*****************************************************************************
**                                                                          **
**  The following sections contain the code that does the real work:        **
**  reading the input and writing the output.                               **
**                                                                          **
*****************************************************************************/


/* -------------------- New line, new column, new page -------------------- */

/* Start a new page. When this is called, the line buffer is always
 * empty.
 */
static void newpage(void) {
  line_num=0; col_num=1; ++page_num;
  if (for_real) {
    if (page_num>1)
      printf("restore showpage\n\n%%%%Page: %d %d\nsave ",page_num,page_num);
    if (show_n_pages) printf("(%d of %d) newpage\n",page_num,n_pages);
    else printf("(%d of \?\?) newpage\n",page_num);
    printf("col1 F%d\n",output_font);
  }
}

/* Start a new column. When this is called, the line buffer is always
 * empty.
 */
static void newcol(void) {
  if (col_num>=n_columns) { newpage(); return; }
  line_num=0; col_num++;
  if (for_real) printf("col%d\n",col_num);
}

/* Send the contents of |current_line| to the output file.
 * The argument signifies:
 *   if 0, that we shouldn't start a new line;
 *   if 1, that we should start a new line;
 *   if 2, that we should start a new line and mark it as overrun.
 */
static void flush_line(int why) {
  *next_char=0;
  if (for_real && *current_line) {
    putchar('(');
    emit_string(current_line);
    putchar(')'); putchar(' ');
  }
  next_char=current_line;
  switch(why) {
    case 0:
      if (for_real && *current_line) printf("s%s\n",underlining?"hu":"");
      break;
    case 1:
      if (for_real) printf(*current_line ? "l%s\n" : "nl%s\n",
                           underlining?"u":"");
      if (for_real && show_line_numbers && line_number_interval
          && !(input_line_num%line_number_interval))
        printf("(%d ) lnum\n",input_line_num);
      current_pos=0; if (++line_num>=lines_per_col) newcol();
      break;
    case 2:
      if (for_real) printf(*current_line ? "l%s bar\n" : "nl%s bar\n",
                           underlining?"u":"");
      current_pos=0; if (++line_num>=lines_per_col) newcol();
      break;
  }
}


/* ------------------------- Various useful things ------------------------- */

/* We just read a tab character. Insert the right number of spaces
 * into the output buffer, doing the right thing if we overrun.
 * NB that underlined tabs that overrun are only underlined as of
 * the start of the following line. Tough.
 */
static void do_tab(void) {
  int n=current_pos+tab_width-(current_pos%tab_width);
  if (n>chars_per_line) { n-=chars_per_line; flush_line(2); }
  n-=current_pos;
  while (n-->0) { *next_char++=' '; ++current_pos; }
}

/* Make sure there are at least |n| lines left in this column,
 * unless |n| is more than the number of lines per column (in
 * which case, just start a new column anyway).
 * When this happens, we have always just done a |flush_line|.
 */
static void ensure_lines(int n) {
  if (line_num+n>lines_per_col) newcol();
}

/* Skip over |n| lines (i.e., fill them with blanks).
 */
static void skip_lines(int n) {
  while (line_num+n>lines_per_col) { newcol(); n-=lines_per_col; }
  if (for_real) printf("/y y %lg sub def xym\n",n*line_spacing);
  line_num+=n;
}


/* -------------------------------- Mark-up -------------------------------- */

/* Read a string from the input file, putting it in newly-allocated
 * storage. (So it's safe to free it when it's finished with.)
 * The string stops at the next white-space character.
 */
static char *read_string(void) {
  int c;
  char *s;
  int n=0;
  int l=256;
  while ((c=getc(input_file))!=EOF && isspace(c)) {
    if (c=='\n') { ungetc(c,input_file); return copy_string(""); }
  }
  s=xmalloc(257,"a string");
  s[n++]=c;
  while ((c=getc(input_file))!=EOF && !isspace(c)) {
    if (n>=l) {
      s=realloc(s,(l<<=1)+1);
      if (!s) fatal("Out of memory, expanding a string");
    }
    s[n++]=c;
  }
  if (c=='\n') ungetc(c,input_file);
  s[n]=0;
  return s;
}

/* Read a double-precision number from the input file.
 */
static double read_double(void) {
  int c;
  char s[256];
  int n=0;
  double x;
  char *cp;
  while ((c=getc(input_file))!=EOF && isspace(c)) {
    if (c=='\n') { ungetc(c,input_file); return 0; }
  }
  s[n++]=c;
  while ((c=getc(input_file))!=EOF && !isspace(c)) {
    if (n<255) s[n++]=c;
  }
  if (c=='\n') ungetc(c,input_file);
  s[n]=0;
  x=strtod(s,&cp);
  if (*cp) {
    error("Dodgy number in mark-up directive: `%s'",s);
    return 0;
  }
  return x;
}

/* Read an integer from the input file.
 */
static int read_int(void) {
  int c;
  char s[256];
  int n=0;
  int x;
  char *cp;
  while ((c=getc(input_file))!=EOF && isspace(c)) {
    if (c=='\n') { ungetc(c,input_file); return 0; }
  }
  s[n++]=c;
  while ((c=getc(input_file))!=EOF && !isspace(c)) {
    if (n<255) s[n++]=c;
  }
  if (c=='\n') ungetc(c,input_file);
  s[n]=0;
  x=(int)strtol(s,&cp,10);
  if (*cp) {
    error("Dodgy number in mark-up directive: `%s'",s);
    return 0;
  }
  return x;
}

/* Interpret the mark-up character |c|. We have just done |flush_line(0)|.
 * We understand the following markup codes:
 *   %% is a percent sign
 *   %B, %I, %U toggle bold, italic, underline
 *   %T <font> <size> <n> means:
 *     output the following line in font <font> (which should
 *     be known to your printer), at size <size> points, and allow
 *     <n> lines of text for it.
 *   %C, %R are the same but centred and right-justified.
 *   %t, %c, %r are the same but take two extra parameters, the
 *     first and last+1 columns in which to left/centre/right-justify
 *     the title. The extra parameters got at the START.
 *   %N <n> starts a new column if there are fewer than <n> lines
 *     left in this one. Incidentally, the title directives do this too.
 *   %H <l> <r> <t> produces a horizontal line across the column, starting
 *     at the LH end of character number <l> and ending at the LH end of
 *     character number <r>. These need not be integers. The line is
 *     clipped to the edges of the column. The thickness of the line is
 *     <t> points.
 *   %P <n> begins an embedded PostScript object.
 *     It is terminated by a single blank line. <n> lines are
 *     reserved for it (and a new column is begun if there aren't
 *     that many already). Be careful. NB lines longer than 254 chars
 *     may cause confusion here.
 *   %x, for any other character x, produces undefined results, which
 *     may include destroying your computer.
 * Case is significant.
 */
static void do_markup(char c) {
  double p,q;
  int i,j;
  int x0=0,x1=chars_per_line;
  char *s;
  switch(c) {
    case 'B': output_font^=1; if (for_real) printf("F%d ",output_font); break;
    case 'I': output_font^=2; if (for_real) printf("F%d ",output_font); break;
    case 'U': underlining=!underlining; break;
    case 'N': ensure_lines(read_int()); break;
    case 'H':
      p=read_double(); q=read_double();
      if (for_real) {
        if (p<0) p=0; else if (p>chars_per_line) p=chars_per_line;
        if (q<0) q=0; else if (q>chars_per_line) q=chars_per_line;
        printf("gsave %lg slw 0 sg ",read_double());
        printf("np xym %lg %lg rmoveto ",p*char_width,font_size/2);
        printf("%lg 0 rlineto st grestore\n",(q-p)*char_width); }
      else (void)read_double();
      break;
    case 't': case 'r': case 'c':
      x0=read_int(); x1=read_int();
    case 'T': case 'R': case 'C':
      if (current_pos) flush_line(1);
      s=read_string(); p=read_double(); i=read_int();
      while ((j=getc(input_file))!=EOF && j!='\n') ;
      ensure_lines(i);
      skip_lines(i-1);
      if (for_real) {
        if (x0) printf("%lg 0 rmoveto\n",x0*char_width);
        printf("/%s ff %lg scalefont setfont\n(",s,p);
        while ((j=getc(input_file))!=EOF && j!='\n') {
          if (j=='(' || j==')' || j=='\\') putchar('\\');
          putchar(j);
        }
        switch(c) {
          case 'T': case 't': printf(") s\n"); break;
          case 'R': case 'r': printf(") dup sw pop %lg exch sub 0 rmoveto s\n",
                           (x1-x0)*char_width); break;
          case 'C': case 'c': printf(") dup sw pop 2 div %lg exch sub 0 rmoveto s\n",
                           (x1-x0)*char_width/2); break;
        }
      }
      else
        while ((j=getc(input_file))!=EOF && j!='\n') ;
      if (i) skip_lines(1);
      if (for_real) printf("F%d\n",output_font);
      free(s);
      break;
    case 'P': {
      char buf[256];
      i=read_int();
      ensure_lines(i);
      if (for_real) printf("gsave %% EMBEDDED OBJECT BEGINS\n");
      while ((j=getc(input_file))!=EOF && j!='\n') ;
      while (fgets(buf,256,input_file)) {
        if (!buf[1]) break;
        if (for_real) printf("%s",buf);
      }
      if (for_real) printf("grestore %% EMBEDDED OBJECT ENDS\n");
      if (i) skip_lines(i);
      break; }
    default:
      error("Unknown mark-up directive: %%%c",c);
  }
}

/* ----------------------------- The real work ----------------------------- */

/* Go through all the input files, doing all the work.
 */
static void process_files(void) {
  int i;
  int c;
  page_num=0; current_pos=0; next_char=current_line;
  newpage();
  for (i=0;i<n_input_files;++i) {
    output_font=0;
    underlining=0;
    if (for_real) printf("F0\n");
    if (i) {
      switch(new_file_action) {
        case ignore: break;
        case as_newline: flush_line(1); break;
        case new_column: if (line_num) newcol(); break;
        case new_page: if (line_num || col_num>1) newpage(); break;
      }
    }
    if (n_input_files>1 && new_file_title) {
      ensure_lines(file_name_skip_lines);
      if (for_real) {
        printf("fn setfont (");
        if (input_filenames[i]==tempfile_name) emit_string("<stdin>");
        else emit_string(input_filenames[i]);
        printf(") show xym F%d\n",output_font);
      }
      skip_lines(file_name_skip_lines);
    }
    input_file=fopen(input_filenames[i],"r");
    if (!input_file) {
      error("I couldn't open the file `%s'",input_filenames[i]);
      continue;
    }
    input_line_num=0;
    while ((c=getc(input_file))!=EOF) {
      switch(c) {
        case '\n': ++input_line_num; flush_line(1); break;
        case '\t': do_tab(); break;
        case '\b':
          if (current_pos) {
            flush_line(0);
            if (for_real) { printf("del "); --current_pos; }
          }
          else error("\\b at start of line -- ignoring it");
          break;
        case '\f': switch(ff_behaviour) {
          case ignore: break;
          case as_newline: flush_line(1); break;
          case new_column: flush_line(1); if (line_num) newcol(); break;
          case new_page:
            flush_line(1); if (line_num || col_num>1) newpage(); break; }
          break;
        case '\r':
          flush_line(1);
          if (for_real)
            printf("/y y %lg add def xym\n",line_spacing);
          --line_num;
          break;
        case '%':
          if (!mark_up) goto def;
          c=getc(input_file);
          if (c==EOF) {
            error("Markup character at end of file");
            c='%'; goto def; }
          if (c=='%') goto def;
          flush_line(0);
          do_markup(c);
          break;
        default:
def:      if (current_pos>=chars_per_line) {
            if (truncating) {
              flush_line(1);
              if (for_real) printf("rbar\n");
              while ((c=getc(input_file))!=EOF && c!='\n') ;
              break; }
            else flush_line(2);
          }
          *next_char++=c; ++current_pos;
      }
    }
    flush_line(0);
  }
  if (for_real) printf("restore showpage\n");
}


/* ============================== At the end ============================== */

/* Do whatever is necessary before we die.
 */
static void tidy_up(void) {
  if (tempfile_name) remove(tempfile_name);
}


/* =========================== The main program =========================== */

int main(int argc, char *argv[]) {
  do_config_files();
  do_command_line(argc,argv);
  grok_things();
  if (show_n_pages) {
    for_real=0; process_files();
    n_pages=page_num;
    fprintf(stderr,"%d page%s in total.\n",n_pages,n_pages>1?"s":"");
  }
  emit_prologue();
  for_real=1; process_files();
  emit_trailer();
  tidy_up();
  return err_status;
}
