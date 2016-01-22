#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(my-dir)

# Default values for the USE flags. Override these USE flags from your product
# by setting BRILLO_USE_* values. Note that we define local variables like
# local_use_* to prevent leaking our default setting for other packages.
local_use_binder := $(if $(BRILLO_USE_BINDER),$(BRILLO_USE_BINDER),1)
local_use_dbus := $(if $(BRILLO_USE_DBUS),$(BRILLO_USE_DBUS),0)
local_use_hwid_override := \
    $(if $(BRILLO_USE_HWID_OVERRIDE),$(BRILLO_USE_HWID_OVERRIDE),0)
local_use_mtd := $(if $(BRILLO_USE_MTD),$(BRILLO_USE_MTD),0)
local_use_power_management := \
    $(if $(BRILLO_USE_POWER_MANAGEMENT),$(BRILLO_USE_POWER_MANAGEMENT),0)
local_use_weave := $(if $(BRILLO_USE_WEAVE),$(BRILLO_USE_WEAVE),0)

ue_common_cflags := \
    -DUSE_BINDER=$(local_use_binder) \
    -DUSE_DBUS=$(local_use_dbus) \
    -DUSE_HWID_OVERRIDE=$(local_use_hwid_override) \
    -DUSE_MTD=$(local_use_mtd) \
    -DUSE_POWER_MANAGEMENT=$(local_use_power_management) \
    -DUSE_WEAVE=$(local_use_weave) \
    -D_FILE_OFFSET_BITS=64 \
    -D_POSIX_C_SOURCE=199309L \
    -Wa,--noexecstack \
    -Wall \
    -Werror \
    -Wextra \
    -Wformat=2 \
    -Wno-psabi \
    -Wno-unused-parameter \
    -ffunction-sections \
    -fstack-protector-strong \
    -fvisibility=hidden
ue_common_cppflags := \
    -Wnon-virtual-dtor \
    -fno-strict-aliasing \
    -std=gnu++11
ue_common_ldflags := \
    -Wl,--gc-sections
ue_common_c_includes := \
    $(LOCAL_PATH)/client_library/include \
    external/gtest/include \
    system
ue_common_shared_libraries := \
    libbrillo \
    libbrillo-http \
    libbrillo-stream \
    libchrome

ifeq ($(local_use_dbus),1)

# update_engine_client-dbus-proxies (from generate-dbus-proxies.gypi)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine_client-dbus-proxies
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_SRC_FILES := \
    dbus_bindings/dbus-service-config.json \
    dbus_bindings/org.chromium.UpdateEngineInterface.dbus-xml
LOCAL_DBUS_PROXY_PREFIX := update_engine
include $(BUILD_STATIC_LIBRARY)

endif  # local_use_dbus == 1

# update_metadata-protos (type: static_library)
# ========================================================
# Protobufs.
ue_update_metadata_protos_exported_static_libraries := \
    update_metadata-protos
ue_update_metadata_protos_exported_shared_libraries := \
    libprotobuf-cpp-lite

ue_update_metadata_protos_src_files := \
    update_metadata.proto

# Build for the host.
include $(CLEAR_VARS)
LOCAL_MODULE := update_metadata-protos
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_IS_HOST_MODULE := true
generated_sources_dir := $(call local-generated-sources-dir)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(generated_sources_dir)/proto/system
LOCAL_SRC_FILES := $(ue_update_metadata_protos_src_files)
include $(BUILD_HOST_STATIC_LIBRARY)

# Build for the target.
include $(CLEAR_VARS)
LOCAL_MODULE := update_metadata-protos
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
generated_sources_dir := $(call local-generated-sources-dir)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(generated_sources_dir)/proto/system
LOCAL_SRC_FILES := $(ue_update_metadata_protos_src_files)
include $(BUILD_STATIC_LIBRARY)

ifeq ($(local_use_dbus),1)

# update_engine-dbus-adaptor (from generate-dbus-adaptors.gypi)
# ========================================================
# Chrome D-Bus bindings.
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine-dbus-adaptor
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_SRC_FILES := \
    dbus_bindings/org.chromium.UpdateEngineInterface.dbus-xml
include $(BUILD_STATIC_LIBRARY)

# update_engine-dbus-libcros-client (from generate-dbus-proxies.gypi)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine-dbus-libcros-client
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_SRC_FILES := \
    dbus_bindings/org.chromium.LibCrosService.dbus-xml
