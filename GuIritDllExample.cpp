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
#include "glew.h"
#include "png.h"

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass* FI);
static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct* MouseEvent);
static void SaveTexture(IrtMdlrFuncInfoClass* FI, GLuint handle);
static void saveScreenshotToFile(IrtMdlrFuncInfoClass* FI, std::string filename, int windowWidth, int windowHeight);

IRT_DSP_STATIC_DATA IrtRType R = 255;
IRT_DSP_STATIC_DATA IrtRType G = 255;
IRT_DSP_STATIC_DATA IrtRType B = 255;
IRT_DSP_STATIC_DATA IrtRType alpha = 0;
IRT_DSP_STATIC_DATA IrtRType dotSize = 5;
IRT_DSP_STATIC_DATA IrtMdlrFuncInfoClass* GlobalFI = NULL;

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
			5,
			IRT_MDLR_PARAM_EXACT,
		{
			IRT_MDLR_NUMERIC_EXPR,
				IRT_MDLR_NUMERIC_EXPR,
				IRT_MDLR_NUMERIC_EXPR,
				IRT_MDLR_NUMERIC_EXPR,
				IRT_MDLR_NUMERIC_EXPR,
		},
		{
			&R,
				&G,
				&B,
				&alpha,
				&dotSize

			},
			{
				"R",
					"G",
					"B",
					"Alpha",
					"Size"

			},
			{
				"Red component",
					"Green component",
					"Blue component",
					"Alpha component",
					"Size of the dot"
				}
	}
};

IRT_DSP_STATIC_DATA const int FUNC_TABLE_SIZE = sizeof(TexturePainterFunctionTable) / sizeof(IrtMdlrFuncTableStruct);

extern "C" bool _IrtMdlrDllRegister(void)
{
	GuIritMdlrDllRegister(TexturePainterFunctionTable, FUNC_TABLE_SIZE, "Example", IconMenuExample);
	return true;
}

static void IrtMdlrTexturePainter(IrtMdlrFuncInfoClass* FI)
{
	IPObjectStruct* ResultObject;

	if (FI->InvocationNumber == 0 && GlobalFI == NULL) {
		GuIritMdlrDllPushMouseEventFunc(
			FI,
			IrtMdlrTexturePainterMouseCallBack,
			(IrtDspMouseEventType)(IRT_DSP_MOUSE_EVENT_LEFT),
			(IrtDspKeyModifierType)(IRT_DSP_KEY_MODIFIER_CTRL_DOWN | IRT_DSP_KEY_MODIFIER_SHIFT_DOWN),
			FI);
		GlobalFI = FI;
	}

	GuIritMdlrDllGetInputParameter(FI, 0, &R);
	GuIritMdlrDllGetInputParameter(FI, 1, &G);
	GuIritMdlrDllGetInputParameter(FI, 2, &B);
	GuIritMdlrDllGetInputParameter(FI, 3, &alpha);
	GuIritMdlrDllGetInputParameter(FI, 4, &dotSize);

	if (FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_APPLY ||
		FI->CnstrctState == IRT_MDLR_CNSTRCT_STATE_OK)
	{
		ResultObject = IPGenNUMValObject(0);
		GuIritMdlrDllSetObjectName(FI, ResultObject, "Painter");
		GuIritMdlrDllInsertModelingFuncObj(FI, ResultObject);
	}
}



