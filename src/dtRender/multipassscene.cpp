/* -*-c++-*-
* Delta3D Open Source Game and Simulation Engine
* Copyright (C) 2014, Caper Holdings, LLC
*
* This library is free software; you can redistribute it and/or modify it under
* the terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; either version 2.1 of the License, or (at your option)
* any later version.
*
* This library is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
* details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this library; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*
* Bradley Anderegg
*/

#include <dtRender/multipassscene.h>
#include <dtRender/ppuscene.h>

//for getting camera
#include <dtRender/scenemanager.h>

#include <dtCore/observerptr.h>

#include <dtCore/camera.h>
#include <dtCore/propertymacros.h>
#include <dtUtil/nodemask.h>
#include <dtUtil/cullmask.h>
#include <osg/Camera>
#include <osg/Viewport>

#include <osgPPU/Processor.h>
#include <osgPPU/Unit.h>
#include <osgPPU/UnitCamera.h>
#include <osgPPU/UnitOut.h>
#include <osgPPU/UnitBypass.h>
#include <osgPPU/UnitDepthbufferBypass.h>
#include <osgPPU/UnitCameraAttachmentBypass.h>
#include <osgPPU/UnitInResampleOut.h>

#include <osg/Texture2D>

#include <dtCore/scene.h>
#include <dtABC/application.h>

namespace dtRender
{
   const int MultipassScene::TEXTURE_UNIT_PREDEPTH = 6;

   const int MultipassScene::TEXTURE_UNIT_REFLECTION = 7;

   const std::string MultipassScene::REFLECTION_TEXTURE_UNIFORM("d3d_ReflectionCubeMap");

   const std::string MultipassScene::UNIFORM_DEPTH_ONLY_PASS("d3d_DepthOnlyPass");
   const std::string MultipassScene::UNIFORM_PREDEPTH_TEXTURE("d3d_PreDepthTexture");


   //class UpdateUniformsCallback : public osg::NodeCallback


   class UpdateCameraCallback : public osg::NodeCallback
   {
   public:

      UpdateCameraCallback(osg::Camera* target, osg::Camera* camera)
         : mTarget(target)
         , mCamera(camera)
      {
      }

      virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
      {
         // first update subgraph to make sure objects are all moved into postion
         traverse(node,nv);

         mCamera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
         mCamera->setProjectionMatrix(mTarget->getProjectionMatrix());;
         mCamera->setViewMatrix(mTarget->getViewMatrix());

      }

   protected:

      virtual ~UpdateCameraCallback() {}

      dtCore::ObserverPtr<osg::Camera>                mTarget;
      dtCore::ObserverPtr<osg::Camera>                mCamera;

   };

   class PreDepthDrawCallback : public osg::Camera::DrawCallback
   {
   public:

      enum Phase{ PRE_DRAW, POST_DRAW};

      PreDepthDrawCallback(osg::Camera* depthCam, osg::Node& n, Phase p)
         : mDepthCamera(depthCam)
         , mNode(&n)
         , mPhase(p)
      {

      }

      ~PreDepthDrawCallback(){}


      virtual void operator () (const osg::Camera& /*camera*/) const
      {
         if(mNode.valid())
         {

            osg::StateSet* ss = mNode->getOrCreateStateSet();
     
            osg::Uniform* sceneDepthUniform = ss->getOrCreateUniform(MultipassScene::UNIFORM_DEPTH_ONLY_PASS, osg::Uniform::BOOL);
            sceneDepthUniform->setDataVariance(osg::Object::DYNAMIC);

            if(mPhase == PRE_DRAW)
            {
               sceneDepthUniform->set(true);
            }
            else
            {
               sceneDepthUniform->set(false);
            }
         }
      }

   private:

      dtCore::ObserverPtr<osg::Camera> mDepthCamera;
      dtCore::ObserverPtr<osg::Node> mNode;
      Phase mPhase;

   };


   const dtCore::RefPtr<SceneType> MultipassScene::MULTIPASS_SCENE(new SceneType("Multipass Scene", "Scene", "Makes the main scene render to a render target."));

