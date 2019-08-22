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
IRT_DSP_STATIC_DATA const int DEFAULT_TEXTURE_SIZE = 256;

typedef struct {
	double x;
	double y;
} Point;

typedef struct {
	int x;
	int y;
} Pixel;

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrTexturePainterResizeTexture(IrtMdlrFuncInfoClass* FI, int new_width, int new_height);
static void IrtMdlrTexturePainterLoadShape(IrtMdlrFuncInfoClass* FI, const char* filename);
static void IrtMdlrTexturePainterInitShapeHierarchy(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrTexturePainterCalculateLine(const Pixel& start, const Pixel& end, vector<int>& points);
static void IrtMdlrTexturePainterRenderShape(IrtMdlrFuncInfoClass* FI, int x_offset, int y_offset);
static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);

IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

IRT_DSP_STATIC_DATA IrtRType _texture_width = DEFAULT_TEXTURE_SIZE;
IRT_DSP_STATIC_DATA IrtRType _texture_height = DEFAULT_TEXTURE_SIZE;
IRT_DSP_STATIC_DATA int texture_width = -1;
IRT_DSP_STATIC_DATA int texture_height = -1;
IRT_DSP_STATIC_DATA bool texture_reset_pending = false;
IRT_DSP_STATIC_DATA high_resolution_clock::time_point reset_timer;
IRT_DSP_STATIC_DATA bool reset_timer_init = false;
IRT_DSP_STATIC_DATA IrtImgPixelStruct* texture;
IRT_DSP_STATIC_DATA IrtImgPixelStruct* texture_alpha;
IRT_DSP_STATIC_DATA IrtImgPixelStruct* texture_buffer;

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

IRT_DSP_STATIC_DATA IPObjectStruct* last_obj = NULL;
IRT_DSP_STATIC_DATA int last_x_offset = -1;
IRT_DSP_STATIC_DATA int last_y_offset = -1;

