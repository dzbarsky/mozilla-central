/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "nsFrameMessageManager.h"

#include "AppProcessChecker.h"
#include "ContentChild.h"
#include "mozilla/dom/nsIContentParent.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsError.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsJSPrincipals.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsFrameLoader.h"
#include "nsIXULRuntime.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSRuntimeService.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMFile.h"
#include "xpcpublic.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/StructuredCloneUtils.h"
#include "JavaScriptChild.h"
#include "JavaScriptParent.h"
#include "nsDOMLists.h"
#include "nsPrintfCString.h"
#include <algorithm>
#include "nsXULAppAPI.h"

#ifdef ANDROID
#include <android/log.h>
#endif
#ifdef XP_WIN
#include <windows.h>
# if defined(SendMessage)
#  undef SendMessage
# endif
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameMessageManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameMessageManager)
  uint32_t count = tmp->mListeners.Length();
  for (uint32_t i = 0; i < count; i++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mListeners[i] mStrongListener");
    cb.NoteXPCOMChild(tmp->mListeners[i].mStrongListener.get());
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildManagers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameMessageManager)
  tmp->mListeners.Clear();
  for (int32_t i = tmp->mChildManagers.Count(); i > 0; --i) {
    static_cast<nsFrameMessageManager*>(tmp->mChildManagers[i - 1])->
      Disconnect(false);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildManagers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentFrameMessageManager)

  /* nsFrameMessageManager implements nsIMessageSender and nsIMessageBroadcaster,
   * both of which descend from nsIMessageListenerManager. QI'ing to
   * nsIMessageListenerManager is therefore ambiguous and needs explicit casts
   * depending on which child interface applies. */
  NS_INTERFACE_MAP_ENTRY_AGGREGATED(nsIMessageListenerManager,
                                    (mIsBroadcaster ?
                                       static_cast<nsIMessageListenerManager*>(
                                         static_cast<nsIMessageBroadcaster*>(this)) :
                                       static_cast<nsIMessageListenerManager*>(
                                         static_cast<nsIMessageSender*>(this))))

  /* Message managers in child process implement nsIMessageSender and
     nsISyncMessageSender.  Message managers in the chrome process are
     either broadcasters (if they have subordinate/child message
     managers) or they're simple message senders. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISyncMessageSender, !mChrome)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMessageSender, !mChrome || !mIsBroadcaster)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMessageBroadcaster, mChrome && mIsBroadcaster)

  /* nsIContentFrameMessageManager is accessible only in TabChildGlobal. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIContentFrameMessageManager,
                                     !mChrome && !mIsProcessManager)

  /* Frame message managers (non-process message managers) support nsIFrameScriptLoader. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIFrameScriptLoader,
                                     mChrome && !mIsProcessManager)

  /* Message senders in the chrome process support nsIProcessChecker. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIProcessChecker,
                                     mChrome && !mIsBroadcaster)

  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(ChromeMessageBroadcaster,
                                                   mChrome && mIsBroadcaster)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(ChromeMessageSender,
                                                   mChrome && !mIsBroadcaster)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameMessageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameMessageManager)

template<ActorFlavorEnum>
struct DataBlobs
{ };

template<>
struct DataBlobs<Parent>
{
  typedef BlobTraits<Parent>::ProtocolType ProtocolType;

  static InfallibleTArray<ProtocolType*>& Blobs(ClonedMessageData& aData)
  {
    return aData.blobsParent();
  }

  static const InfallibleTArray<ProtocolType*>& Blobs(const ClonedMessageData& aData)
  {
    return aData.blobsParent();
  }
};

template<>
struct DataBlobs<Child>
{
  typedef BlobTraits<Child>::ProtocolType ProtocolType;

  static InfallibleTArray<ProtocolType*>& Blobs(ClonedMessageData& aData)
  {
    return aData.blobsChild();
  }

  static const InfallibleTArray<ProtocolType*>& Blobs(const ClonedMessageData& aData)
  {
    return aData.blobsChild();
  }
};

template<ActorFlavorEnum Flavor>
static bool
BuildClonedMessageData(typename BlobTraits<Flavor>::ConcreteContentManagerType* aManager,
                       const StructuredCloneData& aData,
                       ClonedMessageData& aClonedData)
{
  SerializedStructuredCloneBuffer& buffer = aClonedData.data();
  buffer.data = aData.mData;
  buffer.dataLength = aData.mDataLength;
  const nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = aData.mClosure.mBlobs;
  if (!blobs.IsEmpty()) {
    typedef typename BlobTraits<Flavor>::ProtocolType ProtocolType;
    InfallibleTArray<ProtocolType*>& blobList = DataBlobs<Flavor>::Blobs(aClonedData);
    uint32_t length = blobs.Length();
    blobList.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      Blob<Flavor>* protocolActor = aManager->GetOrCreateActorForBlob(blobs[i]);
      if (!protocolActor) {
        return false;
      }
      blobList.AppendElement(protocolActor);
    }
  }
  return true;
}

bool
MessageManagerCallback::BuildClonedMessageDataForParent(nsIContentParent* aParent,
                                                        const StructuredCloneData& aData,
                                                        ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Parent>(aParent, aData, aClonedData);
}

bool
MessageManagerCallback::BuildClonedMessageDataForChild(ContentChild* aChild,
                                                       const StructuredCloneData& aData,
                                                       ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Child>(aChild, aData, aClonedData);
}

template<ActorFlavorEnum Flavor>
static StructuredCloneData
UnpackClonedMessageData(const ClonedMessageData& aData)
{
  const SerializedStructuredCloneBuffer& buffer = aData.data();
  typedef typename BlobTraits<Flavor>::ProtocolType ProtocolType;
  const InfallibleTArray<ProtocolType*>& blobs = DataBlobs<Flavor>::Blobs(aData);
  StructuredCloneData cloneData;
  cloneData.mData = buffer.data;
  cloneData.mDataLength = buffer.dataLength;
  if (!blobs.IsEmpty()) {
    uint32_t length = blobs.Length();
    cloneData.mClosure.mBlobs.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      Blob<Flavor>* blob = static_cast<Blob<Flavor>*>(blobs[i]);
      MOZ_ASSERT(blob);
      nsCOMPtr<nsIDOMBlob> domBlob = blob->GetBlob();
      MOZ_ASSERT(domBlob);
      cloneData.mClosure.mBlobs.AppendElement(domBlob);
    }
  }
  return cloneData;
}

StructuredCloneData
mozilla::dom::ipc::UnpackClonedMessageDataForParent(const ClonedMessageData& aData)
{
  return UnpackClonedMessageData<Parent>(aData);
}

StructuredCloneData
mozilla::dom::ipc::UnpackClonedMessageDataForChild(const ClonedMessageData& aData)
{
  return UnpackClonedMessageData<Child>(aData);
}

bool
SameProcessCpowHolder::ToObject(JSContext* aCx, JS::MutableHandleObject aObjp)
{
  if (!mObj) {
    return true;
  }

  aObjp.set(mObj);
  return JS_WrapObject(aCx, aObjp);
}

// nsIMessageListenerManager

NS_IMETHODIMP
nsFrameMessageManager::AddMessageListener(const nsAString& aMessage,
                                          nsIMessageListener* aListener)
{
  nsCOMPtr<nsIAtom> message = do_GetAtom(aMessage);
  uint32_t len = mListeners.Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (mListeners[i].mMessage == message &&
      mListeners[i].mStrongListener == aListener) {
      return NS_OK;
    }
  }
  nsMessageListenerInfo* entry = mListeners.AppendElement();
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);
  entry->mMessage = message;
  entry->mStrongListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveMessageListener(const nsAString& aMessage,
                                             nsIMessageListener* aListener)
{
  nsCOMPtr<nsIAtom> message = do_GetAtom(aMessage);
  uint32_t len = mListeners.Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (mListeners[i].mMessage == message &&
      mListeners[i].mStrongListener == aListener) {
      mListeners.RemoveElementAt(i);
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::AddWeakMessageListener(const nsAString& aMessage,
                                              nsIMessageListener* aListener)
{
  nsWeakPtr weak = do_GetWeakReference(aListener);
  NS_ENSURE_TRUE(weak, NS_ERROR_NO_INTERFACE);

#ifdef DEBUG
  // It's technically possible that one object X could give two different
  // nsIWeakReference*'s when you do_GetWeakReference(X).  We really don't want
  // this to happen; it will break e.g. RemoveWeakMessageListener.  So let's
  // check that we're not getting ourselves into that situation.
  nsCOMPtr<nsISupports> canonical = do_QueryInterface(aListener);
  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    if (!mListeners[i].mWeakListener) {
      continue;
    }

    nsCOMPtr<nsISupports> otherCanonical =
      do_QueryReferent(mListeners[i].mWeakListener);
    MOZ_ASSERT((canonical == otherCanonical) ==
               (weak == mListeners[i].mWeakListener));
  }
#endif

  nsCOMPtr<nsIAtom> message = do_GetAtom(aMessage);
  uint32_t len = mListeners.Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (mListeners[i].mMessage == message &&
        mListeners[i].mWeakListener == weak) {
      return NS_OK;
    }
  }

  nsMessageListenerInfo* entry = mListeners.AppendElement();
  entry->mMessage = message;
  entry->mWeakListener = weak;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveWeakMessageListener(const nsAString& aMessage,
                                                 nsIMessageListener* aListener)
{
  nsWeakPtr weak = do_GetWeakReference(aListener);
  NS_ENSURE_TRUE(weak, NS_OK);

  nsCOMPtr<nsIAtom> message = do_GetAtom(aMessage);
  uint32_t len = mListeners.Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (mListeners[i].mMessage == message &&
        mListeners[i].mWeakListener == weak) {
      mListeners.RemoveElementAt(i);
      return NS_OK;
    }
  }

  return NS_OK;
}

// nsIFrameScriptLoader

NS_IMETHODIMP
nsFrameMessageManager::LoadFrameScript(const nsAString& aURL,
                                       bool aAllowDelayedLoad)
{
  if (aAllowDelayedLoad) {
    if (IsGlobal() || IsWindowLevel()) {
      // Cache for future windows or frames
      mPendingScripts.AppendElement(aURL);
    } else if (!mCallback) {
      // We're frame message manager, which isn't connected yet.
      mPendingScripts.AppendElement(aURL);
      return NS_OK;
    }
  }

  if (mCallback) {
#ifdef DEBUG_smaug
    printf("Will load %s \n", NS_ConvertUTF16toUTF8(aURL).get());
#endif
    NS_ENSURE_TRUE(mCallback->DoLoadFrameScript(aURL), NS_ERROR_FAILURE);
  }

  for (int32_t i = 0; i < mChildManagers.Count(); ++i) {
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mChildManagers[i]);
    if (mm) {
      // Use false here, so that child managers don't cache the script, which
      // is already cached in the parent.
      mm->LoadFrameScript(aURL, false);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveDelayedFrameScript(const nsAString& aURL)
{
  mPendingScripts.RemoveElement(aURL);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::GetDelayedFrameScripts(nsIDOMDOMStringList** aList)
{
  // Frame message managers may return an incomplete list because scripts
  // that were loaded after it was connected are not added to the list.
  if (!IsGlobal() && !IsWindowLevel()) {
    NS_WARNING("Cannot retrieve list of pending frame scripts for frame"
               "message managers as it may be incomplete");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsRefPtr<nsDOMStringList> scripts = new nsDOMStringList();

  for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
    scripts->Add(mPendingScripts[i]);
  }

  scripts.forget(aList);

  return NS_OK;
}

static bool
JSONCreator(const jschar* aBuf, uint32_t aLen, void* aData)
{
  nsAString* result = static_cast<nsAString*>(aData);
  result->Append(static_cast<const PRUnichar*>(aBuf),
                 static_cast<uint32_t>(aLen));
  return true;
}

static bool
GetParamsForMessage(JSContext* aCx,
                    const JS::Value& aJSON,
                    JSAutoStructuredCloneBuffer& aBuffer,
                    StructuredCloneClosure& aClosure)
{
  if (WriteStructuredClone(aCx, aJSON, aBuffer, aClosure)) {
    return true;
  }
  JS_ClearPendingException(aCx);

  // Not clonable, try JSON
  //XXX This is ugly but currently structured cloning doesn't handle
  //    properly cases when interface is implemented in JS and used
  //    as a dictionary.
  nsAutoString json;
  JS::Rooted<JS::Value> v(aCx, aJSON);
  NS_ENSURE_TRUE(JS_Stringify(aCx, &v, JS::NullPtr(), JS::NullHandleValue,
                              JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  JS::Rooted<JS::Value> val(aCx, JS::NullValue());
  NS_ENSURE_TRUE(JS_ParseJSON(aCx, static_cast<const jschar*>(json.get()),
                              json.Length(), &val), false);

  return WriteStructuredClone(aCx, val, aBuffer, aClosure);
}


// nsISyncMessageSender

static bool sSendingSyncMessage = false;

NS_IMETHODIMP
nsFrameMessageManager::SendSyncMessage(const nsAString& aMessageName,
                                       const JS::Value& aJSON,
                                       const JS::Value& aObjects,
                                       JSContext* aCx,
                                       uint8_t aArgc,
                                       JS::Value* aRetval)
{
  return SendMessage(aMessageName, aJSON, aObjects, aCx, aArgc, aRetval, true);
}

NS_IMETHODIMP
nsFrameMessageManager::SendRpcMessage(const nsAString& aMessageName,
                                      const JS::Value& aJSON,
                                      const JS::Value& aObjects,
                                      JSContext* aCx,
                                      uint8_t aArgc,
                                      JS::Value* aRetval)
{
  return SendMessage(aMessageName, aJSON, aObjects, aCx, aArgc, aRetval, false);
}

nsresult
nsFrameMessageManager::SendMessage(const nsAString& aMessageName,
                                   const JS::Value& aJSON,
                                   const JS::Value& aObjects,
                                   JSContext* aCx,
                                   uint8_t aArgc,
                                   JS::Value* aRetval,
                                   bool aIsSync)
{
  NS_ASSERTION(!IsGlobal(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!IsWindowLevel(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!mParentManager, "Should not have parent manager in content!");

  *aRetval = JSVAL_VOID;
  NS_ENSURE_TRUE(mCallback, NS_ERROR_NOT_INITIALIZED);

  if (sSendingSyncMessage && aIsSync) {
    // No kind of blocking send should be issued on top of a sync message.
    return NS_ERROR_UNEXPECTED;
  }

  StructuredCloneData data;
  JSAutoStructuredCloneBuffer buffer;
  if (aArgc >= 2 &&
      !GetParamsForMessage(aCx, aJSON, buffer, data.mClosure)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  data.mData = buffer.data();
  data.mDataLength = buffer.nbytes();

  JS::RootedObject objects(aCx);
  if (aArgc >= 3 && aObjects.isObject()) {
    objects = &aObjects.toObject();
  }

  InfallibleTArray<nsString> retval;

  sSendingSyncMessage |= aIsSync;
  bool rv = mCallback->DoSendBlockingMessage(aCx, aMessageName, data, objects, &retval, aIsSync);
  if (aIsSync) {
    sSendingSyncMessage = false;
  }

  if (!rv) {
    return NS_OK;
  }

  uint32_t len = retval.Length();
  JS::Rooted<JSObject*> dataArray(aCx, JS_NewArrayObject(aCx, len, nullptr));
  NS_ENSURE_TRUE(dataArray, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < len; ++i) {
    if (retval[i].IsEmpty()) {
      continue;
    }

    JS::Rooted<JS::Value> ret(aCx);
    if (!JS_ParseJSON(aCx, static_cast<const jschar*>(retval[i].get()),
                      retval[i].Length(), &ret)) {
      return NS_ERROR_UNEXPECTED;
    }
    NS_ENSURE_TRUE(JS_SetElement(aCx, dataArray, i, &ret),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  *aRetval = OBJECT_TO_JSVAL(dataArray);
  return NS_OK;
}

nsresult
nsFrameMessageManager::DispatchAsyncMessageInternal(JSContext* aCx,
                                                    const nsAString& aMessage,
                                                    const StructuredCloneData& aData,
                                                    JS::Handle<JSObject *> aCpows)
{
  if (mIsBroadcaster) {
    int32_t len = mChildManagers.Count();
    for (int32_t i = 0; i < len; ++i) {
      static_cast<nsFrameMessageManager*>(mChildManagers[i])->
         DispatchAsyncMessageInternal(aCx, aMessage, aData, aCpows);
    }
    return NS_OK;
  }

  NS_ENSURE_TRUE(mCallback, NS_ERROR_NOT_INITIALIZED);
  if (!mCallback->DoSendAsyncMessage(aCx, aMessage, aData, aCpows)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsFrameMessageManager::DispatchAsyncMessage(const nsAString& aMessageName,
                                            const JS::Value& aJSON,
                                            const JS::Value& aObjects,
                                            JSContext* aCx,
                                            uint8_t aArgc)
{
  StructuredCloneData data;
  JSAutoStructuredCloneBuffer buffer;

  if (aArgc >= 2 &&
      !GetParamsForMessage(aCx, aJSON, buffer, data.mClosure)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  JS::RootedObject objects(aCx);
  if (aArgc >= 3 && aObjects.isObject()) {
    objects = &aObjects.toObject();
  }

  data.mData = buffer.data();
  data.mDataLength = buffer.nbytes();

  return DispatchAsyncMessageInternal(aCx, aMessageName, data, objects);
}


// nsIMessageSender

NS_IMETHODIMP
nsFrameMessageManager::SendAsyncMessage(const nsAString& aMessageName,
                                        const JS::Value& aJSON,
                                        const JS::Value& aObjects,
                                        JSContext* aCx,
                                        uint8_t aArgc)
{
  return DispatchAsyncMessage(aMessageName, aJSON, aObjects, aCx, aArgc);
}


// nsIMessageBroadcaster

NS_IMETHODIMP
nsFrameMessageManager::BroadcastAsyncMessage(const nsAString& aMessageName,
                                             const JS::Value& aJSON,
                                             const JS::Value& aObjects,
                                             JSContext* aCx,
                                             uint8_t aArgc)
{
  return DispatchAsyncMessage(aMessageName, aJSON, aObjects, aCx, aArgc);
}

NS_IMETHODIMP
nsFrameMessageManager::GetChildCount(uint32_t* aChildCount)
{
  *aChildCount = static_cast<uint32_t>(mChildManagers.Count()); 
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::GetChildAt(uint32_t aIndex, 
                                  nsIMessageListenerManager** aMM)
{
  *aMM = nullptr;
  nsCOMPtr<nsIMessageListenerManager> mm =
    do_QueryInterface(mChildManagers.SafeObjectAt(static_cast<uint32_t>(aIndex)));
  mm.swap(*aMM);
  return NS_OK;
}


// nsIContentFrameMessageManager

NS_IMETHODIMP
nsFrameMessageManager::Dump(const nsAString& aStr)
{
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", NS_ConvertUTF16toUTF8(aStr).get());
#endif
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    OutputDebugStringW(PromiseFlatString(aStr).get());
  }
#endif
  fputs(NS_ConvertUTF16toUTF8(aStr).get(), stdout);
  fflush(stdout);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::PrivateNoteIntentionalCrash()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFrameMessageManager::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::GetDocShell(nsIDocShell** aDocShell)
{
  *aDocShell = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::Btoa(const nsAString& aBinaryData,
                            nsAString& aAsciiBase64String)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::Atob(const nsAString& aAsciiString,
                            nsAString& aBinaryData)
{
  return NS_OK;
}

// nsIProcessChecker

nsresult
nsFrameMessageManager::AssertProcessInternal(ProcessCheckerType aType,
                                             const nsAString& aCapability,
                                             bool* aValid)
{
  *aValid = false;

  // This API is only supported for message senders in the chrome process.
  if (!mChrome || mIsBroadcaster) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (!mCallback) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  switch (aType) {
    case PROCESS_CHECKER_PERMISSION:
      *aValid = mCallback->CheckPermission(aCapability);
      break;
    case PROCESS_CHECKER_MANIFEST_URL:
      *aValid = mCallback->CheckManifestURL(aCapability);
      break;
    case ASSERT_APP_HAS_PERMISSION:
      *aValid = mCallback->CheckAppHasPermission(aCapability);
      break;
    default:
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::AssertPermission(const nsAString& aPermission,
                                        bool* aHasPermission)
{
  return AssertProcessInternal(PROCESS_CHECKER_PERMISSION,
                               aPermission,
                               aHasPermission);
}

NS_IMETHODIMP
nsFrameMessageManager::AssertContainApp(const nsAString& aManifestURL,
                                        bool* aHasManifestURL)
{
  return AssertProcessInternal(PROCESS_CHECKER_MANIFEST_URL,
                               aManifestURL,
                               aHasManifestURL);
}

NS_IMETHODIMP
nsFrameMessageManager::AssertAppHasPermission(const nsAString& aPermission,
                                              bool* aHasPermission)
{
  return AssertProcessInternal(ASSERT_APP_HAS_PERMISSION,
                               aPermission,
                               aHasPermission);
}

NS_IMETHODIMP
nsFrameMessageManager::CheckAppHasStatus(unsigned short aStatus,
                                         bool* aHasStatus)
{
  *aHasStatus = false;

  // This API is only supported for message senders in the chrome process.
  if (!mChrome || mIsBroadcaster) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (!mCallback) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aHasStatus = mCallback->CheckAppHasStatus(aStatus);

  return NS_OK;
}

class MMListenerRemover
{
public:
  MMListenerRemover(nsFrameMessageManager* aMM)
    : mWasHandlingMessage(aMM->mHandlingMessage)
    , mMM(aMM)
  {
    mMM->mHandlingMessage = true;
  }
  ~MMListenerRemover()
  {
    if (!mWasHandlingMessage) {
      mMM->mHandlingMessage = false;
      if (mMM->mDisconnected) {
        mMM->mListeners.Clear();
      }
    }
  }

  bool mWasHandlingMessage;
  nsRefPtr<nsFrameMessageManager> mMM;
};


// nsIMessageListener

nsresult
nsFrameMessageManager::ReceiveMessage(nsISupports* aTarget,
                                      const nsAString& aMessage,
                                      bool aIsSync,
                                      const StructuredCloneData* aCloneData,
                                      CpowHolder* aCpows,
                                      InfallibleTArray<nsString>* aJSONRetVal)
{
  AutoSafeJSContext ctx;

  if (mListeners.Length()) {
    nsCOMPtr<nsIAtom> name = do_GetAtom(aMessage);
    MMListenerRemover lr(this);

    for (uint32_t i = 0; i < mListeners.Length(); ++i) {
      // Remove mListeners[i] if it's an expired weak listener.
      nsCOMPtr<nsISupports> weakListener;
      if (mListeners[i].mWeakListener) {
        weakListener = do_QueryReferent(mListeners[i].mWeakListener);
        if (!weakListener) {
          mListeners.RemoveElementAt(i--);
          continue;
        }
      }

      if (mListeners[i].mMessage == name) {
        nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS;
        if (weakListener) {
          wrappedJS = do_QueryInterface(weakListener);
        } else {
          wrappedJS = do_QueryInterface(mListeners[i].mStrongListener);
        }

        if (!wrappedJS) {
          continue;
        }
        JS::Rooted<JSObject*> object(ctx, wrappedJS->GetJSObject());
        if (!object) {
          continue;
        }
        JSAutoCompartment ac(ctx, object);

        // The parameter for the listener function.
        JS::Rooted<JSObject*> param(ctx,
          JS_NewObject(ctx, nullptr, nullptr, nullptr));
        NS_ENSURE_TRUE(param, NS_ERROR_OUT_OF_MEMORY);

        JS::Rooted<JS::Value> targetv(ctx);
        JS::Rooted<JSObject*> global(ctx, JS_GetGlobalForObject(ctx, object));
        nsContentUtils::WrapNative(ctx, global, aTarget, &targetv,
                                   nullptr, true);

        JS::RootedObject cpows(ctx);
        if (aCpows) {
          if (!aCpows->ToObject(ctx, &cpows)) {
            return NS_ERROR_UNEXPECTED;
          }
        }

        if (!cpows) {
          cpows = JS_NewObject(ctx, nullptr, nullptr, nullptr);
          if (!cpows) {
            return NS_ERROR_UNEXPECTED;
          }
        }

        JS::RootedValue cpowsv(ctx, JS::ObjectValue(*cpows));

        JS::Rooted<JS::Value> json(ctx, JS::NullValue());
        if (aCloneData && aCloneData->mDataLength &&
            !ReadStructuredClone(ctx, *aCloneData, json.address())) {
          JS_ClearPendingException(ctx);
          return NS_OK;
        }
        JS::Rooted<JSString*> jsMessage(ctx,
          JS_NewUCStringCopyN(ctx,
                              static_cast<const jschar*>(aMessage.BeginReading()),
                              aMessage.Length()));
        NS_ENSURE_TRUE(jsMessage, NS_ERROR_OUT_OF_MEMORY);
        JS_DefineProperty(ctx, param, "target", targetv, nullptr, nullptr, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "name",
                          STRING_TO_JSVAL(jsMessage), nullptr, nullptr, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "sync",
                          BOOLEAN_TO_JSVAL(aIsSync), nullptr, nullptr, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "json", json, nullptr, nullptr, JSPROP_ENUMERATE); // deprecated
        JS_DefineProperty(ctx, param, "data", json, nullptr, nullptr, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "objects", cpowsv, nullptr, nullptr, JSPROP_ENUMERATE);

        JS::Rooted<JS::Value> thisValue(ctx, JS::UndefinedValue());

        JS::Rooted<JS::Value> funval(ctx);
        if (JS_ObjectIsCallable(ctx, object)) {
          // If the listener is a JS function:
          funval.setObject(*object);

          // A small hack to get 'this' value right on content side where
          // messageManager is wrapped in TabChildGlobal.
          nsCOMPtr<nsISupports> defaultThisValue;
          if (mChrome) {
            defaultThisValue = do_QueryObject(this);
          } else {
            defaultThisValue = aTarget;
          }
          JS::Rooted<JSObject*> global(ctx, JS_GetGlobalForObject(ctx, object));
          nsContentUtils::WrapNative(ctx, global, defaultThisValue,
                                     &thisValue, nullptr, true);
        } else {
          // If the listener is a JS object which has receiveMessage function:
          if (!JS_GetProperty(ctx, object, "receiveMessage", &funval) ||
              !funval.isObject())
            return NS_ERROR_UNEXPECTED;

          // Check if the object is even callable.
          NS_ENSURE_STATE(JS_ObjectIsCallable(ctx, &funval.toObject()));
          thisValue.setObject(*object);
        }

        JS::Rooted<JS::Value> rval(ctx, JSVAL_VOID);
        JS::Rooted<JS::Value> argv(ctx, JS::ObjectValue(*param));

        {
          JS::Rooted<JSObject*> thisObject(ctx, thisValue.toObjectOrNull());

          JSAutoCompartment tac(ctx, thisObject);
          if (!JS_WrapValue(ctx, argv.address())) {
            return NS_ERROR_UNEXPECTED;
          }

          JS_CallFunctionValue(ctx, thisObject,
                               funval, 1, argv.address(), rval.address());
          if (aJSONRetVal) {
            nsString json;
            if (JS_Stringify(ctx, &rval, JS::NullPtr(), JS::NullHandleValue,
                             JSONCreator, &json)) {
              aJSONRetVal->AppendElement(json);
            }
          }
        }
      }
    }
  }
  nsRefPtr<nsFrameMessageManager> kungfuDeathGrip = mParentManager;
  return mParentManager ? mParentManager->ReceiveMessage(aTarget, aMessage,
                                                         aIsSync, aCloneData,
                                                         aCpows,
                                                         aJSONRetVal) : NS_OK;
}

void
nsFrameMessageManager::AddChildManager(nsFrameMessageManager* aManager,
                                       bool aLoadScripts)
{
  mChildManagers.AppendObject(aManager);
  if (aLoadScripts) {
    nsRefPtr<nsFrameMessageManager> kungfuDeathGrip = this;
    nsRefPtr<nsFrameMessageManager> kungfuDeathGrip2 = aManager;
    // We have parent manager if we're a window message manager.
    // In that case we want to load the pending scripts from global
    // message manager.
    if (mParentManager) {
      nsRefPtr<nsFrameMessageManager> globalMM = mParentManager;
      for (uint32_t i = 0; i < globalMM->mPendingScripts.Length(); ++i) {
        aManager->LoadFrameScript(globalMM->mPendingScripts[i], false);
      }
    }
    for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
      aManager->LoadFrameScript(mPendingScripts[i], false);
    }
  }
}

void
nsFrameMessageManager::SetCallback(MessageManagerCallback* aCallback, bool aLoadScripts)
{
  NS_ASSERTION(!mIsBroadcaster || !mCallback,
               "Broadcasters cannot have callbacks!");
  if (aCallback && mCallback != aCallback) {
    mCallback = aCallback;
    if (mOwnsCallback) {
      mOwnedCallback = aCallback;
    }
    // First load global scripts by adding this to parent manager.
    if (mParentManager) {
      mParentManager->AddChildManager(this, aLoadScripts);
    }
    if (aLoadScripts) {
      for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
        LoadFrameScript(mPendingScripts[i], false);
      }
    }
  }
}

void
nsFrameMessageManager::RemoveFromParent()
{
  if (mParentManager) {
    mParentManager->RemoveChildManager(this);
  }
  mParentManager = nullptr;
  mCallback = nullptr;
  mOwnedCallback = nullptr;
}

void
nsFrameMessageManager::Disconnect(bool aRemoveFromParent)
{
  if (!mDisconnected) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
       obs->NotifyObservers(NS_ISUPPORTS_CAST(nsIContentFrameMessageManager*, this),
                            "message-manager-disconnect", nullptr);
    }
  }
  if (mParentManager && aRemoveFromParent) {
    mParentManager->RemoveChildManager(this);
  }
  mDisconnected = true;
  mParentManager = nullptr;
  mCallback = nullptr;
  mOwnedCallback = nullptr;
  if (!mHandlingMessage) {
    mListeners.Clear();
  }
}

namespace {

struct MessageManagerReferentCount {
  MessageManagerReferentCount() : strong(0), weakAlive(0), weakDead(0) {}
  size_t strong;
  size_t weakAlive;
  size_t weakDead;
  nsCOMArray<nsIAtom> suspectMessages;
  nsDataHashtable<nsPtrHashKey<nsIAtom>, uint32_t> messageCounter;
};

} // anonymous namespace

namespace mozilla {
namespace dom {

class MessageManagerReporter MOZ_FINAL : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER
protected:
  static const size_t kSuspectReferentCount = 300;
  void CountReferents(nsFrameMessageManager* aMessageManager,
                      MessageManagerReferentCount* aReferentCount);
};

NS_IMPL_ISUPPORTS1(MessageManagerReporter, nsIMemoryReporter)

void
MessageManagerReporter::CountReferents(nsFrameMessageManager* aMessageManager,
                                       MessageManagerReferentCount* aReferentCount)
{
  for (uint32_t i = 0; i < aMessageManager->mListeners.Length(); i++) {
    const nsMessageListenerInfo& listenerInfo = aMessageManager->mListeners[i];

    if (listenerInfo.mWeakListener) {
      nsCOMPtr<nsISupports> referent =
        do_QueryReferent(listenerInfo.mWeakListener);
      if (referent) {
        aReferentCount->weakAlive++;
      } else {
        aReferentCount->weakDead++;
      }
    } else {
      aReferentCount->strong++;
    }

    uint32_t oldCount = 0;
    aReferentCount->messageCounter.Get(listenerInfo.mMessage, &oldCount);
    uint32_t currentCount = oldCount + 1;
    aReferentCount->messageCounter.Put(listenerInfo.mMessage, currentCount);

    // Keep track of messages that have a suspiciously large
    // number of referents (symptom of leak).
    if (currentCount == kSuspectReferentCount) {
      aReferentCount->suspectMessages.AppendElement(listenerInfo.mMessage);
    }
  }

  // Add referent count in child managers because the listeners
  // participate in messages dispatched from parent message manager.
  for (uint32_t i = 0; i < aMessageManager->mChildManagers.Length(); i++) {
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(aMessageManager->mChildManagers[i]);
    CountReferents(mm, aReferentCount);
  }
}

NS_IMETHODIMP
MessageManagerReporter::GetName(nsACString& aName)
{
  aName.AssignLiteral("message-manager");
  return NS_OK;
}

static nsresult
ReportReferentCount(const char* aManagerType,
                    const MessageManagerReferentCount& aReferentCount,
                    nsIMemoryReporterCallback* aCb,
                    nsISupports* aClosure)
{
#define REPORT(_path, _amount, _desc)                                         \
    do {                                                                      \
      nsresult rv;                                                            \
      rv = aCb->Callback(EmptyCString(), _path,                               \
                         nsIMemoryReporter::KIND_OTHER,                       \
                         nsIMemoryReporter::UNITS_COUNT, _amount,             \
                         _desc, aClosure);                                    \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
    } while (0)

  REPORT(nsPrintfCString("message-manager/referent/%s/strong", aManagerType),
         aReferentCount.strong,
         nsPrintfCString("The number of strong referents held by the message "
                         "manager in the %s manager.", aManagerType));
  REPORT(nsPrintfCString("message-manager/referent/%s/weak/alive", aManagerType),
         aReferentCount.weakAlive,
         nsPrintfCString("The number of weak referents that are still alive "
                         "held by the message manager in the %s manager.",
                         aManagerType));
  REPORT(nsPrintfCString("message-manager/referent/%s/weak/dead", aManagerType),
         aReferentCount.weakDead,
         nsPrintfCString("The number of weak referents that are dead "
                         "held by the message manager in the %s manager.",
                         aManagerType));

  for (uint32_t i = 0; i < aReferentCount.suspectMessages.Length(); i++) {
    uint32_t totalReferentCount = 0;
    aReferentCount.messageCounter.Get(aReferentCount.suspectMessages[i],
                                      &totalReferentCount);
    nsAtomCString suspect(aReferentCount.suspectMessages[i]);
    REPORT(nsPrintfCString("message-manager-suspect/%s/referent(message=%s)",
                           aManagerType, suspect.get()), totalReferentCount,
           nsPrintfCString("A message in the %s message manager with a "
                           "suspiciously large number of referents (symptom "
                           "of a leak).", aManagerType));
  }

#undef REPORT

  return NS_OK;
}

NS_IMETHODIMP
MessageManagerReporter::CollectReports(nsIMemoryReporterCallback* aCb,
                                       nsISupports* aClosure)
{
  nsresult rv;

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    nsCOMPtr<nsIMessageBroadcaster> globalmm =
      do_GetService("@mozilla.org/globalmessagemanager;1");
    if (globalmm) {
      nsRefPtr<nsFrameMessageManager> mm =
        static_cast<nsFrameMessageManager*>(globalmm.get());
      MessageManagerReferentCount count;
      CountReferents(mm, &count);
      rv = ReportReferentCount("global-manager", count, aCb, aClosure);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (nsFrameMessageManager::sParentProcessManager) {
    MessageManagerReferentCount count;
    CountReferents(nsFrameMessageManager::sParentProcessManager, &count);
    rv = ReportReferentCount("parent-process-manager", count, aCb, aClosure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (nsFrameMessageManager::sChildProcessManager) {
    MessageManagerReferentCount count;
    CountReferents(nsFrameMessageManager::sChildProcessManager, &count);
    rv = ReportReferentCount("child-process-manager", count, aCb, aClosure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewGlobalMessageManager(nsIMessageBroadcaster** aResult)
{
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default,
                 NS_ERROR_NOT_AVAILABLE);
  nsFrameMessageManager* mm = new nsFrameMessageManager(nullptr,
                                                        nullptr,
                                                        MM_CHROME | MM_GLOBAL | MM_BROADCASTER);
  NS_RegisterMemoryReporter(new MessageManagerReporter());
  return CallQueryInterface(mm, aResult);
}

nsDataHashtable<nsStringHashKey, nsFrameJSScriptExecutorHolder*>*
  nsFrameScriptExecutor::sCachedScripts = nullptr;
nsScriptCacheCleaner* nsFrameScriptExecutor::sScriptCacheCleaner = nullptr;

void
nsFrameScriptExecutor::DidCreateGlobal()
{
  NS_ASSERTION(mGlobal, "Should have mGlobal!");
  if (!sCachedScripts) {
    sCachedScripts =
      new nsDataHashtable<nsStringHashKey, nsFrameJSScriptExecutorHolder*>;

    nsRefPtr<nsScriptCacheCleaner> scriptCacheCleaner =
      new nsScriptCacheCleaner();
    scriptCacheCleaner.forget(&sScriptCacheCleaner);
  }
}

static PLDHashOperator
CachedScriptUnrooter(const nsAString& aKey,
                       nsFrameJSScriptExecutorHolder*& aData,
                       void* aUserArg)
{
  JSContext* cx = static_cast<JSContext*>(aUserArg);
  JS_RemoveScriptRoot(cx, &(aData->mScript));
  delete aData;
  return PL_DHASH_REMOVE;
}

// static
void
nsFrameScriptExecutor::Shutdown()
{
  if (sCachedScripts) {
    AutoSafeJSContext cx;
    NS_ASSERTION(sCachedScripts != nullptr, "Need cached scripts");
    sCachedScripts->Enumerate(CachedScriptUnrooter, cx);

    delete sCachedScripts;
    sCachedScripts = nullptr;

    nsRefPtr<nsScriptCacheCleaner> scriptCacheCleaner;
    scriptCacheCleaner.swap(sScriptCacheCleaner);
  }
}

void
nsFrameScriptExecutor::LoadFrameScriptInternal(const nsAString& aURL)
{
  if (!mGlobal || !sCachedScripts) {
    return;
  }

  nsFrameJSScriptExecutorHolder* holder = sCachedScripts->Get(aURL);
  if (!holder) {
    TryCacheLoadAndCompileScript(aURL, EXECUTE_IF_CANT_CACHE);
    holder = sCachedScripts->Get(aURL);
  }

  if (holder) {
    AutoSafeJSContext cx;
    JS::Rooted<JSObject*> global(cx, mGlobal->GetJSObject());
    if (global) {
      JSAutoCompartment ac(cx, global);
      (void) JS_ExecuteScript(cx, global, holder->mScript, nullptr);
    }
  }
}

void
nsFrameScriptExecutor::TryCacheLoadAndCompileScript(const nsAString& aURL,
                                                    CacheFailedBehavior aBehavior)
{
  nsCString url = NS_ConvertUTF16toUTF8(aURL);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), url);
  if (NS_FAILED(rv)) {
    return;
  }

  bool hasFlags;
  rv = NS_URIChainHasFlags(uri,
                           nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                           &hasFlags);
  if (NS_FAILED(rv) || !hasFlags) {
    NS_WARNING("Will not load a frame script!");
    return;
  }

  nsCOMPtr<nsIChannel> channel;
  NS_NewChannel(getter_AddRefs(channel), uri);
  if (!channel) {
    return;
  }

  nsCOMPtr<nsIInputStream> input;
  channel->Open(getter_AddRefs(input));
  nsString dataString;
  uint64_t avail64 = 0;
  if (input && NS_SUCCEEDED(input->Available(&avail64)) && avail64) {
    if (avail64 > UINT32_MAX) {
      return;
    }
    nsCString buffer;
    uint32_t avail = (uint32_t)std::min(avail64, (uint64_t)UINT32_MAX);
    if (NS_FAILED(NS_ReadInputStreamToString(input, buffer, avail))) {
      return;
    }
    nsScriptLoader::ConvertToUTF16(channel, (uint8_t*)buffer.get(), avail,
                                   EmptyString(), nullptr, dataString);
  }

  if (!dataString.IsEmpty()) {
    AutoSafeJSContext cx;
    JS::Rooted<JSObject*> global(cx, mGlobal->GetJSObject());
    if (global) {
      JSAutoCompartment ac(cx, global);
      JS::CompileOptions options(cx);
      options.setNoScriptRval(true)
             .setFileAndLine(url.get(), 1)
             .setPrincipals(nsJSPrincipals::get(mPrincipal));
      JS::RootedObject empty(cx, nullptr);
      JS::Rooted<JSScript*> script(cx,
        JS::Compile(cx, empty, options, dataString.get(),
                    dataString.Length()));

      if (script) {
        nsAutoCString scheme;
        uri->GetScheme(scheme);
        // We don't cache data: scripts!
        if (!scheme.EqualsLiteral("data")) {
          nsFrameJSScriptExecutorHolder* holder =
            new nsFrameJSScriptExecutorHolder(script);
          // Root the object also for caching.
          JS_AddNamedScriptRoot(cx, &(holder->mScript),
                                "Cached message manager script");
          sCachedScripts->Put(aURL, holder);
        } else if (aBehavior == EXECUTE_IF_CANT_CACHE) {
          (void) JS_ExecuteScript(cx, global, script, nullptr);
        }
      }
    }
  }
}

bool
nsFrameScriptExecutor::InitTabChildGlobalInternal(nsISupports* aScope,
                                                  const nsACString& aID)
{

  nsCOMPtr<nsIJSRuntimeService> runtimeSvc =
    do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
  NS_ENSURE_TRUE(runtimeSvc, false);

  JSRuntime* rt = nullptr;
  runtimeSvc->GetRuntime(&rt);
  NS_ENSURE_TRUE(rt, false);

  AutoSafeJSContext cx;
  nsContentUtils::GetSecurityManager()->GetSystemPrincipal(getter_AddRefs(mPrincipal));

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  const uint32_t flags = nsIXPConnect::INIT_JS_STANDARD_CLASSES;

  JS::CompartmentOptions options;
  options.setZone(JS::SystemZone)
         .setVersion(JSVERSION_LATEST);

  nsresult rv =
    xpc->InitClassesWithNewWrappedGlobal(cx, aScope, mPrincipal,
                                         flags, options, getter_AddRefs(mGlobal));
  NS_ENSURE_SUCCESS(rv, false);


  JS::Rooted<JSObject*> global(cx, mGlobal->GetJSObject());
  NS_ENSURE_TRUE(global, false);

  // Set the location information for the new global, so that tools like
  // about:memory may use that information.
  xpc::SetLocationForGlobal(global, aID);

  DidCreateGlobal();
  return true;
}

NS_IMPL_ISUPPORTS1(nsScriptCacheCleaner, nsIObserver)

nsFrameMessageManager* nsFrameMessageManager::sChildProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sParentProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sSameProcessParentManager = nullptr;
nsTArray<nsCOMPtr<nsIRunnable> >* nsFrameMessageManager::sPendingSameProcessAsyncMessages = nullptr;

class nsAsyncMessageToSameProcessChild : public nsRunnable
{
public:
  nsAsyncMessageToSameProcessChild(JSContext* aCx,
                                   const nsAString& aMessage,
                                   const StructuredCloneData& aData,
                                   JS::Handle<JSObject *> aCpows)
    : mRuntime(js::GetRuntime(aCx)),
      mMessage(aMessage),
      mCpows(aCpows)
  {
    if (aData.mDataLength && !mData.copy(aData.mData, aData.mDataLength)) {
      NS_RUNTIMEABORT("OOM");
    }
    if (mCpows && !js_AddObjectRoot(mRuntime, &mCpows)) {
      NS_RUNTIMEABORT("OOM");
    }
    mClosure = aData.mClosure;
  }

  ~nsAsyncMessageToSameProcessChild()
  {
    if (mCpows) {
      JS_RemoveObjectRootRT(mRuntime, &mCpows);
    }
  }

  NS_IMETHOD Run()
  {
    if (nsFrameMessageManager::sChildProcessManager) {
      StructuredCloneData data;
      data.mData = mData.data();
      data.mDataLength = mData.nbytes();
      data.mClosure = mClosure;

      SameProcessCpowHolder cpows(mRuntime, JS::Handle<JSObject *>::fromMarkedLocation(&mCpows));

      nsRefPtr<nsFrameMessageManager> ppm = nsFrameMessageManager::sChildProcessManager;
      ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()), mMessage,
                          false, &data, &cpows, nullptr);
    }
    return NS_OK;
  }
  JSRuntime* mRuntime;
  nsString mMessage;
  JSAutoStructuredCloneBuffer mData;
  StructuredCloneClosure mClosure;
  JSObject* mCpows;
};


/**
 * Send messages to an imaginary child process in a single-process scenario.
 */
