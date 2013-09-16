/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TimedItem.h"

#include "mozilla/Casting.h"
#include "mozilla/dom/TimedItemList.h"
#include "mozilla/dom/Timing.h"
#include "mozilla/dom/TimingGroup.h"

namespace mozilla {
namespace dom {

TimedItem::TimedItem(const TimingInput& aTiming)
{
  mTiming = new Timing(this, aTiming);
  mStartTime = TimeStamp::Now();
  mDuration = TimeDuration::FromSeconds(mTiming->IterationDuration());
  SetIsDOMBinding();
}

TimedItem::TimedItem(TimedItem* aOther)
{
  nsRefPtr<Timing> timing = aOther->Specified();
  mTiming = new Timing(timing);
  SetIsDOMBinding();
}

TimedItem::~TimedItem()
{
}

already_AddRefed<Timing>
TimedItem::Specified() const
{
  nsRefPtr<Timing> timing = mTiming;
  return timing.forget();
}

void
TimedItem::SetParent(TimingGroup* aParent)
{
  mGroup = aParent;
}

TimedItem*
TimedItem::GetPreviousSibling()
{
  if (!mGroup) {
    return nullptr;
  }

  const nsTArray<nsRefPtr<TimedItem> >& parentItems =
    mGroup->Children()->AsArray();
  int32_t index = parentItems.IndexOf(this);
  MOZ_ASSERT(index >= 0);
  if (index == 0) {
    return nullptr;
  }

  return parentItems[index - 1];
}

TimedItem*
TimedItem::GetNextSibling()
{
  if (!mGroup) {
    return nullptr;
  }

  const nsTArray<nsRefPtr<TimedItem> >& parentItems =
    mGroup->Children()->AsArray();
  int32_t index = parentItems.IndexOf(this);
  if (SafeCast<uint32_t>(index) == parentItems.Length() - 1) {
    return nullptr;
  }

  return parentItems[index + 1];
}

void
TimedItem::Before(const Sequence<OwningNonNull<TimedItem> >& aItems,
                  ErrorResult& rv)
{
  for (TimingGroup* group = GetParent(); group; group = group->GetParent()) {
    for (uint32_t i = 0; i < aItems.Length(); i++) {
      if (aItems[i].get() == group) {
        rv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return;
      }
    }
  }

  nsTArray<nsRefPtr<TimedItem> >& items = GetParent()->Children()->AsArray();
  int32_t index = items.IndexOf(this);
  for (int32_t i = aItems.Length(); i >= 0; i--) {
    items.InsertElementAt(index, aItems[i].get());
    aItems[i].get()->SetParent(GetParent());
  }
}

void
TimedItem::After(const Sequence<OwningNonNull<TimedItem> >& aItems,
                 ErrorResult& rv)
{
  for (TimingGroup* group = GetParent(); group; group = group->GetParent()) {
    for (uint32_t i = 0; i < aItems.Length(); i++) {
      if (aItems[i].get() == group) {
        rv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return;
      }
    }
  }

  nsTArray<nsRefPtr<TimedItem> >& items = GetParent()->Children()->AsArray();
  int32_t index = items.IndexOf(this);
  for (int32_t i = aItems.Length(); i >= 0; i--) {
    items.InsertElementAt(index + 1, aItems[i].get());
    aItems[i].get()->SetParent(GetParent());
  }
}

void
TimedItem::Replace(const Sequence<OwningNonNull<TimedItem> >& aItems,
                   ErrorResult& rv)
{
  for (TimingGroup* group = GetParent(); group; group = group->GetParent()) {
    for (uint32_t i = 0; i < aItems.Length(); i++) {
      if (aItems[i].get() == group) {
        rv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return;
      }
    }
  }

  nsTArray<nsRefPtr<TimedItem> >& items = GetParent()->Children()->AsArray();
  int32_t index = items.IndexOf(this);
  for (int32_t i = aItems.Length(); i >= 0; i--) {
    items.InsertElementAt(index, aItems[i].get());
    aItems[i].get()->SetParent(GetParent());
  }

  items.RemoveElement(this);
  SetParent(nullptr);
}

void
TimedItem::Remove()
{
  if (mGroup) {
    mGroup->Remove(this);
  }
}

} // namespace dom
} // namespace mozilla
