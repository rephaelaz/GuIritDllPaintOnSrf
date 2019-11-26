/******************************************************************************
* GuiIritDllPaintOnSrf.cpp - painting textures over surfaces.		      *
*******************************************************************************
* (C) Gershon Elber, Technion, Israel Institute of Technology                 *
*******************************************************************************
* Written by Ilan Coronel and Raphael Azoulay, April 2019.		     *
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

using std::vector;
using std::map;
using std::pair;
using std::swap;

#define IRT_MDLR_POS_DFLT_WIDTH 256
#define IRT_MDLR_POS_DFLT_HEIGHT 256
#define IRT_MDLR_POS_EPSILON 10e-5
#define IRT_MDLR_POS_DFLT_SIZE (IRT_MDLR_POS_DFLT_WIDTH) * \
                                  (IRT_MDLR_POS_DFLT_HEIGHT)
#define IRT_MDLR_POS_DIST(a, b) (fabs((double)(a) - (double)(b)))


#ifdef __WINNT__
#define GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH "\\SandArt\\Masks"
#else
#define GUIRIT_DLL_PAINT__ON_SRF_FILE_RELATIVE_PATH "/SandArt/Masks"
#endif /* __WINNT__ */

struct IrtMdlrPoSShapeStruct {
    int Width, Height;
    IrtRType Alpha, XFactor, YFactor;
    unsigned char Color[3];
    float *Shape;
};

struct IrtMdlrPoSTextureStruct {
    bool Saved;
    int Width, Height, Alpha;
    IrtImgPixelStruct *Texture;
};

static void IrtMdlrPaintOnSrf(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrPoSResizeTexture(IrtMdlrFuncInfoClass* FI,
                        int Width,
                        int Height,
                        bool Reset);
static int IrtMdlrPoSInitShapes(IrtMdlrFuncInfoClass* FI);
static void IrtMdlrPoSLoadShape(IrtMdlrFuncInfoClass* FI,
                        const char* Filename);
static void IrtMdlrPoSBresenham(const pair<int, int>& a,
                        const pair<int, int>& b,
                        vector<pair<int, int>>& Points);
static void IrtMdlrPoSRenderShape(IrtMdlrFuncInfoClass* FI,
                        int XOff,
                        int YOff);
static int IrtMdlrPoSMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);

IRT_DSP_STATIC_DATA IrtImgPixelStruct 
    *IrtMdlrPoSTextureAlpha,
    *IrtMdlrPoSTextureBuffer;

IRT_DSP_STATIC_DATA map<IPObjectStruct *, IrtMdlrPoSTextureStruct *> 
    IrtMdlrPoSTextures;

IRT_DSP_STATIC_DATA char 
    IrtMdlrPoSShapesNames[IRIT_LINE_LEN_XLONG];

IRT_DSP_STATIC_DATA const char 
    **IrtMdlrPoSShapesFiles = NULL;

IRT_DSP_STATIC_DATA IrtMdlrPoSShapeStruct 
    IrtMdlrPoSShape = {0, 0, 255, 1, 1, {0, 0, 0}, NULL};

IRT_DSP_STATIC_DATA IPObjectStruct
    *IrtMdlrPoSSurface = NULL;

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
        | IRT_MDLR_PARAM_INTERMEDIATE_UPDATE_DFLT_ON,
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
            IRT_MDLR_BUTTON_EXPR,			    /* RGB Values.*/
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
            &IrtMdlrPoSShape.Alpha,
            IrtMdlrPoSShapesNames,
            &IrtMdlrPoSShape.XFactor,
            &IrtMdlrPoSShape.YFactor,
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
            "Transparency factor of the painting brush.",
            "Shape of the painting brush.",
            "X factor of the painting brush.",
            "Y factor of the painting brush."
        }
    }
};

