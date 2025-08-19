/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* menu.c *** Menu routines for ADT */

#define DEBUG_XERROR 0
#define DEBUG_SDDS 0

#ifdef WIN32
#include <windows.h>
#endif

#include "adt.h"

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

/**************************** makefilemenu ********************************/
void makefilemenu(void)
{
    Widget w,w1,wparent;
    int i;

    
  /* Define file menu */
    wparent=mainmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"fileMenu",args,nargs);
    filemenu=w1;
#if DEBUG_XERROR
    debug1("filemenu",filemenu);
#endif

    
  /* Define load menu */
    wparent=filemenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"loadMenu",args,nargs);
    loadmenu=w1;
#if DEBUG_XERROR
    debug1("loadmenu",loadmenu);
#endif    
    
#if DEBUG_SDDS
#if DEBUG_XERROR
    print("Init file not read: %s\n",initfilename);
#endif    
#else
  /* Read init file to get items for load menu*/    
    if(!readinit(initfilename)) {
	fprintf(stderr,"Problems reading initialization file: %s\n",
	  initfilename);
	exit(1);
    }
#if DEBUG_XERROR
    print("Init file read: %s\n",initfilename);
#endif    
#endif    

  /* Separator */
    wparent=w1;
    nargs=0;
    w=XmCreateSeparator(wparent,"separator",args,nargs);
    XtManageChild(w);
#if DEBUG_XERROR
    debug1("separator",w);
#endif    

  /* Custom */
    wparent=w1;
    nargs=0;
    w=XmCreatePushButton(wparent,"Custom...",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,openmenucustomcb,(XtPointer)0);
#if DEBUG_XERROR
    debug1("Custom... button",w);
#endif    
    
  /* Define load menu button */
    wparent=filemenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"Load",args,nargs);
    XtManageChild(w);
#if DEBUG_XERROR
    debug1("Load button",w);
#endif    
    
  /* Separator */
    wparent=filemenu;
    nargs=0;
    w=XmCreateSeparator(w1,"separator",args,nargs);
    XtManageChild(w);
#if DEBUG_XERROR
    debug1("separator",w);
#endif    
    
  /* Reference */
    wparent=filemenu;
    nargs=0;
    w=XmCreatePushButton(wparent,"Read Reference",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,filereferencecb,(XtPointer)"0");
#if DEBUG_XERROR
    debug1("Read Reference button",w);
#endif    
    
  /* Define read snapshot menu */
    wparent=filemenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"readSnapMenu",args,nargs);
#if DEBUG_XERROR
    debug1("Read Snapshot menu",w);
#endif    
    
    for(i=0; i < NSAVE; i++) {
	wparent=w1;
	nargs=0;
	w=XmCreatePushButton(wparent,savenumbers[i],args,nargs);
	XtManageChild(w);
	XtAddCallback(w,XmNactivateCallback,filereadcb,
	  (XtPointer)savenumbers[i]);
#if DEBUG_XERROR
	debug1("Read Snapshot button",w);
#endif    
    }

  /* Define read snapshot menu button */
    wparent=filemenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"Read Snapshot",args,nargs);
    XtManageChild(w);
    
  /* Define write menu */
    wparent=filemenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"writeMenu",args,nargs);

    nargs=0;
    wparent=w1;
    w=XmCreatePushButton(wparent,"Current",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,filewritecb,(XtPointer)"0");
    
    for(i=0; i < NSAVE; i++) {
	wparent=w1;
	nargs=0;
	w=XmCreatePushButton(wparent,savenumbers[i],args,nargs);
	XtManageChild(w);
	XtAddCallback(w,XmNactivateCallback,filewritecb,
	  (XtPointer)savenumbers[i]);
    }
    
  /* Define write menu button */
    wparent=filemenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"Write",args,nargs);
    XtManageChild(w);
    

  /* Define plot menu */
    wparent=filemenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"plotMenu",args,nargs);

    nargs=0;
    wparent=w1;
    w=XmCreatePushButton(wparent,"Current",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,fileplotcb,(XtPointer)"0");
    
    for(i=0; i < NSAVE; i++) {
	wparent=w1;
	nargs=0;
	w=XmCreatePushButton(wparent,savenumbers[i],args,nargs);
	XtManageChild(w);
	XtAddCallback(w,XmNactivateCallback,fileplotcb,
	  (XtPointer)savenumbers[i]);
    }
    
  /* Define plot menu button */
    wparent=filemenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"Plot",args,nargs);
    XtManageChild(w);
    
  /* Status */
    wparent=filemenu;
    nargs=0;
    w=XmCreatePushButton(wparent,"Status...",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,viewcb,(XtPointer)0);
    
  /* Separator */
    wparent=filemenu;
    nargs=0;
    w=XmCreateSeparator(wparent,"separator",args,nargs);
    XtManageChild(w);
    
  /* Quit */
    wparent=filemenu;
    nargs=0;
    w=XmCreatePushButton(wparent,"Quit",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,quitfunc,(XtPointer)NULL);

  /* Define file button */
    wparent=mainmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,filemenu); nargs++;
    w=XmCreateCascadeButton(wparent,"File",args,nargs);
    XtManageChild(w);
    filebutton=w;
