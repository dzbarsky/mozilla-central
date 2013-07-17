/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIContentParent.h"
#include "nsFrameMessageManager.h"
#include "mozilla/unused.h"
#include "mozilla/dom/PTabContext.h"
#include "mozilla/dom/TabParent.h"
#include "nsDOMFile.h"
#include "mozilla/dom/ipc/nsIRemoteBlob.h"
#include "mozilla/dom/StructuredCloneUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "JavaScriptParent.h"
#include "nsPrintfCString.h"

using namespace mozilla::jsipc;

namespace mozilla {
namespace dom {

nsIContentParent::nsIContentParent()
{
     mMessageManager = nsFrameMessageManager::NewProcessMessageManager(this);
}

ContentParent*
nsIContentParent::AsContentParent()
{
  MOZ_ASSERT(IsContentParent());
  return static_cast<ContentParent*>(this);
}

mozilla::jsipc::PJavaScriptParent *
nsIContentParent::AllocPJavaScriptParent()
{
    mozilla::jsipc::JavaScriptParent *parent = new mozilla::jsipc::JavaScriptParent();
    if (!parent->init()) {
        delete parent;
        return NULL;
    }
    return parent;
}

bool
nsIContentParent::DeallocPJavaScriptParent(PJavaScriptParent *parent)
{
    static_cast<mozilla::jsipc::JavaScriptParent *>(parent)->destroyFromContent();
    return true;
}

bool
nsIContentParent::CanOpenBrowser(const IPCTabContext& aContext)
{
    const IPCTabAppBrowserContext& appBrowser = aContext.appBrowserContext();

    // We don't trust the IPCTabContext we receive from the child, so we'll bail
    // if we receive an IPCTabContext that's not a PopupIPCTabContext.
    // (PopupIPCTabContext lets the child process prove that it has access to
    // the app it's trying to open.)
    if (appBrowser.type() != IPCTabAppBrowserContext::TPopupIPCTabContext) {
        NS_ERROR("Unexpected IPCTabContext type.  Aborting AllocPBrowserParent.");
        return false;
    }

    const PopupIPCTabContext& popupContext = appBrowser.get_PopupIPCTabContext();
    TabParent* opener = static_cast<TabParent*>(popupContext.openerParent());
    if (!opener) {
        NS_ERROR("Got null opener from child; aborting AllocPBrowserParent.");
        return false;
    }

    // Popup windows of isBrowser frames must be isBrowser if the parent
    // isBrowser.  Allocating a !isBrowser frame with same app ID would allow
    // the content to access data it's not supposed to.
    if (!popupContext.isBrowserElement() && opener->IsBrowserElement()) {
        NS_ERROR("Child trying to escalate privileges!  Aborting AllocPBrowserParent.");
        return false;
    }

    MaybeInvalidTabContext tc(aContext);
    if (!tc.IsValid()) {
        NS_ERROR(nsPrintfCString("Child passed us an invalid TabContext.  (%s)  "
                                 "Aborting AllocPBrowserParent.",
                                 tc.GetInvalidReason()).get());
        return false;
    }

    return true;
}

PBrowserParent*
nsIContentParent::AllocPBrowserParent(const IPCTabContext& aContext,
                                      const uint32_t &aChromeFlags)
{
    unused << aChromeFlags;

    if (!CanOpenBrowser(aContext)) {
        return nullptr;
    }

    MaybeInvalidTabContext tc(aContext);
    MOZ_ASSERT(tc.IsValid());
    TabParent* parent = new TabParent(this, tc.GetTabContext());

    // We release this ref in DeallocPBrowserParent()
    NS_ADDREF(parent);
    return parent;
}

bool
nsIContentParent::DeallocPBrowserParent(PBrowserParent* frame)
{
    TabParent* parent = static_cast<TabParent*>(frame);
    NS_RELEASE(parent);
    return true;
}

PBlobParent*
nsIContentParent::AllocPBlobParent(const BlobConstructorParams& aParams)
{
  return BlobParent::Create(this, aParams);
}

bool
nsIContentParent::DeallocPBlobParent(PBlobParent* aActor)
{
  delete aActor;
  return true;
}

BlobParent*
nsIContentParent::GetOrCreateActorForBlob(nsIDOMBlob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);

  // If the blob represents a remote blob for this ContentParent then we can
  // simply pass its actor back here.
  if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(aBlob)) {
    BlobParent* actor = static_cast<BlobParent*>(
        static_cast<PBlobParent*>(remoteBlob->GetPBlob()));
    MOZ_ASSERT(actor);

    if (actor->Manager() == this) {
      return actor;
    }
  }

