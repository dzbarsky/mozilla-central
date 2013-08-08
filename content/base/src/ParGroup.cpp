/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ParGroup.h"

#include "mozilla/dom/ParGroupBinding.h"
#include "mozilla/dom/Timing.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_3(ParGroup,
                                        mOwner,
                                        mChildren,
                                        mTiming)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ParGroup)
NS_INTERFACE_MAP_END_INHERITING(TimingGroup)

NS_IMPL_ADDREF_INHERITED(ParGroup, TimingGroup)
NS_IMPL_RELEASE_INHERITED(ParGroup, TimingGroup)

JSObject*
ParGroup::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return ParGroupBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
