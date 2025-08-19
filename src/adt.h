/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* adt.h *** Header file for ADT */

#include "adtVersion.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/ArrowB.h>
#include <Xm/ArrowBG.h>
#include <Xm/CascadeB.h>
#include <Xm/CascadeBG.h>
#include <Xm/DrawingA.h>
#include <Xm/FileSB.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/MainW.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrolledW.h>
#include <Xm/SelectioB.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/ToggleBG.h>

#include <epicsVersion.h>
#include <cadef.h>

/* WIN32 differences */
#ifdef WIN32
/* Hummingbird extra functions including lprintf
 *   Needs to be included after Intrinsic.h for Exceed 5
 *   (Intrinsic.h is included in xtParams.h) */
# include <X11/XlibXtra.h>
/* In MSVC timeval is in winsock.h, winsock2.h, ws2spi.h, nowhere else */
# include <X11/Xos.h>
/* For getcwd */
# include <direct.h>
/* The following is done in Exceed 6 but not in Exceed 5
 *   Need it to define printf as lprintf for Windows
 *   (as opposed to Console) apps */
# ifdef _WINDOWS
#  ifndef printf
#   define printf lprintf
#  endif    
# endif    
#else /* #ifdef WIN32 */
/* Path delimiter is different */
# include <sys/time.h>
/* WIN32 does not have unistd.h */
# include <unistd.h>
/* WIN32 does not have utsname.h */
# include <sys/utsname.h>
#endif /* #ifdef WIN32 */

/* PATH_MAX */
#ifdef WIN32
/* Is in stdlib.h for WIN32 */
# define PATH_MAX _MAX_PATH
#else
/* May be in limits.h.  Kludge it if not */
# ifndef PATH_MAX
# define PATH_MAX 1024
# endif
#endif

#define DEFAULTDIRECTORY "."
#define INITFILENAME "adtrc"
#define HELPFILENAME "adt.html"
#define SAVEPATTERN "*.snap"
#define PVPATTERN "*.pv"
#define XORBITPREFIX "Xorbit:"

#define ECANAMESIZE 34
#define HEADSIZE 60
#define UNITSSIZE 40
#define RESERVENUMBER "0000000"
#define HEAD1 "%heading1="
#define HEAD2 "%heading2="
#define MAXARGS 10
#define GRIDDIVISIONS 5

#define NSAVE 5
#define SAVENUMBERSIZE 3
#define COLORNAMESIZE 32

#define LEGENDSIZE 5

#define LARGEVAL 1.e40

#define NOTCONN 100
#define UNUSED 200

#define CHECK 0
#define REQUIRED 1

/* Structures */

struct AREA {
    int area;
    int curscale;
    double centerval;
    double oldcenterval;
    double aptox;
    double aptos;
    double axtop;
    double astop;
    double bptox;
    double bptos;
    double bxtop;
    double bstop;
    double xmax;
    double xmin;
    double smin;
    double smax;
    Widget wcontrol;
    Widget wscalebutton;
    Widget wcenter;
    Widget wgrapharea;
    GC gc;
    Pixmap pixmap;
    unsigned int width;
    unsigned int height;
    Boolean graphinitialize;
    Boolean tempclear;
    Boolean tempnodraw;
};

struct ARRAY {
    int array;
    int nvals;
    char **names;
    char **statusnames;
    char **thresholdnames;
    double scalefactor;
    double runsdev;
    double runavg;
    double runmax;
    double *vals;
    double *refvals;
    double *minvals;
    double *maxvals;
    double *s;
    double *savevals[NSAVE];
    unsigned short *statusvals;
    unsigned short *thresholdvals;
    double *thresholdLL;
    double *thresholdUL;
    struct AREA *area;
    char *heading;
    char *units;
    int zoomindxmin;
    int zoomindxmax;
    Widget wsdev;
    Widget wavg;
    Widget wmax;
    Widget wtitle;
    Boolean *conn;
    Boolean *statusconn;
    Boolean *thresholdconn;
    Boolean checkstatus;
    Boolean checkthreshold;
    Boolean zoom;
    Boolean logscale;
    Pixel pixel;
    Pixmap legendpix;
};

struct SYMBOL {
    double s;
    double len;
    double height;
};

struct ZOOMAREA {
    int curscale;
    double centerval;
    double oldcenterval;
    double aptox;
    double aptos;
    double axtop;
    double astop;
    double bptox;
    double bptos;
    double bxtop;
    double bstop;
    double xmax;
    double xmin;
    double smin;
    double smax;
    Widget wcontrol;
    Widget wscalebutton;
    Widget wcenter;
    Widget wgrapharea;
    Widget winterval;
    Widget wsector;
    GC gc;
    Pixmap pixmap;
    unsigned int width;
    unsigned int height;
    Boolean graphinitialize;
    Boolean tempclear;
    Boolean tempnodraw;
};

