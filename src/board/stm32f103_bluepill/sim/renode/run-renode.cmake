# Renode 启动器:被板包的 run-<demo> 目标以 cmake -P 调用。
# 参数经 execute_process 直达进程 argv,不经构建器 shell——规避 $bin 被 make/ninja 当变量吃掉(见 docs/simulation.md P12)。
# 用法参数:-DRENODE_ROOT=<仓库根> -DRENODE_ELF=<ELF 绝对路径>
#          -DRENODE_RESC=<相对仓库根的 resc 路径> -DRENODE_SECONDS=<运行秒数>
#          -DRENODE_LOG=<日志文件名,默认 renode.log>

# 时间戳文件名:每次运行一个独立日志,不做 .1 .2 .3 轮换——事后翻现场
# 时一眼定位是哪一次跑出来的,不是考古一堆同名编号
string(TIMESTAMP _ts "%Y%m%d-%H%M%S")
set(_log "${RENODE_LOG}")
if(_log STREQUAL "")
    set(_log "renode-${_ts}.log")
endif()

execute_process(
    COMMAND renode --console --disable-xwt
            -e "logFile @${RENODE_ROOT}/${_log}"
            -e "\$bin=@${RENODE_ELF}"
            -e "include @${RENODE_RESC}"
            -e "start" -e "sleep ${RENODE_SECONDS}" -e "quit"
    WORKING_DIRECTORY ${RENODE_ROOT}
    RESULT_VARIABLE _rc
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "renode exited with ${_rc}")
endif()
message(STATUS "renode log: ${RENODE_ROOT}/${_log}")
