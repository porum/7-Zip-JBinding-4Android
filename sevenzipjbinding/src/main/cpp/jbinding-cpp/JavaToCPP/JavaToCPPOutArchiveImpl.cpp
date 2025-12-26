#include "SevenZipJBinding.h"

#include "JNITools.h"

#include "net_sf_sevenzipjbinding_impl_OutArchiveImpl.h"
#include "CodecTools.h"

#include "CPPToJava/CPPToJavaOutStream.h"
#include "CPPToJava/CPPToJavaArchiveUpdateCallback.h"

#include "UnicodeHelper.h"
#include "UserTrace.h"

// void updateItemsNative(int archiveFormatIndex, IOutStream outStream, int numberOfItems,
//                        IArchiveUpdateCallback archiveUpdateCallback)

static JBindingSession & GetJBindingSession(JNIEnv * env, jobject thiz) {
    jlong pointer = jni::OutArchiveImpl::jbindingSession_Get(env, thiz);
    FATALIF(!pointer, "GetJBindingSession() : pointer == NULL");

    return *((JBindingSession *) (void *) (size_t) pointer);
}

static IOutArchive * GetArchive(JNIEnv * env, jobject thiz) {
    jlong pointer = jni::OutArchiveImpl::sevenZipArchiveInstance_Get(env, thiz);
    FATALIF(!pointer, "GetArchive() : pointer == NULL");

    return (IOutArchive *) (void *) (size_t) pointer;
}

/*
 * Class:     net_sf_sevenzipjbinding_impl_OutArchiveImpl
 * Method:    updateItemsNative
 * Signature: (ILnet/sf/sevenzipjbinding/ISequentialOutStream;ILnet/sf/sevenzipjbinding/IArchiveUpdateCallback;Z)V
 */
JBINDING_JNIEXPORT void JNICALL Java_net_sf_sevenzipjbinding_impl_OutArchiveImpl_nativeUpdateItems(
                                                                                                   JNIEnv * env,
                                                                                                   jobject thiz,
                                                                                                   jobject outStream,
                                                                                                   jint numberOfItems,
                                                                                                   jobject archiveUpdateCallback) {
    TRACE("OutArchiveImpl.updateItemsNative()");

    JBindingSession & jbindingSession = GetJBindingSession(env, thiz);
    JNINativeCallContext jniNativeCallContext(jbindingSession, env);
    JNIEnvInstance jniEnvInstance(jbindingSession, jniNativeCallContext, env);

	CMyComPtr<IOutArchive> outArchive(GetArchive(env, thiz));

    jobject archiveFormat = jni::OutArchiveImpl::archiveFormat_Get(env, thiz);
    int archiveFormatIndex = codecTools.getArchiveFormatIndex(jniEnvInstance, archiveFormat);
    jboolean isInArchiveAttached = jni::OutArchiveImpl::inArchive_Get(env, thiz) != NULL;

	if (isUserTraceEnabled(jniEnvInstance, thiz)) {
	    if (isInArchiveAttached) {
	        userTrace(jniEnvInstance, thiz, UString(L"Updating ") << (UInt32)numberOfItems << L" items");
	    } else {
	        userTrace(jniEnvInstance, thiz, UString(L"Compressing ") << (UInt32)numberOfItems << L" items");
	    }
	}

	CMyComPtr<IOutStream> cppToJavaOutStream = new CPPToJavaOutStream(jbindingSession, env,
			outStream);

	CPPToJavaArchiveUpdateCallback * cppToJavaArchiveUpdateCallback = new CPPToJavaArchiveUpdateCallback(
	        jbindingSession, env,
	        archiveUpdateCallback,
	        isInArchiveAttached,
	        archiveFormatIndex,
	        thiz);

	CMyComPtr<IArchiveUpdateCallback> cppToJavaArchiveUpdateCallbackPtr = cppToJavaArchiveUpdateCallback;

	HRESULT hresult  = outArchive->UpdateItems(cppToJavaOutStream, numberOfItems,
			cppToJavaArchiveUpdateCallback);
	if (hresult) {
		jniEnvInstance.reportError(hresult, "Error creating '%S' archive with %i items",
				(const wchar_t*) codecTools.codecs.Formats[archiveFormatIndex].Name,
				(int) numberOfItems);
	}

	cppToJavaArchiveUpdateCallback->freeOutItem(jniEnvInstance);
}

