/*****************************************************************************
* OglRendererBase.h - header file for the gr::OglRendererBaseClass class     *
* A base class for OpenGL-based implementations of the gr::IRenderer class.  *
******************************************************************************
* (C) Gershon Elber, Technion, Israel Institute of Technology                *
******************************************************************************
* Written by: Daniil Rodin                            Ver 1.0, March 2016.   *
*****************************************************************************/

#ifndef IRT_DSP_OGL_RENDERER_BASE_H
#define IRT_DSP_OGL_RENDERER_BASE_H

#include "IrtDspBasicDefs.h"
#include "IrtDspBsList.h"
#include "IrtDspGrIRenderer.h"
#include "Geometry/GeometryExtractor.h"
#include "Auxiliary/IOglAntiAliasing.h"
#include "Auxiliary/IOglGridRenderer.h"
#include "Auxiliary/IOglObjectRenderingContext.h"
#include "Auxiliary/OglRenderingCacheBase.h"
#include "Auxiliary/IOglSelectedPolygonsRenderer.h"

namespace gr
{
    class OglRendererBaseClass : public IRenderer
    {
    protected:
        IOglAntiAliasing* AntiAliasings[6];
        IOglObjectRenderingContext* ObjectRenderingContext;
        IOglSelectedPolygonsRenderer* SelectedPolygonsRenderer;
        IOglGridRenderer* GridRenderer;
        GeometryExtractorClass *GeometryExtractor;

        IrtDspOGLWinPropsClass *OGLWinProps;
        GridProperties GridProps;
        bool WindowIsFocused;
        bool WindowIsDragged;
    public:
        OglRendererBaseClass();
        virtual ~OglRendererBaseClass();
        virtual void Render(IrtDspOGLWinPropsClass* OGLWinProps, 
                            int ScreenWidth, 
                            int ScreenHeight, 
                            const Matrix4x4& View, 
                            const Matrix4x4& Projection, 
                            const Vector3& Eye, 
                            const Vector3& ViewDir, 
                            bool WindowIsFocused, 
                            bool WindowIsDragged, 
                            bool ResetCache, 
                            long* NumRenderedPolys);
    private:
        IOglAntiAliasing* GetAntiAliasing(int SampleCount);
        void SetViewport(int ScreenWidth, int ScreenHeight);
        void RenderGeometry(const Matrix4x4& View,
                            float AspectRatio,
                            bool ForceDLRebuild, 
                            long* NumRenderedPolys);
        void DrawObjectTransInfo();
        void RenderObject(IrtDspDbObjectClass *DbObj,
                          bool ForceDLRebuild,
                          long* NumRenderedPolys);
        void PrepareObjectCache(IrtDspDbObjectClass *DbObj,
                                bool ForceRebuildCache);
        void RenderObjectUsingCache(IrtDspDbObjectClass* DbObj);
        void HandleObjectTexture(IrtDspDbObjectClass* DbObj);
        static void SortObjsByScreenSpaceZ(IrtDspObjectPList &ObjList, 
                                           const Matrix4x4& View);
        static int IndexOf(IrtDspObjectPList &ObjectsList, const char *Name);
        static int CompareZDepthValues(const VoidPtr Obj1, const VoidPtr Obj2);
        static Color4 GetBorderColor(bool IsFocused, bool IsDragged);
    protected:
        virtual void SetAdditionalGlobalState() = 0;
        virtual IOglAntiAliasing* CreateAntiAliasing(int SampleCount) = 0;
        virtual void ClearBuffers(const IrtDspRGBAClrClass& IrtColor) = 0;
        virtual void SetCamera(const Matrix4x4& View,
                       const Matrix4x4& Projection,
                       const Vector3& Eye,
                       const Vector3& ViewDir,
                       float AspectRatio,
                       bool IsPerspective) = 0;
        virtual void SetLights(IrtDspObjectPList *LightSourcesList) = 0;
        virtual void SetFog() = 0;
        virtual bool IsObjectCacheAppropriate(const IObjectRenderingCache
								  *Cache) = 0;
        virtual OglRenderingCacheBase* CreateObjectCache() = 0;
	virtual void RenderBorder(Color4 Color) = 0;
	virtual void RenderBackground(bool GradiengColor,
				      const IrtDspRGBAClrClass &TopLeftColor,
				      const IrtDspRGBAClrClass &TopRightColor,
				      const IrtDspRGBAClrClass &BotLeftColor,
				      const IrtDspRGBAClrClass &BotRightColor)
								          = 0;
    };
}

#endif