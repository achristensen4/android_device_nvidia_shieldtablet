# Inherit device configuration for shieldtablet.
$(call inherit-product, device/nvidia/shieldtablet/full_shieldtablet.mk)

# Inherit some common CM stuff.
$(call inherit-product, vendor/omni/config/common.mk)

PRODUCT_NAME := omni_shieldtablet
PRODUCT_DEVICE := shieldtablet
