/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* XINTOLAT.C *** Program to convert .xin files from Xorbit to */
/*   .lat files for ADT */

#define LATID "ADTLATTICE"
#define SDDSID "SDDS1"
#define NAMESIZE 8
#define NFILES 2
#define HEADERLINES 2

#define sign(x) (((x) < 0)?-1:1)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

/* Function prototypes */

int main(int argc, char **argv);
static void errmsg(char *s);
static char *getstr(char *s, int n, FILE *stream);
static void usage(void);

/* Global variables */

char string[BUFSIZ];

/**************************** main ****************************************/
int main(int argc, char **argv)
{
    FILE *file[NFILES];
    char filename[NFILES][PATH_MAX];
    double s,a,b,slen,stotal;
    int i,unit,nsect,nfiles,ring;
    size_t len;
    int nelements,nsymbols,nlines,namesize;
    char name[PATH_MAX],fulltype[PATH_MAX],input[11];
    static char iotype[NFILES][2]={"r","w",};
    double pi=acos(-1.);
    
/* Parse command line */
    nfiles=0;
    for (i=1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
	    case 'h':
	    case '?':
		usage();
		exit(0);
		break;
	    default:
		sprintf(string,"Invalid option %s",argv[i]);
		errmsg(string);
		exit(8);
	    }
	}
	else {   /* Assume any other arguments are filenames */
	    if(nfiles >= 2) {
		errmsg("Too many arguments");
		usage();
		exit(8);
	    }
	    strcpy(filename[nfiles],argv[i]);
	    file[nfiles]=fopen(filename[nfiles],iotype[nfiles]);
	    if(file[nfiles] == NULL) {
		sprintf(string,"Unable to open %s",filename[nfiles]);
		errmsg(string);
		exit(8);
	    }
	    nfiles++;
	}
    }
/* Prompt for files that were not specified */
    for(i=nfiles; i < NFILES; i++) {
	if( nfiles == 0) printf("Enter templet filename:\n");
	else if( nfiles == 1) printf("Enter list filename:\n");
	else if( nfiles == 2) printf("Enter output filename:\n");
	getstr(filename[i],PATH_MAX,stdin);
	file[i]=fopen(filename[i],iotype[i]);
	if(file[i] == NULL) {
	    sprintf(string,"Unable to open %s",filename[i]);
	    errmsg(string);
	    exit(8);
	}
	nfiles++;
    }
/* Read header lines */
    for(i=0; i < HEADERLINES; i++) {
	if(!fgets(string,BUFSIZ,file[0])) {
	    sprintf(string,"Reached EOF in %s without header",
	      filename[0]);
	    errmsg(string);
	    exit(8);
	}
    }
/* Prompt for header information */
    printf("Enter number of sectors: \n");
    getstr(string,BUFSIZ,stdin);
    nsect=atoi(string);
    printf("Enter total length:\n");
    getstr(string,BUFSIZ,stdin);
    stotal=atof(string);
    printf("Enter 1 if a ring, 0 if a beam line:\n");
    getstr(string,BUFSIZ,stdin);
    ring=atoi(string);
    if(ring) ring=1;
    printf("Enter the NAMESIZE (4-8):\n");
    getstr(string,BUFSIZ,stdin);
    namesize=atoi(string);
    if(namesize < 4 || namesize > 8) {
	errmsg("Invalid NAMESIZE");
	exit(8);
    }
/* Write header */    
    fprintf(file[1],"%s\n",SDDSID);
    fprintf(file[1],"&description text=\"ADT Lattice File\" &end\n");
    fprintf(file[1],"&parameter name=ADTFileType fixed_value="
      "\"%s\" type=string &end\n",LATID);
    fprintf(file[1],"&parameter name=Nsectors fixed_value=%d type=short &end\n",
      nsect);
    fprintf(file[1],"&parameter name=Stotal fixed_value=%f type=double &end\n",
      stotal);
    fprintf(file[1],"&parameter name=Ring fixed_value=%d type=short &end\n",
      ring);
    fprintf(file[1],"&column name=S type=double &end\n");
    fprintf(file[1],"&column name=Length type=double &end\n");
    fprintf(file[1],"&column name=SymbolHeight type=short &end\n");
    fprintf(file[1],"&column name=Name type=string &end\n");
    fprintf(file[1],"&data mode=ascii no_row_counts=1 &end\n");
