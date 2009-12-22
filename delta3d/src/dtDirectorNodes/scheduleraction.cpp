/*
 * Delta3D Open Source Game and Simulation Engine
 * Copyright (C) 2008 MOVES Institute
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
 * Author: Jeff P. Houde
 */

#include <dtDirectorNodes/scheduleraction.h>

#include <dtDirector/director.h>

#include <dtDAL/enginepropertytypes.h>
#include <dtDAL/containeractorproperty.h>
#include <dtDAL/arrayactorproperty.h>


namespace dtDirector
{
   ////////////////////////////////////////////////////////////////////////////////
   SchedulerAction::SchedulerAction()
      : ActionNode()
      , mIsActive(false)
      , mEventIndex(-1)
      , mElapsedTime(0.0f)
      , mTotalTime(0.0f)
   {
      AddAuthor("Jeff P. Houde");
   }

   ////////////////////////////////////////////////////////////////////////////////
   SchedulerAction::~SchedulerAction()
   {
   }

   ////////////////////////////////////////////////////////////////////////////////
   void SchedulerAction::Init(const NodeType& nodeType, DirectorGraph* graph)
   {
      ActionNode::Init(nodeType, graph);

      // Create multiple inputs for different operations.
      mInputs.clear();
      mInputs.push_back(InputLink(this, "Play"));
      mInputs.push_back(InputLink(this, "Reverse"));
      mInputs.push_back(InputLink(this, "Stop"));
      mInputs.push_back(InputLink(this, "Pause"));

      mOutputs.clear();
   }

   ////////////////////////////////////////////////////////////////////////////////
   void SchedulerAction::BuildPropertyMap()
   {
      ActionNode::BuildPropertyMap();

      // Create our value links.
      dtDAL::FloatActorProperty* timeProp = new dtDAL::FloatActorProperty(
         "Time", "Time",
         dtDAL::FloatActorProperty::SetFuncType(this, &SchedulerAction::SetTime),
         dtDAL::FloatActorProperty::GetFuncType(this, &SchedulerAction::GetTime),
         "The current time (in seconds).");

      dtDAL::BooleanActorProperty* simTimeProp = new dtDAL::BooleanActorProperty(
         "UseSimTime", "Use Sim Time",
         dtDAL::BooleanActorProperty::SetFuncType(this, &SchedulerAction::SetUseSimTime),
         dtDAL::BooleanActorProperty::GetFuncType(this, &SchedulerAction::GetUseSimTime),
         "True to use game/sim time, false to use real time.");
      AddProperty(simTimeProp);

      dtDAL::ContainerActorProperty* eventGroupProp = new dtDAL::ContainerActorProperty(
         "OutputEventData", "Output Event Data", "This is an output event.", "");

      dtDAL::StringActorProperty* eventNameProp = new dtDAL::StringActorProperty(
         "EventName", "Event Name",
         dtDAL::StringActorProperty::SetFuncType(this, &SchedulerAction::SetEventName),
         dtDAL::StringActorProperty::GetFuncType(this, &SchedulerAction::GetEventName),
         "The name of the output to fire.");
      eventGroupProp->AddProperty(eventNameProp);

      dtDAL::FloatActorProperty* eventTimeProp = new dtDAL::FloatActorProperty(
         "EventTime", "Event Time",
         dtDAL::FloatActorProperty::SetFuncType(this, &SchedulerAction::SetEventTime),
         dtDAL::FloatActorProperty::GetFuncType(this, &SchedulerAction::GetEventTime),
         "The time that the output will fire.");
      eventGroupProp->AddProperty(eventTimeProp);

      dtDAL::ArrayActorPropertyBase* eventListProp = new dtDAL::ArrayActorProperty<OutputEventData>(
         "EventList", "Event List", "The list of events.",
         dtDAL::MakeFunctor(*this, &SchedulerAction::SetEventIndex),
         dtDAL::MakeFunctorRet(*this, &SchedulerAction::GetDefaultEvent),
         dtDAL::MakeFunctorRet(*this, &SchedulerAction::GetEventList),
         dtDAL::MakeFunctor(*this, &SchedulerAction::SetEventList),
         eventGroupProp, "");
      AddProperty(eventListProp);

      // This will expose the properties in the editor and allow
      // them to be connected to ValueNodes.
      mValues.push_back(ValueLink(this, timeProp, true, true, false));
   }

