/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* event.c *** Event and work routines for ADT */

#include "adt.h"

/**************************** eventgraph **********************************/
void eventgraph(Widget w,  XtPointer clientdata, XEvent *event,
  Boolean *contdispatch)
{
    int nmid,nmid1,iarea,iarray;
    static int ia,ifound,nvals,buttondown;
    struct AREA *area=NULL;
    struct ARRAY *array=NULL;
    char *ptr;
    double val;
    
/* Find which area */
    iarea=(int)clientdata;
    area=&areas[iarea];
/* Set pixel conversion coefficients */    
    aptox=area->aptox;
    aptos=area->aptos;
    axtop=area->axtop;
    astop=area->astop;
    bptox=area->bptox;
    bptos=area->bptos;
    bxtop=area->bxtop;
    bstop=area->bstop;
    switch (event->type) {
    case ButtonPress:
	switch(event->xbutton.button) {
	case Button1:
	    if(!ecainitialized) break;
	    nmid=(int)(pixs(event->xbutton.x)+.5);
	    ptr=string;
	    for(iarray=0; iarray < narrays; iarray++) {
		array=&arrays[iarray];
		if(array->area != area) continue;
		sprintf(ptr,"%s\n",array->heading);
		ptr=string+strlen(string);
		for(nmid1=nmid-1; nmid1 <= nmid+1; nmid1++) {
		    if(nmid1 >= 0 && nmid1 < array->nvals) {
			val=array->scalefactor*array->vals[nmid1];
			if(arrays->logscale) val=val > 0.?log10(val):0.;
			if(nmid1 == nmid)
			  sprintf(ptr,"->%d %s  % 7.3f\n",nmid1+1,array->names[nmid1],
			    val);
			else
			  sprintf(ptr,"  %d %s  % 7.3f\n",nmid1+1,array->names[nmid1],
			    val);
			ptr=string+strlen(string);
		    }
		}
	    }
	    xinfomsg(string);	    
	    buttondown=1;
	    break;
	case Button2:
	    if(!ecainitialized || !zoomon || !zoom) break;
	  /* Find the first array in this area */
	    ifound=0;
	    for(ia=0; ia < narrays; ia++) {
		array=&arrays[ia];
		if(array->area == area) {
		    ifound=1;
		    break;
		}
	    }
	    if(ifound) {
		nvals=array->nvals;
		nmid=(int)(pixs(event->xbutton.x)+.5);
		if(nmid > (nvals-1)) nmid=nvals-1;
		else if(nmid < 0) nmid=0;
		smid=array->s[nmid];
		movezoommid();
	    }
	    break;
	case Button3:     /* Reset Max/Min */
	    viewcb((Widget)0,(XtPointer)6,(XtPointer)0);
	    break;
	}
	break;
    case ButtonRelease:
	switch(event->xbutton.button) {
	case Button1:
	    xpress=event->xbutton.x;
	    ypress=event->xbutton.y;
	    if(buttondown && xpress >= 0 && xpress < (int)area->width &&
	      ypress >= 0 && ypress < (int)area->height)
	      XtDestroyWidget(infobox);
	    buttondown=0;
	    break;
	}
    default:
	break;
    }
}
/**************************** eventzoom ***********************************/
void eventzoom(Widget w,  XtPointer clientdata, XEvent *event,
  Boolean *contdispatch)
{
/* Set pixel conversion coefficients */    
    aptox=zoomarea->aptox;
    aptos=zoomarea->aptos;
    axtop=zoomarea->axtop;
    astop=zoomarea->astop;
    bptox=zoomarea->bptox;
    bptos=zoomarea->bptos;
    bxtop=zoomarea->bxtop;
    bstop=zoomarea->bstop;
    switch (event->type) {
    case ButtonPress:
	switch(event->xbutton.button) {
	case Button1:
	    xpress=event->xbutton.x;
	    break;
	case Button2:
	    if(!ecainitialized) break;
	    smid=pixs(event->xbutton.x);
	    movezoommid();
	    break;
	case Button3:
	    if(!ecainitialized || !zoomon || !zoom) break;
	    xscroll=event->xbutton.x;
	    scrollzoommid();
	    break;
	}
    default:
	break;
    }
}
