/*
 * Copyright (C) 2017 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <sdf/sdf.hh>

#include <ignition/math/Helpers.hh>

#include "gazebo/common/Assert.hh"
#include "gazebo/rendering/ogre_gazebo.h"
#include "gazebo/rendering/Camera.hh"
#include "gazebo/rendering/Conversions.hh"
#include "gazebo/rendering/Light.hh"
#include "gazebo/rendering/Scene.hh"
#include "gazebo/rendering/LensFlare.hh"

namespace gazebo
{
  namespace rendering
  {
    // We'll create an instance of this class for each camera, to be used to
    // inject time each render call.
    class LensFlareCompositorListener
      : public Ogre::CompositorInstance::Listener
    {
      /// \brief Constructor
      public: LensFlareCompositorListener(CameraPtr _camera, LightPtr _light)
      {
        this->camera = _camera;
        this->light = _light;

        this->dir = ignition::math::Quaterniond(this->light->Rotation()) *
            this->light->Direction();
      }
  
      /// \brief Callback that OGRE will invoke for us on each render call
      /// \param[in] _passID OGRE material pass ID.
      /// \param[in] _mat Pointer to OGRE material.
      public: virtual void notifyMaterialRender(unsigned int _passId,
                                                Ogre::MaterialPtr &_mat)
      {
        GZ_ASSERT(!_mat.isNull(), "Null OGRE material");
        // These calls are setting parameters that are declared in two places:
        // 1. media/materials/scripts/gazebo.material, in
        //    fragment_program Gazebo/CameraLensFlareFS
        // 2. media/materials/scripts/camera_lens_flare_fs.glsl
        Ogre::Technique *technique = _mat->getTechnique(0);
        GZ_ASSERT(technique, "Null OGRE material technique");
        Ogre::Pass *pass = technique->getPass(_passId);
        GZ_ASSERT(pass, "Null OGRE material pass");
        Ogre::GpuProgramParametersSharedPtr params =
            pass->getFragmentProgramParameters();
        GZ_ASSERT(!params.isNull(), "Null OGRE material GPU parameters");
  
        // used for animating flare
        params->setNamedConstant("time", static_cast<Ogre::Real>(
            common::Time::GetWallTime().Double()));
        // for adjust aspect ratio of glare
        params->setNamedConstant("viewport", 
            Ogre::Vector3(static_cast<double>(this->camera->ViewportWidth()),
            static_cast<double>(this->camera->ViewportHeight()), 1.0));
        
        params->setNamedConstant("lightDir", 
            Ogre::Vector3(this->dir.X(), this->dir.Y(), this->dir.Z()));

        auto viewProj = this->camera->OgreCamera()->getProjectionMatrix() *
          this->camera->OgreCamera()->getViewMatrix();
        params->setNamedConstant("viewProj", viewProj);
      }

      /// \brief Pointer to camera
      private: CameraPtr camera;

      /// \brief Pointer to light source 
      private: LightPtr light; 

      /// \brief Light dir in world frame
      private: ignition::math::Vector3d dir;
    };

    /// \brief Private data class for LensFlare
    class LensFlarePrivate
    {
      /// \brief Pointer to ogre lens flare material
      public: Ogre::MaterialPtr lensFlareMaterial; 

      /// \brief Pointer to ogre lens flare compositor instance
      public: Ogre::CompositorInstance *lensFlareInstance;

      /// \brief Pointer to ogre lens flare compositor listener 
      public: std::shared_ptr<LensFlareCompositorListener>
          lensFlareCompositorListener;

      /// \brief Connection for the pre render event.
      public: event::ConnectionPtr preRenderConnection;

      /// \brief Pointer to camera
      public: CameraPtr camera;
    };
  }
}

using namespace gazebo;
using namespace rendering;

//////////////////////////////////////////////////
LensFlare::LensFlare()
  : dataPtr(new LensFlarePrivate)
{
}

//////////////////////////////////////////////////
LensFlare::~LensFlare()
{
}

//////////////////////////////////////////////////
void LensFlare::SetCamera(CameraPtr _camera)
{
  if (!_camera)
  {
    gzerr << "Unable to apply lens flare, camera is NULL" << std::endl;
    return;
  }

  this->dataPtr->camera = _camera;
  this->dataPtr->preRenderConnection = event::Events::ConnectPreRender(
      std::bind(&LensFlare::Update, this));
}

//////////////////////////////////////////////////
void LensFlare::Update()
{
  // Get the first directional light
  LightPtr directionalLight;
  for (unsigned int i = 0; i < this->dataPtr->camera->GetScene()->LightCount();
      ++i)
  {
    LightPtr light = this->dataPtr->camera->GetScene()->GetLight(i);
    if (light->Type() == "directional")
    {
      directionalLight = light;
      break;
    }
  }
  if (!directionalLight)
    return;

  // set up the lens flare instance
  this->dataPtr->lensFlareMaterial =
      Ogre::MaterialManager::getSingleton().getByName(
          "Gazebo/CameraLensFlare");
  this->dataPtr->lensFlareMaterial =
      this->dataPtr->lensFlareMaterial->clone(
          "Gazebo/" + this->dataPtr->camera->Name() + "_CameraLensFlare");

  this->dataPtr->lensFlareCompositorListener.reset(new
        LensFlareCompositorListener(this->dataPtr->camera, directionalLight));

  this->dataPtr->lensFlareInstance =
      Ogre::CompositorManager::getSingleton().addCompositor(
      this->dataPtr->camera->OgreViewport(), "CameraLensFlare/Default");
  this->dataPtr->lensFlareInstance->getTechnique()->getOutputTargetPass()->
      getPass(0)->setMaterial(this->dataPtr->lensFlareMaterial);

  this->dataPtr->lensFlareInstance->setEnabled(true);
  this->dataPtr->lensFlareInstance->addListener(
      this->dataPtr->lensFlareCompositorListener.get());

  // disconnect
  this->dataPtr->preRenderConnection.reset();
}
