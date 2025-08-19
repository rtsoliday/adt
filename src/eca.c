/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* eca.c *** EPICS Channel Access for ADT */

#define DEBUG_INPUT_ID 0
#define DEBUG_GC 0

#include <cadef.h>
#include <SDDS.h>
#include "adt.h"

#define CA_PEND_IO_TIME 30.0

#define NOTCONN 100
#define UNUSED 200

#ifdef WIN32
    int inputReadMask=XtInputReadWinsock;
#else
    int inputReadMask=XtInputReadMask;
#endif

/* Structures */

struct ECADATA {
    chid chid;
    chid statuschid;
    chid thresholdchid;
    int ok;
    int statusok;
    int thresholdok;
    int checkstatus;
    int checkthreshold;
    int array;
    int index;
};

struct FDLIST {
    struct FDLIST *prev;
    XtInputId inpid;
    int fd;
};

/* Function prototypes */
static void ecaabort(void);
static int ecaexit(void);
static void ecafreearrays(void);
static void ecagetvals(void);
static int ecainit(void);
static void ecaprocesscb(XtPointer clientdata, int *source, XtInputId *id);
static void ecareadbackcb(struct event_handler_args args);
static void ecaregisterfd(void *dummy, int fd, int opened);
static void ecastatusreadbackcb(struct event_handler_args args);
static void ecathresholdreadbackcb(struct event_handler_args args);
static void ecatimer(XtPointer clientdata, XtIntervalId *id);

/* Global variables */

struct ECADATA *ecadata;
XtIntervalId ecatimerprocid=(XtIntervalId)0;
XtInputId ecainputid=(XtInputId)0;
int nerrors=0;

struct FDLIST *inpstart=(struct FDLIST *)0;