LOCAL_DBUS_PROXY_PREFIX := libcros
include $(BUILD_STATIC_LIBRARY)

endif  # local_use_dbus == 1

# libpayload_consumer (type: static_library)
# ========================================================
# The payload application component and common dependencies.
ue_libpayload_consumer_exported_static_libraries := \
    update_metadata-protos \
    libxz-host \
    libbz \
    $(ue_update_metadata_protos_exported_static_libraries)
ue_libpayload_consumer_exported_shared_libraries := \
    libcrypto-host \
    libcurl-host \
    libssl-host \
    $(ue_update_metadata_protos_exported_shared_libraries)

ue_libpayload_consumer_src_files := \
    common/action_processor.cc \
    common/boot_control_stub.cc \
    common/certificate_checker.cc \
    common/clock.cc \
    common/constants.cc \
    common/hash_calculator.cc \
    common/http_common.cc \
    common/http_fetcher.cc \
    common/hwid_override.cc \
    common/libcurl_http_fetcher.cc \
    common/multi_range_http_fetcher.cc \
    common/platform_constants_android.cc \
    common/prefs.cc \
    common/subprocess.cc \
    common/terminator.cc \
    common/utils.cc \
    payload_consumer/bzip_extent_writer.cc \
    payload_consumer/delta_performer.cc \
    payload_consumer/download_action.cc \
    payload_consumer/extent_writer.cc \
    payload_consumer/file_descriptor.cc \
    payload_consumer/file_writer.cc \
    payload_consumer/filesystem_verifier_action.cc \
    payload_consumer/install_plan.cc \
    payload_consumer/payload_constants.cc \
    payload_consumer/payload_verifier.cc \
    payload_consumer/postinstall_runner_action.cc \
    payload_consumer/xz_extent_writer.cc

ifeq ($(HOST_OS),linux)
# Build for the host.
include $(CLEAR_VARS)
LOCAL_MODULE := libpayload_consumer
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := \
    $(ue_common_c_includes) \
    external/e2fsprogs/lib
LOCAL_STATIC_LIBRARIES := \
    update_metadata-protos \
    $(ue_libpayload_consumer_exported_static_libraries) \
    $(ue_update_metadata_protos_exported_static_libraries)
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries) \
    $(ue_libpayload_consumer_exported_shared_libraries) \
    $(ue_update_metadata_protos_exported_shared_libraries)
LOCAL_SRC_FILES := $(ue_libpayload_consumer_src_files)
include $(BUILD_HOST_STATIC_LIBRARY)
endif  # HOST_OS == linux

# Build for the target.
include $(CLEAR_VARS)
LOCAL_MODULE := libpayload_consumer
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := \
    $(ue_common_c_includes) \
    external/e2fsprogs/lib
LOCAL_STATIC_LIBRARIES := \
    update_metadata-protos \
    $(ue_libpayload_consumer_exported_static_libraries:-host=) \
    $(ue_update_metadata_protos_exported_static_libraries)
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries) \
    $(ue_libpayload_consumer_exported_shared_libraries:-host=) \
    $(ue_update_metadata_protos_exported_shared_libraries)
LOCAL_SRC_FILES := $(ue_libpayload_consumer_src_files)
include $(BUILD_STATIC_LIBRARY)

ifeq ($(local_use_dbus),1)

# libupdate_engine (type: static_library)
# ========================================================
# The main daemon static_library with all the code used to check for updates
# with Omaha and expose a DBus daemon.
ue_libupdate_engine_exported_c_includes := \
    $(LOCAL_PATH)/include \
    external/cros/system_api/dbus
ue_libupdate_engine_exported_static_libraries := \
    libpayload_consumer \
    update_metadata-protos \
    update_engine-dbus-adaptor \
    update_engine-dbus-libcros-client \
    update_engine_client-dbus-proxies \
    libbz \
    libfs_mgr \
    $(ue_libpayload_consumer_exported_static_libraries) \
    $(ue_update_metadata_protos_exported_static_libraries)
ue_libupdate_engine_exported_shared_libraries := \
    libdbus \
    libbrillo-dbus \
    libchrome-dbus \
    libmetrics \
    libshill-client \
    libexpat \
    libbrillo-policy \
    libhardware \
    libcutils \
    $(ue_libpayload_consumer_exported_shared_libraries) \
    $(ue_update_metadata_protos_exported_shared_libraries)