   class MultipassSceneImpl
   {
   public:
      MultipassSceneImpl()
         : mColorImageFormat(GL_RGBA16F_ARB)
         , mEnableColorBypass(true)
         , mEnableResampleColor(true)
         , mDepthImageFormat(GL_DEPTH_COMPONENT)
         , mEnableDepthBypass(true)
         , mPreDepthImageFormat(GL_DEPTH_COMPONENT32)
         , mEnablePreDepthPass(true)
         , mResampleColorFactor(0.5)
      {
         

      }
      
      ~MultipassSceneImpl()
      {
         mMultipassCamera= NULL;
         mPreDepthSceneCamera = NULL;

         mPPUProcessor= NULL;

         mColorBypass= NULL;
         mResampleColor= NULL;
         mDepthBufferBypass= NULL;
         mUnitOut= NULL;

         mFirstUnit= NULL;
         mLastUnit= NULL;
      }

      int mColorImageFormat;
      bool mEnableColorBypass;
      bool mEnableResampleColor;

      int mDepthImageFormat;
      bool mEnableDepthBypass;
      
      int mPreDepthImageFormat;
      bool mEnablePreDepthPass;
      
      float mResampleColorFactor;

      dtCore::RefPtr<osg::Texture> mPreDepthTexture;
      dtCore::RefPtr<osg::Camera> mMultipassCamera;
      dtCore::RefPtr<osg::Camera> mPreDepthSceneCamera;

      dtCore::RefPtr<osgPPU::Processor> mPPUProcessor;

      dtCore::RefPtr<osgPPU::UnitBypass> mColorBypass;
      dtCore::RefPtr<osgPPU::UnitInResampleOut> mResampleColor;
      dtCore::RefPtr<osgPPU::UnitDepthbufferBypass> mPreDepthBufferBypass;
      dtCore::RefPtr<osgPPU::UnitDepthbufferBypass> mDepthBufferBypass;

      dtCore::RefPtr<osgPPU::UnitOut> mUnitOut;

      dtCore::ObserverPtr<osgPPU::Unit> mFirstUnit;
      dtCore::ObserverPtr<osgPPU::Unit> mLastUnit;

   };

   MultipassScene::MultipassScene()
   : BaseClass(*MULTIPASS_SCENE, SceneEnum::MULTIPASS)
   , mImpl(new MultipassSceneImpl())
   {
      SetName("MultipassScene");
      
   }

   MultipassScene::MultipassScene(const SceneType& sceneId, const SceneEnum& defaultScene)
      : BaseClass(sceneId, defaultScene)
      , mImpl(new MultipassSceneImpl())
   {
      SetName("MultipassScene");

   }


   MultipassScene::~MultipassScene()
   {
      delete mImpl;
   }


