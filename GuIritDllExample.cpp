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
    IrtMdlrSrfPaintTextureHeight = 256; 
    

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
            IRT_MDLR_BUTTON_EXPR,				/* Load Texture */
            IRT_MDLR_BUTTON_EXPR,				/* Save Texture */
            IRT_MDLR_BUTTON_EXPR,	            /* Reset Texture */
            IRT_MDLR_INTEGER_EXPR,	            /* Texture width */
            IRT_MDLR_INTEGER_EXPR,				/* Texture height */

            /* Brush fields */
            IRT_MDLR_VECTOR_EXPR,				/* RGB Values */
            IRT_MDLR_NUMERIC_EXPR,				/* Alpha Value */
            IRT_MDLR_HIERARCHY_SELECTION_EXPR,	/* Shape selection menu */
            IRT_MDLR_NUMERIC_EXPR,				/* X Factor */
            IRT_MDLR_NUMERIC_EXPR,				/* Y Factor */
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
    IRT_DSP_STATIC_DATA bool 
        TextureUpdate = false, 
        TextureInit = false;
    IRT_DSP_STATIC_DATA int ShapeIndex = -1;
    IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass *GlobalFI = NULL;
    int TmpIndex;
    IrtRType TmpXFactor, TmpYFactor;
    char *TmpPtr = IrtMdlrSrfPaintShapesNames;
    IPObjectStruct *Surface = NULL;
    
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
            SrfPaintTextureStruct *Texture = IrtMdlrSrfPaintTextures[Surface];
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
            for (int i = 0; i < Width * Height; i++) {
                Texture->Texture[i] = Image[i];
                IrtMdlrSrfPaintTextureAlpha[i] = Image[i];
                IrtMdlrSrfPaintTextureBuffer[i] = Image[i];
            }
            GuIritMdlrDllSetTextureFromImage(FI, 
                IrtMdlrSrfPaintSurface, 
                Texture->Texture, 
                Texture->Width, 
                Texture->Height, 
                FALSE);
            TextureUpdate = true;
            GuIritMdlrDllSetInputParameter(FI, 
                SRF_PAINT_WIDTH, 
                &Texture->Width);
            GuIritMdlrDllSetInputParameter(FI, 
                SRF_PAINT_HEIGHT, 
                &Texture->Height);
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
            int Width = DEFAULT_WIDTH, 
                Height = DEFAULT_HEIGHT;
            SrfPaintTextureStruct* Texture = IrtMdlrSrfPaintTextures[Surface];

            IrtMdlrSrfPaintResizeTexture(FI, 
                DEFAULT_WIDTH, 
                DEFAULT_HEIGHT, 
                true);
            TextureUpdate = true;
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
            bool Res = true;

            TextureUpdate = true;
            if (TextureInit) {
                IRT_DSP_STATIC_DATA high_resolution_clock::time_point Timer;
                IRT_DSP_STATIC_DATA bool TimerInit = false;

                if (!TimerInit 
                    || duration_cast<seconds>(high_resolution_clock::now() 
                    - Timer).count() >= 20) {
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
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_SHAPE, &TmpIndex, &TmpPtr);
    if (TmpIndex != ShapeIndex) {
        ShapeIndex = TmpIndex;
        IrtMdlrSrfPaintLoadShape(FI, 
            IrtMdlrSrfPaintShapesFiles[TmpIndex]);
    }

    

    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_X_FACTOR, &TmpXFactor);
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_Y_FACTOR, &TmpYFactor);

    if (D(TmpXFactor, IrtMdlrSrfPaintXFactor) > 10e-5 
        || D(TmpYFactor, IrtMdlrSrfPaintYFactor) > 10e-5) {
        IrtMdlrSrfPaintXFactor = TmpXFactor;
        IrtMdlrSrfPaintYFactor = TmpYFactor;
        IrtMdlrSrfPaintLoadShape(FI, 
            IrtMdlrSrfPaintShapesFiles[TmpIndex]);
    }
}