/**************************** ecaabort ************************************/
static void ecaabort(void)
{
    xerrmsg("Aborting Channel Access");
    ecashutdown();
    ecafreearrays();
    ecainitialized=0;
    makegraphform();
}
/**************************** ecacb ***************************************/
void ecacb(Widget w, XtPointer clientdata, XtPointer calldata)
{
    switch((int)clientdata) {
    case 0:
	if(ecaresponding) {
	    ecaresponding=0;
	    ecastoptimer();
	    cstring=XmStringCreateLocalized("Start");
	    nargs=0;
	    XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	    XtSetValues(echoecabutton,args,nargs);
	    XmStringFree(cstring);
	} else {
	    if(!ecastartup()) {
		ecaresponding=0;
		ecastoptimer();
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
	}
	break;
    case 1:
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
	break;
    case 2:
	if(ecaconnected) {
	    ecagetvals();
	    resetgraph();
	} else {
	    xerrmsg("Channel Access is not connected");
	}
	break;
    case 3:
	if(ecaconnected) {
	    ecashutdown();
	    cstring=XmStringCreateLocalized("Start");
	    nargs=0;
	    XtSetArg(args[nargs],XmNlabelString,cstring); nargs++;
	    XtSetValues(echoecabutton,args,nargs);
	    XmStringFree(cstring);
	}
	break;
    }
}
/**************************** ecaexit *************************************/
static int ecaexit(void)
{
  int status,ecaok=1;
  //int fd;
    struct FDLIST *cur,*prev;
    
#if DEBUG_INPUT_ID
    print("ecaexit: Starting\n");
#endif
/* Terminate channel access */
    status=ca_task_exit();
    if(status != ECA_NORMAL) {
        xerrmsg("Error terminating Channel Access\n%s",
	  ca_message(status));
        ecaok=0;
    }
/* Remove any unremoved file descriptor callbacks */
    cur=inpstart;
    while(cur) {
	if(cur->inpid) {
#if DEBUG_INPUT_ID
	    print("ecaexit: Remove fd=%d inpid=%lx\n",cur->fd,cur->inpid);
#endif	
	    XtRemoveInput(cur->inpid);
#if DEBUG_INPUT_ID
	    print("         XtRemoveInput finished\n");
#endif
	}
	prev=cur->prev;
	//fd=cur->fd;
	free(cur);
	cur=prev;
    }
    inpstart=(struct FDLIST *)0;
#if DEBUG_INPUT_ID
    print("ecaexit: Returning\n");
#endif
/* Return */
    return(ecaok);
}
/**************************** ecafreearrays *******************************/
static void ecafreearrays(void)
{
    int i,isave;
    
    if(ecadata) {free(ecadata); ecadata=(struct ECADATA *)0;}
    for(i=0; i < narrays; i++) {
	if(arrays[i].vals) {free(arrays[i].vals);
	arrays[i].vals=(double *)0;}
	if(arrays[i].refvals) {free(arrays[i].refvals);
	arrays[i].refvals=(double *)0;}
	if(arrays[i].minvals) {free(arrays[i].minvals);
	arrays[i].minvals=(double *)0;}
	if(arrays[i].maxvals) {free(arrays[i].maxvals);
	arrays[i].maxvals=(double *)0;}
	if(arrays[i].s) {free(arrays[i].s);
	arrays[i].s=(double *)0;}
	if(arrays[i].statusvals) {free(arrays[i].statusvals);
	arrays[i].statusvals=(unsigned short *)0;}
	if(arrays[i].thresholdvals) {free(arrays[i].thresholdvals);
	arrays[i].thresholdvals=(unsigned short *)0;}
	if(arrays[i].conn) {free(arrays[i].conn);
	arrays[i].conn=(Boolean *)0;}
	if(arrays[i].statusconn) {free(arrays[i].statusconn);
	arrays[i].statusconn=(Boolean *)0;}
	if(arrays[i].thresholdconn) {free(arrays[i].thresholdconn);
	arrays[i].thresholdconn=(Boolean *)0;}
	if(arrays[i].names) {freenames(arrays[i].nvals,&arrays[i].names);
	arrays[i].names=(char **)0;}
	if(arrays[i].statusnames)
	    {freenames(arrays[i].nvals,&arrays[i].statusnames);
	arrays[i].statusnames=(char **)0;}
	if(arrays[i].thresholdnames)
	    {freenames(arrays[i].nvals,&arrays[i].thresholdnames);
	arrays[i].thresholdnames=(char **)0;}
	if(arrays[i].heading) {free(arrays[i].heading);
	arrays[i].heading=(char *)0;}
	if(arrays[i].units) {free(arrays[i].units);
	arrays[i].units=(char *)0;}
	if(arrays[i].legendpix) {XFreePixmap(display,arrays[i].legendpix);
	arrays[i].legendpix=(Pixmap)0;}
	for(isave=0; isave < NSAVE; isave++) {
	    if(arrays[i].savevals[isave]) {
		free(arrays[i].savevals[isave]);
		arrays[i].savevals[isave]=(double *)0;
	    }
	}
    }
    if(arrays) {free(arrays); arrays=(struct ARRAY *)0;}
    for(i=0; i < nareas; i++) {
#if DEBUG_GC
	print("ecafreearrays: areas[%d].gc=%x\n",i,areas[i].gc);
#endif
	if(areas[i].gc) {XFree(areas[i].gc); areas[i].gc=(GC)0;}
    }
    if(areas) {free(areas); areas=(struct AREA *)0;}
    if(zoomarea) {
	if(zoomarea->gc) {XFree(zoomarea->gc); zoomarea->gc=(GC)0;}
	{free(zoomarea); zoomarea=(struct ZOOMAREA *)0;}
    }
    if(xplot) {free(xplot); xplot=(double *)0;}
    if(xarray) {free(xarray); xarray=(int *)0;}
    if(sarray) {free(sarray); sarray=(int *)0;}
    if(ecasyms) {SDDS_Free(ecasyms); ecasyms=(double *)0;}
    if(ecasymlen) {SDDS_Free(ecasymlen); ecasymlen=(double *)0;}
    if(ecasymheight) {SDDS_Free(ecasymheight); ecasymheight=(short *)0;}
/* Clear names */
    *latfilename='\0';
    *reffilename='\0';
/* Restore defaults */
    setup();
    zoomform=(Widget)0;
/* Change title */
    maketitle();
/* Set widgets */    
    XmToggleButtonSetState(wmarkers,(Boolean)markers,FALSE);
    XmToggleButtonSetState(wlines,(Boolean)lines,FALSE);
    XmToggleButtonSetState(wbars,(Boolean)bars,FALSE);
    XmToggleButtonSetState(wgrid,(Boolean)grid,FALSE);
    XmToggleButtonSetState(wshowmaxmin,(Boolean)showmaxmin,FALSE);
    XmToggleButtonSetState(wfillmaxmin,(Boolean)fillmaxmin,FALSE);
}
/**************************** ecagetvals **********************************/
static void ecagetvals(void)
{
    int ia,ineca,index,status,nerrors;
    double *val;
    unsigned short *statusval;
    double thresholdval;
    
    
/* Ask for values */
    windowmessage=0;
    nerrors=0;
/* Values */
    for(ineca=0; ineca < neca; ineca++) {
	if(!ecadata[ineca].ok) continue;
	ia=ecadata[ineca].array;
	index=ecadata[ineca].index;
	val=&arrays[ia].vals[index];
	status=ca_get(DBR_DOUBLE,ecadata[ineca].chid,val);
	if(status != ECA_NORMAL) {
	    nerrors++;
	    xerrmsg("Error ASKING for value for %s",arrays[ia].names[index]);
	}
    }
/* Status Values */
    if(checkstatus) {
	for(ineca=0; ineca < neca; ineca++) {
	    if(!ecadata[ineca].checkstatus) continue;
	    if(!ecadata[ineca].statusok) continue;
	    ia=ecadata[ineca].array;
	    index=ecadata[ineca].index;
	    statusval=&arrays[ia].statusvals[index];
	    status=ca_get(DBR_ENUM,ecadata[ineca].statuschid,statusval);
	    if(status != ECA_NORMAL) {
		nerrors++;
		xerrmsg("Error ASKING for value for %s",
		  arrays[ia].names[index]);
	    }
	}
    }
/* Threshold Values */
    if(checkthreshold) {
	for(ineca=0; ineca < neca; ineca++) {
	    if(!ecadata[ineca].checkthreshold) continue;
	    if(!ecadata[ineca].thresholdok) continue;
	    ia=ecadata[ineca].array;
	    index=ecadata[ineca].index;
	    status=ca_get(DBR_DOUBLE,ecadata[ineca].thresholdchid,&thresholdval);
	    if(status != ECA_NORMAL) {
		nerrors++;
		xerrmsg("Error ASKING for value for %s",
		  arrays[ia].names[index]);
	    } else {
              if ((thresholdval >= arrays[ia].thresholdLL[index]) && (thresholdval <= arrays[ia].thresholdUL[index]))
                arrays[ia].thresholdvals[index]=1; //Valid
              else
                arrays[ia].thresholdvals[index]=0; //InValid
            }
	}
    }
    windowmessage=1;
    if(nerrors) {
	xerrmsg("Errors ASKING for current values for %d process variables",
	  nerrors);
    }
/* Wait until data is back */
    status=ca_pend_io(CA_PEND_IO_TIME);
    if(status != ECA_NORMAL) {
	xerrmsg("Errors getting current values\n%s",
	  ca_message(status));
    }
}
/**************************** ecainit *************************************/
static int ecainit(void)
{
    int isok=1;
    int status;
    
/* Initialize channel access */
    status=ca_task_initialize();
    if(status != ECA_NORMAL) {
	xerrmsg("Error initializing Channel Access\n%s",
	  ca_message(status));
	isok=0;
    }
/* Register the CA routine to handle fd's */
    status=ca_add_fd_registration(ecaregisterfd,NULL);
    if(status != ECA_NORMAL) {
	xerrmsg("Error registering file descriptor routine\n%s",
	  ca_message(status));
	isok=0;
    }
/* Poll CA */
    ca_poll();
/* Return */
    ecaconnected=1;
    return(isok);
}
/**************************** ecaprocesscb ********************************/
static void ecaprocesscb(XtPointer clientdata, int *source, XtInputId *id)
{
    ca_poll();
}
/**************************** ecareadbackcb *******************************/
static void ecareadbackcb(struct event_handler_args args)
{
    struct ECADATA *curecadata;
    double val;
    
/* Get new value */
    val=*(double *)args.dbr;
    curecadata=(struct ECADATA *)args.usr;
    arrays[curecadata->array].vals[curecadata->index]=val;
}
/**************************** ecaregisterfd *******************************/
static void ecaregisterfd(void *dummy, int fd, int opened)
{
  /* Note that using XtAppAddInput is probably unnecessary, as long as
     ca_poll is called in the timer proc */
    struct FDLIST *cur,*next;
    int found;
    
/* Register a call back for this fd */
    if(opened) {
      /* Look for a linked list structure for this fd */
	cur=inpstart;
	while(cur) {
	    if(cur->fd == fd) {
		xerrmsg("Tried to add a second callback "
		  "for file descriptor %d",fd);
		return;
	    }
	    cur=cur->prev;
	}
      /* Allocate and fill a linked list structure for this fd */
	cur=(struct FDLIST *)calloc(1,sizeof(struct FDLIST));
	if(cur == NULL) {
	    xerrmsg("Could not allocate space to keep track of "
	      "file descriptor %d",fd);
	    return;
	}
	cur->prev=inpstart;
	cur->inpid=XtAppAddInput(appcontext,fd,
	  (XtPointer)inputReadMask,
	  ecaprocesscb,
	  (XtPointer)NULL);
 	cur->fd=fd;
	inpstart=cur;
#if DEBUG_INPUT_ID
	print("ecaregisterfd: Add fd=%d inpid=%lx\n",cur->fd,cur->inpid);
#endif	
    } else {
      /* Remove the callback for this fd */
      /* Find the linked list structure for this fd */
	found=0;
	cur=next=inpstart;
	while(cur) {
	    if(cur->fd == fd) {
		found=1;
		break;
	    }
	    next=cur;
	    cur=cur->prev;
	}
      /* Remove the callback */
	if(found) {
#if DEBUG_INPUT_ID
	    print("ecaregisterfd: Remove fd=%d inpid=%lx\n",cur->fd,cur->inpid);
#endif
	    if(cur->inpid) XtRemoveInput(cur->inpid);
#if DEBUG_INPUT_ID
	    print("               XtRemoveInput finished\n");
	    XFlush(display);
	    print("               XFlush finished\n");
#endif
	    if(cur == inpstart) inpstart=cur->prev;
	    else next->prev=cur->prev;
	    free(cur);
	} else {
	    xerrmsg("Error removing callback for file descriptor %d",fd);
	}
    }
}
/**************************** ecashutdown *********************************/
void ecashutdown(void)
{
    ecastoptimer();
    if(ecaconnected) ecaexit();
    ecaconnected=ecaresponding=0;
}
/**************************** ecastarttimer *******************************/
void ecastarttimer(void)
{
    ecatimerprocid=XtAppAddTimeOut(appcontext,timeinterval,ecatimer,NULL);
}
/**************************** ecastartup **********************************/
int ecastartup(void)
{
    int i,ia,ineca,status,len,index,nerrors=0;
    
/* Check if initialized */
    if(ecaconnected) {
	ecaresponding=1;
	ecastarttimer();
	return(1);
    }
/* Free any old arrays */
    ecafreearrays();
/* Start channel access */
    if(!ecainit()) {
	xerrmsg("Could not start Channel Access");
	return(0);
    }
/* Set cursor */
    XDefineCursor(display,mainwindow,watch);
    XFlush(display);
/* Read process variable file */
    if(!readpvs(pvfilename)) {
	xerrmsg("Could not read process variable names");
	ecaabort();
	XUndefineCursor(display,mainwindow);
	return(0);
    }
/* Read reference file */
    if(reference) {
	if(!readreference(reffilename)) {
	    xerrmsg("Could not read reference data from %s\n"
	      "Referencing is inactive",reffilename);
	    reference=0;
	}
    }
/* Read lattice file for symbols */
    if(zoom) {
	if(!readlat(latfilename)) {
	    xerrmsg("Could not read lattice symbol data from %s\n"
	      "Zoom Window is inactive",latfilename);
	    zoom=0;
	}
    }
/* Allocate space */
    neca=0;
    for(i=0; i < narrays; i++) neca+=arrays[i].nvals;
    ecadata=(struct ECADATA *)calloc(neca,sizeof(struct ECADATA));
    if(ecadata == NULL) {
	xerrmsg("Could not allocate space for ECA structures");
	ecaabort();
	XUndefineCursor(display,mainwindow);
	return(0);
    }
/* Initialize */
    windowmessage=0;
    nerrors=0;
/* Values */
    ineca=0;
    for(ia=0; ia < narrays; ia++) {
	for(i=0; i < arrays[ia].nvals; i++,ineca++) {
	    status=ca_search(arrays[ia].names[i],&ecadata[ineca].chid);
	    ecadata[ineca].index=i;
	    ecadata[ineca].array=ia;
	    ecadata[ineca].ok=1;
	    ecadata[ineca].statuschid=(chid)0;
	    ecadata[ineca].thresholdchid=(chid)0;
	    ecadata[ineca].statusok=0;
	    ecadata[ineca].thresholdok=0;
	    ecadata[ineca].checkstatus=0;
	    ecadata[ineca].checkthreshold=0;
	    if(status != ECA_NORMAL) {
		nerrors++;
		arrays[ia].vals[i]=0.0;
		ecadata[ineca].ok=0;
		xerrmsg("Error ASKING for search for %s",arrays[ia].names[i]);
	    }
	  /* Winan's Hack */
	    if(ineca%50 == 0) ca_poll();
	}
    }
/* Status Values */
    if(checkstatus) {
	ineca=0;
	for(ia=0; ia < narrays; ia++) {
	    for(i=0; i < arrays[ia].nvals; i++,ineca++) {
		if(strcmp(arrays[ia].statusnames[i],"-")) {
		    ecadata[ineca].checkstatus=1;
		    status=ca_search(arrays[ia].statusnames[i],
		      &ecadata[ineca].statuschid);
		    ecadata[ineca].statusok=1;
		    if(status != ECA_NORMAL) {
			nerrors++;
			ecadata[ineca].checkstatus=0;
			arrays[ia].statusvals[i]=NOTCONN;
			ecadata[ineca].statusok=0;
			xerrmsg("Error ASKING for search for %s",
			  arrays[ia].statusnames[i]);
		    }
		} else {
		    ecadata[ineca].checkstatus=0;
		    ecadata[ineca].statusok=0;
		    arrays[ia].statusvals[i]=UNUSED;
		}
	      /* Winan's Hack */
		if(ineca%50 == 0) ca_poll();
	    }
	}
    }
/* Threshold Values */
    if(checkthreshold) {
	ineca=0;
	for(ia=0; ia < narrays; ia++) {
	    for(i=0; i < arrays[ia].nvals; i++,ineca++) {
		if(strcmp(arrays[ia].thresholdnames[i],"-")) {
		    ecadata[ineca].checkthreshold=1;
		    status=ca_search(arrays[ia].thresholdnames[i],
		      &ecadata[ineca].thresholdchid);
		    ecadata[ineca].thresholdok=1;
		    if(status != ECA_NORMAL) {
			nerrors++;
			ecadata[ineca].checkthreshold=0;
			arrays[ia].thresholdvals[i]=NOTCONN;
			ecadata[ineca].thresholdok=0;
			xerrmsg("Error ASKING for search for %s",
			  arrays[ia].thresholdnames[i]);
		    }
		} else {
		    ecadata[ineca].checkthreshold=0;
		    ecadata[ineca].thresholdok=0;
		    arrays[ia].thresholdvals[i]=UNUSED;;
		}
	      /* Winan's Hack */
		if(ineca%50 == 0) ca_poll();
	    }
	}
    }
    windowmessage=1;
    if(nerrors) {
	xerrmsg("Errors ASKING for search for %d process variables",
	  nerrors);
    }
/* Wait until data is back */
    allconnected=1;
    status=ca_pend_io(CA_PEND_IO_TIME);
    if(status != ECA_NORMAL) {
	xerrmsg("Errors searching for process variables\n%s",
	  ca_message(status));
	allconnected=0;
    }
/* Set connected flags */
    for(ineca=0; ineca < neca; ineca++) {
	ia=ecadata[ineca].array;
	index=ecadata[ineca].index;
	if(ca_state(ecadata[ineca].chid) == cs_conn) {
	    arrays[ia].conn[index]=1;
	} else {
	    ecadata[ineca].ok=0;
	    arrays[ia].conn[index]=0;
	}
    }
    if(checkstatus) {
	for(ineca=0; ineca < neca; ineca++) {
	    if(ecadata[ineca].checkstatus) {
		ia=ecadata[ineca].array;
		index=ecadata[ineca].index;
		if(ca_state(ecadata[ineca].statuschid) == cs_conn) {
		    arrays[ia].statusconn[index]=1;
		} else {
		    ecadata[ineca].statusok=0;
		    arrays[ia].statusconn[index]=0;
		}
		if(!arrays[ia].conn[index] || !arrays[ia].statusconn[index])
		  arrays[ia].statusvals[index]=NOTCONN;
	    }
	}
    }
    if(checkthreshold) {
	for(ineca=0; ineca < neca; ineca++) {
	    if(ecadata[ineca].checkthreshold) {
		ia=ecadata[ineca].array;
		index=ecadata[ineca].index;
		if(ca_state(ecadata[ineca].thresholdchid) == cs_conn) {
		    arrays[ia].thresholdconn[index]=1;
		} else {
		    ecadata[ineca].thresholdok=0;
		    arrays[ia].thresholdconn[index]=0;
		}
		if(!arrays[ia].conn[index] || !arrays[ia].thresholdconn[index])
		  arrays[ia].thresholdvals[index]=NOTCONN;
	    }
	}
    }
/* Print connection information */
    for(ineca=0; ineca < neca; ineca++) {
	ia=ecadata[ineca].array;
	index=ecadata[ineca].index;
	if(!arrays[ia].conn[index]) {
	    printf("Not connected: DisplayArea=%d Index=%d %s\n",
	      ia+1,index,arrays[ia].names[index]);
	}
	if(ecadata[ineca].checkstatus && !arrays[ia].statusconn[index]) {
	    printf("Not connected: DisplayArea=%d Index=%d %s\n",
	      ia+1,index,arrays[ia].statusnames[index]);
	}
	if(ecadata[ineca].checkthreshold && !arrays[ia].thresholdconn[index]) {
	    printf("Not connected: DisplayArea=%d Index=%d %s\n",
	      ia+1,index,arrays[ia].thresholdnames[index]);
	}
    }
/* Add callbacks (Will also get initial values on first callback) */
    windowmessage=0;
    nerrors=0;
    for(ineca=0; ineca < neca; ineca++) {
	if(!ecadata[ineca].ok) continue;
	ia=ecadata[ineca].array;
	index=ecadata[ineca].index;
	status=ca_add_event(DBR_DOUBLE,ecadata[ineca].chid,
	  ecareadbackcb,&ecadata[ineca],(evid *)0);
	if(status != ECA_NORMAL) {
	    nerrors++;
	    xerrmsg("Error ASKING for callback for %s",
	      arrays[ia].names[index]);
	}
    }
    if(checkstatus) {
	for(ineca=0; ineca < neca; ineca++) {
	    if(!ecadata[ineca].checkstatus) continue;
	    if(!ecadata[ineca].statusok) continue;
	    ia=ecadata[ineca].array;
	    index=ecadata[ineca].index;
	    status=ca_add_event(DBR_ENUM,ecadata[ineca].statuschid,
	      ecastatusreadbackcb,&ecadata[ineca],(evid *)0);
	    if(status != ECA_NORMAL) {
		nerrors++;
		xerrmsg("Error ASKING for callback for %s",
		  arrays[ia].statusnames[index]);
	    }
	}
    }
    if(checkthreshold) {
	for(ineca=0; ineca < neca; ineca++) {
	    if(!ecadata[ineca].checkthreshold) continue;
	    if(!ecadata[ineca].thresholdok) continue;
	    ia=ecadata[ineca].array;
	    index=ecadata[ineca].index;
	    status=ca_add_event(DBR_DOUBLE,ecadata[ineca].thresholdchid,
	      ecathresholdreadbackcb,&ecadata[ineca],(evid *)0);
	    if(status != ECA_NORMAL) {
		nerrors++;
		xerrmsg("Error ASKING for callback for %s",
		  arrays[ia].thresholdnames[index]);
	    }
	}
    }
    windowmessage=1;
    if(nerrors) {
	xerrmsg("Errors ASKING for callbacks for %d process variables",
	  nerrors);
    }
/* Flush and poll CA */
    status=ca_poll();
/* Initialize the max and min history vals */
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
/* Make the display areas */
    makegraphform();
/* Reset interval */
    if(zoom) {
	sprintf(string,"% #11.4g",sdel);
	len=(int)strlen(string);
	nargs=0;
	XtSetArg(args[nargs],XmNcursorPosition,len); nargs++;
	XtSetArg(args[nargs],XmNvalue,string); nargs++;
	XtSetValues(zoomarea->winterval,args,nargs);
	movezoommid();
    }
/* Start the timer proc */
    ecainitialized=ecaresponding=1;
    ecastarttimer();
    XUndefineCursor(display,mainwindow);
    return(1);
}
/**************************** ecastatusreadbackcb *************************/
static void ecastatusreadbackcb(struct event_handler_args args)
{
    struct ECADATA *curecadata;
    unsigned short val;
    

/* Get new value */
    val=*(unsigned short *)args.dbr;
    curecadata=(struct ECADATA *)args.usr;
    arrays[curecadata->array].statusvals[curecadata->index]=val;
}
/**************************** ecathresholdreadbackcb *************************/
static void ecathresholdreadbackcb(struct event_handler_args args)
{
    struct ECADATA *curecadata;
    double val;
    

/* Get new value */
    val=*(double *)args.dbr;
    curecadata=(struct ECADATA *)args.usr;
    if ((val >= arrays[curecadata->array].thresholdLL[curecadata->index]) && (val <= arrays[curecadata->array].thresholdUL[curecadata->index]))
      arrays[curecadata->array].thresholdvals[curecadata->index]=1; //Valid
    else
      arrays[curecadata->array].thresholdvals[curecadata->index]=0; //InValid
}
/**************************** ecastoptimer ********************************/
void ecastoptimer(void)
{
    if(ecatimerprocid) XtRemoveTimeOut(ecatimerprocid);
    ecatimerprocid=(XtIntervalId)0;
}
/**************************** ecatimer ************************************/
static void ecatimer(XtPointer clientdata, XtIntervalId *id)
{
    int i,ia;
    
/* Poll CA */
    ca_poll();
/* Calculate and display statistics */
    statistics();
/* Determine max and min for history */
    for(ia=0; ia < narrays; ia++) {
	for(i=0; i < arrays[ia].nvals; i++) {
	    if(arrays[ia].vals[i] < arrays[ia].minvals[i])
	      arrays[ia].minvals[i]=arrays[ia].vals[i];
	    if(arrays[ia].vals[i] > arrays[ia].maxvals[i])
	      arrays[ia].maxvals[i]=arrays[ia].vals[i];
	}
    }
/* Redraw graph areas */
    resetgraph();
/* Restart timer for the next time */
    ecastarttimer();
}
