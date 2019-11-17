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

using namespace std::chrono;

#define SRF_PAINT_WIDTH 256
#define SRF_PAINT_HEIGHT 256
#define SRF_PAINT_SIZE 65536
IRT_DSP_STATIC_DATA IrtImgPixelStruct WHITE = { 255, 255, 255 };

struct SrfPaintTextureStruct {
    int Width;
    int Height;
    IrtImgPixelStruct* Texture;
};

struct SrfPaintShapeStruct {
    int Width;
    int Height;
    float* Shape;
};

static void IrtMdlrSrfPaint(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrSrfPaintResizeTexture(
                        IrtMdlrFuncInfoClass* FI, 
                        int Width,
                        int Height,
                        bool Reset);
static void IrtMdlrSrfPaintLoadShape(
                        IrtMdlrFuncInfoClass* FI,
                        const char* Filename);
static void IrtMdlrSrfPaintInitShapes(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrSrfPaintBresenham(
                        const std::pair<int, int>& Start,
                        const std::pair<int, int>& End,
                        std::vector<int>& Points);
static void IrtMdlrSrfPaintRenderShape(
                        IrtMdlrFuncInfoClass* FI,
                        int XOffset,
                        int YOffset);
static int IrtMdlrSrfPaintMouseCallBack(
                        IrtMdlrMouseEventStruct* MouseEvent);

IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass 
    *GlobalFI = NULL;

IRT_DSP_STATIC_DATA bool
    IrtMdlrSrfPaintTextureUpdate = false,
    IrtMdlrSrfPaintTextureInit = false,
    IrtMdlrSrfPaintShapeInit = false,
    IrtMdlrSrfPaintTimerReset = false;

IRT_DSP_STATIC_DATA IrtRType
    IrtMdlrSrfPaintTextureWidth = SRF_PAINT_WIDTH,
    IrtMdlrSrfPaintTextureHeight = SRF_PAINT_HEIGHT,
    IrtMdlrSrfPaintAlpha = 255,
    IrtMdlrSrfPaintXFactor = 1,
    IrtMdlrSrfPaintYFactor = 1;

IRT_DSP_STATIC_DATA IrtVecType
    IrtMdlrSrfPaintColor = { 0, 0, 0 };

IRT_DSP_STATIC_DATA IrtImgPixelStruct
    *IrtMdlrSrfPaintTextureAlpha = NULL,
    *IrtMdlrSrfPaintTextureBuffer = NULL;

IRT_DSP_STATIC_DATA SrfPaintShapeStruct
    *IrtMdlrSrfPaintShape = NULL;

IRT_DSP_STATIC_DATA IPObjectStruct
    *IrtMdlrSrfPaintSurface = NULL;

IRT_DSP_STATIC_DATA char 
    IrtMdlrDrfPaintShapeNames[IRIT_LINE_LEN_XLONG] = "";

IRT_DSP_STATIC_DATA const char
    **IrtMdlrDrfPaintShapeFiles = NULL;

IRT_DSP_STATIC_DATA std::map<IPObjectStruct*, SrfPaintTextureStruct*> 
    IrtMdlrSrfPaintTexture;

enum {
    SRF_PAINT_SURFACE = 0,
    SRF_PAINT_LOAD,
    SRF_PAINT_SAVE,
    SRF_PAINT_RESET,
    SRF_PAINT_TXTR_WIDTH,
    SRF_PAINT_TXTR_HEIGHT,
    SRF_PAINT_COLOR,
    SRF_PAINT_ALPHA,
    SRF_PAINT_SHAPE,
    SRF_PAINT_X_FACTOR,
    SRF_PAINT_Y_FACTOR
};

IRT_DSP_STATIC_DATA IrtMdlrFuncTableStruct SrfPaintFunctionTable[] = {
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
            IRT_MDLR_SURFACE_EXPR,
            IRT_MDLR_BUTTON_EXPR,
            IRT_MDLR_BUTTON_EXPR,
            IRT_MDLR_BUTTON_EXPR,
            IRT_MDLR_NUMERIC_EXPR,
            IRT_MDLR_NUMERIC_EXPR,
            IRT_MDLR_VECTOR_EXPR,
            IRT_MDLR_NUMERIC_EXPR,
            IRT_MDLR_HIERARCHY_SELECTION_EXPR,
            IRT_MDLR_NUMERIC_EXPR,
            IRT_MDLR_NUMERIC_EXPR 
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
            IrtMdlrDrfPaintShapeNames,
            &IrtMdlrSrfPaintXFactor,
            &IrtMdlrSrfPaintYFactor
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
            "Width of the texture.", "Height of the texture.",
            "Color of the painting brush.",
            "Transparence factor of the painting brush.",
            "Shape of the painting brush.",
            "X factor of the painting brush.",
            "Y factor of the painting brush."
        } 
    },
};

