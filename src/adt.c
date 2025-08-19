/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* adt.c *** Main C program for Array Display Tool */

#define DEBUG_GEN 0
#define DEBUG_XERROR 0

#include <sys/types.h>
#include <sys/stat.h>
#define ADT_ALLOCATE_STORAGE
#include "adt.h"

#include <Xm/MwmUtil.h>
#include <Xm/Protocols.h>
#include <X11/Xatom.h>

#ifndef WIN32
#include <fcntl.h>
#endif

#ifdef EDITRES
#  include <X11/Xmu/Editres.h>
#endif

/* Icons (Refer to as name_bits[]) */

#include "aps.icon"
#include "pv.icon"     /* Normal */
#include "pv1.icon"    /* Not connected */
#include "pv2.icon"    /* Bad */
#include "resources.h"

#define DISPLAYCOLOR "Grey40"
#define GRIDCOLOR    "Grey75"
#define NOTCONNCOLOR "Black"
#define INVALIDCOLOR "Grey30"
#define OLDDATACOLOR "Grey50"

/* Global variables */

XtWorkProcId workprocid=(XtWorkProcId)0;
Atom WM_DELETE_WINDOW;

#if DEBUG_XERROR
static void debug1(char *title, Widget w)
{
    Widget w1=NULL,w2=NULL;
    
    w1=XtParent(w);
    if(w1) w2=XtParent(w1);
    
    print("%-40s NextRequest=%ld [0x%lx]\n",
      title,NextRequest(display),NextRequest(display));
    print("  widget=%x window=%x\n",w,XtWindow(w));
    if(w1)
      print("  parent=%x window=%x\n",w1,XtWindow(w1));
    if(w2)
      print("  grandp=%x window=%x\n",w2,XtWindow(w2));
}
#endif

/**************************** main ****************************************/

