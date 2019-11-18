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
#define DEFAULT_SIZE (DEFAULT_WIDTH)*(DEFAULT_HEIGHT)

#define D(a, b) (fabs((double)(a) - (double)(b)))

struct SrfPaintShapeStruct {
    int Width;
    int Height;
    float* Shape;
};

struct SrfPaintTextureStruct {
    int Width;
    int Height;
    IrtImgPixelStruct* Texture;
};

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

IRT_DSP_STATIC_DATA int 
    IrtMdlrSrfPaintTextureWidth = 256,
    IrtMdlrSrfPaintTextureHeight = 256,
    IrtMdlrSrfPaintShapeIndex = -1;

IRT_DSP_STATIC_DATA IrtRType 
    IrtMdlrSrfPaintAlpha = 255,
    IrtMdlrSrfPaintXFactor = 1,
    IrtMdlrSrfPaintYFactor = 1;

IRT_DSP_STATIC_DATA IrtVecType 
    IrtMdlrSrfPaintColor = {0, 0, 0};

IRT_DSP_STATIC_DATA IrtImgPixelStruct 
    *IrtMdlrSrfPaintTextureAlpha,
    *IrtMdlrSrfPaintTextureBuffer;

IRT_DSP_STATIC_DATA map<IPObjectStruct*, SrfPaintTextureStruct*> 
    IrtMdlrSrfPaintTextures;

IRT_DSP_STATIC_DATA char 
    IrtMdlrSrfPaintShapesNames[IRIT_LINE_LEN_XLONG];

IRT_DSP_STATIC_DATA const char 
    **IrtMdlrSrfPaintShapesFiles = NULL;

IRT_DSP_STATIC_DATA SrfPaintShapeStruct 
    *IrtMdlrSrfPaintShape;

IRT_DSP_STATIC_DATA IPObjectStruct
    *IrtMdlrSrfPaintSurface = NULL;

enum {
    SRF_PAINT_SURFACE = 0,
    SRF_PAINT_LOAD,
    SRF_PAINT_SAVE,
    SRF_PAINT_RESET,
    SRF_PAINT_WIDTH,
    SRF_PAINT_HEIGHT,
    SRF_PAINT_COLOR,
    SRF_PAINT_ALPHA,
    SRF_PAINT_SHAPE,
    SRF_PAINT_X_FACTOR,
    SRF_PAINT_Y_FACTOR
};

