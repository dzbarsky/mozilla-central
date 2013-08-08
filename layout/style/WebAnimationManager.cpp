/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebAnimationManager.h"
#include "nsStyleAnimation.h"
#include "nsRuleProcessorData.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

Animation*
WebAnimationManager::AddAnimation(Element* aElement, const TimingInput& aTiming)
{
  nsRefPtr<Animation> anim = new Animation(aElement, aTiming);
  mAnimations.AppendElement(anim);
  if (!mObservingRefreshDriver) {
    // We need to observe the refresh driver.
    mPresContext->RefreshDriver()->AddRefreshObserver(this, Flush_Style);
    mObservingRefreshDriver = true;
  }
  aElement->AddAnimation(anim);
  return anim;
}

void
WebAnimationManager::EnsureStyleRuleFor(Animation* aAnim)
{
  aAnim->EnsureStyleRuleFor(mPresContext->RefreshDriver()->MostRecentRefresh());
  CheckNeedsRefresh();
}

/* virtual */ void
WebAnimationManager::RulesMatching(ElementRuleProcessorData* aData)
{
  NS_ABORT_IF_FALSE(aData->mPresContext == mPresContext,
                    "pres context mismatch");
  nsIStyleRule *rule =
    GetAnimationRule(aData->mElement,
                     nsCSSPseudoElements::ePseudo_NotPseudoElement);
  if (rule) {
    aData->mRuleWalker->Forward(rule);
  }
}

/* virtual */ void
WebAnimationManager::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  NS_ABORT_IF_FALSE(aData->mPresContext == mPresContext,
                    "pres context mismatch");
  if (aData->mPseudoType != nsCSSPseudoElements::ePseudo_before &&
      aData->mPseudoType != nsCSSPseudoElements::ePseudo_after) {
    return;
  }

  // FIXME: Do we really want to be the only thing keeping a
  // pseudo-element alive?  I *think* the non-animation restyle should
  // handle that, but should add a test.
  nsIStyleRule *rule = GetAnimationRule(aData->mElement, aData->mPseudoType);
  if (rule) {
    aData->mRuleWalker->Forward(rule);
  }
}

/* virtual */ void
WebAnimationManager::RulesMatching(AnonBoxRuleProcessorData* aData)
{
}

#ifdef MOZ_XUL
/* virtual */ void
WebAnimationManager::RulesMatching(XULTreeRuleProcessorData* aData)
{
}
#endif

/* virtual */ size_t
WebAnimationManager::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return CommonAnimationManager::SizeOfExcludingThis(aMallocSizeOf);
}

/* virtual */ size_t
WebAnimationManager::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

nsIStyleRule*
WebAnimationManager::GetAnimationRule(mozilla::dom::Element* aElement,
                                      nsCSSPseudoElements::Type aPseudoType)
{
  NS_ABORT_IF_FALSE(
    aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
    aPseudoType == nsCSSPseudoElements::ePseudo_before ||
    aPseudoType == nsCSSPseudoElements::ePseudo_after,
    "forbidden pseudo type");

  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore animations.
    return nullptr;
  }

  if (aElement->GetAnimations().Length() == 0) {
    return nullptr;
  }

  Animation* anim = aElement->GetAnimations()[0];
  if (!anim) {
    return nullptr;
  }

  if (mPresContext->IsProcessingRestyles() &&
      !mPresContext->IsProcessingAnimationStyleChange()) {
    // During the non-animation part of processing restyles, we don't
    // add the animation rule.

    if (anim->mStyleRule) {
      anim->PostRestyleForAnimation(mPresContext);
    }

    return nullptr;
  }

  NS_WARN_IF_FALSE(anim->mStyleRuleRefreshTime ==
                     mPresContext->RefreshDriver()->MostRecentRefresh(),
                   "should already have refreshed style rule");

  return anim->mStyleRule;
}

/* virtual */ void
WebAnimationManager::WillRefresh(mozilla::TimeStamp aTime)
{
  NS_ABORT_IF_FALSE(mPresContext,
                    "refresh driver should not notify additional observers "
                    "after pres context has been destroyed");
  if (!mPresContext->GetPresShell()) {
    // Someone might be keeping mPresContext alive past the point
    // where it has been torn down; don't bother doing anything in
    // this case.  But do get rid of all our transitions so we stop
    // triggering refreshes.
    RemoveAllElementData();
    return;
  }

  FlushAnimations(Can_Throttle);
}

void
WebAnimationManager::CheckNeedsRefresh()
{
  for (uint32_t i = 0; i < mAnimations.Length(); i++) {
    Animation* anim = mAnimations[i];
    if (anim->mNeedsRefreshes) {
      if (!mObservingRefreshDriver) {
        mPresContext->RefreshDriver()->AddRefreshObserver(this, Flush_Style);
        mObservingRefreshDriver = true;
      }
      return;
    }
  }
  if (mObservingRefreshDriver) {
    mObservingRefreshDriver = false;
    mPresContext->RefreshDriver()->RemoveRefreshObserver(this, Flush_Style);
  }
}

void
WebAnimationManager::FlushAnimations(FlushFlags aFlags)
{
  // FIXME: check that there's at least one style rule that's not
  // in its "done" state, and if there isn't, remove ourselves from
  // the refresh driver (but leave the animations!).
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
  bool didThrottle = false;
  for (uint32_t i = 0; i < mAnimations.Length(); i++) {
    Animation* anim = mAnimations[i];

    nsRefPtr<css::AnimValuesStyleRule> oldStyleRule = anim->mStyleRule;
    anim->EnsureStyleRuleFor(now);
    CheckNeedsRefresh();
    if (oldStyleRule != anim->mStyleRule) {
      anim->PostRestyleForAnimation(mPresContext);
    } else {
      didThrottle = true;
    }
  }

  if (didThrottle) {
    mPresContext->Document()->SetNeedStyleFlush();
  }
}