IRT_DSP_STATIC_DATA const int SRF_PAINT_FUNC_TABLE_SIZE =
    sizeof(SrfPainterFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

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
static void IrtMdlrPaintOnSrf(IrtMdlrFuncInfoClass* FI)
{
    IRT_DSP_STATIC_DATA bool 
        PanelUpdate = false, 
        TextureInit = false;
    IRT_DSP_STATIC_DATA int 
        ShapeIndex = -1;
    IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass
        *GlobalFI = NULL;
    IRT_DSP_STATIC_DATA char
        *LastSurfaceName = NULL;
    IRT_DSP_STATIC_DATA IPObjectStruct
        *LastSurface = NULL;
    int TmpIndex, TextureWidth, TextureHeight;
    IrtRType XFactor = 1, YFactor = 1;
    IrtRType Span[2] = {1.0, 1.0};
    char
        *TmpPtr = IrtMdlrPoSShapesNames;
    IPObjectStruct
        *Surface = NULL;
    
    if (FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_INIT) {
        return;
    }

    if (FI -> InvocationNumber == 0 && GlobalFI == NULL) {
        GuIritMdlrDllPushMouseEventFunc( FI, IrtMdlrPoSMouseCallBack,
            (IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
            (IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_CTRL_DOWN), FI);

	    GlobalFI = FI;

        if (!IrtMdlrPoSInitShapes(FI)) {
	        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
		            "Failed to initialized cursors.\n");
	        return;
	    }

        GuIritMdlrDllSetIntInputDomain(FI, 1, IRIT_MAX_INT, 
            IRT_MDLR_POS_WIDTH);
        GuIritMdlrDllSetIntInputDomain(FI, 1, IRIT_MAX_INT, 
            IRT_MDLR_POS_HEIGHT);
        GuIritMdlrDllSetRealInputDomain(FI, 0, 255, IRT_MDLR_POS_ALPHA);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, IRT_MDLR_POS_X_FACTOR);
        GuIritMdlrDllSetRealInputDomain(FI, 0.01, 100, IRT_MDLR_POS_Y_FACTOR);

        IrtMdlrPoSTextures.clear();
        IrtMdlrPoSSurface = NULL;

        if (LastSurface != NULL) {
            IritFree(LastSurfaceName);
            LastSurface = NULL;
            LastSurfaceName = NULL;
        }

        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
		        "Surface Painter initialized\n");
    }

    if (GlobalFI != NULL &&
	(FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK ||
	 FI -> CnstrctState == IRT_MDLR_CNSTRCT_STATE_CANCEL)) {
        bool Saved = true;
        map<IPObjectStruct *, IrtMdlrPoSTextureStruct *>::iterator it;

        GuIritMdlrDllPopMouseEventFunc(FI);
        GlobalFI = NULL;
        for (it = IrtMdlrPoSTextures.begin(); 
            it != IrtMdlrPoSTextures.end();
            it++) {
            if (!it -> second -> Saved) {
                GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_WARNING,
                    "Surface %s had changes that were not saved.\n",
                    it -> first -> ObjName);
            }
        }

        GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO,
			    "Surface Painter unloaded\n");
        return;
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_SURFACE, &Surface);

    if (Surface == NULL) {
        if (LastSurface != NULL) {
            IritFree(IrtMdlrPoSTextures[LastSurface] -> Texture);
            IrtMdlrPoSTextures.erase(LastSurface);
            LastSurface = NULL;
            IritFree(LastSurfaceName);
            IrtMdlrPoSSurface = NULL;
        }
    }
    else {
        if (LastSurface != NULL && LastSurface != Surface
            && strcmp(LastSurfaceName, Surface -> ObjName) == 0) {
            IritFree(IrtMdlrPoSTextures[LastSurface] -> Texture);
            IrtMdlrPoSTextures.erase(LastSurface);
        }
        LastSurface = Surface;
        if (LastSurfaceName != NULL) {
            IritFree(LastSurfaceName);
        }
        LastSurfaceName = (char *) IritMalloc(strlen(Surface -> ObjName) + 1);
        strcpy(LastSurfaceName, Surface -> ObjName);
        
    }
    
    if (Surface != NULL && !PanelUpdate) {
        PanelUpdate = true;
        if (IrtMdlrPoSTextures.find(Surface) == IrtMdlrPoSTextures.end()) {
	        int i, j;
            IRT_DSP_STATIC_DATA IrtImgPixelStruct 
                DefaultTexture[4][4] = {0};
            IrtMdlrPoSTextureStruct
	        *Texture = (IrtMdlrPoSTextureStruct *)
			           IritMalloc(sizeof(IrtMdlrPoSTextureStruct));
            IrtDspOGLObjPropsClass *OGLProps;
            
            if (DefaultTexture[0][0].r == 0) {
                for (i = 0; i < 4; i++) {
                    for (j = 0; j < 4; j++) {
                        DefaultTexture[i][j].r = 255;
                        DefaultTexture[i][j].g = 255;
                        DefaultTexture[i][j].b = 255;
                    }
                }
            }
            Texture -> Width = IRT_MDLR_POS_DFLT_WIDTH;
            Texture -> Height = IRT_MDLR_POS_DFLT_HEIGHT;
            Texture -> Alpha = 0;
			Texture -> Saved = false;
            Texture -> Texture = (IrtImgPixelStruct *) 
                IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);
            for (i = 0; i < IRT_MDLR_POS_DFLT_SIZE; i++) {
                Texture -> Texture[i].r = 255;
                Texture -> Texture[i].g = 255;
                Texture -> Texture[i].b = 255;
            }
            IrtMdlrPoSTextures[Surface] = Texture;

            /* Apply dummy white texture. This ensure UV mapping is correct.*/
            GuIritMdlrDllSetTextureFromImage(FI, Surface, DefaultTexture, 
					     4, 4, FALSE, Span);
            
            /* Set Object color to white (default is red) */
            AttrIDSetObjectRGBColor(Surface, 255, 255, 255);
            OGLProps = GuIritMdlrDllGetObjectOGLProps(FI, Surface);

            if (OGLProps != NULL) {
                IrtDspRGBAClrClass
                    White = IrtDspRGBAClrClass(255, 255, 255);

                OGLProps -> FrontFaceColor.SetValue(White);
                OGLProps -> BackFaceColor.SetValue(White);
                OGLProps -> BDspListNeedsUpdate = true;
                GuIritMdlrDllRedrawScreen(FI);
            }
        }
        if (Surface != IrtMdlrPoSSurface) {
	    int i;
            IrtMdlrPoSTextureStruct
	        *Texture = IrtMdlrPoSTextures[Surface];

            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH, 
					   &Texture -> Width);
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT, 
					   &Texture -> Height);
            IrtMdlrPoSSurface = Surface;
            IrtMdlrPoSTextureAlpha = (IrtImgPixelStruct *)
                IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);
            IrtMdlrPoSTextureBuffer = (IrtImgPixelStruct *)
                IritMalloc(sizeof(IrtImgPixelStruct) * IRT_MDLR_POS_DFLT_SIZE);
            for (i = 0; i < IRT_MDLR_POS_DFLT_SIZE; i++) {
                IrtMdlrPoSTextureAlpha[i] = Texture -> Texture[i];
                IrtMdlrPoSTextureBuffer[i] = Texture -> Texture[i];
            }
            TextureInit = true;
        }
        PanelUpdate = false;
    }

    /* Texture fields. */
    if (FI -> IntermediateWidgetMajor == IRT_MDLR_POS_LOAD &&
	!PanelUpdate &&
	IrtMdlrPoSSurface != NULL) {
        char *Filename;
        bool
	    Res = GuIritMdlrDllGetAsyncInputFileName(FI,
						     "Load Texture from....", 
						     "*.png", &Filename);

        if (Res) {
	    int Width, Height,
                Alpha = 0;
            IrtMdlrPoSTextureStruct
	        *Texture = IrtMdlrPoSTextures[Surface];
            IrtImgPixelStruct
	        *Image = IrtImgReadImage2(Filename, &Width, &Height, &Alpha);

            Width++, Height++;
            Texture -> Alpha = Alpha;
			Texture -> Saved = false;
            GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, 
			 "Texture loaded successfully from \"%s\" (%dx%d)\n",
				Filename, Width, Height);
            IrtMdlrPoSResizeTexture(FI, Width, Height, false);

			int offsetW = 0;
			if (Width%4 != 0) {
				offsetW = 4-Width%4;
			}
            for (int h = 0; h <  Height; h++) {
				for (int w = 0; w < Width; w++) {
					Texture -> Texture[h*Width+w+(h*offsetW)] = Image[h*Width+w];
					IrtMdlrPoSTextureAlpha[h*Width+w+(h*offsetW)] = Image[h*Width+w];
					IrtMdlrPoSTextureBuffer[h*Width+w+(h*offsetW)] = Image[h*Width+w];
				}
            }

            GuIritMdlrDllSetTextureFromImage(FI, 
					     IrtMdlrPoSSurface, 
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
        IrtMdlrPoSSurface != NULL) {
        char *Filename;
        bool
	    Res = GuIritMdlrDllGetAsyncInputFileName(FI, 
						     "Save Texture to....", 
						     "*.png", &Filename);
        if (Res) {
            IrtMdlrPoSTextureStruct
	        *Texture = IrtMdlrPoSTextures[Surface];
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
        IrtMdlrPoSSurface != NULL) {
        bool
	    Res = GuIritMdlrDllGetAsyncInputConfirm(FI, 
                            "Texture Reset", 
                            "Are you sure you want to reset the texture ?");

        if (Res) {
            int Width = IRT_MDLR_POS_DFLT_WIDTH, 
                Height = IRT_MDLR_POS_DFLT_HEIGHT;
            IrtMdlrPoSTextureStruct
	        *Texture = IrtMdlrPoSTextures[Surface];

            IrtMdlrPoSResizeTexture(FI, IRT_MDLR_POS_DFLT_WIDTH, 
				    IRT_MDLR_POS_DFLT_HEIGHT, true);
			Texture -> Saved = false;
            PanelUpdate = true;
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_WIDTH, &Width);
            GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_HEIGHT, &Height);
            PanelUpdate = false;
            GuIritMdlrDllSetTextureFromImage(FI, IrtMdlrPoSSurface, 
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
            IrtMdlrPoSShape.Color[0] = Red;
            IrtMdlrPoSShape.Color[1] = Green;
            IrtMdlrPoSShape.Color[2] = Blue;
        }
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_WIDTH, &TextureWidth);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_HEIGHT, &TextureHeight);
	int oldWidth = TextureWidth;
	int oldHeight = TextureHeight;
	if (TextureHeight%4 != 0) {
		TextureHeight -= TextureHeight%4;
		TextureHeight += 4;
		
	}
	if (TextureWidth%4 != 0) {
		TextureWidth -= TextureWidth%4;
		TextureWidth += 4;
	}

    if (IrtMdlrPoSSurface != NULL) {
        IrtMdlrPoSTextureStruct
	    *Texture = IrtMdlrPoSTextures[Surface];

        if ((Texture -> Width != TextureWidth ||
	     Texture -> Height != TextureHeight) &&
            !PanelUpdate) {
            bool
	        Res = true;

            PanelUpdate = true;
            if (TextureInit) {
                IRT_DSP_STATIC_DATA bool
		    TimerInit = false;

                if (!TimerInit) {
                    Res = GuIritMdlrDllGetAsyncInputConfirm(FI, "", 
                        "This will reset the texture.\n" \
                        "Are you sure you want to resize the texture ?");
                    if (Res) {
                        TimerInit = true;
                    }
                }
            }
            if (Res) {
				if (oldHeight != TextureHeight) {
					GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Invalid Height (not a multiple of 4), changed to %d\n", TextureHeight);
				}
				if (oldWidth != TextureWidth) {
					GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Invalid Width (not a multiple of 4), changed to %d\n", TextureWidth);
				}
                IrtMdlrPoSResizeTexture(FI, TextureWidth, 
					TextureHeight, true);
                Texture -> Saved = false;
                GuIritMdlrDllSetTextureFromImage(FI, IrtMdlrPoSSurface, 
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
        &IrtMdlrPoSShape.Alpha);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_SHAPE, &TmpIndex, &TmpPtr);
    if (TmpIndex != ShapeIndex && IrtMdlrPoSShapesFiles != NULL) {
        ShapeIndex = TmpIndex;
        IrtMdlrPoSLoadShape(FI, IrtMdlrPoSShapesFiles[TmpIndex]);
    }

    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_X_FACTOR, &XFactor);
    GuIritMdlrDllGetInputParameter(FI, IRT_MDLR_POS_Y_FACTOR, &YFactor);

    if (IrtMdlrPoSShapesFiles != NULL && 
        (IRT_MDLR_POS_DIST(XFactor, IrtMdlrPoSShape.XFactor) > 
        IRT_MDLR_POS_EPSILON ||
        IRT_MDLR_POS_DIST(YFactor, IrtMdlrPoSShape.YFactor) > 
        IRT_MDLR_POS_EPSILON)) {
        IrtMdlrPoSShape.XFactor = XFactor;
        IrtMdlrPoSShape.YFactor = YFactor;
        IrtMdlrPoSLoadShape(FI, IrtMdlrPoSShapesFiles[TmpIndex]);
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
static void IrtMdlrPoSResizeTexture(IrtMdlrFuncInfoClass* FI,
                        int Width,
                        int Height,
                        bool Reset)
{
	if (Height%4 != 0) {
		Height -= Height%4;
		Height += 4;
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Invalid Height (not a multiple of 4), changed to %d\n", Height);
	}
	if (Width%4 != 0) {
		Width -= Width%4;
		Width += 4;
		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Invalid Width (not a multiple of 4), changed to %d\n", Width);
	}

    IrtImgPixelStruct
        *Texture = (IrtImgPixelStruct *) 
		      IritMalloc(sizeof(IrtImgPixelStruct) * Width * Height);
    IrtImgPixelStruct
        *TextureAlpha = (IrtImgPixelStruct *)
		      IritMalloc(sizeof(IrtImgPixelStruct) * Width * Height);
    IrtImgPixelStruct
        *TextureBuffer = (IrtImgPixelStruct *)
		      IritMalloc(sizeof(IrtImgPixelStruct) * Width * Height);

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
    IritFree(IrtMdlrPoSTextures[IrtMdlrPoSSurface] -> Texture);
    IritFree(IrtMdlrPoSTextureAlpha);
    IritFree(IrtMdlrPoSTextureBuffer);
    IrtMdlrPoSTextures[IrtMdlrPoSSurface] -> Width = Width;
    IrtMdlrPoSTextures[IrtMdlrPoSSurface] -> Height = Height;
    IrtMdlrPoSTextures[IrtMdlrPoSSurface] -> Texture = Texture;
    IrtMdlrPoSTextureAlpha = TextureAlpha;
    IrtMdlrPoSTextureBuffer = TextureBuffer;

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

    sprintf(FilePath, "%s%s",
	    searchpath(SysFileNames -> AuxiliaryDataName, Base),
	    GUIRIT_DLL_PAINT_ON_SRF_FILE_RELATIVE_PATH);

    if (IrtMdlrPoSShapesFiles == NULL) {
        IrtMdlrPoSShapesFiles = 
            GuIritMdlrDllGetAllFilesNamesInDirectory(FI, FilePath,
				            "*.rle|*.ppm|*.gif|*.jpeg|*.png");
    }

    if (IrtMdlrPoSShapesFiles == NULL) {
	GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_ERROR,
		    "Masks files were not found. Check directory: \"%s\"\n",
		    FilePath);
	return FALSE;
    }

    strcpy(IrtMdlrPoSShapesNames, "");
    for (i = 0; IrtMdlrPoSShapesFiles[i] != NULL; ++i) {
        p = strstr(IrtMdlrPoSShapesFiles[i], 
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
            sprintf(IrtMdlrPoSShapesNames, "%s%s;", 
		    IrtMdlrPoSShapesNames, Name);
        }
    }

    if (IrtMdlrPoSShapesFiles[0] != NULL) {
        GuIritMdlrDllSetInputSelectionStruct
	    Files(IrtMdlrPoSShapesNames);

        sprintf(IrtMdlrPoSShapesNames, "%s:%d", IrtMdlrPoSShapesNames, 
		Index);
        GuIritMdlrDllSetInputParameter(FI, IRT_MDLR_POS_SHAPE, &Files);
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

    Width++, Height++;
    IritFree(IrtMdlrPoSShape.Shape);

    IrtMdlrPoSShape.Width = (int) (Width * IrtMdlrPoSShape.XFactor);
    IrtMdlrPoSShape.Height = (int) (Height * IrtMdlrPoSShape.YFactor);
    Size = IrtMdlrPoSShape.Height * IrtMdlrPoSShape.Width;
    IrtMdlrPoSShape.Shape = (float *) IritMalloc(sizeof(float) * Size);
    XRatio = (float) Width / (float) IrtMdlrPoSShape.Width;
    YRatio = (float) Height / (float) IrtMdlrPoSShape.Height;

    for (y = 0; y < IrtMdlrPoSShape.Height; y++) {
        for (x = 0; x < IrtMdlrPoSShape.Width; x++) {
            int Off = y * IrtMdlrPoSShape.Width + x;
            float gray;
            IrtImgPixelStruct
	        *ptr = Image + (int)((int)(y * YRatio) * Width + x * XRatio);

            IRT_DSP_RGB2GREY(ptr, gray);
            IrtMdlrPoSShape.Shape[Off] = (float)(gray / 255.0);
        }
    }
    GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Loaded shape of size %dx%d\n",
			IrtMdlrPoSShape.Width, 
			IrtMdlrPoSShape.Height);
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
static void IrtMdlrPoSRenderShape(IrtMdlrFuncInfoClass* FI,
                        int XOff,
                        int YOff)
{
    int u, v,
        XMin = XOff - IrtMdlrPoSShape.Width / 2,
        YMin = (YOff - IrtMdlrPoSShape.Height / 2);
    IrtMdlrPoSTextureStruct* 
        Texture = IrtMdlrPoSTextures[IrtMdlrPoSSurface];
    /* Modulo needs positive values to work as intended */
    while (XMin < 0) {
        XMin += Texture -> Width;
    }
    while (YMin < 0) {
        YMin += Texture -> Height;
    }
    XMin %= Texture -> Width;
    YMin %= Texture -> Height;

    for (v = 0; v < IrtMdlrPoSShape.Height; v++) {
        for (u = 0; u < IrtMdlrPoSShape.Width; u++) {
            int x = (XMin + u) % Texture -> Width,
                y = (YMin + v) % Texture -> Height,
                TextureOff = x + Texture -> Width * y,
                ShapeOff = u + IrtMdlrPoSShape.Width * v;

            IrtMdlrPoSTextureAlpha[TextureOff].r += 
                (IrtBType)((IrtMdlrPoSShape.Color[0]
                - IrtMdlrPoSTextureAlpha[TextureOff].r) 
                * IrtMdlrPoSShape.Shape[ShapeOff]);
            IrtMdlrPoSTextureAlpha[TextureOff].g += 
                (IrtBType)((IrtMdlrPoSShape.Color[1]
                - IrtMdlrPoSTextureAlpha[TextureOff].g) 
                * IrtMdlrPoSShape.Shape[ShapeOff]);
            IrtMdlrPoSTextureAlpha[TextureOff].b +=
                (IrtBType)((IrtMdlrPoSShape.Color[2]
                - IrtMdlrPoSTextureAlpha[TextureOff].b) 
                * IrtMdlrPoSShape.Shape[ShapeOff]);
            Texture -> Texture[TextureOff].r = 
                (IrtBType)(IrtMdlrPoSTextureBuffer[TextureOff].r 
                + (IrtMdlrPoSTextureAlpha[TextureOff].r 
                - IrtMdlrPoSTextureBuffer[TextureOff].r) 
                * (IrtMdlrPoSShape.Alpha / 255.0));
            Texture -> Texture[TextureOff].g = 
                (IrtBType)(IrtMdlrPoSTextureBuffer[TextureOff].g 
                + (IrtMdlrPoSTextureAlpha[TextureOff].g 
                - IrtMdlrPoSTextureBuffer[TextureOff].g)
                * (IrtMdlrPoSShape.Alpha / 255.0));
            Texture -> Texture[TextureOff].b = 
                (IrtBType)(IrtMdlrPoSTextureBuffer[TextureOff].b 
                + (IrtMdlrPoSTextureAlpha[TextureOff].b 
                - IrtMdlrPoSTextureBuffer[TextureOff].b) 
                * (IrtMdlrPoSShape.Alpha / 255.0));
        }
    }
    Texture -> Saved = false;
}

/*****************************************************************************
* DESCRIPTION:                                                               *
*   Callback function for mouse events for painting .          	             *
*                                                                            *
* PARAMETERS:                                                                *
*   MouseEvent:  The mouse call back event to handle.		                 *
*                                                                            *
* RETURN VALUE:                                                              *
*   int: True if event handled, false to propagate event to next handler.    *
*****************************************************************************/
static int IrtMdlrPoSMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent)
{
    IRT_DSP_STATIC_DATA bool 
        Clicking = false;
    IRT_DSP_STATIC_DATA int 
        PrevXOff = -1,
        PrefYOff = -1;
    IrtRType Span[2] = {1.0, 1.0};
    IrtMdlrFuncInfoClass 
        *FI = (IrtMdlrFuncInfoClass *) MouseEvent -> Data;
    IPObjectStruct 
        *PObj = (IPObjectStruct *) MouseEvent -> PObj;

    if (MouseEvent -> KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN) {
        switch (MouseEvent -> Type) {
            case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
	        GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
		Clicking = TRUE;
		break;

            case IRT_DSP_MOUSE_EVENT_LEFT_UP:
	        GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
		Clicking = FALSE;
		if (IrtMdlrPoSSurface != NULL) {
		    int x, y;
		    IrtMdlrPoSTextureStruct
		        *Texture = IrtMdlrPoSTextures[IrtMdlrPoSSurface];

		    for (y = 0; y < Texture -> Height; y++) {
		        for (x = 0; x < Texture -> Width; x++) {
			    int offset = y * Texture -> Width + x;

			    IrtMdlrPoSTextureAlpha[offset] = 
			                          Texture -> Texture[offset];
			    IrtMdlrPoSTextureBuffer[offset] = 
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
	    MouseEvent -> UV != NULL &&
	    PObj != NULL &&
            IP_IS_SRF_OBJ(PObj) &&
	    PObj == IrtMdlrPoSSurface) {
            IrtMdlrPoSTextureStruct 
                *Texture = IrtMdlrPoSTextures[IrtMdlrPoSSurface];
            int XOff = (int) ((double) Texture -> Width *
			                               MouseEvent -> UV[0]),
                YOff = (int) ((double) Texture -> Height *
			                               MouseEvent -> UV[1]);

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
                IrtMdlrPoSSurface, 
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
