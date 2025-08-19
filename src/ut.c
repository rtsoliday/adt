/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* ut.c *** Utilities for ADT */

#ifdef WIN32
#include <windows.h>
#endif

#include <SDDS.h>
#include "adt.h"

/**************************** allocnames **********************************/
int allocnames(int n, int m, char ***p)
{
    int i;
    
    *p=(char **)calloc(n,sizeof(char *));
    if((*p) == (char **)NULL) return(0);
    for(i=0; i < n; i++) {
	*((*p)+i)=(char *)calloc(m,sizeof(char));
	if((*((*p)+i)) == (char *)NULL) return(0);
    }
    return(1);
}
#if 0
/**************************** createlabelbox ******************************/
Widget createlabelbox(Widget parent, char *widgetname, char *label,
  char *value,XtCallbackProc callback, XtPointer clientdata, Widget *w2)
{
    Widget w0,w1;
    XmString clabel=XmStringCreateLocalized(label);
    int len;
    char name[64];
    
/* Create container */
    nargs=0;
    XtSetArg(args[nargs],XmNorientation,XmHORIZONTAL); nargs++;
    w0=XmCreateRowColumn(parent,widgetname,args,nargs);
    XtManageChild(w0);
/* Create label */    
    sprintf(name,"%sLabel\0",widgetname);
    nargs=0;
    XtSetArg(args[nargs],XmNlabelString,clabel); nargs++;
    w1=XmCreateLabel(w0,name,args,nargs);
    XmStringFree(clabel);
    XtManageChild(w1);
/* Create text box */
    sprintf(name,"%sTextField\0",widgetname);
    len=strlen(value);
    nargs=0;
    XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
    XtSetArg(args[nargs],XmNvalue,value); nargs++;
/*    XtSetArg(args[nargs],XmNcolumns,FLOATNUM); nargs++;*/
    *w2=XmCreateTextField(w0,name,args,nargs);
    XtManageChild(*w2);
    XtAddCallback(*w2,XmNactivateCallback,callback,clientdata);
/* Return */
    return(w0);
}
/**************************** createlabelwidget ***************************/
Widget createlabelwidget(Widget parent, char *widgetname, char *label)
{
    Widget w0,w1,w2;
    XmString clabel=XmStringCreateLocalized(label);
    char name[64];
    
/* Create container */
    nargs=0;
    XtSetArg(args[nargs],XmNorientation,XmHORIZONTAL); nargs++;
    w0=XmCreateRowColumn(parent,widgetname,args,nargs);
    XtManageChild(w0);
/* Create label */    
    sprintf(name,"%sLabel\0",widgetname);
    nargs=0;
    XtSetArg(args[nargs],XmNlabelString,clabel); nargs++;
    w1=XmCreateLabel(w0,name,args,nargs);
    XmStringFree(clabel);
    XtManageChild(w1);
/* Return */
    return(w0);
}
#endif
/**************************** destroywidget *******************************/
void destroywidget(Widget w, XtPointer clientdata, XtPointer calldata)
{
    XtDestroyWidget(w);
}
/**************************** freenames ***********************************/
void freenames(int n, char ***p)
/* free() returns int in SunOs but is void in ANSII */
{
    int i;
    
    errno=0;
    for(i=0; i < n; i++) {
	free(*((*p)+i));
    }
    free(*p);
    *p=(char **)NULL;
}
/**************************** freenames ***********************************/
void freesddsnames(int n, char ***p)
/* free() returns int in SunOs but is void in ANSII */
{
    int i;
    
    errno=0;
    for(i=0; i < n; i++) {
	SDDS_Free(*((*p)+i));
    }
    SDDS_Free(*p);
    *p=(char **)NULL;
}
/**************************** getuname ************************************/
int getuname(void)
{
#ifdef WIN32
  /* Get system information */
    BOOL status1,status2;
    TCHAR buffer[MAX_COMPUTERNAME_LENGTH+1];
    DWORD nSize=MAX_COMPUTERNAME_LENGTH+1;
    OSVERSIONINFO versioninfo;
    LPSTR platform;

    versioninfo.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
    status1=GetVersionEx(&versioninfo);
    if(status1) {
	switch(versioninfo.dwPlatformId) {
	case VER_PLATFORM_WIN32s:
	    platform="Windows 3.1";
	    break;
	case VER_PLATFORM_WIN32_WINDOWS:
	    if(versioninfo.dwMajorVersion > 4 ||
	      (versioninfo.dwMajorVersion == 4 && versioninfo.dwMinorVersion)) {
		platform="Windows 98";
	    } else {
		platform="Windows 95";
	    }
	    break;
	case VER_PLATFORM_WIN32_NT:
	    platform="Windows NT";
	    break;
	}
    } else {
	platform="Unknown OS";
    }
    status2=GetComputerName(buffer,&nSize);

    unamestring=calloc(1024,sizeof(char));     /* Danger, fixed size */
    if(!unamestring) {
	xerrmsg("Could not allocate space for system information");
	unamestring=(char *)0;
	return 0;
    }

    sprintf(unamestring,"Running %s %d.%02d on %s",
      platform,
      status1?versioninfo.dwMajorVersion:0,
      status1?versioninfo.dwMinorVersion:0,
      status2?buffer:"Unknown");

    return 1;    
#else
  /* Get system info from uname */
    struct utsname name;
    int len=0;
    
    if(uname(&name) == -1) {
	xerrmsg("Could not find system information");
	unamestring=(char *)0;
	return 0;
    }
    len+=strlen(name.sysname);
    len+=strlen(name.release);
    len+=strlen(name.nodename);
/*     len+=strlen(name.version); */
/*     len+=strlen(name.machine); */
    len+=14;
    unamestring=calloc(len,sizeof(char));
    if(!unamestring) {
	xerrmsg("Could not allocate space for system information");
	unamestring=(char *)0;
	return 0;
    }
    sprintf(unamestring,"Running %s %s on %s",
      name.sysname,name.release,name.nodename);

    return 1;
#endif    
}
/**************************** trapalarm ***********************************/
void trapalarm(int signum)
{
}
/**************************** xerrorhandler *******************************/
int xerrorhandler(Display *dpy, XErrorEvent *event)
  /* Partially obtained from the X sources */
{
    char buffer[BUFSIZ];
    
    *buffer='\0';
    XGetErrorText(dpy,event->error_code,buffer,BUFSIZ);
    sprintf(string,
      "  X Error: %s\n"
      "  Request Major Code: %u\n"
      "  Request Minor Code: %u\n"
      "  Resource ID:        %ld\n"
      "  Serial # of Failed: %lu [0x%lx]\n"
      "  Next Serial #:      %lu [0x%lx]\n",
      buffer,(unsigned)event->request_code,(unsigned)event->minor_code,
      event->resourceid,event->serial,event->serial,
      NextRequest(dpy),NextRequest(dpy));

#ifdef WIN32
    lprintf("%s\n",string);
#else
    fprintf(stderr,"%s\n",string);
#endif

  /* Call xerrmsg */
    if(windowmessage) xerrmsg("X Error:\n%s",buffer);
    
  /* Return value is ignored */
    return 0;
}
/**************************** xerrmsg *************************************/
void xerrmsg(char *fmt, ...)
{
    Widget child;
    va_list vargs;
    static char lstring[BUFSIZ];
    
    va_start(vargs,fmt);
    (void)vsprintf(lstring,fmt,vargs);
    va_end(vargs);
    
    if(lstring[0] != '\0') {
	if(windowmessage) {
	    XBell(display,50); XBell(display,50); XBell(display,50); 
	    cstring=XmStringCreateLtoR(lstring,XmSTRING_DEFAULT_CHARSET);
	    nargs=0;
	    XtSetArg(args[nargs],XmNtitle,"Warning"); nargs++;
	    XtSetArg(args[nargs],XmNmessageString,cstring); nargs++;
#ifndef WIN32
	    warningbox=XmCreateWarningDialog(mainwin,"warningMessage",
#else
	    warningbox=XmCreateMessageDialog(mainwin,"warningMessage",
#endif
	      args,nargs);
	    XmStringFree(cstring);
	    child=XmMessageBoxGetChild(warningbox,XmDIALOG_CANCEL_BUTTON);
	    XtDestroyWidget(child);
	    child=XmMessageBoxGetChild(warningbox,XmDIALOG_HELP_BUTTON);
	    XtDestroyWidget(child);
	    XtManageChild(warningbox);
	    XtAddCallback(warningbox,XmNokCallback,destroywidget,NULL);
	}
	else {
#ifdef WIN32
	    lprintf("%s\n",lstring);
#else
	    fprintf(stderr,"%s\n",lstring);
#endif
	}
    }
}
/**************************** xinfomsg ************************************/
void xinfomsg(char *fmt, ...)
{
    Widget child;
    va_list vargs;
    static char lstring[BUFSIZ];
    
    va_start(vargs,fmt);
    (void)vsprintf(lstring,fmt,vargs);
    va_end(vargs);
    
    if(lstring[0] != '\0') {
	cstring=XmStringCreateLtoR(lstring,XmSTRING_DEFAULT_CHARSET);
	nargs=0;
	XtSetArg(args[nargs],XmNtitle,"Information"); nargs++;
	XtSetArg(args[nargs],XmNmessageString,cstring); nargs++;
#ifndef WIN32
	/* 6/2012  Xming hangs */
	infobox=XmCreateInformationDialog(mainwin,"informationMessage",
#else
	infobox=XmCreateMessageDialog(mainwin,"informationMessage",
#endif
	  args,nargs);
	XmStringFree(cstring);
	child=XmMessageBoxGetChild(infobox,XmDIALOG_CANCEL_BUTTON);
	XtDestroyWidget(child);
	child=XmMessageBoxGetChild(infobox,XmDIALOG_HELP_BUTTON);
	XtDestroyWidget(child);
	XtManageChild(infobox);
	XtAddCallback(infobox,XmNokCallback,destroywidget,NULL);
    }
}
/**************************** xterrorhandler ******************************/
void xterrorhandler(char *message)
{
#ifdef WIN32
    lprintf("%s\n",message);
#else
    fprintf(stderr,"%s\n",message);
#endif
    if(windowmessage) xerrmsg(message);
}
/**************************** print ***************************************/
/* General purpose output routine
 * Works with both UNIX and WIN32
 * Uses sprintf to avoid problem with lprintf not handling %f, etc.
 *   (Exceed 5 only)
 * Use for debugging */

void print(const char *fmt, ...)
{
    va_list vargs;
    static char lstring[1024];  /* DANGER: Fixed buffer size */
    
    va_start(vargs,fmt);
    vsprintf(lstring,fmt,vargs);
    va_end(vargs);
    
    if(lstring[0] != '\0') {
#ifdef WIN32
	lprintf("%s",lstring);
#else
	printf("%s",lstring);
#endif
    }
}