typedef enum {
	FIELD_LOAD = 0,
	FIELD_SAVE,
	FIELD_RESET,
	FIELD_TEXTURE_WIDTH,
	FIELD_TEXTURE_HEIGHT,
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
		10,
		IRT_MDLR_PARAM_EXACT,
		{
			// Texture fields
			IRT_MDLR_BUTTON_EXPR,				// Load Texture
			IRT_MDLR_BUTTON_EXPR,				// Save Texture
			IRT_MDLR_BUTTON_EXPR,				// Reset Texture
			IRT_MDLR_NUMERIC_EXPR,				// Texture width
			IRT_MDLR_NUMERIC_EXPR,				// Texture height

			// Brush fields
			IRT_MDLR_VECTOR_EXPR,				// RGB Values
			IRT_MDLR_NUMERIC_EXPR,				// Alpha Value
			IRT_MDLR_HIERARCHY_SELECTION_EXPR,	// Shape selection menu
			IRT_MDLR_NUMERIC_EXPR,				// X Factor
			IRT_MDLR_NUMERIC_EXPR,				// Y Factor
		},
		{
			NULL,
			NULL, 
			NULL,
			& _texture_width,
			& _texture_height,

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
			"Texture Width",
			"Texture Height",

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
			"Width of the texture.",
			"Height of the texture.",

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

static double diff(double a, double b) {
	return fabs(a - b);
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

		GuIritMdlrDllSetRealInputDomain(FI, 1, IRIT_INFNTY, FIELD_TEXTURE_WIDTH);
		GuIritMdlrDllSetRealInputDomain(FI, 1, IRIT_INFNTY, FIELD_TEXTURE_HEIGHT);
		GuIritMdlrDllSetRealInputDomain(FI, 0, 255, FIELD_COLOR, 0);
		GuIritMdlrDllSetRealInputDomain(FI, 0, 255, FIELD_COLOR, 1);
		GuIritMdlrDllSetRealInputDomain(FI, 0, 255, FIELD_COLOR, 2);
		GuIritMdlrDllSetRealInputDomain(FI, 0, 255, FIELD_ALPHA);
		GuIritMdlrDllSetRealInputDomain(FI, 0.01, IRIT_INFNTY, FIELD_X_FACTOR);
		GuIritMdlrDllSetRealInputDomain(FI, 0.01, IRIT_INFNTY, FIELD_Y_FACTOR);

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
		char* filename;
		bool res = GuIritMdlrDllGetAsyncInputFileName(FI, "Load Texture from....", "*.png", &filename);
		if (res) {
			int alpha, width, height;
			IrtImgPixelStruct* image = IrtImgReadImage2(filename, &width, &height, &alpha);
			width++;
			height++;
			GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture loaded successfully from \"%s\" (%dx%d)\n", filename, width, height);
			IrtMdlrTexturePainterResizeTexture(FI, width, height);
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					int offset = y * width + x;
					texture[offset].r = image[offset].r;
					texture[offset].g = image[offset].g;
					texture[offset].b = image[offset].b;
					texture_alpha[offset].r = image[offset].r;
					texture_alpha[offset].g = image[offset].g;
					texture_alpha[offset].b = image[offset].b;
					texture_buffer[offset].r = image[offset].r;
					texture_buffer[offset].g = image[offset].g;
					texture_buffer[offset].b = image[offset].b;
				}
			}
			if (last_obj != NULL) {
				GuIritMdlrDllSetTextureFromImage(FI, last_obj, texture, texture_width, texture_height, FALSE);
			}
			else {
				GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING, "Could not display loaded texture - no active object!\n");
			}
			texture_reset_pending = true;
			IrtRType _width = (IrtRType)width;
			IrtRType _height = (IrtRType)height;
			GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_WIDTH, &_width);
			GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &_height);
			texture_reset_pending = false;
		}
	}

	if (FI->IntermediateWidgetMajor == FIELD_SAVE) {
		char* filename;
		bool res = GuIritMdlrDllGetAsyncInputFileName(FI, "Save Texture to....", "*.png", &filename);
		if (res) {
			MiscWriteGenInfoStructPtr GI = IrtImgWriteOpenFile(NULL, filename, IRIT_IMAGE_PNG_TYPE, false, texture_width, texture_height);
			if (GI == NULL) {
				GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "Error saving texture to \"%s\"\n", filename);
			}
			else {
				for (int y = 0; y < texture_height; y++) {
					IrtImgWritePutLine(GI, NULL, &texture[y * texture_width]);
				}
				IrtImgWriteCloseFile(GI);
				GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture saved successfully to \"%s\"\n", filename);
			}
		}
	}

	if (FI->IntermediateWidgetMajor == FIELD_RESET) {
		bool res = GuIritMdlrDllGetAsyncInputConfirm(FI, "", "Are you sure you want to reset the texture ?");
		if (res) {
			IrtMdlrTexturePainterResizeTexture(FI, DEFAULT_TEXTURE_SIZE, DEFAULT_TEXTURE_SIZE);
			texture_reset_pending = true;
			IrtRType _width = (IrtRType)DEFAULT_TEXTURE_SIZE;
			IrtRType _height = (IrtRType)DEFAULT_TEXTURE_SIZE;
			GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_WIDTH, &_width);
			GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &_height);
			texture_reset_pending = false;
			if (last_obj != NULL) {
				GuIritMdlrDllSetTextureFromImage(FI, last_obj, texture, texture_width, texture_height, FALSE);
			}
			else {
				GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING, "Could not display loaded texture - no active object!\n");
			}
		}
	}

	GuIritMdlrDllGetInputParameter(FI, FIELD_TEXTURE_WIDTH, &_texture_width);
	GuIritMdlrDllGetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &_texture_height);
	int tmp_width = (int)_texture_width;
	int tmp_height = (int)_texture_height;
	if ((texture_width != tmp_width || texture_height != tmp_height) && !texture_reset_pending) {
		texture_reset_pending = true;
		bool res = true;
		bool init = texture_width == -1;
		if (texture_width != -1) {
			if (!reset_timer_init || duration_cast<seconds>(high_resolution_clock::now() - reset_timer).count() >= 10) {
				res = GuIritMdlrDllGetAsyncInputConfirm(FI, "", "This will reset the texture.\nAre you sure you want to resize the texture ?");
				if (res) {
					reset_timer_init = true;
					reset_timer = high_resolution_clock::now();
				}
			}
		}
		if (res) {
			IrtMdlrTexturePainterResizeTexture(FI, tmp_width, tmp_height);
			if (last_obj != NULL) {
				GuIritMdlrDllSetTextureFromImage(FI, last_obj, texture, texture_width, texture_height, FALSE);
			}
			else if (!init) {
				GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING, "Could not display loaded texture - no active object!\n");
			}
		}
		else {
			IrtRType width = (IrtRType)texture_width;
			IrtRType height = (IrtRType)texture_height;
			GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_WIDTH, &width);
			GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &height);
		}
		texture_reset_pending = false;
	}

	// Brush fields
	GuIritMdlrDllGetInputParameter(FI, FIELD_COLOR, &color);
	GuIritMdlrDllGetInputParameter(FI, FIELD_ALPHA, &alpha);

	IrtRType tmp_x, tmp_y;

	GuIritMdlrDllGetInputParameter(FI, FIELD_X_FACTOR, &tmp_x);
	GuIritMdlrDllGetInputParameter(FI, FIELD_Y_FACTOR, &tmp_y);

	if (diff(tmp_x, x_factor) > 10e-5 || diff(tmp_y, y_factor) > 10e-5) {
		x_factor = tmp_x;
		y_factor = tmp_y;
		IrtMdlrTexturePainterLoadShape(FI, shape_files[shape_index]);
	}

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