ifeq ($(local_use_weave),1)
ue_libupdate_engine_exported_shared_libraries += \
    libweaved
endif  # local_use_weave == 1
ifeq ($(local_use_binder),1)
ue_libupdate_engine_exported_shared_libraries += \
    libbinder \
    libbrillo-binder \
    libutils
endif  # local_use_binder == 1

include $(CLEAR_VARS)
LOCAL_MODULE := libupdate_engine
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_EXPORT_C_INCLUDE_DIRS := $(ue_libupdate_engine_exported_c_includes)
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := \
    $(ue_common_c_includes) \
    $(ue_libupdate_engine_exported_c_includes)
LOCAL_STATIC_LIBRARIES := \
    libpayload_consumer \
    update_metadata-protos \
    update_engine-dbus-adaptor \
    update_engine-dbus-libcros-client \
    update_engine_client-dbus-proxies \
    $(ue_libupdate_engine_exported_static_libraries:-host=) \
    $(ue_libpayload_consumer_exported_static_libraries:-host=) \
    $(ue_update_metadata_protos_exported_static_libraries)
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries) \
    $(ue_libupdate_engine_exported_shared_libraries:-host=) \
    $(ue_libpayload_consumer_exported_shared_libraries:-host=) \
    $(ue_update_metadata_protos_exported_shared_libraries)
LOCAL_SRC_FILES := \
    boot_control_android.cc \
    chrome_browser_proxy_resolver.cc \
    common_service.cc \
    connection_manager.cc \
    daemon.cc \
    dbus_service.cc \
    hardware_android.cc \
    image_properties_android.cc \
    libcros_proxy.cc \
    metrics.cc \
    metrics_utils.cc \
    omaha_request_action.cc \
    omaha_request_params.cc \
    omaha_response_handler_action.cc \
    p2p_manager.cc \
    payload_state.cc \
    proxy_resolver.cc \
    real_system_state.cc \
    shill_proxy.cc \
    update_attempter.cc \
    update_manager/boxed_value.cc \
    update_manager/chromeos_policy.cc \
    update_manager/default_policy.cc \
    update_manager/evaluation_context.cc \
    update_manager/policy.cc \
    update_manager/real_config_provider.cc \
    update_manager/real_device_policy_provider.cc \
    update_manager/real_random_provider.cc \
    update_manager/real_shill_provider.cc \
    update_manager/real_system_provider.cc \
    update_manager/real_time_provider.cc \
    update_manager/real_updater_provider.cc \
    update_manager/state_factory.cc \
    update_manager/update_manager.cc \
    update_status_utils.cc \
    weave_service_factory.cc
ifeq ($(local_use_weave),1)
LOCAL_SRC_FILES += \
    weave_service.cc
endif  # local_use_weave == 1

ifeq ($(local_use_binder),1)
LOCAL_AIDL_INCLUDES += $(LOCAL_PATH)/binder_bindings
LOCAL_SRC_FILES += \
    binder_bindings/android/brillo/IUpdateEngine.aidl \
    binder_bindings/android/brillo/IUpdateEngineStatusCallback.aidl \
    binder_service.cc \
    parcelable_update_engine_status.cc
endif  # local_use_binder == 1

include $(BUILD_STATIC_LIBRARY)

endif  # local_use_dbus == 1

# update_engine (type: executable)
# ========================================================
# update_engine daemon.
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_REQUIRED_MODULES := \
    bspatch \
    cacerts_google
ifeq ($(local_use_weave),1)
LOCAL_REQUIRED_MODULES += updater.json
endif  # local_use_weave == 1
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := \
    $(ue_common_c_includes)
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries)
LOCAL_SRC_FILES := \
    main.cc

ifdef BRILLO
LOCAL_C_INCLUDES += \
    $(ue_libupdate_engine_exported_c_includes)
LOCAL_STATIC_LIBRARIES := \
    libupdate_engine \
    $(ue_libupdate_engine_exported_static_libraries:-host=)
LOCAL_SHARED_LIBRARIES += \
    $(ue_libupdate_engine_exported_shared_libraries:-host=)
