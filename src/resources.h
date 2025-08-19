/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* fallback.h *** Fallback resources for ADT */

#ifndef INCfallbackh
#define INCfallbackh 1

static String resources[] = {
/* ControlApp */
    "adt*XmTextField*background:           White",
    "adt*XmTextField*foreground:           Black",
    "adt*XmTextField.columns:              15",
    "adt*allowShellResize:                 True",
    "adt*arrow*background:                 #59597e7ee1e1",
    "adt*arrow*foreground:                 White",
    "adt*background:                       Gray80",
    "adt*borderWidth:                      0",
    "adt*fileSelection*Text*cursorPositionVisible: TRUE",
    "adt*fontList:                         fixed",
    "adt*foreground:                       Black",
    "adt*graphArea.background:             White",
    "adt*graphArea.borderWidth:            1",
    "adt*graphArea.bottomOffset:           20",
    "adt*graphArea.leftOffset:             50",
    "adt*graphArea.rightOffset:            50",
    "adt*graphControl.bottomOffset:        2",
    "adt*graphControl.leftOffset:          2",
    "adt*graphControl.marginHeight:        7",
    "adt*graphControl.marginWidth:         7",
    "adt*graphControl.rightOffset:         2",
    "adt*graphControl.topOffset:           2",
    "adt*graphDraw.background:             White",
    "adt*graphForm.bottomOffset:           10",
    "adt*graphScroll.borderWidth:          1",
    "adt*graphScroll.bottomOffset:         15",
    "adt*graphScroll.leftOffset:           15",
    "adt*graphScroll.rightOffset:          15",
    "adt*highlightThickness:               2",
    "adt*informationMessage*background:    Gray70",
    "adt*keyboardFocusPolicy:              explicit",
    "adt*mainForm.bottomOffset:            0",
    "adt*mainMenu*background:              #59597e7ee1e1",
    "adt*mainMenu*foreground:              White",
    "adt*mainMenu*marginHeight:            2",
#if 1
  /* As it was originally */
    "adt*mainWindow*marginHeight:          2",
    "adt*mainWindow*marginWidth:           10",
    "adt*mainWindow.bottomOffset:          2",
    "adt*mainWindow.leftOffset:            2",
    "adt*mainWindow.rightOffset:           2",
    "adt*mainWindow.topOffset:             2",
#else
  /* Fixes Exceed problem */
    "adt*mainForm*marginHeight:            2",
    "adt*mainForm*marginWidth:             10",
    "adt*mainForm.bottomOffset:            2",
    "adt*mainForm.leftOffset:              2",
    "adt*mainForm.rightOffset:             2",
    "adt*mainForm.topOffset:               2",
#endif    
    "adt*optPopup*background:              Gray70",
    "adt*optPopup*marginHeight:            2",
    "adt*optPopup*marginWidth:             2",
    "adt*shadowThickness:                  2",
    "adt*traversalOn:                      True",
    "adt*warningMessage*background:        Red",
    "adt*warningMessage*foreground:        White",
/* ADT Specific */
    "adt*controlMenu*background:           #59597e7ee1e1",
    "adt*controlMenu*foreground:           White",
    "adt*controlMenu*marginHeight:         1",
    "adt*elementPopup*background:          Gray70",
    "adt*elementPopup*marginHeight:        2",
    "adt*elementPopup*marginWidth:         2",
    "adt*graphArea.height:                 150",
    "adt*graphArea.width:                  760",
    "adt*graphTitleLabel*marginHeight:     0",
    "adt*graphTitleLabel.bottomOffset:     0",
    "adt*graphTitleLabel.topOffset:        0",
    "adt*graphZoomArea.background:         White",
    "adt*graphZoomArea.borderWidth:        1",
    "adt*graphZoomArea.bottomOffset:       20",
    "adt*graphZoomArea.height:             200",
    "adt*graphZoomArea.leftOffset:         50",
    "adt*graphZoomArea.rightOffset:        50",
    "adt*graphZoomArea.width:              760",
    "adt*zoomAreaForm.bottomOffset:        20",
    "adt.display1Color:                    Magenta",
    "adt.display2Color:                    Green4",
    "adt.title:                            Array Display Tool",
/* Must end in NULL */
    NULL,
};

#endif /* INCfallbackh */