static void IrtMdlrTexturePainterResizeTexture(IrtMdlrFuncInfoClass* FI, int new_width, int new_height)
{	
	if (texture_width >= 0) {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Deleting old textures... ");
		delete[] texture;
		delete[] texture_alpha;
		delete[] texture_buffer;
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Done\n");
	}
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Allocating new textures... ");
	IrtImgPixelStruct* new_texture = new IrtImgPixelStruct[new_width * new_height];
	IrtImgPixelStruct* new_texture_alpha = new IrtImgPixelStruct[new_width * new_height];
	IrtImgPixelStruct* new_texture_buffer = new IrtImgPixelStruct[new_width * new_height];
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Done\n");
	for (int y = 0; y < new_height; y++) {
		for (int x = 0; x < new_width; x++) {
			new_texture[y * new_width + x].r = 255;
			new_texture[y * new_width + x].g = 255;
			new_texture[y * new_width + x].b = 255;
			new_texture_alpha[y * new_width + x].r = 255;
			new_texture_alpha[y * new_width + x].g = 255;
			new_texture_alpha[y * new_width + x].b = 255;
			new_texture_buffer[y * new_width + x].r = 255;
			new_texture_buffer[y * new_width + x].g = 255;
			new_texture_buffer[y * new_width + x].b = 255;
		}
	}
	texture = new_texture;
	texture_alpha = new_texture_alpha;
	texture_buffer = new_texture_buffer;
	texture_width = new_width;
	texture_height = new_height;
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture resized to %dx%d\n", new_width, new_height);
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
	int alpha, width, height;
	IrtImgPixelStruct* image = IrtImgReadImage2(filename, &width, &height, &alpha);
	width++;
	height++;
	if (!shape_init) {
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Deleting old shape... ");
		delete[] shape_matrix;
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Done\n");
	}
	else {
		shape_init = 0;
	}

	shape_width = (int)(width * x_factor);
	shape_height = (int)(height * y_factor);

	float x_ratio = (float)width / (float)shape_width;
	float y_ratio = (float)height / (float)shape_height;

	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Allocating new shape... ");
	shape_matrix = new float[shape_height * shape_width];
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Done\n");
	for (int y = 0; y < shape_height; y++) {
		for (int x = 0; x < shape_width; x++) {
			float gray;
			IrtImgPixelStruct* ptr = image + (int)((int)(y * y_ratio) * width + x * x_ratio);
			IRT_DSP_RGB2GREY(ptr, gray);
			shape_matrix[y * shape_width + x] = (float)(gray / 255.0);
		}
	}
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Loaded shape of size %dx%d\n", shape_width, shape_height);
}

