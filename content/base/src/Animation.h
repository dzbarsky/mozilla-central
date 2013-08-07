/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Animation_h
#define mozilla_dom_Animation_h

#include "mozilla/Attributes.h"
#include "AnimationCommon.h"

class WebAnimationManager;

namespace mozilla {
namespace dom {

class Element;

class PropertyAnimationFrame
{
public:
  float mOffset;
  nsStyleAnimation::Value mValue;
};

class PropertyAnimation
{
public:
  nsTArray<PropertyAnimationFrame> mKeyframes;
};

class Animation MOZ_FINAL : public nsWrapperCache
{
  friend class ::WebAnimationManager;
  friend class Element;
public:
  Animation(Element* aElement)
    : mElement(aElement)
    , mNeedsRefreshes(true)
  {
    SetIsDOMBinding();
    mPropertyAnimations.Init(1);
  }

  virtual ~Animation()
  { }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Animation)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Animation)

  void EnsureStyleRuleFor(TimeStamp aRefreshTime);

  void PostRestyleForAnimation(nsPresContext *aPresContext) {
    aPresContext->PresShell()->RestyleForAnimation(mElement, eRestyle_Self);
  }

  // WebIDL
  Element* GetTarget() {
    return mElement;
  }

  already_AddRefed<Animation> Clone() {
    nsRefPtr<Animation> anim = new Animation(mElement);
    return anim.forget();
  }

  Element* GetParentObject() {
    return mElement;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

protected:
  nsRefPtr<mozilla::css::AnimValuesStyleRule> mStyleRule;
  TimeStamp mStyleRuleRefreshTime;

  TimeStamp mStartTime;
  TimeDuration mDuration;
  nsRefPtr<Element> mElement;
  nsDataHashtable<nsUint64HashKey, PropertyAnimation*> mPropertyAnimations;

  // False when we know that our current style rule is valid
  // indefinitely into the future (because all of our animations are
  // either completed or paused).  May be invalidated by a style change.
  bool mNeedsRefreshes;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Animation_h