else  # !defined(BRILLO)
LOCAL_AIDL_INCLUDES := $(LOCAL_PATH)/binder_bindings
LOCAL_STATIC_LIBRARIES := \
    libpayload_consumer \
    $(ue_libpayload_consumer_exported_static_libraries:-host=)
LOCAL_SHARED_LIBRARIES += \
    $(ue_libpayload_consumer_exported_shared_libraries:-host=) \
    libbinder \
    libbrillo-binder \
    libutils
LOCAL_SRC_FILES += \
    binder_bindings/android/os/IUpdateEngine.aidl \
    binder_bindings/android/os/IUpdateEngineCallback.aidl \
    binder_service_android.cc \
    daemon.cc
endif  # defined(BRILLO)

ifeq ($(local_use_binder),1)
LOCAL_SHARED_LIBRARIES += \
    libbinder \
    libbinderwrapper \
    libutils
endif  # local_use_binder == 1

LOCAL_INIT_RC := update_engine.rc
include $(BUILD_EXECUTABLE)

ifeq ($(local_use_dbus),1)

# update_engine_client (type: executable)
# ========================================================
# update_engine console client.
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine_client
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := \
    $(ue_common_c_includes) \
    $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := update_engine_client-dbus-proxies
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries) \
    libdbus \
    libbrillo-dbus \
    libchrome-dbus \
    libupdate_engine_client
LOCAL_SRC_FILES := \
    update_engine_client.cc
include $(BUILD_EXECUTABLE)

endif  # local_use_dbus == 1

# libpayload_generator (type: static_library)
# ========================================================
# server-side code. This is used for delta_generator and unittests but not
# for any client code.
ue_libpayload_generator_exported_static_libraries := \
    libpayload_consumer \
    update_metadata-protos \
    $(ue_libpayload_consumer_exported_static_libraries) \
    $(ue_update_metadata_protos_exported_static_libraries)
ue_libpayload_generator_exported_shared_libraries := \
    libext2fs-host \
    $(ue_libpayload_consumer_exported_shared_libraries) \
    $(ue_update_metadata_protos_exported_shared_libraries)

ue_libpayload_generator_src_files := \
    payload_generator/ab_generator.cc \
    payload_generator/annotated_operation.cc \
    payload_generator/blob_file_writer.cc \
    payload_generator/block_mapping.cc \
    payload_generator/bzip.cc \
    payload_generator/cycle_breaker.cc \
    payload_generator/delta_diff_generator.cc \
    payload_generator/delta_diff_utils.cc \
    payload_generator/ext2_filesystem.cc \
    payload_generator/extent_ranges.cc \
    payload_generator/extent_utils.cc \
    payload_generator/full_update_generator.cc \
    payload_generator/graph_types.cc \
    payload_generator/graph_utils.cc \
    payload_generator/inplace_generator.cc \
    payload_generator/payload_file.cc \
    payload_generator/payload_generation_config.cc \
    payload_generator/payload_signer.cc \
    payload_generator/raw_filesystem.cc \
    payload_generator/tarjan.cc \
    payload_generator/topological_sort.cc

ifeq ($(HOST_OS),linux)
# Build for the host.
include $(CLEAR_VARS)
LOCAL_MODULE := libpayload_generator
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := $(ue_common_c_includes)
LOCAL_STATIC_LIBRARIES := \
    libpayload_consumer \
    update_metadata-protos \
    $(ue_libpayload_consumer_exported_static_libraries) \
    $(ue_update_metadata_protos_exported_static_libraries)
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries) \
    $(ue_libpayload_generator_exported_shared_libraries) \
    $(ue_libpayload_consumer_exported_shared_libraries) \
    $(ue_update_metadata_protos_exported_shared_libraries)
LOCAL_SRC_FILES := $(ue_libpayload_generator_src_files)
include $(BUILD_HOST_STATIC_LIBRARY)
endif  # HOST_OS == linux

# Build for the target.
include $(CLEAR_VARS)
LOCAL_MODULE := libpayload_generator
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := $(ue_common_c_includes)
LOCAL_STATIC_LIBRARIES := \
    libpayload_consumer \
    update_metadata-protos \
    $(ue_libpayload_consumer_exported_static_libraries:-host=) \
    $(ue_update_metadata_protos_exported_static_libraries)
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries) \
    $(ue_libpayload_generator_exported_shared_libraries:-host=) \
    $(ue_libpayload_consumer_exported_shared_libraries:-host=) \
    $(ue_update_metadata_protos_exported_shared_libraries)