#if DEBUG_XERROR
    print("filebutton=%x parent=%x window=%x NextRequest=%ld\n",
      w,XtParent(w),XtWindow(w),NextRequest(display));
#endif    
}
/**************************** makeoptionsmenu *****************************/
void makeoptionsmenu(void)
{
    Widget w,w1,wparent;
    int i;
    
  /* Define options menu */
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(mainmenu,"optionsMenu",args,nargs);
    optmenu=w1;

  /* Define channel access menu */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"epicsMenu",args,nargs);
    ecamenu=w1;

    nargs=0;
    wparent=w1;
    w=XmCreatePushButton(wparent,ecaresponding?"Stop":"Start",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,ecacb,(XtPointer)0);
    echoecabutton=w;
    
    nargs=0;
    wparent=w1;
    w=XmCreatePushButton(wparent,"Reinitialize",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,ecacb,(XtPointer)1);
    
    nargs=0;
    wparent=w1;
    w=XmCreatePushButton(wparent,"Rescan",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,ecacb,(XtPointer)2);
    
    nargs=0;
    wparent=w1;
    w=XmCreatePushButton(wparent,"Exit",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,ecacb,(XtPointer)3);
    
  /* Define channel access button */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"EPICS",args,nargs);
    XtManageChild(w);
    
  /* Define store menu */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"storeMenu",args,nargs);
    
    for(i=0; i < NSAVE; i++) {
	wparent=w1;
	nargs=0;
	w=XmCreatePushButton(wparent,savenumbers[i],args,nargs);
	XtManageChild(w);
	XtAddCallback(w,XmNactivateCallback,orbitsavecb,
	  (XtPointer)savenumbers[i]);
    }
    
  /* Define store menu button */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"Store",args,nargs);
    XtManageChild(w);
    
  /* Define display menu */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"displayMenu",args,nargs);
    
    nargs=0;
    w=XmCreatePushButton(wparent,"Off",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,orbitrestcb,(XtPointer)"0");
    
    for(i=0; i < NSAVE; i++) {
	wparent=w1;
	nargs=0;
	w=XmCreatePushButton(wparent,savenumbers[i],args,nargs);
	XtManageChild(w);
	XtAddCallback(w,XmNactivateCallback,orbitrestcb,
	  (XtPointer)savenumbers[i]);
    }
    
  /* Define display menu button */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"Display",args,nargs);
    XtManageChild(w);
    
  /* Define difference menu */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"differenceMenu",args,nargs);
    
    wparent=w1;
    nargs=0;
    w=XmCreatePushButton(wparent,"Off",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,orbitdiffcb,(XtPointer)"0");
    
    for(i=0; i < NSAVE; i++) {
	wparent=w1;
	nargs=0;
	w=XmCreatePushButton(wparent,savenumbers[i],args,nargs);
	XtManageChild(w);
	XtAddCallback(w,XmNactivateCallback,orbitdiffcb,
	  (XtPointer)savenumbers[i]);
    }

  /* Define difference menu button */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"Difference",args,nargs);
    XtManageChild(w);
    
  /* Define check status menu */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(wparent,"statusMenu",args,nargs);

    wparent=w1;
    nargs=0;
    w=XmCreatePushButton(wparent,"Off",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,optcb,(XtPointer)7);
    
    wparent=w1;
    nargs=0;
    w=XmCreatePushButton(wparent,"Check InValid",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,optcb,(XtPointer)8);
    
    wparent=w1;
    nargs=0;
    w=XmCreatePushButton(wparent,"Check All",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,optcb,(XtPointer)9);
    
  /* Define check status menu button */
    wparent=optmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(wparent,"Check Status",args,nargs);
    checkstatusw=w;
    XtSetSensitive(checkstatusw,False);
    XtManageChild(w);
    
  /* Define other buttons */    
    wparent=optmenu;
    nargs=0;
    w=XmCreateToggleButton(wparent,"Reference Enabled",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,optcb,(XtPointer)5);
    XmToggleButtonSetState(w,(Boolean)refon,False);
    
    wparent=optmenu;
    nargs=0;
    w=XmCreateToggleButton(wparent,"Zoom Enabled",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,optcb,(XtPointer)6);
    XmToggleButtonSetState(w,(Boolean)zoomon,False);
    
    wparent=optmenu;
    nargs=0;
    w=XmCreateToggleButton(wparent,"Accumulated Statistics",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,optcb,(XtPointer)10);
    XmToggleButtonSetState(w,(Boolean)statmode,False);
    
    wparent=optmenu;
    nargs=0;
    w=XmCreatePushButton(wparent,"Reset Max/Min",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,viewcb,(XtPointer)6);

  /* Define options button */
    wparent=mainmenu;
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,optmenu); nargs++;
    w=XmCreateCascadeButton(wparent,"Options",args,nargs);
    XtManageChild(w);
}
/**************************** makeviewmenu ********************************/
void makeviewmenu(void)
{
    Widget w,w1;
    
  /* Define view menu */
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(mainmenu,"viewMenu",args,nargs);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Timing...",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,viewcb,(XtPointer)1);
    
    nargs=0;
    w=XmCreateToggleButton(w1,"Markers",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,viewcb,(XtPointer)2);
    XmToggleButtonSetState(w,(Boolean)markers,False);
    wmarkers=w;
    
    nargs=0;
    w=XmCreateToggleButton(w1,"Lines",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,viewcb,(XtPointer)7);
    XmToggleButtonSetState(w,(Boolean)lines,False);
    wlines=w;
    
    nargs=0;
    w=XmCreateToggleButton(w1,"Bars",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,viewcb,(XtPointer)8);
    XmToggleButtonSetState(w,(Boolean)lines,False);
    wbars=w;
    
    nargs=0;
    w=XmCreateToggleButton(w1,"Grid",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,viewcb,(XtPointer)4);
    XmToggleButtonSetState(w,(Boolean)grid,False);
    wgrid=w;
    
    nargs=0;
    w=XmCreateToggleButton(w1,"Max/Min",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,viewcb,(XtPointer)5);
    XmToggleButtonSetState(w,(Boolean)showmaxmin,False);
    wshowmaxmin=w;
    
    nargs=0;
    w=XmCreateToggleButton(w1,"Filled Max/Min",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,viewcb,(XtPointer)9);
    XmToggleButtonSetState(w,(Boolean)fillmaxmin,False);
    wfillmaxmin=w;

  /* Define view button */
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(mainmenu,"View",args,nargs);
    XtManageChild(w);
}
/**************************** makeclearmenu *******************************/
void makeclearmenu(void)
{
    Widget w,w1;
    
  /* Define clear menu */
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(mainmenu,"clearMenu",args,nargs);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Clear",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,clearcb,(XtPointer)1);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Redraw",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,clearcb,(XtPointer)2);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Update",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,clearcb,(XtPointer)3);
    
    nargs=0;
    w=XmCreateToggleButton(w1,"Autoclear",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNvalueChangedCallback,clearcb,(XtPointer)4);
    XmToggleButtonSetState(w,(Boolean)autoclear,False);

  /* Define clear button */
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(mainmenu,"Clear",args,nargs);
    XtManageChild(w);
}
/**************************** makehelpmenu ********************************/
void makehelpmenu(void)
{
    Widget w,w1;
    
  /* Define help menu */
    nargs=0;
    XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
    w1=XmCreatePulldownMenu(mainmenu,"helpMenu",args,nargs);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Contents",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,helpcb,(XtPointer)1);
    
    nargs=0;
    w=XmCreatePushButton(w1,"New Features",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,helpcb,(XtPointer)2);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Overview",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,helpcb,(XtPointer)3);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Mouse Operations",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,helpcb,(XtPointer)4);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Color Code",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,helpcb,(XtPointer)5);
    
    nargs=0;
    w=XmCreatePushButton(w1,"Version",args,nargs);
    XtManageChild(w);
    XtAddCallback(w,XmNactivateCallback,helpcb,(XtPointer)10);

  /* Define help button */
    nargs=0;
    XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
    w=XmCreateCascadeButton(mainmenu,"Help",args,nargs);
    XtManageChild(w);
    helpbutton=w;
    
    nargs=0;
    XtSetArg(args[nargs],XmNmenuHelpWidget,w); nargs++;
    XtSetValues(mainmenu,args,nargs);
    
}
