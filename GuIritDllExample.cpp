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
static void IrtMdlrTexturePainterInitTexture();
static void IrtMdlrTexturePainterInitShapeHierarchy(IrtMdlrFuncInfoClass* FI);
static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);

IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

IRT_DSP_STATIC_DATA IrtVecType color = {0, 0, 0};
IRT_DSP_STATIC_DATA IrtRType alpha = 0;
IRT_DSP_STATIC_DATA IrtRType size = 1;

#define SHAPE_FILE_RELATIVE_PATH "\\Example\\Shape"
IRT_DSP_STATIC_DATA char shape_name[IRIT_LINE_LEN_XLONG] = "";
IRT_DSP_STATIC_DATA const int TEXTURE_SIZE = 256;
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
			5,
			IRT_MDLR_PARAM_EXACT,
		{
			IRT_MDLR_BUTTON_EXPR, // Color picker
			IRT_MDLR_VECTOR_EXPR, // RGB Values
			IRT_MDLR_NUMERIC_EXPR, // Alpha Value
			IRT_MDLR_HIERARCHY_SELECTION_EXPR,
			IRT_MDLR_NUMERIC_EXPR, // Size
		},
		{
			NULL,
			&color,
			&alpha,
			shape_name,
			&size,

		},
		{
			"Color Picker",
			"Color",
			"Alpha",
			"Shape",
			"Size",

		},
		{
			"Pick the color of the painter.",
			"RGB values of the painter.",
			"Alpha channel value of the painter.",
			"Shape of the painting brush.",
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
	if (FI->InvocationNumber == 0 && GlobalFI == NULL) {

		IrtMdlrTexturePainterInitTexture();
		IrtMdlrTexturePainterInitShapeHierarchy(FI);

		GuIritMdlrDllPushMouseEventFunc(
			FI,
			IrtMdlrTexturePainterMouseCallBack,
			(IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
			(IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_CTRL_DOWN | IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
			FI);
		GlobalFI = FI;
	}

	int shape_index = 0;
	char* tmp = shape_name;
	char** ptr = &tmp;

	GuIritMdlrDllGetInputParameter(FI, 1, &color);
	GuIritMdlrDllGetInputParameter(FI, 2, &alpha);
	GuIritMdlrDllGetInputParameter(FI, 3, &shape_index, ptr);
	GuIritMdlrDllGetInputParameter(FI, 4, &size);

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

	// Don't need to instantiate
	/*if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_APPLY ||
		FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK)
	{
		ResultObject = IPGenNUMValObject(0);
		GuIritMdlrDllSetObjectName(FI, ResultObject, "Painter");
		GuIritMdlrDllInsertModelingFuncObj(FI, ResultObject);
	}*/
}

static void IrtMdlrTexturePainterInitTexture() 
{
	for (int i = 0; i < TEXTURE_SIZE; i++) {
		for (int j = 0; j < TEXTURE_SIZE; j++) {
			texture[i][j].r = 255;
			texture[i][j].g = 255;
			texture[i][j].b = 255;
		}
	}
}

static void IrtMdlrTexturePainterInitShapeHierarchy(IrtMdlrFuncInfoClass* FI)
{
	const IrtDspGuIritSystemInfoStruct *sys_file_names = GuIritMdlrDllGetGuIritSystemProps(FI); 
	char file_path[IRIT_LINE_LEN_LONG], search_path[IRIT_LINE_LEN_LONG];
	const char **shape_files = NULL;
	sprintf(file_path, "%s%s", searchpath(sys_file_names -> AuxiliaryDataName, search_path), SHAPE_FILE_RELATIVE_PATH);
	shape_files = GuIritMdlrDllGetAllFilesNamesInDirectory(FI, file_path, "*.itd");
	if (shape_files == NULL || shape_files[0] == NULL) {
	    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "Shape files not found. Check directory: \"%s\"\n", file_path);
	    return;
	}
	strcpy(shape_name, ""); 
	for (int i = 0; shape_files[i] != NULL; ++i) {
		const char *p = strstr(shape_files[i], SHAPE_FILE_RELATIVE_PATH);
		const char *q = strchr(p, '.');
		if (p != NULL) {
			p += strlen(SHAPE_FILE_RELATIVE_PATH) + 1;
			char name[IRIT_LINE_LEN_LONG];
			strcpy(name, ""); 
			strncpy(name, p, q - p);
			name[q - p] = '\0';
			sprintf(shape_name, "%s%s;", shape_name, name); 
		}
	}
	sprintf(shape_name, "%s:0", shape_name);
	GuIritMdlrDllSetInputSelectionStruct names(shape_name, IRIT_MAX_INT, file_path);
	GuIritMdlrDllSetInputParameter(FI, 3, &names);
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
						texture[j][i].r = (IrtBType) color[0];
						texture[j][i].g = (IrtBType) color[1];
						texture[j][i].b = (IrtBType) color[2];
					}
				}
				GuIritMdlrDllSetTextureFromImage(FI, PObj, texture, TEXTURE_SIZE, TEXTURE_SIZE, FALSE);
			}
		}
	}
	return TRUE;
}
