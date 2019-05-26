/******************************************************************************
* GuiIritDllExample.cpp - a simple example of a Dll extension of GuIrit.      *
*******************************************************************************
* (C) Gershon Elber, Technion, Israel Institute of Technology                 *
*******************************************************************************
* Written by Gershon Elber, Oct 2007.					      *
******************************************************************************/

#include <ctype.h>
#include <stdlib.h>

#ifdef __WINNT__        /* All these includes just for making some beeps... */
#   include <windows.h>
#elif defined(LINUX386)
#   include <unistd.h>
#   include <sys/types.h>
#   include <fcntl.h>
#   include <sys/ioctl.h>
#   include <linux/kd.h>
#elif defined(__MACOSX__)
#endif /* __WINNT__ || LINUX386 || __MACOSX__ */

#include "IrtDspBasicDefs.h"
#include "IrtMdlr.h"
#include "IrtMdlrFunc.h"
#include "IrtMdlrDll.h"
#include "GuIritDllExtensions.h"

/* Include and define function icons. */
#include "Icons/IconMenuExample.xpm"
#include "Icons/IconExample.xpm"

#define GUIRIT_DLL_EXAMPLE_NUM_OF_TYPE_CHOICES	3

struct IrtMdlrExampleGenericEventStruct
{
    public:
	double GenericReal;
	int GenericInt;
	char GenericStr[10];
	IrtMdlrFuncInfoClass *GenericFI;
};

IRT_DSP_STATIC_DATA IPObjectStruct
    *PObjSlicePlane = NULL;

static void IrtMdlrBeepExample(IrtMdlrFuncInfoClass *FI);
static IrtRType IrtMdlrExampleTesting(IrtMdlrFuncInfoClass *FI,
				      IrtRType Pitch);
static void IrtMdlrObjectWasTransformUpdate(void *UpdateInfo,
					    IrtDspPanelUpdateStateType State);
static bool IrtMdlrInterpInput(const char *InterpInput);
static int IrtMdlrExampleMouseCallBack(IrtMdlrMouseEventStruct *MouseEvent);
static int IrtMdlrExampleGenericEventCallBack(IrtMdlrGenericEventStruct
						         *GuiritGenericEvent);
static bool AnimCallBackPrintTime(IrtDspGrAnimationBasicInfoClass
							      *AnimationInfo);
static void IrtMdlrMenuCheckboxExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuIntegerExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuGridIntegerExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuSelectionExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuRealExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuGridRealExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuVectorExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuRadioboxExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuTextExample(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrMenuDisableTextExample(IrtMdlrFuncInfoClass *FI);

/* Parameters are saved here - we provide 'reasonable' initial values and   */
/* also store last result as new default values for the next time.          */
IRT_DSP_STATIC_DATA IrtRType
    IrtMdlrBeepPitch = 1000.0,
    IrtMdlrBeepDuration = 100.0,
    IrtMdlrMenuRealValue = 1.0;
IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass
    *GlblExampleFI = NULL;
IRT_DSP_STATIC_DATA CagdBType
    IrtMdlrMenuCheckBoxState = TRUE;
IRT_DSP_STATIC_DATA int
    IrtMdlrMenuIntegerValue = 2,
    IrtMdlrMenuRadioBoxSelection = 1;
IRT_DSP_STATIC_DATA const char * const 
    IrtMdlrMenuSelectionItems[] = {"Test item 0", "Test item 1", "Test item 2", NULL };
IRT_DSP_STATIC_DATA IrtVecType
    IrtMdlrMenuVectorValue = { 0.0, 0.0, 0.0 };
IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass
    *IrtMdlrInterpFI = NULL;

IRT_DSP_STATIC_DATA IrtMdlrFuncTableStruct ExampleFunctionTable[] = {  
    { 0, IconExample, "IRT_MDLR_EXAMPLE_BEEP", "Beep", "Make some sound",
	"Make some sound, controlling the pitch and the duration,\nand return the pitch as a new (numeric) object.\n\nAlso demonstrates mouse events - try Ctrl-left-mouse-click-and-drag\nand Shift-left-mouse-click-and-drag in the graphics window,\nafter an 'Apply' was invoked.\n\nAlso demonstrates some special features - if pitch value equal:\n1001 or 1002 - Generates and sends generic IrtMdlrGenericEventStruct\n\tevent\n1010 - demonstrate a confirmation popup widget use\n1011 - demonstrate an error popup widget use\n1012 - demonstrate a string input popup widget use\n1013 - demonstrate a filename input popup widget use\n1014 - demonstrate a directory popup widget use\n1015 - demonstrate a real value input popup widget use\n1016 - demonstrate a point value input popup widget use\n1017 - demonstrate a color input popup widget use\n1018 - demonstrate a symbol selection popup\n1019 - demonstrate a color map input popup widget\n1020 - demonstrate a system's directories query function call\n1021 - demonstrate the use of a transformation controller over some object\n1030 - demonstrate the application of a texture image to a surface\n1050 - emulates execution of interpreter commands\n1100 and 1101 - demonstrate a request to close this panel directly\n\twith cancel (1100) or ok (1101)\n1200, 1201, and 1202 - highlight an objected called \"Object\"\n\twith Highlight1/2/3 respectively.\n\nIf Duration equals 88, \"intermediate update\" and \"view change\" events are\nreported to the status screen, after an 'Apply' was invoked."
	"\\'<BR><BR> <LEFT><IMG SRC=\"GuIritSplash.png\" WIDTH=100 HEIGHT=70 ALIGN=middle ></LEFT>\\'",
	IrtMdlrBeepExample, NULL, IRT_MDLR_PARAM_POINT_SIZE_CONTROL | IRT_MDLR_PARAM_VIEW_CHANGES_UDPATE | IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_DFLT_ON, IRT_MDLR_NUMERIC_EXPR, 3, IRT_MDLR_PARAM_EXACT,
        { IRT_MDLR_STRING_EXPR, IRT_MDLR_NUMERIC_EXPR, IRT_MDLR_NUMERIC_EXPR },
        { "BEEP", &IrtMdlrBeepPitch, &IrtMdlrBeepDuration },
        { "Name", "Pitch", "Duration" },
        { "Resulting object's name", "The frequency of the sound (Hz)", "The duration to make the sound, in millisecs." } },

    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_CHECKBOX", "Checkbox", "Test checkbox",
	"Example of checkbox widget supporting function.",
        IrtMdlrMenuCheckboxExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_CHECK, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
        { },
        { },
        { },
        { } },

    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_INTEGER", "Integer", "Test integer",
	"Example of integer widget supporting function.",
        IrtMdlrMenuIntegerExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_INTEGER, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
	{ },
        { },
        { },
        { } },
    
    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_GRID_INTEGER", "Grid Integer", "Test grid integer",
	"Example of integer widget supporting function.",
        IrtMdlrMenuGridIntegerExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_INTEGER, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
	{ },
        { },
        { },
        { } },
     
    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_SELECTION", "Selection", "Test selection",
	"Example of selection widget supporting function.",
        IrtMdlrMenuSelectionExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_SELECTION, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
        { },
        { },
        { },
        { } },

    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_REAL", "Real", "Test real",
	"Example of real widget supporting function.",
        IrtMdlrMenuRealExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_REAL, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
        { },
        { },
        { },
        { } },
    
    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_GRID_REAL", "Grid Real", "Test grid real",
	"Example of real widget supporting function.",
        IrtMdlrMenuGridRealExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_REAL, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
        { },
        { },
        { },
        { } },
    
    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_VECTOR", "Vector", "Test vector",
	"Example of vector widget supporting function.",
        IrtMdlrMenuVectorExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_VECTOR, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
        { },
        { },
        { },
        { } },

    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_RADIOBOX", "Radiobox", "Test menu radiobox",
	"Example of radiobox widget supporting function.",
        IrtMdlrMenuRadioboxExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_RADIO, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
        { },
        { },
        { },
        { } },

    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_TEXT", "Text", "Test menu text",
	"Example of text widget supporting function.",
        IrtMdlrMenuTextExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_TEXT, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
        { },
        { },
        { },
        { } },
    
    { 0, NULL, "IRT_MDLR_EXAMPLE_MENU_DISABLE_TEXT", "Disable Text", "Test disable text",
	"Example of disable text widget supporting function.",
        IrtMdlrMenuDisableTextExample, NULL, (int) IRT_DSP_MENU_ITEM_TYPE_TEXT | IRT_DSP_MENU_DISABLE_PARAMETER, IRT_MDLR_NO_EXPR, 0, IRT_MDLR_PARAM_DONT_CARE,
        { },
        { },
        { },
        { } },
};

