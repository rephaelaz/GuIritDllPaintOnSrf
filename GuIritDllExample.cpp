/******************************************************************************
* GuiIritDllExample.cpp - a simple example of a Dll extension of GuIrit.      *
*******************************************************************************
* (C) Gershon Elber, Technion, Israel Institute of Technology                 *
*******************************************************************************
* Written by Ilan Coronel and Raphael Azoulay, April 2019.					  *
******************************************************************************/

#include <ctype.h>
#include <stdlib.h>

#include "IrtDspBasicDefs.h"
#include "IrtMdlr.h"
#include "IrtMdlrFunc.h"
#include "IrtMdlrDll.h"
#include "GuIritDllExtensions.h"

#include "Icons/IconMenuExample.xpm"
#include "Icons/IconExample.xpm"

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass *FI);
static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct *MouseEvent);

IRT_DSP_STATIC_DATA IrtRType IrtMdlrBeepPitch = 1000.0;
IRT_DSP_STATIC_DATA IrtRType IrtMdlrBeepDuration = 100.0;
IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass *GlobalFI = NULL;

IRT_DSP_STATIC_DATA IrtMdlrFuncTableStruct TexturePainterFunctionTable[] = 
{
	{
		0,
		IconExample,
		"IRT_MDLR_EXAMPLE_BEEP",
		"Test",
		"Test test test",
		"This is a test with a long text.",
		IrtMdlrTexturePainter,
		NULL,
		IRT_MDLR_PARAM_POINT_SIZE_CONTROL | IRT_MDLR_PARAM_VIEW_CHANGES_UDPATE | IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_DFLT_ON,
		IRT_MDLR_NUMERIC_EXPR,
		3,
		IRT_MDLR_PARAM_EXACT,
		{
			IRT_MDLR_STRING_EXPR,
			IRT_MDLR_NUMERIC_EXPR,
			IRT_MDLR_NUMERIC_EXPR
		},
		{
			"BEEP", 
			&IrtMdlrBeepPitch,
			&IrtMdlrBeepDuration
		},
		{
			"Test 1",
			"Test 2",
			"Test 3"
		},
		{
			"Resulting object's name",
			"The frequency of the sound (Hz)",
			"The duration to make the sound, in millisecs."
		}
	}
};

IRT_DSP_STATIC_DATA const int FUNC_TABLE_SIZE = sizeof(TexturePainterFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

extern "C" bool _IrtMdlrDllRegister(void)
{
	GuIritMdlrDllRegister(TexturePainterFunctionTable,	FUNC_TABLE_SIZE, "Example",	IconMenuExample);
	return true;
}

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass *FI)
{
	char Name[IRIT_LINE_LEN_LONG];
	IPObjectStruct *ResultObject;

	if (FI -> InvocationNumber == 0 && GlobalFI == NULL) {
		GuIritMdlrDllPushMouseEventFunc(
			FI,
			IrtMdlrTexturePainterMouseCallBack,
			(IrtDspMouseEventType) (IRT_DSP_MOUSE_EVENT_LEFT),
			(IrtDspKeyModifierType) (IRT_DSP_KEY_MODIFIER_CTRL_DOWN | IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
			FI);
		GuIritMdlrDllSetRealInputDomain(FI, 20.0, 20000.0, 1);
		GuIritMdlrDllSetRealInputDomain(FI, 1.0, IRIT_INFNTY, 2);
		GlobalFI = FI;
	}

	GuIritMdlrDllGetInputParameter(FI, 0, &Name);
	GuIritMdlrDllGetInputParameter(FI, 1, &IrtMdlrBeepPitch);
	GuIritMdlrDllGetInputParameter(FI, 2, &IrtMdlrBeepDuration);

	if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_APPLY ||
		FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK) 
	{
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Pitch = %f, Duration = %f.\n", IrtMdlrBeepPitch, IrtMdlrBeepDuration);
		ResultObject = IPGenNUMValObject(IrtMdlrBeepPitch);
		GuIritMdlrDllSetObjectName(FI, ResultObject, Name);
		GuIritMdlrDllInsertModelingFuncObj(FI, ResultObject);
	}
}

static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct *MouseEvent)
{
	IrtMdlrFuncInfoClass *FI = (IrtMdlrFuncInfoClass *) MouseEvent -> Data;
	IRT_DSP_STATIC_DATA int MouseActive = FALSE;
	IRT_DSP_STATIC_DATA IrtPtType Point;

	if (MouseEvent->KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN)
	{
		switch (MouseEvent -> Type)
		{
		case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
			GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
			MouseActive = TRUE;
			break;
		case IRT_DSP_MOUSE_EVENT_LEFT_UP:
			GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
			MouseActive = FALSE;
			break;
		case IRT_DSP_MOUSE_EVENT_MOVE:
			if (!MouseActive)
				break;
			IRIT_PT_COPY(Point, MouseEvent->Coord);
			GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Mouse position : (%f, %f, %f)\n", Point[0], Point[1], Point[2]);
			break;
		default:
			break;
		}
	}
	return TRUE;
}