/* Function prototypes */

int allocnames(int n, int m, char ***p);
int callBrowser(char *url, char *bookmark);
void clearcb(Widget w, XtPointer clientdata, XtPointer calldata);
Widget createlabelbox(Widget parent, char *widgetname, char *label,
  char *value, XtCallbackProc callback, XtPointer clientdata, Widget *w2);
void destroywidget(Widget w, XtPointer clientdata, XtPointer calldata);
Widget createlabelwidget(Widget parent, char *widgetname, char *label);
void draw(int iarray, Pixel pixel);
void drawmaxmin(int iarray);
void drawzoom(int iarray, Pixel pixel);
void ecacb(Widget w, XtPointer clientdata, XtPointer calldata);
int readlat(char *filename);
int readpvs(char *filename);
void ecashutdown(void);
void ecastarttimer(void);
int ecastartup(void);
void ecastoptimer(void);
void eventgraph(Widget w,  XtPointer clientdata, XEvent *event,
  Boolean *contdispatch);
void eventzoom(Widget w,  XtPointer clientdata, XEvent *event,
  Boolean *contdispatch);
int execute(char *s);
void fileplotcb(Widget w, XtPointer clientdata, XtPointer calldata);
void filereadcb(Widget w, XtPointer clientdata, XtPointer calldata);
void filereferencecb(Widget w, XtPointer clientdata,  XtPointer calldata);
void filewritecb(Widget w, XtPointer clientdata,  XtPointer calldata);
void freenames(int n, char ***p);
void freesddsnames(int n, char ***p);
int getuname(void);
void graphcontrolcb(Widget w, XtPointer clientdata, XtPointer calldata);
void helpcb(Widget w, XtPointer clientdata, XtPointer calldata);
int indxabove(double s, double *array, int nmax);
int indxbelow(double s, double *array, int nmax);
void indxlimits(double smin, double smax, double *array, int nmax,
  int *imin, int *imax);
int main(int argc, char **argv);
void makeclearmenu(void);
void makefilemenu(void);
Widget makegrapharea(int iarea);
Widget makegraphform(void);
void makehelpmenu(void);
Widget makelogo(void);
void makeoptionsmenu(void);
Widget makescalemenu(Widget wparent, int iarea);
void maketitle(void);
void makeviewmenu(void);
Widget makezoomarea(void);
void movezoommid(void);
void openmenucb(Widget w, XtPointer clientdata, XtPointer calldata);
void openmenucustomcb(Widget w, XtPointer clientdata,  XtPointer calldata);
void optcb(Widget w, XtPointer clientdata, XtPointer calldata);
void orbitdiffcb(Widget w, XtPointer clientdata, XtPointer calldata);
void orbitrestcb(Widget w, XtPointer clientdata, XtPointer calldata);
void orbitsavecb(Widget w, XtPointer clientdata, XtPointer calldata);
double pixs(int p);
double pixx(int p);
void print(const char *fmt, ...);
void quitfunc(Widget w, XtPointer clientdata, XtPointer calldata);
int readinit(char *filename);
int readreference(char *filename);
int readsnap(char *filename, int nsave);
void repaintgraph(Widget w, XtPointer clientdata, XtPointer calldata);
void repaintlogo(Widget w, XtPointer clientdata, XtPointer calldata);
void repaintzoom(Widget w, XtPointer clientdata, XtPointer calldata);
void resetgraph(void);
void resizegraph(Widget w, XtPointer clientdata, XtPointer calldata);
void resizelogo(Widget w, XtPointer clientdata, XtPointer calldata);
void resizezoom(Widget w, XtPointer clientdata, XtPointer calldata);
void scalemenucb(Widget w, XtPointer clientdata, XtPointer calldata);
void scrollzoommid(void);
Boolean scrollzoomwork(XtPointer clientdata);
void setup(void);
int spix(double z);
void statistics(void);
void status(void);
void trapalarm(int signum);
void viewcb(Widget w, XtPointer clientdata, XtPointer calldata);
Boolean work(XtPointer clientdata);
int writeplotfile(char *filename, int nsave);
int writesnap(char *filename, int nsave);
void xerrmsg(char *fmt, ...);
int xerrorhandler(Display *dpy, XErrorEvent *event);
void xinfomsg(char *fmt, ...);
int xpix(double z);
void xterrorhandler(char *message) __attribute__((noreturn));
void zoomcontrolcb(Widget w, XtPointer clientdata, XtPointer calldata);

/* Macro functions */

#define sign(x) (((x) < 0)?-1:1)

#ifdef ADT_ALLOCATE_STORAGE
#define ADT_EXTERN
#else
#define ADT_EXTERN extern
#endif

