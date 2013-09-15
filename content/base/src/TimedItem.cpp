/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TimedItem.h"

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
TimedItem::SetParent(TimingGroup& aParent)
{
  mGroup = &aParent;
}

} // namespace dom
} // namespace mozilla
