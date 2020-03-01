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

using std::vector;
using std::map;
using std::pair;
using std::swap;

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
    IrtMdlrPoSShapeStruct(int _Width, int _Height, int _Index,
		          IrtRType _Alpha, IrtRType _XFactor, IrtRType _YFactor,
			  char *_Color, const char *_Names,
			  const char **_Files, float *_Shape):

        Width(_Width),
	Height(_Height),
	Index(_Index),
	Alpha(_Alpha),
	XFactor(_XFactor),
	YFactor(_YFactor),
	Files(_Files),
	Shape(_Shape)
    {
        if (_Color != NULL) {
	    Color[0] = _Color[0];
	    Color[1] = _Color[1];
	    Color[2] = _Color[2];
	}
	else {
	    Color[0] = Color[1] = Color[2] = 0;
	}
	if (_Names != NULL)
	    strcpy(Names, _Names);
	else
	    Names[0] = 0;
    }

    int Width, Height, Index;
    IrtRType Alpha, XFactor, YFactor;
    unsigned char Color[3];
    char Names[IRIT_LINE_LEN_XLONG];
    const char **Files;
    float *Shape;
};

struct IrtMdlrPoSTextureStruct {
    bool Saved, Resize;
    int Width, Height, Alpha;
    IrtImgPixelStruct *Texture;
};

class IrtMdlrPaintOnSrfLclClass : public IrtMdlrLclDataClass {
    public:
	IrtMdlrPaintOnSrfLclClass(const IrtMdlrFuncTableStruct *FuncTbl):
	    IrtMdlrLclDataClass(NULL),
            SrfExpr(),
            TextureWidth(4),
            TextureHeight(4),
            Shape(0, 0, -1, 255, 1, 1, NULL, "", NULL, NULL),
            Names(IrtMdlrSelectExprClass(Shape.Names, 0)),
            Surface(NULL)
	{
	    ParamVals[0] = (void *) &SrfExpr;
	    ParamVals[1] = NULL;
	    ParamVals[2] = NULL;
	    ParamVals[3] = NULL;
	    ParamVals[4] = (void *) &TextureWidth;
	    ParamVals[5] = (void *) &TextureHeight;
	    ParamVals[6] = NULL;
	    ParamVals[7] = (void *) &Shape.Alpha;
        ParamVals[8] = (void *) &Names;
	    ParamVals[9] = (void *) &Shape.XFactor;
	    ParamVals[10] = (void *) &Shape.YFactor;
	}
	
	IrtMdlrObjectExprClass SrfExpr;
    int TextureWidth;
	int TextureHeight;
    IrtMdlrPoSShapeStruct Shape;
    IrtMdlrSelectExprClass Names;
    IrtImgPixelStruct *TextureAlpha;
    IrtImgPixelStruct *TextureBuffer;
    map<IPObjectStruct *, IrtMdlrPoSTextureStruct *> Textures;
    IPObjectStruct *Surface;
 };