/* Global variables */

ADT_EXTERN char string[10*BUFSIZ];

ADT_EXTERN int exitcode;

ADT_EXTERN int screen,windowmessage,nplanes;
ADT_EXTERN Arg args[MAXARGS];
ADT_EXTERN Cardinal nargs;
ADT_EXTERN Cursor crosshair,watch;
ADT_EXTERN Display *display;
ADT_EXTERN Pixmap apspix,pvpix,pv1pix,pv2pix;
ADT_EXTERN Widget appshell,mainwin,mainmenu,warningbox,infobox;
ADT_EXTERN Widget mainform,graphform,zoomform;
ADT_EXTERN Widget ecamenu,echoecabutton;
ADT_EXTERN Widget filebutton,filemenu;
ADT_EXTERN Widget helpbutton,helpmenu;
ADT_EXTERN Widget helppopup,optpopup;
ADT_EXTERN Widget optmenu,loadmenu;
ADT_EXTERN Widget wmarkers,wlines,wbars,wgrid,wshowmaxmin,wfillmaxmin;
ADT_EXTERN Window rootwindow,mainwindow,zoomareawindow;
ADT_EXTERN XColor newcolordef,requestcolordef;
ADT_EXTERN XmString cstring;
ADT_EXTERN XtAppContext appcontext;

ADT_EXTERN Widget checkstatusw;
ADT_EXTERN int checkstatus,checkstatusmode,checkthreshold,checkthresholdmode;

ADT_EXTERN int monitors,firstmon,lastmon;
ADT_EXTERN int xpress,ypress,xscroll;
ADT_EXTERN int ngraphunit,nunits,unitinterval;
ADT_EXTERN double smid,sdel;

ADT_EXTERN int neca;
ADT_EXTERN int ecaresponding,ecainitialized,ecaconnected,allconnected;

ADT_EXTERN char pvfilename[PATH_MAX];
ADT_EXTERN char latfilename[PATH_MAX];
ADT_EXTERN char reffilename[PATH_MAX];
ADT_EXTERN char initfilename[PATH_MAX];
ADT_EXTERN char browserfile[PATH_MAX];
ADT_EXTERN char snapdirectory[PATH_MAX];
ADT_EXTERN char pvdirectory[PATH_MAX];
ADT_EXTERN char customdirectory[PATH_MAX];
ADT_EXTERN char defaulttitle[BUFSIZ];
ADT_EXTERN char *adthome;
ADT_EXTERN char ***pvfiles;
ADT_EXTERN int npvmenus;

ADT_EXTERN double aptos,aptox,astop,axtop;
ADT_EXTERN double bptos,bptox,bstop,bxtop;

ADT_EXTERN int nscales;
ADT_EXTERN char scalelabel[21][8];
ADT_EXTERN double scale[21];

ADT_EXTERN unsigned long timeinterval;
ADT_EXTERN unsigned long zoominterval;
ADT_EXTERN unsigned long zoomsector;
ADT_EXTERN unsigned long zoomsectorused;
ADT_EXTERN unsigned long zoomintervalInput;
ADT_EXTERN unsigned long zoomintervalInputused;

ADT_EXTERN int markers,lines,bars,symbols,grid;
ADT_EXTERN int showmaxmin,fillmaxmin,autoclear;
ADT_EXTERN int zoomsymmin,zoomsymmax;
ADT_EXTERN int zoomon,zoom,refon,reference;
ADT_EXTERN int pvstartup,configspecified;

ADT_EXTERN int nsymbols,nsect,whichorbit,ring;
ADT_EXTERN double stotal;
ADT_EXTERN double *ecasyms,*ecasymlen;
ADT_EXTERN short *ecasymheight;

ADT_EXTERN double *xplot;
ADT_EXTERN int *xarray,*sarray;

ADT_EXTERN int displaysave;
ADT_EXTERN char displaycolor[COLORNAMESIZE];
ADT_EXTERN char savenumbers[NSAVE][SAVENUMBERSIZE];
ADT_EXTERN char savetime[NSAVE][26];
ADT_EXTERN char savefilename[NSAVE][PATH_MAX];

ADT_EXTERN int xorbit,prefixlength;
ADT_EXTERN char prefix[ECANAMESIZE];

ADT_EXTERN struct AREA *areas;
ADT_EXTERN struct ARRAY *arrays;
ADT_EXTERN struct ZOOMAREA *zoomarea;
ADT_EXTERN int narrays,nareas,nvalsmax;

ADT_EXTERN Pixel whitepixel,blackpixel,greypixel;
ADT_EXTERN Pixel notconnpixel,invalidpixel,olddatapixel,displaypixel;

ADT_EXTERN int statmode;
ADT_EXTERN double nstat,nstattime;

ADT_EXTERN char *unamestring;
