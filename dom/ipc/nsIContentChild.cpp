/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsDOMFile.h"
#include "mozilla/dom/ipc/nsIRemoteBlob.h"
#include "mozilla/dom/StructuredCloneUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "JavaScriptChild.h"
#include "nsIJSRuntimeService.h"
#include "nsPrintfCString.h"

using namespace mozilla::ipc;
using namespace mozilla::jsipc;

namespace mozilla {
namespace dom {

mozilla::jsipc::PJavaScriptChild *
nsIContentChild::AllocPJavaScriptChild()
{
    nsCOMPtr<nsIJSRuntimeService> svc = do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
    NS_ENSURE_TRUE(svc, NULL);

    JSRuntime *rt;
    svc->GetRuntime(&rt);
    NS_ENSURE_TRUE(svc, NULL);

    mozilla::jsipc::JavaScriptChild *child = new mozilla::jsipc::JavaScriptChild(rt);
    if (!child->init()) {
        delete child;
        return NULL;
    }
    return child;
}

bool
nsIContentChild::DeallocPJavaScriptChild(PJavaScriptChild *child)
{
    delete child;
    return true;
}

PBrowserChild*
nsIContentChild::AllocPBrowserChild(const IPCTabContext& aContext,
                                    const uint32_t& aChromeFlags)
{
    // We'll happily accept any kind of IPCTabContext here; we don't need to
    // check that it's of a certain type for security purposes, because we
    // believe whatever the parent process tells us.

    MaybeInvalidTabContext tc(aContext);
    if (!tc.IsValid()) {
        NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                                 "the parent process. (%s)  Crashing...",
                                 tc.GetInvalidReason()).get());
        MOZ_CRASH("Invalid TabContext received from the parent process.");
    }

    nsRefPtr<TabChild> child = TabChild::Create(this, tc.GetTabContext(), aChromeFlags);
    // The ref here is released in DeallocPBrowserChild.
    return child.forget().get();
}

bool
nsIContentChild::DeallocPBrowserChild(PBrowserChild* iframe)
{
    TabChild* child = static_cast<TabChild*>(iframe);
    NS_RELEASE(child);
    return true;
}

PBlobChild*
nsIContentChild::AllocPBlobChild(const BlobConstructorParams& aParams)
{
  return BlobChild::Create(this, aParams);
}

bool
nsIContentChild::DeallocPBlobChild(PBlobChild* aActor)
{
  delete aActor;
  return true;
}

BlobChild*
nsIContentChild::GetOrCreateActorForBlob(nsIDOMBlob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);

  // If the blob represents a remote blob then we can simply pass its actor back
  // here.
  if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(aBlob)) {
    BlobChild* actor =
      static_cast<BlobChild*>(
        static_cast<PBlobChild*>(remoteBlob->GetPBlob()));
    MOZ_ASSERT(actor);
    return actor;
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
      // The wrapping was unnecessary, see if we can simply pass an existing
      // remote blob.
      if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(subBlob)) {
        BlobChild* actor =
          static_cast<BlobChild*>(
            static_cast<PBlobChild*>(remoteBlob->GetPBlob()));
        MOZ_ASSERT(actor);
        return actor;
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

  ParentBlobConstructorParams params;

  if (blob->IsSizeUnknown() || blob->IsDateUnknown()) {
    // We don't want to call GetSize or GetLastModifiedDate
    // yet since that may stat a file on the main thread
    // here. Instead we'll learn the size lazily from the
    // other process.
    params.blobParams() = MysteryBlobConstructorParams();
    params.optionalInputStreamParams() = void_t();
  }
  else {
    nsString contentType;
    nsresult rv = aBlob->GetType(contentType);
    NS_ENSURE_SUCCESS(rv, nullptr);

    uint64_t length;
    rv = aBlob->GetSize(&length);
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsCOMPtr<nsIInputStream> stream;
    rv = aBlob->GetInternalStream(getter_AddRefs(stream));
    NS_ENSURE_SUCCESS(rv, nullptr);

    InputStreamParams inputStreamParams;
    SerializeInputStream(stream, inputStreamParams);

    params.optionalInputStreamParams() = inputStreamParams;

    nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
    if (file) {
      FileBlobConstructorParams fileParams;

      rv = file->GetName(fileParams.name());
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = file->GetMozLastModifiedDate(&fileParams.modDate());
      NS_ENSURE_SUCCESS(rv, nullptr);

      fileParams.contentType() = contentType;
      fileParams.length() = length;

      params.blobParams() = fileParams;
    } else {
      NormalBlobConstructorParams blobParams;
      blobParams.contentType() = contentType;
      blobParams.length() = length;
      params.blobParams() = blobParams;
    }
  }

  BlobChild* actor = BlobChild::Create(this, aBlob);
  NS_ENSURE_TRUE(actor, nullptr);

  return SendPBlobConstructor(actor, params) ? actor : nullptr;
}

bool
nsIContentChild::RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows)
{
  nsRefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::sChildProcessManager;
  if (cpm) {
    StructuredCloneData cloneData = ipc::UnpackClonedMessageDataForChild(aData);
    CpowIdHolder cpows(GetCPOWManager(), aCpows);
    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()),
                        aMsg, false, &cloneData, &cpows, nullptr);
  }
  return true;
}

} // namespace dom
} // namespace mozilla
