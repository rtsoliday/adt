/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* file.c *** Routines to read and write files */

#define DEBUG_GC 0

#include <SDDS.h>
#include "adt.h"

#define PVID "ADTPV"
#define LATID "ADTLATTICE"
#define SNAPID "ADTSNAP"
#define SDDSID "SDDS1"
#define ARRAYEXTRA 4
    
/* Function prototypes */

int checkcolumn(SDDS_TABLE *table, char *name, char * filename, int required);
int checkparameter(SDDS_TABLE *table, char *name, char * filename,
  int required);

/**************************** checkcolumn *********************************/
int checkcolumn(SDDS_TABLE *table, char *name, char * filename, int required)
{
    COLUMN_DEFINITION *coldef;

    coldef=SDDS_GetColumnDefinition(table,name);
    if(!coldef) {
	if(required == REQUIRED) {
	    xerrmsg("Did not find %s column in %s",name,filename);
	    SDDS_Terminate(table);
	}
	return 0;
    }
    SDDS_FreeColumnDefinition(coldef);
    return 1;
}
/**************************** checkparameter ******************************/
int checkparameter(SDDS_TABLE *table, char *name, char * filename,
  int required)
{
    PARAMETER_DEFINITION *parmdef;

    parmdef=SDDS_GetParameterDefinition(table,name);
    if(!parmdef) {
	if(required == REQUIRED) {
	    xerrmsg("Did not find %s parameter in %s",name,filename);
	    SDDS_Terminate(table);
	}
	return 0;
    }
    SDDS_FreeParameterDefinition(parmdef);
    return 1;
}
/**************************** readinit ************************************/
int readinit(char *filename)
{
    SDDS_TABLE table;
    Widget w,w1;
    int foundlabels=0;
    int i,npvfiles;
    long nmenu;
    int32_t templong;
    char **rawnames,**pvmenulabel;
    char *strptr;
    
  /* Set defaults */
    sprintf(pvdirectory,"%s/pv",adthome);
    sprintf(customdirectory,"%s/pv",adthome);
    sprintf(snapdirectory,"%s/snap",adthome);
    sprintf(browserfile,"%s/%s",adthome,HELPFILENAME);
  /* Check filename */
    if(!*filename) return 1;
  /* Clear errors */
    SDDS_ClearErrors();
  /* Open file */
    if(!SDDS_InitializeInput(&table,filename)) {
	if(SDDS_NumberOfErrors())
	  SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
	fprintf(stderr,"Unable to read %s -- Using defaults\n",filename);
	return 1;
    }
  /* Check for columns */
    if(!checkparameter(&table,"ADTNMenus",filename,REQUIRED)) return 0;
    if(!checkparameter(&table,"ADTMenuTitle",filename,REQUIRED)) return 0;
    if(!checkcolumn(&table,"ADTPVFile",filename,REQUIRED)) return 0;
    if(!checkcolumn(&table,"ADTMenuLabel",filename,CHECK)) foundlabels=0;
    else foundlabels=1;
  /* Loop over pages */
    while((nmenu=SDDS_ReadTable(&table)) > 0) {
	nmenu--;
      /* First time */
	if(nmenu == 0) {
	  /* Get ADTPVDirectory */
	    if(checkparameter(&table,"ADTPVDirectory",filename,CHECK)) {
		strptr=NULL;
		if(SDDS_GetParameter(&table,"ADTPVDirectory",&strptr)) {
		    strncpy(pvdirectory,strptr,PATH_MAX);
		    pvdirectory[PATH_MAX-1]='\0';
		    strncpy(customdirectory,strptr,PATH_MAX);
		    customdirectory[PATH_MAX-1]='\0';
		}
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    }
	  /* Get ADTPVSubDirectory */
	    if(checkparameter(&table,"ADTPVSubDirectory",filename,CHECK)) {
		strptr=NULL;
		if(SDDS_GetParameter(&table,"ADTPVSubDirectory",&strptr)) {
                  if (strlen(strptr) > 0) {
                    sprintf(pvdirectory,"%s/%s", adthome, strptr);
                    sprintf(customdirectory,"%s/%s", adthome, strptr);
                  } else {
                    sprintf(pvdirectory,"%s", adthome);
                    sprintf(customdirectory,"%s", adthome);
                  }
		}
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    }
	  /* Get ADTSnapDirectory */
	    if(checkparameter(&table,"ADTSnapDirectory",filename,CHECK)) {
		strptr=NULL;
		if(SDDS_GetParameter(&table,"ADTSnapDirectory",&strptr)) {
		    strncpy(snapdirectory,strptr,PATH_MAX);
		    snapdirectory[PATH_MAX-1]='\0';
		}
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    }
	  /* Get Xorbit directories if necessary */
	    if(xorbit) {
	      /* Get ADTXPVDirectory */
		if(checkparameter(&table,"ADTXPVDirectory",filename,CHECK)) {
		    strptr=NULL;
		    if(SDDS_GetParameter(&table,"ADTXPVDirectory",&strptr)) {
			strncpy(pvdirectory,strptr,PATH_MAX);
			pvdirectory[PATH_MAX-1]='\0';
			strncpy(customdirectory,strptr,PATH_MAX);
			customdirectory[PATH_MAX-1]='\0';
		    }
		    if(strptr) SDDS_Free(strptr); strptr=NULL;
		}
	      /* Get ADTXSnapDirectory */
		if(checkparameter(&table,"ADTXSnapDirectory",filename,CHECK)) {
		    strptr=NULL;
		    if(SDDS_GetParameter(&table,"ADTXSnapDirectory",&strptr)) {
			strncpy(snapdirectory,strptr,PATH_MAX);
			snapdirectory[PATH_MAX-1]='\0';
		    }
		    if(strptr) SDDS_Free(strptr); strptr=NULL;
		}
	    }
	  /* Get ADTHelpFile */
	    if(checkparameter(&table,"ADTHelpFile",filename,CHECK)) {
		strptr=NULL;
		if(SDDS_GetParameter(&table,"ADTHelpFile",&strptr)) {
		    strncpy(browserfile,strptr,PATH_MAX);
		    browserfile[PATH_MAX-1]='\0';
		}
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    }
	  /* Allocate filename array pointers */
	    if(!SDDS_GetParameterAsLong(&table,"ADTNMenus",&templong)) {
		xerrmsg("Could not get ADTNMenus in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	    npvmenus=(int)templong;
	    pvfiles=(char ***)calloc(npvmenus,sizeof(char **));
	    if(!pvfiles) {
		xerrmsg("Could not allocate space for pv menu name pointers"
		  "in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	}     /* End first time */
      /* Check menu number */
	if(nmenu >= npvmenus) {
	    if(nmenu == npvmenus)
	      xerrmsg("Found too many menus in %s\n Check ADTNMenus",filename);
	    continue;
	}
      /* Find number of files */
	npvfiles=SDDS_CountRowsOfInterest(&table);
	if(npvfiles <= 0) continue;
      /* Get names */
	rawnames=(char **)SDDS_GetColumn(&table,"ADTPVFile");
	if(!rawnames) {
	    fprintf(stderr,"Could not get ADTPVFile's for menu %ld in %s\n",
	      nmenu+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Make file names */
	allocnames(npvfiles,PATH_MAX,&pvfiles[nmenu]);
	for(i=0; i < npvfiles; i++) {
	    sprintf(pvfiles[nmenu][i],"%s/%s",pvdirectory,rawnames[i]);
	}
	freesddsnames(npvfiles,&rawnames);
      /* Get menu labels */
	if(foundlabels) {
	    pvmenulabel=(char **)SDDS_GetColumn(&table,"ADTMenuLabel");
	    if(!pvmenulabel) {
		fprintf(stderr,"Could not get ADTMenuLabel's in %s\n",
		  filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	} else {     /* Make menu labels */
	    allocnames(npvfiles,80,&pvmenulabel);
	    for(i=0; i < npvfiles; i++) {
		strncpy(pvmenulabel[i],pvfiles[nmenu][i],80);
		pvmenulabel[79]='\0';
	    }
	}
      /* Define menu */
	nargs=0;
	XtSetArg(args[nargs],XmNtearOffModel,XmTEAR_OFF_ENABLED); nargs++;
	w1=XmCreatePulldownMenu(loadmenu,"openMenu",args,nargs);
      /* Get button title */
	strptr=NULL;
	if(!SDDS_GetParameter(&table,"ADTMenuTitle",&strptr)) {
	    fprintf(stderr,"Could not get ADTMenuTitle for menu %ld in %s\n",
	      nmenu+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Make cascade button */
	nargs=0;
	XtSetArg(args[nargs],XmNsubMenuId,w1); nargs++;
	w=XmCreateCascadeButton(loadmenu,strptr,args,nargs);
	XtManageChild(w);
	if(strptr) SDDS_Free(strptr); strptr=NULL;
      /* Add buttons */
	for(i=0; i < npvfiles; i++) {
	    nargs=0;
	    w=XmCreatePushButton(w1,pvmenulabel[i],args,nargs);
	    XtManageChild(w);
	    XtAddCallback(w,XmNactivateCallback,openmenucb,(caddr_t)pvfiles[nmenu][i]);
	}
	freesddsnames(npvfiles,&pvmenulabel);
    }
  /* Free the SDDS files */
    SDDS_Terminate(&table);
  /* Check for errors */
    if(SDDS_NumberOfErrors())
      SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
  /* Return     */
    return 1;
}
/**************************** readlat *************************************/
int readlat(char *filename)
{
    SDDS_TABLE table;
    COLUMN_DEFINITION *coldef;
    struct ARRAY *array;
    double sscale,stotal0;
    int i,ia,j,jj,jstart,found;
    char **rawnames;
    char *strptr;
    long nsect_exists,ring_exists;
    int32_t templong;
    
  /* Check filename */
    if(!filename) {
	xerrmsg("The Lattice filename is undefined");
	return 0;
    }
    if(!*filename) {
	xerrmsg("The Lattice filename is blank");
	return 0;
    }
  /* Clear errors */
    SDDS_ClearErrors();
  /* Open file */
    if(!SDDS_InitializeInput(&table,filename)) {
	xerrmsg("Unable to read %s",filename);
	if(SDDS_NumberOfErrors())
	  SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
	return 0;
    }
  /* Check file type */
    switch(SDDS_CheckParameter(&table,"ADTFileType",NULL,SDDS_STRING,NULL)) {
    case SDDS_CHECK_OKAY:
	break;
    default:
	xerrmsg("Did not find ADTFileType in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    }
  /* Check nsect */
    nsect=1;
    switch(SDDS_CheckParameter(&table,"Nsectors",NULL,SDDS_SHORT,NULL)) {
    case SDDS_CHECK_OKAY:
	nsect_exists = 1;
	break;
    case SDDS_CHECK_NONEXISTENT:
	nsect_exists = 0;
	break;
    default:
	xerrmsg("Problem with Nsectors in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    }
  /* Get stotal */
    switch(SDDS_CheckParameter(&table,"Stotal",NULL,SDDS_DOUBLE,NULL)) {
    case SDDS_CHECK_OKAY:
	break;
    default:
	xerrmsg("Did not find Stotal in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    }
    
  /* Check ring */
    ring=1;
    switch(SDDS_CheckParameter(&table,"Ring",NULL,SDDS_SHORT,NULL)) {
    case SDDS_CHECK_OKAY:
	ring_exists = 1;
	break;
    case SDDS_CHECK_NONEXISTENT:
	ring_exists = 0;
    default:
	xerrmsg("Problem with Ring in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    }
    
  /* Check for S column */
    coldef=SDDS_GetColumnDefinition(&table,"S");
    if(!coldef) {
	xerrmsg("Did not find S column in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    } else SDDS_FreeColumnDefinition(coldef);
  /* Check for Length column */
    coldef=SDDS_GetColumnDefinition(&table,"Length");
    if(!coldef) {
	xerrmsg("Did not find Length column in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    } else SDDS_FreeColumnDefinition(coldef);
  /* Check for SymbolHeight column */
    coldef=SDDS_GetColumnDefinition(&table,"SymbolHeight");
    if(!coldef) {
	xerrmsg("Did not find SymbolHeight column in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    } else SDDS_FreeColumnDefinition(coldef);
  /* Check for Name column */
    coldef=SDDS_GetColumnDefinition(&table,"Name");
    if(!coldef) {
	xerrmsg("Did not find Name column in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    } else SDDS_FreeColumnDefinition(coldef);
    
  /* Get data */
    if(!SDDS_ReadTable(&table)) {
	xerrmsg("Could not get Data in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    }
  /* Check the values of the parameters after reading the page */  
    strptr=NULL;
    if(!(SDDS_GetParameter(&table,"ADTFileType",&strptr))) {
	SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	exit(1);
    }
    if(strcmp(strptr,LATID)) {
	xerrmsg("Not a valid ADT Lattice file: %s",filename);
	if(strptr) SDDS_Free(strptr); strptr=NULL;
	SDDS_Terminate(&table);
	return 0;
    }
    if(strptr) SDDS_Free(strptr); strptr=NULL;
    if(nsect_exists) {
	if(!(SDDS_GetParameterAsLong(&table,"Nsectors",&templong))) {
	    SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	    exit(1);
	}
	nsect = (int)templong;
	if(nsect <= 0) {
	    xerrmsg("Lattice has a bad number of sectors: %d",nsect);
	    SDDS_Terminate(&table);
	    return 0;
	}
    }
    if(!(SDDS_GetParameterAsDouble(&table,"Stotal",&stotal0))) {
	SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	exit(1);
    }
    
    if(stotal0 <= 0.) {
	xerrmsg("Lattice has a bad length: %f",stotal0);
	SDDS_Terminate(&table);
	return 0;
    }
    sscale=(double)nsect/stotal0;
    stotal=nsect;
    if(ring_exists) {
	if(!(SDDS_GetParameterAsLong(&table,"Ring",&ring))) {
	    SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	    exit(1);
	}
    }
    nsymbols=SDDS_CountRowsOfInterest(&table);
    if(nsymbols < 1) {
	xerrmsg("Zero rows in Data in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    }
  /* Get columns */
    rawnames=(char **)SDDS_GetColumn(&table,"Name");
    if(!rawnames) {
	xerrmsg("Could not get Name\'s in %s",filename);
	SDDS_Terminate(&table);
	return 0;
    }
    for(i=0; i < nsymbols; i++) {
	if((int)strlen(rawnames[i]) >= ECANAMESIZE) {
	    xerrmsg("Symbol name %s\n from %s is too long",
              rawnames[i],filename);
	    freesddsnames(nsymbols,&rawnames);
	    SDDS_Terminate(&table);
	    return 0;
	}
    }
    ecasyms=(double *)SDDS_GetColumn(&table,"S");
    if(!ecasyms) {
	xerrmsg("Could not get S\'s in %s",filename);
	freesddsnames(nsymbols,&rawnames);
	SDDS_Terminate(&table);
	return 0;
    }
    ecasymlen=(double *)SDDS_GetColumn(&table,"Length");
    if(!ecasymlen) {
	xerrmsg("Could not get Length\'s in %s",filename);
	freesddsnames(nsymbols,&rawnames);
	SDDS_Terminate(&table);
	return 0;
    }
    ecasymheight=(short *)SDDS_GetColumn(&table,"SymbolHeight");
    if(!ecasymheight) {
	xerrmsg("Could not get SymbolHeight\'s in %s",filename);
	freesddsnames(nsymbols,&rawnames);
	SDDS_Terminate(&table);
	return 0;
    }
  /* Scale the lengths to sector units */    
    for(i=0; i < nsymbols; i++) {
	ecasyms[i]*=sscale;
	ecasymlen[i]*=sscale;
    }
  /* Find the PV's in the lattice for each array */
    for(ia=0; ia < narrays; ia++) {
	array=&arrays[ia];
	if(!array->zoom) continue;
	jstart=0;
	for(i=0; i < array->nvals; i++) {
	    found=0;
	    for(j=0; j < nsymbols; j++) {
		jj=jstart+j;    /* Assume PV's are in lattice order */
		if(jj >= nsymbols) jj-=nsymbols;
	      /* DEBUG */
	      /* 		printf("%5d %5d %5d %5d %10.5f %34s %34s\n", */
	      /* 		  i,j,jstart,jj,array->s[i],array->names[i],rawnames[jj]); */
		if(strstr(array->names[i],rawnames[jj])) {
		    found=1;
		    array->s[i]=ecasyms[jj];
		    jstart=jj+1;
		    break;
		}
	    }
	    if(!found) {
		xerrmsg("Did not find an s value for %s in array %d",
		  array->names[i],ia+1);
		freesddsnames(nsymbols,&rawnames);
		SDDS_Terminate(&table);
		return 0;
	    }
	}
    }
  /* Free the SDDS files and unused data */
    freesddsnames(nsymbols,&rawnames);
    SDDS_Terminate(&table);
  /* Check for errors */
    if(SDDS_NumberOfErrors())
      SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
    return 1;
}
/**************************** readpvs *************************************/
int readpvs(char *filename)
{
    SDDS_TABLE table;
    XGCValues gcvalues;
    int i,iarray,iarea,nvals,maxcurscale;
    double unitsperdiv,avgcenterval;
    int oneareaperarray=0,defaultcolor=0;
    char **rawnames;
    int32_t templong;
    char *colorname;
    char *strptr;
#define NCOLORS 16
    static char *defaultcolors[NCOLORS]={
	"Red",
	"Blue",
	"Green3",           /* Green */
	"Gold",             /* Yellow */      
	"Magenta",
	"Cyan",
	"Orange",
	"Purple",
	"SpringGreen",
	"Chartreuse",
	"DeepSkyBlue",
	"MediumSpringGreen",
	"Tomato",
	"Tan",
	"Grey75",
	"Black"
    };	

  /* Check filename */
    if(!filename) {
	xerrmsg("The PV filename is undefined");
	return 0;
    }
    if(!*filename) {
	xerrmsg("The PV filename is blank");
	return 0;
    }
  /* Clear errors */
    SDDS_ClearErrors();
  /* Open file */
    if(!SDDS_InitializeInput(&table,filename)) {
	xerrmsg("Unable to read %s",filename);
	if(SDDS_NumberOfErrors())
	  SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
	return 0;
    }
  /* Check for required parameters */
    if(!checkparameter(&table,"ADTFileType",filename,REQUIRED)) return 0;
    if(!checkparameter(&table,"ADTNArrays",filename,REQUIRED)) return 0;
    if(!checkparameter(&table,"ADTNAreas",filename,CHECK)) oneareaperarray=1;
    if(!checkparameter(&table,"ADTColor",filename,CHECK)) defaultcolor=1;
  /* Check for required columns */
    if(!checkcolumn(&table,"ControlName",filename,REQUIRED)) return 0;
    if(xorbit || !checkcolumn(&table,"StatusName",filename,CHECK)) {
	checkstatus=0;
	XtSetSensitive(checkstatusw,FALSE);
    } else {
	checkstatus=1;
	XtSetSensitive(checkstatusw,TRUE);
    }
    if(xorbit || !checkcolumn(&table,"ThresholdName",filename,CHECK)) {
	checkthreshold=0;
    } else {
      if(!checkcolumn(&table,"ThresholdLowerLimit",filename,REQUIRED)) return 0;
      if(!checkcolumn(&table,"ThresholdUpperLimit",filename,REQUIRED)) return 0;
	checkthreshold=1;
    }
  /* Loop over pages */
    while((iarray=SDDS_ReadTable(&table)) > 0) {
	iarray--;
      /* First time */
	if(iarray == 0) {
	    char *strptr;
	    
	  /* Check file type */
	    if(checkparameter(&table,"ADTFileType",filename,CHECK)) {
		strptr=NULL;
		if(!SDDS_GetParameter(&table,"ADTFileType",&strptr)) {
		    xerrmsg("Could not get ADTFileType in %s",filename);
		    SDDS_Terminate(&table);
		    return 0;
		} else {
		    if(strcmp(strptr,PVID)) {
			xerrmsg("Not a valid ADT PV file: %s",filename);
			if(strptr) SDDS_Free(strptr); strptr=NULL;
			SDDS_Terminate(&table);
			return 0;
		    }
		}
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    } else {
		xerrmsg("Could not get ADTFileType in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	  /* Get time interval */
	    if(checkparameter(&table,"ADTTimeInterval",filename,CHECK)) {
		if(SDDS_GetParameterAsLong(&table,"ADTTimeInterval",&templong)) {
                  timeinterval=(unsigned long)templong;
		}
	    }
            /* Get zoom interval */
            if(checkparameter(&table,"ADTZoomInterval",filename,CHECK)) {
		if(SDDS_GetParameterAsLong(&table,"ADTZoomInterval",&templong)) {
                  zoominterval=(unsigned long)templong;
		}
	    }
	  /* Get markers */
	    if(checkparameter(&table,"ADTMarkers",filename,CHECK)) {
		if(SDDS_GetParameterAsLong(&table,"ADTMarkers",&templong)) {
                  markers=(int)templong;
                  XmToggleButtonSetState(wmarkers,(Boolean)markers,FALSE);
		}
	    }
	  /* Get lines */
	    if(checkparameter(&table,"ADTLines",filename,CHECK)) {
		if(SDDS_GetParameterAsLong(&table,"ADTLines",&templong)) {
                  lines=(int)templong;
                  XmToggleButtonSetState(wlines,(Boolean)lines,FALSE);
		}
	    }
	  /* Get bars */
	    if(checkparameter(&table,"ADTBars",filename,CHECK)) {
		if(SDDS_GetParameterAsLong(&table,"ADTBars",&templong)) {
                  bars=(int)templong;
                  XmToggleButtonSetState(wbars,(Boolean)bars,FALSE);
		}
	    }
	  /* Get grid */
	    if(checkparameter(&table,"ADTGrid",filename,CHECK)) {
		if(SDDS_GetParameterAsLong(&table,"ADTGrid",&templong)) {
                  grid=(int)templong;
                  XmToggleButtonSetState(wgrid,(Boolean)grid,FALSE);
		}
	    }
	  /* Get maxs/min */
	    if(checkparameter(&table,"ADTMaxMin",filename,CHECK)) {
		if(SDDS_GetParameterAsLong(&table,"ADTMaxMin",&templong)) {
                  showmaxmin=(int)templong;
                  XmToggleButtonSetState(wshowmaxmin,(Boolean)showmaxmin,FALSE);
		}
	    }
	  /* Get filed max/min */
	    if(checkparameter(&table,"ADTFillMaxMin",filename,CHECK)) {
		if(SDDS_GetParameterAsLong(&table,"ADTFillMaxMin",&templong)) {
                  fillmaxmin=(int)templong;
		    XmToggleButtonSetState(wfillmaxmin,(Boolean)fillmaxmin,FALSE);
		}
	    }
	  /* Get lattice file name */
	    if(checkparameter(&table,"ADTLatticeFile",filename,CHECK)) {
		strptr=NULL;
		if(!SDDS_GetParameter(&table,"ADTLatticeFile",&strptr)) {
		    zoom=0;
		    *latfilename='\0';
		} else {
		    zoom=1;
		    if(!strchr(strptr,'/')) sprintf(latfilename,"%s/%s",pvdirectory,strptr);
		    else strncpy(latfilename,strptr,PATH_MAX);
		    latfilename[PATH_MAX-1]='\0';
		}
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    } else {
		zoom=0;
		*latfilename='\0';
	    }
	  /* Get reference file name */
	    if(checkparameter(&table,"ADTReferenceFile",filename,CHECK)) {
		strptr=NULL;
		if(!SDDS_GetParameter(&table,"ADTReferenceFile",&strptr)) {
		    reference=0;
		    *reffilename='\0';
		} else {
		    reference=1;
		    if(!strchr(strptr,'/')) sprintf(reffilename,"%s/%s",snapdirectory,strptr);
		    else strncpy(reffilename,strptr,PATH_MAX);
		    reffilename[PATH_MAX-1]='\0';		      
		}
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    } else {
		reference=0;
		*reffilename='\0';
	    }
	  /* Allocate arrays and areas arrays */
	    if(!SDDS_GetParameterAsLong(&table,"ADTNArrays",&templong)) {
		xerrmsg("Could not get ADTNArrays in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	    narrays=(int)templong;
	    arrays=(struct ARRAY *)calloc(narrays,sizeof(struct ARRAY));
	    if(!arrays) {
		xerrmsg("Could not allocate space for arrays array "
		  "in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	    if(oneareaperarray) templong=narrays;
	    else if(!SDDS_GetParameterAsLong(&table,"ADTNAreas",&templong)) {
		xerrmsg("Could not get ADTNAreas in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	    nareas=(int)templong;
	    areas=(struct AREA *)calloc(nareas,sizeof(struct AREA));
	    if(!areas) {
		xerrmsg("Could not allocate space for areas array "
		  "in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	}     /* End first time */
      /* Note: User arrays start at iarray=1, code arrays start at iarray=0 */	
      /* Note: User areas start at iarea=1, code areas start at iarea=0 */
      /* Arrays values */
      /* Set index in struct */
	if(iarray >= narrays) {
	    xerrmsg("Invalid array (%d) (NArray= %d) in %s",
	      iarray+1,narrays,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
	arrays[iarray].array=iarray;
      /* Get log scale switch  */
	if(checkparameter(&table,"ADTLogScale",filename,CHECK)) {
	    if(!SDDS_GetParameterAsLong(&table,"ADTLogScale",&templong)) {
		templong=0;
	    }
	} else {
	    templong=0;
	}
	arrays[iarray].logscale=(char)templong;
      /* Get zoom area switch  */
	if(checkparameter(&table,"ADTZoomArea",filename,CHECK)) {
	    if(!SDDS_GetParameterAsLong(&table,"ADTZoomArea",&templong)) {
		templong=0;
	    }
	} else {
	    templong=0;
	}
	arrays[iarray].zoom=(char)templong;
      /* Get color */
	strptr=NULL;
	if(defaultcolor) {
	    colorname=defaultcolors[iarray%NCOLORS];
	} else {
	    strptr=NULL;
	    if(!SDDS_GetParameter(&table,"ADTColor",&strptr)) {
		defaultcolor=1;
		colorname=defaultcolors[iarray%NCOLORS];
	    } else {
		colorname=strptr;
	    }
	}
	if(XAllocNamedColor(display,DefaultColormap(display,screen),
	  colorname,&newcolordef,&requestcolordef) == 0) {
	    xerrmsg("Could not allocate color: %s for array %d in %s\n",
	      colorname,iarray+1,filename);
	    arrays[iarray].pixel=blackpixel;
	} else arrays[iarray].pixel=newcolordef.pixel;
	if(strptr) SDDS_Free(strptr); strptr=NULL;
	colorname=NULL;
      /* Get heading */
	if(checkparameter(&table,"ADTHeading",filename,CHECK)) {
	    strptr=NULL;
	    if(!SDDS_GetParameter(&table,"ADTHeading",&strptr) || !strptr) {
		sprintf(string,"Array %d",iarray+1);     /* Default heading */
		arrays[iarray].heading=(char *)calloc((int)strlen(string)+1,sizeof(char));
		strcpy(arrays[iarray].heading,string);
	    } else {
		int len=(int)strlen(strptr)+1;

		if(len > HEADSIZE) len=HEADSIZE;
		arrays[iarray].heading=(char *)calloc(len,sizeof(char));
		strncpy(arrays[iarray].heading,strptr,len);
		arrays[iarray].heading[len-1]='\0';
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    }
	} else {
	    sprintf(string,"Array %d",iarray+1);     /* Default heading */
	    arrays[iarray].heading=(char *)calloc((int)strlen(string)+1,sizeof(char));
	    strcpy(arrays[iarray].heading,string);
	}
      /* Get units */
	if(checkparameter(&table,"ADTUnits",filename,CHECK)) {
	    strptr=NULL;
	    if(!SDDS_GetParameter(&table,"ADTUnits",&strptr) || !strptr) {
		arrays[iarray].units=(char *)calloc(1,sizeof(char));
	    } else {
		int len=(int)strlen(strptr)+1;

		if(len > UNITSSIZE) len=UNITSSIZE;
		arrays[iarray].units=(char *)calloc(len,sizeof(char));
		strncpy(arrays[iarray].units,strptr,len);
		arrays[iarray].units[len-1]='\0';
		if(strptr) SDDS_Free(strptr); strptr=NULL;
	    }
	} else {
	    if(!SDDS_GetParameter(&table,"ADTUnits",&arrays[iarray].units)) {
		arrays[iarray].units=(char *)calloc(1,sizeof(char));
	    }
	}
      /* Get scale factor */
	if(checkparameter(&table,"ADTScaleFactor",filename,CHECK)) {
	    if(!SDDS_GetParameterAsDouble(&table,"ADTScaleFactor",
	      &arrays[iarray].scalefactor)) {
		arrays[iarray].scalefactor=1.0;
	    }
	} else {
	    arrays[iarray].scalefactor=1.0;
	}
      /* Get area */
	if(oneareaperarray) iarea=iarray;
	else {
	    if(checkparameter(&table,"ADTDisplayArea",filename,CHECK)) {
		if(!SDDS_GetParameterAsLong(&table,"ADTDisplayArea",&templong)) {
		    templong=iarray+1;
		}
	    } else templong=iarray+1;
	    iarea=templong-1;
	}
	if(iarea < 0 || iarea >= nareas) {
	    xerrmsg("Invalid area (%d) for array %d in %s\nUsing 1",
	      iarea+1,iarray+1,filename);
	    iarea=0;
	}
	arrays[iarray].area=&areas[iarea];
	
      /* Initialize areas values if not initialized */
	if(!areas[iarea].gc) {
	    areas[iarea].area=iarea;
	    areas[iarea].graphinitialize=1;
	    areas[iarea].tempclear=1;
	    areas[iarea].tempnodraw=1;
	    gcvalues.graphics_exposures = False;
	    gcvalues.background=whitepixel;
	    gcvalues.foreground=blackpixel;
	    areas[iarea].gc=XCreateGC(display,rootwindow,
	      GCBackground|GCForeground|GCGraphicsExposures,
	      &gcvalues);
#if DEBUG_GC
	    print("readpvs: areas[%d].gc=%x\n",iarea,areas[iarea].gc);
#endif
	  /* Get center val */
	    if(checkparameter(&table,"ADTCenterVal",filename,CHECK)) {
		if(!SDDS_GetParameterAsDouble(&table,"ADTCenterVal",&areas[iarea].centerval)) {
		    areas[iarea].centerval=0.0;
		}
	    } else {
		areas[iarea].centerval=0.0;
	    }
	  /* Get units per div */
	    if(checkparameter(&table,"ADTUnitsPerDiv",filename,CHECK)) {
		if(!SDDS_GetParameterAsDouble(&table,"ADTUnitsPerDiv",&unitsperdiv)) {
		    unitsperdiv=1.0;
		}
	    } else {
		unitsperdiv=1.0;
	    }
	  /* Find closest units per div */
	    {
		double labelunits;
		int iscale=0;
		
		for(i=0; i < nscales; i++) {
		    iscale=i;
		    labelunits=scale[i];
		    if(unitsperdiv <= labelunits) break;
		}
		areas[iarea].curscale=iscale;
		areas[iarea].xmax=(areas[iarea].centerval+
		  scale[areas[iarea].curscale]*GRIDDIVISIONS);
		areas[iarea].xmin=(areas[iarea].centerval-
		  scale[areas[iarea].curscale]*GRIDDIVISIONS);
	    }
	}
      /* Allocate space */
	nvals=SDDS_CountRowsOfInterest(&table);
	if(nvals < 1) {
	    xerrmsg("Zero rows in array %d in %s",iarray+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
	arrays[iarray].nvals=nvals;
      /* Make label pixmap */
	arrays[iarray].legendpix=
	  XCreatePixmap(display,rootwindow,LEGENDSIZE,LEGENDSIZE,nplanes);
	XSetForeground(display,areas[iarea].gc,arrays[iarray].pixel);
	XFillRectangle(display,arrays[iarray].legendpix,areas[iarea].gc,
	  0,0,LEGENDSIZE,LEGENDSIZE);
      /* Names */
	if(!allocnames(nvals,ECANAMESIZE,&arrays[iarray].names)) {
	    xerrmsg("Could not allocate space for ControlName's "
	      "for array %d in %s",
	      iarray+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Values */
	arrays[iarray].vals=(double *)calloc(nvals,sizeof(double));
	if(arrays[iarray].vals == NULL) {
	    xerrmsg("Could not allocate space for values for array %d in %s",
	      iarray+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Min values */
	arrays[iarray].minvals=(double *)calloc(nvals,sizeof(double));
	if(arrays[iarray].minvals == NULL) {
	    xerrmsg("Could not allocate space for minimum values "
	      "for array %d in %s",
	      iarray+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Max values */
	arrays[iarray].maxvals=(double *)calloc(nvals,sizeof(double));
	if(arrays[iarray].maxvals == NULL) {
	    xerrmsg("Could not allocate space for maximum values "
	      "for array %d in %s",
	      iarray+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Connection flags */
	arrays[iarray].conn=(Boolean *)calloc(nvals,sizeof(Boolean));
	if(arrays[iarray].conn == NULL) {
	    xerrmsg("Could not allocate space for connection flags "
	      "for array %d in %s",
	      iarray+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Status */
	if(checkstatus) {
	  /* Status Names */
	    if(!allocnames(nvals,ECANAMESIZE,&arrays[iarray].statusnames)) {
		xerrmsg("Could not allocate space for StatusNames's "
		  "for array %d in %s",
		  iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	  /* Status values */
	    arrays[iarray].statusvals=
	      (unsigned short *)calloc(nvals,sizeof(unsigned short));
	    if(arrays[iarray].statusvals == NULL) {
		xerrmsg("Could not allocate space for status values "
		  "for array %d in %s",iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	  /* Status connection flags */
	    arrays[iarray].statusconn=(Boolean *)calloc(nvals,sizeof(Boolean));
	    if(arrays[iarray].statusconn == NULL) {
		xerrmsg("Could not allocate space for status connection flags "
		  "for array %d in %s",iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	}
      /* Threshold */
	if(checkthreshold) {
	  /* Threshold Names */
	    if(!allocnames(nvals,ECANAMESIZE,&arrays[iarray].thresholdnames)) {
		xerrmsg("Could not allocate space for ThresholdNames's "
		  "for array %d in %s",
		  iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	  /* Threshold values */
	    arrays[iarray].thresholdvals=
	      (unsigned short *)calloc(nvals,sizeof(unsigned short));
	    if(arrays[iarray].thresholdvals == NULL) {
		xerrmsg("Could not allocate space for threshold values "
		  "for array %d in %s",iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	  /* Threshold connection flags */
	    arrays[iarray].thresholdconn=(Boolean *)calloc(nvals,sizeof(Boolean));
	    if(arrays[iarray].thresholdconn == NULL) {
		xerrmsg("Could not allocate space for threshold connection flags "
		  "for array %d in %s",iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	}
      /* Get columns */	
      /* Get ControlNames */
	rawnames=(char **)SDDS_GetColumn(&table,"ControlName");
	if(!rawnames) {
	    xerrmsg("Could not get ControlName's for array %d in %s",
	      iarray+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Copy names */
	for(i=0; i < nvals; i++) {
	    if((int)strlen(rawnames[i]) >= ECANAMESIZE-(int)strlen(prefix)) {
		xerrmsg("Name %s\n for array %d in %s is too long",
		  rawnames[i],iarray+1,filename);
		SDDS_Terminate(&table);
	    }
	    strcpy(arrays[iarray].names[i],prefix);
	    strcat(arrays[iarray].names[i],rawnames[i]);
	}
	freesddsnames(nvals,&rawnames);
      /* Status */
	if(checkstatus) {
	  /* Get StatusNames */
	    rawnames=(char **)SDDS_GetColumn(&table,"StatusName");
	    if(!rawnames) {
		xerrmsg("Could not get StatusName's for array %d in %s",
		  iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	  /* Copy status names */
	    for(i=0; i < nvals; i++) {
		if((int)strlen(rawnames[i]) >= ECANAMESIZE-(int)strlen(prefix)) {
		    xerrmsg("StatusName %s\n for array %d in %s is too long",
		      rawnames[i],iarray+1,filename);
		    SDDS_Terminate(&table);
		}
		strcpy(arrays[iarray].statusnames[i],prefix);
		strcat(arrays[iarray].statusnames[i],rawnames[i]);
	    }
	    freesddsnames(nvals,&rawnames);
	}
      /* Threshold */
	if(checkthreshold) {
	  /* Get ThresholdNames */
	    rawnames=(char **)SDDS_GetColumn(&table,"ThresholdName");
	    if(!rawnames) {
		xerrmsg("Could not get ThresholdName's for array %d in %s",
		  iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	  /* Copy threshold names */
	    for(i=0; i < nvals; i++) {
		if((int)strlen(rawnames[i]) >= ECANAMESIZE-(int)strlen(prefix)) {
		    xerrmsg("ThresholdName %s\n for array %d in %s is too long",
		      rawnames[i],iarray+1,filename);
		    SDDS_Terminate(&table);
		}
		strcpy(arrays[iarray].thresholdnames[i],prefix);
		strcat(arrays[iarray].thresholdnames[i],rawnames[i]);
	    }
	    freesddsnames(nvals,&rawnames);
            arrays[iarray].thresholdLL = SDDS_GetColumnInDoubles(&table, "ThresholdLowerLimit");
	    if(arrays[iarray].thresholdLL == NULL) {
		xerrmsg("Could not allocate space for threshold limits "
		  "for array %d in %s",iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
            arrays[iarray].thresholdUL = SDDS_GetColumnInDoubles(&table, "ThresholdUpperLimit");
	    if(arrays[iarray].thresholdUL == NULL) {
		xerrmsg("Could not allocate space for threshold limits "
		  "for array %d in %s",iarray+1,filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	}
    }
  /* Free the SDDS files and unused data */
    SDDS_Terminate(&table);
  /* Check for errors */
    if(SDDS_NumberOfErrors())
      SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
    if(!narrays) {
	xerrmsg("Did not find any arrays in %s",filename);
	return 0;
    }
  /* Check if all areas are defined */
    for(iarea=0; iarea < nareas; iarea++) {
	if(!areas[iarea].tempclear) {
	    xerrmsg("All areas from 1 to NAreas must be used in %s",filename);
	    return 0;
	}
    }
  /* Get maximum size for all arrays */
    nvalsmax=0;
    for(iarray=0; iarray < narrays; iarray++) {
	if(arrays[iarray].nvals > nvalsmax) nvalsmax=arrays[iarray].nvals;
    }
  /* Get maximum size when there is more than one array in an area */
    for(iarea=0; iarea < nareas; iarea++) {
	int nmax=0;
	
	for(iarray=0; iarray < narrays; iarray++) {
	    if(arrays[iarray].area == &areas[iarea] &&
	      arrays[iarray].nvals > nmax)
	      nmax=arrays[iarray].nvals;
	}
	areas[iarea].smin=-.5;
	areas[iarea].smax=(double)(nmax-1)+.5;
    }
  /* Allocate work arrays */
    xplot=(double *)calloc(nvalsmax,sizeof(double));
    if(xplot == NULL) {
	xerrmsg("Could not allocate space for x plot values "
	  "in %s",filename);
	return 0;
    }
    xarray=(int *)calloc(nvalsmax+ARRAYEXTRA,sizeof(int));
    if(xarray == NULL) {
	xerrmsg("Could not allocate space for x array values "
	  "in %s",filename);
	return 0;
    }
    sarray=(int *)calloc(nvalsmax+ARRAYEXTRA,sizeof(int));
    if(sarray == NULL) {
	xerrmsg("Could not allocate space for s array values "
	  "in %s",filename);
	return 0;
    }
  /* Setup zoom area */    
    zoom=0;
    maxcurscale=0;
    avgcenterval=0.;
    for(iarray=0; iarray < narrays; iarray++) {
	if(arrays[iarray].zoom) {
	    zoom++;
	    avgcenterval+=arrays[iarray].area->centerval;
	    if(arrays[iarray].area->curscale > maxcurscale)
	      maxcurscale=arrays[iarray].area->curscale;
	  /* Allocate space for s values */
	    arrays[iarray].s=(double *)calloc(arrays[iarray].nvals,sizeof(double));
	    if(arrays[iarray].s == NULL) {
		xerrmsg("Could not allocate space for s for array %d in %s",
		  iarray+1,filename);
		return 0;
	    }
	}
    }
    if(zoom) {
	zoomarea=(struct ZOOMAREA *)calloc(1,sizeof(struct ZOOMAREA));
	if(zoomarea == NULL) {
	    xerrmsg("Could not allocate space zoom area structure in %s",
	      filename);
	    return 0;
	}
	zoomarea->graphinitialize=1;
	zoomarea->tempclear=1;
	zoomarea->tempnodraw=1;
	gcvalues.graphics_exposures = False;
	gcvalues.background=whitepixel;
	gcvalues.foreground=blackpixel;
	zoomarea->gc=XCreateGC(display,rootwindow,
	  GCBackground|GCForeground|GCGraphicsExposures,
	  &gcvalues);
	zoomarea->curscale=maxcurscale;
	zoomarea->centerval=avgcenterval/(double)zoom;
	zoomarea->xmax=(zoomarea->centerval+
	  scale[zoomarea->curscale]*GRIDDIVISIONS);
	zoomarea->xmin=(zoomarea->centerval-
	  scale[zoomarea->curscale]*GRIDDIVISIONS);
	zoomarea->smin=0.;
	zoomarea->smax=1.;
	sdel=1.;
	smid=.5;
    }
  /* Return */	
    return 1;
}
/**************************** readreference *******************************/
int readreference(char *filename)
{
    SDDS_TABLE table;
    int i,ia,nvals=0;
    char **rawnames,**rawvalues;
    
  /* Check if there is data */
    if(!narrays) {
	xerrmsg("There are no arrays defined");
	return 0;
    }
  /* Check filename */
    if(!filename) {
	xerrmsg("The Reference filename is undefined");
	return 0;
    }
    if(!*filename) {
	xerrmsg("The Reference filename is blank");
	return 0;
    }
  /* Clear errors */
    SDDS_ClearErrors();
  /* Open file */
    if(!SDDS_InitializeInput(&table,filename)) {
	xerrmsg("Unable to read %s",filename);
	if(SDDS_NumberOfErrors())
	  SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
	return 0;
    }
  /* Check for required columns */
    if(!checkcolumn(&table,"ControlName",filename,REQUIRED)) return 0;
    if(!checkcolumn(&table,"ValueString",filename,REQUIRED)) return 0;
  /* Loop over pages */
    for(ia=0; ia < narrays; ia++) {
	if(!SDDS_ReadTable(&table)) {
	    xerrmsg("Could not get Data for array %d in %s",ia+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* First time */
	if(ia == 0) {
	    char *tempstring;

	  /* Check file type */
	    tempstring=NULL;
	    if(!SDDS_GetParameter(&table,"ADTFileType",&tempstring)) {
		xerrmsg("Could not get ADTFileType in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	    if(strcmp(tempstring,SNAPID)) {
		xerrmsg("Not a valid ADT Reference/Snapshot file: %s",filename);
		SDDS_Terminate(&table);
		if(tempstring) SDDS_Free(tempstring); tempstring=NULL;
		return 0;
	    }
	    if(tempstring) SDDS_Free(tempstring); tempstring=NULL;
	}     /* End first time */
      /* Get number of rows */
	nvals=SDDS_CountRowsOfInterest(&table);
	if(nvals != arrays[ia].nvals) {
	    xerrmsg("Wrong number of items [%d] for array %d in %s",
	      nvals,ia+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Get names */
	rawnames=(char **)SDDS_GetColumn(&table,"ControlName");
	if(!rawnames) {
	    xerrmsg("Could not get ControlName\'s for array %d in %s",
	      ia+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Get string values */
	rawvalues=(char **)SDDS_GetColumn(&table,"ValueString");
	if(!rawvalues) {
	    xerrmsg("Could not get ValueString\'s for array %d in %s",
	      ia+1,filename);
	    freesddsnames(nvals,&rawnames);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Allocate space */
	if(!arrays[ia].refvals) {
	    arrays[ia].refvals=
	      (double *)calloc(arrays[ia].nvals,sizeof(double));
	    if(arrays[ia].refvals == NULL) {
		xerrmsg("Could not allocate space for arrray %d in %s",
		  ia+1,filename);
		return 0;
	    }
	}
	strcpy(reffilename,filename);
      /* Check names for match and copy values */
	for(i=0; i < nvals; i++) {
	    if(strcmp(rawnames[i],arrays[ia].names[i]+prefixlength)) {
		xerrmsg("PV does not match: %s from %s",
		  rawnames[i],filename);
		freesddsnames(nvals,&rawnames);
		freesddsnames(nvals,&rawvalues);
		SDDS_Terminate(&table);
		return 0;
	    }
	    arrays[ia].refvals[i]=atof(rawvalues[i]);
	}
    }
  /* Free the SDDS files and unused data */
    freesddsnames(nvals,&rawnames);
    freesddsnames(nvals,&rawvalues);
    SDDS_Terminate(&table);
  /* Check for errors */
    if(SDDS_NumberOfErrors())
      SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
  /* Return */    
    return 1;
}
/**************************** readsnap ************************************/
int readsnap(char *filename, int nsave)
{
    SDDS_TABLE table;
    int i,ia,nvals=0;
    char **rawnames,**rawvalues;
    char time[25];
    
  /* Check if there is data */
    if(!narrays) {
	xerrmsg("There are no arrays defined");
	return 0;
    }
  /* Check filename */
    if(!filename) {
	xerrmsg("The Snapshot filename is undefined");
	return 0;
    }
    if(!*filename) {
	xerrmsg("The Snapshot filename is blank");
	return 0;
    }
  /* Check nsave */
    if(nsave < 0 || nsave > (NSAVE-1)) {
	xerrmsg("Invalid set number for Snapshot file: %s: %d",nsave+1,filename);
	return 0;
    }
  /* Clear errors */
    SDDS_ClearErrors();
  /* Open file */
    if(!SDDS_InitializeInput(&table,filename)) {
	xerrmsg("Unable to read %s",filename);
	if(SDDS_NumberOfErrors())
	  SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
	return 0;
    }
  /* Check for required columns */
    if(!checkcolumn(&table,"ControlName",filename,REQUIRED)) return 0;
    if(!checkcolumn(&table,"ValueString",filename,REQUIRED)) return 0;
  /* Loop over pages */
    for(ia=0; ia < narrays; ia++) {
	if(!SDDS_ReadTable(&table)) {
	    xerrmsg("Could not get Data for array %d in %s",ia+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* First time */
	if(ia == 0) {
	    char *tempstring;
	    
	  /* Check file type */
	    tempstring=NULL;
	    if(!SDDS_GetParameter(&table,"ADTFileType",&tempstring)) {
		xerrmsg("Could not get ADTFileType in %s",filename);
		SDDS_Terminate(&table);
		return 0;
	    }
	    if(strcmp(tempstring,SNAPID)) {
		xerrmsg("Not a valid ADT Snapshot file: %s",filename);
		SDDS_Terminate(&table);
		if(tempstring) SDDS_Free(tempstring); tempstring=NULL;
		return 0;
	    }
	    if(tempstring) SDDS_Free(tempstring); tempstring=NULL;
	  /* Get timestamp */
	    if(checkparameter(&table,"TimeStamp",filename,CHECK)) {
		tempstring=NULL;
		if(!SDDS_GetParameter(&table,"TimeStamp",&tempstring)) {
		    strcpy(time,"Unknown");
		    time[24]='\0';
		} else {
		    strncpy(time,tempstring,24);
		    time[24]='\0';
		}
		if(tempstring) SDDS_Free(tempstring); tempstring=NULL;
	    } else {
		strcpy(time,"Unknown");
		time[24]='\0';
	    }
	}     /* End first time */
      /* Get number of rows */
	nvals=SDDS_CountRowsOfInterest(&table);
	if(nvals != arrays[ia].nvals) {
	    xerrmsg("Wrong number of items [%d] for array %d in %s",
	      nvals,ia+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Get names */
	rawnames=(char **)SDDS_GetColumn(&table,"ControlName");
	if(!rawnames) {
	    xerrmsg("Could not get ControlName\'s for array %d in %s",
	      ia+1,filename);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Get string values */
	rawvalues=(char **)SDDS_GetColumn(&table,"ValueString");
	if(!rawvalues) {
	    xerrmsg("Could not get ValueString\'s for array %d in %s",
	      ia+1,filename);
	    freesddsnames(nvals,&rawnames);
	    SDDS_Terminate(&table);
	    return 0;
	}
      /* Allocate space */
	if(!arrays[ia].savevals[nsave]) {
	    arrays[ia].savevals[nsave]=
	      (double *)calloc(arrays[ia].nvals,sizeof(double));
	    if(arrays[ia].savevals[nsave] == NULL) {
		xerrmsg("Could not allocate space for arrray %d in %s",
		  ia+1,filename);
		return 0;
	    }
	}
	strcpy(savetime[nsave],time);
	strcpy(savefilename[nsave],filename);
      /* Check names for match and copy values */
	for(i=0; i < nvals; i++) {
	    if(strcmp(rawnames[i],arrays[ia].names[i]+prefixlength)) {
		xerrmsg("PV does not match: %s from %s",
		  rawnames[i],filename);
		freesddsnames(nvals,&rawnames);
		freesddsnames(nvals,&rawvalues);
		SDDS_Terminate(&table);
		return 0;
	    }
	    arrays[ia].savevals[nsave][i]=atof(rawvalues[i]);
	}
    }
  /* Free the SDDS files and unused data */
    freesddsnames(nvals,&rawnames);
    freesddsnames(nvals,&rawvalues);
    SDDS_Terminate(&table);
  /* Check for errors */
    if(SDDS_NumberOfErrors())
      SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors);
  /* Return */    
    return 1;
}
/**************************** writeplotfile *******************************/
int writeplotfile(char *filename, int nsave)
{
    struct timeval tv;
    time_t clock;
    char time[26];
    FILE *file;
    int i,ia;
    struct ARRAY *array;
    struct AREA *area;
    double *vals;
    
  /* Check if there are any arrays */
    if(!narrays) {
	xerrmsg("There are no PV's defined");
	return 0;
    }
  /* Get timestamp */    
    if(nsave < 0) {     /* Current data */
	gettimeofday(&tv,NULL);
	clock=tv.tv_sec;
	strcpy(time,ctime(&clock));
	time[24]='\0';
    } else if(nsave < NSAVE) {     /* Saved data */
	strcpy(time,savetime[nsave]);
      /* Check if there is saved data */
	for(ia=0; ia < narrays; ia++) {
	    if(!arrays[ia].savevals[nsave]) {
		xerrmsg("Data is not defined for set %d",nsave+1);
		return 0;
	    }
	}
    } else {
	xerrmsg("Invalid set number [%d]",nsave);
	return 0;
    }
  /* Check time */    
    if((int)strlen(time) > 25) {
	xerrmsg("Invalid time: %s",time);
	return 0;
    }
    time[25]='\0';
  /* Open file */
    file=fopen(filename,"w");
    if(file == NULL) {
	xerrmsg("Unable to open %s",filename);
	return 0;
    }
  /* Write header information */
    fprintf(file,"%s\n",SDDSID);
    fprintf(file,"&description text=\"ADT Plot from %s\" &end\n",
      pvfilename);
    fprintf(file,"&parameter name=TimeStamp fixed_value=\"%s\""
      " type=string &end\n",time);
    fprintf(file,"&parameter name=ADTNArrays type=short fixed_value=%d &end\n",narrays+1);
    fprintf(file,"&parameter name=ADTNAreas type=short fixed_value=%d &end\n",nareas+1);
    fprintf(file,"&parameter name=ADTHeading type=string &end\n");
    fprintf(file,"&parameter name=ADTUnits type=string &end\n");
    fprintf(file,"&parameter name=ADTScreenTitle type=string &end\n");
    fprintf(file,"&parameter name=ADTDisplayArea type=short &end\n");
    fprintf(file,"&column name=Index type=short &end\n");
    fprintf(file,"&column name=ControlName type=string &end\n");
    fprintf(file,"&column name=Value type=double &end\n");
    if(checkstatus) {
	fprintf(file,"&column name=StatusName type=string &end\n");
	fprintf(file,"&column name=Status type=string &end\n");
    }
    if(checkthreshold) {
	fprintf(file,"&column name=ThresholdName type=string &end\n");
	fprintf(file,"&column name=Threshold type=string &end\n");
    }
    fprintf(file,"&data mode=ascii no_row_counts=1 "
      "additional_header_lines=1 &end\n");
  /* Loop over PV's */
    for(ia=0; ia < narrays; ia++) {
	array=&arrays[ia];
	area=array->area;
	if(nsave < 0) vals=array->vals;
	else vals=array->savevals[nsave];
	fprintf(file,"\n");
      /* Parameters */
	fprintf(file,"%s\n",array->heading);
	fprintf(file,"%s\n",array->units);
	fprintf(file,"%s (%s)\n",array->heading,array->units);
	fprintf(file,"%d     !ADTDisplayArea\n",area->area+1);
        for(i=0; i < array->nvals; i++) {
          if(nsave < 0) vals=array->vals;
          else vals=array->savevals[nsave];
          fprintf(file,"%d %s % f",i+1,array->names[i],vals[i]);
          if(checkstatus) {
            fprintf(file," %s ",array->statusnames[i]);
            switch(array->statusvals[i]) {
            case 0:
              fprintf(file,"InValid");
              break;
            case 1:
              fprintf(file,"Valid");
              break;
            case 2:
              fprintf(file,"OldData");
              break;
            case UNUSED:
              fprintf(file,"-");
              break;
            case NOTCONN:
              fprintf(file,"NotConnected");
              break;
            }
          }
          if(checkthreshold) {
            fprintf(file," %s ",array->thresholdnames[i]);
            switch(array->thresholdvals[i]) {
            case 0:
              fprintf(file,"InValid");
              break;
            case 1:
              fprintf(file,"Valid");
              break;
            case 2:
              fprintf(file,"OldData");
              break;
            case UNUSED:
              fprintf(file,"-");
              break;
            case NOTCONN:
              fprintf(file,"NotConnected");
              break;
            }
          }
          fprintf(file,"\n");
        }

    }
  /* Check for errors and return */
    fclose(file);
    /* if(ferror(file)) {
	xerrmsg("Error writing %s",filename);
	return 0;
        } */
    return 1;
}
/**************************** writesnap ***********************************/
int writesnap(char *filename, int nsave)
{
    struct timeval tv;
    time_t clock;
    char time[26];
    FILE *file;
    struct ARRAY *array;
    double *vals;
    int i,ia;
    
  /* Check if there are any arrays */
    if(!narrays) {
	xerrmsg("There are no PV's defined");
	return 0;
    }
  /* Get timestamp */    
    if(nsave < 0) {     /* Current data */
	gettimeofday(&tv,NULL);
	clock=tv.tv_sec;
	strcpy(time,ctime(&clock));
	time[24]='\0';
    } else if(nsave < NSAVE) {     /* Saved data */
	strcpy(time,savetime[nsave]);
      /* Check if there is saved data */
	for(ia=0; ia < narrays; ia++) {
	    if(!arrays[ia].savevals[nsave]) {
		xerrmsg("Data is not defined for set %d",nsave+1);
		return 0;
	    }
	}
    } else {
	xerrmsg("Invalid set number [%d]",nsave);
	return 0;
    }
  /* Check time */    
    if((int)strlen(time) > 25) {
	xerrmsg("Invalid time: %s",time);
	return 0;
    }
    time[25]='\0';
  /* Open file */
    file=fopen(filename,"w");
    if(file == NULL) {
	xerrmsg("Unable to open %s",filename);
	return 0;
    }
  /* Write header information */
    fprintf(file,"%s\n",SDDSID);
    fprintf(file,"&description text=\"ADT Snapshot from %s\""
      " contents=\"BURT\" &end\n",
      pvfilename);
    fprintf(file,"&parameter name=ADTFileType fixed_value=\"%s\" "
      "type=string &end\n",SNAPID);
    fprintf(file,"&parameter name=TimeStamp fixed_value=\"%s\""
      " type=string &end\n",time);
    fprintf(file,"&parameter name=SnapType fixed_value=\"Absolute\""
      " type=string &end\n");
    fprintf(file,"&parameter name=Heading"
      " type=string &end\n");
    fprintf(file,"&column name=ControlName type=string &end\n");
    fprintf(file,"&column name=ControlType type=string &end\n");
    fprintf(file,"&column name=Lineage type=string &end\n");
    fprintf(file,"&column name=Count type=long &end\n");
    fprintf(file,"&column name=ValueString type=string &end\n");
    if(checkstatus) {
	fprintf(file,"&column name=StatusName type=string &end\n");
	fprintf(file,"&column name=Status type=string &end\n");
    }
    if(checkthreshold) {
	fprintf(file,"&column name=ThresholdName type=string &end\n");
	fprintf(file,"&column name=Threshold type=string &end\n");
    }
    fprintf(file,
      "&data mode=ascii no_row_counts=1 additional_header_lines=1 &end\n");
  /* Loop over PV's */
    for(ia=0; ia < narrays; ia++) {
	array=&arrays[ia];
	if(nsave < 0) vals=array->vals;
	else vals=array->savevals[nsave];
	fprintf(file,"\n");
	fprintf(file,"%s (%s)\n",array->heading,array->units);
      /* Parameters */
        for(i=0; i < array->nvals; i++) {
          if(nsave < 0) vals=array->vals;
          else vals=array->savevals[nsave];
          fprintf(file,"%s pv - 1 %f",array->names[i]+prefixlength,vals[i]);
          if(checkstatus) {
            fprintf(file," %s ",array->statusnames[i]);
            switch(array->statusvals[i]) {
            case 0:
              fprintf(file,"InValid");
              break;
            case 1:
              fprintf(file,"Valid");
              break;
            case 2:
              fprintf(file,"OldData");
              break;
            case UNUSED:
              fprintf(file,"-");
              break;
            case NOTCONN:
              fprintf(file,"NotConnected");
              break;
            }
          }
          if(checkthreshold) {
            fprintf(file," %s ",array->thresholdnames[i]);
            switch(array->thresholdvals[i]) {
            case 0:
              fprintf(file,"InValid");
              break;
            case 1:
              fprintf(file,"Valid");
              break;
            case 2:
              fprintf(file,"OldData");
              break;
            case UNUSED:
              fprintf(file,"-");
              break;
            case NOTCONN:
              fprintf(file,"NotConnected");
              break;
            }
          }
          fprintf(file,"\n");
        }
    }
  /* Check for errors and return */
    fclose(file);
    /* if(ferror(file)) {
	xerrmsg("Error writing %s",filename);
	return 0;
        } */
    return 1;
}
