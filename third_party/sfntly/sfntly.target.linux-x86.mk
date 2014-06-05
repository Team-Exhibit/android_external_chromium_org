# This file is generated by gyp; do not edit.

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := third_party_sfntly_sfntly_gyp
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_TAGS := optional
gyp_intermediate_dir := $(call local-intermediates-dir)
gyp_shared_intermediate_dir := $(call intermediates-dir-for,GYP,shared)

# Make sure our deps are built first.
GYP_TARGET_DEPENDENCIES := \
	$(call intermediates-dir-for,GYP,third_party_icu_icuuc_gyp)/icuuc.stamp

GYP_GENERATED_OUTPUTS :=

# Make sure our deps and generated files are built first.
LOCAL_ADDITIONAL_DEPENDENCIES := $(GYP_TARGET_DEPENDENCIES) $(GYP_GENERATED_OUTPUTS)

LOCAL_CPP_EXTENSION := .cc
LOCAL_GENERATED_SOURCES :=

GYP_COPIED_SOURCE_ORIGIN_DIRS :=

LOCAL_SRC_FILES := \
	third_party/sfntly/cpp/src/sfntly/data/byte_array.cc \
	third_party/sfntly/cpp/src/sfntly/data/font_data.cc \
	third_party/sfntly/cpp/src/sfntly/data/font_input_stream.cc \
	third_party/sfntly/cpp/src/sfntly/data/font_output_stream.cc \
	third_party/sfntly/cpp/src/sfntly/data/growable_memory_byte_array.cc \
	third_party/sfntly/cpp/src/sfntly/data/memory_byte_array.cc \
	third_party/sfntly/cpp/src/sfntly/data/readable_font_data.cc \
	third_party/sfntly/cpp/src/sfntly/data/writable_font_data.cc \
	third_party/sfntly/cpp/src/sfntly/font.cc \
	third_party/sfntly/cpp/src/sfntly/font_factory.cc \
	third_party/sfntly/cpp/src/sfntly/port/file_input_stream.cc \
	third_party/sfntly/cpp/src/sfntly/port/lock.cc \
	third_party/sfntly/cpp/src/sfntly/port/memory_input_stream.cc \
	third_party/sfntly/cpp/src/sfntly/port/memory_output_stream.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/big_glyph_metrics.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/bitmap_glyph.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/bitmap_glyph_info.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/bitmap_size_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/composite_bitmap_glyph.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/ebdt_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/eblc_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/ebsc_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/glyph_metrics.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/index_sub_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/index_sub_table_format1.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/index_sub_table_format2.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/index_sub_table_format3.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/index_sub_table_format4.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/index_sub_table_format5.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/simple_bitmap_glyph.cc \
	third_party/sfntly/cpp/src/sfntly/table/bitmap/small_glyph_metrics.cc \
	third_party/sfntly/cpp/src/sfntly/table/byte_array_table_builder.cc \
	third_party/sfntly/cpp/src/sfntly/table/core/cmap_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/core/font_header_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/core/horizontal_device_metrics_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/core/horizontal_header_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/core/horizontal_metrics_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/core/maximum_profile_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/core/name_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/core/os2_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/font_data_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/generic_table_builder.cc \
	third_party/sfntly/cpp/src/sfntly/table/header.cc \
	third_party/sfntly/cpp/src/sfntly/table/subtable.cc \
	third_party/sfntly/cpp/src/sfntly/table/table.cc \
	third_party/sfntly/cpp/src/sfntly/table/table_based_table_builder.cc \
	third_party/sfntly/cpp/src/sfntly/table/truetype/glyph_table.cc \
	third_party/sfntly/cpp/src/sfntly/table/truetype/loca_table.cc \
	third_party/sfntly/cpp/src/sfntly/tag.cc \
	third_party/sfntly/cpp/src/sample/chromium/font_subsetter.cc \
	third_party/sfntly/cpp/src/sample/chromium/subsetter_impl.cc


