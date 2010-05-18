/* -*-c++-*-
 * Delta3D Open Source Game and Simulation Engine
 * Copyright (C) 2007, Alion Science and Technology
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
 * Bradley Anderegg 05/11/2007
 */

#include <dtAnim/animationcomponent.h>
#include <dtGame/basemessages.h>
#include <dtGame/defaultgroundclamper.h>
#include <dtGame/messagetype.h>
#include <dtGame/gameactorproxy.h>
#include <dtGame/gameactor.h>
#include <dtUtil/log.h>

namespace dtAnim
{


/////////////////////////////////////////////////////////////
AnimationComponent::AnimCompUpdateTask::AnimCompUpdateTask()
: mUpdateDT(0.0)
{
}

/////////////////////////////////////////////////////////////
void AnimationComponent::AnimCompUpdateTask::operator()()
{
   for (unsigned i = 0; i < mAnimActorComps.size(); ++i)
   {
      mAnimActorComps[i]->Update(mUpdateDT);
   }
}


/////////////////////////////////////////////////////////////
const std::string AnimationComponent::DEFAULT_NAME("Animation Component");

/////////////////////////////////////////////////////////////////////////////////
AnimationComponent::AnimationComponent(const std::string& name)
   : BaseClass(name)
   , mRegisteredActors()
   , mGroundClamper(new dtGame::DefaultGroundClamper)
{
   mGroundClamper->SetHighResGroundClampingRange(0.01);
   mGroundClamper->SetLowResGroundClampingRange(0.1);
}

/////////////////////////////////////////////////////////////////////////////////
AnimationComponent::~AnimationComponent()
{
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationComponent::ProcessMessage(const dtGame::Message& message)
{
   if (message.GetMessageType() == dtGame::MessageType::TICK_LOCAL)
   {
      const dtGame::TickMessage& mess = static_cast<const dtGame::TickMessage&>(message);
      TickLocal(mess.GetDeltaSimTime());
   }
   else if (message.GetMessageType() == dtGame::MessageType::INFO_ACTOR_DELETED)
   {
      // TODO Write unit tests for the delete message.
      UnregisterActor(message.GetAboutActorId());

      if (GetTerrainActor() != NULL && GetTerrainActor()->GetUniqueId() == message.GetAboutActorId())
      {
         SetTerrainActor(NULL);
      }
   }
   else if (message.GetMessageType() == dtGame::MessageType::INFO_MAP_UNLOADED)
   {
      SetTerrainActor(NULL);
      mRegisteredActors.clear();
   }
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationComponent::BuildThreadWorkerTasks()
{
   unsigned threads = dtUtil::ThreadPool::GetNumImmediateWorkerThreads();


   for (unsigned i = 0; i < threads; ++i)
   {
      mThreadTasks.push_back(new AnimCompUpdateTask);
      mThreadTasks.back()->mAnimActorComps.reserve(mRegisteredActors.size() + 1);
   }

   AnimCompIter iter = mRegisteredActors.begin();
   AnimCompIter end = mRegisteredActors.end();

   while (iter != end)
   {
      for (unsigned i = 0; i < threads && iter != end; ++i)
      {
         AnimCompMapping& current = *iter;
         mThreadTasks[i]->mAnimActorComps.push_back(current.second.mAnimActorComp);
         ++iter;
      }
   }
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationComponent::TickLocal(float dt)
{
   if (mThreadTasks.empty())
   {
      BuildThreadWorkerTasks();
   }

   for (unsigned i = 0; i < mThreadTasks.size(); ++i)
   {
      if (!mThreadTasks[i]->mAnimActorComps.empty())
      {
         mThreadTasks[i]->mUpdateDT = dt;
         dtUtil::ThreadPool::AddTask(*mThreadTasks[i]);
      }
   }

   GroundClamp();

   dtUtil::ThreadPool::ExecuteTasks();
}

/////////////////////////////////////////////////////////////////////////////////
const dtAnim::AnimationHelper* AnimationComponent::GetHelperForProxy(dtGame::GameActorProxy& proxy) const
{
   const AnimCompMap::const_iterator iter = mRegisteredActors.find(proxy.GetId());
   if (iter == mRegisteredActors.end())
   {
      return NULL;
   }

   return (*iter).second.mAnimActorComp.get();
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationComponent::RegisterActor(dtGame::GameActorProxy& proxy, dtAnim::AnimationHelper& helper)
{
   AnimCompData data;
   data.mAnimActorComp = &helper;
   //if the insert fails, log a message.
   if (!mRegisteredActors.insert(AnimCompMapping(proxy.GetId(), data)).second)
   {
      LOG_ERROR("GameActor already registered with Animation Component.");
   }
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationComponent::UnregisterActor(dtGame::GameActorProxy& proxy)
{
   UnregisterActor(proxy.GetId());
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationComponent::UnregisterActor(const dtCore::UniqueId& actorId)
{
   AnimCompIter iter = mRegisteredActors.find(actorId);
   if (iter != mRegisteredActors.end())
   {
      mRegisteredActors.erase(iter);
      mThreadTasks.clear();
   }
}

/////////////////////////////////////////////////////////////////////////////////
bool AnimationComponent::IsRegisteredActor(dtGame::GameActorProxy& proxy)
{
   AnimCompIter iter = mRegisteredActors.find(proxy.GetId());
   return iter != mRegisteredActors.end();
}

/////////////////////////////////////////////////////////////////////////////////
dtCore::Transformable* AnimationComponent::GetTerrainActor()
{
   return mGroundClamper->GetTerrainActor();
}

/////////////////////////////////////////////////////////////////////////////////
const dtCore::Transformable* AnimationComponent::GetTerrainActor() const
{
   return mGroundClamper->GetTerrainActor();
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationComponent::SetTerrainActor(dtCore::Transformable* newTerrain)
{
   mGroundClamper->SetTerrainActor(newTerrain);
}

//////////////////////////////////////////////////////////////////////
dtCore::Transformable* AnimationComponent::GetEyePointActor()
{
   return mGroundClamper->GetEyePointActor();
}

//////////////////////////////////////////////////////////////////////
const dtCore::Transformable* AnimationComponent::GetEyePointActor() const
{
   return mGroundClamper->GetEyePointActor();
}

//////////////////////////////////////////////////////////////////////
void AnimationComponent::SetEyePointActor(dtCore::Transformable* newEyePointActor)
{
   mGroundClamper->SetEyePointActor(newEyePointActor);
}

//////////////////////////////////////////////////////////////////////
void AnimationComponent::SetGroundClamper(dtGame::BaseGroundClamper& clamper)
{
   mGroundClamper = &clamper;
}

//////////////////////////////////////////////////////////////////////
dtGame::BaseGroundClamper& AnimationComponent::GetGroundClamper()
{
   return *mGroundClamper;
}

//////////////////////////////////////////////////////////////////////
const dtGame::BaseGroundClamper& AnimationComponent::GetGroundClamper() const
{
   return *mGroundClamper;
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationComponent::GroundClamp()
{
   if (mGroundClamper->GetTerrainActor() != NULL)
   {
      dtGame::GameManager* gm = GetGameManager();

      AnimCompIter end = mRegisteredActors.end();
      AnimCompIter iter = mRegisteredActors.begin();

      while (iter != end)
      {
         dtGame::GroundClampingData gcData;
         gcData.SetAdjustRotationToGround(false);
         gcData.SetUseModelDimensions(false);

         for (; iter != end; ++iter)
         {
            dtGame::GameActorProxy* pProxy = gm->FindGameActorById((*iter).first);
            if (pProxy != NULL)
            {
               dtCore::Transform xform;
               pProxy->GetGameActor().GetTransform(xform, dtCore::Transformable::REL_CS);

               mGroundClamper->ClampToGround(dtGame::BaseGroundClamper::GroundClampingType::RANGED,
                        0.0, xform, *pProxy, gcData, true);
            } // if
         } // for

         mGroundClamper->FinishUp();
      } // while
   } // if
}


} // namespace dtGame
