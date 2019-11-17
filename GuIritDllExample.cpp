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

#define DEFAULT_WIDTH 256
#define DEFAULT_HEIGHT 256
#define SRF_PAINT_DFLT_SIZE (DEFAULT_WIDTH)*(DEFAULT_HEIGHT)

#define D(a, b) (fabs((double)(a) - (double)(b)))

typedef struct {
    int width;
    int height;
    IrtImgPixelStruct* texture;
} Texture;

static void IrtMdlrSrfPaint(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrSrfPaintResizeTexture(IrtMdlrFuncInfoClass* FI,
                        int Width,
                        int Height,
                        bool Reset);
static void IrtMdlrSrfPaintInitShapes(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrSrfPaintLoadShape(IrtMdlrFuncInfoClass* FI,
                        const char* Filename);
static void IrtMdlrSrfPaintBresenham(const pair<int, int>& a,
                        const pair<int, int>& b,
                        vector<int>& Points);
static void IrtMdlrSrfPaintRenderShape(IrtMdlrFuncInfoClass* FI,
                        int XOff,
                        int YOff);
static int IrtMdlrSrfPaintMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);

IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

IRT_DSP_STATIC_DATA IPObjectStruct* active_surface = NULL;

IRT_DSP_STATIC_DATA int texture_width = 256;
IRT_DSP_STATIC_DATA int texture_height = 256;
IRT_DSP_STATIC_DATA high_resolution_clock::time_point reset_timer;
IRT_DSP_STATIC_DATA bool reset_timer_init = false;
IRT_DSP_STATIC_DATA map<IPObjectStruct*, Texture*> textures;
IRT_DSP_STATIC_DATA IrtImgPixelStruct* texture_alpha;
IRT_DSP_STATIC_DATA IrtImgPixelStruct* texture_buffer;

IRT_DSP_STATIC_DATA bool texture_reset_flag = false;
IRT_DSP_STATIC_DATA bool init_done_flag = false;


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

IRT_DSP_STATIC_DATA int last_x_offset = -1;
IRT_DSP_STATIC_DATA int last_y_offset = -1;

typedef enum {
    FIELD_SURFACE = 0,
    FIELD_LOAD,
    FIELD_SAVE,
    FIELD_RESET,
    FIELD_TEXTURE_WIDTH,
    FIELD_TEXTURE_HEIGHT,
    FIELD_COLOR,
    FIELD_ALPHA,
    FIELD_SHAPE,
    FIELD_X_FACTOR,
    FIELD_Y_FACTOR
};

IRT_DSP_STATIC_DATA IrtMdlrFuncTableStruct TexturePainterFunctionTable[] =
{
    {
        0,
        IconExample,
        "IRT_MDLR_EXAMPLE_BEEP",
        "Painter",
        "Texture Painter",
        "This is a texture painter",
        IrtMdlrSrfPaint,
        NULL,
        IRT_MDLR_PARAM_VIEW_CHANGES_UDPATE
        | IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_DFLT_ON,
        IRT_MDLR_NUMERIC_EXPR,
        11,
        IRT_MDLR_PARAM_EXACT,
        {
    // Surface selection
    IRT_MDLR_SURFACE_EXPR,

    // Texture fields
    IRT_MDLR_BUTTON_EXPR,				// Load Texture
    IRT_MDLR_BUTTON_EXPR,				// Save Texture
    IRT_MDLR_BUTTON_EXPR,				// Reset Texture
    IRT_MDLR_INTEGER_EXPR,				// Texture width
    IRT_MDLR_INTEGER_EXPR,				// Texture height

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
    NULL,
    &texture_width,
    &texture_height,

    &color,
    &alpha,
    shape_names,
    &x_factor,
    &y_factor,
},
{
    "Surface",

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
    "Select a surface.",

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

IRT_DSP_STATIC_DATA const int FUNC_TABLE_SIZE =
sizeof(TexturePainterFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

extern "C" bool _IrtMdlrDllRegister(void)
{
    GuIritMdlrDllRegister(TexturePainterFunctionTable, FUNC_TABLE_SIZE, "Example", IconMenuExample);
    return true;
}

static void IrtMdlrSrfPaint(IrtMdlrFuncInfoClass* FI)
{
    if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_INIT) {
        return;
    }

    if (FI->InvocationNumber == 0 && GlobalFI == NULL) {
        GuIritMdlrDllPushMouseEventFunc(
            FI,
            IrtMdlrSrfPaintMouseCallBack,
            (IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
            (IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
            FI);
        IrtMdlrSrfPaintInitShapes(FI);

        GuIritMdlrDllSetIntInputDomain(FI, 1, IRIT_MAX_INT, FIELD_TEXTURE_WIDTH);
        GuIritMdlrDllSetIntInputDomain(FI, 1, IRIT_MAX_INT, FIELD_TEXTURE_HEIGHT);
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
        FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_CANCEL)) {
        GuIritMdlrDllPopMouseEventFunc(FI);
        GlobalFI = false;
        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture Painter unloaded\n");
    }

    IPObjectStruct* surface = NULL;
    GuIritMdlrDllGetInputParameter(FI, FIELD_SURFACE, &surface);
    if (surface != NULL && !texture_reset_flag) {
        texture_reset_flag = true;
        if (textures.find(surface) == textures.end()) {
            Texture* new_texture = new Texture;
            new_texture->width = DEFAULT_WIDTH;
            new_texture->height = DEFAULT_HEIGHT;
            new_texture->texture = new IrtImgPixelStruct[SRF_PAINT_DFLT_SIZE];
            for (int i = 0; i < SRF_PAINT_DFLT_SIZE; i++) {
                new_texture->texture[i].r = 255;
                new_texture->texture[i].g = 255;
                new_texture->texture[i].b = 255;
            }
            textures[surface] = new_texture;
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Surface %s initialized\n", surface->ObjName);
        }
        if (surface != active_surface) {
            Texture* texture = textures[surface];
            GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_WIDTH, &texture->width);
            GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &texture->height);
            active_surface = surface;
            texture_alpha = new IrtImgPixelStruct[SRF_PAINT_DFLT_SIZE];
            texture_buffer = new IrtImgPixelStruct[SRF_PAINT_DFLT_SIZE];
            for (int i = 0; i < SRF_PAINT_DFLT_SIZE; i++) {
                texture_alpha[i] = texture->texture[i];
                texture_buffer[i] = texture->texture[i];
            }
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Surface %s selected\n", active_surface->ObjName);
            init_done_flag = true;
        }
        texture_reset_flag = false;
    }

    // Texture fields
    if (FI->IntermediateWidgetMajor == FIELD_LOAD && !texture_reset_flag) {
        if (active_surface != NULL) {
            char* filename;
            bool res = GuIritMdlrDllGetAsyncInputFileName(FI, "Load Texture from....", "*.png", &filename);
            if (res) {
                int alpha, width, height;
                IrtImgPixelStruct* image = IrtImgReadImage2(filename, &width, &height, &alpha);
                width++;
                height++;
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture loaded successfully from \"%s\" (%dx%d)\n", filename, width, height);
                IrtMdlrSrfPaintResizeTexture(FI, width, height, false);
                Texture* texture = textures[surface];
                for (int i = 0; i < width * height; i++) {
                    texture->texture[i] = image[i];
                    texture_alpha[i] = image[i];
                    texture_buffer[i] = image[i];
                }
                GuIritMdlrDllSetTextureFromImage(FI, active_surface, texture->texture, texture->width, texture->height, FALSE);
                texture_reset_flag = true;
                GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_WIDTH, &texture->width);
                GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &texture->height);
                texture_reset_flag = false;
            }
        }
        else {
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING, "Please select a surface.\n");
        }
    }

    if (FI->IntermediateWidgetMajor == FIELD_SAVE) {
        if (active_surface != NULL) {
            char* filename;
            bool res = GuIritMdlrDllGetAsyncInputFileName(FI, "Save Texture to....", "*.png", &filename);
            if (res) {
                Texture* texture = textures[surface];
                MiscWriteGenInfoStructPtr GI = IrtImgWriteOpenFile(NULL, filename, IRIT_IMAGE_PNG_TYPE, false, texture->width, texture->height);
                if (GI == NULL) {
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "Error saving texture to \"%s\"\n", filename);
                }
                else {
                    for (int y = 0; y < texture->height; y++) {
                        IrtImgWritePutLine(GI, NULL, &texture->texture[y * texture->width]);
                    }
                    IrtImgWriteCloseFile(GI);
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture saved successfully to \"%s\"\n", filename);
                }
            }
        }
        else {
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING, "Please select a surface.\n");
        }
    }

    if (FI->IntermediateWidgetMajor == FIELD_RESET && !texture_reset_flag) {
        if (active_surface != NULL) {
            bool res = GuIritMdlrDllGetAsyncInputConfirm(FI, "", "Are you sure you want to reset the texture ?");
            if (res) {
                Texture* texture = textures[surface];
                IrtMdlrSrfPaintResizeTexture(FI, DEFAULT_WIDTH, DEFAULT_HEIGHT, true);
                texture_reset_flag = true;
                int width = 256, height = 256;
                GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_WIDTH, &width);
                GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &height);
                texture_reset_flag = false;
                GuIritMdlrDllSetTextureFromImage(FI, active_surface, texture->texture, texture->width, texture->height, FALSE);
            }
        }
        else {
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING, "Please select a surface.\n");
        }
    }

    GuIritMdlrDllGetInputParameter(FI, FIELD_TEXTURE_WIDTH, &texture_width);
    GuIritMdlrDllGetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &texture_height);
    if (active_surface != NULL) {
        Texture* texture = textures[surface];
        if ((texture->width != texture_width || texture->height != texture_height) && !texture_reset_flag) {
            texture_reset_flag = true;
            bool res = true;
            if (init_done_flag) {
                if (!reset_timer_init || duration_cast<seconds>(high_resolution_clock::now() - reset_timer).count() >= 10) {
                    res = GuIritMdlrDllGetAsyncInputConfirm(FI, "", "This will reset the texture.\nAre you sure you want to resize the texture ?");
                    if (res) {
                        reset_timer_init = true;
                        reset_timer = high_resolution_clock::now();
                    }
                }
            }
            if (res) {
                IrtMdlrSrfPaintResizeTexture(FI, texture_width, texture_height, true);
                GuIritMdlrDllSetTextureFromImage(FI, active_surface, texture->texture, texture->width, texture->height, FALSE);
            }
            else {
                GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_WIDTH, &texture->width);
                GuIritMdlrDllSetInputParameter(FI, FIELD_TEXTURE_HEIGHT, &texture->height);
            }
            texture_reset_flag = false;
        }
    }

    // Brush fields
    GuIritMdlrDllGetInputParameter(FI, FIELD_COLOR, &color);
    GuIritMdlrDllGetInputParameter(FI, FIELD_ALPHA, &alpha);

    char* tmp = shape_names;
    char** ptr = &tmp;
    int index;

    GuIritMdlrDllGetInputParameter(FI, FIELD_SHAPE, &index, ptr);
    if (index != shape_index) {
        shape_index = index;
        IrtMdlrSrfPaintLoadShape(FI, shape_files[shape_index]);
    }

    IrtRType tmp_x, tmp_y;

    GuIritMdlrDllGetInputParameter(FI, FIELD_X_FACTOR, &tmp_x);
    GuIritMdlrDllGetInputParameter(FI, FIELD_Y_FACTOR, &tmp_y);

    if (D(tmp_x, x_factor) > 10e-5 || D(tmp_y, y_factor) > 10e-5) {
        x_factor = tmp_x;
        y_factor = tmp_y;
        IrtMdlrSrfPaintLoadShape(FI, shape_files[shape_index]);
    }
}

