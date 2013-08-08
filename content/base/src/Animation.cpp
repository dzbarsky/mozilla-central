/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Animation.h"

#include "mozilla/dom/AnimationBinding.h"
#include "mozilla/dom/Timing.h"
#include "nsStyleAnimation.h"

using mozilla::css::AnimValuesStyleRule;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(Animation,
                                        mElement,
                                        mTiming)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Animation)
NS_INTERFACE_MAP_END_INHERITING(TimedItem)

NS_IMPL_ADDREF_INHERITED(Animation, TimedItem)
NS_IMPL_RELEASE_INHERITED(Animation, TimedItem)

JSObject*
Animation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AnimationBinding::Wrap(aCx, aScope, this);
}

/* static */ already_AddRefed<Animation>
Animation::Constructor(const GlobalObject& aGlobal, JSContext* aCx,
                       Element* aTarget, Sequence<JSObject*> aKeyframes,
                       const TimingInput& aTiming, ErrorResult& rv)
{
  nsRefPtr<Animation> anim = new Animation(aTarget, aTiming);

  return anim.forget();
}

struct InterpolationData {
  AnimValuesStyleRule* mStyleRule;
  double mPosition;
};

static PLDHashOperator
InterpolateProperties(const uint64_t& aProperty,
                      PropertyAnimation* aAnimation,
                      void* aUserArg)
{
  InterpolationData* data = static_cast<InterpolationData*>(aUserArg);
  NS_ASSERTION(aAnimation->mKeyframes.Length() >= 2,
               "not enough keyframes! need to implement timing model stuff");
  nsStyleAnimation::Value* val =
    data->mStyleRule->AddEmptyValue(nsCSSProperty(aProperty));

  printf_stderr("\nposition: %f, opacity: computing...\n", data->mPosition);

  uint32_t currentIndex = 0;
  float startOffset, endOffset;
  nsStyleAnimation::Value startValue, endValue;
  do {
    startOffset = aAnimation->mKeyframes[currentIndex].mOffset;
    endOffset = aAnimation->mKeyframes[currentIndex+1].mOffset;
    startValue = aAnimation->mKeyframes[currentIndex].mValue;
    endValue = aAnimation->mKeyframes[currentIndex+1].mValue;
    currentIndex++;
    printf_stderr("\nstartoff: %f, endoff: %f\n", startOffset, endOffset);
  } while ((currentIndex + 1) < aAnimation->mKeyframes.Length() &&
           data->mPosition > aAnimation->mKeyframes[currentIndex].mOffset);

  float positionInSegment = (data->mPosition - startOffset) / (endOffset - startOffset);

#ifdef DEBUG
  bool result =
#endif
    nsStyleAnimation::Interpolate(nsCSSProperty(aProperty), startValue, endValue,
                                  positionInSegment, *val);
  NS_ABORT_IF_FALSE(result, "interpolate must succeed now");
  printf_stderr("\nposition: %f, opacity: %f\n", data->mPosition, val->GetFloatValue());
  return PL_DHASH_NEXT;
}

void
Animation::EnsureStyleRuleFor(TimeStamp aRefreshTime)
{
  if (!mNeedsRefreshes) {
    mStyleRuleRefreshTime = aRefreshTime;
    return;
  }

  // mStyleRule may be null and valid, if we have no style to apply.
  if (mStyleRuleRefreshTime.IsNull() ||
      mStyleRuleRefreshTime != aRefreshTime) {
    mStyleRuleRefreshTime = aRefreshTime;
    mStyleRule = nullptr;
    // We'll set mNeedsRefreshes to true below in all cases where we need them.
    mNeedsRefreshes = false;

    if (!mStyleRule) {
      // Allocate the style rule now that we know we have animation data.
      mStyleRule = new css::AnimValuesStyleRule();
    }

    double valuePosition = (aRefreshTime - mStartTime) / mDuration;
    if (valuePosition < 1) {
      mNeedsRefreshes = true;
    }

    printf_stderr("\nposition in ensurestyle %f\n", valuePosition);
    InterpolationData data;
    data.mStyleRule = mStyleRule;
    data.mPosition = valuePosition;

    mPropertyAnimations.EnumerateRead(InterpolateProperties, &data);
  }
}

} // namespace dom
} // namespace mozilla
