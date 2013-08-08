/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimingGroup_h
#define mozilla_dom_TimingGroup_h

#include "mozilla/dom/TimedItem.h"

namespace mozilla {
namespace dom {

class TimingGroup : public TimedItem
{
public:
  TimingGroup(nsISupports* aOwner,
              const Nullable<Sequence<OwningNonNull<TimedItem> > >& aChildren,
              const TimingInput& aTiming)
    : TimedItem(aTiming)
    , mOwner(aOwner)
  {
    if (!aChildren.IsNull()) {
      const Sequence<OwningNonNull<TimedItem> >& items = aChildren.Value();
      for (uint32_t i = 0; i < items.Length(); i++) {
        mChildren.AppendElement(items[i].get());
      }
    }
  }

  TimingGroup(TimingGroup* aOther)
    : TimedItem(aOther)
    , mOwner(aOther->mOwner)
    , mChildren(aOther->mChildren)
  {
  }

  // WebIDL
  nsISupports* GetParentObject() {
    return mOwner;
  }

protected:
  nsCOMPtr<nsISupports> mOwner;
  nsTArray<nsRefPtr<TimedItem> > mChildren;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TimingGroup_h