int main(int argc, char **argv)
{
  //Widget wparent;
    struct stat statinfo;
    int i, diffmode;
    char *value;
    
  /* WIN32 initialization */
#if HCLXMINIT
#ifdef WIN32	
    HCLXmInit();
#endif
#endif
#if DEBUG_XERROR
    print("\nStarting ADT\n");
#endif    

  /* Initializations */
    exitcode=0;
    windowmessage=0;
    diffmode = 0;
  /* Set up top level and get remaining command-line arguments */
    XtToolkitInitialize();
    appcontext=XtCreateApplicationContext();
    XtAppSetFallbackResources(appcontext,resources);
    display=XtOpenDisplay(appcontext,NULL,"adt","ControlApp",
      NULL,0,&argc,argv);
    if(display == NULL) {
	XtWarning("ADT initialization: Cannot open display");
	exit(-1);
    }
#if DEBUG_GEN
    XSynchronize(display,True);
#endif    

  /* Constants, defaults, initializations */
    setup();
    zoomsectorused=zoomintervalInputused=0;
    zoomsector=zoomintervalInput=0;
  /* Parse remains of command line */
    for (i=1; i < argc; i++) {
	if (argv[i][0] == '-' || argv[i][0] == '/') {
	    switch (argv[i][1]) {
	    case 'a':
		if(++i < argc) adthome=argv[i];
		sprintf(initfilename,"%s/%s",adthome,INITFILENAME);
		configspecified=1;
		break;
	    case 'f':
                if(++i < argc) snprintf(pvfilename,PATH_MAX,"%s",argv[i]);
		pvstartup=1;
		break;
            case 'd':
              diffmode=1;
              break;
            case 's':
              if(++i < argc) zoomsector=atoi(argv[i]);
              break;
            case 'z':
              if(++i < argc) zoomintervalInput=atoi(argv[i]);
              break;
	    case 'h':
	    case '?':
		fprintf(stderr,"Usage: adt [X-Options] [-x] [-f pvfilename] [-s zoomsector] [-z zoominterval] [-d] "
		  "[-a adthomedirectory] [&]\n");
		exit(0);
		break;
	    case 'x':
		xorbit=1;
		break;
	    default:
		fprintf(stderr,"Invalid option %s",argv[i]);
		exit(8);
	    }
	}
    }
  /* Find init file if not given on command line */
    if(!configspecified) {
      /* Get ADTHOME */
	adthome=getenv("ADTHOME");
	if(!adthome) {
	    adthome=(char *)calloc(1,sizeof(DEFAULTDIRECTORY));
	    strcpy(adthome,DEFAULTDIRECTORY);
	}
	sprintf(initfilename,"$HOME/.%s",INITFILENAME);
	if(stat(initfilename,&statinfo) == -1) {
	    sprintf(initfilename,"%s/%s",adthome,INITFILENAME);
	    if(stat(initfilename,&statinfo) == -1) {
		*initfilename='\0';
	    }
	}
    }

  /* Check that display was valid */
    if(display == NULL) {
	fprintf(stderr,"ADT initialization: Cannot open display\n");
	exit(4);
    }
    screen=DefaultScreen(display);
    nplanes=DisplayPlanes(display,screen);
    rootwindow=RootWindow(display,screen);

  /* Check for color display */
    if(nplanes <= 1) {
	fprintf(stderr,"ADT initialization: Monochrome not supported\n");
	return(1);
    }

  /* Define icon */
    apspix=XCreatePixmapFromBitmapData(display,rootwindow,
      (char *)aps_bits,aps_width,aps_height,
      1,0,1);

  /* Create app (top level) shell */
    nargs=0;
#ifndef WIN32
    XtSetArg(args[nargs],XtNiconPixmap,apspix); nargs++; 
#endif
    appshell=XtAppCreateShell("adt","ControlApp",applicationShellWidgetClass,
      display,args,nargs);
#if DEBUG_XERROR
    debug1("appshell",appshell);
#endif    
    
  /* Set error handlers */
    XtAppSetErrorHandler(appcontext, xterrorhandler);
    XtAppSetWarningHandler(appcontext, xterrorhandler);
    XSetErrorHandler(xerrorhandler);
    
  /* Fix up WM close */
    WM_DELETE_WINDOW=XmInternAtom(display,"WM_DELETE_WINDOW",False);
    XmAddWMProtocolCallback(appshell,WM_DELETE_WINDOW,
      quitfunc,(XtPointer)0);

  /* Enable Editres */
#ifdef EDITRES    
    XtAddEventHandler(appshell,(EventMask)NULL,TRUE,
      (XtEventHandler)_XEditResCheckMessages,NULL);
#endif
    
  /* Get defaults */
    strncpy(displaycolor,DISPLAYCOLOR,COLORNAMESIZE);
    
  /* Get current title */
    value=XGetDefault(display,"adt","title");
    if(value) strncpy(defaulttitle,value,BUFSIZ);
    else *defaulttitle='\0';
    defaulttitle[BUFSIZ-1]='\0';
    
  /* Change title */
    if(xorbit) {
	strcpy(prefix,XORBITPREFIX);
	prefixlength=(int)strlen(XORBITPREFIX);
	maketitle();
    }
    else {
	*prefix='\0';
	prefixlength=0;
    }
    
  /* Define main window */
    nargs=0;
    mainwin=XmCreateMainWindow(appshell,"mainWindow",args,nargs);
    XtManageChild(mainwin);
#if DEBUG_XERROR
    debug1("mainwin",mainwin);
#endif    

  /* Define menu bar */
    nargs=0;
    mainmenu=XmCreateMenuBar(mainwin,"mainMenu",args,nargs);
    XtManageChild(mainmenu);
    //wparent=mainmenu;
#if DEBUG_XERROR
    debug1("mainmenu",mainmenu);
#endif    
    
  /* Define main menu items */
    makefilemenu();
    makeoptionsmenu();
    makeviewmenu();
    makeclearmenu();
    makehelpmenu();
    
  /* Define form for rest of main window */
    nargs=0;
    mainform=XmCreateForm(mainwin,"mainForm",args,nargs);
    XtManageChild(mainform);

  /* Define container form for graphs */
    graphform=(Widget)0;
    makegraphform();

  /* Set main window areas */
    XmMainWindowSetAreas(mainwin,mainmenu,NULL,NULL,NULL,mainform);
    
  /* Create pixmaps */
    pvpix=XCreatePixmapFromBitmapData(display,rootwindow,
      (char *)pv_bits,pv_width,pv_height,
      1,0,1);
    pv1pix=XCreatePixmapFromBitmapData(display,rootwindow,
      (char *)pv1_bits,pv1_width,pv1_height,
      1,0,1);
    pv2pix=XCreatePixmapFromBitmapData(display,rootwindow,
      (char *)pv2_bits,pv2_width,pv2_height,
      1,0,1);
    
  /* Realize */
    XtRealizeWidget(appshell);
    
  /* Get window ids */
    mainwindow=XtWindow(mainwin);
    
  /* Define cursors */
    crosshair=XCreateFontCursor(display,XC_crosshair);
    watch=XCreateFontCursor(display,XC_watch);
    
  /* Define colors */
  /* Black */
    blackpixel=BlackPixel(display,screen);
  /* White */
    whitepixel=WhitePixel(display,screen);
  /* Grey */    
    if(XAllocNamedColor(display,DefaultColormap(display,screen),
      GRIDCOLOR,&newcolordef,&requestcolordef) == 0) {
	xerrmsg("Could not allocate color: %s",GRIDCOLOR);
	greypixel=blackpixel;
    }
    else greypixel=newcolordef.pixel;
  /* Invalid */    
    if(XAllocNamedColor(display,DefaultColormap(display,screen),
      INVALIDCOLOR,&newcolordef,&requestcolordef) == 0) {
	xerrmsg("Could not allocate color: %s",INVALIDCOLOR);
	invalidpixel=blackpixel;
    }
    else invalidpixel=newcolordef.pixel;
  /* Olddata */    
    if(XAllocNamedColor(display,DefaultColormap(display,screen),
      OLDDATACOLOR,&newcolordef,&requestcolordef) == 0) {
	xerrmsg("Could not allocate color: %s",OLDDATACOLOR);
	olddatapixel=blackpixel;
    }
    else olddatapixel=newcolordef.pixel;
  /* Not connected */    
    if(XAllocNamedColor(display,DefaultColormap(display,screen),
      NOTCONNCOLOR,&newcolordef,&requestcolordef) == 0) {
	xerrmsg("Could not allocate color: %s",NOTCONNCOLOR);
	notconnpixel=blackpixel;
    }
    else notconnpixel=newcolordef.pixel;
  /* Display */
    if(XAllocNamedColor(display,DefaultColormap(display,screen),
      displaycolor,&newcolordef,&requestcolordef) == 0) {
	xerrmsg("Could not allocate color: %s",displaycolor);
	displaypixel=blackpixel;
    }
    else displaypixel=newcolordef.pixel;

#ifndef WIN32
  /* Allow forked processes to disinherit the display file descriptor */
    if(fcntl(ConnectionNumber(display),F_SETFD,1) == -1) {
	xerrmsg("Cannot set display file decriptor for no inheritance");
    }
#endif
    
  /* Set to start EPICS if pvstartup=1 */
    if(pvstartup) workprocid=XtAppAddWorkProc(appcontext,work,NULL);
  /* Start main loop */
    windowmessage=1;
    if (diffmode) {
      time_t startTime;
      XEvent event;
      startTime = time(NULL);
      do {
        XtAppNextEvent (appcontext, &event);
        XtDispatchEvent (&event);
      } while (difftime(time(NULL),startTime) <= 1.0);
      /* Do your event here */
      orbitsavecb((Widget)0, (XtPointer)savenumbers[0], work);
      orbitdiffcb((Widget)0, (XtPointer)savenumbers[0], work); 
    }
    XtAppMainLoop(appcontext);
    quitfunc((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
    return(0);
}
/**************************** makegraphform *******************************/
Widget makegraphform(void)
{
    int i;
    static int nareasold=-1;

  /* Destroy old form */
    if(graphform) {
	if(nareas == 0 && nareasold == 0) return graphform;
	XtUnmanageChild(graphform);
	XtDestroyWidget(graphform);	
    }
    nareasold=nareas;
  /* Make a new form */    
    nargs=0;
    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNshadowThickness,0); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    if(nareas) {
	if(!zoom) {XtSetArg(args[nargs],XmNfractionBase,10*nareas); nargs++;}
	else {XtSetArg(args[nargs],XmNfractionBase,10*(nareas+1)); nargs++;}
    }
    graphform=XmCreateForm(mainform,"graphForm",args,nargs);
    XtManageChild(graphform);
  /* Add areas to the form */
    if(nareas) {
	for(i=0; i < nareas; i++)
	  makegrapharea(i);
    }
    else {
	makelogo();
    }
    if(zoom) {
	zoomform=makezoomarea();
	if(!zoomon) {
	    nargs=0;
	    XtSetArg(args[nargs],XmNfractionBase,10*nareas); nargs++;
	    XtSetValues(graphform,args,nargs);
	    XtUnmanageChild(zoomform);
	}
    }
  /* Return */
    maketitle();
    return graphform;    
}
/**************************** makegrapharea *******************************/
Widget makegrapharea(int iarea)
{
    Widget form,w,w1,lasttitle=(Widget)0;
    int iarray,len;
    
  /* Define form container */
    nargs=0;
    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_POSITION); nargs++;
    XtSetArg(args[nargs],XmNtopPosition,10*iarea); nargs++;
    XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_POSITION); nargs++;
    XtSetArg(args[nargs],XmNbottomPosition,10*(iarea+1)); nargs++;
    XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNorientation,XmHORIZONTAL); nargs++;
    XtSetArg(args[nargs],XmNshadowThickness,0); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    form=XmCreateForm(graphform,"graphAreaForm",args,nargs);
    XtManageChild(form);
    
  /* Define graph title for each array in the area */
    for(iarray=0; iarray < narrays; iarray++) {
	if(arrays[iarray].area != &areas[iarea]) continue;
	nargs=0;
	XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
	if(!lasttitle) {
	    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	}
	else {
	    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_WIDGET); nargs++;
	    XtSetArg(args[nargs],XmNtopWidget,lasttitle); nargs++;
	}
	XtSetArg(args[nargs],XmNorientation,XmHORIZONTAL); nargs++;
	XtSetArg(args[nargs],XmNshadowThickness,0); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	arrays[iarray].wtitle=XmCreateForm(form,"graphTitle",args,nargs);
	XtManageChild(arrays[iarray].wtitle);
	lasttitle=arrays[iarray].wtitle;
	
	cstring=XmStringCreateLocalized(" ");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNrecomputeSize,False); nargs++;
	XtSetArg(args[nargs],XmNmarginWidth,0); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XmStringFree(cstring);
	XtManageChild(w);
	
	cstring=XmStringCreateLocalized(RESERVENUMBER);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_WIDGET); nargs++;
	XtSetArg(args[nargs],XmNrightWidget,w); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNrecomputeSize,False); nargs++;
	XtSetArg(args[nargs],XmNmarginWidth,0); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XmStringFree(cstring);
	XtManageChild(w);
	arrays[iarray].wmax=w;
	
	cstring=XmStringCreateLocalized(" ");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(arrays[iarray].wmax,args,nargs);
	XmStringFree(cstring);
	
	cstring=XmStringCreateLocalized("    MAX:");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_WIDGET); nargs++;
	XtSetArg(args[nargs],XmNrightWidget,w); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNrecomputeSize,False); nargs++;
	XtSetArg(args[nargs],XmNmarginWidth,0); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XmStringFree(cstring);
	XtManageChild(w);
	
	cstring=XmStringCreateLocalized(RESERVENUMBER);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_WIDGET); nargs++;
	XtSetArg(args[nargs],XmNrightWidget,w); nargs++;
	XtSetArg(args[nargs],XmNrightOffset,0); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNrecomputeSize,False); nargs++;
	XtSetArg(args[nargs],XmNmarginWidth,0); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XmStringFree(cstring);
	XtManageChild(w);
	arrays[iarray].wavg=w;
	
	cstring=XmStringCreateLocalized(" ");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(arrays[iarray].wavg,args,nargs);
	XmStringFree(cstring);
	
	cstring=XmStringCreateLocalized("    AVG:");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_WIDGET); nargs++;
	XtSetArg(args[nargs],XmNrightWidget,w); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNrecomputeSize,False); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XmStringFree(cstring);
	XtManageChild(w);
	
	cstring=XmStringCreateLocalized(RESERVENUMBER);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_WIDGET); nargs++;
	XtSetArg(args[nargs],XmNrightWidget,w); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNrecomputeSize,False); nargs++;
	XtSetArg(args[nargs],XmNmarginWidth,0); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XmStringFree(cstring);
	XtManageChild(w);
	arrays[iarray].wsdev=w;
	
	cstring=XmStringCreateLocalized(" ");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(arrays[iarray].wsdev,args,nargs);
	XmStringFree(cstring);
	
	cstring=XmStringCreateLocalized("SDEV:");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_WIDGET); nargs++;
	XtSetArg(args[nargs],XmNrightWidget,w); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNrecomputeSize,False); nargs++;
	XtSetArg(args[nargs],XmNmarginWidth,0); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XmStringFree(cstring);
	XtManageChild(w);
	
	nargs=0;
	XtSetArg(args[nargs],XmNlabelType,XmPIXMAP); nargs++;
	XtSetArg(args[nargs],XmNlabelPixmap,arrays[iarray].legendpix); nargs++;
	XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNalignment,XmALIGNMENT_CENTER); nargs++;
	XtSetArg(args[nargs],XmNmarginWidth,0); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XtManageChild(w);

	sprintf(string,"%s (%s)",arrays[iarray].heading,arrays[iarray].units);
	cstring=XmStringCreateLocalized(string);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_WIDGET); nargs++;
	XtSetArg(args[nargs],XmNleftWidget,w); nargs++;
	XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
	XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
	XtSetArg(args[nargs],XmNalignment,XmALIGNMENT_BEGINNING); nargs++;
	XtSetArg(args[nargs],XmNrecomputeSize,False); nargs++;
	w=XmCreateLabel(arrays[iarray].wtitle,"graphTitleLabel",args,nargs);
	XmStringFree(cstring);
	XtManageChild(w);
    }

  /* Define graph control area */
    nargs=0;
    XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_WIDGET); nargs++;
    XtSetArg(args[nargs],XmNtopWidget,lasttitle); nargs++;
    XtSetArg(args[nargs],XmNorientation,XmHORIZONTAL); nargs++;
    areas[iarea].wcontrol=XmCreateRowColumn(form,"graphControl",args,nargs);
    XtManageChild(areas[iarea].wcontrol);
    
  /* Define menu bar, scale button, and menu */
    nargs=0;
    w1=XmCreateMenuBar(areas[iarea].wcontrol,"controlMenu",args,nargs);
    XtManageChild(w1);
    
    w=makescalemenu(w1,iarea);
    
    sprintf(string,"%s /Div",scalelabel[areas[iarea].curscale]);
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w); nargs++;
    w=XmCreateCascadeButton(w1,string,args,nargs);
    XtManageChild(w);
    areas[iarea].wscalebutton=w;

  /* Define center value box */    
    cstring=XmStringCreateLocalized("Center:");
    nargs=0;
    XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateLabel(areas[iarea].wcontrol,"graphLabel",args,nargs);
    XmStringFree(cstring);
    XtManageChild(w);
    nargs=0;
    sprintf(string,"% #11.4g",areas[iarea].centerval);
    len=(int)strlen(string);
    nargs=0;
    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
    XtSetArg(args[nargs],XmNvalue,string); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,TRUE); nargs++;
    w=XmCreateTextField(areas[iarea].wcontrol,"floatEntry",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,graphcontrolcb,
      (XtPointer)(intptr_t)(100*iarea+1));
    areas[iarea].wcenter=w;
    
  /* Define center value arrows */
    nargs=0;
    XtSetArg(args[nargs],XmNarrowDirection,XmARROW_UP); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateArrowButton(areas[iarea].wcontrol,"arrow",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,graphcontrolcb,
      (XtPointer)(intptr_t)(100*iarea+2));
    nargs=0;
    XtSetArg(args[nargs],XmNarrowDirection,XmARROW_DOWN); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateArrowButton(areas[iarea].wcontrol,"arrow",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,graphcontrolcb,
      (XtPointer)(intptr_t)(100*iarea+3));

  /* Define graph drawing area */
    nargs=0;
    XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_WIDGET); nargs++;
    XtSetArg(args[nargs],XmNtopWidget,areas[iarea].wcontrol); nargs++;
    XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateDrawingArea(form,"graphArea",args,nargs);
    XtManageChild(w);
    XtAddEventHandler(w,ButtonPressMask|ButtonReleaseMask,False,
      eventgraph,(XtPointer)(intptr_t)areas[iarea].area);
    XtAddCallback(w,XmNexposeCallback,repaintgraph,(XtPointer)(intptr_t)areas[iarea].area);
    XtAddCallback(w,XmNresizeCallback,resizegraph,(XtPointer)(intptr_t)areas[iarea].area);
    areas[iarea].wgrapharea=w;
  /* Return */
    return form;
}
/**************************** makelogo ************************************/
Widget makelogo(void)
{
    Widget logo;
    
  /* Define graph area */
    nargs=0;
    XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    logo=XmCreateDrawingArea(graphform,"graphArea",args,nargs);
    XtManageChild(logo);
  /*     XtAddEventHandler(logo,ButtonPressMask|ButtonReleaseMask,False, */
  /*       eventlogo,NULL); */
    XtAddCallback(logo,XmNexposeCallback,repaintlogo,NULL);
    XtAddCallback(logo,XmNresizeCallback,resizelogo,NULL);

  /* Return */
    return logo;    
}
/**************************** maketitle ***********************************/
void maketitle(void)
{
    char *ptr;
    
  /* Make new title */
    sprintf(string,"%s",defaulttitle);
    if(xorbit) {
	ptr=string+strlen(string);
	sprintf(ptr," (Xorbit Simulation)");
    }
    if(reference && refon) {
	ptr=string+strlen(string);
	sprintf(ptr," (REF)");
    }
    if(whichorbit >= 0) {
	ptr=string+strlen(string);
	sprintf(ptr," (Difference w.r.t. Set %s)",savenumbers[whichorbit]);
    }
  /* Set new title on title bar*/
    nargs=0;
    XtSetArg(args[nargs],XmNtitle,string); nargs++;
    XtSetValues(appshell,args,nargs);
    resetgraph();
}
/**************************** makezoomarea ********************************/
Widget makezoomarea(void)
{
    Widget form,w,w1,lasttitle=(Widget)0;
    int len;
    
  /* Define form container */
    nargs=0;
    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_POSITION); nargs++;
    XtSetArg(args[nargs],XmNtopPosition,10*nareas); nargs++;
    XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_POSITION); nargs++;
    XtSetArg(args[nargs],XmNbottomPosition,10*(nareas+1)); nargs++;
    XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNorientation,XmHORIZONTAL); nargs++;
    XtSetArg(args[nargs],XmNshadowThickness,0); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    form=XmCreateForm(graphform,"graphAreaForm",args,nargs);
    XtManageChild(form);
    
  /* Define graph control area */
    nargs=0;
    XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_WIDGET); nargs++;
    XtSetArg(args[nargs],XmNtopWidget,lasttitle); nargs++;
    XtSetArg(args[nargs],XmNorientation,XmHORIZONTAL); nargs++;
    zoomarea->wcontrol=XmCreateRowColumn(form,"graphControl",args,nargs);
    XtManageChild(zoomarea->wcontrol);
    
  /* Define menu bar, scale button, and menu */
    nargs=0;
    w1=XmCreateMenuBar(zoomarea->wcontrol,"controlMenu",args,nargs);
    XtManageChild(w1);
    
    w=makescalemenu(w1,nareas);
    
    sprintf(string,"%s /Div",scalelabel[zoomarea->curscale]);
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w); nargs++;
    w=XmCreateCascadeButton(w1,string,args,nargs);
    XtManageChild(w);
    zoomarea->wscalebutton=w;

  /* Define center value box */    
    cstring=XmStringCreateLocalized("Center:");
    nargs=0;
    XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateLabel(zoomarea->wcontrol,"graphLabel",args,nargs);
    XmStringFree(cstring);
    XtManageChild(w);
    nargs=0;
    sprintf(string,"% #11.4g",zoomarea->centerval);
    len=(int)strlen(string);
    nargs=0;
    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
    XtSetArg(args[nargs],XmNvalue,string); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,TRUE); nargs++;
    w=XmCreateTextField(zoomarea->wcontrol,"floatEntry",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,zoomcontrolcb,(XtPointer)(intptr_t)1);
    zoomarea->wcenter=w;
    
  /* Define center value arrows */
    nargs=0;
    XtSetArg(args[nargs],XmNarrowDirection,XmARROW_UP); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateArrowButton(zoomarea->wcontrol,"arrow",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,zoomcontrolcb,(XtPointer)(intptr_t)2);
    nargs=0;
    XtSetArg(args[nargs],XmNarrowDirection,XmARROW_DOWN); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateArrowButton(zoomarea->wcontrol,"arrow",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,zoomcontrolcb,(XtPointer)(intptr_t)3);

  /* Define graph interval control */
    cstring=XmStringCreateLocalized("Interval:");
    nargs=0;
    XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
    XtSetArg(args[nargs],XmNvalue,string); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateLabel(zoomarea->wcontrol,"graphLabel",args,nargs);
    XmStringFree(cstring);
    XtManageChild(w);
    nargs=0;
    sdel=(double)zoominterval; 
    sprintf(string,"% #11.4g",sdel);
    len=(int)strlen(string);
    nargs=0;
    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
    XtSetArg(args[nargs],XmNvalue,string); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,TRUE); nargs++;
    w=XmCreateTextField(zoomarea->wcontrol,"floatEntry",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,zoomcontrolcb,(XtPointer)(intptr_t)4);
    zoomarea->winterval=w;
    
  /* Define graph sector control */
    cstring=XmStringCreateLocalized("Sector:");
    nargs=0;
    XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
    XtSetArg(args[nargs],XmNvalue,string); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateLabel(zoomarea->wcontrol,"graphLabel",args,nargs);
    XmStringFree(cstring);
    XtManageChild(w);
    nargs=0;
    sprintf(string,"%d",ngraphunit);
    len=(int)strlen(string);
    nargs=0;
    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
    XtSetArg(args[nargs],XmNvalue,string); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,TRUE); nargs++;
    w=XmCreateTextField(zoomarea->wcontrol,"intEntry",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,zoomcontrolcb,(XtPointer)(intptr_t)5);
    zoomarea->wsector=w;
    
  /* Define graph sector arrows */
    nargs=0;
    XtSetArg(args[nargs],XmNarrowDirection,XmARROW_LEFT); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateArrowButton(zoomarea->wcontrol,"arrow",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,zoomcontrolcb,(XtPointer)(intptr_t)6);
    nargs=0;
    XtSetArg(args[nargs],XmNarrowDirection,XmARROW_RIGHT); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateArrowButton(zoomarea->wcontrol,"arrow",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,zoomcontrolcb,(XtPointer)(intptr_t)7);

  /* Define graph drawing area */
    nargs=0;
    XtSetArg(args[nargs],XmNleftAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNtopAttachment,XmATTACH_WIDGET); nargs++;
    XtSetArg(args[nargs],XmNtopWidget,zoomarea->wcontrol); nargs++;
    XtSetArg(args[nargs],XmNbottomAttachment,XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs],XmNtraversalOn,False); nargs++;
    w=XmCreateDrawingArea(form,"graphArea",args,nargs);
    XtManageChild(w);
    XtAddEventHandler(w,ButtonPressMask|ButtonReleaseMask,False,
      eventzoom,(XtPointer)0);
    XtAddCallback(w,XmNexposeCallback,repaintzoom,(XtPointer)0);
    XtAddCallback(w,XmNresizeCallback,resizezoom,(XtPointer)0);
    zoomarea->wgrapharea=w;
    zoomareawindow=XtWindow(w);
  /* Return */
    return form;
}
/**************************** makescalemenu *******************************/
Widget makescalemenu(Widget wparent, int iarea)
{
    Widget w,w1;
    int i;
    
  /* Define menu */
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    XtSetArg(args[nargs],XmNnumColumns,nscales/3); nargs++;
    XtSetArg(args[nargs],XmNpacking,XmPACK_COLUMN); nargs++;
    XtSetArg(args[nargs],XmNorientation,XmHORIZONTAL); nargs++;
    w1=XmCreatePulldownMenu(wparent,"scaleMenu",args,nargs);
  /* Add buttons */
    for(i=0; i < nscales; i++) {
	nargs=0;
	w=XmCreatePushButton(w1,scalelabel[i],args,nargs);
	XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,scalemenucb,(XtPointer)(intptr_t)(100*iarea+i));
    }
    return w1;
}
/**************************** setup ***************************************/
void setup(void)
{
    int i;
    static int ifirst=1;
    char *cwd;
    
  /* First time only */
    if(ifirst) {
	ifirst=0;
	autoclear=1;
	ecainitialized=ecaconnected=ecaresponding=0;
	pvstartup=0;
	configspecified=0;
	refon=1;
	xorbit=0;
	zoomon=1;
	getuname();
	cwd=getcwd(NULL,PATH_MAX);
	if(cwd) {
	    strcpy(snapdirectory,cwd);
	    free(cwd);
	}
    }
  /* Initialize variables */
    bars=1;
    checkstatus=0;
    checkstatusmode=2;
    checkthreshold=0;
    checkthresholdmode=2;
    displaysave=0;
    fillmaxmin=1;
    grid=1;
    lines=1;
    markers=1;
    narrays=nareas=nsymbols=0;
    ngraphunit=1;
    nsect=0;
    reference=0;
    ring=1;
    sdel=0.;
    showmaxmin=1;
    smid=sdel/2.;
    statmode=0;
    stotal=0.;
    symbols=1;
    timeinterval=2000;     /* in msec */
    zoominterval = 1;
    whichorbit=-1;
    zoom=0;
  /* Initialize numbers for save menus */
    for(i=0; i < NSAVE; i++) {
	sprintf(savenumbers[i],"%d",i+1);
	savetime[i][0]='\0';
	savefilename[i][0]='\0';
    }
  /* Define labels for scales */
    i=0;
    strcpy(scalelabel[i++],"0.001");
    strcpy(scalelabel[i++],"0.002");
    strcpy(scalelabel[i++],"0.005");
    strcpy(scalelabel[i++],"0.010");
    strcpy(scalelabel[i++],"0.020");
    strcpy(scalelabel[i++],"0.050");
    strcpy(scalelabel[i++],"0.100");
    strcpy(scalelabel[i++],"0.200");
    strcpy(scalelabel[i++],"0.500");
    strcpy(scalelabel[i++],"1.000");
    strcpy(scalelabel[i++],"2.000");
    strcpy(scalelabel[i++],"5.000");
    strcpy(scalelabel[i++],"10.00");
    strcpy(scalelabel[i++],"20.00");
    strcpy(scalelabel[i++],"50.00");
    strcpy(scalelabel[i++],"100.0");
    strcpy(scalelabel[i++],"200.0");
    strcpy(scalelabel[i++],"500.0");
    strcpy(scalelabel[i++],"1000.");
    strcpy(scalelabel[i++],"2000.");
    strcpy(scalelabel[i++],"5000.");
    nscales=i;
    for(i=0; i < nscales; i++) {
	scale[i]=atof(scalelabel[i]);
    }
}
/**************************** statistics **********************************/
void statistics(void)
{
    XmString text;
    double sum,sumsq,max,val,avg,sdev,dn;
    double sdevval,avgval,maxval;
    int i,ia,statusval,nvals;
    int thresholdval;
    double *vals=NULL,*savevals=NULL;
    unsigned short *statusvals=NULL;
    unsigned short *thresholdvals=NULL;
    
    if(!ecainitialized) return;
  /* Check if arrays are defined */
    if(!narrays) return;
  /* Calculate */
    nstat+=1.;
    nstattime+=(double)timeinterval;
    for(ia=0; ia < narrays; ia++) {
	dn=sum=sumsq=max=0.;
	vals=arrays[ia].vals;
	if(whichorbit >= 0) savevals=arrays[ia].savevals[whichorbit];;
	statusvals=arrays[ia].statusvals;
	thresholdvals=arrays[ia].thresholdvals;
	nvals=arrays[ia].nvals;
	for(i=0; i < nvals; i++) {
	    if(checkstatus) {
		statusval=statusvals[i];
		if(statusval == NOTCONN) continue;
		if(checkstatusmode && statusval != UNUSED) {
		    if(statusval == 0) continue;
		    if(checkstatusmode == 2 && statusval == 2) continue;
		}
	    }
	    if(checkthreshold) {
		thresholdval=thresholdvals[i];
		if(thresholdval == NOTCONN) continue;
		if(checkthresholdmode && thresholdval != UNUSED) {
		    if(thresholdval < 0) continue;
		    if(checkthresholdmode == 2 && thresholdval == 2) continue;
		}
	    }
	    if(whichorbit < 0) val=vals[i];
	    else val=vals[i]-savevals[i];
	    dn+=1.;
	    sum+=val;
	    sumsq+=(val*val);
	    if(fabs(val) > fabs(max)) max=val;
	}
	if(dn) {
	    avg=sum/dn;
	    sdev=sqrt(sumsq/dn-avg*avg);
	}
	else {
	    avg=0.;
	    sdev=0.;
	}
      /* Convert to log10 if logscale is set */
	if(arrays[ia].logscale) {
	    sdev=sdev > 0.?log10(sdev):0.;
	    avg=avg > 0.?log10(avg):0.;
	    max=max > 0.?log10(max):0.;
	}
      /* Accumulate averages */
	arrays[ia].runsdev+=sdev;
	arrays[ia].runavg+=avg;
	if(fabs(max) > fabs(arrays[ia].runmax))
	  arrays[ia].runmax=max;
      /* Write to  labels */
	if(statmode) {
	    sdevval=arrays[ia].runsdev/nstat;
	    avgval=arrays[ia].runavg/nstat;
	    maxval=arrays[ia].runmax;
	}
	else {
	    sdevval=sdev;
	    avgval=avg;
	    maxval=max;
	}
	sprintf(string,"% 7.3f",arrays[ia].scalefactor*sdevval);
	text=XmStringCreateLocalized(string);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,text); nargs++;
	XtSetValues(arrays[ia].wsdev,args,nargs);
	XmStringFree(text);
	
	sprintf(string,"% 7.3f", arrays[ia].scalefactor*avgval);
	text=XmStringCreateLocalized(string);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,text); nargs++;
	XtSetValues(arrays[ia].wavg,args,nargs);
	XmStringFree(text);
	
	sprintf(string,"% 7.3f", arrays[ia].scalefactor*maxval);
	text=XmStringCreateLocalized(string);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,text); nargs++;
	XtSetValues(arrays[ia].wmax,args,nargs);
	XmStringFree(text);
    }
}
/**************************** status **************************************/
void status(void)
{
    struct timeval tv;
    time_t clock;
    char time[26];
    Widget w,w1;
    XmString cstring1,cstring2;
    int i,ia;
    char *ptr;
    
  /* Get timestamp */
#ifdef WIN32
  /* MSVC does not have gettimeofday.  It comes from Exceed and is
   *   defined differently */
    gettimeofday(&tv,0);
#else
    gettimeofday(&tv,0);
#endif
    clock=tv.tv_sec;
    strcpy(time,ctime(&clock));
    time[24]='\0';

  /* Make contents */    
    sprintf(string,
      "Time:         %s\n"
      "PV file:      %s\n"
      "Lattice file: %s\n"
      "Reference file: %s\n\n"
      "Accumulated History Time (min): %.1f    Nsymbols=%d\n",
      time,pvfilename,latfilename,reffilename,nstattime/60000.,nsymbols);
    ptr=string+strlen(string);
    sprintf(ptr,"Array Nvals    SDEV     AVG     MAX (Accumulated)\n");
    for(ia=0; ia < narrays; ia++) {
	ptr=string+strlen(string);
	sprintf(ptr,"%5d %5d % 7.3f % 7.3f %7.3f\n",ia+1,arrays[ia].nvals,
	  arrays[ia].runsdev/nstat,arrays[ia].runavg/nstat,arrays[ia].runmax);
    }
    ptr=string+strlen(string);
    sprintf(ptr,
      "\n"
      "Slot  Time                      File\n");
    cstring=XmStringCreateLtoR(string,XmSTRING_DEFAULT_CHARSET);
    for(i=0; i < NSAVE; i++) {
	cstring1=cstring;
	sprintf(string,"%3d   %.24s  %s\n",i+1,savetime[i],savefilename[i]);
	cstring2=XmStringCreateLtoR(string,XmSTRING_DEFAULT_CHARSET);
	cstring=XmStringConcat(cstring1,cstring2);
	XmStringFree(cstring1);
	XmStringFree(cstring2);
    }
    
    nargs=0;
    XtSetArg(args[nargs],XmNtitle,"Status"); nargs++;
    XtSetArg(args[nargs],XmNmessageString,cstring); nargs++;
#ifndef WIN32
    /* 6/12/2012 Xming hangs */
    w=XmCreateInformationDialog(mainwin,"informationMessage",args,nargs);
#else
    w=XmCreateMessageDialog(mainwin,"informationMessage",args,nargs);
#endif
    XmStringFree(cstring);
    w1=XmMessageBoxGetChild(w,XmDIALOG_CANCEL_BUTTON);
    XtDestroyWidget(w1);
    w1=XmMessageBoxGetChild(w,XmDIALOG_HELP_BUTTON);
    XtDestroyWidget(w1);
    XtManageChild(w);
    XtAddCallback(w,XmNokCallback,destroywidget,NULL);
}
/**************************** work ****************************************/
Boolean work(XtPointer clientdata)
{
    if(ecaconnected) ecashutdown();
    if(!ecastartup()) {
	if(ecaconnected) ecashutdown();
	cstring=XmStringCreateLocalized("Start");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(echoecabutton,args,nargs);
	XmStringFree(cstring);
    } else {
	cstring=XmStringCreateLocalized("Stop");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(echoecabutton,args,nargs);
	XmStringFree(cstring);
    }
    return(TRUE);     /* Remove it afterwards */
}