   void MultipassScene::CreateScene( SceneManager& sm, const GraphicsQuality& g)
   {
       BaseClass::CreateScene(sm, g);

      dtCore::Camera* mainSceneCamera = sm.GetSceneCamera();

      if(mainSceneCamera != NULL)
      {
         osg::Camera* mainSceneOSGCamera = mainSceneCamera->GetOSGCamera();
       
         //dtCore::Camera* multiPassCamera = new dtCore::Camera();
         osg::Camera* multiPassOSGCam = new osg::Camera();
         mImpl->mMultipassCamera = multiPassOSGCam;

         //setup a render to texture camera for the multi pass scene
         SetupMultipassCamera(*mainSceneOSGCamera, *mainSceneOSGCamera->getViewport(), true, false);
         SetupMultipassCamera(*multiPassOSGCam, *mainSceneOSGCamera->getViewport(), mImpl->mEnableColorBypass, mImpl->mEnableDepthBypass);
         
         //create ppu processor
         mImpl->mPPUProcessor = new osgPPU::Processor();
         mImpl->mPPUProcessor->setCamera(mainSceneOSGCamera);
         mImpl->mPPUProcessor->dirtyUnitSubgraph();
         GetSceneNode()->addChild(mImpl->mPPUProcessor.get());

         //add the main scene to the multipass camera
         multiPassOSGCam->addChild(sm.GetOSGNode());
         
         //put at end of the pre render pass
         multiPassOSGCam->setRenderOrder(osg::Camera::PRE_RENDER, 1000);
         
         GetSceneNode()->setNodeMask(dtUtil::NodeMask::MULTIPASS);
         
         mainSceneOSGCamera->setCullMask(dtUtil::CullMask::MAIN_CAMERA_MULTIPASS);
         multiPassOSGCam->setCullMask(dtUtil::CullMask::ADDITIONAL_CAMERA_MULTIPASS);
         
         //keep the multipass camera in synch with the main camera
         multiPassOSGCam->setUpdateCallback(new UpdateCameraCallback(mainSceneOSGCamera, multiPassOSGCam));
                

         // setup unit, which will bring the camera and its output into the pipeline
         osgPPU::UnitCamera* unitMultipassCamera = new osgPPU::UnitCamera;
         unitMultipassCamera->setCamera(multiPassOSGCam);
         mImpl->mPPUProcessor->addChild(unitMultipassCamera);

         mImpl->mUnitOut = new osgPPU::UnitOut();
         mImpl->mUnitOut->setName("PipelineResult");
         mImpl->mUnitOut->setInputTextureIndexForViewportReference(-1); 
         
         if(mImpl->mEnablePreDepthPass)
         {
            mImpl->mPreDepthSceneCamera = new osg::Camera();

            PreDepthDrawCallback* vrcPre = new PreDepthDrawCallback(mImpl->mPreDepthSceneCamera, *sm.GetOSGNode(), PreDepthDrawCallback::PRE_DRAW);
            PreDepthDrawCallback* vrcPost = new PreDepthDrawCallback(mImpl->mPreDepthSceneCamera, *sm.GetOSGNode(), PreDepthDrawCallback::POST_DRAW);

            mImpl->mPreDepthSceneCamera->setPreDrawCallback(vrcPre);
            mImpl->mPreDepthSceneCamera->setPostDrawCallback(vrcPost);

            mImpl->mPreDepthSceneCamera->setRenderOrder(osg::Camera::PRE_RENDER);
            mImpl->mPreDepthSceneCamera->setCullMask(dtUtil::CullMask::MULTIPASS_DEPTH_ONLY_MASK);

            mImpl->mPreDepthTexture = SetupMultipassCamera(*mImpl->mPreDepthSceneCamera, *mainSceneOSGCamera->getViewport(), false, true);
           
            //keep the predepth camera in synch with the main camera
            mImpl->mPreDepthSceneCamera->setUpdateCallback(new UpdateCameraCallback(mainSceneOSGCamera, mImpl->mPreDepthSceneCamera.get()));

            /*osgPPU::UnitCamera* unitPreDepthCamera = new osgPPU::UnitCamera;
            unitPreDepthCamera->setCamera(mImpl->mPreDepthSceneCamera.get());
            mImpl->mPPUProcessor->addChild(unitPreDepthCamera);

            mImpl->mPreDepthBufferBypass = new osgPPU::UnitDepthbufferBypass();
            mImpl->mPreDepthBufferBypass->setName("DepthBypass");
            unitPreDepthCamera->addChild(mImpl->mPreDepthBufferBypass.get());

            SetFirstUnit(*mImpl->mPreDepthBufferBypass);
            SetLastUnit(*mImpl->mPreDepthBufferBypass);*/
            
            mImpl->mPreDepthSceneCamera->addChild(sm.GetOSGNode());
            mainSceneOSGCamera->addChild(mImpl->mPreDepthSceneCamera);

         }

         if(mImpl->mEnableColorBypass)
         {
            osgPPU::UnitCameraAttachmentBypass* cameraBypass = new osgPPU::UnitCameraAttachmentBypass();
            cameraBypass->setBufferComponent(osg::Camera::COLOR_BUFFER0);

            mImpl->mColorBypass = cameraBypass;
            mImpl->mColorBypass->setName("ColorBypass");
            
            if(GetFirstUnit() == NULL)
            {
               SetFirstUnit(*mImpl->mColorBypass);
            }

            SetLastUnit(*mImpl->mColorBypass);
            
            unitMultipassCamera->addChild(mImpl->mColorBypass.get());
         }

         if(mImpl->mEnableDepthBypass)
         {
            mImpl->mDepthBufferBypass = new osgPPU::UnitDepthbufferBypass();
            mImpl->mDepthBufferBypass->setName("DepthBypass");

            if(GetFirstUnit() == NULL)
            {
               SetFirstUnit(*mImpl->mDepthBufferBypass);
            }

            SetLastUnit(*mImpl->mDepthBufferBypass);

            unitMultipassCamera->addChild(mImpl->mDepthBufferBypass.get());
         }

         if(mImpl->mEnableResampleColor)
         {
            mImpl->mResampleColor = new osgPPU::UnitInResampleOut();
            mImpl->mResampleColor->setName("ResampleColor");
            mImpl->mResampleColor->setFactorX(mImpl->mResampleColorFactor);
            mImpl->mResampleColor->setFactorY(mImpl->mResampleColorFactor);

            
            //add resample child to color bypass
            mImpl->mColorBypass->addChild(mImpl->mResampleColor);
            SetLastUnit(*mImpl->mResampleColor);
         }

         mImpl->mColorBypass->addChild(GetUnitOut());
         //GetPreDepthBufferBypass()->addChild(GetUnitOut());

         //finally add the camera as a child to the main camera
         mainSceneOSGCamera->addChild(multiPassOSGCam);
         
      }
      else
      {
         LOG_ERROR("Must have a main scene camera available to setup a multipass scene.");
      }
   }


