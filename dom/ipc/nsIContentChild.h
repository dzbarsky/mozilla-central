/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsIContentChild_h
#define mozilla_dom_nsIContentChild_h

#include "nsISupports.h"
#include "mozilla/dom/ipc/Blob.h"

class PBrowserChild;

namespace mozilla {

namespace jsipc {
class PJavaScriptChild;
class JavaScriptChild;
class CpowEntry;
}

namespace dom {
struct IPCTabContext;

class nsIContentChild : public nsISupports
{
public:
    BlobChild* GetOrCreateActorForBlob(nsIDOMBlob* aBlob);

    virtual PBlobChild*
    SendPBlobConstructor(PBlobChild* actor,
                         const BlobConstructorParams& params) NS_WARN_UNUSED_RESULT = 0;
    virtual bool SendPBrowserConstructor(PBrowserChild* actor,
                                         const IPCTabContext& context,
                                         const uint32_t& chromeFlags) = 0;
    virtual jsipc::JavaScriptChild* GetCPOWManager() = 0;
protected:
    virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild();
    virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*);

    virtual PBrowserChild* AllocPBrowserChild(const IPCTabContext &aContext,
                                              const uint32_t &chromeFlags);
    virtual bool DeallocPBrowserChild(PBrowserChild*);

    virtual PBlobChild* AllocPBlobChild(const BlobConstructorParams& aParams);
    virtual bool DeallocPBlobChild(PBlobChild*);

    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<jsipc::CpowEntry>& aCpows);

};

} // namespace dom
} // namespace mozilla

#endif