IRT_DSP_STATIC_DATA IrtMdlrFuncTableStruct SrfPainterFunctionTable[] =
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
            /* Surface selection */
            IRT_MDLR_SURFACE_EXPR,

            /* Texture fields */
            IRT_MDLR_BUTTON_EXPR,				             /* Load Texture */
            IRT_MDLR_BUTTON_EXPR,				             /* Save Texture */
            IRT_MDLR_BUTTON_EXPR,				            /* Reset Texture */
            IRT_MDLR_INTEGER_EXPR,				            /* Texture width */
            IRT_MDLR_INTEGER_EXPR,				           /* Texture height */

            /* Brush fields */
            IRT_MDLR_VECTOR_EXPR,				               /* RGB Values */
            IRT_MDLR_NUMERIC_EXPR,				              /* Alpha Value */
            IRT_MDLR_HIERARCHY_SELECTION_EXPR,	     /* Shape selection menu */
            IRT_MDLR_NUMERIC_EXPR,				                 /* X Factor */
            IRT_MDLR_NUMERIC_EXPR,				                 /* Y Factor */
        },
        {
            NULL,
            NULL,
            NULL,
            NULL,
            &IrtMdlrSrfPaintTextureWidth,
            &IrtMdlrSrfPaintTextureHeight,
            &IrtMdlrSrfPaintColor,
            &IrtMdlrSrfPaintAlpha,
            IrtMdlrSrfPaintShapesNames,
            &IrtMdlrSrfPaintXFactor,
            &IrtMdlrSrfPaintYFactor,
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

IRT_DSP_STATIC_DATA const int SRF_PAINT_FUNC_TABLE_SIZE =
    sizeof(SrfPainterFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

extern "C" bool _IrtMdlrDllRegister(void)
{
    GuIritMdlrDllRegister(SrfPainterFunctionTable, 
        SRF_PAINT_FUNC_TABLE_SIZE, 
        "Example", 
        IconMenuExample);
    return true;
}

static void IrtMdlrSrfPaint(IrtMdlrFuncInfoClass* FI)
{
    IRT_DSP_STATIC_DATA bool TextureUpdate = false, TextureInit = false;
    IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass *GlobalFI = NULL;
    int ShapeIndex;
    IrtRType TmpXFactor, TmpYFactor;
    char *TmpPtr = IrtMdlrSrfPaintShapesNames;
    
    
    if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_INIT) {
        return;
    }

    if (FI->InvocationNumber == 0 && GlobalFI == NULL) {
        GuIritMdlrDllPushMouseEventFunc(
            FI,
            IrtMdlrSrfPaintMouseCallBack,
            (IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
            (IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_CTRL_DOWN),
            FI);
        IrtMdlrSrfPaintInitShapes(FI);

        GuIritMdlrDllSetIntInputDomain(FI, 1, IRIT_MAX_INT, SRF_PAINT_WIDTH);
        GuIritMdlrDllSetIntInputDomain(FI, 1, IRIT_MAX_INT, SRF_PAINT_HEIGHT);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, SRF_PAINT_COLOR, 0);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, SRF_PAINT_COLOR, 1);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, SRF_PAINT_COLOR, 2);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, SRF_PAINT_ALPHA);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, SRF_PAINT_X_FACTOR);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, SRF_PAINT_Y_FACTOR);

        GlobalFI = FI;
        GuIritMdlrDllPrintf(FI,
            IRT_DSP_LOG_INFO,
            "Surface Painter initialized\n");
    }

    if (GlobalFI != NULL && (
        FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK ||
        FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_CANCEL)) {
        GuIritMdlrDllPopMouseEventFunc(FI);
        GlobalFI = false;
        GuIritMdlrDllPrintf(FI,
            IRT_DSP_LOG_INFO,
            "Surface Painter unloaded\n");
    }

    IPObjectStruct* Surface = NULL;
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_SURFACE, &Surface);
    if (Surface != NULL && !TextureUpdate) {
        TextureUpdate = true;
        if (IrtMdlrSrfPaintTextures.find(Surface) 
            == IrtMdlrSrfPaintTextures.end()) {
            SrfPaintTextureStruct* Texture = new SrfPaintTextureStruct;
            Texture->Width = DEFAULT_WIDTH;
            Texture->Height = DEFAULT_HEIGHT;
            Texture->Texture = new IrtImgPixelStruct[DEFAULT_SIZE];
            for (int i = 0; i < DEFAULT_SIZE; i++) {
                Texture->Texture[i].r = 255;
                Texture->Texture[i].g = 255;
                Texture->Texture[i].b = 255;
            }
            IrtMdlrSrfPaintTextures[Surface] = Texture;
            GuIritMdlrDllPrintf(FI, 
                IRT_DSP_LOG_INFO, 
                "Surface %s initialized\n", 
                Surface->ObjName);
        }
        if (Surface != IrtMdlrSrfPaintSurface) {
            SrfPaintTextureStruct* Texture = IrtMdlrSrfPaintTextures[Surface];
            GuIritMdlrDllSetInputParameter(FI, 
                SRF_PAINT_WIDTH, 
                &Texture->Width);
            GuIritMdlrDllSetInputParameter(FI, 
                SRF_PAINT_HEIGHT, 
                &Texture->Height);
            IrtMdlrSrfPaintSurface = Surface;
            IrtMdlrSrfPaintTextureAlpha = new IrtImgPixelStruct[DEFAULT_SIZE];
            IrtMdlrSrfPaintTextureBuffer = new IrtImgPixelStruct[DEFAULT_SIZE];
            for (int i = 0; i < DEFAULT_SIZE; i++) {
                IrtMdlrSrfPaintTextureAlpha[i] = Texture->Texture[i];
                IrtMdlrSrfPaintTextureBuffer[i] = Texture->Texture[i];
            }
            GuIritMdlrDllPrintf(FI, 
                IRT_DSP_LOG_INFO, 
                "Surface %s selected\n",
                IrtMdlrSrfPaintSurface->ObjName);
            TextureInit = true;
        }
        TextureUpdate = false;
    }

    // Texture fields
    if (FI->IntermediateWidgetMajor == SRF_PAINT_LOAD && !TextureUpdate 
        && IrtMdlrSrfPaintSurface != NULL) {
        char* Filename;
        bool Res = GuIritMdlrDllGetAsyncInputFileName(FI, 
            "Load Texture from....", 
            "*.png", 
            &Filename);
        if (Res) {
            int Alpha, Width, Height;
            IrtImgPixelStruct* Image = 
                IrtImgReadImage2(Filename, &Width, &Height, &Alpha);
            Width++, Height++;
            GuIritMdlrDllPrintf(FI, 
                IRT_DSP_LOG_INFO, 
                "Texture loaded successfully from \"%s\" (%dx%d)\n",
                Filename, 
                Width, 
                Height);
            IrtMdlrSrfPaintResizeTexture(FI, Width, Height, false);
            SrfPaintTextureStruct* texture = IrtMdlrSrfPaintTextures[Surface];
            for (int i = 0; i < Width * Height; i++) {
                texture->Texture[i] = Image[i];
                IrtMdlrSrfPaintTextureAlpha[i] = Image[i];
                IrtMdlrSrfPaintTextureBuffer[i] = Image[i];
            }
            GuIritMdlrDllSetTextureFromImage(FI, 
                IrtMdlrSrfPaintSurface, 
                texture->Texture, 
                texture->Width, 
                texture->Height, 
                FALSE);
            TextureUpdate = true;
            GuIritMdlrDllSetInputParameter(FI, 
                SRF_PAINT_WIDTH, 
                &texture->Width);
            GuIritMdlrDllSetInputParameter(FI, 
                SRF_PAINT_HEIGHT, 
                &texture->Height);
            TextureUpdate = false;
        }
    }

    if (FI->IntermediateWidgetMajor == SRF_PAINT_SAVE 
        && IrtMdlrSrfPaintSurface != NULL) {
        char* Filename;
        bool Res = GuIritMdlrDllGetAsyncInputFileName(FI, 
            "Save Texture to....", 
            "*.png", 
            &Filename);
        if (Res) {
            SrfPaintTextureStruct* Texture = IrtMdlrSrfPaintTextures[Surface];
            MiscWriteGenInfoStructPtr GI = IrtImgWriteOpenFile(NULL, 
                Filename, 
                IRIT_IMAGE_PNG_TYPE, 
                false, 
                Texture->Width, 
                Texture->Height);
            if (GI == NULL) {
                GuIritMdlrDllPrintf(FI, 
                    IRT_DSP_LOG_ERROR, 
                    "Error saving texture to \"%s\"\n", 
                    Filename);
            }
            else {
                for (int y = 0; y < Texture->Height; y++) {
                    IrtImgWritePutLine(GI, 
                        NULL, 
                        &Texture->Texture[y * Texture->Width]);
                }
                IrtImgWriteCloseFile(GI);
                GuIritMdlrDllPrintf(FI, 
                    IRT_DSP_LOG_INFO, 
                    "Texture saved successfully to \"%s\"\n", 
                    Filename);
            }
        }
    }

    if (FI->IntermediateWidgetMajor == SRF_PAINT_RESET && !TextureUpdate 
        && IrtMdlrSrfPaintSurface != NULL) {
        bool Res = GuIritMdlrDllGetAsyncInputConfirm(FI, 
            "Texture Reset", 
            "Are you sure you want to reset the texture ?");
        if (Res) {
            SrfPaintTextureStruct* Texture = IrtMdlrSrfPaintTextures[Surface];
            IrtMdlrSrfPaintResizeTexture(FI, 
                DEFAULT_WIDTH, 
                DEFAULT_HEIGHT, 
                true);
            TextureUpdate = true;
            int Width = DEFAULT_WIDTH, Height = DEFAULT_HEIGHT;
            GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_WIDTH, &Width);
            GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_HEIGHT, &Height);
            TextureUpdate = false;
            GuIritMdlrDllSetTextureFromImage(FI, 
                IrtMdlrSrfPaintSurface, 
                Texture->Texture, 
                Texture->Width, 
                Texture->Height, 
                FALSE);
        }
    }

    GuIritMdlrDllGetInputParameter(FI, 
        SRF_PAINT_WIDTH, 
        &IrtMdlrSrfPaintTextureWidth);
    GuIritMdlrDllGetInputParameter(FI, 
        SRF_PAINT_HEIGHT, 
        &IrtMdlrSrfPaintTextureHeight);
    if (IrtMdlrSrfPaintSurface != NULL) {
        SrfPaintTextureStruct* Texture = IrtMdlrSrfPaintTextures[Surface];
        if ((Texture->Width != IrtMdlrSrfPaintTextureWidth 
            || Texture->Height != IrtMdlrSrfPaintTextureHeight) 
            && !TextureUpdate) {
            TextureUpdate = true;
            bool Res = true;
            if (TextureInit) {
                IRT_DSP_STATIC_DATA high_resolution_clock::time_point Timer;
                IRT_DSP_STATIC_DATA bool TimerInit = false;

                if (!TimerInit 
                    || duration_cast<seconds>(high_resolution_clock::now() - Timer).count() >= 20) {
                    Res = GuIritMdlrDllGetAsyncInputConfirm(FI, 
                        "", 
                        "This will reset the texture.\n" \
                        "Are you sure you want to resize the texture ?");
                    if (Res) {
                        TimerInit = true;
                        Timer = high_resolution_clock::now();;
                    }
                }
            }
            if (Res) {
                IrtMdlrSrfPaintResizeTexture(FI, 
                    IrtMdlrSrfPaintTextureWidth, 
                    IrtMdlrSrfPaintTextureHeight, 
                    true);
                GuIritMdlrDllSetTextureFromImage(FI, 
                    IrtMdlrSrfPaintSurface, 
                    Texture->Texture, 
                    Texture->Width, 
                    Texture->Height, 
                    FALSE);
            }
            else {
                GuIritMdlrDllSetInputParameter(FI,
                    SRF_PAINT_WIDTH,
                    &Texture->Width);
                GuIritMdlrDllSetInputParameter(FI, 
                    SRF_PAINT_HEIGHT, 
                    &Texture->Height);
            }
            TextureUpdate = false;
        }
    }

    // Brush fields
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_COLOR, &IrtMdlrSrfPaintColor);
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_ALPHA, &IrtMdlrSrfPaintAlpha);
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_SHAPE, &ShapeIndex, &TmpPtr);
    if (ShapeIndex != IrtMdlrSrfPaintShapeIndex) {
        IrtMdlrSrfPaintShapeIndex = ShapeIndex;
        IrtMdlrSrfPaintLoadShape(FI, 
            IrtMdlrSrfPaintShapesFiles[ShapeIndex]);
    }

    

    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_X_FACTOR, &TmpXFactor);
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_Y_FACTOR, &TmpYFactor);

    if (D(TmpXFactor, IrtMdlrSrfPaintXFactor) > 10e-5 
        || D(TmpYFactor, IrtMdlrSrfPaintYFactor) > 10e-5) {
        IrtMdlrSrfPaintXFactor = TmpXFactor;
        IrtMdlrSrfPaintYFactor = TmpYFactor;
        IrtMdlrSrfPaintLoadShape(FI, 
            IrtMdlrSrfPaintShapesFiles[ShapeIndex]);
    }
}