LOCAL_SRC_FILES := $(ue_libpayload_generator_src_files)
include $(BUILD_STATIC_LIBRARY)

# delta_generator (type: executable)
# ========================================================
# server-side delta generator.
ue_delta_generator_src_files := \
    payload_generator/generate_delta_main.cc

ifeq ($(HOST_OS),linux)
# Build for the host.
include $(CLEAR_VARS)
LOCAL_MODULE := delta_generator
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := $(ue_common_c_includes)
LOCAL_STATIC_LIBRARIES := \
    libpayload_consumer \
    libpayload_generator \
    $(ue_libpayload_consumer_exported_static_libraries) \
    $(ue_libpayload_generator_exported_static_libraries)
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries) \
    $(ue_libpayload_consumer_exported_shared_libraries) \
    $(ue_libpayload_generator_exported_shared_libraries)
LOCAL_SRC_FILES := $(ue_delta_generator_src_files)
include $(BUILD_HOST_EXECUTABLE)
endif  # HOST_OS == linux

# Build for the target.
include $(CLEAR_VARS)
LOCAL_MODULE := delta_generator
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_CFLAGS := $(ue_common_cflags)
LOCAL_CPPFLAGS := $(ue_common_cppflags)
LOCAL_LDFLAGS := $(ue_common_ldflags)
LOCAL_C_INCLUDES := $(ue_common_c_includes)
LOCAL_STATIC_LIBRARIES := \
    libpayload_consumer \
    libpayload_generator \
    $(ue_libpayload_consumer_exported_static_libraries:-host=) \
    $(ue_libpayload_generator_exported_static_libraries:-host=)
LOCAL_SHARED_LIBRARIES := \
    $(ue_common_shared_libraries) \
    $(ue_libpayload_consumer_exported_shared_libraries:-host=) \
    $(ue_libpayload_generator_exported_shared_libraries:-host=)
LOCAL_SRC_FILES := $(ue_delta_generator_src_files)
include $(BUILD_EXECUTABLE)

ifeq ($(local_use_dbus),1)

# libupdate_engine_client
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libupdate_engine_client
LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-unused-parameter \
    -DUSE_BINDER=$(local_use_binder)
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := .cc
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/client_library/include \
    external/cros/system_api/dbus \
    system \
    external/gtest/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/client_library/include
LOCAL_SHARED_LIBRARIES := \
    libchrome \
    libchrome-dbus \
    libbrillo \
    libbrillo-dbus
LOCAL_STATIC_LIBRARIES := \
    update_engine_client-dbus-proxies
LOCAL_SRC_FILES := \
    client_library/client.cc \
    update_status_utils.cc

ifeq ($(local_use_binder),1)
LOCAL_AIDL_INCLUDES := $(LOCAL_PATH)/binder_bindings
LOCAL_SRC_FILES += \
    client_library/client_binder.cc \
    parcelable_update_engine_status.cc \
    binder_bindings/android/brillo/IUpdateEngine.aidl \
    binder_bindings/android/brillo/IUpdateEngineStatusCallback.aidl
LOCAL_SHARED_LIBRARIES += \
    libbinder \
    libutils
else  # local_use_binder != 1
LOCAL_SRC_FILES += client_library/client_dbus.cc
endif  # local_use_binder == 1

include $(BUILD_SHARED_LIBRARY)

endif  # local_use_dbus == 1

# Weave schema files
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := updater.json
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/weaved/traits
LOCAL_SRC_FILES := weaved/traits/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

# Update payload signing public key.
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := brillo-update-payload-key
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/update_engine
LOCAL_MODULE_STEM := update-payload-key.pub.pem
LOCAL_SRC_FILES := update_payload_key/brillo-update-payload-key.pub.pem
LOCAL_BUILT_MODULE_STEM := update_payload_key/brillo-update-payload-key.pub.pem
include $(BUILD_PREBUILT)

# Brillo update payload generation script
# ========================================================
ifeq ($(HOST_OS),linux)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := scripts/brillo_update_payload
LOCAL_MODULE := brillo_update_payload
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := \
    delta_generator \
    shflags
include $(BUILD_PREBUILT)
endif  # HOST_OS == linux