IRT_DSP_STATIC_DATA const int SRF_PAINTER_FUNC_TABLE_SIZE = 
    sizeof(SrfPaintFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

static double diff(double a, double b)
{
    return fabs(a - b);
}

/*****************************************************************************
* DESCRIPTION:                                                               M
*   Registration function of GuIrit dll extension.  Always define as         M
*   'extern "C"' to prevent from name mangling.                              M
*                                                                            *
* PARAMETERS:                                                                M
*   None                                                                     M
*                                                                            *
* RETURN VALUE:                                                              M
*   extern "C" bool:     True if successful, false otherwise.                M
*                                                                            *
* KEYWORDS:                                                                  M
*   _IrtMdlrDllRegister                                                      M
*****************************************************************************/

extern "C" bool _IrtMdlrDllRegister(void)
{
    GuIritMdlrDllRegister(
        SrfPaintFunctionTable, 
        SRF_PAINTER_FUNC_TABLE_SIZE, 
        "Example", 
        IconMenuExample);
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
        GuIritMdlrDllSetRealInputDomain(FI, 1, 100000, SRF_PAINT_TXTR_WIDTH);
        GuIritMdlrDllSetRealInputDomain(FI, 1, 100000, SRF_PAINT_TXTR_HEIGHT);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, SRF_PAINT_COLOR, 0);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, SRF_PAINT_COLOR, 1);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, SRF_PAINT_COLOR, 2);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, SRF_PAINT_ALPHA);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, SRF_PAINT_X_FACTOR);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, SRF_PAINT_Y_FACTOR);
        GlobalFI = FI;
    }

    if (GlobalFI != NULL && (
        FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK ||
        FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_CANCEL)) {
        GuIritMdlrDllPopMouseEventFunc(FI);
        GlobalFI = false;
    }

    IPObjectStruct* Surface = NULL;
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_SURFACE, &Surface);
    if (Surface != NULL && !IrtMdlrSrfPaintTextureUpdate) {
        IrtMdlrSrfPaintTextureUpdate = true;
        if (IrtMdlrSrfPaintTexture.find(Surface) 
            == IrtMdlrSrfPaintTexture.end()) {
            SrfPaintTextureStruct* Texture = new SrfPaintTextureStruct;
            Texture->Width = SRF_PAINT_WIDTH;
            Texture->Height = SRF_PAINT_HEIGHT;
            Texture->Texture = new IrtImgPixelStruct[SRF_PAINT_SIZE];
            for (int i = 0; i < SRF_PAINT_SIZE; i++) {
                Texture->Texture[i] = WHITE;
            }
            IrtMdlrSrfPaintTexture[Surface] = Texture;
        }
        if (Surface != IrtMdlrSrfPaintSurface) {
            SrfPaintTextureStruct* Texture = IrtMdlrSrfPaintTexture[Surface];
            IrtRType Width = (IrtRType)(Texture->Width);
            IrtRType Height = (IrtRType)(Texture->Height);
            GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_TXTR_WIDTH, &Width);
            GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_TXTR_HEIGHT, &Height);
            IrtMdlrSrfPaintSurface = Surface;
            IrtMdlrSrfPaintTextureAlpha = 
                new IrtImgPixelStruct[SRF_PAINT_SIZE];
            IrtMdlrSrfPaintTextureBuffer = 
                new IrtImgPixelStruct[SRF_PAINT_SIZE];
            for (int i = 0; i < SRF_PAINT_SIZE; i++) {
                IrtMdlrSrfPaintTextureAlpha[i] = Texture->Texture[i];
                IrtMdlrSrfPaintTextureBuffer[i] = Texture->Texture[i];
            }
            IrtMdlrSrfPaintTextureInit = true;
        }
        IrtMdlrSrfPaintTextureUpdate = false;
    }

    // Texture fields
    if (FI->IntermediateWidgetMajor == SRF_PAINT_LOAD 
        && !IrtMdlrSrfPaintTextureUpdate && IrtMdlrSrfPaintSurface != NULL) {
        char* Filename;
        bool Res = GuIritMdlrDllGetAsyncInputFileName(
            FI, 
            "Load Texture from....", 
            "*.png", 
            &Filename);
        if (Res) {
            int Width, Height, Alpha;
            IrtImgPixelStruct* Image = 
                IrtImgReadImage2(Filename, &Width, &Height, &Alpha);
            Width++, Height++;
            IrtMdlrSrfPaintResizeTexture(FI, Width, Height, false);
            SrfPaintTextureStruct* Texture = 
                IrtMdlrSrfPaintTexture[Surface];
            for (int i = 0; i < Width * Height; i++) {
                Texture->Texture[i] = Image[i];
                IrtMdlrSrfPaintTextureAlpha[i] = Image[i];
                IrtMdlrSrfPaintTextureBuffer[i] = Image[i];
            }
            GuIritMdlrDllSetTextureFromImage(
                FI,
                IrtMdlrSrfPaintSurface,
                Texture->Texture, 
                Texture->Width,
                Texture->Height, 
                FALSE);
            IrtMdlrSrfPaintTextureUpdate = true;
            IrtRType TmpWidth = (IrtRType)Texture->Width;
            IrtRType TmpHeight = (IrtRType)Texture->Height;
            GuIritMdlrDllSetInputParameter(
                FI, 
                SRF_PAINT_TXTR_WIDTH, 
                &TmpWidth);
            GuIritMdlrDllSetInputParameter(
                FI, 
                SRF_PAINT_TXTR_HEIGHT, 
                &TmpHeight);
            IrtMdlrSrfPaintTextureUpdate = false;
        }
    }

    if (FI->IntermediateWidgetMajor == SRF_PAINT_SAVE) {
        if (IrtMdlrSrfPaintSurface != NULL) {
            char* Filename;
            bool Res = GuIritMdlrDllGetAsyncInputFileName(
                FI, 
                "Save Texture to....", 
                "*.png", 
                &Filename);
            if (Res) {
                SrfPaintTextureStruct* Texture = 
                    IrtMdlrSrfPaintTexture[Surface];
                MiscWriteGenInfoStructPtr GI = IrtImgWriteOpenFile(
                    NULL, 
                    Filename, 
                    IRIT_IMAGE_PNG_TYPE, 
                    false, 
                    Texture->Width, 
                    Texture->Height);
                if (GI == NULL)
                    GuIritMdlrDllPrintf(
                        FI, 
                        IRT_DSP_LOG_ERROR, 
                        "Error saving texture to \"%s\"\n", 
                        Filename);
                else {
                    for (int y = 0; y < Texture->Height; y++)
                        IrtImgWritePutLine(
                            GI, 
                            NULL, 
                            &Texture->Texture[y * Texture->Width]);
                    IrtImgWriteCloseFile(GI);
                }
            }
        }
    }

    if (FI->IntermediateWidgetMajor == SRF_PAINT_RESET 
        && !IrtMdlrSrfPaintTextureUpdate && IrtMdlrSrfPaintSurface != NULL) {
        bool Res = GuIritMdlrDllGetAsyncInputConfirm(
            FI, 
            "", 
            "Are you sure you want to reset the texture ?");
        if (Res) {
            SrfPaintTextureStruct* Texture = IrtMdlrSrfPaintTexture[Surface];
            IrtMdlrSrfPaintResizeTexture(
                FI, 
                SRF_PAINT_WIDTH, 
                SRF_PAINT_HEIGHT, 
                true);
            IrtMdlrSrfPaintTextureUpdate = true;
            IrtRType Width = (IrtRType)SRF_PAINT_WIDTH;
            IrtRType Height = (IrtRType)SRF_PAINT_HEIGHT;
            GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_TXTR_WIDTH, &Width);
            GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_TXTR_HEIGHT, &Height);
            IrtMdlrSrfPaintTextureUpdate = false;
            GuIritMdlrDllSetTextureFromImage(
                FI, 
                IrtMdlrSrfPaintSurface,   
                Texture->Texture, 
                Texture->Width, 
                Texture->Height, 
                FALSE);
        }
    }

    GuIritMdlrDllGetInputParameter(
        FI, 
        SRF_PAINT_TXTR_WIDTH, 
        &IrtMdlrSrfPaintTextureWidth);
    GuIritMdlrDllGetInputParameter(
        FI, 
        SRF_PAINT_TXTR_HEIGHT, 
        &IrtMdlrSrfPaintTextureHeight);
    int TmpWidth = (int)IrtMdlrSrfPaintTextureWidth,
        TmpHeight = (int)IrtMdlrSrfPaintTextureHeight;
    if (IrtMdlrSrfPaintSurface != NULL) {
        SrfPaintTextureStruct* Texture = IrtMdlrSrfPaintTexture[Surface];
        if ((Texture->Width != TmpWidth || Texture->Height != TmpHeight) 
            && !IrtMdlrSrfPaintTextureUpdate) {
            IRT_DSP_STATIC_DATA high_resolution_clock::time_point reset_timer;
            IrtMdlrSrfPaintTextureUpdate = true;
            bool res = true;
            if (IrtMdlrSrfPaintTextureInit) {
                if (!IrtMdlrSrfPaintTimerReset || 
                    duration_cast<seconds>(high_resolution_clock::now() 
                    - reset_timer).count() >= 20) {
                    res = GuIritMdlrDllGetAsyncInputConfirm(
                        FI, 
                        "", 
                        "This will reset the texture.\n"
                        "Are you sure you want to resize the texture ?");
                    if (res) {
                        IrtMdlrSrfPaintTimerReset = true;
                        reset_timer = high_resolution_clock::now();
                    }
                }
            }
            if (res) {
                IrtMdlrSrfPaintResizeTexture(FI, TmpWidth, TmpHeight, true);
                GuIritMdlrDllSetTextureFromImage(
                    FI,
                    IrtMdlrSrfPaintSurface,
                    Texture->Texture,
                    Texture->Width,
                    Texture->Height,
                    FALSE);
            }
            else {
                IrtRType Width = (IrtRType)Texture->Width;
                IrtRType Height = (IrtRType)Texture->Height;
                GuIritMdlrDllSetInputParameter(
                    FI, 
                    SRF_PAINT_TXTR_WIDTH, 
                    &Width);
                GuIritMdlrDllSetInputParameter(
                    FI, 
                    SRF_PAINT_TXTR_HEIGHT,
                    &Height);
            }
            IrtMdlrSrfPaintTextureUpdate = false;
        }
    }

    // Brush fields
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_COLOR, &IrtMdlrSrfPaintColor);
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_ALPHA, &IrtMdlrSrfPaintAlpha);

    IRT_DSP_STATIC_DATA int ShapeIndex = -1;
    char* Tmp = IrtMdlrDrfPaintShapeNames;
    int Index;

    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_SHAPE, &Index, &Tmp);
    if (Index != ShapeIndex) {
        ShapeIndex = Index;
        IrtMdlrSrfPaintLoadShape(FI, IrtMdlrDrfPaintShapeFiles[ShapeIndex]);
    }

    IrtRType tmp_x, tmp_y;

    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_X_FACTOR, &tmp_x);
    GuIritMdlrDllGetInputParameter(FI, SRF_PAINT_Y_FACTOR, &tmp_y);

    if (diff(tmp_x, IrtMdlrSrfPaintXFactor) > 10e-5 
        || diff(tmp_y, IrtMdlrSrfPaintYFactor) > 10e-5) {
        IrtMdlrSrfPaintXFactor = tmp_x;
        IrtMdlrSrfPaintYFactor = tmp_y;
        IrtMdlrSrfPaintLoadShape(FI, IrtMdlrDrfPaintShapeFiles[ShapeIndex]);
    }
}

