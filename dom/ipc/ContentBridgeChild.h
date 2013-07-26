/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBridgeChild_h
#define mozilla_dom_ContentBridgeChild_h

#include "mozilla/dom/PContentBridgeChild.h"
#include "mozilla/dom/nsIContentChild.h"

namespace mozilla {
namespace dom {

class ContentBridgeChild MOZ_FINAL : public PContentBridgeChild
                                   , public nsIContentChild
{
public:
  ContentBridgeChild(Transport* aTransport);

  NS_DECL_ISUPPORTS

  virtual ~ContentBridgeChild();

  static ContentBridgeChild*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  void DeferredDestroy();

  virtual bool RecvAsyncMessage(
            const nsString& aMessage,
            const ClonedMessageData& aData,
            const InfallibleTArray<jsipc::CpowEntry>& aCpows) MOZ_OVERRIDE;

  virtual PBlobChild*
  SendPBlobConstructor(PBlobChild* actor,
                       const BlobConstructorParams& params);

  jsipc::JavaScriptChild* GetCPOWManager();

  virtual bool SendPBrowserConstructor(PBrowserChild* actor,
                                       const IPCTabContext& context,
                                       const uint32_t& chromeFlags);

protected:
  virtual PBrowserChild* AllocPBrowserChild(const IPCTabContext &aContext,
                                            const uint32_t &chromeFlags) MOZ_OVERRIDE;
  virtual bool DeallocPBrowserChild(PBrowserChild*);

  virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() MOZ_OVERRIDE;
  virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) MOZ_OVERRIDE;

  virtual PBlobChild* AllocPBlobChild(const BlobConstructorParams& aParams) MOZ_OVERRIDE;
  virtual bool DeallocPBlobChild(PBlobChild*) MOZ_OVERRIDE;

  DISALLOW_EVIL_CONSTRUCTORS(ContentBridgeChild);

  nsRefPtr<ContentBridgeChild> mSelfRef;
  Transport* mTransport; // owned
};

} // dom
} // mozilla

#endif // mozilla_dom_ContentBridgeChild_h