   //////////////////////////////////////////////////////////////////////////
   bool SchedulerAction::Update(float simDelta, float delta, int input, bool firstUpdate)
   {
      bool result = false;

      float elapsedTime = simDelta;
      if (!mUseSimTime) elapsedTime = delta;

      switch (input)
      {
      case INPUT_PLAY:
      case INPUT_REVERSE:
         {
            // We need to check the current active status because this node
            // will be updated multiple times using this same index until
            // the desired time has elapsed.  And we only want to trigger
            // the "Out" output once at the beginning.
            if (!mIsActive)
            {
               if (firstUpdate)
               {
                  mIsActive = true;

                  if (input == INPUT_PLAY)
                  {
                     // If we are playing from the beginning and our
                     // elapsed time is at the end, reset it back
                     // to the start.
                     if (mElapsedTime >= mTotalTime)
                     {
                        mElapsedTime = 0.0f;
                     }
                  }
                  else if (input == INPUT_REVERSE)
                  {
                     // If we are playing in reverse and our elapsed time
                     // is at the beginning, flip it to start at the end.
                     if (mElapsedTime <= 0.0f)
                     {
                        mElapsedTime = mTotalTime;
                     }
                  }

                  result = true;
               }
               // If this is not the first update for this node, then
               // we must be paused or stopped, so we want to stop this update.
               else
               {
                  return false;
               }
            }

            float start = 0.0f;
            float end = 0.0f;

            if (input == INPUT_PLAY)
            {
               start = mElapsedTime;
               mElapsedTime += elapsedTime;
               end = mElapsedTime;
            }
            else
            {
               end = mElapsedTime;
               mElapsedTime -= elapsedTime;
               start = mElapsedTime;
            }

            // Trigger any events that are between the current time and the
            // time delta.
            TestEvents(start, end);

            // Test if the desired time has elapsed.
            result = true;
            if ((input == INPUT_PLAY && mElapsedTime >= mTotalTime) ||
               (input == INPUT_REVERSE && mElapsedTime <= 0))
            {
               mIsActive = false;

               // Clamp the time to the bounds of the track.
               if (mElapsedTime < 0.0f) mElapsedTime = 0.0f;
               else if (mElapsedTime > mTotalTime) mElapsedTime = mTotalTime;

               // Return false so this node does not remain active.
               result = false;
            }

            SetFloat(mElapsedTime, "Time");
         }
         break;

      case INPUT_STOP:
         // Reset the elapsed time and deactivate it.
         if (mIsActive)
         {
            mElapsedTime = 0.0f;
            SetFloat(mElapsedTime, "Time");
            mIsActive = false;
         }
         break;

      case INPUT_PAUSE:
         // Deactivate the node, but do not reset the timer.
         mIsActive = false;
         break;
      }

      return result;
   }

   //////////////////////////////////////////////////////////////////////////
   bool SchedulerAction::CanConnectValue(ValueLink* link, ValueNode* value)
   {
      if (Node::CanConnectValue(link, value))
      {
         // Delay link can only connect to basic types.
         if (link == GetValueLink("Time"))
         {
            dtDAL::DataType& type = value->GetPropertyType();
            switch (type.GetTypeId())
            {
            case dtDAL::DataType::FLOAT_ID:
            case dtDAL::DataType::DOUBLE_ID:
               return true;

            default:
               return false;
            }
         }

         return true;
      }

      return false;
   }

   //////////////////////////////////////////////////////////////////////////
   void SchedulerAction::SetTime(float value)
   {
      mElapsedTime = value;
   }

   //////////////////////////////////////////////////////////////////////////
   float SchedulerAction::GetTime()
   {
      return mElapsedTime;
   }

