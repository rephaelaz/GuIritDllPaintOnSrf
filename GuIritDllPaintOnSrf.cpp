/*****************************************************************************
* GuiIritDllPaintOnSrf.cpp - painting textures over surfaces.		     *
******************************************************************************
* (C) Gershon Elber, Technion, Israel Institute of Technology                *
******************************************************************************
* Written by Ilan Coronel and Raphael Azoulay, 2019.		             *
*****************************************************************************/

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
using std::string;
using std::map;
using std::pair;
using std::find;

#define IRT_MDLR_POS_DFLT_WIDTH  256
#define IRT_MDLR_POS_DFLT_HEIGHT 256
#define IRT_MDLR_POS_MAX_WIDTH  4096
#define IRT_MDLR_POS_MAX_HEIGHT 4096
#define IRT_MDLR_POS_DFLT_SIZE \
    (IRT_MDLR_POS_DFLT_WIDTH) * (IRT_MDLR_POS_DFLT_HEIGHT)
#define IRT_MDLR_POS_DIST(a, b) (IRIT_ABS((a) - (b)))
#define IRT_MDLR_POS_DOT(u, v) \
    ((u)[0] * (v)[0] + (u)[1] * (v)[1] + (u)[2] * (v)[2])
#define IRT_MDLR_POS_DRAWABLE(o) \
    (IP_IS_SRF_OBJ(o) || IP_IS_TRIMSRF_OBJ(o) || IP_IS_MODEL_OBJ(o))

#ifdef __WINNT__
#define GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH "\\SandArt\\Masks"
#else
#define GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH "/SandArt/Masks"
#endif /* __WINNT__ */

enum {
    IRT_MDLR_POS_SURFACE = 0,
    IRT_MDLR_POS_LOAD,
    IRT_MDLR_POS_SAVE,
    IRT_MDLR_POS_RESET,
    IRT_MDLR_POS_WIDTH,
    IRT_MDLR_POS_HEIGHT,
    IRT_MDLR_POS_BG_COLOR,
    IRT_MDLR_POS_SHAPE_COLOR,
    IRT_MDLR_POS_ALPHA,
    IRT_MDLR_POS_SHAPE,
    IRT_MDLR_POS_X_FACTOR,
    IRT_MDLR_POS_Y_FACTOR
};

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

struct IrtMdlrPoSPartialDerivStruct {
    IrtMdlrPoSPartialDerivStruct(): Avg(0), Srf(NULL) {}

    CagdRType Avg;
    CagdSrfStruct *Srf;
};

struct IrtMdlrPoSDerivDataStruct {
    IrtMdlrPoSPartialDerivStruct u, v;
};

struct IrtMdlrPoSTexDataStruct {
    IrtMdlrPoSTexDataStruct(): 
        Texture(NULL),
        Original(NULL),
        Parent(NULL),
        ModelCopy(NULL),
        ModelSurface(NULL) {}

    bool Saved;
    int Width, Height, Alpha;
    IrtImgPixelStruct *Texture;
    IrtMdlrPoSDerivDataStruct Deriv;
    vector<IPObjectStruct *> Children;
    IPObjectStruct *Original;
    IPObjectStruct *Parent;
    IPObjectStruct *ModelCopy;
    CagdSrfStruct *ModelSurface;
};

class IrtMdlrPaintOnSrfLclClass : public IrtMdlrLclDataClass {
public:
    IrtMdlrPaintOnSrfLclClass(const IrtMdlrFuncTableStruct *FuncTbl):
        IrtMdlrLclDataClass(NULL),
        ObjExpr(),
        Resize(false),
        TextureWidth(4),
        TextureHeight(4),
        ShapeIndex(-1),
        ShapeFiles(NULL),
        Base(0, 0, 255, 1, 1, NULL),
        Updated(0, 0, 255, 1, 1, NULL),
        Names(IrtMdlrSelectExprClass(ShapeNames, 0)),
        TextureAlpha(NULL),
        TextureBuffer(NULL),
        Object(NULL)
    {
        ParamVals[0] = (void *) &ObjExpr;
        ParamVals[1] = NULL;
        ParamVals[2] = NULL;
        ParamVals[3] = NULL;
        ParamVals[4] = (void *) &TextureWidth;
        ParamVals[5] = (void *) &TextureHeight;
        ParamVals[6] = NULL;
        ParamVals[7] = NULL;
        ParamVals[8] = (void *) &Base.Alpha;
        ParamVals[9] = (void *) &Names;
        ParamVals[10] = (void *) &Base.XFactor;
        ParamVals[11] = (void *) &Base.YFactor;

        Span[0] = 1.0;
        Span[1] = 1.0;
        
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
    bool Resize;
    int TextureWidth;
    int TextureHeight;
    int ShapeIndex;
    IrtRType Span[2];
    unsigned char Color[3];
    char ShapeNames[IRIT_LINE_LEN_XLONG];
    const char **ShapeFiles;
    IrtMdlrPoSShapeStruct Base;
    IrtMdlrPoSShapeStruct Updated;
    IrtMdlrSelectExprClass Names;
    IrtImgPixelStruct DefaultTexture[4][4];
    IrtImgPixelStruct *TextureAlpha;
    IrtImgPixelStruct *TextureBuffer;
    map<IPObjectStruct *, IrtMdlrPoSTexDataStruct> TexDatas;
    vector<IPObjectStruct *> Selection;
    vector<IPObjectStruct *> CopyList;
    vector<IPObjectStruct *> OriginalList;
    IPObjectStruct *Object;
};


static void IrtMdlrPaintOnSrf(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrPoSInitSelection(IrtMdlrFuncInfoClass* FI,
				    IPObjectStruct* Object);
static void IrtMdlrPoSInitObject(IrtMdlrFuncInfoClass* FI,
				 IPObjectStruct* Object);
static void IrtMdlrPoSSetModelVisibility(IrtMdlrFuncInfoClass* FI,
					 IPObjectStruct* Object,
					 bool enabled);
static void IrtMdlrPoSInitTexture(IrtMdlrFuncInfoClass *FI,
				  IPObjectStruct *Object);
static void IrtMdlrPoSLoadTexture(IrtMdlrFuncInfoClass *FI,
				  IrtMdlrPoSTexDataStruct &TexData,
				  const char *Path);
static void IrtMdlrPoSSaveTexture(IrtMdlrFuncInfoClass *FI,
				  IPObjectStruct *Object,
				  char *Path = NULL);
static void IrtMdlrPoSResizeTexture(IrtMdlrFuncInfoClass *FI,
				    IrtMdlrPoSTexDataStruct &TexData,
				    int Width,
				    int Height,
				    bool Reset);
static void IrtMdlrPoSDeriveTexture(IrtMdlrFuncInfoClass *FI,
				    IPObjectStruct *Object);
static void IrtMdlrPoSRecolorObject(IrtMdlrFuncInfoClass *FI,
				    IPObjectStruct *Object);
static int IrtMdlrPoSInitShapes(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrPoSLoadShape(IrtMdlrFuncInfoClass *FI,
				const char *Filename);
static void IrtMdlrPoSBresenham(const pair<int, int> &a,
				const pair<int, int> &b,
				vector<pair<int, int>> &Points);
static void IrtMdlrPoSRenderShape(IrtMdlrFuncInfoClass *FI,
				  IPObjectStruct *Object,
				  int XOff,
				  int YOff);
static int IrtMdlrPoSMouseCallBack(IrtMdlrMouseEventStruct *MouseEvent);
static void IrtMdlrPoSShapeUpdate(IrtMdlrFuncInfoClass *FI,
				  IPObjectStruct *Object,
				  double u,
				  double v);
static void IrtMdlrPoSApplyResize(IrtMdlrFuncInfoClass *FI, 
				  CagdVType SuVec, 
				  CagdVType SvVec);
static void IrtMdlrPoSApplyShear(IrtMdlrFuncInfoClass *FI, 
				 CagdVType SuVec, 
				 CagdVType SvVec);
static CagdRType IrtMdlrEvalTangentMagnitudeOnBndry(const CagdSrfStruct *Srf,
						    CagdRType u,
						    CagdRType v,
						    const CagdVType UVUnitDir);
static void IrtMdlrComputeJunctionBetweenSrfs(const TrimSrfStruct *CrntSrf,
					      const CagdUVType CrntUV,
					      const TrimSrfStruct *NextSrf,
					      const CagdUVType NextUV,
					      CagdUVType CrntBndryUV,
					      CagdUVType NextBndryUV,
					      CagdRType *ScaleChange);

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
        12,
        IRT_MDLR_PARAM_EXACT,
        {
            /* Surface selection. */
            IRT_MDLR_OLST_GEOM_EXPR,

            /* Texture fields. */
            IRT_MDLR_BUTTON_EXPR,			   /* Load Texture. */
            IRT_MDLR_BUTTON_EXPR,			   /* Save Texture. */
            IRT_MDLR_BUTTON_EXPR,	                  /* Reset Texture. */
            IRT_MDLR_INTEGER_EXPR,	                  /* Texture width. */
            IRT_MDLR_INTEGER_EXPR,			 /* Texture height. */
            IRT_MDLR_BUTTON_EXPR,                  /* Ambient Color button. */

            /* Brush fields. */
            IRT_MDLR_BUTTON_EXPR,		     /* Shape Color button. */
            IRT_MDLR_NUMERIC_EXPR,		    	    /* Alpha Value. */
            IRT_MDLR_HIERARCHY_SELECTION_EXPR,	   /* Shape selection menu. */
            IRT_MDLR_NUMERIC_EXPR,			       /* X Factor. */
            IRT_MDLR_NUMERIC_EXPR,			       /* Y Factor. */
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
            NULL,
        },
        {
            "Surface",
            "Load Texture",
            "Save Geometry",
            "Reset Texture",
            "Texture\nWidth",
            "Texture\nHeight",
            "Background Color",
            "Shape Color",
            "Alpha",
            "Shape",
            "X\nFactor",
            "Y\nFactor"
        },
        {
            "Select a surface.",
            "Loads a texture from an image file.",
            "Saves the selected geometry and its textures into an .itd file.",
            "Resets the current texture to a blank texture.",
            "Width of the texture.",
            "Height of the texture.",
            "Color of the ambient background.",
            "Color of the painting shape.",
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

        GuIritMdlrDllPickGuIritObjTypeMask(FI, IRT_DSP_TYPE_OBJ_TEMP_GEOM);

        GuIritMdlrDllSetIntInputDomain(FI, 4, IRT_MDLR_POS_MAX_WIDTH,
            IRT_MDLR_POS_WIDTH);
        GuIritMdlrDllSetIntInputDomain(FI, 4, IRT_MDLR_POS_MAX_HEIGHT,
            IRT_MDLR_POS_HEIGHT);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, IRT_MDLR_POS_ALPHA);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, IRT_MDLR_POS_X_FACTOR);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, IRT_MDLR_POS_Y_FACTOR);

