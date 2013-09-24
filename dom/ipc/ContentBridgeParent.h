/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBridgeParent_h
#define mozilla_dom_ContentBridgeParent_h

#include "mozilla/dom/PContentBridgeParent.h"
#include "mozilla/dom/nsIContentParent.h"

namespace mozilla {
namespace dom {

class ContentBridgeParent : public PContentBridgeParent
                          , public nsIContentParent
{
  friend class ContentParent;
public:
  ContentBridgeParent(Transport* aTransport);

  NS_DECL_ISUPPORTS

  virtual ~ContentBridgeParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  void DeferredDestroy();

  static ContentBridgeParent*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  virtual PBlobParent*
  SendPBlobConstructor(PBlobParent* actor,
                       const BlobConstructorParams& params) MOZ_OVERRIDE;

  virtual PBrowserParent*
  SendPBrowserConstructor(PBrowserParent* actor,
                          const IPCTabContext& context,
                          const uint32_t& chromeFlags) MOZ_OVERRIDE;

  jsipc::JavaScriptParent* GetCPOWManager();

  uint64_t ChildID() MOZ_OVERRIDE
  {
    return mChildID;
  }

protected:
  void SetChildID(uint64_t aId)
  {
    mChildID = aId;
  }

  virtual bool RecvSyncMessage(
            const nsString& aMessage,
            const ClonedMessageData& aData,
            const InfallibleTArray<jsipc::CpowEntry>& aCpows,
            InfallibleTArray<nsString>* retval) MOZ_OVERRIDE;

  virtual bool RecvAsyncMessage(
            const nsString& aMessage,
            const ClonedMessageData& aData,
            const InfallibleTArray<jsipc::CpowEntry>& aCpows) MOZ_OVERRIDE;

  virtual mozilla::jsipc::PJavaScriptParent* AllocPJavaScriptParent() MOZ_OVERRIDE;
  virtual bool
  DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*) MOZ_OVERRIDE;

  virtual PBrowserParent*
  AllocPBrowserParent(const IPCTabContext &aContext,
                      const uint32_t &chromeFlags) MOZ_OVERRIDE;
  virtual bool DeallocPBrowserParent(PBrowserParent*) MOZ_OVERRIDE;

  virtual PBlobParent*
  AllocPBlobParent(const BlobConstructorParams& aParams) MOZ_OVERRIDE;

  virtual bool DeallocPBlobParent(PBlobParent*) MOZ_OVERRIDE;

  DISALLOW_EVIL_CONSTRUCTORS(ContentBridgeParent);

  nsRefPtr<ContentBridgeParent> mSelfRef;

  Transport* mTransport; // owned
  uint64_t mChildID;
};

} // dom
} // mozilla

#endif // mozilla_dom_ContentBridgeParent_h
