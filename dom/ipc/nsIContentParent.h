/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsIContentParent_h
#define mozilla_dom_nsIContentParent_h

#include "nsISupports.h"
#include "nsFrameMessageManager.h"
#include "mozilla/dom/ipc/Blob.h"

class nsFrameMessageManager;

namespace mozilla {

namespace jsipc {
class PJavaScriptParent;
class JavaScriptParent;
class CpowEntry;
}

namespace dom {
struct IPCTabContext;

class ContentParent;

class nsIContentParent : public nsISupports
                       , public mozilla::dom::ipc::MessageManagerCallback
{
public:
    nsIContentParent();
    BlobParent* GetOrCreateActorForBlob(nsIDOMBlob* aBlob);
    virtual uint64_t ChildID() = 0;

    virtual PBlobParent*
    SendPBlobConstructor(
            PBlobParent* actor,
            const BlobConstructorParams& params) NS_WARN_UNUSED_RESULT = 0;

    virtual PBrowserParent*
    SendPBrowserConstructor(
            PBrowserParent* actor,
            const IPCTabContext& context,
            const uint32_t& chromeFlags) NS_WARN_UNUSED_RESULT = 0;

    virtual jsipc::JavaScriptParent *GetCPOWManager() = 0;

    virtual bool IsContentParent() { return false; }
    ContentParent* AsContentParent();

protected:
    virtual mozilla::jsipc::PJavaScriptParent* AllocPJavaScriptParent();
    virtual bool DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*);

    bool CanOpenBrowser(const IPCTabContext& aContext);

    virtual PBrowserParent* AllocPBrowserParent(const IPCTabContext& aContext,
                                                const uint32_t& aChromeFlags);
    virtual bool DeallocPBrowserParent(PBrowserParent* frame);

    virtual PBlobParent* AllocPBlobParent(const BlobConstructorParams& aParams);
    virtual bool DeallocPBlobParent(PBlobParent*);

    virtual bool RecvSyncMessage(const nsString& aMsg,
                                 const ClonedMessageData& aData,
                                 const InfallibleTArray<jsipc::CpowEntry>& aCpows,
                                 InfallibleTArray<nsString>* aRetvals);
    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<jsipc::CpowEntry>& aCpows);


    nsRefPtr<nsFrameMessageManager> mMessageManager;
};

} // namespace dom
} // namespace mozilla

#endif
