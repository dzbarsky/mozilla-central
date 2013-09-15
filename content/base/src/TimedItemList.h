/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimedItemList_h
#define mozilla_dom_TimedItemList_h

#include "nsWrapperCache.h"

#include "mozilla/Attributes.h"

#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class TimedItem;
class TimingGroup;

class TimedItemList MOZ_FINAL : public nsISupports // nneded for Proxy bindings
                              , public nsWrapperCache
{
public:
  TimedItemList(nsISupports* aOwner);

  ~TimedItemList();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TimedItemList)

  void AppendElement(TimedItem* aItem);
  void AppendElement(already_AddRefed<TimedItem> aItem);

  const nsTArray<nsRefPtr<TimedItem> >& AsArray()
  {
    return mItems;
  }

  // WebIDL
  nsISupports* GetParentObject() {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  uint32_t Length();

  TimedItem* Item(uint32_t aIndex);
  TimedItem* IndexedGetter(uint32_t aIndex, bool& aFound)
  {
    TimedItem* item = Item(aIndex);
    aFound = !!item;
    return item;
  }

protected:
  nsRefPtr<nsISupports> mOwner;
  nsTArray<nsRefPtr<TimedItem> > mItems;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TimedItemList_h
