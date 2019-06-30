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
#include <algorithm>
#include <math.h>

using std::string;
using std::vector;
using std::swap;

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
static void IrtMdlrTexturePainterInitTexture();
static void IrtMdlrTexturePainterInitShapeHierarchy(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrTexturePainterCalculateLine(const Pixel& start, const Pixel& end, vector<int>& points, const int min_y);
static void IrtMdlrTexturePainterRenderShape(IrtMdlrFuncInfoClass* FI, int x, int y);
static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);

IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

IRT_DSP_STATIC_DATA IrtVecType color = { 0, 0, 0 };
IRT_DSP_STATIC_DATA IrtRType alpha = 128;
IRT_DSP_STATIC_DATA char alpha_mode_selection[IRIT_LINE_LEN_LONG] = "Regular;Stabilized;:0";
IRT_DSP_STATIC_DATA int stabilized_alpha = 0;
IRT_DSP_STATIC_DATA IrtRType size = 64;
IRT_DSP_STATIC_DATA IrtRType rotation = 0;

IRT_DSP_STATIC_DATA const char SHAPE_FILE_RELATIVE_PATH[IRIT_LINE_LEN_LONG] = "\\Example\\Shape";
IRT_DSP_STATIC_DATA char shape_selection[IRIT_LINE_LEN_LONG] = "";
IRT_DSP_STATIC_DATA vector<vector<Point>> shapes;
IRT_DSP_STATIC_DATA int shape_index = 0;


