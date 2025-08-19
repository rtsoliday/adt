/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* repaint.c *** Repaint, resize, and reset routines for ADT */

#include "adt.h"

/**************************** draw ****************************************/
void draw(int iarray, Pixel pixel)
/* Draws the curves, lines, and markers for current xplot array */
/* Pixel coefficients must be set up first */
{
    GC gc;
    Pixmap pix;
    struct ARRAY *array=&arrays[iarray];
    struct AREA *area=array->area;
    double scalefactor,xtemp;
    int i,iar,na,snew,xnew,x00,nvals;
    Boolean *connarray,logscale;
    unsigned short *statusarray;
    unsigned short *thresholdarray;
    
/* Set up for this array */
    nvals=array->nvals;
    connarray=array->conn;
    statusarray=array->statusvals;
    thresholdarray=array->thresholdvals;
    scalefactor=array->scalefactor;
    logscale=array->logscale;
/* Set up for this area */
    x00=xpix(area->centerval);
    gc=areas->gc;
    pix=area->pixmap;
/* Define array  */
    iar=0;
    for(i=0; i < nvals; i++) {
	sarray[iar]=snew=spix((double)i);
	xtemp=xplot[i]*scalefactor;
	if(logscale) xtemp=log10(xtemp);
	xarray[iar]=xnew=xpix(xtemp);
	if(!allconnected && !connarray[i]) {
	    XSetForeground(display,gc,notconnpixel);
	    XCopyPlane(display,pv1pix,
	      pix,gc,0,0,5,5,snew-2,xnew-2,1);
	}
	else if ((checkstatus && checkstatusmode && statusarray[i] == 0) || (checkthreshold && checkthresholdmode && thresholdarray[i] == 0)) {
	    XSetForeground(display,gc,invalidpixel);
	    XCopyPlane(display,pv2pix,
	      pix,gc,0,0,5,5,snew-2,xnew-2,1);
	}
	else if ((checkstatus && checkstatusmode > 1 && statusarray[i] == 2) || (checkthreshold && checkthresholdmode > 1 && thresholdarray[i] == 2)) {
	    XSetForeground(display,gc,olddatapixel);
	    XCopyPlane(display,pv2pix,
	      pix,gc,0,0,5,5,snew-2,xnew-2,1);
	}
	else if(markers) {
	    XSetForeground(display,gc,pixel);
	    XCopyPlane(display,pvpix,
	      pix,gc,0,0,5,5,snew-1,xnew-1,1);
	}
	iar++;
    }
/* Plot array */
    na=iar;
    XSetForeground(display,gc,pixel);
    if(bars) {
	for(iar=0; iar < na; iar++) {
	    XDrawLine(display,pix,gc,
              sarray[iar],x00,sarray[iar],xarray[iar]);
	}
    }
    else if(lines) {
	for(iar=1; iar < na; iar++) {
	    XDrawLine(display,pix,gc,
	      sarray[iar-1],xarray[iar-1],sarray[iar],xarray[iar]);
	}
    }
}
/**************************** drawmaxmin **********************************/
void drawmaxmin(int iarray)
/* Draws the (filled) maximum and minimum history for one arrray */
/* Pixel coefficients must be set up first */
{
    XPoint points[4];
    GC gc;
    Pixmap pix;
    struct ARRAY *array=&arrays[iarray];
    struct AREA *area=array->area;
    double scalefactor,*wsavevals=NULL,*maxvals=NULL,*minvals=NULL,*refvals=NULL;
    double xtemp;
    Boolean logscale;
    int i,nvals;
    int snew,xnewmax,xnewmin;
    int sold,xoldmax,xoldmin;
    int doreference=reference && refon;
    
/* Set up for this array */
    nvals=array->nvals;
    maxvals=array->maxvals;
    minvals=array->minvals;
    scalefactor=array->scalefactor;
    logscale=array->logscale;
/* Set up for this area */
   if(whichorbit >= 0) wsavevals=array->savevals[whichorbit];
   else if(reference && refon) refvals=array->refvals;
    gc=areas->gc;
    XSetForeground(display,gc,greypixel);
    pix=area->pixmap;
    if (whichorbit >= 0) {
	xtemp=(minvals[0]-wsavevals[0])*scalefactor;
	if(logscale) xtemp=log10(xtemp);
	xoldmin=xpix(xtemp);
	xtemp=(maxvals[0]-wsavevals[0])*scalefactor;
	if(logscale) xtemp=log10(xtemp);
	xoldmax=xpix(xtemp);
    }
    else if (doreference) {
	xtemp=(minvals[0]-refvals[0])*scalefactor;
	if(logscale) xtemp=log10(xtemp);
	xoldmin=xpix(xtemp);
	xtemp=(maxvals[0]-refvals[0])*scalefactor;
	if(logscale) xtemp=log10(xtemp);
	xoldmax=xpix(xtemp);
    }
    else {
	xtemp=minvals[0]*scalefactor;
	if(logscale) xtemp=log10(xtemp);
	xoldmin=xpix(xtemp);
	xtemp=maxvals[0]*scalefactor;
	if(logscale) xtemp=log10(xtemp);
	xoldmax=xpix(xtemp);
    }
    sold=spix(0.);
    for(i=0; i < nvals; i++) {
	if (whichorbit >= 0) {
	    xtemp=(minvals[i]-wsavevals[i])*scalefactor;
	    if(logscale) xtemp=log10(xtemp);
 	    xnewmin=xpix(xtemp);
	    xtemp=(maxvals[i]-wsavevals[i])*scalefactor;
	    if(logscale) xtemp=log10(xtemp);
	    xnewmax=xpix(xtemp);
	}
	else if (doreference) {
	    xtemp=(maxvals[i]-refvals[i])*scalefactor;
	    if(logscale) xtemp=log10(xtemp);
	    xnewmax=xpix(xtemp);
	    xtemp=(minvals[i]-refvals[i])*scalefactor;
	    if(logscale) xtemp=log10(xtemp);
 	    xnewmin=xpix(xtemp);
	}
	else {
	    xtemp=maxvals[i]*scalefactor;
	    if(logscale) xtemp=log10(xtemp);
	    xnewmax=xpix(xtemp);
	    xtemp=minvals[i]*scalefactor;
	    if(logscale) xtemp=log10(xtemp);
	    xnewmin=xpix(xtemp);
	}
	snew=spix((double)i);
	points[0].x=sold;
	points[0].y=xoldmin;
	points[1].x=sold;
	points[1].y=xoldmax;
	points[2].x=snew;
	points[2].y=xnewmax;
	points[3].x=snew;
	points[3].y=xnewmin;
	XFillPolygon(display,pix,gc,points,4,Convex,CoordModeOrigin);
	xoldmax=xnewmax;
	xoldmin=xnewmin;
	sold=snew;
    }
}
/**************************** drawzoom ************************************/
void drawzoom(int iarray, Pixel pixel)
/* Draws the curves, lines, and markers for current xplot array */
/* Pixel coefficients must be set up first */
{
    GC gc=zoomarea->gc;
    Pixmap pix=zoomarea->pixmap;
    struct ARRAY *array=&arrays[iarray];
    int nvals=array->nvals;
    int zoomindxmin=array->zoomindxmin;
    int zoomindxmax=array->zoomindxmax;
    double scalefactor=array->scalefactor;
    unsigned short *statusarray=array->statusvals;
    unsigned short *thresholdarray=array->thresholdvals;
    Boolean *connarray=array->conn;
    double *svals=array->s;
    Boolean logscale=array->logscale;
    double soff;
    double xtemp;
    //int x00
    int iar,na,snew,xnew,iis,is;
    
/* Set up for this area */
    //x00=xpix(zoomarea->centerval);
/* Define array within visible limits */
    iar=0;
      /* DEBUG */
/*     printf("iarray=%d zoomindxmin=%d zoomindxmax=%d nvals=%d stotal=%f\n", */
/*       iarray,zoomindxmin,zoomindxmax,nvals,stotal); */
    for(iis=zoomindxmin; iis <= zoomindxmax; iis++) {
	is=iis;
	soff=0.;
	while(is > nvals-1) {
	    is-=nvals;
	    soff+=stotal;
	}
	while(is < 0) {
	    is+=nvals;
	    soff-=stotal;
	}
	sarray[iar]=snew=spix(svals[is]+soff);
	if(lines) {
	    xtemp=xplot[is]*scalefactor;
	    if(logscale) xtemp=log10(xtemp);
	    xarray[iar]=xnew=xpix(xtemp);
	}
      /* DEBUG */
/* 	printf("%5d %5d %5d: %5d %5d: %10.5f %10.5f %10.5f\n", */
/* 	  iis,is,iar,xarray[is],sarray[is],soff,xplot[is],svals[is]); */
	iar++;
	if(!allconnected && !connarray[is]) {
	    XSetForeground(display,gc,notconnpixel);
	    xnew=xpix(xplot[is]*scalefactor);
	    XCopyPlane(display,pv1pix,pix,gc,0,0,5,5,snew-2,xnew-2,1);
	}
	else if ((checkstatus && checkstatusmode && statusarray[is] == 0) || (checkthreshold && checkthresholdmode && thresholdarray[is] == 0)) {
	    XSetForeground(display,gc,invalidpixel);
	    xnew=xpix(xplot[is]*scalefactor);
	    XCopyPlane(display,pv2pix,
	      pix,gc,0,0,5,5,snew-2,xnew-2,1);
	}
	else if ((checkstatus && checkstatusmode > 1 && statusarray[is] == 2) || (checkthreshold && checkthresholdmode > 1 && thresholdarray[is] == 2)) {
	    XSetForeground(display,gc,olddatapixel);
	    xnew=xpix(xplot[is]*scalefactor);
	    XCopyPlane(display,pv2pix,
	      pix,gc,0,0,5,5,snew-2,xnew-2,1);
	}
	else if(markers) {
	    XSetForeground(display,gc,pixel);
	    xnew=xpix(xplot[is]*scalefactor);
	    XCopyPlane(display,pvpix,
	      pix,gc,0,0,5,5,snew-1,xnew-1,1);
	}
    }
/* Plot array */
    na=iar;
    XSetForeground(display,gc,pixel);
    for(iar=1; iar < na; iar++) {
	if(lines) XDrawLine(display,pix,gc,
	  sarray[iar-1],xarray[iar-1],sarray[iar],xarray[iar]);
    }
}
/**************************** indxabove ***********************************/
int indxabove(double s, double *array, int nmax)
/* Returns an index in the range [-1,nmax] */
{
    int i,ii,istart;
    double stest;
    int first,last;
    
/* Set limits */
    first=0;
    last=nmax-1;
/* Adjust s */
    if(s < 0) s+=stotal;
    if(s > stotal) s-=stotal;
/* Pick a guess */
    istart=(int)(nmax*(s/stotal)-1);
    if(istart < first) istart=first;
    else if(istart > last) istart=last;
/* Guess is too low */
    if(s > array[istart]) {
	for(i=istart+1; i <= last; i++) {
	    stest=array[i];
	    if(s <= stest) return(i);
	}
	return(-1);     /* Highest possible, but not good enough */
    }
/* Guess is too high */
    else if(s < array[istart]) {
	ii=istart;
	for(i=istart-1; i >= 0; i--) {
	    stest=array[i];
	    if(s > stest) return(ii);
	    ii=i;
	}
	return(ii);
    }
/* Guess is exact */
    else return(istart);
}
/**************************** indxbelow ***********************************/
int indxbelow(double s, double *array, int nmax)
/* Returns an index in the range [-1,nmax] */
{
    int i,ii,istart;
    double stest;
    int first,last;
    
/* Set limits */
    first=0;
    last=nmax-1;
/* Adjust s */
    if(s < 0) s+=stotal;
    if(s > stotal) s-=stotal;
/* Pick a guess */
    istart=(int)(nmax*(s/stotal)+1);
    if(istart < first) istart=first;
    else if(istart > last) istart=last;
/* Guess is too high */
    if(s < array[istart]) {
	for(i=istart-1; i >=0; i--) {
	    stest=array[i];
	    if(s >= stest) return(i);
	}
	return(-1);     /* Lowest possible, but not good enough */
    }
/* Guess is too low */
    else if(s > array[istart]) {
	ii=istart;
	for(i=istart+1; i <= last; i++) {
	    stest=array[i];
	    if(s < stest) return(ii);
	    ii=i;
	}
	return(ii);
    }
  /* Guess is exact */
    else return(istart);
}
/**************************** indxlimits **********************************/
void indxlimits(double smin, double smax, double *array, int nmax,
  int *imin, int *imax)
{
    int imin0,imax0;
    double soff;
    
    if(!nmax) {
	*imin=*imax=0;
	return;
    }
/* Minimum */
    imin0=*imin=indxbelow(smin,array,nmax);
    if(ring) {
	soff=0.;
	if(imin0 < 0) {
	    imin0=nmax-1;
	    *imin=-1;
	    soff+=stotal;
	}
	while(array[imin0] > smin+soff) {
	    *imin-=nmax;
	    soff+=stotal;
	}
    }
    else *imin=0;
/* Maximum */    
    imax0=*imax=indxabove(smax,array,nmax);
    if(ring) {
	soff=0.;
	if(imax0 < 0) {
	    imax0=1;
	    *imax=nmax+1;
	    soff-=stotal;
	}
	while(array[imax0] < smax+soff) {
	    *imax+=nmax;
	    soff-=stotal;
	}
    }
    else *imax=nmax-1;
}
/**************************** movezoommid *********************************/
void movezoommid(void)
{
/* Return if zoom is off */
    if(!zoomon || !zoom) return;
/* Set and adjust smid */
    if (zoomsector && !zoomsectorused) {
      smid = zoomsector - 0.5;
      zoomsectorused=1;
    }
    if (zoomintervalInput && !zoomintervalInputused) {
      sdel = (double)zoomintervalInput;
      zoomintervalInputused=1;
    }
    if(ring) {
	if(smid < sdel/2.) smid +=stotal;
	else if(smid >= stotal+sdel/2.) smid-=stotal;
	ngraphunit=(int)(smid+1.);
	if(ngraphunit < 1) ngraphunit+=nsect;
	else if(ngraphunit > nsect) ngraphunit-=nsect;
    }
    else {
	ngraphunit=(int)(smid+1.);
     }
    zoomarea->smin=smid-sdel/2.;
    zoomarea->smax=smid+sdel/2.;
    zoomarea->graphinitialize=1;
    sprintf(string,"% #11.4g",sdel);
    XmTextFieldSetString(zoomarea->winterval,string);
    sprintf(string,"%d",ngraphunit);
    XmTextFieldSetString(zoomarea->wsector,string);
    zoomarea->graphinitialize=1;
    repaintzoom((Widget)0,(XtPointer)0,(XtPointer)0);
}
/**************************** repaintgraph ********************************/
void repaintgraph(Widget w, XtPointer clientdata, XtPointer calldata)
{
    GC gc;
    Pixmap pix;
    Window win;
    struct AREA *area;
    struct ARRAY *array;
    double *dsavevals=NULL,*wsavevals=NULL,*vals=NULL,*refvals=NULL,
      *maxvals=NULL,*minvals=NULL;
    int i,iarea,iarray,nvals;
    int s0,s1,xnew,x00;
    int doreference=reference && refon;
    
/* Find which area */
    iarea=(int)clientdata;
    area=&areas[iarea];
/* Initialize the scale factors */
    if(area->graphinitialize) {
	area->graphinitialize=0;
	resizegraph(w,clientdata,calldata);
      /* resizegraph calls repaintgraph so we can exit here */
	return;
    }
/* Exit if channel access not initialized */
    if(!ecainitialized) return;
/* Define common variables */    
    pix=area->pixmap;
    gc=area->gc;
    win=XtWindow(w);
/* Clear window */
    if(autoclear || area->tempclear) {
	XSetForeground(display,gc,whitepixel);
	XFillRectangle(display,pix,gc,0,0,area->width,area->height);
    }
    area->tempclear=0;
/* Set pixel conversion coefficients */    
    aptox=area->aptox;
    aptos=area->aptos;
    axtop=area->axtop;
    astop=area->astop;
    bptox=area->bptox;
    bptos=area->bptos;
    bxtop=area->bxtop;
    bstop=area->bstop;
/* Calculate pixel coordinates for x=0 */
    x00=xpix(area->centerval);
/* Draw axes and grid */
    if(grid) {
	XSetForeground(display,gc,greypixel);
	s0=spix(area->smin);
	s1=spix(area->smax);
	for(i=-GRIDDIVISIONS; i <= GRIDDIVISIONS; i++) {
	    xnew=xpix(area->centerval+(double)i*scale[area->curscale]);
	    XDrawLine(display,pix,gc,s0,xnew,s1,xnew);
	}
    }
    XSetForeground(display,gc,blackpixel);
    XDrawLine(display,pix,gc,
      spix(area->smin),x00,spix(area->smax),x00);
/* Quit if temporary no draw */
    if(area->tempnodraw) {
	XCopyArea(display,pix,win,gc,0,0,area->width,area->height,0,0);
	area->tempnodraw=0;
	return;
    }
/* Loop over arrays for max/min */
    if(showmaxmin) {
	for(iarray=0; iarray < narrays; iarray++) {
	    array=&arrays[iarray];
	    if(array->area != area) continue;
	  /* Filled mode */
	    if(fillmaxmin) {
		drawmaxmin(iarray);
	    }
	  /* Lines mode */
	    else {
		nvals=array->nvals;
		minvals=array->minvals;
		maxvals=array->maxvals;
		if(whichorbit >= 0) wsavevals=array->savevals[whichorbit];
		else if(doreference) refvals=array->refvals;
		for(i=0; i < nvals; i++) {
		    xplot[i]=minvals[i];
		    if(whichorbit >= 0) xplot[i]-=wsavevals[i];
		    else if(reference && refon) xplot[i]-=refvals[i];
		}
		draw(iarray,array->pixel);
		for(i=0; i < nvals; i++) {
		    xplot[i]=maxvals[i];
		    if(whichorbit >= 0) xplot[i]-=wsavevals[i];
		    else if(reference && refon) xplot[i]-=refvals[i];
		}
		draw(iarray,array->pixel);
	    }
	}
      /* Redraw axis in case it was filled over */
	XSetForeground(display,gc,blackpixel);
	XDrawLine(display,pix,gc,
	  spix(area->smin),x00,spix(area->smax),x00);
    }
/* Loop over arrays for saved sets */
    if(displaysave) {
	for(iarray=0; iarray < narrays; iarray++) {
	    array=&arrays[iarray];
	    if(array->area != area) continue;
	    nvals=array->nvals;
	    dsavevals=array->savevals[displaysave-1];
	    if(whichorbit >= 0) wsavevals=array->savevals[whichorbit];
	    else if(doreference) refvals=array->refvals;
	    for(i=0; i < nvals; i++) {
		xplot[i]=dsavevals[i];
		if(whichorbit >= 0) xplot[i]-=wsavevals[i];
		else if(doreference) xplot[i]-=refvals[i];
	    }
	    draw(iarray,displaypixel);
	}
    }
/* Loop over arrays for main curves */
    for(iarray=0; iarray < narrays; iarray++) {
	array=&arrays[iarray];
	if(array->area != area) continue;
	nvals=array->nvals;
	vals=array->vals;
	if(whichorbit >= 0) wsavevals=array->savevals[whichorbit];
	else if(doreference) refvals=array->refvals;
	for(i=0; i < nvals; i++) {
	    xplot[i]=vals[i];
	    if(whichorbit >= 0) xplot[i]-=wsavevals[i];
	    else if(doreference) xplot[i]-=refvals[i];
	}
	draw(iarray,array->pixel);
    }
/* Copy the pixmap to the window */
    XCopyArea(display,pix,XtWindow(area->wgrapharea),gc,0,0,
      area->width,area->height,0,0);
}
/**************************** repaintlogo *********************************/
void repaintlogo(Widget w, XtPointer clientdata, XtPointer calldata)
{
    Window root,win;
    unsigned int border,depth,wwidth,wheight;
    unsigned int aps_width=64,aps_height=64;
    int x0,y0,width;
    int len;
    int xstart,ystart;
    static int ifirst=1,fheight,linespacing;
    static XFontStruct *fontinfo;
    static GC gc;
    
/* Load font and gc */
    if(ifirst) {
	XGCValues gcvalues;
	
	fontinfo=XLoadQueryFont(display,"9x15");
	if(!fontinfo) return;
	fheight=fontinfo->ascent+fontinfo->descent;
	linespacing=(int)(1.5*fheight);
	gcvalues.graphics_exposures = False;
	gcvalues.background=whitepixel;
	gcvalues.foreground=blackpixel;
	gcvalues.font=fontinfo->fid;
	gc=XCreateGC(display,rootwindow,
	  GCBackground|GCForeground|GCGraphicsExposures|GCFont,
	  &gcvalues);
	ifirst=0;
    }
/* Get window size */
    win=XtWindow(w);
    if(!win) return;
    XGetGeometry(display,win,&root,&x0,&y0,&wwidth,&wheight,&border,&depth);
/* Clear the window */    
    XClearWindow(display,win);
/* Write the logo */
    XCopyPlane(display,apspix,win,gc,
      0,0,aps_width,aps_height,50,(wheight-aps_height)/2,1);

/*     ystart=(wheight-fheight)/2+fheight/2; */
/*     ystart=(wheight-fheight)/2; */
    ystart=(wheight-fheight)/2-fheight/2;

    sprintf(string,"Array Display Tool");
    len=(int)strlen(string);
    width=XTextWidth(fontinfo,string,len);
    xstart=(wwidth-width)/2;
    XDrawString(display,win,gc,xstart,ystart-linespacing,string,len);

    sprintf(string,"Written by Dwarfs in the Waterfall Glenn");
    len=(int)strlen(string);
    width=XTextWidth(fontinfo,string,len);
    xstart=(wwidth-width)/2;
    XDrawString(display,win,gc,xstart,ystart,string,len);

    sprintf(string,"Version %s %s",ADT_VERSION_STRING,EPICS_VERSION_STRING);
    len=(int)strlen(string);
    width=XTextWidth(fontinfo,string,len);
    xstart=(wwidth-width)/2;
    XDrawString(display,win,gc,xstart,ystart+linespacing,string,len);

    sprintf(string,unamestring);
    len=(int)strlen(string);
    width=XTextWidth(fontinfo,string,len);
    xstart=(wwidth-width)/2;
    XDrawString(display,win,gc,xstart,ystart+2*linespacing,string,len);

    sprintf(string,"Please Send Comments and Bugs to evans@aps.anl.gov");
    len=(int)strlen(string);
    width=XTextWidth(fontinfo,string,len);
    xstart=(wwidth-width)/2;
    XDrawString(display,win,gc,xstart,ystart+3*linespacing,string,len);
}
/**************************** repaintzoom *********************************/
void repaintzoom(Widget w, XtPointer clientdata, XtPointer calldata)
{
    GC gc;
    Pixmap pix;
    struct ARRAY *array;
    int i,iarray,is,iis,nvals,zoomindxmax,zoomindxmin;
    int s0,s1,s2,x00,xmid,xnew,unit,bunit;
    int doreference=reference && refon;
    double soff,snumber;
    double *dsavevals=NULL,*wsavevals=NULL,*vals=NULL,*refvals=NULL;
    
/* Return if zoom is off */
    if(!zoomon || !zoom) return;
/* Initialize the scale factors */
    if(zoomarea->graphinitialize) {
	zoomarea->graphinitialize=0;
	resizezoom(w,clientdata,calldata);
      /* resizezoom calls repaintzoom so we can exit here */
	return;
    }
/* Exit if channel access not initialized */
    if(!ecainitialized) return;
/* Define common variables */    
    pix=zoomarea->pixmap;
    gc=zoomarea->gc;
/* Clear window */
    if(autoclear || zoomarea->tempclear) {
	XSetForeground(display,gc,whitepixel);
	XFillRectangle(display,pix,gc,0,0,zoomarea->width,zoomarea->height);
    }
    zoomarea->tempclear=0;
/* Set pixel conversion coefficients */    
    aptox=zoomarea->aptox;
    aptos=zoomarea->aptos;
    axtop=zoomarea->axtop;
    astop=zoomarea->astop;
    bptox=zoomarea->bptox;
    bptos=zoomarea->bptos;
    bxtop=zoomarea->bxtop;
    bstop=zoomarea->bstop;
/* Calculate pixel coordinates for x=0 */
    x00=xpix(zoomarea->centerval);
/* Draw axes and grid */
    if(grid) {
	XSetForeground(display,gc,greypixel);
	s0=spix(zoomarea->smin);
	s1=spix(zoomarea->smax);
	for(i=-GRIDDIVISIONS; i <= GRIDDIVISIONS; i++) {
	    xnew=xpix(zoomarea->centerval+(double)i*scale[zoomarea->curscale]);
	    XDrawLine(display,pix,gc,s0,xnew,s1,xnew);
	}
    }
    XSetForeground(display,gc,blackpixel);
    XDrawLine(display,pix,gc,
      spix(zoomarea->smin),x00,spix(zoomarea->smax),x00);
/* Quit if temporary no draw */
    if(zoomarea->tempnodraw) {
	XCopyArea(display,pix,zoomareawindow,gc,0,0,
	  zoomarea->width,zoomarea->height,0,0);
	zoomarea->tempnodraw=0;
	return;
    }
/* Draw symbols */
    if(symbols && nsymbols) {
	XSetForeground(display,gc,blackpixel);
	bunit=(int)(-.015*zoomarea->height);
	for(iis=zoomsymmin; iis <= zoomsymmax; iis++) {
	    is=iis;
	    soff=0.;
	    while(is > nsymbols-1) {
		is-=nsymbols;
		soff+=stotal;
		
	    }
	    while(is < 0) {
		is+=nsymbols;
		soff-=stotal;
	    }
	    s1=spix(ecasyms[is]+soff);
	    unit=ecasymheight[is]*bunit;
	    XDrawLine(display,pix,gc,s1,x00,s1,x00+unit);
	    if(ecasymlen[is]) {
		s2=spix(ecasyms[is]+ecasymlen[is]+soff);
		XDrawLine(display,pix,gc,s1,x00+unit,s2,x00+unit);
		XDrawLine(display,pix,gc,s2,x00+unit,s2,x00);
	    }
	}
    }
/* Draw numbers */
    if(symbols && nsymbols) {
	XSetForeground(display,gc,blackpixel);
	bunit=15;
	for(i=0; i < nsect; i++) {
	    snumber=((double)i+.5)*stotal/(double)nsect;
	    if(snumber < zoomarea->smin) snumber +=stotal;
	    if(snumber > zoomarea->smax) snumber -=stotal;
	    if(snumber > zoomarea->smin && snumber < zoomarea->smax) {
		sprintf(string,"%d",i+1);
		XDrawString(display,pix,gc,
		  spix(snumber),bunit,string,(int)strlen(string));
	    }
	}
    }
/* Draw guideline */
    if(symbols && nsymbols) {
	XSetForeground(display,gc,blackpixel);
	bunit=3;
	xmid=spix(smid);
	XDrawLine(display,pix,gc,xmid,0,xmid,bunit);
    }
/* Draw displayed arrays if any */
    if(displaysave) {
	for(iarray=0; iarray < narrays; iarray++) {
	    array=&arrays[iarray];
	    if(!array->zoom) continue;
	    nvals=array->nvals;
	    zoomindxmin=array->zoomindxmin;
	    zoomindxmax=array->zoomindxmax;
	    dsavevals=array->savevals[displaysave-1];
	    if(whichorbit >= 0) wsavevals=array->savevals[whichorbit];
	    else if(doreference) refvals=array->refvals;
	    for(iis=zoomindxmin; iis <= zoomindxmax; iis++) {
		is=iis;
		while(is > nvals-1) {
		    is-=nvals;
		}
		while(is < 0) {
		    is+=nvals;
		}
		xplot[is]=dsavevals[is];
		if(whichorbit >= 0) xplot[is]-=wsavevals[is];
		else if(doreference) xplot[is]-=refvals[is];
	    }
	    drawzoom(iarray,displaypixel);
	}
    }
/* Draw current arrays */    
    for(iarray=0; iarray < narrays; iarray++) {
	array=&arrays[iarray];
	if(!array->zoom) continue;
	nvals=array->nvals;
	zoomindxmin=array->zoomindxmin;
	zoomindxmax=array->zoomindxmax;
	if(whichorbit >= 0) wsavevals=array->savevals[whichorbit];
	else if(doreference) refvals=array->refvals;
	vals=array->vals;
	for(iis=zoomindxmin; iis <= zoomindxmax; iis++) {
	    is=iis;
	    while(is > nvals-1) {
		is-=nvals;
	    }
	    while(is < 0) {
		is+=nvals;
	    }
	    xplot[is]=vals[is];
	    if(whichorbit >= 0) xplot[is]-=wsavevals[is];
	    else if(doreference) xplot[is]-=refvals[is];
	}
	drawzoom(iarray,array->pixel);
    }
/* Copy the pixmap to the window */
    XCopyArea(display,pix,zoomareawindow,gc,0,0,
      zoomarea->width,zoomarea->height,0,0);
}
/**************************** resetgraph **********************************/
void resetgraph(void)
{
    int ia;
    
    for(ia=0; ia < nareas; ia++) {
	repaintgraph(areas[ia].wgrapharea,(XtPointer)areas[ia].area,(XtPointer)NULL);
    }
    if(zoom && zoomon)
	repaintzoom((Widget)0,(XtPointer)0,(XtPointer)0);
}
/**************************** resizegraph *********************************/
void resizegraph(Widget w, XtPointer clientdata, XtPointer calldata)
{
    Window root,win;
    int gxmin,gxmax,gymin,gymax,smargin,ymargin;
    int x0,y0,ia;
    unsigned int border,depth,lwidth,lheight;
    double ssmin,ssmax,xxmin,xxmax;
    double captos,captox,castop,caxtop;
    double cbptos,cbptox,cbstop,cbxtop;
    
/* Find which area */
    ia=(int)clientdata;
/* Get window size */
    win=XtWindow(w);
    if(!win) return;
    XGetGeometry(display,win,&root,&x0,&y0,&lwidth,&lheight,&border,&depth);
    areas[ia].width=lwidth;
    areas[ia].height=lheight;
/* Reset graph parameters */
    gxmin=0;
    gxmax=lwidth-1;
    gymin=0;
    gymax=lheight-1;
    smargin=0;
    ymargin=0;
    xxmin=areas[ia].xmin;
    xxmax=areas[ia].xmax;
    ssmin=areas[ia].smin;
    ssmax=areas[ia].smax;
/* Set coefficients for transformations between real and pixel coordinates */
/* pix to s */
    captos=(-ssmax+ssmin)/(-gxmax+gxmin+2*smargin);
    cbptos=(gxmin*ssmax+smargin*ssmax-gxmax*ssmin+smargin*ssmin)/
      (-gxmax+gxmin+2*smargin);
/* pix to x */
    captox=(xxmax-xxmin)/(-gymax+gymin+2*ymargin);
    cbptox=(-(gymax*xxmax)+gymin*xxmin+xxmax*ymargin+xxmin*ymargin)/
      (-gymax+gymin+2*ymargin);
/* s to pix */
    castop=(-gxmax+gxmin+2*smargin)/(-ssmax+ssmin);
    cbstop=(gxmin*ssmax+smargin*ssmax-gxmax*ssmin+smargin*ssmin)/(ssmax-ssmin);
/* x to pix */
    caxtop=(-gymax+gymin+2*ymargin)/(xxmax-xxmin);
    cbxtop=(gymax*xxmax-gymin*xxmin-xxmax*ymargin-xxmin*ymargin)/(xxmax-xxmin);
/* Set values in AREA structure */
    areas[ia].aptos=captos;
    areas[ia].bptos=cbptos;
    areas[ia].aptox=captox;
    areas[ia].bptox=cbptox;
    areas[ia].astop=castop;
    areas[ia].bstop=cbstop;
    areas[ia].axtop=caxtop;
    areas[ia].bxtop=cbxtop;
    if(areas[ia].pixmap) {
	XFreePixmap(display,areas[ia].pixmap);
	areas[ia].pixmap=(Pixmap)0;
    }
    areas[ia].pixmap=XCreatePixmap(display,rootwindow,lwidth,lheight,nplanes);
    XSetForeground(display,areas[ia].gc,whitepixel);
    XFillRectangle(display,areas[ia].pixmap,areas[ia].gc,0,0,lwidth,lheight);
/* Clear window */
    XClearWindow(display,win);
    repaintgraph(w,clientdata,calldata);
}
/**************************** resizelogo **********************************/
void resizelogo(Widget w, XtPointer clientdata, XtPointer calldata)
{
    repaintlogo(w,clientdata,calldata);
}
/**************************** resizezoom **********************************/
void resizezoom(Widget w, XtPointer clientdata, XtPointer calldata)
{
    Window root;
    struct ARRAY * array;
    int gxmin,gxmax,gymin,gymax,smargin,ymargin;
    int x0,y0,ia;
    unsigned int border,depth,lwidth,lheight;
    double ssmin,ssmax,xxmin,xxmax;
    double captos,captox,castop,caxtop;
    double cbptos,cbptox,cbstop,cbxtop;
    
/* Return if zoom is off */
    if(!zoomon || !zoom) return;
/* Get window size */
    XGetGeometry(display,zoomareawindow,&root,&x0,&y0,
      &lwidth,&lheight,&border,&depth);
    zoomarea->width=lwidth;
    zoomarea->height=lheight;
/* Reset zoom parameters */
    gxmin=0;
    gxmax=lwidth-1;
    gymin=0;
    gymax=lheight-1;
    smargin=0;
    ymargin=0;
    xxmin=zoomarea->xmin;
    xxmax=zoomarea->xmax;
    ssmin=zoomarea->smin;
    ssmax=zoomarea->smax;
/* Set coefficients for transformations between real and pixel coordinates */
/* pix to s */
    captos=(-ssmax+ssmin)/(-gxmax+gxmin+2*smargin);
    cbptos=(gxmin*ssmax+smargin*ssmax-gxmax*ssmin+smargin*ssmin)/
      (-gxmax+gxmin+2*smargin);
/* pix to x */
    captox=(xxmax-xxmin)/(-gymax+gymin+2*ymargin);
    cbptox=(-(gymax*xxmax)+gymin*xxmin+xxmax*ymargin+xxmin*ymargin)/
      (-gymax+gymin+2*ymargin);
/* s to pix */
    castop=(-gxmax+gxmin+2*smargin)/(-ssmax+ssmin);
    cbstop=(gxmin*ssmax+smargin*ssmax-gxmax*ssmin+smargin*ssmin)/(ssmax-ssmin);
/* x to pix */
    caxtop=(-gymax+gymin+2*ymargin)/(xxmax-xxmin);
    cbxtop=(gymax*xxmax-gymin*xxmin-xxmax*ymargin-xxmin*ymargin)/(xxmax-xxmin);
/* Set values in AREA structure */
    zoomarea->aptos=captos;
    zoomarea->bptos=cbptos;
    zoomarea->aptox=captox;
    zoomarea->bptox=cbptox;
    zoomarea->astop=castop;
    zoomarea->bstop=cbstop;
    zoomarea->axtop=caxtop;
    zoomarea->bxtop=cbxtop;
    if(zoomarea->pixmap) {
	XFreePixmap(display,zoomarea->pixmap);
	zoomarea->pixmap=(Pixmap)0;
    }
    zoomarea->pixmap=XCreatePixmap(display,rootwindow,lwidth,lheight,nplanes);
    XSetForeground(display,zoomarea->gc,whitepixel);
    XFillRectangle(display,zoomarea->pixmap,zoomarea->gc,0,0,lwidth,lheight);
/* Get index limits */    
    if(zoom) {
	indxlimits(zoomarea->smin,zoomarea->smax,ecasyms,nsymbols,
	  &zoomsymmin,&zoomsymmax);
	for(ia=0; ia < narrays; ia++) {
	    array=&arrays[ia];
	    if(!array->zoom) continue;
	    indxlimits(zoomarea->smin,zoomarea->smax,array->s,array->nvals,
	      &array->zoomindxmin,&array->zoomindxmax);
	}
    }
/* Clear window */
    XClearWindow(display,zoomareawindow);
    repaintzoom(w,clientdata,calldata);
}
/**************************** scrollzoommid *******************************/
void scrollzoommid(void)
{
    XEvent event;
    XtWorkProcId workprocid;
    
/* Return if zoom is off */
    if(!zoomon || !zoom) return;
/* Initialize */
    XGrabPointer(display,zoomareawindow,FALSE,
      ButtonMotionMask|ButtonReleaseMask,
      GrabModeAsync,GrabModeAsync,zoomareawindow,
      None,CurrentTime);
/*    XGrabServer(display); */
    XFlush(display);
    workprocid=XtAppAddWorkProc(appcontext,scrollzoomwork,NULL);
/* Loop until button released */
    while(TRUE) {
	XtAppNextEvent(appcontext,&event);
	switch(event.type) {
	case MotionNotify:
	    xscroll=event.xbutton.x;
	    break;
	case ButtonRelease:
	    XtRemoveWorkProc(workprocid);
	    xscroll=event.xbutton.x;
	    scrollzoomwork((XtPointer)NULL);
/*	    XUngrabServer(display); */
	    XUngrabPointer(display,CurrentTime);
	    return;
	default:
	    XtDispatchEvent(&event);
	}
    }
}
/**************************** scrollzoomwork ******************************/
Boolean scrollzoomwork(XtPointer clientdata)
{
/* Set pixel conversion coefficients */    
    aptos=zoomarea->aptos;
    bptos=zoomarea->bptos;
    smid=pixs(xscroll);
    movezoommid();
    return(FALSE);
}