static int IrtMdlrTexturePainterMouseCallBack(IrtMdlrMouseEventStruct * MouseEvent)
{
	IrtMdlrFuncInfoClass* FI = (IrtMdlrFuncInfoClass*)MouseEvent->Data;
	IRT_DSP_STATIC_DATA int MouseActive = FALSE;
	IRT_DSP_STATIC_DATA IrtPtType Point;

	GLuint handle = 0;
	const IPObjectStruct* PObj = MouseEvent->PObj;
	handle = AttrIDGetObjectIntAttrib(PObj, IRIT_ATTR_CREATE_ID(_ImageTexID));

	if (MouseEvent->KeyModifiers & IRT_DSP_KEY_MODIFIER_CTRL_DOWN)
	{
		switch (MouseEvent->Type)
		{
		case IRT_DSP_MOUSE_EVENT_LEFT_DOWN:
			GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, true);
			MouseActive = TRUE;
			break;
		case IRT_DSP_MOUSE_EVENT_LEFT_UP:
			GuIritMdlrDllCaptureCursorFocus(FI, MouseEvent, false);
			MouseActive = FALSE;
			//SaveTexture(FI, handle);
			break;
		}
		if (MouseActive) {				
			if (handle > 0 && MouseEvent->UV != NULL) {
				glBindTexture(GL_TEXTURE_2D, handle);

				GLint width;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
				GLint height;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

				//int vv[dotSize][dotSize] = {0};uint8_t data[4]
				uint8_t* vv = (uint8_t*)malloc(sizeof(uint8_t)*dotSize*dotSize*4);
				for (int i = 0; i < dotSize*dotSize; i++) {
					vv[i*4] = R;
					vv[i*4+1] = G;
					vv[i*4+2] = B;
					vv[i*4+3] = alpha;
				}
				glTexSubImage2D(GL_TEXTURE_2D, 0, MouseEvent->UV[0]*width, MouseEvent->UV[1]*height, dotSize, dotSize, GL_RGBA, GL_UNSIGNED_BYTE, vv);
				delete vv;
			}

			else {
				GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "No assigned texture\n");
			}

		}
	}
	//if (IRT_DSP_KEY_MODIFIER_SHIFT_DOWN) {
	//GLvoid* pixels = NULL;
	//glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	//GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "%s\n", pixels);
	//AttrIDGetObjectIntAttrib(PObj, .mb_str(wxConvUTF8));
	//}
	return TRUE;
}


//void saveScreenshotToFile(IrtMdlrFuncInfoClass* FI, std::string filename, int windowWidth, int windowHeight) {    
//	const int numberOfPixels = windowWidth * windowHeight * 4;
//	uint8_t* pixels = (uint8_t*) malloc (sizeof(uint8_t)*numberOfPixels);
//
//
//	// glPixelStorei(GL_PACK_ALIGNMENT, 1);
//	// glReadBuffer(GL_FRONT);
//	glReadPixels(GL_TEXTURE_2D, 0, windowWidth, windowHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
//	if(pixels == NULL){
//		GuIritMdlrDllPrintf(FI, IRT_DSP_LOG_INFO, "Meh\n");
//	}
//
//	FILE *outputFile = fopen(filename.c_str(), "w");
//	short header[] = {0, 2, 0, 0, 0, 0, (short) windowWidth, (short) windowHeight, 24};
//
//	fwrite(&header, sizeof(header), 1, outputFile);
//	fwrite(pixels, numberOfPixels, 1, outputFile);
//	fclose(outputFile);
//
//	//printf("Finish writing to file.\n");
//	delete pixels;
//}
//
//bool save_screenshot(std::string filename, int w, int h)
//{	
//	//This prevents the images getting padded 
//	// when the width multiplied by 3 is not a multiple of 4
//	glPixelStorei(GL_PACK_ALIGNMENT, 1);
//
//	int nSize = w*h*3;
//	// First let's create our buffer, 3 channels per Pixel
//	char* dataBuffer = (char*)malloc(nSize*sizeof(char));
//
//	if (!dataBuffer) return false;
//
//	// Let's fetch them from the backbuffer	
//	// We request the pixels in GL_BGR format, thanks to Berzeger for the tip
//	glReadPixels((GLint)0, (GLint)0,
//		(GLint)w, (GLint)h,
//		GL_BGR, GL_UNSIGNED_BYTE, dataBuffer);
//
//	//Now the file creation
//	FILE *filePtr = fopen(filename.c_str(), "wb");
//	if (!filePtr) return false;
//
//
//	unsigned char TGAheader[12]={0,0,2,0,0,0,0,0,0,0,0,0};
//	unsigned char header[6] = { w%256,w/256,
//		h%256,h/256,
//		24,0};
//	// We write the headers
//	fwrite(TGAheader,	sizeof(unsigned char),	12,	filePtr);
//	fwrite(header,	sizeof(unsigned char),	6,	filePtr);
//	// And finally our image data
//	fwrite(dataBuffer,	sizeof(GLubyte),	nSize,	filePtr);
//	fclose(filePtr);
//
//	free(dataBuffer);
//
//	return true;
//}

