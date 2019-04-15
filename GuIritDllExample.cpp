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

#include <string>
#include "glew.h"

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass* FI);
static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);


IRT_DSP_STATIC_DATA IrtRType IrtMdlrBeepPitch = 1000.0;
IRT_DSP_STATIC_DATA IrtRType IrtMdlrBeepDuration = 100.0;
IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

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
	GuIritMdlrDllRegister(TexturePainterFunctionTable, FUNC_TABLE_SIZE, "Example", IconMenuExample);
	return true;
}

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass* FI)
{
	char Name[IRIT_LINE_LEN_LONG];
	IPObjectStruct* ResultObject;

	if (FI->InvocationNumber == 0 && GlobalFI == NULL) {
		GuIritMdlrDllPushMouseEventFunc(
			FI,
			IrtMdlrTexturePainterMouseCallBack,
			(IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
			(IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_CTRL_DOWN | IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
			FI);
		GuIritMdlrDllSetRealInputDomain(FI, 20.0, 20000.0, 1);
		GuIritMdlrDllSetRealInputDomain(FI, 1.0, IRIT_INFNTY, 2);
		GlobalFI = FI;
	}

	GuIritMdlrDllGetInputParameter(FI, 0, &Name);
	GuIritMdlrDllGetInputParameter(FI, 1, &IrtMdlrBeepPitch);
	GuIritMdlrDllGetInputParameter(FI, 2, &IrtMdlrBeepDuration);

	if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_APPLY ||
		FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK)
	{
		ResultObject = IPGenNUMValObject(IrtMdlrBeepPitch);
		GuIritMdlrDllSetObjectName(FI, ResultObject, Name);
		GuIritMdlrDllInsertModelingFuncObj(FI, ResultObject);
	}
}

static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct * MouseEvent)
{
	IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)MouseEvent->Data;
	IRT_DSP_STATIC_DATA int MouseActive = FALSE;
	IRT_DSP_STATIC_DATA IrtPtType Point;

	GLuint handle = 0;
	const IPObjectStruct* PObj = MouseEvent->PObj;

	if (MouseEvent->KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN)
	{
		switch (MouseEvent->Type)
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

			handle = AttrIDGetObjectIntAttrib(PObj, IRIT_ATTR_CREATE_ID(_ImageTexID));


			if (handle > 0 && MouseEvent->UV != NULL) {
				
				glBindTexture(GL_TEXTURE_2D, handle);

				GLint width;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
				GLint height;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

				int vv[5][5] = {0};
				glTexSubImage2D(GL_TEXTURE_2D, 0, MouseEvent->UV[0]*width, MouseEvent->UV[1]*height, 5, 5, GL_RGBA, GL_UNSIGNED_BYTE, vv);

			}
			else {
				GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "No assigned texture\n");
			}

			break;
		default:
			break;
		}
	}
	return TRUE;
}
