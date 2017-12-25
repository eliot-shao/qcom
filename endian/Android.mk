LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := testendian_arm 
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := testendian.c
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