class SameParentProcessMessageManagerCallback : public MessageManagerCallback
{
public:
  SameParentProcessMessageManagerCallback()
  {
    MOZ_COUNT_CTOR(SameParentProcessMessageManagerCallback);
  }
  virtual ~SameParentProcessMessageManagerCallback()
  {
    MOZ_COUNT_DTOR(SameParentProcessMessageManagerCallback);
  }

  virtual bool DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows)
  {
    nsRefPtr<nsIRunnable> ev =
      new nsAsyncMessageToSameProcessChild(aCx, aMessage, aData, aCpows);
    NS_DispatchToCurrentThread(ev);
    return true;
  }

  bool CheckPermission(const nsAString& aPermission)
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }

  bool CheckManifestURL(const nsAString& aManifestURL)
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }

  bool CheckAppHasPermission(const nsAString& aPermission)
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }
};


/**
 * Send messages to the parent process.
 */
class ChildProcessMessageManagerCallback : public MessageManagerCallback
{
public:
  ChildProcessMessageManagerCallback()
  {
    MOZ_COUNT_CTOR(ChildProcessMessageManagerCallback);
  }
  virtual ~ChildProcessMessageManagerCallback()
  {
    MOZ_COUNT_DTOR(ChildProcessMessageManagerCallback);
  }

  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                     const nsAString& aMessage,
                                     const mozilla::dom::StructuredCloneData& aData,
                                     JS::Handle<JSObject *> aCpows,
                                     InfallibleTArray<nsString>* aJSONRetVal,
                                     bool aIsSync) MOZ_OVERRIDE
  {
    mozilla::dom::ContentChild* cc =
      mozilla::dom::ContentChild::GetSingleton();
    if (!cc) {
      return true;
    }
    ClonedMessageData data;
    if (!BuildClonedMessageDataForChild(cc, aData, data)) {
      return false;
    }
    InfallibleTArray<mozilla::jsipc::CpowEntry> cpows;
    if (!cc->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
      return false;
    }
    if (aIsSync) {
      return cc->SendSyncMessage(nsString(aMessage), data, cpows, aJSONRetVal);
    }
    return cc->CallRpcMessage(nsString(aMessage), data, cpows, aJSONRetVal);
  }

  virtual bool DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows) MOZ_OVERRIDE
  {
    mozilla::dom::ContentChild* cc =
      mozilla::dom::ContentChild::GetSingleton();
    if (!cc) {
      return true;
    }
    ClonedMessageData data;
    if (!BuildClonedMessageDataForChild(cc, aData, data)) {
      return false;
    }
    InfallibleTArray<mozilla::jsipc::CpowEntry> cpows;
    if (!cc->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
      return false;
    }
    return cc->SendAsyncMessage(nsString(aMessage), data, cpows);
  }

};


