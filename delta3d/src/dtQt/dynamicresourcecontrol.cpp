/* -*-c++-*-
 * Delta3D Simulation Training And Game Editor (STAGE)
 * STAGE - This source file (.h & .cpp) - Using 'The MIT License'
 * Copyright (C) 2005-2008, Alion Science and Technology Corporation
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
 *
 * This software was developed by Alion Science and Technology Corporation under
 * circumstances in which the U. S. Government may have rights in the software.
 *
 * Curtiss Murphy
 */
#include <prefix/dtqtprefix.h>
#include <dtQt/dynamicresourcecontrol.h>

#include <dtDAL/actorproxy.h>
#include <dtDAL/datatype.h>
#include <dtDAL/project.h>
#include <dtDAL/resourcedescriptor.h>
#include <dtDAL/resourceactorproperty.h>

#include <dtQt/dynamiclabelcontrol.h>
#include <dtQt/dynamicsubwidgets.h>
#include <dtQt/propertyeditortreeview.h>

#include <dtUtil/log.h>

#include <QtCore/QRect>
#include <QtCore/QSize>


#include <QtGui/QColor>
#include <QtGui/QFocusFrame>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QPalette>
#include <QtGui/QPushButton>
#include <QtGui/QWidget>

namespace dtQt
{

   ///////////////////////////////////////////////////////////////////////////////
   DynamicResourceControl::DynamicResourceControl()
      : mTemporaryComboBox(NULL)
   {
   }

   /////////////////////////////////////////////////////////////////////////////////
   DynamicResourceControl::~DynamicResourceControl()
   {
   }

   /////////////////////////////////////////////////////////////////////////////////
   void DynamicResourceControl::InitializeData(DynamicAbstractControl* newParent,
      PropertyEditorModel* newModel, dtDAL::PropertyContainer* newPC, dtDAL::ActorProperty* newProperty)
   {
      // Note - We used to have dynamic_cast in here, but it was failing to properly cast in
      // all cases in Linux with gcc4.  So we replaced it with a static cast.
      if (newProperty != NULL && newProperty->GetDataType().IsResource())
      {
         mProperty = static_cast<dtDAL::ResourceActorProperty*>(newProperty);
         DynamicAbstractControl::InitializeData(newParent, newModel, newPC, newProperty);
      }
      else
      {
         std::string propertyName = (newProperty != NULL) ? newProperty->GetName() : "NULL";
         std::string propType     = (newProperty != NULL) ? newProperty->GetDataType().GetName() : "NULL";
         std::string isResource   = (newProperty != NULL) ?
            (newProperty->GetDataType().IsResource() ? "IsResource" : "NotAResource") : "NULL";
         LOG_ERROR("Cannot create dynamic control because property [" +
            propertyName + "] is not the correct type[" + propType + "], [" +
            isResource + "].");
      }
   }

   /////////////////////////////////////////////////////////////////////////////////
   const QString DynamicResourceControl::getDisplayName()
   {
      return QString(tr(mProperty->GetLabel().c_str()));
   }

   /////////////////////////////////////////////////////////////////////////////////
   const QString DynamicResourceControl::getDescription()
   {
      std::string tooltip = mProperty->GetDescription() + " - To assign a resource, select a [" +
         mProperty->GetDataType().GetName() + "] resource in the Resource Browser and press Use Current";
      return QString(tr(tooltip.c_str()));
   }

   /////////////////////////////////////////////////////////////////////////////////
   const QString DynamicResourceControl::getValueAsString()
   {
      DynamicAbstractControl::getValueAsString();

      // if we have no current resource, show special text that indicates the type
      dtDAL::ResourceDescriptor resource = mProperty->GetValue();
      QString resourceTag;
      if (resource.IsEmpty())
      {
         resourceTag = QString(tr("<None>"));
      }
      else
      {
         resourceTag = QString(tr(resource.GetDisplayName().c_str()));
      }

      return resourceTag;
   }


   /////////////////////////////////////////////////////////////////////////////////
   void DynamicResourceControl::updateEditorFromModel(QWidget* widget)
   {
      // update our label
      if (widget == mWrapper && mTemporaryComboBox)
      {
         mTemporaryComboBox->setCurrentIndex(mTemporaryComboBox->findText(getValueAsString()));
      }

      DynamicAbstractControl::updateEditorFromModel(widget);
   }

