/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimedItem_h
#define mozilla_dom_TimedItem_h

#include "nsDOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class Timing;
class TimingInput;

class TimedItem : public nsDOMEventTargetHelper
{
public:
  TimedItem(const TimingInput& aTiming);
  TimedItem(TimedItem* aOther);

  ~TimedItem();

  // WebIDL
  already_AddRefed<Timing> Specified() const;

  virtual already_AddRefed<TimedItem> CloneInternal() = 0;

  TimeStamp mStartTime;
  TimeDuration mDuration;
protected:
  nsRefPtr<Timing> mTiming;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TimedItem_h