static void IrtMdlrSrfPaintResizeTexture(IrtMdlrFuncInfoClass* FI,
                        int Width,
                        int Height,
                        bool Reset)
{
    IrtImgPixelStruct
        *Texture = new IrtImgPixelStruct[Width * Height],
        *TextureAlpha = new IrtImgPixelStruct[Width * Height],
        *TextureBuffer = new IrtImgPixelStruct[Width * Height];

    if (Reset) {
        for (int y = 0; y < Height; y++) {
            for (int x = 0; x < Width; x++) {
                Texture[y * Width + x].r = 255;
                Texture[y * Width + x].g = 255;
                Texture[y * Width + x].b = 255;
                TextureAlpha[y * Width + x].r = 255;
                TextureAlpha[y * Width + x].g = 255;
                TextureAlpha[y * Width + x].b = 255;
                TextureBuffer[y * Width + x].r = 255;
                TextureBuffer[y * Width + x].g = 255;
                TextureBuffer[y * Width + x].b = 255;
            }
        }
    }
    IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface]->Width = Width;
    IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface]->Height = Height;
    delete[] IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface]->Texture;
    IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface]->Texture = Texture;
    delete[] IrtMdlrSrfPaintTextureAlpha;
    IrtMdlrSrfPaintTextureAlpha = TextureAlpha;
    delete[] IrtMdlrSrfPaintTextureBuffer;
    IrtMdlrSrfPaintTextureBuffer = TextureBuffer;
    GuIritMdlrDllPrintf(FI, 
        IRT_DSP_LOG_INFO, 
        "Texture resized to %dx%d\n", 
        Width, 
        Height);
}

void IrtMdlrSrfPaintInitShapes(IrtMdlrFuncInfoClass* FI)
{
    int Index = 0;
    const char* p, * q;
    char Base[IRIT_LINE_LEN_LONG], Path[IRIT_LINE_LEN_LONG];
    const IrtDspGuIritSystemInfoStruct* Props = 
        GuIritMdlrDllGetGuIritSystemProps(FI);

    sprintf(Path, 
        "%s\\Example\\Masks", 
        searchpath(Props->AuxiliaryDataName, Base));
    if (IrtMdlrSrfPaintShapesFiles == NULL) {
        IrtMdlrSrfPaintShapesFiles = 
            GuIritMdlrDllGetAllFilesNamesInDirectory(FI, 
                Path, 
                "*.rle|*.ppm|*.gif|*.jpeg|*.png");
    }

    strcpy(IrtMdlrSrfPaintShapesNames, "");
    for (int i = 0; IrtMdlrSrfPaintShapesFiles[i] != NULL; ++i) {
        p = strstr(IrtMdlrSrfPaintShapesFiles[i], "\\Example\\Masks");
        if (p != NULL) {
            p += strlen("\\Example\\Masks") + 1;
            char Name[IRIT_LINE_LEN_LONG];
            strcpy(Name, "");
            q = strchr(p, '.');
            strncpy(Name, p, q - p);
            Name[q - p] = '\0';
            if (strstr(Name, "GaussianFull25") != NULL) {
                Index = i;
            }
            sprintf(IrtMdlrSrfPaintShapesNames, 
                "%s%s;", 
                IrtMdlrSrfPaintShapesNames, 
                Name);
        }
    }

    if (IrtMdlrSrfPaintShapesFiles[0] != NULL) {
        GuIritMdlrDllSetInputSelectionStruct Files(IrtMdlrSrfPaintShapesNames);

        sprintf(IrtMdlrSrfPaintShapesNames, 
            "%s:%d", 
            IrtMdlrSrfPaintShapesNames, 
            Index);
        GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_SHAPE, &Files);
    }
    else {
        GuIritMdlrDllPrintf(FI, 
            IRT_DSP_LOG_ERROR, 
            "Masks files were not found. Check directory: \"%s\"\n", 
            Path);
        return;
    }
}

