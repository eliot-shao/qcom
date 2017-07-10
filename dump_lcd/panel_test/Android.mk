# Copyright 2006-2014 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= panel_test.c


LOCAL_SHARED_LIBRARIES        := $(common_libs) libqdutils libdl liblog libbase libcutils
LOCAL_C_INCLUDES              := $(common_includes) $(kernel_includes)
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_deps) $(kernel_deps)

LOCAL_MODULE := panel_test

LOCAL_CFLAGS := -Werror

include $(BUILD_EXECUTABLE)

include $(call first-makefiles-under,$(LOCAL_PATH))