static void IrtMdlrSrfPaintResizeTexture(
                        IrtMdlrFuncInfoClass* FI, 
                        int Width, 
                        int Height, 
                        bool Reset)
{
    delete[] IrtMdlrSrfPaintTexture[IrtMdlrSrfPaintSurface]->Texture;
    delete[] IrtMdlrSrfPaintTextureAlpha;
    delete[] IrtMdlrSrfPaintTextureBuffer;
    IrtImgPixelStruct* Texture = new IrtImgPixelStruct[Width * Height];
    IrtImgPixelStruct* TextureAlpha = new IrtImgPixelStruct[Width * Height];
    IrtImgPixelStruct* TextureBuffer = new IrtImgPixelStruct[Width * Height];
    if (Reset) {
        for (int y = 0; y < Height; y++) {
            for (int x = 0; x < Width; x++) {
                Texture[y * Width + x] = WHITE;
                TextureAlpha[y * Width + x] = WHITE;
                TextureBuffer[y * Width + x] = WHITE;
            }
        }
    }
    IrtMdlrSrfPaintTexture[IrtMdlrSrfPaintSurface]->Texture = Texture;
    IrtMdlrSrfPaintTexture[IrtMdlrSrfPaintSurface]->Width = Width;
    IrtMdlrSrfPaintTexture[IrtMdlrSrfPaintSurface]->Height = Height;
    IrtMdlrSrfPaintTextureAlpha = TextureAlpha;
    IrtMdlrSrfPaintTextureBuffer = TextureBuffer;
}