        LclData -> TexDatas.clear();
        LclData -> Selection.clear();

        if (LastObj != NULL) {
            LastObj = NULL;
            if (LastObjName != NULL)
                IritFree(LastObjName);
            LastObjName = NULL;
        }
    }

    if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK) {
        map<IPObjectStruct *, IrtMdlrPoSTexDataStruct>::iterator it;
        vector<IPObjectStruct *>::iterator Object;

        /* Save textures */
        for (it = LclData -> TexDatas.begin();
	     it != LclData -> TexDatas.end();
	     it++) {
            if (IP_IS_MODEL_OBJ(it -> first)) {
            }
            else {
                if (!it -> second.Saved) {
                    IrtMdlrPoSSaveTexture(FI, it -> first);
                }
            }
        }

        /* Insert textured geometries into DB */
        for (Object = LclData -> OriginalList.begin(); 
            Object != LclData -> OriginalList.end(); 
            Object++) {
            char Name[IRIT_LINE_LEN_XLONG];
            const char *Path;

            if (IP_IS_MODEL_OBJ(*Object)) {
                vector<IPObjectStruct *>::iterator Srf;
                IPObjectStruct
	                *CopyList = IPGenLISTObject(NULL),
                    *ToInsert = LclData -> TexDatas[*Object].ModelCopy;

                sprintf(Name, "TRLIST_%s", (*Object) -> ObjName);
                GuIritMdlrDllSetObjectName(FI, CopyList, Name);

                for (Srf = LclData -> TexDatas[*Object].Children.begin();
                    Srf != LclData -> TexDatas[*Object].Children.end();
                    Srf++) {
                    IrtMdlrPoSTexDataStruct
                        &TexData = LclData -> TexDatas[*Srf];

                    /* Duplicate trimmed surface */
                    IPObjectStruct *CopySrf = IPCopyObject(NULL, *Srf, false);

                    AttrFreeAttributes(&CopySrf -> Attr);
                    CopySrf -> Attr = NULL;
                    IP_SET_OBJ_NAME2(CopySrf, "TRIM");

                    Path = AttrIDGetObjectStrAttrib(*Srf, 
                        IRIT_ATTR_CREATE_ID(ptexture));
                    if (Path != NULL) {
                        AttrIDSetObjectStrAttrib(CopySrf, 
                            IRIT_ATTR_CREATE_ID(ptexture), 
                            Path);
                    }

                    IrtMdlrPoSRecolorObject(FI, CopySrf);
                    GuIritMdlrDllSetTextureFromImage(FI,
                        CopySrf,
                        TexData.Texture,
                        TexData.Width,
                        TexData.Height,
                        TexData.Alpha,
                        LclData -> Span);
                    IPListObjectAppend(CopyList, CopySrf);

                    /* Assign ptexture in model copy */
                    int CopyIndex = AttrIDGetIntAttrib(
                        (*Srf) -> U.TrimSrfs -> Srf -> Attr,
                        IRIT_ATTR_CREATE_ID(SrfIndex));

                    MdlTrimSrfStruct 
                        *MdlTSrf = ToInsert -> U.Mdls -> TrimSrfList;
                    while(MdlTSrf != NULL) {
                        int OriginalIndex = AttrIDGetIntAttrib(
                            MdlTSrf -> Srf -> Attr, 
					        IRIT_ATTR_CREATE_ID(SrfIndex));

                        if (CopyIndex == OriginalIndex) {
                            if (Path != NULL) {
                                AttrIDSetStrAttrib(
                                    &MdlTSrf -> Srf -> Attr, 
                                    IRIT_ATTR_CREATE_ID(ptexture), 
                                    Path);
                            }
                            break;
                        }
                        MdlTSrf = MdlTSrf -> Pnext;
                    }
                }
                GuIritMdlrDllInsertModelingNewObj(FI, CopyList);
                GuIritMdlrDllInsertModelingNewObj(FI, ToInsert);
                GuIritMdlrDllSetObjectVisible(FI, ToInsert, false);
            }
            else {
                IPObjectStruct 
                    *Hidden = LclData -> TexDatas[*Object].Children[0];
                IrtMdlrPoSTexDataStruct
                    &TexData = LclData -> TexDatas[Hidden];

                IPObjectStruct *Copy = IPCopyObject(NULL, *Object, false);
                sprintf(Name, "COPY_%s", (*Object) -> ObjName);

                AttrFreeAttributes(&Copy -> Attr);
                Copy -> Attr = NULL;
                IP_SET_OBJ_NAME2(Copy, Name);

                Path = AttrIDGetObjectStrAttrib(Hidden, 
                    IRIT_ATTR_CREATE_ID(ptexture));
                if (Path != NULL) {
                    AttrIDSetObjectStrAttrib(Copy, 
                        IRIT_ATTR_CREATE_ID(ptexture), 
                        Path);
                }

                IrtMdlrPoSRecolorObject(FI, Copy);
                GuIritMdlrDllSetTextureFromImage(FI,
                    Copy,
                    TexData.Texture,
                    TexData.Width,
                    TexData.Height,
                    TexData.Alpha,
                    LclData -> Span);
                GuIritMdlrDllInsertModelingNewObj(FI, Copy);
            }
        }
        return;
    }

    if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_CANCEL) {
        vector<IPObjectStruct *>::iterator it;

        /* Display back the hidden original geometries */
        for (it = LclData -> OriginalList.begin(); 
            it != LclData -> OriginalList.end(); 
            it++) {
            GuIritMdlrDllSetObjectVisible(FI, *it, true);
        }
        return;
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_SURFACE,
        LclData -> ObjExpr.GetObjAddr());

    if (LclData -> ObjExpr.GetIPObj() == NULL) {
        if (LastObj != NULL) {
            IritFree(LclData -> TexDatas[LastObj].Texture);
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
            IritFree(LclData -> TexDatas[LastObj].Texture);
            LclData -> TexDatas.erase(LastObj);
        }
        LastObj = LclData -> ObjExpr.GetIPObj();
        if (LastObjName != NULL)
            IritFree(LastObjName);
        LastObjName = IritStrdup(LclData -> ObjExpr.GetIPObj() -> ObjName);
    }

    if (LclData -> ObjExpr.GetIPObj() != NULL &&
        GuIritMdlrDllGetObjectMatrix(FI, LclData -> ObjExpr.GetIPObj())) {
        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
            "Paint on surface not supported for objects with transformation - \"Apply Transform\" to the object first.\n");
        return;
    }

    if (LclData -> ObjExpr.GetIPObj() != NULL && !PanelUpdate) {
        PanelUpdate = true;
        if (LclData -> ObjExpr.GetIPObj() != LclData -> Object) {
            LclData -> Selection.clear();
            IrtMdlrPoSInitSelection(FI, LclData -> ObjExpr.GetIPObj());
            LclData -> Object = LclData -> ObjExpr.GetIPObj();

            if (LclData -> Selection.size() > 0) {
                IrtMdlrPoSTexDataStruct
                    &TexData = LclData -> TexDatas[LclData -> Selection[0]];

                GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH,
                    &TexData.Width);
                GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT,
                    &TexData.Height);
                LclData -> Resize = false;
            }
        }
        PanelUpdate = false;
    }

    /* Texture fields. */
    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_LOAD &&
        !PanelUpdate &&
        LclData -> Object != NULL) {
        char *Path;

        bool Res = GuIritMdlrDllGetAsyncInputFileName(FI,
            "Load Texture from...",
            "PNG files (*.png)|*.png|JPG files (*.jpg)|*.jpg|"
            "PPM files (*.ppm)|*.ppm|GIF files (*.gif)|*.gif", 
            &Path);

        if (Res) {
            int Width = -1,
	        Height = -1;
            vector<IPObjectStruct *>::iterator it;

            for (it = LclData -> Selection.begin(); 
                it != LclData -> Selection.end(); 
                it++) {
                IrtMdlrPoSTexDataStruct
                    &TexData = LclData -> TexDatas[*it];

                IrtMdlrPoSLoadTexture(FI, TexData, Path);

                GuIritMdlrDllSetTextureFromImage(FI,
                    *it,
                    TexData.Texture,
                    TexData.Width,
                    TexData.Height,
                    TexData.Alpha,
                    LclData -> Span);
                Width = TexData.Width;
                Height = TexData.Height;
            }
            PanelUpdate = true;
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH, &Width);
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT, &Height);
            PanelUpdate = false;
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_SAVE &&
        LclData -> Object != NULL) {
        char *Path;
        bool Res = GuIritMdlrDllGetAsyncInputFileName(FI,
						      "Save Geometry from...",
						      "*.itd", 
						      &Path);

        if (Res) {
            int Index = 1;
            string
	        Directory = string(Path);
            IPObjectStruct
	        *SaveList = NULL;

            Directory = Directory.substr(0, Directory.size() - 4);
            
            vector<IPObjectStruct *>::iterator it;
            for (it = LclData -> Selection.begin(); 
                it != LclData -> Selection.end(); 
                it++) {
                char Filename[IRIT_LINE_LEN_XLONG];
                IrtMdlrPoSTexDataStruct
                    &TexData = LclData -> TexDatas[*it];

                sprintf(Filename, "%s_%d.png", Directory.c_str(), Index++);

                IrtMdlrPoSSaveTexture(FI, *it, Filename);
            }

            if (LclData -> Selection.size() == 1) {
                SaveList = LclData -> TexDatas[LclData -> Selection[0]].Parent;
            }
            else if (LclData -> Selection.size() > 1) {
                vector<IPObjectStruct *> Children;

                SaveList = IPGenLISTObject(NULL);
                for (it = LclData -> Selection.begin(); 
                    it != LclData -> Selection.end(); 
                    it++) {
                    IPObjectStruct
		        *Obj = LclData -> TexDatas[*it].Parent;

                    if (find(Children.begin(), Children.end(), Obj) ==
                        Children.end()) {
                        if (IP_IS_MODEL_OBJ(Obj)) {
                            GuIritMdlrDllSetObjectVisible(FI, Obj, true);
                        }
                        IPListObjectAppend(SaveList, Obj);
                        Children.push_back(Obj);
                    }
                }
                if (Children.size() == 1) {
                    SaveList = Children[0];
                }
            }
            
            if (SaveList != NULL) {
                IPPutObjectToFile3(Path, SaveList, 0);
                for (it = LclData -> Selection.begin(); 
                    it != LclData -> Selection.end(); 
                    it++) {
                    IPObjectStruct
		        *Obj = LclData -> TexDatas[*it].Parent;

                    if (IP_IS_MODEL_OBJ(Obj)) {
                        GuIritMdlrDllSetObjectVisible(FI, Obj, false);
                    }
                }
            }
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_RESET &&
        !PanelUpdate &&
        LclData -> Object != NULL) {
        bool Res = GuIritMdlrDllGetAsyncInputConfirm(FI,
						     "Texture Reset",
                              "Are you sure you want to reset the texture ?");

        if (Res) {
            int Width = IRT_MDLR_POS_DFLT_WIDTH,
                Height = IRT_MDLR_POS_DFLT_HEIGHT;
            vector<IPObjectStruct *>::iterator it;

            for (it = LclData -> Selection.begin(); 
		 it != LclData -> Selection.end(); 
		 it++) {
                IrtMdlrPoSTexDataStruct
                    &TexData = LclData -> TexDatas[*it];

                IrtMdlrPoSResizeTexture(FI, TexData, Width, Height, true);
                GuIritMdlrDllSetTextureFromImage(FI, *it, TexData.Texture,
						 TexData.Width,
						 TexData.Height,
						 TexData.Alpha,
						 LclData -> Span);
                TexData.Saved = false;
            }
            PanelUpdate = true;
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH, &Width);
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT, &Height);
            PanelUpdate = false;
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_BG_COLOR) {
        unsigned char Red, Green, Blue;
        IrtDspOGLWinPropsClass
            *OGLWinProps = GuIritMdlrDllGetWindowOGLProps(FI);

        if (GuIritMdlrDllGetAsyncInputColor(FI, &Red, &Green, &Blue)) {
            IrtDspRGBAClrClass Color(Red, Green, Blue);
            OGLWinProps -> WinAmbientIntensity.SetValue(Color);
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_SHAPE_COLOR) {
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

    if (LclData -> Selection.size() > 0) {
        IrtMdlrPoSTexDataStruct
            &TexData = LclData -> TexDatas[LclData -> Selection[0]];

        if ((TexData.Width != LclData -> TextureWidth ||
            TexData.Height != LclData -> TextureHeight) &&
            !PanelUpdate) {

            PanelUpdate = true;
            if (!LclData -> Resize) {
                LclData -> Resize = GuIritMdlrDllGetAsyncInputConfirm(FI, "",
                    "This will reset the texture.\n" \
                    "Are you sure you want to resize the texture ?");
            }

            if (LclData -> Resize) {
                if (PrevHeight != LclData -> TextureHeight) {
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
                        "Height is not a multiple of 4, changed to %d\n",
                        LclData -> TextureHeight);
                }
                if (PrevWidth != LclData -> TextureWidth) {
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
                        "Width  is not a multiple of 4, changed to %d\n",
                        LclData -> TextureWidth);
                }

                vector<IPObjectStruct *>::iterator it;

                for (it = LclData -> Selection.begin(); 
                    it != LclData -> Selection.end(); 
                    it++) {
                    IrtMdlrPoSTexDataStruct
                        &TexData = LclData -> TexDatas[*it];

                    IrtMdlrPoSResizeTexture(FI, TexData,
					    LclData -> TextureWidth,
					    LclData -> TextureHeight, true);
                    GuIritMdlrDllSetTextureFromImage(FI, *it,
						     TexData.Texture,
						     TexData.Width,
						     TexData.Height,
						     TexData.Alpha,
						     LclData -> Span);
                    TexData.Saved = false;
                }
            }
            else {
                GuIritMdlrDllSetInputParameter(FI,
                    IRT_MDLR_POS_WIDTH,
                    &TexData.Width);
                GuIritMdlrDllSetInputParameter(FI,
                    IRT_MDLR_POS_HEIGHT,
                    &TexData.Height);
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

    GuIritMdlrDllGetInputParameter(FI, 
				   IRT_MDLR_POS_X_FACTOR, 
				   &LclData -> Base.XFactor);
    GuIritMdlrDllGetInputParameter(FI, 
				   IRT_MDLR_POS_Y_FACTOR, 
				   &LclData -> Base.YFactor);
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Init the object with a default white texture. If the object is a list,   *
*   the function will initialize every object in it recursively.             *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Object:     The object to initialize and parse recursively.              *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSInitSelection(IrtMdlrFuncInfoClass* FI,
    IPObjectStruct* Object) {

    IrtMdlrPaintOnSrfLclClass
        * LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass*>
						      (FI -> LocalFuncData());

    if (IRT_MDLR_POS_DRAWABLE(Object)) {
        if (LclData -> TexDatas.find(Object) == LclData -> TexDatas.end()) {
            IrtMdlrPoSInitObject(FI, Object);
        }

        vector<IPObjectStruct *>::iterator it;

        for (it = LclData -> TexDatas[Object].Children.begin(); 
            it != LclData -> TexDatas[Object].Children.end(); 
            it++) {
            LclData -> Selection.push_back(*it);
        }
    }
    else if (IP_IS_OLST_OBJ(Object)) {
        int i;
        IPObjectStruct *Obj;

        for (i = 0; (Obj = IPListObjectGet(Object, i)) != NULL; i++) {
            IrtMdlrPoSInitSelection(FI, Obj);
        }
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Init the texture of the object, or the list of textures for a model      *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Object:     The object to initialize.                                    *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSInitObject(IrtMdlrFuncInfoClass* FI,
				 IPObjectStruct* Object)
{
    char Name[IRIT_LINE_LEN_XLONG];
    const char *Path;
    IrtMdlrPoSTexDataStruct TexData;

    TexData.Width = IRT_MDLR_POS_DFLT_WIDTH;
    TexData.Height = IRT_MDLR_POS_DFLT_HEIGHT;

    IrtMdlrPaintOnSrfLclClass
        * LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass*>
						      (FI -> LocalFuncData());

    // Dupplicate the object to leave original geometry unaltered
    IPObjectStruct *Copy = IPCopyObject(NULL, Object, false);
    sprintf(Name, "COPY_%s", Object -> ObjName);
        
    AttrFreeAttributes(&Copy -> Attr);
    Copy -> Attr = NULL;
    IP_SET_OBJ_NAME2(Copy, Name);

    Path = AttrIDGetObjectStrAttrib(Object, IRIT_ATTR_CREATE_ID(ptexture));
                
    if (Path != NULL) {
        AttrIDSetObjectStrAttrib(Copy, IRIT_ATTR_CREATE_ID(ptexture), Path);
    }

    LclData -> CopyList.push_back(Copy);

    if (IP_IS_MODEL_OBJ(Object)) {
        int index = 0;

        /* Assign unique ID to each surface of the model. */
        MdlTrimSrfStruct
	    *MdlTSrf = Copy -> U.Mdls -> TrimSrfList;

        while(MdlTSrf != NULL) {
            AttrIDSetIntAttrib(&MdlTSrf -> Srf -> Attr, 
                IRIT_ATTR_CREATE_ID(SrfIndex), 
                index++);
            MdlTSrf = MdlTSrf -> Pnext;
        }

        /* Generate copy to be inserted in the db on OK, to keep track of
           surface IDs */
        IPObjectStruct *CopyCopy = IPCopyObject(NULL, Copy, false);
        sprintf(Name, "COPY_%s", Object -> ObjName);
        
        AttrFreeAttributes(&CopyCopy -> Attr);
        CopyCopy -> Attr = NULL;
        IP_SET_OBJ_NAME2(CopyCopy, Name);

        TexData.ModelCopy = CopyCopy;

        /* Surface IDs should propagate to TSrfList. */
        TrimSrfStruct
	    *TSrfList = MdlCnvrtMdl2TrimmedSrfs(Copy -> U.Mdls, 0);
        IPObjectStruct
	    *ObjList = IPGenLISTObject(NULL);

        /* Generate object list that will be worked on. */
        sprintf(Name, "TRLIST_%s", Object -> ObjName);
        GuIritMdlrDllSetObjectName(FI, ObjList, Name);

        while(TSrfList != NULL) {
            TrimSrfStruct *TSrf;

            IRIT_LIST_POP(TSrf, TSrfList);

            int CopyIndex = AttrIDGetIntAttrib(TSrf -> Srf -> Attr, 
					       IRIT_ATTR_CREATE_ID(SrfIndex));
            IPObjectStruct
	        *Obj = IPGenTRIMSRFObject(TSrf);

            AttrIDSetObjectIntAttrib(Obj,
                IRIT_ATTR_CREATE_ID(SrfIndex),
                CopyIndex);

            GuIritMdlrDllSetObjectName(FI, Obj, "TRIM");
            IrtMdlrPoSInitTexture(FI, Obj);
            IrtMdlrPoSDeriveTexture(FI, Obj);
            IPListObjectAppend(ObjList, Obj);

            /* If original surface has ptexture, load it to the new surface. */
            const char
	        *Path = AttrIDGetStrAttrib(TSrf -> Srf -> Attr, 
					   IRIT_ATTR_CREATE_ID(ptexture));

            if (Path != NULL) {
                IrtMdlrPoSLoadTexture(FI, LclData -> TexDatas[Obj], Path);
            }

            /* Link model to child surface object. */
            TexData.Children.push_back(Obj);

            /* Link surface object to parent model and original surface. */
            LclData -> TexDatas[Obj].Parent = Copy;
            MdlTSrf = Copy -> U.Mdls -> TrimSrfList;
            while(MdlTSrf != NULL) {
                int OriginalIndex = AttrIDGetIntAttrib(MdlTSrf -> Srf -> Attr, 
					        IRIT_ATTR_CREATE_ID(SrfIndex));

                if (CopyIndex == OriginalIndex) {
                    LclData -> TexDatas[Obj].ModelSurface = MdlTSrf -> Srf;
                    break;
                }
                MdlTSrf = MdlTSrf -> Pnext;
            }
        };
        LclData -> CopyList.push_back(ObjList);
        GuIritMdlrDllAddTempDisplayObject(FI, ObjList, false);
        GuIritMdlrDllAddTempDisplayObject(FI, Copy, false);
        GuIritMdlrDllSetObjectVisible(FI, Copy, false);

        LclData -> OriginalList.push_back(Object);
        GuIritMdlrDllSetObjectVisible(FI, Object, false);

        LclData -> TexDatas[Object] = TexData;
    }
    else if (IP_IS_SRF_OBJ(Object) || IP_IS_TRIMSRF_OBJ(Object)) {
        IrtMdlrPoSInitTexture(FI, Copy);
        IrtMdlrPoSDeriveTexture(FI, Copy);

        /* Link original object to its copy. */
        TexData.Children.push_back(Copy);
        LclData -> TexDatas[Copy].Parent = Copy;

        LclData -> OriginalList.push_back(Object);
        GuIritMdlrDllSetObjectVisible(FI, Object, false);

        GuIritMdlrDllAddTempDisplayObject(FI, Copy, false);
        LclData -> TexDatas[Object] = TexData;
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Set the visibility of all models in the objet recursively.               *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Object:  The object tree containing models.                              *
*   Enabled: Set models visibles if true, hidden else.
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSSetModelVisibility(IrtMdlrFuncInfoClass* FI,
					 IPObjectStruct* Object,
					 bool enabled)
{
    if (IP_IS_MODEL_OBJ(Object)) {
        GuIritMdlrDllSetObjectVisible(FI, Object, enabled);
    }
    else if (IP_IS_OLST_OBJ(Object)) {
        int i;
        IPObjectStruct *Obj;

        for (i = 0; (Obj = IPListObjectGet(Object, i)) != NULL; i++) {
            IrtMdlrPoSSetModelVisibility(FI, Obj, enabled);
        }
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Init the texture of the object with a default white texture.             *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Object:  The object having the texture initialized.                      *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSInitTexture(IrtMdlrFuncInfoClass *FI,
				  IPObjectStruct *Object)
{
    int i;
    IrtMdlrPoSTexDataStruct TexData;
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
						      (FI -> LocalFuncData());

    TexData.Alpha = 0;
    TexData.Saved = false;
    TexData.Width = IRT_MDLR_POS_DFLT_WIDTH;
    TexData.Height = IRT_MDLR_POS_DFLT_HEIGHT;

    const char
        *Path = AttrIDGetObjectStrAttrib(Object, IRIT_ATTR_CREATE_ID(ptexture));
                
    if (Path != NULL) {
        IrtMdlrPoSLoadTexture(FI, TexData, Path);
    }
    else {
        TexData.Texture = (IrtImgPixelStruct *)
            IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);

        for (i = 0; i < IRT_MDLR_POS_DFLT_SIZE; i++) {
            TexData.Texture[i].r = 255;
            TexData.Texture[i].g = 255;
            TexData.Texture[i].b = 255;
        }

        /* Apply dummy white texture. This ensure UV mapping is correct.*/
        GuIritMdlrDllSetTextureFromImage(FI, Object, LclData -> DefaultTexture,
					 4, 4, FALSE, LclData -> Span);
    }

    IrtMdlrPoSRecolorObject(FI, Object);
    LclData -> TexDatas[Object] = TexData;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Load the texture of the object from a file.                              *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   TexData: Data about the texture to save.                                 *
*   Path:    Path to the file.                                               *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSLoadTexture(IrtMdlrFuncInfoClass *FI, 
				  IrtMdlrPoSTexDataStruct &TexData, 
				  const char *Path) 
{
    int Width, Height,
        Alpha = 0;
    IrtImgPixelStruct
        *Image = IrtImgReadImage2(Path, &Width, &Height, &Alpha);

    Width++, Height++;
    TexData.Alpha = Alpha;
    TexData.Saved = true;

    if (Width % 4 != 0) {
        Width -= Width % 4;
        Width += 4;
    }
    if (Height % 4 != 0) {
        Height -= Height % 4;
        Height += 4;
    }

    IrtMdlrPoSResizeTexture(FI, TexData, Width, Height, true);

    int h, w,
        offsetW = 0;

    if (Width % 4 != 0) {
        offsetW = 4 - Width % 4;
    }
    for (h = 0; h < Height; h++) {
        for (w = 0; w < Width; w++) {
            TexData.Texture[h * Width + w + (h * offsetW)] =
                Image[h * Width + w];
        }
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Save the texture of the object to a file.                                *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Object:  The object containing the texture to save.                      *
*   TexData: Data about the texture to save.                                 *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSSaveTexture(IrtMdlrFuncInfoClass *FI,
				  IPObjectStruct *Object,
				  char *Path)
{
    bool Res = true;
    char *Filename;
    char Header[IRIT_LINE_LEN_XLONG];
    int PathLength;
    IrtImgImageType IT;
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
						     (FI -> LocalFuncData());
    IrtMdlrPoSTexDataStruct
        &TexData = LclData -> TexDatas[Object];

    sprintf(Header, "Save %s texture to...", Object -> ObjName);

    if (Path != NULL) {
        Filename = Path;
    }
    else {
        Res = GuIritMdlrDllGetAsyncInputFileName(FI,
		            Header,
		            "PNG files (*.png)|*.png|JPG files (*.jpg)|*.jpg|"
		            "PPM files (*.ppm)|*.ppm|GIF files (*.gif)|*.gif", 
		            &Filename);
    }

    if (Res) {
        PathLength = (int)strlen(Filename);
        if (PathLength < 3) {
            return;
        }
        IT = IrtImgWriteGetType((char*)&Filename[PathLength - 3]);

        MiscWriteGenInfoStructPtr
            GI = IrtImgWriteOpenFile(NULL, Filename, IT,
				     TexData.Alpha, TexData.Width,
				     TexData.Height);

        if (GI == NULL) {
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
				"Error saving texture to \"%s\"\n",
				Filename);
        }
        else {
            int y;

            for (y = 0; y < TexData.Height; y++) {
                IrtImgWritePutLine(GI, NULL,
				   &TexData.Texture[y * TexData.Width]);
            }
            IrtImgWriteCloseFile(GI);
            TexData.Saved = true;

            AttrIDSetObjectStrAttrib(Object, IRIT_ATTR_CREATE_ID(ptexture), 
				     Filename);
            if (TexData.ModelSurface != NULL) {
                AttrIDSetStrAttrib(&TexData.ModelSurface -> Attr, 
				   IRIT_ATTR_CREATE_ID(ptexture), 
				   Filename);
            }
        }
    }
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Resize the texture of the selected object.                               *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   TexData: Data about the texture to resize.                               *
*   Width:   New Width of the texture.                                       *
*   Height:  New Height of the texture.                                      *
*   Reset:   Set the texture to blank.                                       *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSResizeTexture(IrtMdlrFuncInfoClass *FI,
				    IrtMdlrPoSTexDataStruct &TexData,
				    int Width,
				    int Height,
				    bool Reset)
{
    IrtImgPixelStruct
        *NewTexture = (IrtImgPixelStruct *)
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

    if (TexData.Texture != NULL) {
        IritFree(TexData.Texture);
    }
    
    TexData.Width = Width;
    TexData.Height = Height;
    TexData.Texture = NewTexture;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Update the derivative data of the texture, based on the partial          *
*   derivatives of the object's surface                                      *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Object:  The object to update                                            *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrPoSDeriveTexture(IrtMdlrFuncInfoClass *FI,
				    IPObjectStruct *Object)
{
    CagdRType UMin, UMax, VMin, VMax, i, j;
    CagdVType SuVec, SvVec,
        SUMSuVec = {0, 0, 0},
        SUMSvVec = {0, 0, 0};
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
						      (FI -> LocalFuncData());
    IrtMdlrPoSTexDataStruct
        &TexData = LclData -> TexDatas[Object];

    if (IP_IS_TRIMSRF_OBJ(Object)) {
        TexData.Deriv.u.Srf = CagdSrfDerive(Object -> U.TrimSrfs -> Srf,
					    CAGD_CONST_U_DIR);
        TexData.Deriv.v.Srf = CagdSrfDerive(Object -> U.TrimSrfs -> Srf,
					    CAGD_CONST_V_DIR);
        CagdSrfDomain(Object -> U.TrimSrfs -> Srf, &UMin, &UMax, &VMin, &VMax);
    }
    else {
        TexData.Deriv.u.Srf = CagdSrfDerive(Object -> U.Srfs, CAGD_CONST_U_DIR);
        TexData.Deriv.v.Srf = CagdSrfDerive(Object -> U.Srfs, CAGD_CONST_V_DIR);
        CagdSrfDomain(Object -> U.Srfs, &UMin, &UMax, &VMin, &VMax);
    }

    TexData.Deriv.u.Avg = 0;
    TexData.Deriv.v.Avg = 0;

    for (i = UMin; i <= UMax; i += (UMax - UMin) / 10) {
        for (j = VMin; j <= VMax; j += (VMax - VMin) / 10) {
            CAGD_SRF_EVAL_E3(TexData.Deriv.u.Srf, i, j, SuVec);
            CAGD_SRF_EVAL_E3(TexData.Deriv.v.Srf, i, j, SvVec);

            TexData.Deriv.u.Avg += IRIT_VEC_LENGTH(SuVec);
            TexData.Deriv.v.Avg += IRIT_VEC_LENGTH(SvVec);
        }
    }

    TexData.Deriv.u.Avg /= 100;
    TexData.Deriv.v.Avg /= 100;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Set the color attributes of the object to white                          *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Object:  The object to update                                            *
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
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
        sprintf(LclData -> Names.GetStr(), "%s:%d",
		LclData -> Names.GetStr(), Index);
        GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_SHAPE,
				       &LclData -> Names);
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
    v = B1;
    for (u = A1; u <= A2; u++) {
        int x = (High) ? v : u,
	    y = (High) ? u : v;
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
				  IPObjectStruct *Object,
				  int XOff,
				  int YOff)
{
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
						      (FI -> LocalFuncData());
    bool UClosed, VClosed;
    int u, v,
        XMin = XOff - LclData -> Updated.Width / 2,
        YMin = (YOff - LclData -> Updated.Height / 2);
    IrtMdlrPoSTexDataStruct
        &TexData = LclData -> TexDatas[Object];

    /* Modulo needs positive values to work as intended */
    while (XMin < 0) {
        XMin += TexData.Width;
    }
    while (YMin < 0) {
        YMin += TexData.Height;
    }
    XMin %= TexData.Width;
    YMin %= TexData.Height;

    if (IP_IS_TRIMSRF_OBJ(Object)) {
        UClosed = Object -> U.TrimSrfs -> Srf -> UPeriodic ||
               CagdIsClosedSrf(Object -> U.TrimSrfs -> Srf, CAGD_CONST_U_DIR);
        VClosed = Object -> U.TrimSrfs -> Srf -> VPeriodic ||
               CagdIsClosedSrf(Object -> U.TrimSrfs -> Srf, CAGD_CONST_V_DIR);
    }
    else {
        UClosed = Object -> U.Srfs -> UPeriodic ||
                  CagdIsClosedSrf(Object -> U.Srfs, CAGD_CONST_U_DIR);
        VClosed = Object -> U.Srfs -> VPeriodic ||
                  CagdIsClosedSrf(Object -> U.Srfs, CAGD_CONST_V_DIR);
    }

    for (v = 0; v < LclData -> Updated.Height; v++) {
        for (u = 0; u < LclData -> Updated.Width; u++) {
            int x = (XMin + u) % TexData.Width,
                y = (YMin + v) % TexData.Height,
                TextureOff = x + TexData.Width * y,
                ShapeOff = u + LclData -> Updated.Width * v;

            if (!UClosed) {
                if (x != (XMin + u)) {
                    continue;
                }
            }
            if (!VClosed) {
                if (y != (YMin + v)) {
                    continue;
                }
            }

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
            TexData.Texture[TextureOff].r =
                (IrtBType) (LclData -> TextureBuffer[TextureOff].r
                + (LclData -> TextureAlpha[TextureOff].r
                - LclData -> TextureBuffer[TextureOff].r)
                * (LclData -> Base.Alpha / 255.0));
            TexData.Texture[TextureOff].g =
                (IrtBType) (LclData -> TextureBuffer[TextureOff].g
                + (LclData -> TextureAlpha[TextureOff].g
                - LclData -> TextureBuffer[TextureOff].g)
                * (LclData -> Base.Alpha / 255.0));
            TexData.Texture[TextureOff].b =
                (IrtBType) (LclData -> TextureBuffer[TextureOff].b
                + (LclData -> TextureAlpha[TextureOff].b
                - LclData -> TextureBuffer[TextureOff].b)
                * (LclData -> Base.Alpha / 255.0));
        }
    }
    TexData.Saved = false;
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
    IRT_DSP_STATIC_DATA IPObjectStruct *
        PrevObj = NULL;
    IrtRType
        Span[2] = {1.0, 1.0};
    IrtMdlrFuncInfoClass
        *FI = (IrtMdlrFuncInfoClass *) MouseEvent -> Data;
    IPObjectStruct
        *Obj = (IPObjectStruct *) MouseEvent -> PObj;
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *>
						      (FI -> LocalFuncData());

    if (MouseEvent -> KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN) {
        switch (MouseEvent -> Type) {
            case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
	        GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
		Clicking = TRUE;
        printf("Wut");
		break;

            case IRT_DSP_MOUSE_EVENT_LEFT_UP:
	        GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
		Clicking = FALSE;
		PrevXOff = -1;
		PrefYOff = -1;
		break;

            default:
	        break;
        }

        if (Clicking &&
            MouseEvent -> UVW != NULL &&
            Obj != NULL &&
            find(LclData -> Selection.begin(), LclData -> Selection.end(),
		 Obj) != LclData -> Selection.end()) {
            IrtMdlrPoSTexDataStruct
                &TexData = LclData -> TexDatas[Obj];

            /* Reset texture buffers if needed. */
            if (Obj != PrevObj) {
                int x = 0;

                if (PrevObj != NULL) {
                    if (IP_IS_TRIMSRF_OBJ(PrevObj)) {
                        const CagdUVType
			     CrntUV = { (CagdRType) PrevXOff / TexData.Width,
					(CagdRType) PrefYOff / TexData.Height },
                             NextUV = { MouseEvent -> UVW[0],
					MouseEvent -> UVW[1] };
                        CagdUVType
			    CrntBndryUV = { PrevXOff, PrefYOff }, 
                            NextBndryUV = { MouseEvent -> UVW[0],
					    MouseEvent -> UVW[1] };
                        CagdRType ScaleChange[2] = { 1, 1 };

                        IrtMdlrComputeJunctionBetweenSrfs(PrevObj -> U.TrimSrfs,
						   CrntUV, Obj -> U.TrimSrfs,
						   NextUV, CrntBndryUV,
						   NextBndryUV, ScaleChange);

                        IrtMdlrPoSShapeUpdate(FI, PrevObj, CrntBndryUV[0],
					      CrntBndryUV[1]);
                        IrtMdlrPoSRenderShape(FI, PrevObj,
			    (int) ((double) CrntBndryUV[0] * TexData.Width),
			    (int) ((double) CrntBndryUV[1] * TexData.Height));

                        PrevXOff = (int) ((double) TexData.Width *
					                      NextBndryUV[0]);
                        PrefYOff = (int) ((double) TexData.Height *
					                      NextBndryUV[1]);

                        x = 1;
                    }
                }
                
                if (x != 1) {
                    PrevXOff = -1;
                    PrefYOff = -1;
                }

                if (LclData -> TextureAlpha != NULL) {
                    IritFree(LclData -> TextureAlpha);
                }
                if (LclData -> TextureBuffer != NULL) {
                    IritFree(LclData -> TextureBuffer);
                }

                LclData -> TextureAlpha = (IrtImgPixelStruct *)
                    IritMalloc(sizeof(IrtImgPixelStruct) * TexData.Width *
			                                   TexData.Height);
                LclData -> TextureBuffer = (IrtImgPixelStruct *)
                    IritMalloc(sizeof(IrtImgPixelStruct) * TexData.Width *
			                                   TexData.Height);

                for (x = 0; x < TexData.Width * TexData.Height; x++) {
                    LclData -> TextureAlpha[x] = TexData.Texture[x];
                    LclData -> TextureBuffer[x] = TexData.Texture[x];
                }

             
            }
            PrevObj = Obj;

            int XOff = (int) ((double) TexData.Width * MouseEvent -> UVW[0]),
                YOff = (int) ((double) TexData.Height * MouseEvent -> UVW[1]);

            if (PrefYOff == -1) {
                IrtMdlrPoSShapeUpdate(FI, Obj, MouseEvent -> UVW[0],
				      MouseEvent -> UVW[1]);
                IrtMdlrPoSRenderShape(FI, Obj, XOff, YOff);
                PrevXOff = XOff;
                PrefYOff = YOff;
            }
            else {
	        size_t i;
                int XBackup = XOff,
                    YBackup = YOff;
                pair<int, int> Start, End;
                vector<pair<int, int>> Points;

                if (IRT_MDLR_POS_DIST(PrevXOff, XOff - TexData.Width)
                    < IRT_MDLR_POS_DIST(PrevXOff, XOff))
                    XOff -= TexData.Width;
                if (IRT_MDLR_POS_DIST(PrevXOff, XOff + TexData.Width)
                    < IRT_MDLR_POS_DIST(PrevXOff, XOff))
                    XOff += TexData.Width;
                if (IRT_MDLR_POS_DIST(PrefYOff, YOff - TexData.Height)
                    < IRT_MDLR_POS_DIST(PrefYOff, YOff))
                    YOff -= TexData.Height;
                if (IRT_MDLR_POS_DIST(PrefYOff, YOff + TexData.Height)
                    < IRT_MDLR_POS_DIST(PrefYOff, YOff))
                    YOff += TexData.Height;
                Start = pair<int, int>(PrevXOff, PrefYOff);
                End = pair<int, int>(XOff, YOff);
                IrtMdlrPoSBresenham(Start, End, Points);

                for (i = 0; i < Points.size(); i++) {
                    IrtMdlrPoSShapeUpdate(FI, Obj, 
                           (float) Points[i].first / (float) TexData.Width,
                           (float) Points[i].second / (float) TexData.Height);
                    IrtMdlrPoSRenderShape(FI, Obj,
					  Points[i].first % TexData.Width,
					  Points[i].second % TexData.Height);
                }

                PrevXOff = XBackup;
                PrefYOff = YBackup;
            }

            GuIritMdlrDllSetTextureFromImage(FI, Obj, TexData.Texture,
					     TexData.Width, TexData.Height,
					     TexData.Alpha, Span);

            GuIritMdlrDllRequestIntermediateUpdate(FI, false);
        }
    }

    return TRUE;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Updates the shape, according to the UV coordinates on the surface's      *
*   UV mapping. This includes resizing and shearing                          *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   Object:  Object containing the shape to update                           *
*   u:       U component of the coordinates in the UV mapping                *
*   v:       V component of the coordinates in the UV mapping                *
*                                                                            *
* RETURN VALUE:                                                              *
*   None                                                                     *
*****************************************************************************/
static void IrtMdlrPoSShapeUpdate(IrtMdlrFuncInfoClass *FI, 
				  IPObjectStruct *Object, 
				  double u, 
				  double v) 
{
    CagdRType UMin, UMax, VMin, VMax;
    CagdVType SuVec, SvVec, SuVecAvg, SvVecAvg;
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *> 
 						      (FI -> LocalFuncData());

    IrtMdlrPoSDerivDataStruct
        &Deriv = LclData -> TexDatas[Object].Deriv;

    /* Copy base shape. */
    IritFree(LclData -> Updated.Shape);

    LclData -> Updated.XFactor = LclData -> Base.XFactor;
    LclData -> Updated.YFactor = LclData -> Base.YFactor;

    LclData -> Updated.Width = LclData -> Base.Width;
    LclData -> Updated.Height = LclData -> Base.Height;

    int i, x, y,
	Size = LclData -> Updated.Height * LclData -> Updated.Width;

    LclData -> Updated.Shape = (float *) IritMalloc(sizeof(float) * Size);

    for (y = 0; y < LclData -> Updated.Height; y++) {
        for (x = 0; x < LclData -> Updated.Width; x++) {
            int Offset = y * LclData -> Updated.Width + x;
            LclData -> Updated.Shape[Offset] = LclData -> Base.Shape[Offset];
        }
    }

    /* Get the UV mapping domain of the object. */
    if (IP_IS_TRIMSRF_OBJ(Object)) {
        CagdSrfDomain(Object -> U.TrimSrfs -> Srf, &UMin, &UMax, &VMin, &VMax);
    }
    else {
        CagdSrfDomain(Object -> U.Srfs, &UMin, &UMax, &VMin, &VMax);
    }

    u = ((UMax - UMin) * u) + UMin;
    v = ((VMax - VMin) * v) + VMin;

    /* Compute the partial derivatives. */
    CAGD_SRF_EVAL_E3(Deriv.u.Srf, u, v, SuVec);
    CAGD_SRF_EVAL_E3(Deriv.v.Srf, u, v, SvVec);

    for (i = 0; i < 3; i++) {
        SuVecAvg[i] = SuVec[i] / Deriv.u.Avg;
        SvVecAvg[i] = SvVec[i] / Deriv.v.Avg;
    }

    /* Execute the transformations. */
    IrtMdlrPoSApplyResize(FI, SuVecAvg, SvVecAvg);
    IrtMdlrPoSApplyShear(FI, SuVec, SvVec);
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Updates scaling factors, using partial derivatives, and applies them     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   SuVec:   Partial derivative vector in the U direction                    *
*   SvVec:   Partial derivative vector in the V direction                    *
*                                                                            *
* RETURN VALUE:                                                              *
*   None                                                                     *
*****************************************************************************/
static void IrtMdlrPoSApplyResize(IrtMdlrFuncInfoClass *FI, 
				  CagdVType SuVec, 
				  CagdVType SvVec) 
{
    double
        normU = IRIT_VEC_LENGTH(SuVec),
        normV = IRIT_VEC_LENGTH(SvVec);
    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *> 
						      (FI -> LocalFuncData());

    LclData -> Updated.XFactor = abs(1 / normU) * LclData -> Updated.XFactor;
    LclData -> Updated.YFactor = abs(1 / normV) * LclData -> Updated.YFactor;

    int x, y, 
        oldWidth = LclData -> Updated.Width,
        oldHeight = LclData -> Updated.Height;
    float
        *oldShape = LclData -> Updated.Shape;

    LclData -> Updated.Width = 
	        (int) (LclData -> Updated.Width * LclData -> Updated.XFactor);
    LclData -> Updated.Height = 
               (int) (LclData -> Updated.Height * LclData -> Updated.YFactor);

    int Size = LclData -> Updated.Height * LclData -> Updated.Width;

    LclData -> Updated.Shape = (float *) IritMalloc(sizeof(float) * Size);

    float
        XRatio = (float) oldWidth / (float) LclData -> Updated.Width,
        YRatio = (float) oldHeight / (float) LclData -> Updated.Height;

    /* Apply the resizing */
    for (y = 0; y < LclData -> Updated.Height; y++) {
        for (x = 0; x < LclData -> Updated.Width; x++) {
	    int UpdatedOff = y * LclData -> Updated.Width + x,
                BaseOff = (int) ((int) (y * YRatio) * oldWidth + x * XRatio);

            LclData -> Updated.Shape[UpdatedOff] = oldShape[BaseOff];
        }
    }

    IritFree(oldShape);
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Shears the current used shape, using partial derivatives        	     *
*                                                                            *
* PARAMETERS:                                                                *
*   FI:      Function information - holds generic information such as Panel. *
*   SuVec:   Partial derivative vector in the U direction                    *
*   SvVec:   Partial derivative vector in the V direction                    *
*                                                                            *
* RETURN VALUE:                                                              *
*   None                                                                     *
*****************************************************************************/
static void IrtMdlrPoSApplyShear(IrtMdlrFuncInfoClass *FI, 
				 CagdVType SuVec,
				 CagdVType SvVec) 
{
    int x, y;
    double
        normU = IRIT_VEC_LENGTH(SuVec),
        normV = IRIT_VEC_LENGTH(SvVec),
        cos = IRT_MDLR_POS_DOT(SuVec, SvVec) / (normU * normV);

    IrtMdlrPaintOnSrfLclClass
        *LclData = dynamic_cast<IrtMdlrPaintOnSrfLclClass *> 
						      (FI -> LocalFuncData());

    /* If the change is negligeable or non-existant, don't do it */
    if (abs(cos) < IRIT_EPS) {
        return;
    }

    double
        Factor = (normV * abs(cos) * LclData -> TextureWidth) /
					   (normU * LclData -> TextureHeight),
        ShearWidth = Factor * LclData -> Updated.Width;
    int NewWidth = (int) (LclData -> Updated.Width + ShearWidth);

    /* Allocate and init the pointer which will contain the sheared shape */
    float
        *NewShape = (float *)
	     IritMalloc(sizeof(float) * LclData -> Updated.Height * NewWidth);

    for (x = 0; x < LclData -> Updated.Height * NewWidth; x++) {
        NewShape[x] = 0;
    }

    /* Fill the texture with the sheared shape */
    for (y = 0; y < LclData -> Updated.Height; y++) {
        for (x = 0; x < LclData -> Updated.Width; x++) {
            int NewX;

            if (cos > 0) {
                NewX = (int) (x + ShearWidth *
                     (double) (LclData -> Updated.Height - y - 1) /
                                  (double) (LclData -> Updated.Height - 1));
            }
            else {
                NewX = (int) (x + ShearWidth * (double) (y)  /
                    		  (double) (LclData -> Updated.Height - 1));
            }

            int OldOff = y * LclData -> Updated.Width + x,
	        NewOff = y * NewWidth + NewX;

            NewShape[NewOff] = LclData -> Updated.Shape[OldOff];
        }
    }

    IritFree(LclData -> Updated.Shape);
    LclData -> Updated.Shape = NewShape;
    LclData -> Updated.Width = NewWidth;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Computes the magnitude of the directional derivative of Srf in (u, v)    *
* in unit UV direction UVUnitDir.                                            *
*                                                                            *
*                                                                            *
* PARAMETERS:                                                                *
*   Srf:       Surface to compute is directional derivative magnitude.       *
*   u, v:      Location in Srf to evaluate the directional derivate at.      *
*   UVUnitDir: The Direction in parametric space of Srf, as a unit vector .  *
*                                                                            *
* RETURN VALUE:                                                              *
*   CagdRType:   The magnitude of the directional derivative.                *
*****************************************************************************/
static CagdRType IrtMdlrEvalTangentMagnitudeOnBndry(const CagdSrfStruct *Srf,
						    CagdRType u,
						    CagdRType v,
						    const CagdVType UVUnitDir)
{
    int i;
    CagdVecStruct DSrfDu, DSrfDv;
    CagdVType DirDeriv;

    CagdSrfTangentToData(Srf, u, v, CAGD_CONST_U_DIR, FALSE, &DSrfDu);
    CagdSrfTangentToData(Srf, u, v, CAGD_CONST_V_DIR, FALSE, &DSrfDv);

    assert(UVUnitDir[2] == 0.0);
    for (i = 0; i < 3; i++) {
        DirDeriv[i] = UVUnitDir[0] * DSrfDu.Vec[i] +
		      UVUnitDir[1] * DSrfDv.Vec[i];
    }

    return IRIT_VEC_LENGTH(DirDeriv);
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Computes the relative scale and the UV on the border between two         *
* surfaces, as we cross with the mouse between surfaces.                     *
*                                                                            *
* PARAMETERS:                                                                *
*   CrntSrf:    The current surfaces from which we came.                     *
*   CrntUV:     The current UV values in the current surface.                *
*   NextSrf:    The next surface into which we moved.                        *
*   NextUV:     The next UV coordinate in the next surface.                  *
*   CrntBndryUV:  Computed UV on the boundary of the current surface.        *
*   NextBndryUV:  Computed UV on the boundary in the next surface.           *
*   ScaleChange:  Scale change to apply, moving from current to next surface.*
*                                                                            *
* RETURN VALUE:                                                              *
*   void                                                                     *
*****************************************************************************/
static void IrtMdlrComputeJunctionBetweenSrfs(const TrimSrfStruct *CrntSrf,
					      const CagdUVType CrntUV,
					      const TrimSrfStruct *NextSrf,
					      const CagdUVType NextUV,
					      CagdUVType CrntBndryUV,
					      CagdUVType NextBndryUV,
					      CagdRType *ScaleChange)
{
    TrimCrvSegStruct *ClosestTSeg1, *ClosestTSeg2;
    CagdRType
	t1 = TrimFindClosestTrimCurve2UV(CrntSrf -> TrimCrvList, CrntUV,
					 &ClosestTSeg1),
	t2 = TrimFindClosestTrimCurve2UV(NextSrf -> TrimCrvList, NextUV,
					 &ClosestTSeg2);
    CagdVecStruct Tan1, Tan2;

    CAGD_CRV_EVAL_E2(ClosestTSeg1 -> UVCrv, t1, CrntBndryUV);
    CAGD_CRV_EVAL_E2(ClosestTSeg2 -> UVCrv, t2, NextBndryUV);

    /* Now compute the relative scale. */
    CagdCrvTangentToData(ClosestTSeg1 -> UVCrv, t1, TRUE, &Tan1);
    CagdCrvTangentToData(ClosestTSeg2 -> UVCrv, t2, TRUE, &Tan2);

    *ScaleChange =
        IrtMdlrEvalTangentMagnitudeOnBndry(CrntSrf -> Srf,
					   CrntBndryUV[0], CrntBndryUV[1],
					   Tan1.Vec) /
        IrtMdlrEvalTangentMagnitudeOnBndry(NextSrf -> Srf,
					   NextBndryUV[0], NextBndryUV[1],
					   Tan2.Vec);

#   ifdef DEBUG_JUCT_EVAL_CASE
    {
	IRT_DSP_STATIC_DATA int
	    i = 1;
	CagdRType Dist, Dist0;
	CagdPType Pt1, Pt2;

	if (*ScaleChange > 1000 || *ScaleChange < 0.001)
	    printf("GERSHON");

	CAGD_SRF_EVAL_E3(CrntSrf -> Srf, CrntBndryUV[0], CrntUV[1], Pt1);
	CAGD_SRF_EVAL_E3(NextSrf -> Srf, NextBndryUV[0], NextUV[1], Pt2);
	Dist0 = IRIT_PT_PT_DIST(Pt1, Pt2);

	CAGD_SRF_EVAL_E3(CrntSrf -> Srf, CrntBndryUV[0], CrntBndryUV[1], Pt1);
	CAGD_SRF_EVAL_E3(NextSrf -> Srf, NextBndryUV[0], NextBndryUV[1], Pt2);
	Dist = IRIT_PT_PT_DIST(Pt1, Pt2);

	fprintf(stderr,
	        "%d) Crossing surfaces from %.5f %.5f %.5f to %.5f %.5f %.5f [Scale = %.5f]\n\t(Dist = %.5f, Init Dist = %.5f) Srf1 = 0x%08lx, Srf2 = 0x%08lx\n",
		i++, Pt1[0], Pt1[1], Pt1[2], Pt2[0], Pt2[1], Pt2[2], *ScaleChange,
		Dist, Dist0, CrntSrf, NextSrf);
    }
#   endif /* DEBUG_JUCT_EVAL_CASE */

    *ScaleChange = 1.0;                           /* Disable scalingfor now. */
}
