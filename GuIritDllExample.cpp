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
#include <vector>
#include <map>
#include <algorithm>
#include <math.h>
#include <chrono>

using std::string;
using std::vector;
using std::map;
using std::pair;
using std::swap;
using namespace std::chrono;

IRT_DSP_STATIC_DATA const double PI = 3.1415926535;

typedef struct {
	double x;
	double y;
} Point;

typedef struct {
	int x;
	int y;
} Pixel;

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrTexturePainterResetAlphaBitmap();
static void IrtMdlrTexturePainterResizeTexture(IrtMdlrFuncInfoClass* FI, int new_size);
static void IrtMdlrTexturePainterLoadShape(IrtMdlrFuncInfoClass* FI, const char* filename);
static void IrtMdlrTexturePainterInitShapeHierarchy(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrTexturePainterCalculateLine(const Pixel& start, const Pixel& end, vector<int>& points, const int min_y);
static void IrtMdlrTexturePainterRenderShape(IrtMdlrFuncInfoClass* FI, int x_offset, int y_offset);
static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);

IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

IRT_DSP_STATIC_DATA IrtRType _texture_size = 1024;
IRT_DSP_STATIC_DATA int texture_size = -1;
IRT_DSP_STATIC_DATA bool texture_reset_pending = false;
IRT_DSP_STATIC_DATA high_resolution_clock::time_point reset_timer;
IRT_DSP_STATIC_DATA bool reset_timer_init = false;
IRT_DSP_STATIC_DATA IrtImgPixelStruct* texture;
IRT_DSP_STATIC_DATA vector<vector<bool>> alpha_bitmap;

IRT_DSP_STATIC_DATA IrtVecType color = { 0, 0, 0 };
IRT_DSP_STATIC_DATA IrtRType alpha = 255;
IRT_DSP_STATIC_DATA IrtRType x_factor = 1;
IRT_DSP_STATIC_DATA IrtRType y_factor = 1;

IRT_DSP_STATIC_DATA const char RELATIVE_PATH[IRIT_LINE_LEN_LONG] = "\\Example\\Masks";
IRT_DSP_STATIC_DATA char shape_names[IRIT_LINE_LEN_XLONG] = "";
IRT_DSP_STATIC_DATA const char** shape_files = NULL;
IRT_DSP_STATIC_DATA int shape_index = -1;
IRT_DSP_STATIC_DATA float* shape_matrix = NULL;
IRT_DSP_STATIC_DATA int shape_width;
IRT_DSP_STATIC_DATA int shape_height;
IRT_DSP_STATIC_DATA int shape_init = 1;

typedef enum {
	FIELD_LOAD = 0,
	FIELD_SAVE,
	FIELD_RESET,
	FIELD_TEXTURE_SIZE,
	FIELD_COLOR,
	FIELD_ALPHA,
	FIELD_SHAPE,
	FIELD_X_FACTOR,
	FIELD_Y_FACTOR
} ;

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
		IRT_MDLR_PARAM_VIEW_CHANGES_UDPATE | IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_DFLT_ON,
		IRT_MDLR_NUMERIC_EXPR,
		9,
		IRT_MDLR_PARAM_EXACT,
		{
			// Texture fields
			IRT_MDLR_BUTTON_EXPR, // Load Texture
			IRT_MDLR_BUTTON_EXPR, // Save Texture
			IRT_MDLR_BUTTON_EXPR, // Reset Texture
			IRT_MDLR_NUMERIC_EXPR, // Texture size

			// Brush fields
			IRT_MDLR_VECTOR_EXPR, // RGB Values
			IRT_MDLR_NUMERIC_EXPR, // Alpha Value
			IRT_MDLR_HIERARCHY_SELECTION_EXPR, // Shape selection menu
			IRT_MDLR_NUMERIC_EXPR, // X Factor
			IRT_MDLR_NUMERIC_EXPR, // Y Factor
		},
		{
			NULL,
			NULL, 
			NULL,
			& _texture_size,

			&color,
			&alpha,
			shape_names,
			&x_factor,
			&y_factor,
		},
		{
			"Load Texture",
			"Save Texture",
			"Reset Texture",
			"Texture Size",

			"Color",
			"Alpha",
			"Shape",
			"X Factor",
			"Y Factor"
		},
		{
			"Loads a texture from an image file.",
			"Saves the texture into an image file.",
			"Resets the current texture to a blank texture.",
			"Dimensions of the texture.",

			"Color of the painting brush.",
			"Transparence factor of the painting brush.",
			"Shape of the painting brush.",
			"X factor of the painting brush.",
			"Y factor of the painting brush."
		}
	}
};

