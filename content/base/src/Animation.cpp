/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/AnimationBinding.h"
#include "nsStyleAnimation.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(Animation, mElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Animation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Animation, Release)

JSObject*
Animation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AnimationBinding::Wrap(aCx, aScope, this);
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

    nsStyleAnimation::Value *val =
      mStyleRule->AddEmptyValue(eCSSProperty_opacity);

     nsStyleAnimation::Value zero, one;
     zero.SetFloatValue(0);
     one.SetFloatValue(1);

#ifdef DEBUG
    bool result =
#endif
      nsStyleAnimation::Interpolate(eCSSProperty_opacity, one, zero,
                                    valuePosition, *val);
    NS_ABORT_IF_FALSE(result, "interpolate must succeed now");
  }
}

} // namespace dom
} // namespace mozilla