void IrtMdlrSrfPaintInitShapes(IrtMdlrFuncInfoClass* FI)
{
    int Index = 0;
    const char *p, *q;
    char Base[IRIT_LINE_LEN_LONG], 
        Path[IRIT_LINE_LEN_LONG],
        RelativePath[IRIT_LINE_LEN_LONG] = "\\Example\\Masks";
    const IrtDspGuIritSystemInfoStruct* Props = 
        GuIritMdlrDllGetGuIritSystemProps(FI);
    sprintf(
        Path, 
        "%s%s", 
        searchpath(Props->AuxiliaryDataName, Base), 
        RelativePath);
    if (IrtMdlrDrfPaintShapeFiles == NULL)
        IrtMdlrDrfPaintShapeFiles = 
            GuIritMdlrDllGetAllFilesNamesInDirectory(
                FI, 
                Path, 
                "*.rle|*.ppm|*.gif|*.jpeg|*.png");

    strcpy(IrtMdlrDrfPaintShapeNames, "");
    for (int i = 0; IrtMdlrDrfPaintShapeFiles[i] != NULL; ++i) {
        p = strstr(IrtMdlrDrfPaintShapeFiles[i], RelativePath);
        if (p != NULL) {
            p += strlen(RelativePath) + 1;
            char Name[IRIT_LINE_LEN_LONG];
            strcpy(Name, "");
            q = strchr(p, '.');
            strncpy(Name, p, q - p);
            Name[q - p] = '\0';
            if (strstr(Name, "GaussianFull25") != NULL) {
                Index = i;
            }
            sprintf(
                IrtMdlrDrfPaintShapeNames, 
                "%s%s;", 
                IrtMdlrDrfPaintShapeNames, 
                Name);
        }
    }

    if (IrtMdlrDrfPaintShapeFiles[0] != NULL) {
        sprintf(
            IrtMdlrDrfPaintShapeNames, 
            "%s:%d", 
            IrtMdlrDrfPaintShapeNames, 
            Index);
        GuIritMdlrDllSetInputSelectionStruct 
            FilesNames(IrtMdlrDrfPaintShapeNames);
        GuIritMdlrDllSetInputParameter(FI, SRF_PAINT_SHAPE, &FilesNames);
    }
}