/* Loop over lines in the file */
    s=0.;
    nelements=0;
    nsymbols=0;
    nlines=HEADERLINES;
    while(1) {
	nlines++;
	if(!fgets(string,sizeof(string),file[0])) {
	    sprintf(string,"Error reading file at line %d\nDid not reach $",
	      nlines);
	    break;
	}
	len=strlen(string);
	if(!len) {
	    sprintf(string,"Error parsing line #%d from %s (Blank)",
	      nlines,filename[0]);
	    errmsg(string);
	    exit(8);
	}
      /* name */
	strncpy(name,string,namesize);
	name[namesize-1]='\0';
	if(strchr(name,'&')) continue;     /* Skip extra line for bends */
	if(strchr(name,'$')) break;        /* End of data */
      /* fulltype */
	if(len > namesize+1) {
	    strncpy(fulltype,string+namesize+1,2);
	    fulltype[2]='\0';
	}
	else {
	    sprintf(string,"Error parsing line #%d from %s (No type given)",
	      nlines,filename[0]);
	    errmsg(string);
	    exit(8);
	}
      /* a */
	if(len > namesize+4) {
	    strncpy(input,string+namesize+4,10);
	    input[10]='\0';
	    a=atof(input);
	}
	else {
	    a=0;
	}
      /* b */
	if(len > namesize+14) {
	    strncpy(input,string+namesize+14,10);
	    input[10]='\0';
	    b=atof(input);
	}
	else {
	    b=0.;
	}
      /* slen */
	if(len > namesize+24) {
	    strncpy(input,string+namesize+24,10);
	    input[10]='\0';
	    slen=atof(input);
	}
	else {
	    slen=0;
	}
      /* Handle bend with no slen */
	if(fulltype[0] == 'b') {     /* Calculate bend length */
	    if(slen == 0.) slen=fabs(pi*a*b);
	}
	nelements++;
	s+=slen;
	switch(fulltype[0]) {
	case 'b':
	    unit=1;
	    break;
	case 'm':
	    unit=0;
	    break;
	case 'n':
	    unit=atoi(fulltype+1)*sign(a+b);
	    break;
	case 'q':
	    unit=2*sign(a);
	    break;
	case 't':
	    unit=2*sign(a);
	    break;
	default:
	    continue;
	}
	fprintf(file[1],"% #10.6f% #10.6f %2d %s\n",s-slen,slen,unit,name);
	nsymbols++;
    }
    printf("\nThere were %d elements, resulting in %d symbols\n",
      nelements,nsymbols);
    printf("Total length specified is %f\n",stotal);
    printf("Total length calculated is %f\n",s);
    printf("Input file is: %s\n",filename[0]);
    printf("Output file is: %s\n",filename[1]);
    printf("\n");
    exit(1);
    return 0;
}
/**************************** errmsg **************************************/
static void errmsg(char *s)
{
    fprintf(stderr,"\nXintolat: %s\n",s);
}
/**************************** getstr **************************************/
static char *getstr(char *s, int n, FILE *stream)
{
    size_t i,len;
    char *retVal=NULL;
    
    if(!s) return NULL;
    retVal=fgets(s,n,stream);
    if(!retVal) {
	*s='\0';
	return NULL;
    }
  /* Strip whitespace from end */
    len=strlen(s);
    for(i=len; i >= 0; i--) {
	if(s[i] == '\n' || s[i] == '\r' || s[i] == '\t') {
	    s[i]='\0';
	} else {
	    break;
	}
    }
    return retVal;
}
/**************************** usage ***************************************/
static void usage(void)
{
    printf("Usage is: xintolat [file1.xin] [file2.lat]\n");
    printf("(The program will prompt for unsupplied filenames.)\n");
}
