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
#include <math.h>

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass* FI);
static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);

IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

IRT_DSP_STATIC_DATA IrtVecType color = {0, 0, 0};
IRT_DSP_STATIC_DATA IrtRType alpha = 0;
IRT_DSP_STATIC_DATA IrtRType size = 1;

IRT_DSP_STATIC_DATA const int TEXTURE_SIZE = 64;
IRT_DSP_STATIC_DATA bool loaded = false;
IRT_DSP_STATIC_DATA IrtImgPixelStruct texture [TEXTURE_SIZE][TEXTURE_SIZE];

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
			IRT_MDLR_PARAM_VIEW_CHANGES_UDPATE | IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_ALWAYS,
			IRT_MDLR_NUMERIC_EXPR,
			4,
			IRT_MDLR_PARAM_EXACT,
		{
			IRT_MDLR_BUTTON_EXPR, // Color picker
				IRT_MDLR_VECTOR_EXPR, // RGB Values
				IRT_MDLR_NUMERIC_EXPR, // Alpha Value
				IRT_MDLR_NUMERIC_EXPR, // Size
		},
		{
			NULL,
				&color,
				&alpha,
				&size,

			},
			{
				"Color Picker",
					"Color",
					"Alpha",
					"Size",

			},
			{
				"Pick the color of the painter.",
					"RGB values of the painter.",
					"Alpha channel value of the painter.",
					"Size of the painting brush.",
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

		for (int i = 0; i < TEXTURE_SIZE; i++) {
			for (int j = 0; j < TEXTURE_SIZE; j++) {
				texture[i][j].r = 255;
				texture[i][j].g = 255;
				texture[i][j].b = 255;
			}
		}
		GlobalFI = FI;
	}

	GuIritMdlrDllGetInputParameter(FI, 1, &color);
	GuIritMdlrDllGetInputParameter(FI, 2, &alpha);
	GuIritMdlrDllGetInputParameter(FI, 3, &size);

	if (FI -> IntermediateWidgetMajor == 0) {
		unsigned char cr = (unsigned char) color[0];
		unsigned char cg = (unsigned char) color[1];
		unsigned char cb = (unsigned char) color[2];
		if (GuIritMdlrDllGetAsyncInputColor(FI, &cr, &cg, &cb)) {
			color[0] = (double) cr;
			color[1] = (double) cg;
			color[2] = (double) cb;
			GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Color set to: %.0lf %.0lf %.0lf\n", color[0], color[1], color[2]);
			GuIritMdlrDllSetInputParameter(FI, 1, &color);
		}
	}

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
	IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*) MouseEvent->Data;
	IRT_DSP_STATIC_DATA int clicking = FALSE;

	IPObjectStruct* PObj = (IPObjectStruct*) MouseEvent->PObj;

	if (MouseEvent->KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN)
	{
		switch (MouseEvent->Type)
		{
		case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
			GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
			clicking = TRUE;
			break;
		case IRT_DSP_MOUSE_EVENT_LEFT_UP:
			GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
			clicking = FALSE;
			//SaveTexture(FI, handle);
			break;
		}
		if (clicking) {
			if (MouseEvent->UV != NULL) {
				int x_offset = (float) TEXTURE_SIZE * (fmod(MouseEvent->UV[0], 1));
				int y_offset = (float) TEXTURE_SIZE * (fmod(MouseEvent->UV[1], 1));
				int x_start = x_offset - size / 2;
				int y_start = y_offset - size / 2;
				for (int i = x_start; i < x_start + size; i++) {
					for (int j = y_start; j < y_start + size; j++) {
						if (i < 0 || i >= TEXTURE_SIZE || j < 0 || j >= TEXTURE_SIZE) {
							continue;
						}
						texture[i][j].r = (IrtBType) color[0];
						texture[i][j].g = (IrtBType) color[1];
						texture[i][j].b = (IrtBType) color[2];
						//texture[i][j].a = alpha;
					}
				}
				if (!loaded) {
					GuIritMdlrDllSetTextureFromImage(FI, PObj, texture, TEXTURE_SIZE, TEXTURE_SIZE, FALSE);
					loaded = true;
				}
			}
		}
	}
	return TRUE;
}