static void IrtMdlrSrfPaintResizeTexture(IrtMdlrFuncInfoClass* FI,
                        int Width,
                        int Height,
                        bool Reset)
{
    delete[] textures[active_surface]->texture;
    delete[] texture_alpha;
    delete[] texture_buffer;
    IrtImgPixelStruct* new_texture = new IrtImgPixelStruct[Width * Height];
    IrtImgPixelStruct* new_texture_alpha = new IrtImgPixelStruct[Width * Height];
    IrtImgPixelStruct* new_texture_buffer = new IrtImgPixelStruct[Width * Height];
    if (Reset) {
        for (int y = 0; y < Height; y++) {
            for (int x = 0; x < Width; x++) {
                new_texture[y * Width + x].r = 255;
                new_texture[y * Width + x].g = 255;
                new_texture[y * Width + x].b = 255;
                new_texture_alpha[y * Width + x].r = 255;
                new_texture_alpha[y * Width + x].g = 255;
                new_texture_alpha[y * Width + x].b = 255;
                new_texture_buffer[y * Width + x].r = 255;
                new_texture_buffer[y * Width + x].g = 255;
                new_texture_buffer[y * Width + x].b = 255;
            }
        }
    }
    textures[active_surface]->texture = new_texture;
    texture_alpha = new_texture_alpha;
    texture_buffer = new_texture_buffer;
    textures[active_surface]->width = Width;
    textures[active_surface]->height = Height;
    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture resized to %dx%d\n", Width, Height);
}