# Flags passed to both C and C++ files.
MY_CFLAGS_Debug := \
	--param=ssp-buffer-size=4 \
	-Werror \
	-fno-exceptions \
	-fno-strict-aliasing \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-m32 \
	-mmmx \
	-march=pentium4 \
	-msse2 \
	-mfpmath=sse \
	-fuse-ld=gold \
	-ffunction-sections \
	-funwind-tables \
	-g \
	-fno-short-enums \
	-finline-limit=64 \
	-Wa,--noexecstack \
	-U_FORTIFY_SOURCE \
	-Wno-extra \
	-Wno-ignored-qualifiers \
	-Wno-type-limits \
	-fno-stack-protector \
	-Os \
	-g \
	-fomit-frame-pointer \
	-fdata-sections \
	-ffunction-sections

MY_DEFS_Debug := \
	'-DV8_DEPRECATION_WARNINGS' \
	'-D_FILE_OFFSET_BITS=64' \
	'-DNO_TCMALLOC' \
	'-DDISABLE_NACL' \
	'-DCHROMIUM_BUILD' \
	'-DUSE_LIBJPEG_TURBO=1' \
	'-DUSE_PROPRIETARY_CODECS' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DDISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY' \
	'-DSYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE' \
	'-DICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC' \
	'-DUSE_OPENSSL=1' \
	'-DENABLE_EGLIMAGE=1' \
	'-DCLD_VERSION=1' \
	'-DENABLE_PRINTING=1' \
	'-DENABLE_MANAGED_USERS=1' \
	'-DSFNTLY_NO_EXCEPTION' \
	'-DU_USING_ICU_NAMESPACE=0' \
	'-D__STDC_CONSTANT_MACROS' \
	'-D__STDC_FORMAT_MACROS' \
	'-DANDROID' \
	'-D__GNU_SOURCE=1' \
	'-DUSE_STLPORT=1' \
	'-D_STLP_USE_PTR_SPECIALIZATIONS=1' \
	'-DCHROME_BUILD_ID=""' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=1' \
	'-DWTF_USE_DYNAMIC_ANNOTATIONS=1' \
	'-D_DEBUG'


# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES_Debug := \
	$(gyp_shared_intermediate_dir)/shim_headers/icuuc/target \
	$(LOCAL_PATH)/third_party/sfntly/cpp/src \
	$(LOCAL_PATH) \
	$(PWD)/external/icu4c/common \
	$(PWD)/external/icu4c/i18n \
	$(PWD)/frameworks/wilhelm/include \
	$(PWD)/bionic \
	$(PWD)/external/stlport/stlport


# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS_Debug := \
	-fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare \
	-Wno-error=c++0x-compat \
	-Wno-non-virtual-dtor \
	-Wno-sign-promo


# Flags passed to both C and C++ files.
MY_CFLAGS_Release := \
	--param=ssp-buffer-size=4 \
	-Werror \
	-fno-exceptions \
	-fno-strict-aliasing \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-m32 \
	-mmmx \
	-march=pentium4 \
	-msse2 \
	-mfpmath=sse \
	-fuse-ld=gold \
	-ffunction-sections \
	-funwind-tables \
	-g \
	-fno-short-enums \
	-finline-limit=64 \
	-Wa,--noexecstack \
	-U_FORTIFY_SOURCE \
	-Wno-extra \
	-Wno-ignored-qualifiers \
	-Wno-type-limits \
	-fno-stack-protector \
	-Os \
	-fno-ident \
	-fdata-sections \
	-ffunction-sections \
	-fomit-frame-pointer \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables

