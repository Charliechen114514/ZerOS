/* stm32f103_bluepill 链接脚本 —— C8T6: 64K Flash / 20K SRAM
 * 内存约束即真机约束(Renode 模型 sim/renode/bluepill.repl 同尺寸,越界即报)。
 * P1:链接脚本符号在代码里必须按整数地址比较,禁止指针比较。 */

ENTRY(reset_handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
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
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > FLASH

    .init_array :
    {
        KEEP(*(.init_array*))
    } > FLASH

    ASSERT(SIZEOF(.init_array) == 0, "runtime global constructors banned (use constinit)")

    /* 初始化数据:LMA 在 Flash,启动时拷贝到 RAM */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)

        /* RAM 常驻热路径:上下文切换在这里跑,零 Flash 等待周期 */
        . = ALIGN(4);
        _sramfunc = .;
        *(.ramfunc*)
        . = ALIGN(4);
        _eramfunc = .;

        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
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