static void IrtMdlrSrfPaintResizeTexture(IrtMdlrFuncInfoClass* FI,
                        int Width,
                        int Height,
                        bool Reset)
{
    delete[] IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface]->Texture;
    delete[] IrtMdlrSrfPaintTextureAlpha;
    delete[] IrtMdlrSrfPaintTextureBuffer;
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
    IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface]->Texture = new_texture;
    IrtMdlrSrfPaintTextureAlpha = new_texture_alpha;
    IrtMdlrSrfPaintTextureBuffer = new_texture_buffer;
    IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface]->Width = Width;
    IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface]->Height = Height;
    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Texture resized to %dx%d\n", Width, Height);
}

void IrtMdlrSrfPaintInitShapes(IrtMdlrFuncInfoClass* FI)
{
    int index = 0;
    const char* p, * q;
    char base_path[IRIT_LINE_LEN_LONG], path[IRIT_LINE_LEN_LONG];
    const IrtDspGuIritSystemInfoStruct* system_props = GuIritMdlrDllGetGuIritSystemProps(FI);
    sprintf(path, "%s\\Example\\Masks", searchpath(system_props->AuxiliaryDataName, base_path));
    if (IrtMdlrSrfPaintShapesFiles == NULL) {
        IrtMdlrSrfPaintShapesFiles = GuIritMdlrDllGetAllFilesNamesInDirectory(FI, path, "*.rle|*.ppm|*.gif|*.jpeg|*.png");
    }

    strcpy(IrtMdlrSrfPaintShapesNames, "");
    for (int i = 0; IrtMdlrSrfPaintShapesFiles[i] != NULL; ++i) {
        p = strstr(IrtMdlrSrfPaintShapesFiles[i], "\\Example\\Masks");
        if (p != NULL) {
            p += strlen("\\Example\\Masks") + 1;
            char shape_name[IRIT_LINE_LEN_LONG];
            strcpy(shape_name, "");
            q = strchr(p, '.');
            strncpy(shape_name, p, q - p);
            shape_name[q - p] = '\0';
            if (strstr(shape_name, "GaussianFull25") != NULL) {
                index = i;
            }
            sprintf(IrtMdlrSrfPaintShapesNames, "%s%s;", IrtMdlrSrfPaintShapesNames, shape_name);
        }
    }

    if (IrtMdlrSrfPaintShapesFiles[0] != NULL) {
        sprintf(IrtMdlrSrfPaintShapesNames, "%s:%d", IrtMdlrSrfPaintShapesNames, index);
        GuIritMdlrDllSetInputSelectionStruct files_names(IrtMdlrSrfPaintShapesNames);
        GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_SHAPE, &files_names);
    }
    else {
        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "Masks files were not found. Check directory: \"%s\"\n", path);
        return;
    }
}

