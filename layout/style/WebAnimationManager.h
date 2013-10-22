/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef WebAnimationManager_h
#define WebAnimationManager_h

#include "mozilla/Attributes.h"
#include "AnimationCommon.h"
#include "nsCSSPseudoElements.h"
#include "nsStyleContext.h"
#include "nsDataHashtable.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/Animation.h"

class WebAnimationManager MOZ_FINAL
  : public mozilla::css::CommonAnimationManager
{
public:
  WebAnimationManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
    , mObservingRefreshDriver(false)
  {
  }

  void EnsureStyleRuleFor(mozilla::dom::Animation* aAnim);

  // nsIStyleRuleProcessor (parts)
  virtual void RulesMatching(ElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) MOZ_OVERRIDE;
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) MOZ_OVERRIDE;
#endif
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime) MOZ_OVERRIDE;

  void FlushAnimations(FlushFlags aFlags);

  mozilla::dom::Animation* AddAnimation(mozilla::dom::Element* aElement,
                                        const mozilla::dom::UnrestrictedDoubleOrTimingInput& aTiming);

protected:
  virtual void ElementDataRemoved() MOZ_OVERRIDE
  {
    CheckNeedsRefresh();
  }

  virtual void AddElementData(mozilla::css::CommonElementAnimationData* aData)
  {
    MOZ_ASSERT(false);
  }

  /**
   * Check to see if we should stop or start observing the refresh driver
   */
  void CheckNeedsRefresh();

private:
  nsIStyleRule* GetAnimationRule(mozilla::dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType);

  bool mObservingRefreshDriver;
  nsTArray<nsRefPtr<mozilla::dom::Animation> > mAnimations;
};

#endif // WebAnimationManager_h
