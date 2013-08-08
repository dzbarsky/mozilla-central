/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TimedItemList.h"

#include "mozilla/dom/TimingGroup.h"
#include "mozilla/dom/TimedItem.h"
#include "mozilla/dom/TimedItemListBinding.h"

namespace mozilla {
namespace dom {

TimedItemList::TimedItemList(nsISupports* aOwner)
  : mOwner(aOwner)
{
  SetIsDOMBinding();
}

TimedItemList::~TimedItemList()
{
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(TimedItemList,
                                        mOwner,
                                        mItems)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TimedItemList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TimedItemList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TimedItemList)

JSObject*
TimedItemList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TimedItemListBinding::Wrap(aCx, aScope, this);
}

void
TimedItemList::AppendElement(TimedItem* aItem)
{
  mItems.AppendElement(aItem);
}

void
TimedItemList::AppendElement(already_AddRefed<TimedItem> aItem)
{
  mItems.AppendElement(aItem);
}

uint32_t
TimedItemList::Length()
{
  return mItems.Length();
}

TimedItem*
TimedItemList::Item(uint32_t aIndex)
{
  return mItems.SafeElementAt(aIndex);
}

} // namespace dom
} // namespace mozilla
