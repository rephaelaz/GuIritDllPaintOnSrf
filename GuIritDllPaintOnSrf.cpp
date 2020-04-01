/******************************************************************************
* GuiIritDllPaintOnSrf.cpp - painting textures over surfaces.		      *
*******************************************************************************
* (C) Gershon Elber, Technion, Israel Institute of Technology                 *
*******************************************************************************
* Written by Ilan Coronel and Raphael Azoulay, 2019.		              *
******************************************************************************/

#include <ctype.h>
#include <stdlib.h>

#include "IrtDspBasicDefs.h"
#include "IrtMdlr.h"
#include "IrtMdlrFunc.h"
#include "IrtMdlrDll.h"
#include "GuIritDllExtensions.h"

/* Include and define function icons. */
#include "Icons/IconPaintOnSrf.xpm"

#include <vector>
#include <map>
#include <sstream>
#include <algorithm> 

using std::vector;
using std::map;
using std::pair;
using std::swap;
using std::min;
using std::max;

#define IRT_MDLR_POS_DFLT_WIDTH  256
#define IRT_MDLR_POS_DFLT_HEIGHT 256
#define IRT_MDLR_POS_MAX_WIDTH  4096
#define IRT_MDLR_POS_MAX_HEIGHT 4096
#define IRT_MDLR_POS_EPSILON  	 10e-5
#define IRT_MDLR_POS_DFLT_SIZE   (IRT_MDLR_POS_DFLT_WIDTH) * \
                                               (IRT_MDLR_POS_DFLT_HEIGHT)
#define IRT_MDLR_POS_DIST(a, b) (fabs((double)(a) - (double)(b)))

enum {
    IRT_MDLR_POS_SURFACE = 0,
    IRT_MDLR_POS_LOAD,
    IRT_MDLR_POS_SAVE,
    IRT_MDLR_POS_RESET,
    IRT_MDLR_POS_WIDTH,
    IRT_MDLR_POS_HEIGHT,
    IRT_MDLR_POS_COLOR,
    IRT_MDLR_POS_ALPHA,
    IRT_MDLR_POS_SHAPE,
    IRT_MDLR_POS_X_FACTOR,
    IRT_MDLR_POS_Y_FACTOR
};

#ifdef __WINNT__
#define GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH "\\SandArt\\Masks"
#else
#define GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH "/SandArt/Masks"
#endif /* __WINNT__ */

struct IrtMdlrPoSShapeStruct {
    IrtMdlrPoSShapeStruct(int Width, int Height, IrtRType Alpha,
        IrtRType XFactor, IrtRType YFactor, float *Shape):

        Width(Width),
        Height(Height),
        Alpha(Alpha),
        XFactor(XFactor),
        YFactor(YFactor),
        Shape(Shape) {}

    int Width, Height;
    IrtRType Alpha, XFactor, YFactor;
    float *Shape;
};

struct IrtMdlrPoSTexDataStruct {
    bool Saved, Resize, Model;
    int Width, Height, Alpha;
    IrtImgPixelStruct *Texture;
    map<CagdSrfStruct *, IrtImgPixelStruct *> TextureMap;
};

struct IrtMdlrPoSPartialDerivStruct {
    IrtMdlrPoSPartialDerivStruct(): Avg(0), Srf(NULL) {}

    CagdRType Avg;
    CagdSrfStruct *Srf;
};

struct IrtMdlrPoSDerivDataStruct {
    IrtMdlrPoSPartialDerivStruct u, v;
};

class IrtMdlrPaintOnSrfLclClass : public IrtMdlrLclDataClass {
public:
    IrtMdlrPaintOnSrfLclClass(const IrtMdlrFuncTableStruct *FuncTbl):
        IrtMdlrLclDataClass(NULL),
        ObjExpr(),
        TextureWidth(4),
        TextureHeight(4),
        ShapeIndex(-1),
        ShapeFiles(NULL),
        Base(0, 0, 255, 1, 1, NULL),
        Updated(0, 0, 255, 1, 1, NULL),
        Names(IrtMdlrSelectExprClass(ShapeNames, 0)),
        Object(NULL)
    {
        ParamVals[0] = (void *) &ObjExpr;
        ParamVals[1] = NULL;
        ParamVals[2] = NULL;
        ParamVals[3] = NULL;
        ParamVals[4] = (void *) &TextureWidth;
        ParamVals[5] = (void *) &TextureHeight;
        ParamVals[6] = NULL;
        ParamVals[7] = (void *) &Base.Alpha;
        ParamVals[8] = (void *) &Names;
        ParamVals[9] = (void *) &Base.XFactor;
        ParamVals[10] = (void *) &Base.YFactor;

        Color[0] = 0;
        Color[1] = 0;
        Color[2] = 0;

        strcpy(ShapeNames, "");

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                DefaultTexture[i][j].r = 255;
                DefaultTexture[i][j].g = 255;
                DefaultTexture[i][j].b = 255;
            }
        }
    }

    IrtMdlrObjectExprClass ObjExpr;
    int TextureWidth;
    int TextureHeight;
    int ShapeIndex;
    unsigned char Color[3];
    char ShapeNames[IRIT_LINE_LEN_XLONG];
    const char **ShapeFiles;
    IrtMdlrPoSShapeStruct Base;
    IrtMdlrPoSShapeStruct Updated;
    IrtMdlrSelectExprClass Names;
    IrtImgPixelStruct DefaultTexture[4][4];
    IrtImgPixelStruct *TextureAlpha;
    IrtImgPixelStruct *TextureBuffer;
    map<IPObjectStruct *, IrtMdlrPoSTexDataStruct *> TexDatas;
    IPObjectStruct *Object;
    IrtMdlrPoSDerivDataStruct Deriv;
    map<CagdSrfStruct *, IrtMdlrPoSDerivDataStruct> DerivMap;
};


