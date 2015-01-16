/* -*-c++-*-
 * testAPP - Using 'The MIT License'
 * Copyright (C) 2014, Caper Holdings LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

////////////////////////////////////////////////////////////////////////////////
// INCLUDE DIRECTIVES
////////////////////////////////////////////////////////////////////////////////
#include <dtCore/baseactor.h>
#include <dtCore/actorcomponent.h>
#include <dtCore/actorhierarchynode.h>



namespace dtCore
{
   /////////////////////////////////////////////////////////////////////////////
   // CLASS CODE
   /////////////////////////////////////////////////////////////////////////////
   BaseActor::BaseActor(const ActorType* actorType)
      : BaseClass(actorType)
   {}

   /////////////////////////////////////////////////////////////////////////////
   BaseActor::BaseActor(const BaseActor& actor)
   {}

   /////////////////////////////////////////////////////////////////////////////
   BaseActor::~BaseActor()
   {}

   /////////////////////////////////////////////////////////////////////////////
   BaseActor* BaseActor::GetChildByType(const dtCore::ActorType& actorType) const
   {
      ActorTypeFilter filter(actorType);
      ActorFilterFunc func(&filter, &ActorTypeFilter::IsMatch);

      return GetChild(func);
   }

   /////////////////////////////////////////////////////////////////////////////
   int BaseActor::GetChildrenByType(const dtCore::ActorType& objType, ObjArray& outObjects, bool exactMatch) const
   {
      int results = 0;

      ActorTypeFilter filter(objType);
      filter.SetExactMatchEnabled(exactMatch);
      ActorFilterFunc func(&filter, &ActorTypeFilter::IsMatch);

      return GetChildrenFiltered(outObjects, func);
   }

   /////////////////////////////////////////////////////////////////////////////
   dtCore::ActorComponent* BaseActor::GetComponentByType(const dtCore::ActorType& compType) const
   {
      return dynamic_cast<dtCore::ActorComponent*>(GetChildByType(compType));
   }

   /////////////////////////////////////////////////////////////////////////////
   int BaseActor::GetComponentsByType(const dtCore::ActorType& objType, ObjArray& outObjects, bool exactMatch) const
   {
      return GetChildrenByType(objType, outObjects, exactMatch);
   }
   
   /////////////////////////////////////////////////////////////////////////////
   int BaseActor::GetAllComponents(BaseActorArray& outComponents) const
   {
      return GetChildrenByType(*ActorComponent::BaseActorComponentType, outComponents, false);
   }

   /////////////////////////////////////////////////////////////////////////////
   int BaseActor::RemoveChildrenByType(const dtCore::ActorType& actorType, BaseActorArray* outActors, bool exactMatch)
   {
      BaseActorArray tmpActors;
      BaseActorArray* children = &tmpActors;
      if (outActors != NULL)
      {
         children = outActors;
      }

      int results = 0;
      GetChildrenByType(actorType, *children, exactMatch);

      if ( ! children->empty())
      {
         BaseActorArray::iterator curIter = children->begin();
         BaseActorArray::iterator endIter = children->end();
         for (; curIter != endIter; ++curIter)
         {
            RemoveChild(*(*curIter));
            ++results;
         }
      }

      return results;
   }

   /////////////////////////////////////////////////////////////////////////////
   void BaseActor::OnChildAdded(BaseActor& actor)
   {
      ActorComponent* actorComp = dynamic_cast<ActorComponent*>(&actor);

      if (actorComp != NULL)
      {
         actorComp->OnAddedToActor(*this);
      }
   }

   /////////////////////////////////////////////////////////////////////////////
   void BaseActor::OnChildRemoved(BaseActor& actor)
   {
      ActorComponent* actorComp = dynamic_cast<ActorComponent*>(&actor);

      if (actorComp != NULL)
      {
         actorComp->OnRemovedFromActor(*this);
      }
   }

   /////////////////////////////////////////////////////////////////////////////
   BaseActor& BaseActor::operator=(const BaseActor& rhs)
   {
      return *this;
   }

} // namespace dtCore