static void IrtMdlrSrfPaintLoadShape(
                        IrtMdlrFuncInfoClass* FI,
                        const char* filename)
{
    int Width, Height, Alpha;
    IrtImgPixelStruct* image = IrtImgReadImage2(
        filename, 
        &Width, 
        &Height, 
        &Alpha);
    Width++, Height++;
    if (IrtMdlrSrfPaintShapeInit)
        delete[] IrtMdlrSrfPaintShape->Shape;
    else
        IrtMdlrSrfPaintShape = new SrfPaintShapeStruct;
        IrtMdlrSrfPaintShapeInit = true;

    IrtMdlrSrfPaintShape->Width = (int)(Width * IrtMdlrSrfPaintXFactor);
    IrtMdlrSrfPaintShape->Height = (int)(Height * IrtMdlrSrfPaintYFactor);

    double XRatio = 1.0 / IrtMdlrSrfPaintXFactor;
    double YRatio = 1.0 / IrtMdlrSrfPaintYFactor;

    IrtMdlrSrfPaintShape->Shape = 
        new float[IrtMdlrSrfPaintShape->Width * IrtMdlrSrfPaintShape->Height];
    for (int y = 0; y < IrtMdlrSrfPaintShape->Height; y++) {
        for (int x = 0; x < IrtMdlrSrfPaintShape->Width; x++) {
            float gray;
            IrtImgPixelStruct* ptr = 
                image + (int)((int)(y * YRatio) * Width + x * XRatio);
            IRT_DSP_RGB2GREY(ptr, gray);
            IrtMdlrSrfPaintShape->Shape[y * IrtMdlrSrfPaintShape->Width + x] = 
                gray / 255.0f;
        }
    }
}