static void IrtMdlrPaintOnSrf(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrPoSLoadTexture(IrtMdlrFuncInfoClass *FI,
    IrtMdlrPoSTexDataStruct *TexData,
    const char *path);
static void IrtMdlrPoSRecolorObject(IrtMdlrFuncInfoClass *FI,
    IPObjectStruct *Object);
static void IrtMdlrPoSResizeTexture(IrtMdlrFuncInfoClass *FI,
    IrtMdlrPoSTexDataStruct *TexData,
    int Width,
    int Height,
    bool Reset);
static int IrtMdlrPoSInitShapes(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrPoSLoadShape(IrtMdlrFuncInfoClass *FI,
    const char *Filename);
static void IrtMdlrPoSBresenham(const pair<int, int> &a,
    const pair<int, int> &b,
    vector<pair<int, int>> &Points);
static void IrtMdlrPoSRenderShape(IrtMdlrFuncInfoClass *FI,
    int XOff,
    int YOff);
static int IrtMdlrPoSMouseCallBack(IrtMdlrMouseEventStruct *MouseEvent);
static void IrtMdlrPoSShapeUpdate(IrtMdlrFuncInfoClass *FI,
    double u,
    double v);
static void IrtMdlrPoSApplyResize(IrtMdlrFuncInfoClass *FI, 
    CagdVType SuVec, 
    CagdVType SvVec);
static void IrtMdlrPoSApplyShear(IrtMdlrFuncInfoClass *FI, 
    CagdVType SuVec, 
    CagdVType SvVec);

IRT_DSP_STATIC_DATA IrtMdlrFuncTableStruct SrfPainterFunctionTable[] =
{
    {
        0,
        IconPaintOnSrf,
        "IRT_MDLR_PAINT_ON_SRF",
        "Paint on srf",
        "Texture Paint on surfaces",
        "A texture painter over surfaces using free-hand mouse interactions",
        IrtMdlrPaintOnSrf,
        NULL,
        IRT_MDLR_PARAM_VIEW_CHANGES_UDPATE
        | IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_DFLT_ON
    | IRT_MDLR_PARAM_NO_APPLY,
    IRT_MDLR_NUMERIC_EXPR,
    11,
    IRT_MDLR_PARAM_EXACT,
    {
        /* Surface selection. */
        IRT_MDLR_SRF_TSRF_MODEL_EXPR,

        /* Texture fields. */
        IRT_MDLR_BUTTON_EXPR,			  /* Load Texture. */
        IRT_MDLR_BUTTON_EXPR,			  /* Save Texture. */
        IRT_MDLR_BUTTON_EXPR,	                 /* Reset Texture. */
        IRT_MDLR_INTEGER_EXPR,	                 /* Texture width. */
        IRT_MDLR_INTEGER_EXPR,			/* Texture height. */

        /* Brush fields. */
        IRT_MDLR_BUTTON_EXPR,			    /* RGB Values. */
        IRT_MDLR_NUMERIC_EXPR,		    	   /* Alpha Value. */
        IRT_MDLR_HIERARCHY_SELECTION_EXPR,	  /* Shape selection menu. */
        IRT_MDLR_NUMERIC_EXPR,			      /* X Factor. */
        IRT_MDLR_NUMERIC_EXPR,			      /* Y Factor. */
},
        {
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
        },
        {
            "Surface",
            "Load Texture",
            "Save Texture",
            "Reset Texture",
            "Texture\nWidth",
            "Texture\nHeight",
            "Color",
            "Alpha",
            "Shape",
            "X\nFactor",
            "Y\nFactor"
        },
        {
            "Select a surface.",
            "Loads a texture from an image file.",
            "Saves the texture into an image file.",
            "Resets the current texture to a blank texture.",
            "Width of the texture.",
            "Height of the texture.",
            "Color of the painting brush.",
            "Transparency factor of the painting brush.",
            "Shape of the painting brush.",
            "X factor of the painting brush.",
            "Y factor of the painting brush."
        }
    }
};

IRT_DSP_STATIC_DATA const int SRF_PAINT_FUNC_TABLE_SIZE =
sizeof(SrfPainterFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

/* IDs into the IrtMdlrFuncTableStruct SrfPainterFunctionTable. */
enum {
    IRT_MDLR_PAINT_ON_SRF_FUNC_TABLE_ID
};

/*****************************************************************************
* DESCRIPTION:                                                               M
*   Registration function of GuIrit dll extension.  Always define as         M
* 'extern "C"' to prevent from name mangling.                                M
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
    GuIritMdlrDllRegister(SrfPainterFunctionTable,
        SRF_PAINT_FUNC_TABLE_SIZE,
        "PaintOnSrf",
        IconPaintOnSrf);
    return true;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Initialize the parameters of the GuIrit dll extension.                   *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPaintOnSrf(IrtMdlrFuncInfoClass *FI)
{
    IRT_DSP_STATIC_DATA bool
        PanelUpdate = false;
    IRT_DSP_STATIC_DATA char
        *LastObjName = NULL;
    IRT_DSP_STATIC_DATA IPObjectStruct
        *LastObj = NULL;
    int PrevWidth, PrevHeight;
    IrtRType
        Span[2] = {1.0, 1.0};
    IrtMdlrPaintOnSrfLclClass
        *LclData = NULL;

    if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_INIT) {
        FI -> LocalFuncData(new IrtMdlrPaintOnSrfLclClass(
            &SrfPainterFunctionTable[IRT_MDLR_PAINT_ON_SRF_FUNC_TABLE_ID]));
        return;
    }

    LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
        (FI -> LocalFuncData());

    if (FI -> InvocationNumber == 0 &&
        FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_INTERMEDIATE) {
        GuIritMdlrDllPushMouseEventFunc(FI, IrtMdlrPoSMouseCallBack,
            (IrtDspMouseEventType) (IRT_DSP_MOUSE_EVENT_LEFT),
            (IrtDspKeyModifierType) (IRT_DSP_KEY_MODIFIER_CTRL_DOWN), FI);

        if (!IrtMdlrPoSInitShapes(FI)) {
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
                "Failed to initialized cursors.\n");
            return;
        }

        GuIritMdlrDllSetIntInputDomain(FI, 4, IRT_MDLR_POS_MAX_WIDTH,
            IRT_MDLR_POS_WIDTH);
        GuIritMdlrDllSetIntInputDomain(FI, 4, IRT_MDLR_POS_MAX_HEIGHT,
            IRT_MDLR_POS_HEIGHT);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, IRT_MDLR_POS_ALPHA);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, IRT_MDLR_POS_X_FACTOR);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, IRT_MDLR_POS_Y_FACTOR);

        LclData -> TexDatas.clear();
        LclData -> Object = NULL;

        if (LastObj != NULL) {
            LastObj = NULL;
            if (LastObjName != NULL)
                IritFree(LastObjName);
            LastObjName = NULL;
        }

        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
            "Surface Painter initialized\n");
    }

    if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_CANCEL) {
        map<IPObjectStruct *, IrtMdlrPoSTexDataStruct *>::iterator it;

        for (it = LclData -> TexDatas.begin();
            it != LclData -> TexDatas.end();
            it++) {
            if (IP_IS_MODEL_OBJ(it -> first)) {
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "TODO Model Cancel restore\n");
            }
            else {
                const char *Path = AttrIDGetObjectStrAttrib(it->first, IRIT_ATTR_CREATE_ID(ptexture));

                if (!it -> second -> Saved) {
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
                        "Surface %s had changes that were not saved.\n",
                        it -> first -> ObjName);
                }
               
                if (Path != NULL) {
                    std::string Relative(Path);
                    std::istringstream Stream(Relative);
                    IrtMdlrPoSTexDataStruct
                        *TexData = LclData->TexDatas[LclData->Object];

                    getline(Stream, Relative, ',');

                    IrtMdlrPoSLoadTexture(FI, TexData, Relative.c_str());

                    GuIritMdlrDllSetTextureFromImage(FI,
                        it->first,
                        TexData->Texture,
                        TexData->Width,
                        TexData->Height,
                        TexData->Alpha,
                        Span);
                }
                else {                    
                    GuIritMdlrDllSetTextureFromImage(FI, it->first,
                        LclData -> DefaultTexture,
                        4, 4, FALSE, Span);
                    IrtMdlrPoSRecolorObject(FI, it->first);
                }
            }
        }
        return;
    }

    if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK) {
        map<IPObjectStruct *, IrtMdlrPoSTexDataStruct *>::iterator it;
        for (it = LclData->TexDatas.begin();
            it != LclData->TexDatas.end();
            it++) {
            if (IP_IS_MODEL_OBJ(it -> first)) {
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "TODO Model Ok save\n");
            }
            else {
                char *Filename;
                bool Res = GuIritMdlrDllGetAsyncInputFileName(FI,
                    "Save Texture to....",
                    "*.png", &Filename);
                if (Res) {
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "Euh\n");
                    IrtMdlrPoSTexDataStruct *TexData = it->second;
                    MiscWriteGenInfoStructPtr
                        GI = IrtImgWriteOpenFile(NULL, Filename, IRIT_IMAGE_PNG_TYPE,
                        TexData->Alpha, TexData->Width,
                        TexData->Height);
                    if (GI == NULL) {
                        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
                            "Error saving texture to \"%s\"\n",
                            Filename);
                    }
                    else {
                        int y;
                        for (y = 0; y < TexData->Height; y++) {
                            IrtImgWritePutLine(GI, NULL,
                                &TexData->Texture[y * TexData->Width]);
                        }
                        IrtImgWriteCloseFile(GI);
                        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
                            "Texture saved successfully to \"%s\"\n",
                            Filename);
                        TexData->Saved = true;

                        AttrIDSetObjectStrAttrib(it->first, IRIT_ATTR_CREATE_ID(ptexture), Filename);
                    }
                }
            }
        }
        return;
    }


    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_SURFACE,
        LclData->ObjExpr.GetObjAddr());

    if (LclData -> ObjExpr.GetIPObj() == NULL) {
        if (LastObj != NULL) {
            IritFree(LclData -> TexDatas[LastObj] -> Texture);
            LclData -> TexDatas.erase(LastObj);
            LastObj = NULL;
            LclData -> Object = NULL;
        }
        if (LastObjName != NULL)
            IritFree(LastObjName);
        LastObjName = NULL;
    }
    else {
        if (LastObj != NULL &&
            LastObj != LclData -> ObjExpr.GetIPObj() &&
            strcmp(LastObjName, LclData -> ObjExpr.GetIPObj() -> ObjName)
            == 0) {
            IritFree(LclData -> TexDatas[LastObj] -> Texture);
            LclData -> TexDatas.erase(LastObj);
        }
        LastObj = LclData -> ObjExpr.GetIPObj();
        if (LastObjName != NULL)
            IritFree(LastObjName);
        LastObjName = IritStrdup(LclData -> ObjExpr.GetIPObj() -> ObjName);
    }

    if (LclData -> ObjExpr.GetIPObj() != NULL &&
        GuIritMdlrDllGetObjectMatrix(FI, LclData -> ObjExpr.GetIPObj()) != NULL) {
        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
            "Paint on surface not supported for objects with transformation - \"Apply Transform\" to the object first.\n");
        return;
    }

    if (LclData -> ObjExpr.GetIPObj() != NULL && !PanelUpdate) {
        PanelUpdate = true;
        if (LclData -> TexDatas.find(LclData -> ObjExpr.GetIPObj()) ==
            LclData -> TexDatas.end()) {
            if (IP_IS_MODEL_OBJ(LclData -> ObjExpr.GetIPObj())) {
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "TODO Model init textures\n");
            }
            else {
                int i;
                IrtMdlrPoSTexDataStruct
                    *TexData = (IrtMdlrPoSTexDataStruct *)
                    IritMalloc(sizeof(IrtMdlrPoSTexDataStruct));

                TexData -> Width = IRT_MDLR_POS_DFLT_WIDTH;
                TexData -> Height = IRT_MDLR_POS_DFLT_HEIGHT;
                TexData -> Alpha = 0;
                TexData -> Saved = false;
                TexData -> Resize = false;
                TexData -> Texture = (IrtImgPixelStruct *)
                    IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);
                for (i = 0; i < IRT_MDLR_POS_DFLT_SIZE; i++) {
                    TexData -> Texture[i].r = 255;
                    TexData -> Texture[i].g = 255;
                    TexData -> Texture[i].b = 255;
                }
                LclData -> TexDatas[LclData -> ObjExpr.GetIPObj()] = TexData;

                /* Apply dummy white texture. This ensure UV mapping is correct.*/
                GuIritMdlrDllSetTextureFromImage(FI, 
                    LclData -> ObjExpr.GetIPObj(),
                    LclData -> DefaultTexture,
                    4, 4, FALSE, Span);

                IrtMdlrPoSRecolorObject(FI, LclData -> ObjExpr.GetIPObj());
            }
        }
        if (LclData -> ObjExpr.GetIPObj() != LclData -> Object) {
            if (IP_IS_MODEL_OBJ(LclData -> ObjExpr.GetIPObj())) {
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "TODO Model init buffers and derivatives\n");
            }
            else {
                CagdRType UMin, UMax, VMin, VMax, i, j;
                CagdVType SuVec, SvVec,
                    SUMSuVec = {0, 0, 0},
                    SUMSvVec = {0, 0, 0};
                IrtMdlrPoSTexDataStruct
                    *TexData = LclData -> TexDatas[LclData -> ObjExpr.GetIPObj()];

                GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH,
                    &TexData -> Width);
                GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT,
                    &TexData -> Height);
                LclData -> Object = LclData -> ObjExpr.GetIPObj();
                LclData -> TextureAlpha = (IrtImgPixelStruct *)
                    IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);
                LclData -> TextureBuffer = (IrtImgPixelStruct *)
                    IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);

                if (IP_IS_TRIMSRF_OBJ(LclData->Object)) {
                    LclData -> Deriv.u.Srf = CagdSrfDerive(LclData->Object->U.TrimSrfs->Srf, CAGD_CONST_U_DIR);
                    LclData -> Deriv.v.Srf = CagdSrfDerive(LclData->Object->U.TrimSrfs->Srf, CAGD_CONST_V_DIR);
                    CagdSrfDomain(LclData->Object->U.TrimSrfs->Srf, &UMin, &UMax, &VMin, &VMax);
                }
                else {
                    LclData -> Deriv.u.Srf = CagdSrfDerive(LclData->Object->U.Srfs, CAGD_CONST_U_DIR);
                    LclData -> Deriv.v.Srf = CagdSrfDerive(LclData->Object->U.Srfs, CAGD_CONST_V_DIR);
                    CagdSrfDomain(LclData->Object->U.Srfs, &UMin, &UMax, &VMin, &VMax);
                }

                LclData -> Deriv.u.Avg = 0;
                LclData -> Deriv.v.Avg = 0;

                for (i = UMin; i <= UMax; i += (UMax - UMin) / 10) {
                    for (j = VMin; j <= VMax; j += (VMax - VMin) / 10) {
                        CAGD_SRF_EVAL_E3(LclData -> Deriv.u.Srf, i, j, SuVec);
                        CAGD_SRF_EVAL_E3(LclData -> Deriv.v.Srf, i, j, SvVec);

                        LclData -> Deriv.u.Avg += sqrt(SuVec[0] * SuVec[0] + SuVec[1] * SuVec[1] + SuVec[2] * SuVec[2]);
                        LclData -> Deriv.v.Avg += sqrt(SvVec[0] * SvVec[0] + SvVec[1] * SvVec[1] + SvVec[2] * SvVec[2]);

                    }
                }

                LclData -> Deriv.u.Avg /= 100;
                LclData -> Deriv.v.Avg /= 100;
            }
        }
        PanelUpdate = false;
    }

    /* Texture fields. */
    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_LOAD &&
        !PanelUpdate &&
        LclData -> Object != NULL) {
        char *Path;
        bool
            Res = GuIritMdlrDllGetAsyncInputFileName(FI,
            "Load Texture from....",
            "*.png", &Path);

        if (Res) {
            IrtMdlrPoSTexDataStruct
                *TexData = LclData -> TexDatas[LclData -> Object];

            IrtMdlrPoSLoadTexture(FI, TexData, Path);

            GuIritMdlrDllSetTextureFromImage(FI,
                LclData -> Object,
                TexData -> Texture,
                TexData -> Width,
                TexData -> Height,
                TexData -> Alpha,
                Span);
            PanelUpdate = true;
            GuIritMdlrDllSetInputParameter(FI,
                IRT_MDLR_POS_WIDTH,
                &TexData -> Width);
            GuIritMdlrDllSetInputParameter(FI,
                IRT_MDLR_POS_HEIGHT,
                &TexData -> Height);
            PanelUpdate = false;
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_SAVE &&
        LclData -> Object != NULL) {
        char *Filename;
        bool
            Res = GuIritMdlrDllGetAsyncInputFileName(FI,
            "Save Texture to....",
            "*.png", &Filename);
        if (Res) {
            IrtMdlrPoSTexDataStruct
                *TexData = LclData -> TexDatas[LclData -> Object];
            MiscWriteGenInfoStructPtr
                GI = IrtImgWriteOpenFile(NULL, Filename, IRIT_IMAGE_PNG_TYPE,
                TexData -> Alpha, TexData -> Width,
                TexData -> Height);

            if (GI == NULL) {
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
                    "Error saving texture to \"%s\"\n",
                    Filename);
            }
            else {
                int y;

                for (y = 0; y < TexData -> Height; y++) {
                    IrtImgWritePutLine(GI, NULL,
                        &TexData -> Texture[y * TexData -> Width]);
                }
                IrtImgWriteCloseFile(GI);
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
                    "Texture saved successfully to \"%s\"\n",
                    Filename);
                TexData -> Saved = true;
            }
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_RESET &&
        !PanelUpdate &&
        LclData -> Object != NULL) {
        bool
            Res = GuIritMdlrDllGetAsyncInputConfirm(FI,
            "Texture Reset",
            "Are you sure you want to reset the texture ?");

        if (Res) {
            int Width = IRT_MDLR_POS_DFLT_WIDTH,
                Height = IRT_MDLR_POS_DFLT_HEIGHT;
            IrtMdlrPoSTexDataStruct
                *TexData = LclData -> TexDatas[LclData -> Object];

            IrtMdlrPoSResizeTexture(FI,
                TexData,
                IRT_MDLR_POS_DFLT_WIDTH,
                IRT_MDLR_POS_DFLT_HEIGHT,
                true);
            TexData -> Saved = false;
            PanelUpdate = true;
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH, &Width);
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT, &Height);
            PanelUpdate = false;
            GuIritMdlrDllSetTextureFromImage(FI, LclData -> Object,
                TexData -> Texture,
                TexData -> Width,
                TexData -> Height,
                TexData -> Alpha,
                Span);

        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_COLOR) {
        unsigned char Red, Green, Blue;

        if (GuIritMdlrDllGetAsyncInputColor(FI, &Red, &Green, &Blue)) {
            LclData -> Color[0] = Red;
            LclData -> Color[1] = Green;
            LclData -> Color[2] = Blue;
        }
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_WIDTH,
        &LclData -> TextureWidth);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_HEIGHT,
        &LclData -> TextureHeight);
    PrevWidth = LclData -> TextureWidth;
    PrevHeight = LclData -> TextureHeight;
    if (LclData -> TextureHeight % 4 != 0) {
        LclData -> TextureHeight -= LclData -> TextureHeight % 4;
        LclData -> TextureHeight += 4;

    }
    if (LclData -> TextureWidth % 4 != 0) {
        LclData -> TextureWidth -= LclData -> TextureWidth % 4;
        LclData -> TextureWidth += 4;
    }

    if (LclData -> Object != NULL) {
        IrtMdlrPoSTexDataStruct
            *TexData = LclData -> TexDatas[LclData -> Object];

        if ((TexData -> Width != LclData -> TextureWidth ||
            TexData -> Height != LclData -> TextureHeight) &&
            !PanelUpdate) {
            bool
                Res = true;

            PanelUpdate = true;
            if (!TexData -> Resize) {
                Res = GuIritMdlrDllGetAsyncInputConfirm(FI, "",
                    "This will reset the texture.\n" \
                    "Are you sure you want to resize the texture ?");
                if (Res) {
                    TexData -> Resize = true;
                }
            }

            if (Res) {
                if (PrevHeight != LclData -> TextureHeight) {
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
                        "Invalid Height (not a multiple of 4), changed to %d\n",
                        LclData -> TextureHeight);
                }
                if (PrevWidth != LclData -> TextureWidth) {
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
                        "Invalid Width (not a multiple of 4), changed to %d\n",
                        LclData -> TextureWidth);
                }
                IrtMdlrPoSResizeTexture(FI,
                    TexData,
                    LclData -> TextureWidth,
                    LclData -> TextureHeight, true);
                TexData -> Saved = false;
                GuIritMdlrDllSetTextureFromImage(FI, LclData -> Object,
                    TexData -> Texture,
                    TexData -> Width,
                    TexData -> Height,
                    TexData -> Alpha,
                    Span);
            }
            else {
                GuIritMdlrDllSetInputParameter(FI,
                    IRT_MDLR_POS_WIDTH,
                    &TexData -> Width);
                GuIritMdlrDllSetInputParameter(FI,
                    IRT_MDLR_POS_HEIGHT,
                    &TexData -> Height);
            }
            PanelUpdate = false;
        }
    }

    /* Brush fields. */
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_ALPHA,
        &LclData -> Base.Alpha);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_SHAPE, &LclData -> Names);

    if (LclData -> Names.GetIndex() != LclData -> ShapeIndex &&
        LclData -> ShapeFiles != NULL) {
        LclData -> ShapeIndex = LclData -> Names.GetIndex();
        IrtMdlrPoSLoadShape(FI,
            LclData -> ShapeFiles[LclData -> Names.GetIndex()]);
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_X_FACTOR, &LclData -> Base.XFactor);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_Y_FACTOR, &LclData -> Base.YFactor);
}

