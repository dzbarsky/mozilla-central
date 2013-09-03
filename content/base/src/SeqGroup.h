/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SeqGroup_h
#define mozilla_dom_SeqGroup_h

#include "mozilla/dom/TimingGroup.h"

#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

class SeqGroup MOZ_FINAL : public TimingGroup
{
public:
  SeqGroup(nsISupports* aOwner,
           const Nullable<Sequence<OwningNonNull<TimedItem> > >& aChildren,
           const TimingInput& aTiming)
    : TimingGroup(aOwner, aChildren, aTiming)
  {
  }

  SeqGroup(SeqGroup* aOther)
    : TimingGroup(aOther)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(SeqGroup, TimingGroup)

  // WebIDL
  static already_AddRefed<SeqGroup>
    Constructor(const GlobalObject& aGlobal,
                const Nullable<Sequence<OwningNonNull<TimedItem> > >& aChildren,
                const TimingInput& aTiming, ErrorResult& rv)
  {
    nsRefPtr<SeqGroup> group = new SeqGroup(aGlobal.GetAsSupports(), aChildren, aTiming);
    return group.forget();
  }

  already_AddRefed<SeqGroup> Clone() {
    nsRefPtr<SeqGroup> group = new SeqGroup(this);
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

#endif // mozilla_dom_SeqGroup_h
