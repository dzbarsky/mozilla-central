/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TimingGroup.h"

#include "mozilla/dom/TimedItemList.h"

namespace mozilla {
namespace dom {

TimingGroup::TimingGroup(nsISupports* aOwner,
                         const Nullable<Sequence<OwningNonNull<TimedItem> > >& aChildren,
                         const TimingInput& aTiming)
  : TimedItem(aTiming)
  , mOwner(aOwner)
{
  mChildren = new TimedItemList(mOwner);
  if (!aChildren.IsNull()) {
    const Sequence<OwningNonNull<TimedItem> >& items = aChildren.Value();
    for (uint32_t i = 0; i < items.Length(); i++) {
      mChildren->AppendElement(items[i].get());
    }
  }
}

TimingGroup::TimingGroup(TimingGroup* aOther)
  : TimedItem(aOther)
  , mOwner(aOther->mOwner)
{
  mChildren = new TimedItemList(mOwner);
  for (uint32_t i = 0; i < aOther->mChildren->Length(); i++) {
    mChildren->AppendElement(aOther->mChildren->Item(i)->CloneInternal());
  }
}

TimingGroup::~TimingGroup()
{
}

TimedItem*
TimingGroup::GetFirstChild()
{
  return mChildren->Length() == 0 ? nullptr : mChildren->Item(0);
}

TimedItem*
TimingGroup::GetLastChild()
{
  return mChildren->Length() == 0 ? nullptr :
    mChildren->Item(mChildren->Length() - 1);
}

void
TimingGroup::Remove(TimedItem* aItem)
{
  mChildren->AsArray().RemoveElement(aItem);
}

void
TimingGroup::Prepend(const Sequence<OwningNonNull<TimedItem> >& aItems)
{
  for (int32_t i = aItems.Length(); i >= 0; i--) {
    TimedItem* item = aItems[i].get();
    TimingGroup* oldParent = item->GetParent();
    if (oldParent) {
      oldParent->Remove(item);
    }
    item->SetParent(this);
    mChildren->AsArray().InsertElementAt(0, item);
  }
}

void
TimingGroup::Append(const Sequence<OwningNonNull<TimedItem> >& aItems)
{
  for (uint32_t i = 0; i < aItems.Length(); i++) {
    TimedItem* item = aItems[i].get();
    TimingGroup* oldParent = item->GetParent();
    if (oldParent) {
      oldParent->Remove(item);
    }
    item->SetParent(this);
    mChildren->AppendElement(item);
  }
}

} // namespace dom
} // namespace mozilla