static void IrtMdlrSrfPaintLoadShape(IrtMdlrFuncInfoClass* FI,
                        const char* Filename)
{
    IRT_DSP_STATIC_DATA bool shape_init = false;

    int alpha, width, height;
    IrtImgPixelStruct* image = IrtImgReadImage2(Filename, &width, &height, &alpha);
    width++;
    height++;
    if (shape_init) {
        delete[] IrtMdlrSrfPaintShape -> Shape;
    }
    else {
        IrtMdlrSrfPaintShape = new SrfPaintShapeStruct;
        shape_init = true;
    }

    IrtMdlrSrfPaintShape -> Width = (int)(width * IrtMdlrSrfPaintXFactor);
    IrtMdlrSrfPaintShape->Height = (int)(height * IrtMdlrSrfPaintYFactor);

    float x_ratio = (float)width / (float)IrtMdlrSrfPaintShape->Width;
    float y_ratio = (float)height / (float) IrtMdlrSrfPaintShape->Height;

    IrtMdlrSrfPaintShape->Shape = new float[IrtMdlrSrfPaintShape->Height * IrtMdlrSrfPaintShape->Width];
    for (int y = 0; y < IrtMdlrSrfPaintShape->Height; y++) {
        for (int x = 0; x < IrtMdlrSrfPaintShape->Width; x++) {
            float gray;
            IrtImgPixelStruct* ptr = image + (int)((int)(y * y_ratio) * width + x * x_ratio);
            IRT_DSP_RGB2GREY(ptr, gray);
            IrtMdlrSrfPaintShape->Shape[y * IrtMdlrSrfPaintShape->Width + x] = (float)(gray / 255.0);
        }
    }
    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Loaded shape of size %dx%d\n", IrtMdlrSrfPaintShape->Width, IrtMdlrSrfPaintShape->Height);
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
    SrfPaintTextureStruct* texture = IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface];
    int x_min = XOff - IrtMdlrSrfPaintShape->Width / 2;
    while (x_min < 0) {
        x_min += texture->Width;
    }
    x_min %= texture->Width;

    int y_min = (YOff - IrtMdlrSrfPaintShape->Height / 2);
    while (y_min < 0) {
        y_min += texture->Height;
    }
    y_min %= texture->Height;

    for (int yy = 0; yy < IrtMdlrSrfPaintShape->Height; yy++) {
        for (int xx = 0; xx < IrtMdlrSrfPaintShape->Width; xx++) {
            int x = (x_min + xx) % texture->Width;
            int y = (y_min + yy) % texture->Height;
            int texture_offset = x + texture->Width * y;
            int shape_offset = xx + IrtMdlrSrfPaintShape->Width * yy;

            IrtMdlrSrfPaintTextureAlpha[texture_offset].r += (IrtBType)((IrtMdlrSrfPaintColor[0] - IrtMdlrSrfPaintTextureAlpha[texture_offset].r) * IrtMdlrSrfPaintShape->Shape[shape_offset]);
            IrtMdlrSrfPaintTextureAlpha[texture_offset].g += (IrtBType)((IrtMdlrSrfPaintColor[1] - IrtMdlrSrfPaintTextureAlpha[texture_offset].g) * IrtMdlrSrfPaintShape->Shape[shape_offset]);
            IrtMdlrSrfPaintTextureAlpha[texture_offset].b += (IrtBType)((IrtMdlrSrfPaintColor[2] - IrtMdlrSrfPaintTextureAlpha[texture_offset].b) * IrtMdlrSrfPaintShape->Shape[shape_offset]);
            texture->Texture[texture_offset].r = (IrtBType)(IrtMdlrSrfPaintTextureBuffer[texture_offset].r + (IrtMdlrSrfPaintTextureAlpha[texture_offset].r - IrtMdlrSrfPaintTextureBuffer[texture_offset].r) * (IrtMdlrSrfPaintAlpha / 255.0));
            texture->Texture[texture_offset].g = (IrtBType)(IrtMdlrSrfPaintTextureBuffer[texture_offset].g + (IrtMdlrSrfPaintTextureAlpha[texture_offset].g - IrtMdlrSrfPaintTextureBuffer[texture_offset].g) * (IrtMdlrSrfPaintAlpha / 255.0));
            texture->Texture[texture_offset].b = (IrtBType)(IrtMdlrSrfPaintTextureBuffer[texture_offset].b + (IrtMdlrSrfPaintTextureAlpha[texture_offset].b - IrtMdlrSrfPaintTextureBuffer[texture_offset].b) * (IrtMdlrSrfPaintAlpha / 255.0));
        }
    }
}

static int IrtMdlrSrfPaintMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent)
{
    IRT_DSP_STATIC_DATA int last_x_offset = -1;
    IRT_DSP_STATIC_DATA int last_y_offset = -1;
    
    IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)MouseEvent->Data;
    IRT_DSP_STATIC_DATA int clicking = FALSE;

    IPObjectStruct* PObj = (IPObjectStruct*)MouseEvent->PObj;

    if (MouseEvent->KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN) {
        switch (MouseEvent->Type) {
        case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
            GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
            clicking = TRUE;
            break;
        case IRT_DSP_MOUSE_EVENT_LEFT_UP:
            GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
            clicking = FALSE;
            if (IrtMdlrSrfPaintSurface != NULL) {
                SrfPaintTextureStruct* texture = IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface];
                for (int y = 0; y < texture->Height; y++) {
                    for (int x = 0; x < texture->Width; x++) {
                        int offset = y * texture->Width + x;
                        IrtMdlrSrfPaintTextureAlpha[offset] = texture->Texture[offset];
                        IrtMdlrSrfPaintTextureBuffer[offset] = texture->Texture[offset];
                    }
                }
            }
            last_x_offset = -1;
            last_y_offset = -1;
            break;
        }
        if (clicking && MouseEvent->UV != NULL && PObj != NULL && PObj == IrtMdlrSrfPaintSurface) {
            SrfPaintTextureStruct* texture = IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface];
            int x_offset = (int)((double)texture->Width * fmod(MouseEvent->UV[0], 1));
            int y_offset = (int)((double)texture->Height * fmod(MouseEvent->UV[1], 1));
            if (last_y_offset == -1) {
                IrtMdlrSrfPaintRenderShape(FI, x_offset, y_offset);
                last_x_offset = x_offset;
                last_y_offset = y_offset;
            }
            else {
                int x_backup = x_offset;
                int y_backup = y_offset;
                if (D(last_x_offset, x_offset - texture->Width) < D(last_x_offset, x_offset)) {
                    x_offset -= texture->Width;
                }
                if (D(last_x_offset, x_offset + texture->Width) < D(last_x_offset, x_offset)) {
                    x_offset += texture->Width;
                }
                if (D(last_y_offset, y_offset - texture->Height) < D(last_y_offset, y_offset)) {
                    y_offset -= texture->Height;
                }
                if (D(last_y_offset, y_offset + texture->Height) < D(last_y_offset, y_offset)) {
                    y_offset += texture->Height;
                }
                vector<pair<int, int>> points;
                pair<int, int>
                    start = pair<int, int>(last_x_offset, last_y_offset),
                    end = pair<int, int>(x_offset, y_offset);
                IrtMdlrSrfPaintBresenham(start, end, points);
                for (pair<int, int>& p : points) {
                    IrtMdlrSrfPaintRenderShape(FI, p.first % texture->Width, p.second % texture->Height);
                }
                last_x_offset = x_backup;
                last_y_offset = y_backup;
            }
            GuIritMdlrDllSetTextureFromImage(FI, IrtMdlrSrfPaintSurface, texture->Texture, texture->Width, texture->Height, FALSE);
        }
    }
    return TRUE;
}