   //////////////////////////////////////////////////////////////////////////
   void SchedulerAction::SetUseSimTime(bool value)
   {
      mUseSimTime = value;
   }

   //////////////////////////////////////////////////////////////////////////
   bool SchedulerAction::GetUseSimTime()
   {
      return mUseSimTime;
   }

   ////////////////////////////////////////////////////////////////////////////////
   void SchedulerAction::SetEventName(const std::string& value)
   {
      if (mEventIndex >= 0 && mEventIndex < (int)mEventList.size())
      {
         OutputEventData& data = mEventList[mEventIndex];
         data.name = value;
         UpdateOutputs();
      }
   }

   ////////////////////////////////////////////////////////////////////////////////
   std::string SchedulerAction::GetEventName()
   {
      if (mEventIndex >= 0 && mEventIndex < (int)mEventList.size())
      {
         OutputEventData& data = mEventList[mEventIndex];
         return data.name;
      }

      return "";
   }

   ////////////////////////////////////////////////////////////////////////////////
   void SchedulerAction::SetEventTime(float value)
   {
      if (mEventIndex >= 0 && mEventIndex < (int)mEventList.size())
      {
         OutputEventData& data = mEventList[mEventIndex];
         data.time = value;
         UpdateOutputs();
      }
   }

   ////////////////////////////////////////////////////////////////////////////////
   float SchedulerAction::GetEventTime()
   {
      if (mEventIndex >= 0 && mEventIndex < (int)mEventList.size())
      {
         OutputEventData& data = mEventList[mEventIndex];
         return data.time;
      }

      return 0.0f;
   }

   ////////////////////////////////////////////////////////////////////////////////
   void SchedulerAction::SetEventIndex(int index)
   {
      mEventIndex = index;
   }

   ////////////////////////////////////////////////////////////////////////////////
   SchedulerAction::OutputEventData SchedulerAction::GetDefaultEvent()
   {
      OutputEventData data;
      data.name = "";
      data.time = 0.0f;
      return data;
   }

   ////////////////////////////////////////////////////////////////////////////////
   std::vector<SchedulerAction::OutputEventData> SchedulerAction::GetEventList()
   {
      return mEventList;
   }

   ////////////////////////////////////////////////////////////////////////////////
   void SchedulerAction::SetEventList(const std::vector<OutputEventData>& value)
   {
      mEventList = value;
      UpdateOutputs();
   }

   ////////////////////////////////////////////////////////////////////////////////
   void SchedulerAction::UpdateOutputs()
   {
      mTotalTime = 0.0f;

      std::vector<OutputLink> outputs = mOutputs;
      mOutputs.clear();

      int count = (int)mEventList.size();
      for (int index = 0; index < count; index++)
      {
         OutputEventData& data = mEventList[index];
         if (data.name.empty()) continue;

         // Create a new output link if the current name does not exist.
         OutputLink* link = GetOutputLink(data.name);
         if (!link)
         {
            bool found = false;
            int linkCount = (int)outputs.size();
            for (int linkIndex = 0; linkIndex < linkCount; linkIndex++)
            {
               if (outputs[linkIndex].GetName() == data.name)
               {
                  found = true;
                  mOutputs.push_back(outputs[linkIndex]);
                  break;
               }
            }

            if (!found) mOutputs.push_back(OutputLink(this, data.name));
         }

         // Update the total time
         if (mTotalTime < data.time)
         {
            mTotalTime = data.time;
         }
      }
   }

   ////////////////////////////////////////////////////////////////////////////////
   void SchedulerAction::TestEvents(float start, float end)
   {
      int count = (int)mEventList.size();
      for (int index = 0; index < count; index++)
      {
         OutputEventData& data = mEventList[index];
         if (data.name.empty()) continue;

         if (data.time >= start && (
            data.time < end ||
            (data.time == end && end == mTotalTime)))
         {
            OutputLink* link = GetOutputLink(data.name);
            if (link) link->Activate();
         }
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