void IrtMdlrSrfPaintInitShapes(IrtMdlrFuncInfoClass* FI)
{
    int index = 0;
    const char* p, * q;
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

static void IrtMdlrSrfPaintLoadShape(IrtMdlrFuncInfoClass* FI,
                        const char* Filename)
{
    int alpha, width, height;
    IrtImgPixelStruct* image = IrtImgReadImage2(Filename, &width, &height, &alpha);
    width++;
    height++;
    if (!shape_init) {
        delete[] shape_matrix;
    }
    else {
        shape_init = 0;
    }

    shape_width = (int)(width * x_factor);
    shape_height = (int)(height * y_factor);

    float x_ratio = (float)width / (float)shape_width;
    float y_ratio = (float)height / (float)shape_height;

    shape_matrix = new float[shape_height * shape_width];
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

static void IrtMdlrSrfPaintBresenham(const pair<int, int>& a,
                        const pair<int, int>& b,
                        vector<pair<int, int>>& Points)
{
    int a1, a2, b1, b2;
    int is_high;
    if (abs(b.first - a.first) > abs(b.second - a.second)) {
        a1 = a.first;
        b1 = a.second;
        a2 = b.first;
        b2 = b.second;
        is_high = 0;
    }
    else {
        a1 = a.second;
        b1 = a.first;
        a2 = b.second;
        b2 = b.first;
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
    int v = b1;
    for (int u = a1; u <= a2; u++) {
        int x = (is_high) ? v : u;
        int y = (is_high) ? u : v;
        pair<int, int> p = pair<int, int>(x, y);
        Points.push_back(p);
        if (d > 0) {
            v += n;
            d -= 2 * da;
        }
        d += 2 * db;
    }
}

static void IrtMdlrSrfPaintRenderShape(IrtMdlrFuncInfoClass* FI,
                        int XOff,
                        int YOff)
{
    Texture* texture = textures[active_surface];
    int x_min = XOff - shape_width / 2;
    while (x_min < 0) {
        x_min += texture->width;
    }
    x_min %= texture->width;

    int y_min = (YOff - shape_height / 2);
    while (y_min < 0) {
        y_min += texture->height;
    }
    y_min %= texture->height;

    for (int yy = 0; yy < shape_height; yy++) {
        for (int xx = 0; xx < shape_width; xx++) {
            int x = (x_min + xx) % texture->width;
            int y = (y_min + yy) % texture->height;
            int texture_offset = x + texture->width * y;
            int shape_offset = xx + shape_width * yy;

            texture_alpha[texture_offset].r += (IrtBType)((color[0] - texture_alpha[texture_offset].r) * shape_matrix[shape_offset]);
            texture_alpha[texture_offset].g += (IrtBType)((color[1] - texture_alpha[texture_offset].g) * shape_matrix[shape_offset]);
            texture_alpha[texture_offset].b += (IrtBType)((color[2] - texture_alpha[texture_offset].b) * shape_matrix[shape_offset]);
            texture->texture[texture_offset].r = (IrtBType)(texture_buffer[texture_offset].r + (texture_alpha[texture_offset].r - texture_buffer[texture_offset].r) * (alpha / 255.0));
            texture->texture[texture_offset].g = (IrtBType)(texture_buffer[texture_offset].g + (texture_alpha[texture_offset].g - texture_buffer[texture_offset].g) * (alpha / 255.0));
            texture->texture[texture_offset].b = (IrtBType)(texture_buffer[texture_offset].b + (texture_alpha[texture_offset].b - texture_buffer[texture_offset].b) * (alpha / 255.0));
        }
    }
}

static int IrtMdlrSrfPaintMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent)
{
    IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)MouseEvent->Data;
    IRT_DSP_STATIC_DATA int clicking = FALSE;

    IPObjectStruct* PObj = (IPObjectStruct*)MouseEvent->PObj;

    if (MouseEvent->KeyModifiers & IRT_DSP_KEY_MODIFIER_SHIFT_DOWN) {
        switch (MouseEvent->Type) {
        case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
            GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
            clicking = TRUE;
            break;
        case IRT_DSP_MOUSE_EVENT_LEFT_UP:
            GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
            clicking = FALSE;
            if (active_surface != NULL) {
                Texture* texture = textures[active_surface];
                for (int y = 0; y < texture->height; y++) {
                    for (int x = 0; x < texture->width; x++) {
                        int offset = y * texture->width + x;
                        texture_alpha[offset] = texture->texture[offset];
                        texture_buffer[offset] = texture->texture[offset];
                    }
                }
            }
            last_x_offset = -1;
            last_y_offset = -1;
            break;
        }
        if (clicking && MouseEvent->UV != NULL && PObj != NULL && PObj == active_surface) {
            Texture* texture = textures[active_surface];
            int x_offset = (int)((double)texture->width * fmod(MouseEvent->UV[0], 1));
            int y_offset = (int)((double)texture->height * fmod(MouseEvent->UV[1], 1));
            if (last_y_offset == -1) {
                IrtMdlrSrfPaintRenderShape(FI, x_offset, y_offset);
                last_x_offset = x_offset;
                last_y_offset = y_offset;
            }
            else {
                int x_backup = x_offset;
                int y_backup = y_offset;
                if (D(last_x_offset, x_offset - texture->width) < D(last_x_offset, x_offset)) {
                    x_offset -= texture->width;
                }
                if (D(last_x_offset, x_offset + texture->width) < D(last_x_offset, x_offset)) {
                    x_offset += texture->width;
                }
                if (D(last_y_offset, y_offset - texture->height) < D(last_y_offset, y_offset)) {
                    y_offset -= texture->height;
                }
                if (D(last_y_offset, y_offset + texture->height) < D(last_y_offset, y_offset)) {
                    y_offset += texture->height;
                }
                vector<pair<int, int>> points;
                pair<int, int>
                    start = pair<int, int>(last_x_offset, last_y_offset),
                    end = pair<int, int>(x_offset, y_offset);
                IrtMdlrSrfPaintBresenham(start, end, points);
                for (pair<int, int>& p : points) {
                    IrtMdlrSrfPaintRenderShape(FI, p.first % texture->width, p.second % texture->height);
                }
                last_x_offset = x_backup;
                last_y_offset = y_backup;
            }
            GuIritMdlrDllSetTextureFromImage(FI, active_surface, texture->texture, texture->width, texture->height, FALSE);
        }
    }
    return TRUE;
}