   bool MultipassScene::AddScene( SceneBase& sb)
   {
      PPUScene* ppu = dynamic_cast<PPUScene*>(&sb);
      if(ppu != NULL)
      {
         osgPPU::Processor* proc = GetPPUProcessor();
         if(proc != NULL)
         {
            if(ppu->GetAddToRootPPUScene())
            {
               proc->addChild(ppu->GetSceneNode());
            }
            else if(ppu->GetAddToMultipassOutput())
            {
               GetLastUnit()->addChild(ppu->GetSceneNode());
            }
            else
            {
               LOG_ERROR("PPU not added to scene no add method specified");
            }

            SceneGroup::GetChildArray().push_back(&sb);
            return DeltaDrawable::AddChild(&sb);
         }
         else
         {
            LOG_ERROR("Multipass scene must have a valid PPU Processor.");
         }
      }
      
      return false;
   }


   osg::Camera* MultipassScene::GetCamera()
   {
      return mImpl->mMultipassCamera.get();
   }

   const osg::Camera* MultipassScene::GetCamera() const
   {
      return mImpl->mMultipassCamera.get();
   }

   void MultipassScene::SetCamera( osg::Camera& cam)
   {
      mImpl->mMultipassCamera = &cam;
   }


   osgPPU::Processor* MultipassScene::GetPPUProcessor()
   {
      return mImpl->mPPUProcessor.get();
   }

   const osgPPU::Processor* MultipassScene::GetPPUProcessor() const
   {
      return mImpl->mPPUProcessor.get();
   }


   //! Setup the camera to do the render to texture
   osg::Texture* MultipassScene::SetupMultipassCamera(osg::Camera& camera, osg::Viewport& vp, bool use_color, bool use_depth)
   {
      osg::Texture* result = NULL;

      // set up the background color and clear mask.
      camera.setClearColor(osg::Vec4(0.0f,0.0f,0.0f,0.0f));
      int clearMask = GL_DEPTH_BUFFER_BIT;
      if (use_color)
      {
         clearMask |= GL_COLOR_BUFFER_BIT;
      }
      else
      {
         camera.setClearColor(osg::Vec4(0.0f,0.0f,1.0f,1.0f));
      }

      camera.setClearMask(clearMask);

      // set viewport
      camera.setViewport(&vp);
      camera.setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);