  // XXX This is only safe so long as all blob implementations in our tree
  //     inherit nsDOMFileBase. If that ever changes then this will need to grow
  //     a real interface or something.
  const nsDOMFileBase* blob = static_cast<nsDOMFileBase*>(aBlob);

  // We often pass blobs that are multipart but that only contain one sub-blob
  // (WebActivities does this a bunch). Unwrap to reduce the number of actors
  // that we have to maintain.
  const nsTArray<nsCOMPtr<nsIDOMBlob> >* subBlobs = blob->GetSubBlobs();
  if (subBlobs && subBlobs->Length() == 1) {
    const nsCOMPtr<nsIDOMBlob>& subBlob = subBlobs->ElementAt(0);
    MOZ_ASSERT(subBlob);

    // We can only take this shortcut if the multipart and the sub-blob are both
    // Blob objects or both File objects.
    nsCOMPtr<nsIDOMFile> multipartBlobAsFile = do_QueryInterface(aBlob);
    nsCOMPtr<nsIDOMFile> subBlobAsFile = do_QueryInterface(subBlob);
    if (!multipartBlobAsFile == !subBlobAsFile) {
      // The wrapping might have been unnecessary, see if we can simply pass an
      // existing remote blob for this ContentParent.
      if (nsCOMPtr<nsIRemoteBlob> remoteSubBlob = do_QueryInterface(subBlob)) {
        BlobParent* actor =
          static_cast<BlobParent*>(
            static_cast<PBlobParent*>(remoteSubBlob->GetPBlob()));

        if (actor->Manager() == this) {
          return actor;
        }
      }

      // No need to add a reference here since the original blob must have a
      // strong reference in the caller and it must also have a strong reference
      // to this sub-blob.
      aBlob = subBlob;
      blob = static_cast<nsDOMFileBase*>(aBlob);
      subBlobs = blob->GetSubBlobs();
    }
  }

  // All blobs shared between processes must be immutable.
  nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(aBlob);
  if (!mutableBlob || NS_FAILED(mutableBlob->SetMutable(false))) {
    NS_WARNING("Failed to make blob immutable!");
    return nullptr;
  }

  ChildBlobConstructorParams params;

  if (blob->IsSizeUnknown() || blob->IsDateUnknown()) {
    // We don't want to call GetSize or GetLastModifiedDate
    // yet since that may stat a file on the main thread
    // here. Instead we'll learn the size lazily from the
    // other process.
    params = MysteryBlobConstructorParams();
  }
  else {
    nsString contentType;
    nsresult rv = aBlob->GetType(contentType);
    NS_ENSURE_SUCCESS(rv, nullptr);

    uint64_t length;
    rv = aBlob->GetSize(&length);
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
    if (file) {
      FileBlobConstructorParams fileParams;

      rv = file->GetMozLastModifiedDate(&fileParams.modDate());
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = file->GetName(fileParams.name());
      NS_ENSURE_SUCCESS(rv, nullptr);

      fileParams.contentType() = contentType;
      fileParams.length() = length;

      params = fileParams;
    } else {
      NormalBlobConstructorParams blobParams;
      blobParams.contentType() = contentType;
      blobParams.length() = length;
      params = blobParams;
    }
  }

  BlobParent* actor = BlobParent::Create(this, aBlob);
  NS_ENSURE_TRUE(actor, nullptr);

  return SendPBlobConstructor(actor, params) ? actor : nullptr;
}

bool
nsIContentParent::RecvSyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  InfallibleTArray<nsString>* aRetvals)
{
  nsRefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    StructuredCloneData cloneData = ipc::UnpackClonedMessageDataForParent(aData);
    CpowIdHolder cpows(GetCPOWManager(), aCpows);
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                        aMsg, true, &cloneData, &cpows, aRetvals);
  }
  return true;
}

bool
nsIContentParent::RecvAsyncMessage(const nsString& aMsg,
                                   const ClonedMessageData& aData,
                                   const InfallibleTArray<CpowEntry>& aCpows)
{
  nsRefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    StructuredCloneData cloneData = ipc::UnpackClonedMessageDataForParent(aData);
    CpowIdHolder cpows(GetCPOWManager(), aCpows);
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                        aMsg, false, &cloneData, &cpows, nullptr);
  }
  return true;
}

} // namespace dom
} // namespace mozilla