class nsAsyncMessageToSameProcessParent : public nsRunnable
{
public:
  nsAsyncMessageToSameProcessParent(JSContext* aCx,
                                    const nsAString& aMessage,
                                    const StructuredCloneData& aData,
                                    JS::Handle<JSObject *> aCpows)
    : mRuntime(js::GetRuntime(aCx)),
      mMessage(aMessage),
      mCpows(aCpows)
  {
    if (aData.mDataLength && !mData.copy(aData.mData, aData.mDataLength)) {
      NS_RUNTIMEABORT("OOM");
    }
    if (mCpows && !js_AddObjectRoot(mRuntime, &mCpows)) {
      NS_RUNTIMEABORT("OOM");
    }
    mClosure = aData.mClosure;
  }

  ~nsAsyncMessageToSameProcessParent()
  {
    if (mCpows) {
      JS_RemoveObjectRootRT(mRuntime, &mCpows);
    }
  }

  NS_IMETHOD Run()
  {
    if (nsFrameMessageManager::sPendingSameProcessAsyncMessages) {
      nsFrameMessageManager::sPendingSameProcessAsyncMessages->RemoveElement(this);
    }
    if (nsFrameMessageManager::sSameProcessParentManager) {
      StructuredCloneData data;
      data.mData = mData.data();
      data.mDataLength = mData.nbytes();
      data.mClosure = mClosure;

      SameProcessCpowHolder cpows(mRuntime, JS::Handle<JSObject *>::fromMarkedLocation(&mCpows));

      nsRefPtr<nsFrameMessageManager> ppm =
        nsFrameMessageManager::sSameProcessParentManager;
      ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                          mMessage, false, &data, &cpows, nullptr);
     }
     return NS_OK;
  }
  JSRuntime* mRuntime;
  nsString mMessage;
  JSAutoStructuredCloneBuffer mData;
  StructuredCloneClosure mClosure;
  JSObject* mCpows;
};

