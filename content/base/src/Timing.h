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

class UnrestrictedDoubleOrString;
class OwningUnrestrictedDoubleOrString;

class Timing MOZ_FINAL : public nsWrapperCache
{
public:
  Timing(TimedItem* aTimedItem, const TimingInput& aTiming);
  Timing(Timing* aOther);
  virtual ~Timing();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Timing)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Timing)

  // WebIDL
  TimedItem* GetParentObject() const {
    return mTimedItem;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  double Delay() const {
    return mStartDelay;
  }

  void SetDelay(double aDelay) {
    mStartDelay = aDelay;
  }

  enum FillMode Fill() const {
    return mFillMode;
  }

  void SetFill(enum FillMode aMode) {
    mFillMode = aMode;
  }

  double IterationStart() const {
    return mIterationStart;
  }

  void SetIterationStart(double aStart) {
    mIterationStart = aStart;
  }

  double Iterations() const {
    return mIterationCount;
  }

  void SetIterations(double aCount) {
    mIterationCount = aCount;
  }

  double IterationDuration() const {
    return mIterationDuration;
  }

  void GetDuration(OwningUnrestrictedDoubleOrString& aDuration) const;

  void SetDuration(const UnrestrictedDoubleOrString& aDuration);

  void GetActiveDuration(OwningUnrestrictedDoubleOrString& aDuration) const;

  void SetActiveDuration(const UnrestrictedDoubleOrString& aDuration);

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

  void GetEasing(nsString& aEasing) {
  }

  void SetEasing(const nsAString& aEasing) {
  }

  void GetEasingTimes(OwningSpacingModeOrDouble& aTimes) {
  }

  void SetEasingTimes(const SpacingModeOrDouble& aTimes) {
  }

protected:
  nsRefPtr<TimedItem> mTimedItem;
  double mStartDelay;
  enum FillMode mFillMode;
  double mIterationStart;
  double mIterationCount;
  double mIterationDuration;
  double mActiveDuration;
  double mPlaybackRate;
  PlaybackDirection mDirection;
  nsString mFunction;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Timing_h