static void IrtMdlrSrfPaintBresenham(
                        const std::pair<int, int>& Start,
                        const std::pair<int, int>& End,
                        std::vector<std::pair<int, int>>& Points) 
{
    int A1, A2, B1, B2, High, Da, Db, n = 1, d, b;
    if (abs(End.first - Start.first) > abs(End.second - Start.second)) {
        A1 = Start.first;
        B1 = Start.second;
        A2 = End.first;
        B2 = End.second;
        High = 0;
    }
    else {
        A1 = Start.second;
        B1 = Start.first;
        A2 = End.second;
        B2 = End.first;
        High = 1;
    }
    if (A1 > A2) {
        std::swap(A1, A2);
        std::swap(B1, B2);
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
    b = B1;
    for (int a = A1; a <= A2; a++) {
        int x = (High) ? b : a;
        int y = (High) ? a : b;
        std::pair<int, int> p(x, y);
        Points.push_back(p);
        if (d > 0) {
            b += n;
            d -= 2 * Da;
        }
        d += 2 * Db;
    }
}

static void IrtMdlrSrfPaintRenderShape(
                        IrtMdlrFuncInfoClass* FI,
                        int XOff,
                        int YOff)
{
    SrfPaintTextureStruct* Texture = 
        IrtMdlrSrfPaintTexture[IrtMdlrSrfPaintSurface];
    int XMin = XOff - IrtMdlrSrfPaintShape->Width / 2,
        YMin = (YOff - IrtMdlrSrfPaintShape->Height / 2);

    /* Modulo op requires positive int to work as intended */
    while (XMin < 0)
        XMin += Texture->Width;
    while (YMin < 0)
        YMin += Texture->Height;
    XMin %= Texture->Width;
    YMin %= Texture->Height;

    for (int u = 0; u < IrtMdlrSrfPaintShape->Height; u++) {
        for (int v = 0; v < IrtMdlrSrfPaintShape->Width; v++) {
            int x = (XMin + v) % Texture->Width;
            int y = (YMin + u) % Texture->Height;
            int TextOff = x + Texture->Width * y;
            int ShapeOff = v + IrtMdlrSrfPaintShape->Width * u;

            IrtMdlrSrfPaintTextureAlpha[TextOff].r += 
                (IrtBType)((IrtMdlrSrfPaintColor[0]   
                - IrtMdlrSrfPaintTextureAlpha[TextOff].r) 
                * IrtMdlrSrfPaintShape->Shape[ShapeOff]);
            IrtMdlrSrfPaintTextureAlpha[TextOff].g += 
                (IrtBType)((IrtMdlrSrfPaintColor[1] 
                - IrtMdlrSrfPaintTextureAlpha[TextOff].g) 
                * IrtMdlrSrfPaintShape->Shape[ShapeOff]);
            IrtMdlrSrfPaintTextureAlpha[TextOff].b += 
                (IrtBType)((IrtMdlrSrfPaintColor[2] 
                - IrtMdlrSrfPaintTextureAlpha[TextOff].b) 
                * IrtMdlrSrfPaintShape->Shape[ShapeOff]);

            Texture->Texture[TextOff].r =
                (IrtBType)(IrtMdlrSrfPaintTextureBuffer[TextOff].r 
                + (IrtMdlrSrfPaintTextureAlpha[TextOff].r 
                - IrtMdlrSrfPaintTextureBuffer[TextOff].r) 
                * (IrtMdlrSrfPaintAlpha / 255.0));
            Texture->Texture[TextOff].g = 
                (IrtBType)(IrtMdlrSrfPaintTextureBuffer[TextOff].g 
                + (IrtMdlrSrfPaintTextureAlpha[TextOff].g 
                - IrtMdlrSrfPaintTextureBuffer[TextOff].g) 
                * (IrtMdlrSrfPaintAlpha / 255.0));
            Texture->Texture[TextOff].b = 
                (IrtBType)(IrtMdlrSrfPaintTextureBuffer[TextOff].b 
                + (IrtMdlrSrfPaintTextureAlpha[TextOff].b 
                - IrtMdlrSrfPaintTextureBuffer[TextOff].b) 
                * (IrtMdlrSrfPaintAlpha / 255.0));
        }
    }
}

static int IrtMdlrSrfPaintMouseCallBack(IrtMdlrMouseEventStruct* Event)
{
    IRT_DSP_STATIC_DATA bool Click = false;
    IRT_DSP_STATIC_DATA int PrevXOff = -1, PrevYOff = -1;
    IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)Event->Data;
    IPObjectStruct* PObj = (IPObjectStruct*)Event->PObj;

    if (Event->KeyModifiers & IRT_DSP_KEY_MODIFIER_SHIFT_DOWN) {
        switch (Event->Type) {
        case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
            GuIritMdlrDllCaptureCursorFocus(FI, Event, true);
            Click = true;
            break;
        case IRT_DSP_MOUSE_EVENT_LEFT_UP:
            GuIritMdlrDllCaptureCursorFocus(FI, Event, false);
            Click = false;
            if (IrtMdlrSrfPaintSurface != NULL) {
                SrfPaintTextureStruct* texture = 
                    IrtMdlrSrfPaintTexture[IrtMdlrSrfPaintSurface];
                for (int i = 0; i < texture->Height; i++) {
                    IrtMdlrSrfPaintTextureAlpha[i] = texture->Texture[i];
                    IrtMdlrSrfPaintTextureBuffer[i] = texture->Texture[i];
                }
            }
            PrevXOff = -1, PrevYOff = -1;
            break;
        }
        if (Click && Event->UV != NULL 
            && PObj != NULL && PObj == IrtMdlrSrfPaintSurface) {
            SrfPaintTextureStruct* texture = 
                IrtMdlrSrfPaintTexture[IrtMdlrSrfPaintSurface];
            int XOff = (int)((double)texture->Width * fmod(Event->UV[0], 1));
            int YOff = (int)((double)texture->Height * fmod(Event->UV[1], 1));
            if (PrevYOff == -1) {
                IrtMdlrSrfPaintRenderShape(FI, XOff, YOff);
                PrevXOff = XOff;
                PrevYOff = YOff;
            }
            else {
                int x_backup = XOff;
                int y_backup = YOff;
                if (diff(PrevXOff, XOff - texture->Width) < diff(PrevXOff, XOff))
                    XOff -= texture->Width;
                if (diff(PrevXOff, XOff + texture->Width) < diff(PrevXOff, XOff))
                    XOff += texture->Width;
                if (diff(PrevYOff, YOff - texture->Height) < diff(PrevYOff, YOff))
                    YOff -= texture->Height;
                if (diff(PrevYOff, YOff + texture->Height) < diff(PrevYOff, YOff))
                    YOff += texture->Height;
                std::vector<std::pair<int, int>> points;
                std::pair<int, int> start(PrevXOff, PrevYOff), end(XOff, YOff);
                IrtMdlrSrfPaintBresenham(start, end, points);
                for (std::pair<int, int>& p : points) {
                    IrtMdlrSrfPaintRenderShape(
                        FI, 
                        p.first % texture->Width, 
                        p.second % texture->Height);
                }
                PrevXOff = x_backup;
                PrevYOff = y_backup;
            }
            GuIritMdlrDllSetTextureFromImage(FI, IrtMdlrSrfPaintSurface, 
                texture->Texture, texture->Width, texture->Height, FALSE);
        }
    }
    return TRUE;
}