/**
 * Send messages to the imaginary parent process in a single-process scenario.
 */
class SameChildProcessMessageManagerCallback : public MessageManagerCallback
{
public:
  SameChildProcessMessageManagerCallback()
  {
    MOZ_COUNT_CTOR(SameChildProcessMessageManagerCallback);
  }
  virtual ~SameChildProcessMessageManagerCallback()
  {
    MOZ_COUNT_DTOR(SameChildProcessMessageManagerCallback);
  }

  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                     const nsAString& aMessage,
                                     const mozilla::dom::StructuredCloneData& aData,
                                     JS::Handle<JSObject *> aCpows,
                                     InfallibleTArray<nsString>* aJSONRetVal,
                                     bool aIsSync) MOZ_OVERRIDE
  {
    nsTArray<nsCOMPtr<nsIRunnable> > asyncMessages;
    if (nsFrameMessageManager::sPendingSameProcessAsyncMessages) {
      asyncMessages.SwapElements(*nsFrameMessageManager::sPendingSameProcessAsyncMessages);
      uint32_t len = asyncMessages.Length();
      for (uint32_t i = 0; i < len; ++i) {
        nsCOMPtr<nsIRunnable> async = asyncMessages[i];
        async->Run();
      }
    }
    if (nsFrameMessageManager::sSameProcessParentManager) {
      SameProcessCpowHolder cpows(js::GetRuntime(aCx), aCpows);
      nsRefPtr<nsFrameMessageManager> ppm = nsFrameMessageManager::sSameProcessParentManager;
      ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()), aMessage,
                          true, &aData, &cpows, aJSONRetVal);
    }
    return true;
  }

  virtual bool DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows)
  {
    if (!nsFrameMessageManager::sPendingSameProcessAsyncMessages) {
      nsFrameMessageManager::sPendingSameProcessAsyncMessages = new nsTArray<nsCOMPtr<nsIRunnable> >;
    }
    nsCOMPtr<nsIRunnable> ev =
      new nsAsyncMessageToSameProcessParent(aCx, aMessage, aData, aCpows);
    nsFrameMessageManager::sPendingSameProcessAsyncMessages->AppendElement(ev);
    NS_DispatchToCurrentThread(ev);
    return true;
  }

};


