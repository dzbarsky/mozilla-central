/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Timing.h"

#include "mozilla/dom/TimedItem.h"
#include "mozilla/dom/TimingBinding.h"
#include "mozilla/dom/TimingInputBinding.h"

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
  , mIterationStart(aTiming.mIterationStart)
  , mIterationCount(aTiming.mIterationCount)
  , mIterationDuration(aTiming.mIterationDuration)
  , mPlaybackRate(aTiming.mPlaybackRate)
{
  SetIsDOMBinding();
}

Timing::Timing(Timing* aOther)
  : mTimedItem(aOther->mTimedItem)
  , mStartDelay(aOther->mStartDelay)
  , mIterationStart(aOther->mIterationStart)
  , mIterationCount(aOther->mIterationCount)
  , mIterationDuration(aOther->mIterationDuration)
  , mPlaybackRate(aOther->mPlaybackRate)
{
  SetIsDOMBinding();
}

}
}
