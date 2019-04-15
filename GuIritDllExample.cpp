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


IRT_DSP_STATIC_DATA IrtRType R = 255;
IRT_DSP_STATIC_DATA IrtRType G = 255;
IRT_DSP_STATIC_DATA IrtRType B = 255;
IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

IRT_DSP_STATIC_DATA IrtMdlrFuncTableStruct TexturePainterFunctionTable[] =
{
	{
		0,
			IconExample,
			"IRT_MDLR_EXAMPLE_BEEP",
			"Painter",
			"Texture Painter",
			"This is a texture painter",
			IrtMdlrTexturePainter,
			NULL,
			IRT_MDLR_PARAM_POINT_SIZE_CONTROL | IRT_MDLR_PARAM_VIEW_CHANGES_UDPATE | IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_DFLT_ON,
			IRT_MDLR_NUMERIC_EXPR,
			3,
			IRT_MDLR_PARAM_EXACT,
		{
				IRT_MDLR_NUMERIC_EXPR,
				IRT_MDLR_NUMERIC_EXPR,
				IRT_MDLR_NUMERIC_EXPR,
		},
		{
				&R,
				&G,
				&B
			},
			{
				"R",
				"G",
				"B"
			},
			{
				"Red component",
				"Green component",
				"Blue component"
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
	IPObjectStruct* ResultObject;

	if (FI->InvocationNumber == 0 && GlobalFI == NULL) {
		GuIritMdlrDllPushMouseEventFunc(
			FI,
			IrtMdlrTexturePainterMouseCallBack,
			(IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
			(IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_CTRL_DOWN | IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
			FI);
		GlobalFI = FI;
	}

	GuIritMdlrDllGetInputParameter(FI, 0, &R);
	GuIritMdlrDllGetInputParameter(FI, 1, &G);
	GuIritMdlrDllGetInputParameter(FI, 2, &B);

	if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_APPLY ||
		FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK)
	{
		ResultObject = IPGenNUMValObject(0);
		GuIritMdlrDllSetObjectName(FI, ResultObject, "Painter");
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
	handle = AttrIDGetObjectIntAttrib(PObj, IRIT_ATTR_CREATE_ID(_ImageTexID));

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


			if (handle > 0 && MouseEvent->UV != NULL) {
				glBindTexture(GL_TEXTURE_2D, handle);

				GLint width;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
				GLint height;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

				int vv[5][5] = {0};
				glTexSubImage2D(GL_TEXTURE_2D, 0, MouseEvent->UV[0]*width, MouseEvent->UV[1]*height, 5, 5, GL_RGB, GL_UNSIGNED_BYTE, vv);
				
				//GLvoid* pixels = NULL;
				//glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
				//GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "%s\n", pixels);
			}

			else {
				GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "No assigned texture\n");
			}
			break;
		default:
			break;
		}
	}
	//if (IRT_DSP_KEY_MODIFIER_SHIFT_DOWN) {
	//GLvoid* pixels = NULL;
	//glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	//GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "%s\n", pixels);
	//AttrIDGetObjectIntAttrib(PObj, .mb_str(wxConvUTF8));
	//}
	return TRUE;
}