// This creates the global parent process message manager.
nsresult
NS_NewParentProcessMessageManager(nsIMessageBroadcaster** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sParentProcessManager,
               "Re-creating sParentProcessManager");
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default,
                 NS_ERROR_NOT_AVAILABLE);
  nsRefPtr<nsFrameMessageManager> mm = new nsFrameMessageManager(nullptr,
                                                                 nullptr,
                                                                 MM_CHROME | MM_PROCESSMANAGER | MM_BROADCASTER);
  nsFrameMessageManager::sParentProcessManager = mm;
  nsFrameMessageManager::NewProcessMessageManager(nullptr); // Create same process message manager.
  return CallQueryInterface(mm, aResult);
}


nsFrameMessageManager*
nsFrameMessageManager::NewProcessMessageManager(nsIContentParent* aProcess)
{
  if (!nsFrameMessageManager::sParentProcessManager) {
     nsCOMPtr<nsIMessageBroadcaster> dummy =
       do_GetService("@mozilla.org/parentprocessmessagemanager;1");
  }

  MOZ_ASSERT(nsFrameMessageManager::sParentProcessManager,
             "parent process manager not created");
  nsFrameMessageManager* mm;
  if (aProcess) {
    mm = new nsFrameMessageManager(aProcess,
                                   nsFrameMessageManager::sParentProcessManager,
                                   MM_CHROME | MM_PROCESSMANAGER);
  } else {
    mm = new nsFrameMessageManager(new SameParentProcessMessageManagerCallback(),
                                   nsFrameMessageManager::sParentProcessManager,
                                   MM_CHROME | MM_PROCESSMANAGER | MM_OWNSCALLBACK);
    sSameProcessParentManager = mm;
  }
  return mm;
}

nsresult
NS_NewChildProcessMessageManager(nsISyncMessageSender** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sChildProcessManager,
               "Re-creating sChildProcessManager");

  MessageManagerCallback* cb;
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    cb = new SameChildProcessMessageManagerCallback();
  } else {
    cb = new ChildProcessMessageManagerCallback();
    NS_RegisterMemoryReporter(new MessageManagerReporter());
  }
  nsFrameMessageManager* mm = new nsFrameMessageManager(cb,
                                                        nullptr,
                                                        MM_PROCESSMANAGER | MM_OWNSCALLBACK);
  nsFrameMessageManager::sChildProcessManager = mm;
  return CallQueryInterface(mm, aResult);
}

bool
nsFrameMessageManager::MarkForCC()
{
  uint32_t len = mListeners.Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (mListeners[i].mStrongListener) {
      xpc_TryUnmarkWrappedGrayObject(mListeners[i].mStrongListener);
    }
  }
  if (mRefCnt.IsPurple()) {
    mRefCnt.RemovePurple();
  }
  return true;
}