IRT_DSP_STATIC_DATA const int TEXTURE_SIZE = 1024;
IRT_DSP_STATIC_DATA IrtImgPixelStruct texture[TEXTURE_SIZE][TEXTURE_SIZE];
IRT_DSP_STATIC_DATA vector<vector<bool>> alpha_bitmap;

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
			7,
			IRT_MDLR_PARAM_EXACT,
		{
			IRT_MDLR_BUTTON_EXPR, // Color picker
			IRT_MDLR_VECTOR_EXPR, // RGB Values
			IRT_MDLR_NUMERIC_EXPR, // Alpha Value
			IRT_MDLR_HIERARCHY_SELECTION_EXPR, // Alpha mode selection menu
			IRT_MDLR_HIERARCHY_SELECTION_EXPR, // Shape selection menu
			IRT_MDLR_NUMERIC_EXPR, // Size
			IRT_MDLR_NUMERIC_EXPR, // Rotation
		},
		{
			NULL,
			&color,
			&alpha,
			alpha_mode_selection,
			shape_selection,
			&size,
			&rotation
		},
		{
			"Color Picker",
			"Color",
			"Alpha",
			"Alpha Mode",
			"Shape",
			"Size",
			"Rotation"
		},
		{
			"Pick the color of the painter.",
			"RGB values of the painter.",
			"Alpha channel value of the painter.",
			"Defines the way alpha channel is applied.",
			"Shape of the painting brush.",
			"Size of the painting brush.",
			"Rotation angle of the painting brush.",
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
	if (FI->InvocationNumber == 0 && GlobalFI == NULL) {

		IrtMdlrTexturePainterInitTexture();
		IrtMdlrTexturePainterInitShapeHierarchy(FI);
		GuIritMdlrDllSetInputSelectionStruct modes(alpha_mode_selection);
		GuIritMdlrDllSetInputParameter(FI, 3, &modes);

		GuIritMdlrDllPushMouseEventFunc(
			FI,
			IrtMdlrTexturePainterMouseCallBack,
			(IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
			(IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_CTRL_DOWN | IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
			FI);
		GlobalFI = FI;
	}

	GuIritMdlrDllGetInputParameter(FI, 1, &color);
	GuIritMdlrDllGetInputParameter(FI, 2, &alpha);

	char* tmp = alpha_mode_selection;
	char** ptr = &tmp;
	IrtRType tmp_index;
	GuIritMdlrDllGetInputParameter(FI, 3, &tmp_index, ptr);
	stabilized_alpha = irtrtype_to_i(tmp_index);

	tmp = shape_selection;
	GuIritMdlrDllGetInputParameter(FI, 4, &tmp_index, ptr);
	shape_index = irtrtype_to_i(tmp_index);

	GuIritMdlrDllGetInputParameter(FI, 5, &size);
	GuIritMdlrDllGetInputParameter(FI, 6, &rotation);

	if (FI->IntermediateWidgetMajor == 0) {
		unsigned char cr = (unsigned char)color[0];
		unsigned char cg = (unsigned char)color[1];
		unsigned char cb = (unsigned char)color[2];
		if (GuIritMdlrDllGetAsyncInputColor(FI, &cr, &cg, &cb)) {
			color[0] = (double)cr;
			color[1] = (double)cg;
			color[2] = (double)cb;
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

static void IrtMdlrTexturePainterResetAlphaBitmap() {
	vector<bool> bitrow(TEXTURE_SIZE, false);
	vector<vector<bool>> bitmap(TEXTURE_SIZE, bitrow);
	alpha_bitmap = bitmap;
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
	IrtMdlrTexturePainterResetAlphaBitmap();
}

static void IrtMdlrTexturePainterInitShapeHierarchy(IrtMdlrFuncInfoClass* FI)
{
	const IrtDspGuIritSystemInfoStruct* sys_file_names = GuIritMdlrDllGetGuIritSystemProps(FI);
	char file_path[IRIT_LINE_LEN_LONG], search_path[IRIT_LINE_LEN_LONG];
	const char** shape_files = NULL;
	sprintf(file_path, "%s%s", searchpath(sys_file_names->AuxiliaryDataName, search_path), SHAPE_FILE_RELATIVE_PATH);
	shape_files = GuIritMdlrDllGetAllFilesNamesInDirectory(FI, file_path, "*.itd");
	if (shape_files == NULL || shape_files[0] == NULL) {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "Shape files not found. Check directory: \"%s\"\n", file_path);
		return;
	}
	strcpy(shape_selection, "");
	for (int i = 0; shape_files[i] != NULL; ++i) {
		const char* p = strstr(shape_files[i], SHAPE_FILE_RELATIVE_PATH);
		const char* q = strchr(p, '.');
		if (p != NULL) {
			p += strlen(SHAPE_FILE_RELATIVE_PATH) + 1;
			char name[IRIT_LINE_LEN_LONG];
			strcpy(name, "");
			strncpy(name, p, q - p);
			name[q - p] = '\0';
			sprintf(shape_selection, "%s%s;", shape_selection, name);
			if (shapes.size() <= i) {
				GuIritMdlrDllLoadFile(FI, shape_files[i]);
				IPObjectStruct* PObj = GuIritMdlrDllGetObjByName(FI, name);
				IPVertexStruct* vertex = PObj->U.Pl->PVertex;
				IPVertexStruct* first = vertex;
				vector<Point> points;
				while (true) {
					if (vertex == NULL) {
						break;
					}
					if (points.size() > 1 && vertex == first) {
						break;
					}
					Point p = { vertex->Coord[0], vertex->Coord[1] };
					points.push_back(p);
					vertex = vertex->Pnext;
				}
				shapes.push_back(points);
				GuIritMdlrDllSetObjectName(FI, PObj, name);
				GuIritMdlrDllSetObjectVisible(FI, PObj, false);
			}
		}
	}
	sprintf(shape_selection, "%s:0", shape_selection);
	GuIritMdlrDllSetInputSelectionStruct names(shape_selection, IRIT_MAX_INT, file_path);
	GuIritMdlrDllSetInputParameter(FI, 4, &names);
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

static void IrtMdlrTexturePainterRenderShape(IrtMdlrFuncInfoClass* FI, int x, int y) {
	int first, last;
	int min_y = TEXTURE_SIZE, max_y = 0;
	int i = 0;

	// Convert shape coordinates to texture coordinates with scale
	vector<Pixel> points;
	for (const Point& point : shapes[shape_index]) {
		double theta = (rotation + 90.0) * PI / 180.0;
		Pixel p = {
			((point.x - 0.5) * cos(theta) - (point.y - 0.5) * sin(theta)) * size + (double)x,
			((point.x - 0.5) * sin(theta) + (point.y - 0.5) * cos(theta)) * size + (double)y
		};
		/*GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "(%lf, %lf)\n", ((point.x - 0.5) * cos(theta) - (point.y - 0.5) * sin(theta)) * size + (double)x, ((point.x - 0.5) * sin(theta) + (point.y - 0.5) * cos(theta)) * size + (double)y);
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "(%d, %d)\n", p.x, p.y);*/
		points.push_back(p);
	}

	// Find starting point and end point for scan conversion
	for (const Pixel& p : points) {
		if (p.y < min_y) {
			first = i;
			min_y = p.y;
		}
		if (p.y > max_y) {
			last = i;
			max_y = p.y;
		}
		i++;
	}

	// Calculate coordinates of lines
	int l = first, r = first;
	int ll = (l - 1 < 0) ? i - 1 : l - 1;
	int rr = (r + 1 >= i) ? 0 : r + 1;
	vector<int> left(max_y - min_y + 1), right(max_y - min_y + 1);
	while (true) {
		IrtMdlrTexturePainterCalculateLine(points[l], points[ll], left, min_y);
		if (ll == last) {
			break;
		}
		l = (l - 1 < 0) ? i - 1 : l - 1;
		ll = (ll - 1 < 0) ? i - 1 : ll - 1;
	}
	while (true) {
		IrtMdlrTexturePainterCalculateLine(points[r], points[rr], right, min_y);
		if (rr == last) {
			break;
		}
		r = (r + 1 >= i) ? 0 : r + 1;
		rr = (rr + 1 >= i) ? 0 : rr + 1;
	}

	// Scan conversion
	for (int y = min_y; y <= max_y; y++) {
		Pixel p1 = { left[y - min_y], y };
		Pixel p2 = { right[y - min_y], y };
		for (int x = p1.x; x <= p2.x; x++) {
			if (x >= 0 && x < TEXTURE_SIZE && y >= 0 && y < TEXTURE_SIZE) {
				double alpha_factor = (255.0 - alpha) / 255.0;
				if (!stabilized_alpha || !alpha_bitmap[y][x]) {
					texture[y][x].r += (double)((IrtBType)color[0] - texture[y][x].r) * alpha_factor;
					texture[y][x].g += (double)((IrtBType)color[1] - texture[y][x].g) * alpha_factor;
					texture[y][x].b += (double)((IrtBType)color[2] - texture[y][x].b) * alpha_factor;
					alpha_bitmap[y][x] = true;
				}
			}
		}
	}
}

static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent)
{
	IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)MouseEvent->Data;
	IRT_DSP_STATIC_DATA int clicking = FALSE;

	IPObjectStruct* PObj = (IPObjectStruct*)MouseEvent->PObj;

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
			IrtMdlrTexturePainterResetAlphaBitmap();
			break;
		}
		if (clicking) {
			if (MouseEvent->UV != NULL) {
				int x_offset = (float)TEXTURE_SIZE * (fmod(MouseEvent->UV[0], 1));
				int y_offset = (float)TEXTURE_SIZE * (fmod(MouseEvent->UV[1], 1));
				IrtMdlrTexturePainterRenderShape(FI, x_offset, y_offset);
				GuIritMdlrDllSetTextureFromImage(FI, PObj, texture, TEXTURE_SIZE, TEXTURE_SIZE, FALSE);
			}
		}
	}
	return TRUE;
}