static void IrtMdlrSrfPaintLoadShape(IrtMdlrFuncInfoClass* FI,
                        const char* Filename)
{
    IRT_DSP_STATIC_DATA bool ShapeInit = false;
    int Alpha, Width, Height;
    float XRatio, YRatio;
    IrtImgPixelStruct* Image = 
        IrtImgReadImage2(Filename, &Width, &Height, &Alpha);

    Width++, Height++;
    if (ShapeInit) {
        delete[] IrtMdlrSrfPaintShape -> Shape;
    }
    else {
        IrtMdlrSrfPaintShape = new SrfPaintShapeStruct;
        ShapeInit = true;
    }

    IrtMdlrSrfPaintShape -> Width = (int)(Width * IrtMdlrSrfPaintXFactor);
    IrtMdlrSrfPaintShape->Height = (int) (Height * IrtMdlrSrfPaintYFactor);
    IrtMdlrSrfPaintShape->Shape = 
        new float[IrtMdlrSrfPaintShape->Height * IrtMdlrSrfPaintShape->Width];
    XRatio = (float) Width / (float) IrtMdlrSrfPaintShape->Width;
    YRatio = (float) Height / (float) IrtMdlrSrfPaintShape->Height;
    for (int y = 0; y < IrtMdlrSrfPaintShape->Height; y++) {
        for (int x = 0; x < IrtMdlrSrfPaintShape->Width; x++) {
            int Off = y * IrtMdlrSrfPaintShape->Width + x;
            float gray;
            IrtImgPixelStruct* ptr = 
                Image + (int)((int)(y * YRatio) * Width + x * XRatio);

            IRT_DSP_RGB2GREY(ptr, gray);
            IrtMdlrSrfPaintShape->Shape[Off] = (float)(gray / 255.0);
        }
    }
    GuIritMdlrDllPrintf(FI, 
        IRT_DSP_LOG_INFO, 
        "Loaded shape of size %dx%d\n",
        IrtMdlrSrfPaintShape->Width, 
        IrtMdlrSrfPaintShape->Height);
}

static void IrtMdlrSrfPaintBresenham(const pair<int, int>& a,
                        const pair<int, int>& b,
                        vector<pair<int, int>>& Points)
{
    bool High;
    int A1, A2, B1, B2, Da, Db, n = 1, d, v;
    if (abs(b.first - a.first) > abs(b.second - a.second)) {
        A1 = a.first;
        B1 = a.second;
        A2 = b.first;
        B2 = b.second;
        High = false;
    }
    else {
        A1 = a.second;
        B1 = a.first;
        A2 = b.second;
        B2 = b.first;
        High = true;
    }
    if (A1 > A2) {
        swap(A1, A2);
        swap(B1, B2);
    }
    Da = A2 - A1;
    Db = B2 - B1;
    if (Da < 0) {
        n *= -1;
        Da *= -1;
    }
    if (Db < 0) {
        n *= -1;
        Db *= -1;
    }
    d = 2 * Db - Da;
    v = B1;
    for (int u = A1; u <= A2; u++) {
        int x = (High) ? v : u;
        int y = (High) ? u : v;
        pair<int, int> p = pair<int, int>(x, y);
        Points.push_back(p);
        if (d > 0) {
            v += n;
            d -= 2 * Da;
        }
        d += 2 * Db;
    }
}

static void IrtMdlrSrfPaintRenderShape(IrtMdlrFuncInfoClass* FI,
                        int XOff,
                        int YOff)
{
    int XMin = XOff - IrtMdlrSrfPaintShape->Width / 2;
    int YMin = (YOff - IrtMdlrSrfPaintShape->Height / 2);
    SrfPaintTextureStruct* Texture = 
        IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface];
    
    /* Modulo needs positive values to work as intended */
    while (XMin < 0) {
        XMin += Texture->Width;
    }
    while (YMin < 0) {
        YMin += Texture->Height;
    }
    XMin %= Texture->Width;
    YMin %= Texture->Height;

    for (int v = 0; v < IrtMdlrSrfPaintShape->Height; v++) {
        for (int u = 0; u < IrtMdlrSrfPaintShape->Width; u++) {
            int x = (XMin + u) % Texture->Width;
            int y = (YMin + v) % Texture->Height;
            int TextureOff = x + Texture->Width * y;
            int ShapeOff = u + IrtMdlrSrfPaintShape->Width * v;

            IrtMdlrSrfPaintTextureAlpha[TextureOff].r += 
                (IrtBType)((IrtMdlrSrfPaintColor[0] 
                - IrtMdlrSrfPaintTextureAlpha[TextureOff].r) 
                * IrtMdlrSrfPaintShape->Shape[ShapeOff]);
            IrtMdlrSrfPaintTextureAlpha[TextureOff].g += 
                (IrtBType)((IrtMdlrSrfPaintColor[1] 
                - IrtMdlrSrfPaintTextureAlpha[TextureOff].g) 
                * IrtMdlrSrfPaintShape->Shape[ShapeOff]);
            IrtMdlrSrfPaintTextureAlpha[TextureOff].b +=
                (IrtBType)((IrtMdlrSrfPaintColor[2] 
                - IrtMdlrSrfPaintTextureAlpha[TextureOff].b) 
                * IrtMdlrSrfPaintShape->Shape[ShapeOff]);
            Texture->Texture[TextureOff].r = 
                (IrtBType)(IrtMdlrSrfPaintTextureBuffer[TextureOff].r 
                + (IrtMdlrSrfPaintTextureAlpha[TextureOff].r 
                - IrtMdlrSrfPaintTextureBuffer[TextureOff].r) 
                * (IrtMdlrSrfPaintAlpha / 255.0));
            Texture->Texture[TextureOff].g = 
                (IrtBType)(IrtMdlrSrfPaintTextureBuffer[TextureOff].g 
                + (IrtMdlrSrfPaintTextureAlpha[TextureOff].g 
                - IrtMdlrSrfPaintTextureBuffer[TextureOff].g)
                * (IrtMdlrSrfPaintAlpha / 255.0));
            Texture->Texture[TextureOff].b = 
                (IrtBType)(IrtMdlrSrfPaintTextureBuffer[TextureOff].b 
                + (IrtMdlrSrfPaintTextureAlpha[TextureOff].b 
                - IrtMdlrSrfPaintTextureBuffer[TextureOff].b) 
                * (IrtMdlrSrfPaintAlpha / 255.0));
        }
    }
}

static int IrtMdlrSrfPaintMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent)
{
    IRT_DSP_STATIC_DATA bool Clicking = false;
    IRT_DSP_STATIC_DATA int PrevXOff = -1;
    IRT_DSP_STATIC_DATA int PrefYOff = -1;
    IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)MouseEvent->Data;
    IPObjectStruct* PObj = (IPObjectStruct*)MouseEvent->PObj;

    if (MouseEvent->KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN) {
        switch (MouseEvent->Type) {
        case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
            GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
            Clicking = TRUE;
            break;
        case IRT_DSP_MOUSE_EVENT_LEFT_UP:
            GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
            Clicking = FALSE;
            if (IrtMdlrSrfPaintSurface != NULL) {
                SrfPaintTextureStruct* Texture 
                    = IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface];

                for (int y = 0; y < Texture->Height; y++) {
                    for (int x = 0; x < Texture->Width; x++) {
                        int offset = y * Texture->Width + x;
                        IrtMdlrSrfPaintTextureAlpha[offset] = 
                            Texture->Texture[offset];
                        IrtMdlrSrfPaintTextureBuffer[offset] = 
                            Texture->Texture[offset];
                    }
                }
            }
            PrevXOff = -1;
            PrefYOff = -1;
            break;
        }
        if (Clicking && MouseEvent->UV != NULL && PObj != NULL 
            && PObj == IrtMdlrSrfPaintSurface) {
            SrfPaintTextureStruct* Texture = 
                IrtMdlrSrfPaintTextures[IrtMdlrSrfPaintSurface];

            int XOff = 
                (int)((double) Texture->Width * fmod(MouseEvent->UV[0], 1)),
                YOff = 
                (int)((double)Texture->Height * fmod(MouseEvent->UV[1], 1));
            if (PrefYOff == -1) {
                IrtMdlrSrfPaintRenderShape(FI, XOff, YOff);
                PrevXOff = XOff;
                PrefYOff = YOff;
            }
            else {
                int XBackup = XOff,
                    YBackup = YOff;
                pair<int, int> Start, End;
                vector<pair<int, int>> Points;

                if (D(PrevXOff, XOff - Texture->Width) < D(PrevXOff, XOff)) {
                    XOff -= Texture->Width;
                }
                if (D(PrevXOff, XOff + Texture->Width) < D(PrevXOff, XOff)) {
                    XOff += Texture->Width;
                }
                if (D(PrefYOff, YOff - Texture->Height) < D(PrefYOff, YOff)) {
                    YOff -= Texture->Height;
                }
                if (D(PrefYOff, YOff + Texture->Height) < D(PrefYOff, YOff)) {
                    YOff += Texture->Height;
                }
                Start = pair<int, int>(PrevXOff, PrefYOff);
                End = pair<int, int>(XOff, YOff);
                IrtMdlrSrfPaintBresenham(Start, End, Points);
                for (pair<int, int>& p : Points) {
                    IrtMdlrSrfPaintRenderShape(FI, 
                        p.first % Texture->Width, 
                        p.second % Texture->Height);
                }
                PrevXOff = XBackup;
                PrefYOff = YBackup;
            }
            GuIritMdlrDllSetTextureFromImage(FI, 
                IrtMdlrSrfPaintSurface, 
                Texture->Texture, 
                Texture->Width, 
                Texture->Height, 
                FALSE);
        }
    }
    return TRUE;
}