IRT_DSP_STATIC_DATA const int FUNC_TABLE_SIZE = sizeof(TexturePainterFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

static int irtrtype_to_i(IrtRType r) {
	char str[IRIT_LINE_LEN_SHORT];
	sprintf(str, "%d", r);
	return atoi(str);
}

extern "C" bool _IrtMdlrDllRegister(void)
{
	GuIritMdlrDllRegister(TexturePainterFunctionTable, FUNC_TABLE_SIZE, "Example", IconMenuExample);
	return true;
}

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass* FI)
{
	if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_INIT) {
		return;
	}

	if (FI->InvocationNumber == 0 && GlobalFI == NULL) {
		GuIritMdlrDllPushMouseEventFunc(
			FI,
			IrtMdlrTexturePainterMouseCallBack,
			(IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
			(IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
			FI);
		IrtMdlrTexturePainterInitShapeHierarchy(FI);

		GuIritMdlrDllSetRealInputDomain(FI, 0, IRIT_INFNTY, FIELD_TEXTURE_SIZE);
		GuIritMdlrDllSetRealInputDomain(FI, 0, 255, FIELD_COLOR, 0);
		GuIritMdlrDllSetRealInputDomain(FI, 0, 255, FIELD_COLOR, 1);
		GuIritMdlrDllSetRealInputDomain(FI, 0, 255, FIELD_COLOR, 2);
		GuIritMdlrDllSetRealInputDomain(FI, 0, 255, FIELD_ALPHA);
		GuIritMdlrDllSetRealInputDomain(FI, 0, IRIT_INFNTY, FIELD_X_FACTOR);
		GuIritMdlrDllSetRealInputDomain(FI, 0, IRIT_INFNTY, FIELD_Y_FACTOR);

		GlobalFI = FI;
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture Painter initialized\n");
	}

	if (GlobalFI != NULL && (
		FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK ||
		FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_CANCEL)) 
	{
		GuIritMdlrDllPopMouseEventFunc(FI);
		GlobalFI = false;
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture Painter unloaded\n");
	}

	// Texture fields
	if (FI->IntermediateWidgetMajor == FIELD_LOAD) {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Test Load\n");
	}
	if (FI->IntermediateWidgetMajor == FIELD_SAVE) {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Test Save\n");
	}
	if (FI->IntermediateWidgetMajor == FIELD_RESET) {
		bool res = GuIritMdlrDllGetAsyncInputConfirm(FI, "", "Are you sure you want to reset the texture ?");
		if (res) {
			IrtMdlrTexturePainterResizeTexture(FI, texture_size);
		}
	}
	GuIritMdlrDllGetInputParameter(FI, FIELD_TEXTURE_SIZE, &_texture_size);
	int tmp_size = (int)_texture_size;
	if (texture_size != tmp_size && !texture_reset_pending) {
		texture_reset_pending = true;
		bool res = true;
		if (texture_size != -1) {
			if (!reset_timer_init || duration_cast<seconds>(high_resolution_clock::now() - reset_timer).count() >= 10) {
				res = GuIritMdlrDllGetAsyncInputConfirm(FI, "", "This will reset the texture.\nAre you sure you want to resize the texture ?");
				if (res) {
					reset_timer_init = true;
					reset_timer = high_resolution_clock::now();
				}
			}
		}
		if (res) {
			IrtMdlrTexturePainterResizeTexture(FI, tmp_size);
		}
		else {
			IrtRType size = (IrtRType)texture_size;
			GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_SIZE, &size);
		}
		texture_reset_pending = false;
	}

	// Brush fields
	GuIritMdlrDllGetInputParameter(FI, FIELD_COLOR, &color);
	GuIritMdlrDllGetInputParameter(FI, FIELD_ALPHA, &alpha);
	GuIritMdlrDllGetInputParameter(FI, FIELD_X_FACTOR, &x_factor);
	GuIritMdlrDllGetInputParameter(FI, FIELD_Y_FACTOR, &y_factor);

	char* tmp = shape_names;
	char** ptr = &tmp;
	IrtRType tmp_irt;
	GuIritMdlrDllGetInputParameter(FI, FIELD_SHAPE, &tmp_irt, ptr);
	int index = irtrtype_to_i(tmp_irt);
	if (index != shape_index) {
		shape_index = index;
		IrtMdlrTexturePainterLoadShape(FI, shape_files[shape_index]);
	}
}

