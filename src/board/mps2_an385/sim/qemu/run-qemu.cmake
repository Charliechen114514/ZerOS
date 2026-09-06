# QEMU 启动器:被 mps2_an385 板包的 run-mps2-<name> 目标以 cmake -P 调用
# (与 bluepill 的 run-renode.cmake 同构——-D 传参,输出直通终端)。
# 用法参数:-DQEMU_ROOT=<仓库根> -DQEMU_ELF=<ELF 绝对路径> -DQEMU_SECONDS=<秒>
# demo 是常驻循环,timeout 到点收场:退出码 124 = 预期内正常,其余 = 真失败

execute_process(
    COMMAND timeout ${QEMU_SECONDS} qemu-system-arm -M mps2-an385 -cpu cortex-m3
            -nographic -monitor none -serial mon:stdio -kernel ${QEMU_ELF}
    WORKING_DIRECTORY ${QEMU_ROOT}
    RESULT_VARIABLE rc
)
if(rc EQUAL 124)
    message(STATUS "(timeout - normal exit for a resident demo)")
elseif(NOT rc EQUAL 0)
    message(FATAL_ERROR "qemu exited with ${rc}")
endif()