IRT_DSP_STATIC_DATA const int
    EXAMPLE_FUNC_TABLE_SIZE = sizeof(ExampleFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

/*****************************************************************************
* DESCRIPTION:                                                               M
*   Registration function of GuIrit dll extension.  Always define as         M
* 'extern "C"' to prevent from name mangling.                                M
*                                                                            *
* PARAMETERS:                                                                M
*   None                                                                     M
*                                                                            *
* RETURN VALUE:                                                              M
*   extern "C" bool:     True if successful, false otherwise.                M
*                                                                            *
* KEYWORDS:                                                                  M
*   _IrtMdlrDllRegister                                                      M
*****************************************************************************/
extern "C" bool _IrtMdlrDllRegister(void)
{
    GuIritMdlrDllRegister(ExampleFunctionTable,
			  EXAMPLE_FUNC_TABLE_SIZE,
			  "Example",
			  IconMenuExample);

    /*   You can also have the call back function activated on load.       */
    /*   In this case, the function will be called every time animation is */
    /* executed, on every frame.  Make sure AnimCallBackPrintTime has a    */
    /* valid FI.							   */
    /* GuIritMdlrDllSetAnimationUpdateFunc(NULL, AnimCallBackPrintTime, true); */

    return true;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   An example function - make some sounds.	                             *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrBeepExample(IrtMdlrFuncInfoClass *FI)
{
    char Name[IRIT_LINE_LEN_LONG];
    IPObjectStruct *ResultObject;

    if (FI -> InvocationNumber == 0 && GlblExampleFI == NULL) {
        GuIritMdlrDllPushMouseEventFunc(
		    FI, IrtMdlrExampleMouseCallBack,
		    (IrtDspMouseEventType) (IRT_DSP_MOUSE_EVENT_LEFT),
		    (IrtDspKeyModifierType) (IRT_DSP_KEY_MODIFIER_CTRL_DOWN |
					     IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
		    FI);
        GuIritMdlrDllPushGenericEventFunc(FI,
					  IrtMdlrExampleGenericEventCallBack,
					  false, FI);

	/* Sets the valid domains of the parameters:                       */
        /* This will not work unless the intermediate update flag is set   */
        /* (IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_DFLT_ON) which you probably */
        /* do not want here...						   */
	GuIritMdlrDllSetRealInputDomain(FI, 20.0, 20000.0, 1);
	GuIritMdlrDllSetRealInputDomain(FI, 1.0, IRIT_INFNTY, 2);

	/* Call back function invoked on every frame of an animation.      */
	/* One can also invoke this function on registration time.         */
	/* See _IrtMdlrDllRegister above.				   */
	GlblExampleFI = FI;
	GuIritMdlrDllSetAnimationUpdateFunc(NULL, AnimCallBackPrintTime, true);
    }

    GuIritMdlrDllGetInputParameter(FI, 0, &Name);
    GuIritMdlrDllGetInputParameter(FI, 1, &IrtMdlrBeepPitch);
    GuIritMdlrDllGetInputParameter(FI, 2, &IrtMdlrBeepDuration);

    /* While we do not care for intermediate updates, we do receive them   */
    /* here and ignore then so we can also get view changes updates, every */
    /* time the view is changing.					   */
    switch (FI -> CnstrctState) {
	case IRT_MDLR_CNSTRCT_STATE_INTERMEDIATE:
	    if (IrtMdlrBeepDuration == 88)
		GuIritMdlrDllStatusPrintf(FI, 0, "Intermediate updates trapped in Beep example.");
	    return;
	case IRT_MDLR_CNSTRCT_STATE_VIEW_CHANGED:
	    if (IrtMdlrBeepDuration == 88)
	        GuIritMdlrDllStatusPrintf(FI, 0, "View change trapped in Beep example.");
	    return;
        case IRT_MDLR_CNSTRCT_STATE_CANCEL:
	    GuIritMdlrDllSetAnimationUpdateFunc(FI, AnimCallBackPrintTime,
						false);
	    GlblExampleFI = NULL;
	    return;
	default:
	    break;
    }

    /* Make the sound! */
#   ifdef __WINNT__
	Beep((int) IrtMdlrBeepPitch, (int) IrtMdlrBeepDuration);
#   elif defined(LINUX386)
        {
            int Fd = open("/dev/tty0", O_RDONLY);

	    if (Fd != -1) {
	        int Arg = ((int) (IrtMdlrBeepPitch) << 16) +
				    (1193180 / (int) (IrtMdlrBeepDuration));

	        ioctl(Fd, KDMKTONE, Arg);
		close(Fd);
	    }
	    else
	        printf("\a");                /* At least do some sound... */
        }
#   elif defined(__MACOSX__)

#   else
#       error "Add here a making-sound-function"
#   endif /* __WINNT__ || LINUX386 || __MACOSX__ */

    if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_APPLY ||
	FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK) {
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
			    "Pitch = %f, Duration = %f.\n",
			    IrtMdlrBeepPitch, IrtMdlrBeepDuration);

	ResultObject = IPGenNUMValObject(IrtMdlrBeepPitch);

	GuIritMdlrDllSetObjectName(FI, ResultObject, Name);

        GuIritMdlrDllInsertModelingFuncObj(FI, ResultObject);
    }

    IrtMdlrBeepPitch = IrtMdlrExampleTesting(FI, IrtMdlrBeepPitch);
    if (IrtMdlrBeepPitch == 998.0)
        /* 998 if need to quit immediately (closed panel), 999 if fine. */
	return;

    if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK) {
	GuIritMdlrDllSetAnimationUpdateFunc(FI, AnimCallBackPrintTime, false);
	GlblExampleFI = NULL;
    }
    else
	GuIritMdlrDllSetInputParameter(FI, 1, &IrtMdlrBeepPitch);
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   For different pitch values - test different features of the environment. *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Pitch:   To examine which test should be done.                           *
*                                                                            *
* RETURN VALUE: 999 if fine, 998 if need to quit immediately (closed panel). *
*   void                                                                     *
*****************************************************************************/
static IrtRType IrtMdlrExampleTesting(IrtMdlrFuncInfoClass *FI,
				      IrtRType Pitch)
{
    bool b;
    char *Str,
        HeadLine[IRIT_LINE_LEN] = "Some Input String";
    int p = (int) Pitch;

    switch (p) {
        case 1001:
        case 1002:
	{
	    /* Exercise the generic event option - push a generic event     */
	    /* with the pitch.					            */
	    IrtMdlrExampleGenericEventStruct GenericEvent;

	    GenericEvent.GenericInt = p;
	    GenericEvent.GenericReal = 3.1415927;
	    strcpy(GenericEvent.GenericStr, "Example");
	    GenericEvent.GenericFI = FI;

	    IrtMdlrGenericEventStruct 
	        *GuiritGenericEvent = new
		     IrtMdlrGenericEventStruct(
			       IRT_MDLR_GENERIC_EVENT_TYPE1, &GenericEvent,
			       sizeof(IrtMdlrExampleGenericEventStruct));

	    GuIritMdlrDllPushGenericEvent(FI, GuiritGenericEvent);
	    break;
	}
        case 1010:
	{
	    /* Exercise the async. confirm input pop up widget option. */
	    b = GuIritMdlrDllGetAsyncInputConfirm(FI, "Confirm Example",
						  "Testing Confirmation?");
	    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				"Retval of confirm example is %s\n",
				b ? "True" : "False");
	    break;
	}
        case 1011:
	{
	    /* Exercise the async. error pop up widget option. */
	    b = GuIritMdlrDllGetAsyncInputErrorMessage(FI,
						     "Error message Example");
	    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				"Retval of error message example is %s\n",
				b ? "True" : "False");
	    break;
	}
        case 1012:
	{
	    /* Exercise the async. string input pop up widget option. */
	    Str = HeadLine;
	    if (GuIritMdlrDllGetAsyncInputString(FI,
						 "Enter String Example",
						 &Str)) {
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched string is \"%s\"\n", Str);
		delete Str;
	    }
	    else
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched string canceled\n");
	    break;
	}
        case 1013:
	{
	    /* Exercise the async. filename input pop up widget option. */
	    if (GuIritMdlrDllGetAsyncInputFileName(FI,
						   "Select File Name Example",
						   "Irit text files (*.itd)|*.itd|IGES files (*.igs)|*.igs",
						   &Str)) {
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched file name is \"%s\"\n", Str);
		delete Str;
	    }
	    else
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched file name canceled\n");
	    break;
	}
        case 1014:
	{
	    /* Exercise the async. directory input pop up widget option. */
	    if (GuIritMdlrDllGetAsyncInputDirectory(FI,
						    "Select Directory Example",
						    &Str)) {
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched directory is \"%s\"\n", Str);
		delete Str;
	    }
	    else
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched directory canceled\n");
	    break;
	}
        case 1015:
	{
	    IrtRType
	        R = 3.1415;

	    /* Exercise the async. real value input pop up widget option. */
	    if (GuIritMdlrDllGetAsyncInputReal(FI,
					       "Select Real Value Example",
					       "Value:",
					       &R))
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched Real Value is %f\n", R);
	    else
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched Real Value canceled\n");
	    break;
	}
        case 1016:
	{
	    IRT_DSP_STATIC_DATA IrtPtType
	        Pt = { 0.3, 0.5, 0.7 };

	    /* Exercise the async. point input pop up widget option. */
	    if (GuIritMdlrDllGetAsyncInputPoint(FI,
						"Select Point Example",
						"Point:",
						Pt))
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched Point is (%f  %f  %f)\n",
				    Pt[0], Pt[1], Pt[2]);
	    else
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched Point canceled\n");
	    break;
	}
        case 1017:
	{
	    /* Exercise the async. color selection pop up widget option: */
	    unsigned char Red, Green, Blue;

	    if (GuIritMdlrDllGetAsyncInputColor(FI, &Red, &Green, &Blue)) {
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Color selection: %u, %u, %u.\n",
				    Red, Green, Blue);
		{
		    IPObjectStruct
		        *PObj = GuIritMdlrDllGetObjByName(FI, "ColorTest",
						     IRT_DSP_TYPE_OBJ_GEOMETRY,
						     true);
		    if (PObj == NULL) /* Expect a model named "ColorTest"... */
		        break;

		    AttrIDSetObjectRGBColor(PObj, Red, Green, Blue);
		    IrtDspOGLObjPropsClass
		        *OGLProps = GuIritMdlrDllGetObjectOGLProps(FI, PObj);

		    if (OGLProps != NULL) {
		        IrtDspRGBAClrClass
			    Clr = IrtDspRGBAClrClass(Red, Green, Blue);

			OGLProps -> FrontFaceColor.SetValue(Clr);
			OGLProps -> BackFaceColor.SetValue(Clr);
			OGLProps -> BDspListNeedsUpdate = true;
			GuIritMdlrDllRedrawScreen(FI);
		    }
		}
	    }
	    break;
	}
        case 1018:
	{
	    int SymbolCode = 0;

	    /* Exercise the async. symbol input pop up widget option. */
	    if (GuIritMdlrDllGetAsyncInputSymbol(FI, &SymbolCode, &Str)) {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched symbol unicode is %d, font \"%s\".\n", 
				    SymbolCode, Str);
		delete Str;
	    }
	    else
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				    "Fetched symbol canceled.\n");
	    break;
	}
	case 1019:
	{
	    /* Exercise the async. color map selection pop up widget option: */
	    IrtDspColorsMapClass *ColMap;
	    IPObjectStruct *PObj;
	    IrtVecType
		BoxCorner = { 0.0, 0.0, 0.0 };
	    if ((ColMap = GuIritMdlrDllGetAsyncInputMapColor(FI)) != NULL &&
		(PObj = PrimGenBOXObject(BoxCorner , 1, 1, 1)) != NULL) {
	    
    		IP_SET_OBJ_NAME2(PObj, "MapColorTest");

		/* Add PObj to hierarchy. */
		GuIritMdlrDllInsertModelingFuncObj(FI, PObj);
		
		IrtDspRGBAClrClass
		    *Color = GuIritMdlrDllGetMapColorRGBFromFloat(FI, ColMap, 0.5);

		if (Color == NULL)
		    break;
		AttrIDSetObjectRGBColor(PObj, Color -> r, Color -> g, Color -> b);

		IrtDspOGLObjPropsClass
		    *OGLProps = GuIritMdlrDllGetObjectOGLProps(FI, PObj);

		if (OGLProps != NULL) {
		    OGLProps -> FrontFaceColor.SetValue(*Color);
		    OGLProps -> BackFaceColor.SetValue(*Color);
		    OGLProps -> BDspListNeedsUpdate = true;
		    GuIritMdlrDllRedrawScreen(FI);
		}
		delete ColMap;
	    }
	    break;
	}
	case 1020:
	{
	    char FullSearchPath[IRIT_LINE_LEN_LONG];
	    const IrtDspGuIritSystemInfoStruct
	        *SysInfo = GuIritMdlrDllGetGuIritSystemProps(FI);

	    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				"Sys info:\n\tRelative Point Size = %f\n\tExecutable file name = \"%s\"\n\tExecutable directory = \"%s\"\n\tConfig file name = \"%s\"\n\tDynamic config file name = \"%s\"\n\tAuxiliary data directory = \"%s\"\n\tTesting directory = \"%s\"\n",
				SysInfo -> RelPointSize,
				SysInfo -> ExecutableFullName,
				SysInfo -> ExecutableDirectory,
				SysInfo -> ConfigFileName,
				SysInfo -> DynConfigFileName,
				searchpath(SysInfo -> AuxiliaryDataName,
					   FullSearchPath),
				SysInfo -> TestingDirectory);
	    break;
	}
	case 1021:
	    if (PObjSlicePlane == NULL) {
		PObjSlicePlane = IPGenSrfObject("_SlicePlane_",
					     CagdPrimPlaneSrf(-2, -2, 2, 2, 0),
					     NULL);
		IrtDspPanelUpdateInfoStruct
		    UpdateInfo(IrtMdlrObjectWasTransformUpdate, FI);

		GuIritMdlrDllInsertModelingNewObj(FI, PObjSlicePlane);
		if (GuIritMdlrDllTempObjTransform(FI, PObjSlicePlane, true,
						  &UpdateInfo)) {
		    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
		        "Sys info: Activated transform object on an object.\n");
		}
		else {
		    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
			"Sys info: Failed to activate transform object on an object.\n");
		}
	    }
	    else {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
		    "Sys info: Already transforming a plane.\n");
	    }
	    break;
	case 1030:
	{
	    /* Apply an image as a texture. */
	    int i, j;
	    CagdVType
		Center = { 0.0, 0.0, 0.0 };
	    IRT_DSP_STATIC_DATA IrtImgPixelStruct Image[8][8];
	    IPObjectStruct
		*PObj = IPGenSrfObject("Cone",
				       CagdPrimCone2Srf(Center, 0.5, 0.3, 1.0,
				                    FALSE, CAGD_PRIM_CAPS_NONE),
				       NULL);

	    /* Build a checker board texture. */
	    for (i = 0; i < 8; i++) {
	        for (j = 0; j < 8; j++) {
		    Image[i][j].r = Image[i][j].g = Image[i][j].b =
					           (i + j) % 2 == 0 ? 255 : 0;
		}
	    }

	    GuIritMdlrDllSetTextureFromImage(FI, PObj, Image, 8, 8, FALSE);

	    GuIritMdlrDllInsertModelingNewObj(FI, PObj);
	    break;
	}
        case 1050:
	{
	    /* Exercise the interpreter interface - IO of strings.  Emulate  */
	    /* an interpreter as a text file, emitting out what is given in. */
	    IrtMdlrInterpFI = FI;

	    int InterpID = GuIritMdlrDllRegisterInterpFunc(FI,
							   IrtMdlrInterpInput,
							   "Text", "Text >",
							   ".txt");

	    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
				"Registered \"text\" interpreter %d for files.txt.\n",
				InterpID);
	    break;
	}
        case 1100:
        case 1101:
	{
	    /* Close the panel with cancel or ok: */
	    bool
	        Ok = (p & 0x01);

	    GuIritMdlrDllTriggerModelingFunc(FI,
					     Ok ? IRT_MDLR_CNSTRCT_STATE_OK
		                                : IRT_MDLR_CNSTRCT_STATE_CANCEL);

	    return 998.0;               /* Panel closed - quit immediately. */
	}
	case 1200:
	case 1201:
	case 1202:
	{
	    IPObjectStruct
		*PObj = GuIritMdlrDllGetObjByName(FI, "Object",
						  IRT_DSP_TYPE_OBJ_GEOMETRY,
						  true);

	    if (PObj == NULL) {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
			"No object named \"Object\" was found to highlight.\n");
	    }
	    else {
		switch (p) {
		    default:
			assert(0);
		    case 1200:
			GuIritMdlrDllHighlightObject(FI, PObj,
						     IRT_DSP_GEOM_HIGHLIGHT1);
			break;
		    case 1201:
			GuIritMdlrDllHighlightObject(FI, PObj,
						     IRT_DSP_GEOM_HIGHLIGHT2);
			break;
		    case 1202:
			GuIritMdlrDllHighlightObject(FI, PObj,
						     IRT_DSP_GEOM_HIGHLIGHT3);
			break;
		}
	    }
	    break;
	}
        default:
	    break;
    }

    return 999.0;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Call back function that will be invoked when the transform controller    *
