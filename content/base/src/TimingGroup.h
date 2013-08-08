/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimingGroup_h
#define mozilla_dom_TimingGroup_h

#include "mozilla/dom/TimedItem.h"

namespace mozilla {
namespace dom {

class TimedItemList;

class TimingGroup : public TimedItem
{
public:
  TimingGroup(nsISupports* aOwner,
              const Nullable<Sequence<OwningNonNull<TimedItem> > >& aChildren,
              const TimingInput& aTiming);

  TimingGroup(TimingGroup* aOther);

  virtual ~TimingGroup();

  // WebIDL
  nsISupports* GetParentObject() {
    return mOwner;
  }

  TimedItemList* Children() {
    return mChildren;
  }

  TimedItem* GetFirstChild();
  TimedItem* GetLastChild();

protected:
  nsCOMPtr<nsISupports> mOwner;
  nsRefPtr<TimedItemList> mChildren;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TimingGroup_h