static void IrtMdlrPoSLoadTexture(IrtMdlrFuncInfoClass *FI, 
    IrtMdlrPoSTexDataStruct *TexData, 
    const char *Path) 
{
    int Width, Height,
        Alpha = 0;
    IrtImgPixelStruct
        *Image = IrtImgReadImage2(Path, &Width, &Height, &Alpha);

    Width++, Height++;
    TexData->Alpha = Alpha;
    TexData->Saved = false;
    TexData->Resize = false;

    if (Width % 4 != 0) {
        Width -= Width % 4;
        Width += 4;
    }
    if (Height % 4 != 0) {
        Height -= Height % 4;
        Height += 4;
    }

    IrtMdlrPoSResizeTexture(FI, TexData, Width, Height, true);

    int offsetW = 0;
    if (Width % 4 != 0) {
        offsetW = 4 - Width % 4;
    }
    for (int h = 0; h < Height; h++) {
        for (int w = 0; w < Width; w++) {
            TexData->Texture[h * Width + w + (h * offsetW)] =
                Image[h * Width + w];
        }
    }
}

static void IrtMdlrPoSRecolorObject(IrtMdlrFuncInfoClass *FI,
    IPObjectStruct *Object)
{
    IrtDspOGLObjPropsClass *OGLProps;

    AttrIDSetObjectRGBColor(Object, 255, 255, 255);

    OGLProps = GuIritMdlrDllGetObjectOGLProps(FI, Object);

    if (OGLProps != NULL) {
        IrtDspRGBAClrClass
            White = IrtDspRGBAClrClass(255, 255, 255);

        OGLProps -> FrontFaceColor.SetValue(White);
        OGLProps -> BackFaceColor.SetValue(White);
        OGLProps -> BDspListNeedsUpdate = true;
        GuIritMdlrDllRedrawScreen(FI);
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Resize the texture of the selected object.                               *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Width:   New Width of the texture.                                       *
*   Height:  New Height of the texture.                                      *
*   Reset:   Set the texture to blank.                                       *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSResizeTexture(IrtMdlrFuncInfoClass *FI,
    IrtMdlrPoSTexDataStruct *TexData,
    int Width,
    int Height,
    bool Reset)
{
    IrtImgPixelStruct
        *NewTexture = (IrtImgPixelStruct *)
        IritMalloc(sizeof(IrtImgPixelStruct) * Width * Height);
    IrtImgPixelStruct
        *TextureAlpha = (IrtImgPixelStruct *)
        IritMalloc(sizeof(IrtImgPixelStruct) * Width * Height);
    IrtImgPixelStruct
        *TextureBuffer = (IrtImgPixelStruct *)
        IritMalloc(sizeof(IrtImgPixelStruct) * Width * Height);
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
        (FI -> LocalFuncData());

    if (Reset) {
        int x, y;
        for (y = 0; y < Height; y++) {
            for (x = 0; x < Width; x++) {
                NewTexture[y * Width + x].r = 255;
                NewTexture[y * Width + x].g = 255;
                NewTexture[y * Width + x].b = 255;
            }
        }
    }
    IritFree(TexData -> Texture);
    IritFree(LclData -> TextureAlpha);
    IritFree(LclData -> TextureBuffer);
    LclData->TexDatas[LclData->Object]-> Width = Width;
    LclData->TexDatas[LclData->Object]-> Height = Height;
    LclData->TexDatas[LclData->Object]-> Texture = NewTexture;
    LclData -> TextureAlpha = TextureAlpha;
    LclData -> TextureBuffer = TextureBuffer;
}

/*****************************************************************************
* AUXILIARY:                                                                 *
* Auxiliary function to function IrtMdlrSrfPaint                             *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*                                                                            *
* RETURN VALUE:                                                              *
*   int:  TRUE if successful, FALSE if error.                                *
*****************************************************************************/
static int IrtMdlrPoSInitShapes(IrtMdlrFuncInfoClass *FI)
{
    int i,
        Index = 0;
    const char *p, *q;
    char Base[IRIT_LINE_LEN_LONG], FilePath[IRIT_LINE_LEN_LONG];
    const IrtDspGuIritSystemInfoStruct
        *SysFileNames = GuIritMdlrDllGetGuIritSystemProps(FI);
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
        (FI -> LocalFuncData());
    sprintf(FilePath, "%s%s",
        searchpath(SysFileNames -> AuxiliaryDataName, Base),
        GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH);

    if (LclData -> ShapeFiles == NULL) {
        LclData -> ShapeFiles =
            GuIritMdlrDllGetAllFilesNamesInDirectory(FI, FilePath,
            "*.rle|*.ppm|*.gif|*.jpeg|*.png");
    }

    if (LclData -> ShapeFiles == NULL) {
        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
            "Masks files were not found. Check directory: \"%s\"\n",
            FilePath);
        return FALSE;
    }

    strcpy(LclData -> ShapeNames, "");
    for (i = 0; LclData -> ShapeFiles[i] != NULL; ++i) {
        p = strstr(LclData -> ShapeFiles[i],
            GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH);
        if (p != NULL) {
            p += strlen(GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH) + 1;

            char Name[IRIT_LINE_LEN_LONG];

            strcpy(Name, "");
            q = strchr(p, '.');
            strncpy(Name, p, q - p);
            Name[q - p] = '\0';
            if (strstr(Name, "SquareFull25") != NULL) {
                Index = i;
            }
            sprintf(LclData -> ShapeNames, "%s%s;",
                LclData -> ShapeNames, Name);
        }
    }

    if (LclData -> ShapeFiles[0] != NULL) {
        LclData -> Names.SetUpdateSelectionsParams(TRUE);
        sprintf(LclData -> Names.GetStr(), "%s:%d", LclData -> Names.GetStr(), Index);
        GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_SHAPE, &LclData -> Names);
        LclData -> Names.SetUpdateSelectionsParams(FALSE);
    }
    else {
        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
            "Masks files were not found. Check directory: \"%s\"\n",
            FilePath);
        return FALSE;
    }

    return TRUE;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Load a shape to be used by the painter.                                  *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:       Function information - holds generic information such as Panel.*