/*
 * Class:     net_sf_sevenzipjbinding_impl_OutArchiveImpl
 * Method:    nativeSetProperties
 * Signature: ([Ljava/lang/String;[Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_net_sf_sevenzipjbinding_impl_OutArchiveImpl_nativeSetProperties
  (JNIEnv * env, jobject thiz, jobjectArray jNames, jobjectArray jValues) {
    TRACE("OutArchiveImpl::nativeSetProperties(). ThreadID=" << PlatformGetCurrentThreadId());

    JBindingSession & jbindingSession = GetJBindingSession(env, thiz);
    JNINativeCallContext jniNativeCallContext(jbindingSession, env);
    JNIEnvInstance jniEnvInstance(jbindingSession, jniNativeCallContext, env);

    CMyComPtr<IOutArchive> outArchive(GetArchive(env, thiz));
    // TODO Delete this and all other such ifs, also in J2CppInArchive.cpp, since this is already tested in GetArchive()
    if (outArchive == NULL) {
        TRACE("Archive==NULL. Do nothing...");
        return;
    }

    // TODO Move query interface to the central location in J2C+SevenZip.cpp
    CMyComPtr<ISetProperties> setProperties;
    HRESULT result = outArchive->QueryInterface(IID_ISetProperties, (void**)&setProperties);
    if (result != S_OK) {
        TRACE("Error getting IID_ISetProperties interface. Result: 0x" << std::hex << result)
        jniNativeCallContext.reportError(result, "Error getting IID_ISetProperties interface.");
        return;
    }

    UStringVector needDels;
    CRecordVector<const wchar_t *> names;

    jclass stringClass = VarTypeToJavaType(jniEnvInstance, VT_BSTR);
    jclass integerClass = VarTypeToJavaType(jniEnvInstance, VT_UINT);
    jclass booleanClass = VarTypeToJavaType(jniEnvInstance, VT_BOOL);

    jsize size = jniEnvInstance->GetArrayLength(jValues);
    NWindows::NCOM::CPropVariant *propValues = new NWindows::NCOM::CPropVariant[size];

    for (jint i = 0; i < size; ++i) {
        jobject jName = jniEnvInstance->GetObjectArrayElement(jNames, i);
        jobject jValue = jniEnvInstance->GetObjectArrayElement(jValues, i);

        UString name(FromJChar(jniEnvInstance, (jstring) jName));
        needDels.Add(name);
        names.Add(needDels.Back().Ptr());

        if (jniEnvInstance->IsInstanceOf(jValue, booleanClass)) {
            jboolean value = jni::Boolean::booleanValue(jniEnvInstance, jValue);
            propValues[i] = (bool) value;
        } else if (jniEnvInstance->IsInstanceOf(jValue, integerClass)) {
            jint value = jni::Integer::intValue(jniEnvInstance, jValue);
            propValues[i] = (UInt32) value;
        } else if (jniEnvInstance->IsInstanceOf(jValue, stringClass)) {
            UString value(FromJChar(jniEnvInstance, (jstring) jValue));
            needDels.Add(value);
            propValues[i] = needDels.Back().Ptr();
        } else {
            propValues[i] = false;
        }
    }

    result = setProperties->SetProperties(&names.Front(), propValues, names.Size());

    delete[] propValues;

    if (result) {
        TRACE("Error setting properties. Result: 0x" << std::hex << result)
        jniNativeCallContext.reportError(result, "Error setting properties.");
        return;
    }
}

/*
 * Class:     net_sf_sevenzipjbinding_impl_OutArchiveImpl
 * Method:    nativeClose
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_net_sf_sevenzipjbinding_impl_OutArchiveImpl_nativeClose
  (JNIEnv * env, jobject thiz) {

    TRACE("InArchiveImpl::nativeClose(). ThreadID=" << PlatformGetCurrentThreadId());

    JBindingSession & jbindingSession = GetJBindingSession(env, thiz);
    {
        JNINativeCallContext jniNativeCallContext(jbindingSession, env);
        JNIEnvInstance jniEnvInstance(jbindingSession, jniNativeCallContext, env);

        CMyComPtr<IOutArchive> outArchive(GetArchive(env, thiz));

        outArchive->Release();

        jni::OutArchiveImpl::sevenZipArchiveInstance_Set(env, thiz, 0);
        jni::OutArchiveImpl::jbindingSession_Set(env, thiz, 0);

        TRACE("sevenZipArchiveInstance and jbindingSession references cleared, outArchive released")
    }
    delete &jbindingSession;

    TRACE("OutArchive closed")
}
