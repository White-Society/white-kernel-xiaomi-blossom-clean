# Жестко объявляем переменные окружения для DDK PowerVR
MTK_PLATFORM := mt6765
MTK_GPU_VERSION := m1.13ED5776728
export MTK_PLATFORM
export MTK_GPU_VERSION

obj-$(CONFIG_MEDIATEK_GPU) += hal/
obj-$(CONFIG_MEDIATEK_GPU) += gpufreq/
obj-$(CONFIG_MEDIATEK_GPU) += ged/

# Включаем сборку основного драйвера GPU
obj-$(CONFIG_MEDIATEK_GPU) += gpu_rgx/m1.13ED5776728/