/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Animation_h
#define mozilla_dom_Animation_h

#include "mozilla/dom/TimedItem.h"

#include "mozilla/Attributes.h"
#include "AnimationCommon.h"

class WebAnimationManager;

namespace mozilla {
namespace dom {

class Element;
class TimingInput;

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

class Animation MOZ_FINAL : public TimedItem
{
  friend class ::WebAnimationManager;
  friend class Element;
public:
  Animation(Element* aElement, const UnrestrictedDoubleOrTimingInput& aTiming)
    : TimedItem(aTiming)
    , mElement(aElement)
    , mNeedsRefreshes(true)
  {
  }

  Animation(Animation* aAnimation)
    : TimedItem(aAnimation)
    , mElement(aAnimation->mElement)
    , mNeedsRefreshes(true)
  {
  }

  virtual ~Animation()
  { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(Animation, TimedItem)

  void EnsureStyleRuleFor(TimeStamp aRefreshTime);

  void PostRestyleForAnimation(nsPresContext *aPresContext) {
    aPresContext->PresShell()->RestyleForAnimation(mElement, eRestyle_Self);
  }

  // WebIDL
  static already_AddRefed<Animation>
    Constructor(const GlobalObject& aGlobal, JSContext* aCx,
                Element* aTarget, Sequence<JSObject*> aKeyframes,
                const UnrestrictedDoubleOrTimingInput& aTiming,
                ErrorResult& rv);

  Element* GetTarget() {
    return mElement;
  }

  already_AddRefed<Animation> Clone() {
    nsRefPtr<Animation> anim = new Animation(this);
    return anim.forget();
  }

  virtual already_AddRefed<TimedItem> CloneInternal() MOZ_OVERRIDE {
    return Clone().downcast<TimedItem>();
  }

  Element* GetParentObject() {
    return mElement;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

protected:
  nsRefPtr<mozilla::css::AnimValuesStyleRule> mStyleRule;
  TimeStamp mStyleRuleRefreshTime;

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
