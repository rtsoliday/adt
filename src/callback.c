/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* callback.c *** Callback routines for ADT */

#define DEBUG_PLOT 0

#include "adt.h"

#ifdef WIN32
# include <process.h>
#endif

/* Prototype for mktemp, which should be in stdlib.h but may not
   because of #ifdef's.  Should probably rewrite fileplotcb */
extern char *mktemp(char *);

/**************************** clearcb *************************************/
void clearcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int ia;

    switch((intptr_t)clientdata) {
    case 1:
	for(ia=0; ia < nareas; ia++) {
	    areas[ia].tempnodraw=1;
	    areas[ia].tempclear=1;
	}
	resetgraph();
	break;
    case 2:
	for(ia=0; ia < nareas; ia++) {
	    areas[ia].tempclear=1;
	}
	resetgraph();
	break;
    case 3:
	resetgraph();
	break;
    case 4:
	autoclear=XmToggleButtonGetState(w)?1:0;
	break;
    }
}
/**************************** fileplotcb **********************************/
void fileplotcb(Widget w, XtPointer clientdata, XtPointer calldata)
#ifdef WIN32
{
  /* Implementation for WIN32 *********************************************/
    int n;
    intptr_t status;
    int nsave;
    char templet[]="ADTPlot";
    time_t seconds;
    char tmpfilename[PATH_MAX];
    static int first=1;
    static char *tempdir = NULL;
    static char *ComSpec = NULL;

  /* Check if there are any arrays */
    if(!narrays) {
	xerrmsg("There are no PV's defined");
	return;
    }
    n=atoi((char *)clientdata);
    nsave=n-1;
  /* Get environment variables the first time */
    if (first) {
	first=0;
	ComSpec = getenv("ComSpec");
	tempdir = getenv("TEMP");
	if(!tempdir) tempdir = getenv("TMP");
    }
    if(!ComSpec) {
	xerrmsg("ComSpec environmental variable not found");
	return;
    }
  /* Make temporary file */
    seconds=time(NULL);
    sprintf(tmpfilename,"%s%s%s%d.sdds",
      tempdir?tempdir:"",
      tempdir?"\\":"",
      templet,seconds);
  /* Write it */
    if(!writeplotfile(tmpfilename,nsave)) return;
  /* Create command */
    status=_spawnlp(_P_NOWAIT,"sddsplot","sddsplot",
      "-graph=sym,connect",
      "-layout=1,2",
      "-split=page",
      "-sep",
      "-ylabel=@TimeStamp",
      "-xlabel=Index",
      "-column=Index,Value",
      tmpfilename,
      "-title=@ADTScreenTitle",
      "-end",
      NULL);
    if(status == -1) {
	char *errstring=strerror(errno);
	
	print("Cannot run Sddsplot:\n"
	  "  %s\n",errstring);
	return;
    }
}
#else
{
  /* Implementation for not WIN32 *****************************************/
    int n;
    static int nsave;
    char *templet="/tmp/ADT.XXXXXX";
    char tmpfilename[24];     /* Big enough to hold templet */
    int pid;

  /* Check if there are any arrays */
    if(!narrays) {
	xerrmsg("There are no PV's defined");
	return;
    }
    n=atoi((char *)clientdata);
    nsave=n-1;
  /* Make temporary file */
    strcpy(tmpfilename,templet);
    mktemp(tmpfilename);
  /* Write it */
    if(!writeplotfile(tmpfilename,nsave)) return;
  /* Create command */
    if(checkstatus) {
	sprintf(string,
	  "sddsplot -graph=sym,connect -layout=1,2 -split=page -sep"
	  " -ylabel=@TimeStamp -xlabel=Index -column=Index,Value"
	  " %s -title=@ADTScreenTitle -end ; \\rm %s",
	  tmpfilename,tmpfilename);
    }
    else {
	sprintf(string,
	  "sddsplot -graph=sym,connect -layout=1,2 -split=page -sep"
	  " -ylabel=@TimeStamp -xlabel=Index -column=Index,Value"
	  " %s -title=@ADTScreenTitle -end ; \\rm %s",
	  tmpfilename,tmpfilename);
    }
#if DEBUG_PLOT
    printf("%s\n",string);
#endif    
  /* Run command */
    if((pid=fork()) == 0) {
	signal(SIGINT,SIG_DFL);
	signal(SIGQUIT,SIG_DFL);
	signal(SIGHUP,SIG_DFL);
	execl("/bin/sh","sh","-c",string,(char *)0);
	perror("ADT: ");   /* Only gets here if error */
	unlink(tmpfilename);
	_exit(127);
    }
    else if(pid == -1) {
	xerrmsg("Cannot execute SDDSPlot");
    }
}
#endif
/**************************** filereadcb **********************************/
void filereadcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    XmFileSelectionBoxCallbackStruct *cbs=
      (XmFileSelectionBoxCallbackStruct *)calldata;
    static Widget wdialog;
    int n;
    static int nsave;

    n=atoi((char *)clientdata);
    if (n > 0 && n <= NSAVE) {
	XmString pattern,directory;

      /* Check if there are any arrays */
	if(!narrays) {
	    xerrmsg("There are no PV's defined");
	    return;
	}
      /* Make dialog box */
	nsave=n-1;
	pattern=XmStringCreateLocalized(SAVEPATTERN);
	directory=XmStringCreateLocalized(snapdirectory);
	nargs=0;
	XtSetArg(args[nargs],XmNtitle,"Read Snapshot File"); nargs++;
	XtSetArg(args[nargs],XmNpattern,pattern); nargs++;
	XtSetArg(args[nargs],XmNdirectory,directory); nargs++;
	wdialog=XmCreateFileSelectionDialog(mainwin,"fileSelection",
	  args,nargs);
	XmStringFree(pattern);
	XmStringFree(directory);
	XtAddCallback(wdialog,XmNcancelCallback,destroywidget,NULL);
	XtAddCallback(wdialog,XmNokCallback,filereadcb,(XtPointer)"100");
	XtSetSensitive(XmSelectionBoxGetChild(wdialog,
	  XmDIALOG_HELP_BUTTON),FALSE);
	XtManageChild(wdialog);
    }
    else if (n == 100) {
	char *filename,*dir;

	if(!XmStringGetLtoR(cbs->value,XmSTRING_DEFAULT_CHARSET,&filename)) {
	    xerrmsg("Error getting filename");
	    return;
	}
	if(!*filename) {
	    xerrmsg("File name is blank");
	    XtFree(filename);
	    return;
	}
      /* Get the pattern the user left */
	if(XmStringGetLtoR(cbs->dir,XmSTRING_DEFAULT_CHARSET,&dir)) {
	    strcpy(snapdirectory,dir);
	    XtFree(dir);
	}
      /* Cleanup */
	XtDestroyWidget(wdialog);
	readsnap(filename,nsave);
	XtFree(filename);
    }
    else {
	xerrmsg("Received invalid case in File/Read callback: %d",n);
    }
}
/**************************** filereferencecb *****************************/
void filereferencecb(Widget w, XtPointer clientdata,  XtPointer calldata)
{
    XmFileSelectionBoxCallbackStruct *cbs=
      (XmFileSelectionBoxCallbackStruct *)calldata;
    static Widget wdialog;
    int n;

    n=atoi((char *)clientdata);
    if (n == 0) {
	XmString pattern,directory;

      /* Check if there are any arrays */
	if(!narrays) {
	    xerrmsg("There are no PV's defined");
	    return;
	}
	pattern=XmStringCreateLocalized(SAVEPATTERN);
	directory=XmStringCreateLocalized(snapdirectory);
	nargs=0;
	XtSetArg(args[nargs],XmNtitle,"Read Reference File"); nargs++;
	XtSetArg(args[nargs],XmNpattern,pattern); nargs++;
	XtSetArg(args[nargs],XmNdirectory,directory); nargs++;
	wdialog=XmCreateFileSelectionDialog(mainwin,"fileSelection",
	  args,nargs);
	XmStringFree(pattern);
	XmStringFree(directory);
	XtAddCallback(wdialog,XmNcancelCallback,destroywidget,NULL);
	XtAddCallback(wdialog,XmNokCallback,filereferencecb,(XtPointer)"100");
	XtSetSensitive(XmSelectionBoxGetChild(wdialog,
	  XmDIALOG_HELP_BUTTON),FALSE);
	XtManageChild(wdialog);
    }
    else if (n == 100) {
	char *filename,*dir;

	if(!XmStringGetLtoR(cbs->value,XmSTRING_DEFAULT_CHARSET,&filename)) {
	    xerrmsg("Error getting filename");
	    return;
	}
	if(!*filename) {
	    xerrmsg("File name is blank");
	    XtFree(filename);
	    return;
	}
      /* Get the pattern the user left */
	if(XmStringGetLtoR(cbs->dir,XmSTRING_DEFAULT_CHARSET,&dir)) {
	    strcpy(snapdirectory,dir);
	    XtFree(dir);
	}
      /* Cleanup */
	XtDestroyWidget(wdialog);
        if(!readreference(filename)) {
            xerrmsg("Could not read reference data\n"
              "Referencing is inactive");
            reference=0;
        }
        else {
            reference=1;
            maketitle();
            snprintf(reffilename,PATH_MAX,"%s",filename);
        }
	XtFree(filename);
    }
    else {
	xerrmsg("Received invalid case in File/Reference callback: %d",n);
    }
}
/**************************** filewritecb *********************************/
void filewritecb(Widget w, XtPointer clientdata,  XtPointer calldata)
{
    XmFileSelectionBoxCallbackStruct *cbs=
      (XmFileSelectionBoxCallbackStruct *)calldata;
    static Widget wdialog;
    int n;
    static int nsave;

    n=atoi((char *)clientdata);
    if (n >= 0 && n <= NSAVE) {
	XmString pattern,directory;

      /* Check if there are any arrays */
	if(!narrays) {
	    xerrmsg("There are no PV's defined");
	    return;
	}
      /* Make dialog box */
	nsave=n-1;
	pattern=XmStringCreateLocalized(SAVEPATTERN);
	directory=XmStringCreateLocalized(snapdirectory);
	nargs=0;
	XtSetArg(args[nargs],XmNtitle,"Write Snapshot File"); nargs++;
	XtSetArg(args[nargs],XmNpattern,pattern); nargs++;
	XtSetArg(args[nargs],XmNdirectory,directory); nargs++;
	wdialog=XmCreateFileSelectionDialog(mainwin,"fileSelection",
	  args,nargs);
	XmStringFree(pattern);
	XmStringFree(directory);
	XtAddCallback(wdialog,XmNcancelCallback,destroywidget,NULL);
	XtAddCallback(wdialog,XmNokCallback,filewritecb,(XtPointer)"100");
	XtSetSensitive(XmSelectionBoxGetChild(wdialog,
	  XmDIALOG_HELP_BUTTON),FALSE);
	XtManageChild(wdialog);
    }
    else if (n == 100) {
	char *filename,*dir;

	if(!XmStringGetLtoR(cbs->value,XmSTRING_DEFAULT_CHARSET,&filename)) {
	    xerrmsg("Error getting filename");
	    return;
	}
	if(!*filename) {
	    xerrmsg("File name is blank");
	    XtFree(filename);
	    return;
	}
      /* Get the pattern the user left */
	if(XmStringGetLtoR(cbs->dir,XmSTRING_DEFAULT_CHARSET,&dir)) {
	    strcpy(snapdirectory,dir);
	    XtFree(dir);
	}
      /* Cleanup */
	XtDestroyWidget(wdialog);
      /* Write it */
	if(!writesnap(filename,nsave)) return;
	XtFree(filename);
    }
    else {
	xerrmsg("Received invalid case in File/Write callback: %d",n);
    }
}
/**************************** graphcontrolcb ******************************/
void graphcontrolcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int iarea,len,n;
    char *value;
    double scaleval;

    iarea=((intptr_t)clientdata)/100;
    scaleval=scale[areas[iarea].curscale];
    n=(intptr_t)clientdata-100*iarea;
    switch(n) {
    case 1:     /* Center value */
	value=XmTextFieldGetString(areas[iarea].wcenter);
	areas[iarea].centerval=atof(value);
	XFree(value);
	areas[iarea].xmax=(areas[iarea].centerval+scaleval*GRIDDIVISIONS);
	areas[iarea].xmin=(areas[iarea].centerval-scaleval*GRIDDIVISIONS);
	break;
    case 2:     /* Up arrow */
	areas[iarea].centerval+=scaleval;
	areas[iarea].xmax+=scaleval;
	areas[iarea].xmin+=scaleval;
	break;
    case 3:     /* Down arrow */
	areas[iarea].centerval-=scaleval;
	areas[iarea].xmax-=scaleval;
	areas[iarea].xmin-=scaleval;
	break;
    }