* is activated.	                                                             *
*                                                                            *
* PARAMETERS:                                                                *
*   UpdateInfo:   FI of active function.		                     *
*   State:        State of applied transform.				     *
*                                                                            *
* RETURN VALUE:                                                              *
*   bool:   True if command were executed, false otherwise.                  *
*****************************************************************************/
static void IrtMdlrObjectWasTransformUpdate(void *UpdateInfo,
				            IrtDspPanelUpdateStateType State)
{
    IrtMdlrFuncInfoClass
	*FI = (IrtMdlrFuncInfoClass *) UpdateInfo;

    switch (State) {
	case IRT_DSP_PANEL_UPDATE_STATE_CHANGE:
	    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
				"WasTransformCBFunc activated due to change.\n");
	    if (PObjSlicePlane != NULL) {
		int i, j;
		const IrtHmgnMatType
		    *ObjMat = GuIritMdlrDllGetObjectMatrix(FI, PObjSlicePlane);

		if (ObjMat != NULL) {
		    for (i = 0; i < 4; i++) {
			for (j = 0; j < 4; j++) {
			    fprintf(stderr, "%10f  \n", (*ObjMat)[i][j]);
			}
			fprintf(stderr, "\n");
		    }
		}
	        else {
		    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
				        "\tHas a unit matrix.\n");
		}
	    }
	    break;
	case IRT_DSP_PANEL_UPDATE_STATE_DONE:
	    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
				"WasTransformCBFunc activation complete.\n");
	    if(PObjSlicePlane != NULL) {
		GuIritMdlrDllDeleteModelingFuncObj(FI, PObjSlicePlane);
		PObjSlicePlane = NULL;
	    }
	    break;
	default:
	    assert(0);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Call back function that will be invoked with commands to execute by the  *