static void IrtMdlrTexturePainterResetAlphaBitmap() {
	vector<bool> bitrow(texture_size, false);
	vector<vector<bool>> bitmap(texture_size, bitrow);
	alpha_bitmap = bitmap;
}

static void IrtMdlrTexturePainterResizeTexture(IrtMdlrFuncInfoClass* FI, int new_size)
{	
	IrtImgPixelStruct* new_texture = new IrtImgPixelStruct[new_size * new_size];
	for (int i = 0; i < new_size; i++) {
		for (int j = 0; j < new_size; j++) {
			new_texture[i * new_size + j].r = 255;
			new_texture[i * new_size + j].g = 255;
			new_texture[i * new_size + j].b = 255;
		}
	}
	if (texture_size >= 0) {
		delete[] texture;
	}
	texture = new_texture;
	texture_size = new_size;
	IrtMdlrTexturePainterResetAlphaBitmap();
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture resized to %dx%d\n", new_size, new_size);
}

void IrtMdlrTexturePainterInitShapeHierarchy(IrtMdlrFuncInfoClass* FI)
{
	int index = 0;
	const char *p, *q;
	char base_path[IRIT_LINE_LEN_LONG], path[IRIT_LINE_LEN_LONG];
	const IrtDspGuIritSystemInfoStruct* system_props = GuIritMdlrDllGetGuIritSystemProps(FI);
	sprintf(path, "%s%s", searchpath(system_props->AuxiliaryDataName, base_path), RELATIVE_PATH);
	if (shape_files == NULL) {
		shape_files = GuIritMdlrDllGetAllFilesNamesInDirectory(FI, path, "*.rle|*.ppm|*.gif|*.jpeg|*.png");
	}

	strcpy(shape_names, "");
	for (int i = 0; shape_files[i] != NULL; ++i) {
		p = strstr(shape_files[i], RELATIVE_PATH);
		if (p != NULL) {
			p += strlen(RELATIVE_PATH) + 1;
			char shape_name[IRIT_LINE_LEN_LONG];
			strcpy(shape_name, "");
			q = strchr(p, '.');
			strncpy(shape_name, p, q - p);
			shape_name[q - p] = '\0';
			if (strstr(shape_name, "GaussianFull25") != NULL) {
				index = i;
			}
			sprintf(shape_names, "%s%s;", shape_names, shape_name);
		}
	}

	if (shape_files[0] != NULL) {
		sprintf(shape_names, "%s:%d", shape_names, index);
		GuIritMdlrDllSetInputSelectionStruct files_names(shape_names);
		GuIritMdlrDllSetInputParameter(FI, FIELD_SHAPE, &files_names);
	}
	else {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "Masks files were not found. Check directory: \"%s\"\n", path);
		return;
	}
}