*   Filename: Path to the file containing the shape.                         *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSLoadShape(IrtMdlrFuncInfoClass *FI,
    const char *Filename)
{
    int Width, Height, Size, x, y,
        Alpha = 0;
    float XRatio, YRatio;
    IrtImgPixelStruct
        *Image = IrtImgReadImage2(Filename, &Width, &Height, &Alpha);
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
        (FI -> LocalFuncData());
    if (Image == NULL) {
        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
            "Failed to load image \"%s\"\n", Filename);
        return;
    }

    Width++, Height++;
    IritFree(LclData -> Base.Shape);

    LclData -> Base.Width = Width;
    LclData -> Base.Height = Height;
    Size = LclData -> Base.Height * LclData -> Base.Width;
    LclData -> Base.Shape = (float *) IritMalloc(sizeof(float) * Size);
    XRatio = (float) Width / (float) LclData -> Base.Width;
    YRatio = (float) Height / (float) LclData -> Base.Height;

    for (y = 0; y < LclData -> Base.Height; y++) {
        for (x = 0; x < LclData -> Base.Width; x++) {
            int Off = y * LclData -> Base.Width + x;
            float gray;
            IrtImgPixelStruct
                *ptr = Image + (int) ((int) (y * YRatio) * Width + x * XRatio);

            IRT_DSP_RGB2GREY(ptr, gray);
            LclData -> Base.Shape[Off] = (float) (gray / 255.0);
        }
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Draw a line between two points on the texture.                           *
*                                                                            *
* PARAMETERS:                                                                *
*   a:       Starting point of the line to draw.                             *
*   b:       Ending point of the line to draw.                               *
*   Points:  The output points to draw.                                      *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSBresenham(const pair<int, int> &a,
    const pair<int, int> &b,
    vector<pair<int, int>> &Points)
{
    bool High;
    int A1, A2, B1, B2, Da, Db, d, u, v,
        n = 1;

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
    for (u = A1; u <= A2; u++) {
        int x = (High) ? v : u;
        int y = (High) ? u : v;
        pair<int, int>
            p = pair<int, int>(x, y);

        Points.push_back(p);
        if (d > 0) {
            v += n;
            d -= 2 * Da;
        }
        d += 2 * Db;
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Paint the current shape on the texture.                                  *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   XOff:    Offset on the X axis                                            *
*   YOff:    Offset on the y axis                                            *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSRenderShape(IrtMdlrFuncInfoClass *FI,
    int XOff,
    int YOff)
{
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
        (FI -> LocalFuncData());

    int u, v,
        XMin = XOff - LclData -> Updated.Width / 2,
        YMin = (YOff - LclData -> Updated.Height / 2);
    IrtMdlrPoSTexDataStruct
        *TexData = LclData -> TexDatas[LclData -> Object];

    /* Modulo needs positive values to work as intended */
    while (XMin < 0) {
        XMin += TexData -> Width;
    }
    while (YMin < 0) {
        YMin += TexData -> Height;
    }
    XMin %= TexData -> Width;
    YMin %= TexData -> Height;

    for (v = 0; v < LclData -> Updated.Height; v++) {
        for (u = 0; u < LclData -> Updated.Width; u++) {
            int x = (XMin + u) % TexData -> Width,
                y = (YMin + v) % TexData -> Height,
                TextureOff = x + TexData -> Width * y,
                ShapeOff = u + LclData -> Updated.Width * v;

            LclData -> TextureAlpha[TextureOff].r +=
                (IrtBType) ((LclData -> Color[0]
                - LclData -> TextureAlpha[TextureOff].r)
                * LclData -> Updated.Shape[ShapeOff]);
            LclData -> TextureAlpha[TextureOff].g +=
                (IrtBType) ((LclData -> Color[1]
                - LclData -> TextureAlpha[TextureOff].g)
                * LclData -> Updated.Shape[ShapeOff]);
            LclData -> TextureAlpha[TextureOff].b +=
                (IrtBType) ((LclData -> Color[2]
                - LclData -> TextureAlpha[TextureOff].b)
                * LclData -> Updated.Shape[ShapeOff]);
            TexData -> Texture[TextureOff].r =
                (IrtBType) (LclData -> TextureBuffer[TextureOff].r
                + (LclData -> TextureAlpha[TextureOff].r
                - LclData -> TextureBuffer[TextureOff].r)
                * (LclData -> Base.Alpha / 255.0));
            TexData -> Texture[TextureOff].g =
                (IrtBType) (LclData -> TextureBuffer[TextureOff].g
                + (LclData -> TextureAlpha[TextureOff].g
                - LclData -> TextureBuffer[TextureOff].g)
                * (LclData -> Base.Alpha / 255.0));
            TexData -> Texture[TextureOff].b =
                (IrtBType) (LclData -> TextureBuffer[TextureOff].b
                + (LclData -> TextureAlpha[TextureOff].b
                - LclData -> TextureBuffer[TextureOff].b)
                * (LclData -> Base.Alpha / 255.0));
        }
    }
    TexData -> Saved = false;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Callback function for mouse events for painting .          	             *
*                                                                            *
* PARAMETERS:                                                                *
*   MouseEvent:  The mouse call back event to handle.		             *
*                                                                            *
* RETURN VALUE:                                                              *
*   int: True if event handled, false to propagate event to next handler.    *
*****************************************************************************/
static int IrtMdlrPoSMouseCallBack(IrtMdlrMouseEventStruct *MouseEvent)
{
    IRT_DSP_STATIC_DATA bool
        Clicking = false;
    IRT_DSP_STATIC_DATA int
        PrevXOff = -1,
        PrefYOff = -1;
    IrtRType
        Span[2] = {1.0, 1.0};
    IrtMdlrFuncInfoClass
        *FI = (IrtMdlrFuncInfoClass *) MouseEvent -> Data;
    IPObjectStruct
        *PObj = (IPObjectStruct *) MouseEvent -> PObj;
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
        (FI -> LocalFuncData());

    if (MouseEvent -> KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN) {
        switch (MouseEvent -> Type) {
            case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
            GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
            Clicking = TRUE;
            if (LclData -> Object != NULL) {
                int x;
                IrtMdlrPoSTexDataStruct
                    *TexData = LclData -> TexDatas[LclData -> Object];

                for (x = 0; x < TexData -> Width * TexData -> Height; x++) {
                    LclData -> TextureAlpha[x] = TexData -> Texture[x];
                    LclData -> TextureBuffer[x] = TexData -> Texture[x];
                }
            }
            break;

            case IRT_DSP_MOUSE_EVENT_LEFT_UP:
            GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
            Clicking = FALSE;
            if (LclData -> Object != NULL) {
                int x;
                IrtMdlrPoSTexDataStruct
                    *TexData = LclData -> TexDatas[LclData -> Object];

                for (x = 0; x < TexData -> Width * TexData -> Height; x++) {
                    LclData -> TextureAlpha[x] = TexData -> Texture[x];
                    LclData -> TextureBuffer[x] = TexData -> Texture[x];
                }
            }
            PrevXOff = -1;
            PrefYOff = -1;
            break;

            default:
            break;
        }

        if (Clicking &&
            MouseEvent -> UVW != NULL &&
            PObj != NULL &&
            (IP_IS_SRF_OBJ(PObj) || IP_IS_TRIMSRF_OBJ(PObj) || IP_IS_MODEL_OBJ(PObj)) &&
            PObj == LclData -> Object) {

            IrtMdlrPoSTexDataStruct
                *TexData = LclData -> TexDatas[LclData -> Object];
            int XOff = (int) ((double) TexData -> Width *
                MouseEvent -> UVW[0]),
                YOff = (int) ((double) TexData -> Height *
                MouseEvent -> UVW[1]);
            if (PrefYOff == -1) {
                IrtMdlrPoSShapeUpdate(FI, MouseEvent->UVW[0], MouseEvent->UVW[1]);
                IrtMdlrPoSRenderShape(FI, XOff, YOff);
                PrevXOff = XOff;
                PrefYOff = YOff;
            }
            else {
                int XBackup = XOff,
                    YBackup = YOff;
                pair<int, int> Start, End;
                vector<pair<int, int>> Points;

                if (IRT_MDLR_POS_DIST(PrevXOff, XOff - TexData -> Width)
                    < IRT_MDLR_POS_DIST(PrevXOff, XOff))
                    XOff -= TexData -> Width;
                if (IRT_MDLR_POS_DIST(PrevXOff, XOff + TexData -> Width)
                    < IRT_MDLR_POS_DIST(PrevXOff, XOff))
                    XOff += TexData -> Width;
                if (IRT_MDLR_POS_DIST(PrefYOff, YOff - TexData -> Height)
                    < IRT_MDLR_POS_DIST(PrefYOff, YOff))
                    YOff -= TexData -> Height;
                if (IRT_MDLR_POS_DIST(PrefYOff, YOff + TexData -> Height)
                    < IRT_MDLR_POS_DIST(PrefYOff, YOff))
                    YOff += TexData -> Height;
                Start = pair<int, int>(PrevXOff, PrefYOff);
                End = pair<int, int>(XOff, YOff);
                IrtMdlrPoSBresenham(Start, End, Points);

                for (size_t i = 0; i < Points.size(); i++) {
                    IrtMdlrPoSShapeUpdate(FI,
                        (float) Points[i].first / (float) TexData -> Width,
                        (float) Points[i].second / (float) TexData -> Height);
                    IrtMdlrPoSRenderShape(FI,
                        Points[i].first % TexData -> Width,
                        Points[i].second % TexData -> Height);
                }

                PrevXOff = XBackup;
                PrefYOff = YBackup;
            }

            GuIritMdlrDllSetTextureFromImage(FI,
                LclData -> Object,
                TexData -> Texture,
                TexData -> Width,
                TexData -> Height,
                TexData -> Alpha,
                Span);

            GuIritMdlrDllRequestIntermediateUpdate(FI, false);
        }
    }

    return TRUE;
}

static void IrtMdlrPoSShapeUpdate(IrtMdlrFuncInfoClass *FI, double u, double v) {
    CagdRType UMin, UMax, VMin, VMax;
    CagdVType SuVec, SvVec, SuVecAvg, SvVecAvg;
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *> (FI->LocalFuncData());

    // Copy base shape
    IritFree(LclData -> Updated.Shape);

    LclData->Updated.XFactor = LclData->Base.XFactor;
    LclData->Updated.YFactor = LclData->Base.YFactor;

    LclData->Updated.Width = LclData->Base.Width;
    LclData->Updated.Height = LclData->Base.Height;
    int Size = LclData->Updated.Height * LclData->Updated.Width;
    LclData->Updated.Shape = (float *) IritMalloc(sizeof(float) * Size);

    for (int y = 0; y < LclData->Updated.Height; y++) {
        for (int x = 0; x < LclData->Updated.Width; x++) {
            int Offset = y * LclData->Updated.Width + x;
            LclData->Updated.Shape[Offset] = LclData->Base.Shape[Offset];
        }
    }

    if (IP_IS_TRIMSRF_OBJ(LclData->Object)) {
        CagdSrfDomain(LclData->Object->U.TrimSrfs->Srf, &UMin, &UMax, &VMin, &VMax);
    }
    else {
        CagdSrfDomain(LclData->Object->U.Srfs, &UMin, &UMax, &VMin, &VMax);
    }

    u = ((UMax - UMin) * u) + UMin;
    v = ((VMax - VMin) * v) + VMin;

    CAGD_SRF_EVAL_E3(LclData -> Deriv.u.Srf, u, v, SuVec);
    CAGD_SRF_EVAL_E3(LclData -> Deriv.v.Srf, u, v, SvVec);

    for (int i = 0; i < 3; i++) {
        SuVecAvg[i] = SuVec[i] / LclData -> Deriv.u.Avg;
        SvVecAvg[i] = SvVec[i] / LclData -> Deriv.v.Avg;
    }

    IrtMdlrPoSApplyResize(FI, SuVecAvg, SvVecAvg);
    IrtMdlrPoSApplyShear(FI, SuVec, SvVec);
}

static void IrtMdlrPoSApplyResize(IrtMdlrFuncInfoClass *FI, CagdVType SuVec, CagdVType SvVec) {
    double normU = sqrt(SuVec[0] * SuVec[0] + SuVec[1] * SuVec[1] + SuVec[2] * SuVec[2]);
    double normV = sqrt(SvVec[0] * SvVec[0] + SvVec[1] * SvVec[1] + SvVec[2] * SvVec[2]);

    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *> (FI->LocalFuncData());

    LclData->Updated.XFactor = abs(1 / normU) * LclData->Updated.XFactor;
    LclData->Updated.YFactor = abs(1 / normV) * LclData->Updated.YFactor;

    int oldWidth = LclData->Updated.Width;
    int oldHeight = LclData->Updated.Height;
    float *oldShape = LclData->Updated.Shape;

    LclData->Updated.Width = (int) (LclData->Updated.Width * LclData->Updated.XFactor);
    LclData->Updated.Height = (int) (LclData->Updated.Height * LclData->Updated.YFactor);
    int Size = LclData->Updated.Height * LclData->Updated.Width;
    LclData->Updated.Shape = (float *) IritMalloc(sizeof(float) * Size);

    float XRatio = (float) oldWidth / (float) LclData->Updated.Width;
    float YRatio = (float) oldHeight / (float) LclData->Updated.Height;

    for (int y = 0; y < LclData->Updated.Height; y++) {
        for (int x = 0; x < LclData->Updated.Width; x++) {
            int UpdatedOff = y * LclData->Updated.Width + x;
            int BaseOff = (int) ((int) (y * YRatio) * oldWidth + x * XRatio);
            LclData->Updated.Shape[UpdatedOff] = oldShape[BaseOff];
        }
    }

    IritFree(oldShape);
}

static void IrtMdlrPoSApplyShear(IrtMdlrFuncInfoClass *FI, CagdVType SuVec, CagdVType SvVec) {
    int x, y;
    double normU = sqrt(SuVec[0] * SuVec[0] + SuVec[1] * SuVec[1] + SuVec[2] * SuVec[2]);
    double normV = sqrt(SvVec[0] * SvVec[0] + SvVec[1] * SvVec[1] + SvVec[2] * SvVec[2]);
    double cos = (SuVec[0] * SvVec[0] + SuVec[1] * SvVec[1] + SuVec[2] * SvVec[2]) / (normU * normV);

    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *> (FI->LocalFuncData());

    if (abs(cos) < IRT_MDLR_POS_EPSILON) {
        return;
    }

    double Factor = (normV * abs(cos) * LclData -> TextureWidth) / (normU * LclData -> TextureHeight);
    double ShearWidth = Factor * LclData -> Updated.Width;
    int NewWidth = (int) (LclData -> Updated.Width + ShearWidth);
    float *NewShape = (float *)
        IritMalloc(sizeof(float) * LclData -> Updated.Height * NewWidth);

    for (x = 0; x < LclData -> Updated.Height * NewWidth; x++) {
        NewShape[x] = 0;
    }

    for (y = 0; y < LclData -> Updated.Height; y++) {
        for (x = 0; x < LclData -> Updated.Width; x++) {
            int NewX;
            if (cos > 0) {
                NewX = (int) (x + ShearWidth * (double) (LclData -> Updated.Height - y - 1) / (double) (LclData -> Updated.Height - 1));
            }
            else {
                NewX = (int) (x + ShearWidth * (double) (y) / (double) (LclData -> Updated.Height - 1));
            }
            int OldOff = y * LclData -> Updated.Width + x;
            int NewOff = y * NewWidth + NewX;
            NewShape[NewOff] = LclData -> Updated.Shape[OldOff];
        }
    }

    IritFree(LclData -> Updated.Shape);
    LclData -> Updated.Shape = NewShape;
    LclData -> Updated.Width = NewWidth;
}