//static int save_png(std::string filename, int width, int height,
//                     int bitdepth, int colortype,
//                     unsigned char* data, int pitch, int transform)
// {
//   int i = 0;
//   int r = 0;
//   FILE* fp = NULL;
//   png_structp png_ptr = NULL;
//   png_infop info_ptr = NULL;
//   png_bytep* row_pointers = NULL;
// 
//   if (NULL == data) {
//     printf("Error: failed to save the png because the given data is NULL.\n");
//     r = -1;
//     goto error;
//   }
// 
//   if (0 == filename.size()) {
//     printf("Error: failed to save the png because the given filename length is 0.\n");
//     r = -2;
//     goto error;
//   }
// 
//   if (0 == pitch) {
//     printf("Error: failed to save the png because the given pitch is 0.\n");
//     r = -3;
//     goto error;
//   }
// 
//   fp = fopen(filename.c_str(), "wb");
//   if (NULL == fp) {
//     printf("Error: failed to open the png file: %s\n", filename.c_str());
//     r = -4;
//     goto error;
//   }
// 
//   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
//   if (NULL == png_ptr) {
//     printf("Error: failed to create the png write struct.\n");
//     r = -5;
//     goto error;
//   }
// 
//   info_ptr = png_create_info_struct(png_ptr);
//   if (NULL == info_ptr) {
//     printf("Error: failed to create the png info struct.\n");
//     r = -6;
//     goto error;
//   }
// 
//   png_set_IHDR(png_ptr,
//                info_ptr,
//                width,
//                height,
//                bitdepth,                 /* e.g. 8 */
//                colortype,                /* PNG_COLOR_TYPE_{GRAY, PALETTE, RGB, RGB_ALPHA, GRAY_ALPHA, RGBA, GA} */
//                PNG_INTERLACE_NONE,       /* PNG_INTERLACE_{NONE, ADAM7 } */
//                PNG_COMPRESSION_TYPE_BASE,
//                PNG_FILTER_TYPE_BASE);
// 
//   row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
// 
//   for (i = 0; i < height; ++i) {
//     row_pointers[i] = data + i * pitch;
//   }
// 
//   png_init_io(png_ptr, fp);
//   png_set_rows(png_ptr, info_ptr, row_pointers);
//   png_write_png(png_ptr, info_ptr, transform, NULL);
// 
//  error:
// 
//   if (NULL != fp) {
//     fclose(fp);
//     fp = NULL;
//   }
// 
//   if (NULL != png_ptr) {
// 
//     if (NULL == info_ptr) {
//       printf("Error: info ptr is null. not supposed to happen here.\n");
//     }
// 
//     png_destroy_write_struct(&png_ptr, &info_ptr);
//     png_ptr = NULL;
//     info_ptr = NULL;
//   }
// 
//   if (NULL != row_pointers) {
//     free(row_pointers);
//     row_pointers = NULL;
//   }
// 
//   printf("And we're all free.\n");
// 
//   return r;
// }
//
//static void SaveTexture(IrtMdlrFuncInfoClass* FI, GLuint handle) {
//	glBindTexture(GL_TEXTURE_2D, handle);
//	uint8_t* pixels = (uint8_t*) malloc (sizeof(uint8_t)*4*300*300);
//	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
//
//	//saveScreenshotToFile(FI, "C:\tata.tga", 300, 300);
//	//save_screenshot("C:\tata.tga", 300, 300);
//	//save_png("C:\tata.tga", 300, 300, 8, PNG_COLOR_TYPE_RGBA, pixels, 4*300, PNG_TRANSFORM_IDENTITY);
//	delete pixels;
//}