static void IrtMdlrTexturePainterLoadShape(IrtMdlrFuncInfoClass* FI, const char* filename) {
	int alpha;
	IrtImgPixelStruct* image = IrtImgReadImage2(filename, &shape_width, &shape_height, &alpha);
	shape_width++;
	shape_height++;
	if (!shape_init) {
		delete[] shape_matrix;
	}
	else {
		shape_init = 0;
	}

	shape_matrix = new float[shape_height * shape_width];
	for (int i = 0; i < shape_height; i++) {
		for (int j = 0; j < shape_width; j++) {
			float gray;
			IrtImgPixelStruct* ptr = image + (i * shape_width + j);
			IRT_DSP_RGB2GREY(ptr, gray);
			shape_matrix[i * shape_width + j] = gray / 255.0;
		}
	}
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Loaded shape of size %dx%d\n", shape_width, shape_height);
}

static void IrtMdlrTexturePainterCalculateLine(const Pixel& start, const Pixel& end, vector<int>& points, const int min_y) {
	int a1, a2, b1, b2;
	int is_high;
	if (abs(end.x - start.x) > abs(end.y - start.y)) {
		a1 = start.x;
		b1 = start.y;
		a2 = end.x;
		b2 = end.y;
		is_high = 0;
	}
	else {
		a1 = start.y;
		b1 = start.x;
		a2 = end.y;
		b2 = end.x;
		is_high = 1;
	}
	if (a1 > a2) {
		swap(a1, a2);
		swap(b1, b2);
	}
	int da = a2 - a1;
	int db = b2 - b1;
	int n = 1;
	if (da < 0) {
		n *= -1;
		da *= -1;
	}
	if (db < 0) {
		n *= -1;
		db *= -1;
	}
	int d = 2 * db - da;
	int b = b1;
	for (int a = a1; a <= a2; a++) {
		int x = (is_high) ? b : a;
		int y = (is_high) ? a : b;
		points[y - min_y] = x;
		if (d > 0) {
			b += n;
			d -= 2 * da;
		}
		d += 2 * db;
	}
}

static void IrtMdlrTexturePainterRenderShape(IrtMdlrFuncInfoClass* FI, int x_offset, int y_offset) {
	int x_min = x_offset - shape_width / 2, y_min = y_offset - shape_height / 2;
	for (int yy = 0; yy < shape_height; yy++) {
		for (int xx = 0; xx < shape_width; xx++) {
			int x = (x_min + xx) % texture_size;
			int y = (y_min + yy) % texture_size;
			int texture_offset = x + texture_size * y;
			int shape_offset = xx + shape_width * yy;
			texture[texture_offset].r = (IrtBType)(shape_matrix[shape_offset]);
			texture[texture_offset].g = (IrtBType)(shape_matrix[shape_offset]);
			texture[texture_offset].b = (IrtBType)(shape_matrix[shape_offset]);
		}
	}
}

static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent)
{
	IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)MouseEvent->Data;
	IRT_DSP_STATIC_DATA int clicking = FALSE;

	IPObjectStruct* PObj = (IPObjectStruct*)MouseEvent->PObj;

	if (MouseEvent->KeyModifiers & IRT_DSP_KEY_MODIFIER_SHIFT_DOWN)
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
			IrtMdlrTexturePainterResetAlphaBitmap();
			break;
		}
		if (clicking) {
			if (MouseEvent->UV != NULL) {
				int x_offset = (int)((double)texture_size * fmod(MouseEvent->UV[0], 1));
				int y_offset = (int)((double)texture_size * fmod(MouseEvent->UV[1], 1));
				IrtMdlrTexturePainterRenderShape(FI, x_offset, y_offset);
				GuIritMdlrDllSetTextureFromImage(FI, PObj, texture, texture_size, texture_size, FALSE);
			}
		}
	}
	return TRUE;
}