static void IrtMdlrTexturePainterCalculateLine(const Pixel& start, const Pixel& end, vector<Pixel>& points) {
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
		Pixel p = { x, y };
		points.push_back(p);
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
			int x = (x_min + xx) % texture_width;
			int y = (y_min + yy) % texture_height;
			int texture_offset = x + texture_width * y;
			int shape_offset = xx + shape_width * yy;
			texture_alpha[texture_offset].r += (IrtBType)((color[0] - texture_alpha[texture_offset].r) * shape_matrix[shape_offset]);
			texture_alpha[texture_offset].g += (IrtBType)((color[1] - texture_alpha[texture_offset].g) * shape_matrix[shape_offset]);
			texture_alpha[texture_offset].b += (IrtBType)((color[2] - texture_alpha[texture_offset].b) * shape_matrix[shape_offset]);
			texture[texture_offset].r = (IrtBType)(texture_buffer[texture_offset].r + 
				(texture_alpha[texture_offset].r - texture_buffer[texture_offset].r) * (alpha / 255.0));
			texture[texture_offset].g = (IrtBType)(texture_buffer[texture_offset].g +
				(texture_alpha[texture_offset].g - texture_buffer[texture_offset].g) * (alpha / 255.0));
			texture[texture_offset].b = (IrtBType)(texture_buffer[texture_offset].b +
				(texture_alpha[texture_offset].b - texture_buffer[texture_offset].b) * (alpha / 255.0));
		}
	}	
}

static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent)
{
	IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)MouseEvent->Data;
	IRT_DSP_STATIC_DATA int clicking = FALSE;

	IPObjectStruct* PObj = (IPObjectStruct*)MouseEvent->PObj;
	last_obj = PObj;

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
			for (int y = 0; y < texture_height; y++) {
				for (int x = 0; x < texture_width; x++) {
					int offset = y * texture_width + x;
					texture_alpha[offset].r = texture[offset].r;
					texture_alpha[offset].g = texture[offset].g;
					texture_alpha[offset].b = texture[offset].b;
					texture_buffer[offset].r = texture[offset].r;
					texture_buffer[offset].g = texture[offset].g;
					texture_buffer[offset].b = texture[offset].b;
				}
			}
			last_x_offset = -1;
			last_y_offset = -1;
			break;
		}
		if (clicking && MouseEvent->UV != NULL) {
			int x_offset = (int)((double)texture_width * fmod(MouseEvent->UV[0], 1));
			int y_offset = (int)((double)texture_height * fmod(MouseEvent->UV[1], 1));
			if (last_y_offset == -1) {
				IrtMdlrTexturePainterRenderShape(FI, x_offset, y_offset);
				last_x_offset = x_offset;
				last_y_offset = y_offset;
			}
			else {
				int x_backup = x_offset;
				int y_backup = y_offset;
				if (diff(last_x_offset, x_offset - texture_width) < diff(last_x_offset, x_offset)) {
					x_offset -= texture_width;
				}
				if (diff(last_x_offset, x_offset + texture_width) < diff(last_x_offset, x_offset)) {
					x_offset += texture_width;
				}
				if (diff(last_y_offset, y_offset - texture_height) < diff(last_y_offset, y_offset)) {
					y_offset -= texture_height;
				}
				if (diff(last_y_offset, y_offset + texture_height) < diff(last_y_offset, y_offset)) {
					y_offset += texture_height;
				}
				vector<Pixel> points;
				Pixel start = { last_x_offset, last_y_offset }, end = { x_offset, y_offset };
				IrtMdlrTexturePainterCalculateLine(start, end, points);
				for (Pixel& p : points) {
					IrtMdlrTexturePainterRenderShape(FI, p.x % texture_width, p.y % texture_height);
				}
				last_x_offset = x_backup;
				last_y_offset = y_backup;
			}
			GuIritMdlrDllSetTextureFromImage(FI, PObj, texture, texture_width, texture_height, FALSE);
		}
	}
	return TRUE;
}
