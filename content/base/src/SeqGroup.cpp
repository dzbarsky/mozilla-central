/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SeqGroup.h"

#include "mozilla/dom/SeqGroupBinding.h"
#include "mozilla/dom/TimedItemList.h"
#include "mozilla/dom/Timing.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_3(SeqGroup,
                                        mOwner,
                                        mChildren,
                                        mTiming)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SeqGroup)
NS_INTERFACE_MAP_END_INHERITING(TimingGroup)

NS_IMPL_ADDREF_INHERITED(SeqGroup, TimingGroup)
NS_IMPL_RELEASE_INHERITED(SeqGroup, TimingGroup)

JSObject*
SeqGroup::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SeqGroupBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
