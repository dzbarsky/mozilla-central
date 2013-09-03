/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ParGroup_h
#define mozilla_dom_ParGroup_h

#include "mozilla/dom/TimingGroup.h"

#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

class ParGroup MOZ_FINAL : public TimingGroup
{
public:
  ParGroup(nsISupports* aOwner,
           const Nullable<Sequence<OwningNonNull<TimedItem> > >& aChildren,
           const TimingInput& aTiming)
    : TimingGroup(aOwner, aChildren, aTiming)
  {
  }

  ParGroup(ParGroup* aOther)
    : TimingGroup(aOther)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ParGroup, TimingGroup)

  // WebIDL
  static already_AddRefed<ParGroup>
    Constructor(const GlobalObject& aGlobal,
                const Nullable<Sequence<OwningNonNull<TimedItem> > >& aChildren,
                const TimingInput& aTiming, ErrorResult& rv)
  {
    nsRefPtr<ParGroup> group = new ParGroup(aGlobal.GetAsSupports(), aChildren, aTiming);
    return group.forget();
  }

  already_AddRefed<ParGroup> Clone() {
    nsRefPtr<ParGroup> group = new ParGroup(this);
    return group.forget();
  }

  virtual already_AddRefed<TimedItem> CloneInternal() MOZ_OVERRIDE {
    return Clone().downcast<TimedItem>();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ParGroup_h
