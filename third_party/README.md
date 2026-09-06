# third_party — 外部依赖(submodule,sparse 只留 CMSIS 头)

初始化(新克隆后跑一次):
    git submodule update --init third_party/STM32CubeF1
    # sparse 配置随 .git/modules/ 走,若 clone 后 sparse 不生效:
    cd third_party/STM32CubeF1 && git sparse-checkout set --no-cone \
        '/Drivers/CMSIS/Include/**' '/Drivers/CMSIS/Device/**' '/Drivers/CMSIS/LICENSE.txt'

孤儿 gitlink(上游 bug,详见 .claude/docs/architecture.md):
    Drivers/CMSIS/Device/ST/STM32F1xx 是无 .gitmodules 映射的孤儿,需手动拉:
    git -C third_party/STM32CubeF1 clone --filter=blob:none --no-checkout \
        https://github.com/STMicroelectronics/cmsis-device-f1.git Drivers/CMSIS/Device/ST/STM32F1xx
    git -C third_party/STM32CubeF1/Drivers/CMSIS/Device/ST/STM32F1xx fetch --depth 1 origin c8e9a4a4
    git -C third_party/STM32CubeF1/Drivers/CMSIS/Device/ST/STM32F1xx checkout c8e9a4a4
