LOCAL_PATH := $(call my-dir)

# Android build system ko bolna ki saare sub-folders ki Android.mk ko scan kare
include $(call all-subdir-makefiles)
