/* mps2_an385 链接脚本 —— QEMU mps2-an385(Cortex-M3 通用开发板模型,无真机)
 * 内存映射取自 QEMU 11.0.3 info mtree(最终裁决就是这台仿真器):
 *   0x00000000  ssram1  4MB(代码,内存型 SRAM——无 Flash 等待,写它即生效)
 *   0x20000000  ssram23 4MB(数据)
 * P1:链接脚本符号在代码里必须按整数地址比较,禁止指针比较。 */

ENTRY(reset_handler)

MEMORY
{
    CODE (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

/* 链接期反制:谁引入这些符号谁链接报错 */
PROVIDE(__cxa_atexit = ASSERT(0, "global destructors banned"));
PROVIDE(_Znwj = ASSERT(0, "heap new banned"));
PROVIDE(_Znwm = ASSERT(0, "heap new banned"));
PROVIDE(__cxa_throw = ASSERT(0, "exceptions banned"));

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > CODE

    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > CODE

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > CODE

    .init_array :
    {
        KEEP(*(.init_array*))
    } > CODE

    ASSERT(SIZEOF(.init_array) == 0, "runtime global constructors banned (use constinit)")

    /* 初始化数据:LMA 在 CODE 区,启动时拷贝到 RAM */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)

        /* RAM 常驻热路径开关段(与 bluepill 同构;F103 上实测负优化,
         * 本板内存型代码区无等待周期,更没有启用的理由) */
        . = ALIGN(4);
        _sramfunc = .;
        *(.ramfunc*)
        . = ALIGN(4);
        _eramfunc = .;

        . = ALIGN(4);
        _edata = .;
    } > RAM AT > CODE
    _sidata = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /DISCARD/ :
    {
        *(.comment)
    }

    _estack = ORIGIN(RAM) + LENGTH(RAM);
}
