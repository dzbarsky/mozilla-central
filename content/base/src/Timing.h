/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Timing_h
#define mozilla_dom_Timing_h

#include "nsWrapperCache.h"
#include "mozilla/dom/TimedItem.h"
#include "mozilla/dom/TimingInputBinding.h"

namespace mozilla {
namespace dom {

class Timing MOZ_FINAL : public nsWrapperCache
{
public:
  Timing(TimedItem* aTimedItem, const TimingInput& aTiming);
  Timing(Timing* aOther);
  virtual ~Timing();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Timing)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Timing)

  // WebIDL
  TimedItem* GetParentObject() {
    return mTimedItem;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  double StartDelay() {
    return mStartDelay;
  }

  void SetStartDelay(double aDelay) {
    mStartDelay = aDelay;
  }

  enum FillMode FillMode() {
    return mFillMode;
  }

  void SetFillMode(enum FillMode aMode) {
    mFillMode = aMode;
  }

  double IterationStart() {
    return mIterationStart;
  }

  void SetIterationStart(double aStart) {
    mIterationStart = aStart;
  }

  double IterationCount() {
    return mIterationCount;
  }

  void SetIterationCount(double aCount) {
    mIterationCount = aCount;
  }

  double IterationDuration() {
    return mIterationDuration;
  }

  void SetIterationDuration(double aDuration) {
    mIterationDuration = aDuration;
  }

  double PlaybackRate() {
    return mPlaybackRate;
  }

  void SetPlaybackRate(double aRate) {
    mPlaybackRate = aRate;
  }

  PlaybackDirection Direction() {
    return mDirection;
  }

  void SetDirection(PlaybackDirection aDirection) {
    mDirection = aDirection;
  }

  void GetTimingFunction(nsString& aFunction) {
    aFunction = mFunction;
  }

  void SetTimingFunction(const nsAString& aFunction) {
    mFunction = aFunction;
  }

protected:
  nsRefPtr<TimedItem> mTimedItem;
  double mStartDelay;
  enum FillMode mFillMode;
  double mIterationStart;
  double mIterationCount;
  double mIterationDuration;
  double mPlaybackRate;
  PlaybackDirection mDirection;
  nsString mFunction;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Timing_h