/* Change the value */
  /* Fix it so very small values are written as zero */
    if(fabs(areas[iarea].centerval) < 1.e-10*scaleval)
      areas[iarea].centerval=0.;
    sprintf(string,"% #11.4g",areas[iarea].centerval);
    len=(int)strlen(string);
    nargs=0;
    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
    XtSetArg(args[nargs],XmNvalue,string); nargs++;
    XtSetValues(areas[iarea].wcenter,args,nargs);
    areas[iarea].tempclear=1;
    resizegraph(areas[iarea].wgrapharea,(XtPointer)(intptr_t)areas[iarea].area,(XtPointer)0);
}
/**************************** helpcb **************************************/
void helpcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int n;
    n=(intptr_t)clientdata;
    
    switch(n) {
    case 0:
	XtDestroyWidget(helppopup);
	break;
    case 1:
	callBrowser(browserfile,"#contents");
	break;
    case 2:
	callBrowser(browserfile,"#new");
	break;
    case 3:
	callBrowser(browserfile,"#overview");
	break;
    case 4:
	callBrowser(browserfile,"#mouse");
	break;
    case 5:
	callBrowser(browserfile,"#color");
	break;
    case 10:
	xinfomsg("%s %s \n\nWritten by Dwarfs in the Waterfall Glen\n\n%s",
	  ADT_VERSION_STRING,EPICS_VERSION_STRING,unamestring);
	break;
    default:
	break;
    }
}
/**************************** openmenucb **********************************/
void openmenucb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    char *filename=(char *)clientdata;;
    
    strcpy(pvfilename,filename);
    if(ecaconnected) ecashutdown();
    if(!ecastartup()) {
	if(ecaconnected) ecashutdown();
	cstring=XmStringCreateLocalized("Start");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(echoecabutton,args,nargs);
	XmStringFree(cstring);
    }
    else {
	cstring=XmStringCreateLocalized("Stop");
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(echoecabutton,args,nargs);
	XmStringFree(cstring);
    }
}
/**************************** openmenucustomcb ****************************/
void openmenucustomcb(Widget w, XtPointer clientdata,  XtPointer calldata)
{
    XmFileSelectionBoxCallbackStruct *cbs=
      (XmFileSelectionBoxCallbackStruct *)calldata;
    static Widget wdialog;
    int n;

    n=(intptr_t)clientdata;
    if(n == 0) {
	XmString pattern=XmStringCreateLocalized(PVPATTERN);
	XmString directory=XmStringCreateLocalized(customdirectory);
	
/* Ask for PV filename */
	nargs=0;
	XtSetArg(args[nargs],XmNtitle,"Choose PV File"); nargs++;
	XtSetArg(args[nargs],XmNpattern,pattern); nargs++;
	XtSetArg(args[nargs],XmNdirectory,directory); nargs++;
	wdialog=XmCreateFileSelectionDialog(mainwin,"fileSelection",args,nargs);
	XmStringFree(pattern);
	XmStringFree(directory);
	XtAddCallback(wdialog,XmNcancelCallback,destroywidget,NULL);
	XtAddCallback(wdialog,XmNokCallback,openmenucustomcb,(XtPointer)100);
	XtSetSensitive(XmSelectionBoxGetChild(wdialog,XmDIALOG_HELP_BUTTON),FALSE);
	XtManageChild(wdialog);
    }
    else if(n == 100) {
	char *filename,*dir;
	
      /* Record PV filename */
	if(!XmStringGetLtoR(cbs->value,XmSTRING_DEFAULT_CHARSET,&filename)) {
	    xerrmsg("Error getting filename");
	    return;
	}
	if(!*filename) {
	    xerrmsg("File name is blank");
	    XtFree(filename);
	    return;
	}
	XtDestroyWidget(wdialog);
	strcpy(pvfilename,filename);
	XtFree(filename);
	
      /* Get the pattern the user left */
	if(XmStringGetLtoR(cbs->dir,XmSTRING_DEFAULT_CHARSET,&dir)) {
	    strcpy(customdirectory,dir);
	    XtFree(dir);
	}
      /* Open the connection */
	if(ecaconnected) ecashutdown();
	if(!ecastartup()) {
	    if(ecaconnected) ecashutdown();
	    cstring=XmStringCreateLocalized("Start");
	    nargs=0;
	    XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	    XtSetValues(echoecabutton,args,nargs);
	    XmStringFree(cstring);
	}
	else {
	    cstring=XmStringCreateLocalized("Stop");
	    nargs=0;
	    XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	    XtSetValues(echoecabutton,args,nargs);
	    XmStringFree(cstring);
	}
    }     /* End if(n== 100) */
    else {
	xerrmsg("Received invalid case in Open/Custom callback: %d",n);
    }
}
/**************************** optcb ***************************************/
void optcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int i;

    switch((intptr_t)clientdata) {
    case 5:
	refon=XmToggleButtonGetState(w)?1:0;
	maketitle();
	resetgraph();
	break;
    case 6:
	zoomon=XmToggleButtonGetState(w)?1:0;
	if(zoom) {
	    if(zoomon) {
		nargs=0;
		XtSetArg(args[nargs],XmNfractionBase,10*(nareas+1)); nargs++;
		XtSetValues(graphform,args,nargs);
		XtManageChild(zoomform);
	    }
	    else {
		nargs=0;
		XtSetArg(args[nargs],XmNfractionBase,10*nareas); nargs++;
		XtSetValues(graphform,args,nargs);
		XtUnmanageChild(zoomform);
	    }
	}
	break;
    case 7:
	checkstatusmode=0;
	break;
    case 8:
	checkstatusmode=1;
	break;
    case 9:
	checkstatusmode=2;
	break;
    case 10:
	statmode=XmToggleButtonGetState(w)?1:0;
	for(i=0; i < nareas; i++) areas[i].tempclear=1;
	resetgraph();
	break;
    }
}
/**************************** orbitdiffcb *********************************/
void orbitdiffcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int ia,iarea,n,nsave,len;
    
    n=atoi((char *)clientdata);
    if (n > 0 && n <= NSAVE) {
      /* Check if there are any arrays */
	if(!narrays) {
	    xerrmsg("There are no PV's defined");
	    return;
	}
      /* Check if there is saved data */
	nsave=n-1;
	for(ia=0; ia < narrays; ia++) {
	    if(!arrays[ia].savevals[nsave]) {
		xerrmsg("Data is not defined");
		return;
	    }
	}
      /* Reset centerval */
	for(iarea=0; iarea < nareas; iarea++) {
	    areas[iarea].oldcenterval=areas[iarea].centerval;
	    areas[iarea].centerval=0.;
	    sprintf(string,"% #11.4g",areas[iarea].centerval);
	    len=(int)strlen(string);
	    nargs=0;
	    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
	    XtSetArg(args[nargs],XmNvalue,string); nargs++;
	    XtSetValues(areas[iarea].wcenter,args,nargs);
	    areas[iarea].tempclear=1;
	}
	if(zoom) {
	    zoomarea->oldcenterval=zoomarea->centerval;
	    zoomarea->centerval=0.;
	    sprintf(string,"% #11.4g",zoomarea->centerval);
	    len=(int)strlen(string);
	    nargs=0;
	    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
	    XtSetArg(args[nargs],XmNvalue,string); nargs++;
	    XtSetValues(zoomarea->wcenter,args,nargs);
	    zoomarea->tempclear=1;
	    resizezoom((Widget)0,(XtPointer)0,(XtPointer)0);
	}
      /* Implement */
	whichorbit=nsave;
	maketitle();
    }
    else {
      /* Reset centerval */
	for(iarea=0; iarea < nareas; iarea++) {
	    areas[iarea].centerval=areas[iarea].oldcenterval;
	    sprintf(string,"% #11.4g",areas[iarea].centerval);
	    len=(int)strlen(string);
	    nargs=0;
	    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
	    XtSetArg(args[nargs],XmNvalue,string); nargs++;
	    XtSetValues(areas[iarea].wcenter,args,nargs);
	    areas[iarea].tempclear=1;
            resizegraph(areas[iarea].wgrapharea,(XtPointer)(intptr_t)areas[iarea].area,(XtPointer)0);
	}
	if(zoom) {
	    zoomarea->centerval=zoomarea->oldcenterval;
	    sprintf(string,"% #11.4g",zoomarea->centerval);
	    len=(int)strlen(string);
	    nargs=0;
	    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
	    XtSetArg(args[nargs],XmNvalue,string); nargs++;
	    XtSetValues(zoomarea->wcenter,args,nargs);
	    zoomarea->tempclear=1;
	    resizezoom((Widget)0,(XtPointer)0,(XtPointer)0);
	}
	whichorbit=-1;
	maketitle();
    }
}
/**************************** orbitrestcb *********************************/
void orbitrestcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int ia,n,nsave;
    
    n=atoi((char *)clientdata);
    if (n > 0 && n <= NSAVE) {
      /* Check if there are any arrays */
	if(!narrays) {
	    xerrmsg("There are no PV's defined");
	    return;
	}
      /* Check if there is saved data */
	nsave=n-1;
	for(ia=0; ia < narrays; ia++) {
	    if(!arrays[ia].savevals[nsave]) {
		xerrmsg("Data is not defined");
		return;
	    }
	}
      /* Implement */
	displaysave=n;
	resetgraph();
    }
    else {
	displaysave=0;
	resetgraph();
    }
}
/**************************** orbitsavecb *********************************/
void orbitsavecb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    struct timeval tv;
    time_t clock;
    int i,ia,n,nsave;
    
    n=atoi((char *)clientdata);
    if (n > 0 && n <= NSAVE) {
      /* Check if there are any arrays */
	if(!narrays) {
	    xerrmsg("There are no PV's defined");
	    return;
	}
      /* Allocate space if needed and fill in values */
	nsave=n-1;
	for(ia=0; ia < narrays; ia++) {
	    if(!arrays[ia].savevals[nsave]) {
		arrays[ia].savevals[nsave]=
		  (double *)calloc(arrays[ia].nvals,sizeof(double));
		if(arrays[ia].savevals[nsave] == NULL) {
		    xerrmsg("Could not allocate space for saved set %d "
		      "for arrray %d",n,ia+1);
		    return;
		}
	    }
	    for(i=0; i < arrays[ia].nvals; i++)
	      arrays[ia].savevals[nsave][i]=arrays[ia].vals[i];
	}
      /* Set time */
	gettimeofday(&tv,NULL);
	clock=tv.tv_sec;
	strcpy(savetime[nsave],ctime(&clock));
	savetime[nsave][24]='\0';
      /* set filename to blank */
	strcpy(savefilename[nsave],"");
    }
}
/**************************** quit ****************************************/
void quitfunc(Widget w, XtPointer clientdata, XtPointer calldata)
{
    if(ecaresponding) ecashutdown();
    callBrowser("quit","");
    XtDestroyWidget(appshell);
    exit(exitcode);
}
/**************************** scalemenucb *********************************/
void scalemenucb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int iarea,iscale;

    iarea=((intptr_t)clientdata)/100;
    iscale=(intptr_t)clientdata-100*iarea;
    if(iarea < nareas) {
	areas[iarea].curscale=iscale;
	areas[iarea].xmax=(areas[iarea].centerval+
	  scale[areas[iarea].curscale]*GRIDDIVISIONS);
	areas[iarea].xmin=(areas[iarea].centerval-
	  scale[areas[iarea].curscale]*GRIDDIVISIONS);
	
	sprintf(string,"%s /Div",scalelabel[iscale]);
	cstring=XmStringCreateLocalized(string);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(areas[iarea].wscalebutton,args,nargs);
	XmStringFree(cstring);
	areas[iarea].graphinitialize=1;
	areas[iarea].tempclear=1;
        repaintgraph(areas[iarea].wgrapharea,(XtPointer)(intptr_t)areas[iarea].area,(XtPointer)0);
    }
    else {
	zoomarea->curscale=iscale;
	zoomarea->xmax=(zoomarea->centerval+
	  scale[zoomarea->curscale]*GRIDDIVISIONS);
	zoomarea->xmin=(zoomarea->centerval-
	  scale[zoomarea->curscale]*GRIDDIVISIONS);
	
	sprintf(string,"%s /Div",scalelabel[iscale]);
	cstring=XmStringCreateLocalized(string);
	nargs=0;
	XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	XtSetValues(zoomarea->wscalebutton,args,nargs);
	XmStringFree(cstring);
	zoomarea->graphinitialize=1;
	zoomarea->tempclear=1;
	repaintzoom((Widget)0,(XtPointer)0,(XtPointer)0);
    }
}
/**************************** viewcb **************************************/
void viewcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int i,ia;

    switch((intptr_t)clientdata) {
    case 0:
	status();
	break;
    case 1:
	{
	    Widget wdialog;
	    XmString prompt,text;
	    
	    prompt=XmStringCreateLocalized("Enter interval between updates in ms:");
	    sprintf(string,"%ld",timeinterval);
	    text=XmStringCreateLocalized(string);
	    nargs=0;
	    XtSetArg(args[nargs],XmNselectionLabelString,prompt); nargs++;
	    XtSetArg(args[nargs],XmNautoUnmanage,FALSE); nargs++;
	    XtSetArg(args[nargs],XmNtextString,text); nargs++;
	    XtSetArg(args[nargs],XmNtitle,"Update Interval"); nargs++;
	    wdialog=XmCreatePromptDialog(w,"prompt",args,nargs);
	    XmStringFree(prompt);
	    XmStringFree(text);
	    XtAddCallback(wdialog,XmNokCallback,viewcb,(XtPointer)11);
	    XtAddCallback(wdialog,XmNcancelCallback,destroywidget,(XtPointer)NULL);
	    XtSetSensitive(XmSelectionBoxGetChild(wdialog,XmDIALOG_HELP_BUTTON),FALSE);
	    XtManageChild(wdialog);
	}
	break;
    case 11:
	{
	    XmSelectionBoxCallbackStruct *cbs;
	    char *text;
	    int newval;
	    
	    cbs=(XmSelectionBoxCallbackStruct *)calldata;
	    XmStringGetLtoR(cbs->value,XmSTRING_DEFAULT_CHARSET,&text);
	    newval=atoi(text);
	    XtFree(text);
	    if(newval > 0) {
		timeinterval=newval;
		XtDestroyWidget(w);
		ecastoptimer();
		ecastarttimer();
	    }
	    else {
		xerrmsg("Invalid time value: %ld",newval);
	    }
	}
	break;
    case 2:
	markers=XmToggleButtonGetState(w)?1:0;
	for(i=0; i < nareas; i++) areas[i].tempclear=1;
	resetgraph();
	break;
    case 3:
	break;
    case 4:
	grid=XmToggleButtonGetState(w)?1:0;
	for(i=0; i < nareas; i++) areas[i].tempclear=1;
	resetgraph();
	break;
    case 5:
	showmaxmin=XmToggleButtonGetState(w)?1:0;
	for(i=0; i < nareas; i++) areas[i].tempclear=1;
	resetgraph();
	break;
    case 6:
	nstat=nstattime=0.;
	for(ia=0; ia < narrays; ia++) {
	    for(i=0; i < arrays[ia].nvals; i++) {
		arrays[ia].minvals[i]=LARGEVAL;
		arrays[ia].maxvals[i]=-LARGEVAL;
		arrays[ia].runsdev=0.;
		arrays[ia].runavg=0.;
		arrays[ia].runmax=0.;
	    }
	}
	for(i=0; i < nareas; i++) areas[i].tempclear=1;
	resetgraph();
	break;
    case 7:
	lines=XmToggleButtonGetState(w)?1:0;
	for(i=0; i < nareas; i++) areas[i].tempclear=1;
	resetgraph();
	break;
    case 8:
	bars=XmToggleButtonGetState(w)?1:0;
	for(i=0; i < nareas; i++) areas[i].tempclear=1;
	resetgraph();
	break;
    case 9:
	fillmaxmin=XmToggleButtonGetState(w)?1:0;
	for(i=0; i < nareas; i++) areas[i].tempclear=1;
	resetgraph();
	break;
    }
}
/**************************** zoomcontrolcb *******************************/
void zoomcontrolcb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    int len,n,changecenter=0;
    char *value;
    double scaleval;

    scaleval=scale[zoomarea->curscale];
    n=(intptr_t)clientdata;
    switch(n) {
    case 1:     /* Center value */
	value=XmTextFieldGetString(zoomarea->wcenter);
	zoomarea->centerval=atof(value);
	XFree(value);
	zoomarea->xmax=(zoomarea->centerval+scaleval*GRIDDIVISIONS);
	zoomarea->xmin=(zoomarea->centerval-scaleval*GRIDDIVISIONS);
	changecenter=1;
	break;
    case 2:     /* Center Up arrow */
	zoomarea->centerval+=scaleval;
	zoomarea->xmax+=scaleval;
	zoomarea->xmin+=scaleval;
	changecenter=1;
	break;
    case 3:     /* Center Down arrow */
	zoomarea->centerval-=scaleval;
	zoomarea->xmax-=scaleval;
	zoomarea->xmin-=scaleval;
	changecenter=1;
	break;
    case 4:     /* Interval */
	value=XmTextFieldGetString(zoomarea->winterval);
	sdel=atof(value);
	XFree(value);
	if(sdel <= 0) sdel=1.;
	else if(sdel > stotal) sdel=stotal;
      /* Reset sdel */
	sprintf(string,"% #11.4g",sdel);
	len=(int)strlen(string);
	nargs=0;
	XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
	XtSetArg(args[nargs],XmNvalue,string); nargs++;
	XtSetValues(zoomarea->winterval,args,nargs);
      /* Recompute and redraw */
	movezoommid();
	break;
    case 5:     /* Sector */
	value=XmTextFieldGetString(zoomarea->wsector);
	ngraphunit=atoi(value);
	XFree(value);
	if(ring) {
	    if(ngraphunit < 1) ngraphunit=1;
	    else if(ngraphunit > nsect) ngraphunit=nsect;
	}
	smid=(ngraphunit-.5);
	movezoommid();
	break;
    case 6:     /* Left Arrow */
	smid-=sdel;
	movezoommid();
	break;
    case 7:     /* Right Arrow */
	smid+=sdel;
	movezoommid();
	break;
    }
/* Change the center value for cases 1-3 */
    if(changecenter) {
      /* Fix it so very small values are written as zero */
	if(fabs(zoomarea->centerval) < 1.e-10*scaleval)
	  zoomarea->centerval=0.;
	sprintf(string,"% #11.4g",zoomarea->centerval);
	len=(int)strlen(string);
	nargs=0;
	XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
	XtSetArg(args[nargs],XmNvalue,string); nargs++;
	XtSetValues(zoomarea->wcenter,args,nargs);
	zoomarea->graphinitialize=1;
	repaintzoom(zoomarea->wgrapharea,(XtPointer)0,(XtPointer)0);
    }
}