   /////////////////////////////////////////////////////////////////////////////////
   bool DynamicResourceControl::updateModelFromEditor(QWidget* widget)
   {
      DynamicAbstractControl::updateModelFromEditor(widget);

      if (widget == mWrapper && mTemporaryComboBox)
      {
         std::string result = mTemporaryComboBox->itemData(
            mTemporaryComboBox->currentIndex()).toString().toStdString();

         // set our value to our object
         if (result != mProperty->GetValue().GetResourceIdentifier())
         {
            // give undo manager the ability to create undo/redo events
            emit PropertyAboutToChange(*mPropContainer, *mProperty,
               mProperty->ToString(), result);

            mProperty->FromString(result);

            emit PropertyChanged(*mPropContainer, *mProperty);

            return true;
         }
      }

      return false;
   }

   /////////////////////////////////////////////////////////////////////////////////
   QWidget *DynamicResourceControl::createEditor(QWidget* parent,
      const QStyleOptionViewItem& option, const QModelIndex& index)
   {
      QWidget* wrapper = DynamicAbstractControl::createEditor(parent, option, index);

      if (!mInitialized)
      {
         LOG_ERROR("Tried to add itself to the parent widget before being initialized");
         return wrapper;
      }

      mTemporaryComboBox = new SubQComboBox(wrapper, this);
      mTemporaryComboBox->setToolTip(getDescription());

      dtUtil::tree<dtDAL::ResourceTreeNode> tree;
      dtDAL::Project::GetInstance().GetResourcesOfType(mProperty->GetDataType(), tree);

      setupList(tree);

      mTemporaryComboBox->setEditable(false);

      updateEditorFromModel(mWrapper);

      mGridLayout->addWidget(mTemporaryComboBox, 0, 0, 1, 1);
      mGridLayout->setColumnMinimumWidth(0, mTemporaryComboBox->sizeHint().width() / 2);
      mGridLayout->setColumnStretch(0, 1);

      connect(mTemporaryComboBox, SIGNAL(activated(int)), this, SLOT(itemSelected(int)));

      wrapper->setFocusProxy(mTemporaryComboBox);
      return wrapper;
   }

   //////////////////////////////////////////////////////////////////////////
   void DynamicResourceControl::setupList(const dtUtil::tree<dtDAL::ResourceTreeNode>& tree)
   {
      mTemporaryComboBox->addItem("<None>");

      dtUtil::tree<dtDAL::ResourceTreeNode>::const_iterator& iter = tree.begin();
      for (iter = tree.begin(); iter != tree.end(); ++iter)
      {
         mTemporaryComboBox->addItem(iter->getResource().GetDisplayName().c_str(),
            QVariant(iter->getResource().GetResourceIdentifier().c_str()));
      }
   }

   /////////////////////////////////////////////////////////////////////////////////
   bool DynamicResourceControl::isEditable()
   {
      return !mProperty->IsReadOnly();
   }

   /////////////////////////////////////////////////////////////////////////////////
   dtDAL::ResourceActorProperty& DynamicResourceControl::GetProperty()
   {
      return *mProperty;
   }

   /////////////////////////////////////////////////////////////////////////////////
   // SLOTS
   /////////////////////////////////////////////////////////////////////////////////

   /////////////////////////////////////////////////////////////////////////////////
   bool DynamicResourceControl::updateData(QWidget* widget)
   {
      // this guy doesn't have any editors.  All the data is edited in child controls
      return false;
   }

   /////////////////////////////////////////////////////////////////////////////////
   void DynamicResourceControl::itemSelected(int index)
   {
      if (mTemporaryComboBox != NULL)
      {
         updateModelFromEditor(mWrapper);
      }
   }

   /////////////////////////////////////////////////////////////////////////////////
   void DynamicResourceControl::handleSubEditDestroy(QWidget* widget, QAbstractItemDelegate::EndEditHint hint)
   {
      if (widget == mWrapper)
      {
         mTemporaryComboBox = NULL;
      }

      DynamicAbstractControl::handleSubEditDestroy(widget, hint);
   }
} // namespace dtEditQt