MY_DEFS_Release := \
	'-DV8_DEPRECATION_WARNINGS' \
	'-D_FILE_OFFSET_BITS=64' \
	'-DNO_TCMALLOC' \
	'-DDISABLE_NACL' \
	'-DCHROMIUM_BUILD' \
	'-DUSE_LIBJPEG_TURBO=1' \
	'-DUSE_PROPRIETARY_CODECS' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DDISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY' \
	'-DSYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE' \
	'-DICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC' \
	'-DUSE_OPENSSL=1' \
	'-DENABLE_EGLIMAGE=1' \
	'-DCLD_VERSION=1' \
	'-DENABLE_PRINTING=1' \
	'-DENABLE_MANAGED_USERS=1' \
	'-DSFNTLY_NO_EXCEPTION' \
	'-DU_USING_ICU_NAMESPACE=0' \
	'-D__STDC_CONSTANT_MACROS' \
	'-D__STDC_FORMAT_MACROS' \
	'-DANDROID' \
	'-D__GNU_SOURCE=1' \
	'-DUSE_STLPORT=1' \
	'-D_STLP_USE_PTR_SPECIALIZATIONS=1' \
	'-DCHROME_BUILD_ID=""' \
	'-DNDEBUG' \
	'-DNVALGRIND' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=0' \
	'-D_FORTIFY_SOURCE=2'


# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES_Release := \
	$(gyp_shared_intermediate_dir)/shim_headers/icuuc/target \
	$(LOCAL_PATH)/third_party/sfntly/cpp/src \
	$(LOCAL_PATH) \
	$(PWD)/external/icu4c/common \
	$(PWD)/external/icu4c/i18n \
	$(PWD)/frameworks/wilhelm/include \
	$(PWD)/bionic \
	$(PWD)/external/stlport/stlport


# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS_Release := \
	-fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare \
	-Wno-error=c++0x-compat \
	-Wno-non-virtual-dtor \
	-Wno-sign-promo


LOCAL_CFLAGS := $(MY_CFLAGS_$(GYP_CONFIGURATION)) $(MY_DEFS_$(GYP_CONFIGURATION))
LOCAL_C_INCLUDES := $(GYP_COPIED_SOURCE_ORIGIN_DIRS) $(LOCAL_C_INCLUDES_$(GYP_CONFIGURATION))
LOCAL_CPPFLAGS := $(LOCAL_CPPFLAGS_$(GYP_CONFIGURATION))
### Rules for final target.

LOCAL_LDFLAGS_Debug := \
	-Wl,-z,now \
	-Wl,-z,relro \
	-Wl,-z,noexecstack \
	-fPIC \
	-m32 \
	-fuse-ld=gold \
	-nostdlib \
	-Wl,--no-undefined \
	-Wl,--exclude-libs=ALL \
	-Wl,--fatal-warnings \
	-Wl,--gc-sections \
	-Wl,--warn-shared-textrel \
	-Wl,-O1 \
	-Wl,--as-needed


LOCAL_LDFLAGS_Release := \
	-Wl,-z,now \
	-Wl,-z,relro \
	-Wl,-z,noexecstack \
	-fPIC \
	-m32 \
	-fuse-ld=gold \
	-nostdlib \
	-Wl,--no-undefined \
	-Wl,--exclude-libs=ALL \
	-Wl,-O1 \
	-Wl,--as-needed \
	-Wl,--gc-sections \
	-Wl,--fatal-warnings \
	-Wl,--warn-shared-textrel


LOCAL_LDFLAGS := $(LOCAL_LDFLAGS_$(GYP_CONFIGURATION))

LOCAL_STATIC_LIBRARIES :=

# Enable grouping to fix circular references
LOCAL_GROUP_STATIC_LIBRARIES := true

LOCAL_SHARED_LIBRARIES := \
	libstlport \
	libdl

# Add target alias to "gyp_all_modules" target.
.PHONY: gyp_all_modules
gyp_all_modules: third_party_sfntly_sfntly_gyp

# Alias gyp target name.
.PHONY: sfntly
sfntly: third_party_sfntly_sfntly_gyp

include $(BUILD_STATIC_LIBRARY)