* interpreter.                                                               *
*                                                                            *
* PARAMETERS:                                                                *
*   InterpInput:   Command to execute in interpreter.                        *
*                                                                            *
* RETURN VALUE:                                                              *
*   bool:   True if command were executed, false otherwise.                  *
*****************************************************************************/
static bool IrtMdlrInterpInput(const char *InterpInput)
{
    unsigned int i;
    char
        *Str = new char[strlen(InterpInput) + 2];

    for (i = 0; i < strlen(InterpInput); i++)
        Str[i] = islower(InterpInput[i]) ? toupper(InterpInput[i])
	                                 : InterpInput[i];
    Str[i] = 0;

    GuIritMdlrDllInterpPrintf(IrtMdlrInterpFI, IRT_DSP_LOG_WARNING, Str);

    delete [] Str;

    return true;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*    Call back function to be invoked on generic events.                     *
*                                                                            *
* PARAMETERS:                                                                *
*    GenericEvent:    To process.                                            *
*                                                                            *
* RETURN VALUE:                                                              *
*    int:          TRUE if handled, FALSE otherwise.                         *
*****************************************************************************/
static int IrtMdlrExampleGenericEventCallBack(IrtMdlrGenericEventStruct
							  *GuiritGenericEvent)
{
    if (GuiritGenericEvent -> EventType != IRT_MDLR_GENERIC_EVENT_TYPE1)
	return FALSE;

    IrtMdlrExampleGenericEventStruct ExampleGenericEvent;
    IRIT_GEN_COPY(&ExampleGenericEvent, GuiritGenericEvent -> GenericData.Buf,
		  sizeof(IrtMdlrExampleGenericEventStruct));

    GuIritMdlrDllPrintf(ExampleGenericEvent.GenericFI, IRT_DSP_LOG_INFO,
			"Generic Event %s Received: Pitch = %d, Pi = %f.\n",
			ExampleGenericEvent.GenericStr,
			ExampleGenericEvent.GenericInt,
			ExampleGenericEvent.GenericReal);
    return TRUE;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Callback function for mouse events for curve creation - handle click and *
* drag of existing control points, modifying the curve.  	             *
*                                                                            *
* PARAMETERS:                                                                *
*   MouseEvent:  The mouse call back event to handle.		             *
*                                                                            *
* RETURN VALUE:                                                              *
*   int: True if event handled, false to propagate event to next handler.    *
*****************************************************************************/
static int IrtMdlrExampleMouseCallBack(IrtMdlrMouseEventStruct *MouseEvent)
{
    IrtMdlrFuncInfoClass
	*FI = (IrtMdlrFuncInfoClass *) MouseEvent -> Data;
    IPObjectStruct *PObj;

    /* Control and shift should always be pressed to handle events. */
    if (MouseEvent -> KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN) {
        IRT_DSP_STATIC_DATA IrtPtType PtStart;
	IRT_DSP_STATIC_DATA int
	    MouseActive = FALSE;
	int VrtcsRvrsd;
	IrtVecType V1, V2, V3, V4;
	IPPolygonStruct *Pl;

        /* Handle a square dragging box. */
        switch (MouseEvent -> Type) {
	    case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
		GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
		IRIT_PT_COPY(PtStart, MouseEvent -> Coord);
		MouseActive = TRUE;
	        break;
	    case IRT_DSP_MOUSE_EVENT_LEFT_UP:
		GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
		MouseActive = FALSE;
	        break;
	    case IRT_DSP_MOUSE_EVENT_MOVE:
		if (!MouseActive)
		    break;
		V1[0] = PtStart[0];
		V1[1] = PtStart[1];
		V1[2] = 0.0;
		V2[0] = PtStart[0];
		V2[1] = MouseEvent -> Coord[1];
		V2[2] = 0.0;
		V3[0] = MouseEvent -> Coord[0];
		V3[1] = MouseEvent -> Coord[1];
		V3[2] = 0.0;
		V4[0] = MouseEvent -> Coord[0];
		V4[1] = PtStart[1];
		V4[2] = 0.0;
		Pl = PrimGenPolygon4Vrtx(V1, V2, V3, V4, NULL, &VrtcsRvrsd,
				         NULL);
		PObj = IPGenPOLYObject(Pl);

		GuIritMdlrDllClearThisModelingFuncTempDisplayObjs(FI);
		
		GuIritMdlrDllAddTempDisplayObject(FI, PObj, false, 2);

		/* Redraw the rectangle. */
		GuIritMdlrDllRequestIntermediateUpdate(FI, false);
	        break;
	    default:
		break;
	}
    }
    else if (MouseEvent -> KeyModifiers & IRT_DSP_KEY_MODIFIER_SHIFT_DOWN) {
        IRT_DSP_STATIC_DATA IPPolygonStruct
	    *Pl = NULL;
	IRT_DSP_STATIC_DATA int
	    MouseActive = FALSE;
	IPVertexStruct *V;

        /* Handle a freeform mouse sketching. */
        switch (MouseEvent -> Type) {
	    case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
		GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
		Pl = IPAllocPolygon(FALSE, NULL, NULL);
		MouseActive = TRUE;
	        break;
	    case IRT_DSP_MOUSE_EVENT_LEFT_UP:
		GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
		IPFreePolygon(Pl);
		Pl = NULL;
		MouseActive = FALSE;
	        break;
	    case IRT_DSP_MOUSE_EVENT_MOVE:
		if (!MouseActive)
		    break;

		V = IPAllocVertex2(Pl -> PVertex);
		Pl -> PVertex = V;
		IRIT_PT_COPY(V -> Coord, MouseEvent -> Coord);

		PObj = IPGenPOLYLINEObject(IPCopyPolygon(Pl));

		GuIritMdlrDllClearThisModelingFuncTempDisplayObjs(FI);

		GuIritMdlrDllAddTempDisplayObject(FI, PObj, false, 2);

		/* Redraw the shape. */
		GuIritMdlrDllRequestIntermediateUpdate(FI, false);
		break;
	    default:
		break;
	}
    }

    return TRUE;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Callback function during active animation.  Will be invoked on every     *
* rendered frame.							     *
*                                                                            *
* PARAMETERS:                                                                *
*   AnimationInfo:  The animation information.			             *
*                                                                            *
* RETURN VALUE:                                                              *
*   bool:   True if successful.						     *
*****************************************************************************/
static bool AnimCallBackPrintTime(IrtDspGrAnimationBasicInfoClass
							      *AnimationInfo)
{
    if (AnimationInfo == NULL)
	return false;

    assert(GlblExampleFI != NULL);
    GuIritMdlrDllPrintf(GlblExampleFI, IRT_DSP_LOG_INFO,
			"Example Anim Call Back: Time = %f.\n",
			AnimationInfo -> RunTime);
    return true;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu checkbox status and print.					     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuCheckboxExample(IrtMdlrFuncInfoClass *FI)
{
    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) {
	if (IrtMdlrMenuCheckBoxState)
	    FI -> Params -> ParamsTypes.BoolVal = true;
	else 
	    FI -> Params -> ParamsTypes.BoolVal = false;
    }
    else {
	/* Get menu tab input widget value. */
	if (FI -> Params -> ParamsTypes.BoolVal)
	    IrtMdlrMenuCheckBoxState = TRUE; 
	else
	    IrtMdlrMenuCheckBoxState = FALSE; 

        GuIritMdlrDllStatusPrintf(FI, 0, 
				  "Test checkbox %schecked (value = %d).",
				  IrtMdlrMenuCheckBoxState ? "" : "un",
				  IrtMdlrMenuCheckBoxState);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu spinctrl value and print.					     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuIntegerExample(IrtMdlrFuncInfoClass *FI)
{
    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) 
	FI -> Params -> ParamsTypes.IntVal = IrtMdlrMenuIntegerValue;
    else {
	/* Get menu tab input widget value. */
	IrtMdlrMenuIntegerValue = FI -> Params -> ParamsTypes.IntVal;   

        GuIritMdlrDllStatusPrintf(FI, 0, "Spin control value changed, now %d",
				  IrtMdlrMenuIntegerValue);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu spinctrl value and print.					     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuGridIntegerExample(IrtMdlrFuncInfoClass *FI)
{
    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) 
	FI -> Params -> ParamsTypes.IntVal = IrtMdlrMenuIntegerValue;
    else {
	/* Get menu tab input widget value. */
	IrtMdlrMenuIntegerValue =  FI -> Params -> ParamsTypes.IntVal;   

        GuIritMdlrDllStatusPrintf(FI, 0, "Grid Spin control value changed, now %d",
				  IrtMdlrMenuIntegerValue);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu combobox selected item and print.				     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuSelectionExample(IrtMdlrFuncInfoClass *FI)
{
    int i,
        Item = 0;

    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) { 
	FI -> Params -> ParamsTypes.Selections =
	                                 (char **) IrtMdlrMenuSelectionItems;
    }
    else {
	for (i = 0; i < GUIRIT_DLL_EXAMPLE_NUM_OF_TYPE_CHOICES; i++) {
	    if (stricmp(FI -> Params -> ParamsTypes.StrVal,
			IrtMdlrMenuSelectionItems[i]) == 0) {
		Item = i;
		break;
	    }
	}
    
	GuIritMdlrDllStatusPrintf(FI, 0, "Choice item %d selected", Item);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu spinctrl value and print.					     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuRealExample(IrtMdlrFuncInfoClass *FI)
{
    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) { 
	FI -> Params -> ParamsTypes.RealVal = IrtMdlrMenuRealValue;
    }
    else {
	/* Get menu tab input widget value. */
	IrtMdlrMenuRealValue =  FI -> Params -> ParamsTypes.RealVal;   

        GuIritMdlrDllStatusPrintf(FI, 0, 
				  "Real Spin control value changed, now %.16g",
				  IrtMdlrMenuRealValue);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu spinctrl value and print.					     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuGridRealExample(IrtMdlrFuncInfoClass *FI)
{
    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) { 
	FI -> Params -> ParamsTypes.RealVal = IrtMdlrMenuRealValue;
    }
    else {
	/* Get menu tab input widget value. */
	IrtMdlrMenuRealValue =  FI -> Params -> ParamsTypes.RealVal;   

        GuIritMdlrDllStatusPrintf(FI, 0, 
				  "Grid Real Spin control value changed, now %.16g",
				  IrtMdlrMenuRealValue);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu vector values and print.					     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuVectorExample(IrtMdlrFuncInfoClass *FI)
{
    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) {
        int i;

	for (i = 0; i < 3; i++) 
	    FI -> Params -> ParamsTypes.VecVal[i] = IrtMdlrMenuVectorValue[i];
    }
    else {
        int i;

	/* Get menu tab input widget value. */
	for (i = 0; i < 3; i++) 
	    IrtMdlrMenuVectorValue[i] =  FI -> Params -> ParamsTypes.VecVal[i];   

        GuIritMdlrDllStatusPrintf(FI, 0, 
		     "Real Spin control value changed, now %.16g %.16g %.16g;",
		     IrtMdlrMenuVectorValue[0], IrtMdlrMenuVectorValue[1], 
		     IrtMdlrMenuVectorValue[2]);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu radiobox selection and print.				     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuRadioboxExample(IrtMdlrFuncInfoClass *FI)
{
    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) {
	FI -> Params -> ParamsTypes.IntVal = IrtMdlrMenuRadioBoxSelection;
    }
    else {
	/* Get menu tab input widget value. */
	IrtMdlrMenuRadioBoxSelection =  FI -> Params -> ParamsTypes.IntVal;   

        GuIritMdlrDllStatusPrintf(FI, 0, "Radiobox selection changed, now %d",
				  IrtMdlrMenuRadioBoxSelection);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu text string and print.					     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuTextExample(IrtMdlrFuncInfoClass *FI)
{
    char 
	Name[IRIT_LINE_LEN_LONG] = "Hello";

    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) { 
	strcpy(FI -> Params -> ParamsTypes.StrVal, Name);
    }
    else {
	/* Get menu tab input widget value. */
	strcpy(Name, FI -> Params -> ParamsTypes.StrVal);
        GuIritMdlrDllStatusPrintf(FI, 0, "Text ctrl value now \"%s\"", Name);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   A menu modeling function with no panel - fetches parameters from GUI.    *
*   A call back function that is invoked via the IrtMdlrFuncs database.      *
*   Get menu text string and print.					     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrMenuDisableTextExample(IrtMdlrFuncInfoClass *FI)
{
    char 
	Name[IRIT_LINE_LEN_LONG] = "GuIrit";

    /* Get default value of the menu tab input widget. */
    if (FI -> Params -> Init) { 
	strcpy(FI -> Params -> ParamsTypes.StrVal, Name);
    }
    else {
	/* Get menu tab input widget value. */
	strcpy(Name, FI -> Params -> ParamsTypes.StrVal);
        GuIritMdlrDllStatusPrintf(FI, 0, "Text ctrl value now \"%s\"", Name);
    }
}