      camera.setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);

      if(use_depth)
      {
         osg::Texture* depth_texture = CreateRenderTexture((int)vp.width(), (int)vp.height(), true, true, GetDepthBufferImageFormat());

         if(!use_color)
         {
            camera.detach(osg::Camera::COLOR_BUFFER);
         }

         camera.attach(osg::Camera::DEPTH_BUFFER, depth_texture);//, 0, 0, false, 4, 4);
         result = depth_texture;
      }

      // attach the texture and use it as the color buffer.
      if(use_color)
      {
         osg::Texture* color_texture = CreateRenderTexture((int)vp.width(), (int)vp.height(), false, false, GetColorBufferImageFormat());
         camera.attach(osg::Camera::COLOR_BUFFER0, color_texture);//, 0, 0, false, 4, 4);
         result = color_texture;
      }

      if(!use_color && !use_depth)
      {
         LOG_ERROR("Must attach a texture either color or depth for a multipass camera.");
      }

      return result;
   }

   osg::Texture2D* MultipassScene::CreateRenderTexture(int tex_width, int tex_height, bool depth, bool nearest, int imageFormat)
   {
      // create simple 2D texture
      osg::Texture2D* texture2D = new osg::Texture2D;
      texture2D->setTextureSize(tex_width, tex_height);
      texture2D->setFilter(osg::Texture2D::MIN_FILTER, nearest ? osg::Texture2D::NEAREST : osg::Texture2D::LINEAR);
      texture2D->setFilter(osg::Texture2D::MAG_FILTER, nearest ? osg::Texture2D::NEAREST : osg::Texture2D::LINEAR);
      
      if (!depth)
      {
         texture2D->setSourceFormat(GL_RGBA);
      }
      else
      {
        texture2D->setSourceFormat(GL_DEPTH_COMPONENT);
      }

      texture2D->setSourceType(GL_FLOAT);
      texture2D->setInternalFormat(imageFormat);

      return texture2D;
   }

   void MultipassScene::SetEnableDepthBypass( bool b)
   {
      mImpl->mEnableDepthBypass = b;
   }

   bool MultipassScene::GetEnableDepthBypass() const
   {
      return mImpl->mEnableDepthBypass;
   }

   int MultipassScene::GetDepthBufferImageFormat() const
   {
      return mImpl->mDepthImageFormat;
   }

   void MultipassScene::SetDepthBufferImageFormat( int e)
   {
      mImpl->mDepthImageFormat = e;
   }

   osgPPU::UnitDepthbufferBypass* MultipassScene::GetDepthBypass()
   {
      return mImpl->mDepthBufferBypass;
   }

   const osgPPU::UnitDepthbufferBypass* MultipassScene::GetDepthBypass() const
   {
      return mImpl->mDepthBufferBypass;
   }

   void MultipassScene::SetEnableColorBypass( bool b)
   {
      mImpl->mEnableColorBypass = b;
   }

   bool MultipassScene::GetEnableColorBypass() const
   {
      return mImpl->mEnableColorBypass;
   }

   int MultipassScene::GetColorBufferImageFormat() const
   {
      return mImpl->mColorImageFormat;
   }

   void MultipassScene::SetColorBufferImageFormat( int e)
   {
      mImpl->mColorImageFormat = e;
   }

   osgPPU::UnitBypass* MultipassScene::GetColorBypass()
   {
      return mImpl->mColorBypass.get();
   }

   const osgPPU::UnitBypass* MultipassScene::GetColorBypass() const
   {
      return mImpl->mColorBypass.get();
   }

   void MultipassScene::SetEnableResampleColor( bool enable )
   {
      mImpl->mEnableResampleColor = enable;
   }

   bool MultipassScene::GetEnableResampleColor() const
   {
      return mImpl->mEnableResampleColor;
   }

   void MultipassScene::SetColorResampleFactor( float factor/*= 0.5*/ )
   {
      mImpl->mResampleColorFactor = factor;
   }

   float MultipassScene::GetColorResampleFactor() const
   {
      return mImpl->mResampleColorFactor;
   }

   osgPPU::UnitInResampleOut* MultipassScene::GetResampleColor()
   {
      return mImpl->mResampleColor.get();
   }

   const osgPPU::UnitInResampleOut* MultipassScene::GetResampleColor() const
   {
      return mImpl->mResampleColor.get();
   }

   osgPPU::UnitOut* MultipassScene::GetUnitOut()
   {
      return mImpl->mUnitOut.get();
   }

   const osgPPU::UnitOut* MultipassScene::GetUnitOut() const
   {
      return mImpl->mUnitOut.get();
   }


   osgPPU::Unit* MultipassScene::GetFirstUnit()
   {
      return mImpl->mFirstUnit.get();
   }

   const osgPPU::Unit* MultipassScene::GetFirstUnit() const
   {
      return mImpl->mFirstUnit.get();
   }

   osgPPU::Unit* MultipassScene::GetLastUnit()
   {
      return mImpl->mLastUnit.get();
   }

   const osgPPU::Unit* MultipassScene::GetLastUnit() const
   {
      return mImpl->mLastUnit.get();

   }

   void MultipassScene::SetFirstUnit( osgPPU::Unit& u)
   {
      mImpl->mFirstUnit = &u;
   }

   void MultipassScene::SetLastUnit( osgPPU::Unit& u)
   {
      mImpl->mLastUnit = &u;
   }

   int MultipassScene::GetPreDepthBufferImageFormat() const
   {
      return mImpl->mPreDepthImageFormat;
   }

   void MultipassScene::SetPreDepthBufferImageFormat( int i)
   {
      mImpl->mPreDepthImageFormat = i;
   }

   void MultipassScene::SetEnablePreDepthPass( bool b)
   {
      mImpl->mEnablePreDepthPass = b;
   }

   bool MultipassScene::GetEnablePreDepthPass() const
   {
      return mImpl->mEnablePreDepthPass;
   }

   osgPPU::UnitDepthbufferBypass* MultipassScene::GetPreDepthBufferBypass()
   {
      return mImpl->mPreDepthBufferBypass;
   }

   const osgPPU::UnitDepthbufferBypass* MultipassScene::GetPreDepthBufferBypass() const
   {
      return mImpl->mPreDepthBufferBypass;
   }

   osg::Camera* MultipassScene::GetPreDepthCamera()
   {
      return mImpl->mPreDepthSceneCamera;
   }

   const osg::Camera* MultipassScene::GetPreDepthCamera() const
   {
      return mImpl->mPreDepthSceneCamera;
   }

   osg::Texture* MultipassScene::GetPreDepthTexture()
   {
      return mImpl->mPreDepthTexture;
   }

   const osg::Texture* MultipassScene::GetPreDepthTexture() const
   {
      return mImpl->mPreDepthTexture;
   }



   ///////////////////////////////////////////////////////////
   //PROXY
   MultipassSceneProxy::MultipassSceneProxy()
   {
   }

   MultipassSceneProxy::~MultipassSceneProxy()
   {
   }

   void MultipassSceneProxy::BuildPropertyMap()
   {
      BaseClass::BuildPropertyMap();


      std::string group("MultipassScene");
      typedef dtCore::PropertyRegHelper<MultipassSceneProxy&, MultipassScene> PropRegHelperType;
      PropRegHelperType propRegHelper(*this, GetDrawable<MultipassScene>(), group);

      //DT_REGISTER_PROPERTY_WITH_NAME_AND_LABEL(MainSceneCullMask, "MainSceneCullMask", "Main Scene Cull Mask", 
      //   "The CullMask to use for the main scene when doing multipass rendering.",
      //   PropRegHelperType, propRegHelper);

      DT_REGISTER_PROPERTY_WITH_NAME_AND_LABEL(EnableColorBypass, "EnableColorBypass", "Enable Color Bypass",
         "Setting this true renders the scene using the main scene cull mask to a color buffer.",
         PropRegHelperType, propRegHelper);

      DT_REGISTER_PROPERTY_WITH_NAME_AND_LABEL(EnableResampleColor, "EnableResampleColor", "Enable Resample Color",
         "Setting this true resamples the main scene by the color resample factor.",
         PropRegHelperType, propRegHelper);

      DT_REGISTER_PROPERTY_WITH_NAME_AND_LABEL(ColorResampleFactor, "ColorResampleFactor", "Color Resample Factor",
         "A scale representing the resampled main color buffer, set Enable Resample Color to true to use this.",
         PropRegHelperType, propRegHelper);

      DT_REGISTER_PROPERTY_WITH_NAME_AND_LABEL(EnableDepthBypass, "EnableDepthBypass", "Enable Depth Bypass",
         "Setting this true saves a depth buffer with the main draw.",
         PropRegHelperType, propRegHelper);

      DT_REGISTER_PROPERTY_WITH_NAME_AND_LABEL(EnablePreDepthPass, "EnablePreDepthPass", "Enables a depth pass before rendering a view in the scene.",
         "Setting this true computes a predepth pass.",
         PropRegHelperType, propRegHelper);

      //DT_REGISTER_PROPERTY_WITH_NAME_AND_LABEL(DepthSceneCullMask, "DepthSceneCullMask", "Main Scene Cull Mask", 
      //   "The CullMask to use for the pre depth scene.",
      //   PropRegHelperType, propRegHelper);


   }

   void MultipassSceneProxy::CreateDrawable()
   {
      dtCore::RefPtr<MultipassScene> es = new MultipassScene();

      SetDrawable(*es);
   }

   bool MultipassSceneProxy::IsPlaceable() const
   {
      return false;
   }


}//namespace dtRender
