/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* pix.c *** Routines to scale graph areas */

#include "adt.h"

/**************************** pixs ****************************************/
double pixs(int p)
{
    return (aptos*p+bptos);
}
/**************************** pixx ****************************************/
double pixx(int p)
{
    return (aptox*p+bptox);
}
/**************************** spix ****************************************/
int spix(double z)
{
    return (int)(astop*z+bstop+.5);
}
/**************************** xpix ****************************************/
int xpix(double z)
{
    return (int)(axtop*z+bxtop+.5);
}
