/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Timing.h"

#include "mozilla/dom/TimingBinding.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {
namespace dom {

JSObject*
Timing::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TimingBinding::Wrap(aCx, aScope, this);
}

Timing::~Timing()
{
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(Timing, mTimedItem)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Timing, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Timing, Release)

Timing::Timing(TimedItem* aTimedItem, const TimingInput& aTiming)
  : mTimedItem(aTimedItem)
  , mStartDelay(aTiming.mStartDelay)
  , mFillMode(aTiming.mFillMode)
  , mIterationStart(aTiming.mIterationStart)
  , mIterationCount(aTiming.mIterationCount)
  , mIterationDuration(aTiming.mIterationDuration)
  , mActiveDuration(aTiming.mActiveDuration)
  , mPlaybackRate(aTiming.mPlaybackRate)
  , mDirection(aTiming.mDirection)
{
  SetIsDOMBinding();
  // XXXdz support other timing functions
  mFunction.AssignLiteral("linear");
}

Timing::Timing(Timing* aOther)
  : mTimedItem(aOther->mTimedItem)
  , mStartDelay(aOther->mStartDelay)
  , mFillMode(aOther->mFillMode)
  , mIterationStart(aOther->mIterationStart)
  , mIterationCount(aOther->mIterationCount)
  , mIterationDuration(aOther->mIterationDuration)
  , mActiveDuration(aOther->mActiveDuration)
  , mPlaybackRate(aOther->mPlaybackRate)
  , mDirection(aOther->mDirection)
{
  SetIsDOMBinding();
  // XXXdz support other timing functions
  mFunction.AssignLiteral("linear");
}

void
Timing::GetIterationDuration(UnrestrictedDoubleOrStringReturnValue& aDuration) const
{
  aDuration.SetAsUnrestrictedDouble() = mIterationDuration;
}

void
Timing::SetIterationDuration(const UnrestrictedDoubleOrString& aDuration)
{
  mIterationDuration = aDuration.GetAsUnrestrictedDouble();
}

void
Timing::GetActiveDuration(UnrestrictedDoubleOrStringReturnValue& aDuration) const
{
  aDuration.SetAsUnrestrictedDouble() = mActiveDuration;
}

void
Timing::SetActiveDuration(const UnrestrictedDoubleOrString& aDuration)
{
  mActiveDuration = aDuration.GetAsUnrestrictedDouble();
}

}
}