static void IrtMdlrPaintOnSrf(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrPoSResizeTexture(IrtMdlrFuncInfoClass *FI,
				    int Width,
				    int Height,
				    bool Reset);
static int IrtMdlrPoSInitShapes(IrtMdlrFuncInfoClass *FI);
static void IrtMdlrPoSLoadShape(IrtMdlrFuncInfoClass *FI,
				const char* Filename);
static void IrtMdlrPoSBresenham(const pair<int, int>& a,
				const pair<int, int>& b,
				vector<pair<int, int>>& Points);
static void IrtMdlrPoSRenderShape(IrtMdlrFuncInfoClass *FI,
				  int XOff,
				  int YOff);
static int IrtMdlrPoSMouseCallBack(IrtMdlrMouseEventStruct *MouseEvent);

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
            IRT_MDLR_SURFACE_EXPR,

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
        *LastSurfaceName = NULL;
    IRT_DSP_STATIC_DATA IPObjectStruct
        *LastSurface = NULL;
    int PrevWidth, PrevHeight;
    IrtRType XFactor = 1, 
        YFactor = 1;
    IrtRType
        Span[2] = { 1.0, 1.0 };
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

        GuIritMdlrDllSetIntInputDomain(FI, 4,  IRT_MDLR_POS_MAX_WIDTH, 
				       IRT_MDLR_POS_WIDTH);
        GuIritMdlrDllSetIntInputDomain(FI, 4,  IRT_MDLR_POS_MAX_HEIGHT, 
				       IRT_MDLR_POS_HEIGHT);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, IRT_MDLR_POS_ALPHA);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, IRT_MDLR_POS_X_FACTOR);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, IRT_MDLR_POS_Y_FACTOR);

        LclData -> Textures.clear();
        LclData -> Surface = NULL;

        if (LastSurface != NULL) {
            LastSurface = NULL;
	    if (LastSurfaceName != NULL)
	        IritFree(LastSurfaceName);
            LastSurfaceName = NULL;
        }

        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
		        "Surface Painter initialized\n");
    }

    if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_CANCEL) {
        map<IPObjectStruct *, IrtMdlrPoSTextureStruct *>::iterator it;

        for (it = LclData -> Textures.begin(); 
            it != LclData -> Textures.end();
            it++) {
            if (!it -> second -> Saved) {
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
			       "Surface %s had changes that were not saved.\n",
			       it -> first -> ObjName);
            }
            const char* tpath = AttrIDGetObjectStrAttrib(it->first, IRIT_ATTR_CREATE_ID(ptexture));
            if (tpath != NULL) {
                std::string relat(tpath);
                std::istringstream ss(relat);
                getline(ss, relat, ',');

                int Width, Height,
                    Alpha = 0;
                IrtMdlrPoSTextureStruct
                    * Texture = LclData->Textures[LclData->SrfExpr.GetIPObj()];
                IrtImgPixelStruct
                    * Image = IrtImgReadImage2(relat.c_str(), &Width, &Height, &Alpha);

                Width++, Height++;
                Texture->Alpha = Alpha;
                Texture->Saved = false;
                Texture->Resize = false;
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
                    "Texture loaded successfully from \"%s\" (%dx%d)\n",
                    relat.c_str(), Width, Height);

                if (Width % 4 != 0) {
                    Width -= Width % 4;
                    Width += 4;
                }
                if (Height % 4 != 0) {
                    Height -= Height % 4;
                    Height += 4;
                }
                IrtMdlrPoSResizeTexture(FI, Width, Height, true);

                int offsetW = 0;
                if (Width % 4 != 0) {
                    offsetW = 4 - Width % 4;
                }
                for (int h = 0; h < Height; h++) {
                    for (int w = 0; w < Width; w++) {
                        Texture->Texture[h * Width + w + (h * offsetW)] =
                            Image[h * Width + w];
                        LclData->TextureAlpha[h * Width + w + (h * offsetW)] =
                            Image[h * Width + w];
                        LclData->TextureBuffer[h * Width + w + (h * offsetW)] =
                            Image[h * Width + w];
                    }
                }
                GuIritMdlrDllSetTextureFromImage(FI,
                    it->first,
                    Texture->Texture,
                    Texture->Width,
                    Texture->Height,
                    Texture->Alpha,
                    Span);
            }
            else {
               // Here put a 4x4 empty texture.
                IrtImgPixelStruct DefaultTexture[4][4];
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        DefaultTexture[i][j].r = 255;
                        DefaultTexture[i][j].g = 255;
                        DefaultTexture[i][j].b = 255;
                    }
                }
                GuIritMdlrDllSetTextureFromImage(FI, it->first,
                    DefaultTexture,
                    4, 4, FALSE, Span);

            }
        }

        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
			    "Surface Painter unloaded\n");
        return;
    }

    if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK) {
        map<IPObjectStruct*, IrtMdlrPoSTextureStruct*>::iterator it;
        for (it = LclData->Textures.begin();
            it != LclData->Textures.end();
            it++) {
            
            char* Filename;
            bool Res = GuIritMdlrDllGetAsyncInputFileName(FI,
                    "Save Texture to....",
                    "*.png", &Filename);
            if (Res) {
                IrtMdlrPoSTextureStruct* Texture = it->second;
                MiscWriteGenInfoStructPtr
                    GI = IrtImgWriteOpenFile(NULL, Filename, IRIT_IMAGE_PNG_TYPE,
                        Texture->Alpha, Texture->Width,
                        Texture->Height);
                if (GI == NULL) {
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
                        "Error saving texture to \"%s\"\n",
                        Filename);
                }
                else {
                    int y;
                    for (y = 0; y < Texture->Height; y++) {
                        IrtImgWritePutLine(GI, NULL,
                            &Texture->Texture[y * Texture->Width]);
                    }
                    IrtImgWriteCloseFile(GI);
                    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
                        "Texture saved successfully to \"%s\"\n",
                        Filename);
                    Texture->Saved = true;
    
                    AttrIDSetObjectStrAttrib(it->first, IRIT_ATTR_CREATE_ID(ptexture), Filename);
                    //GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, "\"%s\"\n", AttrIDGetObjectStrAttrib(it->first, IRIT_ATTR_CREATE_ID(ptexture)));
                }
            }
        }

        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
            "Surface Painter unloaded\n");
        return;
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_SURFACE, 
                                   LclData -> SrfExpr.GetObjAddr());

    if (LclData -> SrfExpr.GetIPObj() == NULL) {
        if (LastSurface != NULL) {
            IritFree(LclData -> Textures[LastSurface] -> Texture);
            LclData -> Textures.erase(LastSurface);
            LastSurface = NULL;
            LclData -> Surface = NULL;
        }
	if (LastSurfaceName != NULL)
            IritFree(LastSurfaceName);
	LastSurfaceName = NULL;
    }
    else {
	    if (GuIritMdlrDllGetObjectMatrix(FI, LclData -> SrfExpr.GetIPObj()) != NULL) {
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING, 
		    "Paint on surfaces not supported for a surface with transformation - \"Apply Transform\" to the surface first.\n");
	        return;
	    }

        if (LastSurface != NULL &&
	    LastSurface != LclData -> SrfExpr.GetIPObj() &&
            strcmp(LastSurfaceName, LclData -> SrfExpr.GetIPObj() -> ObjName) 
                                                                        == 0) {
            IritFree(LclData -> Textures[LastSurface] -> Texture);
            LclData -> Textures.erase(LastSurface);
        }
        LastSurface = LclData -> SrfExpr.GetIPObj();
        if (LastSurfaceName != NULL)
            IritFree(LastSurfaceName);
        LastSurfaceName = IritStrdup(LclData -> SrfExpr.GetIPObj() -> ObjName);        
    }
    
    if (LclData -> SrfExpr.GetIPObj() != NULL && !PanelUpdate) {
        PanelUpdate = true;
        if (LclData -> Textures.find(LclData -> SrfExpr.GetIPObj()) == 
            LclData -> Textures.end()) {
	    int i, j;
            IrtImgPixelStruct DefaultTexture[4][4];
            IrtMdlrPoSTextureStruct
	        *Texture = (IrtMdlrPoSTextureStruct *)
		                  IritMalloc(sizeof(IrtMdlrPoSTextureStruct));
            IrtDspOGLObjPropsClass *OGLProps;
            
            for (i = 0; i < 4; i++) {
                for (j = 0; j < 4; j++) {
                    DefaultTexture[i][j].r = 255;
                    DefaultTexture[i][j].g = 255;
                    DefaultTexture[i][j].b = 255;
                }
            }

            Texture -> Width = IRT_MDLR_POS_DFLT_WIDTH;
            Texture -> Height = IRT_MDLR_POS_DFLT_HEIGHT;
            Texture -> Alpha = 0;
	        Texture -> Saved = false;
	        Texture -> Resize = false;
            Texture -> Texture = (IrtImgPixelStruct *) 
                IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);
            for (i = 0; i < IRT_MDLR_POS_DFLT_SIZE; i++) {
                Texture -> Texture[i].r = 255;
                Texture -> Texture[i].g = 255;
                Texture -> Texture[i].b = 255;
            }
            LclData -> Textures[LclData -> SrfExpr.GetIPObj()] = Texture;

            /* Apply dummy white texture. This ensure UV mapping is correct.*/
            GuIritMdlrDllSetTextureFromImage(FI, LclData -> SrfExpr.GetIPObj(), 
                                             DefaultTexture, 
					     4, 4, FALSE, Span);
            
            /* Set Object color to white (default is red) */
            AttrIDSetObjectRGBColor(LclData -> SrfExpr.GetIPObj(), 
                                    255, 255, 255);
            OGLProps = GuIritMdlrDllGetObjectOGLProps(FI, 
                                                LclData -> SrfExpr.GetIPObj());

            if (OGLProps != NULL) {
                IrtDspRGBAClrClass
                    White = IrtDspRGBAClrClass(255, 255, 255);

                OGLProps -> FrontFaceColor.SetValue(White);
                OGLProps -> BackFaceColor.SetValue(White);
                OGLProps -> BDspListNeedsUpdate = true;
                GuIritMdlrDllRedrawScreen(FI);
            }
        }
        if (LclData -> SrfExpr.GetIPObj() != LclData -> Surface) {
	    int i;
            IrtMdlrPoSTextureStruct
	        *Texture = LclData -> Textures[LclData -> SrfExpr.GetIPObj()];

            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH, 
					   &Texture -> Width);
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT, 
					   &Texture -> Height);
            LclData -> Surface = LclData -> SrfExpr.GetIPObj();
            LclData -> TextureAlpha = (IrtImgPixelStruct *)
                IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);
            LclData -> TextureBuffer = (IrtImgPixelStruct *)
                IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);
            for (i = 0; i < IRT_MDLR_POS_DFLT_SIZE; i++) {
                LclData -> TextureAlpha[i] = Texture -> Texture[i];
                LclData -> TextureBuffer[i] = Texture -> Texture[i];
            }
        }
        PanelUpdate = false;
    }

    /* Texture fields. */
    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_LOAD &&
	!PanelUpdate &&
	LclData -> Surface != NULL) {
        char *Filename;
        bool
	    Res = GuIritMdlrDllGetAsyncInputFileName(FI,
						     "Load Texture from....", 
						     "*.png", &Filename);

        if (Res) {
	    int Width, Height,
                Alpha = 0;
            IrtMdlrPoSTextureStruct
	        *Texture = LclData -> Textures[LclData -> SrfExpr.GetIPObj()];
            IrtImgPixelStruct
	        *Image = IrtImgReadImage2(Filename, &Width, &Height, &Alpha);

            Width++, Height++;
            Texture -> Alpha = Alpha;
			Texture -> Saved = false;
			Texture -> Resize = false;
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, 
			 "Texture loaded successfully from \"%s\" (%dx%d)\n",
				Filename, Width, Height);

            if (Width % 4 != 0) {
                Width -= Width % 4;
                Width += 4;
            }
            if (Height % 4 != 0) {
                Height -= Height % 4;
                Height += 4;
            }

            IrtMdlrPoSResizeTexture(FI, Width, Height, true);

			int offsetW = 0;
			if (Width%4 != 0) {
				offsetW = 4 - Width % 4;
			}
            for (int h = 0; h <  Height; h++) {
	        for (int w = 0; w < Width; w++) {
		    Texture -> Texture[h * Width + w + (h * offsetW)] =
		                                          Image[h * Width + w];
		    LclData -> TextureAlpha[h * Width + w + (h * offsetW)] =
							  Image[h * Width + w];
		    LclData -> TextureBuffer[h * Width + w + (h * offsetW)] =
							  Image[h * Width + w];
		}
            }

            GuIritMdlrDllSetTextureFromImage(FI, 
					     LclData -> Surface, 
					     Texture -> Texture, 
					     Texture -> Width, 
					     Texture -> Height, 
					     Texture -> Alpha,
                         Span);
            PanelUpdate = true;
            GuIritMdlrDllSetInputParameter(FI, 
					   IRT_MDLR_POS_WIDTH, 
					   &Texture -> Width);
            GuIritMdlrDllSetInputParameter(FI, 
					   IRT_MDLR_POS_HEIGHT, 
					   &Texture -> Height);
            PanelUpdate = false;
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_SAVE &&
        LclData -> Surface != NULL) {
        char *Filename;
        bool
	    Res = GuIritMdlrDllGetAsyncInputFileName(FI, 
						     "Save Texture to....", 
						     "*.png", &Filename);
        if (Res) {
            IrtMdlrPoSTextureStruct
	        *Texture = LclData -> Textures[LclData -> SrfExpr.GetIPObj()];
            MiscWriteGenInfoStructPtr
	        GI = IrtImgWriteOpenFile(NULL, Filename, IRIT_IMAGE_PNG_TYPE, 
					 Texture -> Alpha, Texture -> Width, 
					 Texture -> Height);

            if (GI == NULL) {
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR, 
				    "Error saving texture to \"%s\"\n", 
				    Filename);
            }
            else {
	            int y;

                for (y = 0; y < Texture -> Height; y++) {
                    IrtImgWritePutLine(GI, NULL, 
				  &Texture -> Texture[y * Texture -> Width]);
                }
                IrtImgWriteCloseFile(GI);
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, 
				    "Texture saved successfully to \"%s\"\n", 
				    Filename);
		        Texture -> Saved = true;
            }
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_RESET &&
	!PanelUpdate &&
        LclData -> Surface != NULL) {
        bool
	    Res = GuIritMdlrDllGetAsyncInputConfirm(FI, 
                            "Texture Reset", 
                            "Are you sure you want to reset the texture ?");

        if (Res) {
            int Width = IRT_MDLR_POS_DFLT_WIDTH, 
                Height = IRT_MDLR_POS_DFLT_HEIGHT;
            IrtMdlrPoSTextureStruct
	        *Texture = LclData -> Textures[LclData -> SrfExpr.GetIPObj()];

            IrtMdlrPoSResizeTexture(FI, IRT_MDLR_POS_DFLT_WIDTH, 
				    IRT_MDLR_POS_DFLT_HEIGHT, true);
			Texture -> Saved = false;
            PanelUpdate = true;
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH, &Width);
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT, &Height);
            PanelUpdate = false;
            GuIritMdlrDllSetTextureFromImage(FI, LclData -> Surface, 
					     Texture -> Texture,
					     Texture -> Width, 
					     Texture -> Height, 
					     Texture -> Alpha,
                         Span);
            
        }
    }

    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_COLOR) {
        unsigned char Red, Green, Blue;

        /* Set drops color. */
        if (GuIritMdlrDllGetAsyncInputColor(FI, &Red, &Green, &Blue)) {
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
				"Color selection: %u, %u, %u.\n",
				Red, Green, Blue);
            LclData -> Shape.Color[0] = Red;
            LclData -> Shape.Color[1] = Green;
            LclData -> Shape.Color[2] = Blue;
        }
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_WIDTH, 
                                   &LclData -> TextureWidth);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_HEIGHT, 
                                   &LclData -> TextureHeight);
	PrevWidth = LclData -> TextureWidth;
	PrevHeight = LclData -> TextureHeight;
	if (LclData -> TextureHeight%4 != 0) {
	    LclData -> TextureHeight -= LclData -> TextureHeight%4;
	    LclData -> TextureHeight += 4;
		
	}
	if (LclData -> TextureWidth%4 != 0) {
	    LclData -> TextureWidth -= LclData -> TextureWidth%4;
	    LclData -> TextureWidth += 4;
	}

    if (LclData -> Surface != NULL) {
        IrtMdlrPoSTextureStruct
	    *Texture = LclData -> Textures[LclData -> SrfExpr.GetIPObj()];

        if ((Texture -> Width != LclData -> TextureWidth ||
	     Texture -> Height != LclData -> TextureHeight) &&
            !PanelUpdate) {
            bool
	        Res = true;

            PanelUpdate = true;
            if (!Texture -> Resize) {
                Res = GuIritMdlrDllGetAsyncInputConfirm(FI, "", 
                    "This will reset the texture.\n" \
                    "Are you sure you want to resize the texture ?");
                if (Res) {
                    Texture -> Resize = true;
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
                IrtMdlrPoSResizeTexture(FI, LclData -> TextureWidth, 
					LclData -> TextureHeight, true);
                Texture -> Saved = false;
                GuIritMdlrDllSetTextureFromImage(FI, LclData -> Surface, 
						 Texture -> Texture, 
						 Texture -> Width, 
						 Texture -> Height, 
						 Texture -> Alpha,
                         Span);
            }
            else {
                GuIritMdlrDllSetInputParameter(FI,
					       IRT_MDLR_POS_WIDTH,
					       &Texture -> Width);
                GuIritMdlrDllSetInputParameter(FI, 
					       IRT_MDLR_POS_HEIGHT, 
					       &Texture -> Height);
            }
            PanelUpdate = false;
        }
    }

    /* Brush fields. */
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_ALPHA, 
				   &LclData -> Shape.Alpha);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_SHAPE, &LclData -> Names);
  
    if (LclData -> Names.GetIndex() != LclData -> Shape.Index && 
        LclData -> Shape.Files != NULL) {
        LclData -> Shape.Index = LclData -> Names.GetIndex();
        IrtMdlrPoSLoadShape(FI, 
                        LclData -> Shape.Files[LclData -> Names.GetIndex()]);
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_X_FACTOR, &XFactor);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_Y_FACTOR, &YFactor);

    if (LclData -> Shape.Files != NULL && 
        (IRT_MDLR_POS_DIST(XFactor, LclData -> Shape.XFactor) > 
	                                              IRT_MDLR_POS_EPSILON ||
	 IRT_MDLR_POS_DIST(YFactor, LclData -> Shape.YFactor) > 
	                                              IRT_MDLR_POS_EPSILON)) {
        LclData -> Shape.XFactor = XFactor;
        LclData -> Shape.YFactor = YFactor;
        IrtMdlrPoSLoadShape(FI, LclData -> Shape.Files[LclData -> Names.GetIndex()]);
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
                                    int Width,
                                    int Height,
                                    bool Reset)
{
    IrtImgPixelStruct
        *Texture = (IrtImgPixelStruct *) 
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
    IritFree(LclData->Textures[LclData->Surface]-> Texture);
    IritFree(LclData -> TextureAlpha);
    IritFree(LclData -> TextureBuffer);
    LclData->Textures[LclData->Surface]-> Width = Width;
    LclData->Textures[LclData->Surface]-> Height = Height;
    LclData->Textures[LclData->Surface]-> Texture = Texture;
    LclData -> TextureAlpha = TextureAlpha;
    LclData -> TextureBuffer = TextureBuffer;

    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, 
			"Texture resized to %dx%d\n", Width, Height);
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

    if (LclData -> Shape.Files == NULL) {
        LclData -> Shape.Files = 
            GuIritMdlrDllGetAllFilesNamesInDirectory(FI, FilePath,
				            "*.rle|*.ppm|*.gif|*.jpeg|*.png");
    }

    if (LclData -> Shape.Files == NULL) {
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
		    "Masks files were not found. Check directory: \"%s\"\n",
		    FilePath);
	return FALSE;
    }

    strcpy(LclData -> Shape.Names, "");
    for (i = 0; LclData -> Shape.Files[i] != NULL; ++i) {
        p = strstr(LclData -> Shape.Files[i], 
		   GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH);
        if (p != NULL) {
            p += strlen(GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH) + 1;

            char Name[IRIT_LINE_LEN_LONG];

            strcpy(Name, "");
            q = strchr(p, '.');
            strncpy(Name, p, q - p);
            Name[q - p] = '\0';
            if (strstr(Name, "GaussianFull25") != NULL) {
                Index = i;
            }
            sprintf(LclData -> Shape.Names, "%s%s;", 
		    LclData -> Shape.Names, Name);
        }
    }

    if (LclData -> Shape.Files[0] != NULL) {
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
    IritFree(LclData -> Shape.Shape);

    LclData -> Shape.Width = (int) (Width * LclData -> Shape.XFactor);
    LclData -> Shape.Height = (int) (Height * LclData -> Shape.YFactor);
    Size = LclData -> Shape.Height * LclData -> Shape.Width;
    LclData -> Shape.Shape = (float *) IritMalloc(sizeof(float) * Size);
    XRatio = (float) Width / (float) LclData -> Shape.Width;
    YRatio = (float) Height / (float) LclData -> Shape.Height;

    for (y = 0; y < LclData -> Shape.Height; y++) {
        for (x = 0; x < LclData -> Shape.Width; x++) {
            int Off = y * LclData -> Shape.Width + x;
            float gray;
            IrtImgPixelStruct
	        *ptr = Image + (int) ((int) (y * YRatio) * Width + x * XRatio);

            IRT_DSP_RGB2GREY(ptr, gray);
            LclData -> Shape.Shape[Off] = (float)(gray / 255.0);
        }
    }
    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Loaded shape of size %dx%d\n",
			LclData -> Shape.Width, 
			LclData -> Shape.Height);
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
static void IrtMdlrPoSBresenham(const pair<int, int>& a,
                        const pair<int, int>& b,
                        vector<pair<int, int>>& Points)
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
        XMin = XOff - LclData -> Shape.Width / 2,
        YMin = (YOff - LclData -> Shape.Height / 2);
    IrtMdlrPoSTextureStruct
        *Texture = LclData -> Textures[LclData -> Surface];

    /* Modulo needs positive values to work as intended */
    while (XMin < 0) {
        XMin += Texture -> Width;
    }
    while (YMin < 0) {
        YMin += Texture -> Height;
    }
    XMin %= Texture -> Width;
    YMin %= Texture -> Height;

    for (v = 0; v < LclData -> Shape.Height; v++) {
        for (u = 0; u < LclData -> Shape.Width; u++) {
            int x = (XMin + u) % Texture -> Width,
                y = (YMin + v) % Texture -> Height,
                TextureOff = x + Texture -> Width * y,
                ShapeOff = u + LclData -> Shape.Width * v;

            LclData -> TextureAlpha[TextureOff].r += 
                (IrtBType)((LclData -> Shape.Color[0]
                - LclData -> TextureAlpha[TextureOff].r) 
                * LclData -> Shape.Shape[ShapeOff]);
            LclData -> TextureAlpha[TextureOff].g += 
                (IrtBType)((LclData -> Shape.Color[1]
                - LclData -> TextureAlpha[TextureOff].g) 
                * LclData -> Shape.Shape[ShapeOff]);
            LclData -> TextureAlpha[TextureOff].b +=
                (IrtBType)((LclData -> Shape.Color[2]
                - LclData -> TextureAlpha[TextureOff].b) 
                * LclData -> Shape.Shape[ShapeOff]);
            Texture -> Texture[TextureOff].r = 
                (IrtBType)(LclData -> TextureBuffer[TextureOff].r 
                + (LclData -> TextureAlpha[TextureOff].r 
                - LclData -> TextureBuffer[TextureOff].r) 
                * (LclData -> Shape.Alpha / 255.0));
            Texture -> Texture[TextureOff].g = 
                (IrtBType)(LclData -> TextureBuffer[TextureOff].g 
                + (LclData -> TextureAlpha[TextureOff].g 
                - LclData -> TextureBuffer[TextureOff].g)
                * (LclData -> Shape.Alpha / 255.0));
            Texture -> Texture[TextureOff].b = 
                (IrtBType)(LclData -> TextureBuffer[TextureOff].b 
                + (LclData -> TextureAlpha[TextureOff].b 
                - LclData -> TextureBuffer[TextureOff].b) 
                * (LclData -> Shape.Alpha / 255.0));
        }
    }
    Texture -> Saved = false;
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
        Span[2] = { 1.0, 1.0 };
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
		break;

            case IRT_DSP_MOUSE_EVENT_LEFT_UP:
	        GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
		Clicking = FALSE;
		if (LclData -> Surface != NULL) {
		    int x, y;
		    IrtMdlrPoSTextureStruct
		        *Texture = LclData -> Textures[LclData -> Surface];

		    for (y = 0; y < Texture -> Height; y++) {
		        for (x = 0; x < Texture -> Width; x++) {
			    int offset = y * Texture -> Width + x;

			    LclData -> TextureAlpha[offset] = 
			                          Texture -> Texture[offset];
			    LclData -> TextureBuffer[offset] = 
			                          Texture -> Texture[offset];
			}
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
            IP_IS_SRF_OBJ(PObj) &&
	    PObj == LclData -> Surface) {
            IrtMdlrPoSTextureStruct 
                *Texture = LclData -> Textures[LclData -> Surface];
            int XOff = (int) ((double) Texture -> Width *
			                               MouseEvent -> UVW[0]),
                YOff = (int) ((double) Texture -> Height *
			                               MouseEvent -> UVW[1]);

            if (PrefYOff == -1) {
                IrtMdlrPoSRenderShape(FI, XOff, YOff);
                PrevXOff = XOff;
                PrefYOff = YOff;
            }
            else {
                int XBackup = XOff,
                    YBackup = YOff;
                pair<int, int> Start, End;
                vector<pair<int, int>> Points;

                if (IRT_MDLR_POS_DIST(PrevXOff, XOff - Texture -> Width) 
                    < IRT_MDLR_POS_DIST(PrevXOff, XOff))
                    XOff -= Texture -> Width;
                if (IRT_MDLR_POS_DIST(PrevXOff, XOff + Texture -> Width) 
                    < IRT_MDLR_POS_DIST(PrevXOff, XOff))
                    XOff += Texture -> Width;
                if (IRT_MDLR_POS_DIST(PrefYOff, YOff - Texture -> Height) 
                    < IRT_MDLR_POS_DIST(PrefYOff, YOff))
                    YOff -= Texture -> Height;
                if (IRT_MDLR_POS_DIST(PrefYOff, YOff + Texture -> Height) 
                    < IRT_MDLR_POS_DIST(PrefYOff, YOff))
                    YOff += Texture -> Height;
                Start = pair<int, int>(PrevXOff, PrefYOff);
                End = pair<int, int>(XOff, YOff);
                IrtMdlrPoSBresenham(Start, End, Points);

                for (size_t i = 0; i < Points.size(); i++) {
                    IrtMdlrPoSRenderShape(FI,
                        Points[i].first % Texture -> Width,
                        Points[i].second % Texture -> Height);
                }

                PrevXOff = XBackup;
                PrefYOff = YBackup;
            }

            GuIritMdlrDllSetTextureFromImage(FI, 
                LclData -> Surface, 
                Texture -> Texture, 
                Texture -> Width, 
                Texture -> Height, 
                Texture -> Alpha,
                Span);
                       
            GuIritMdlrDllRequestIntermediateUpdate(FI, false);
        }
    }
    return TRUE;
}
