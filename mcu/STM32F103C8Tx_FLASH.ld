/* STM32F103C8Tx_FLASH.ld —— App / Bootloader 共用链接脚本
 *
 * 单一脚本通过链接器参数区分 App/Boot 的 FLASH ORIGIN 与 LENGTH:
 *   - App(默认):        ORIGIN=0x08004000, LENGTH=48K
 *   - Bootloader:        链接时传 --defsym=FLASH_ORIGIN=0x08000000
 *                                --defsym=FLASH_LENGTH=0x4000
 *
 * 机制:PROVIDE 仅在符号未定义时赋值。命令行 --defsym 优先级更高,
 *       会先于脚本符号求值,故 Boot 构建时用命令行值,App 构建时用默认值。
 *
 * MEMORY 区域的 ORIGIN/LENGTH 接受表达式,可在脚本解析期被符号求值,
 * 因此 BOOT/App 可共用此份脚本,无需维护两份高度重复的 .ld 文件。
 */
ENTRY(Reset_Handler)

/* 默认值:App @ 0x08004000, 48KB
 * Boot 构建在 LDFLAGS 用 -Wl,--defsym=FLASH_ORIGIN=0x08000000 等覆盖 */
PROVIDE(FLASH_ORIGIN = 0x08004000);
PROVIDE(FLASH_LENGTH = 48K);

MEMORY
{
    FLASH (rx)  : ORIGIN = FLASH_ORIGIN, LENGTH = FLASH_LENGTH
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.extab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        . = . + 0x100;
        . = ALIGN(8);
    } > RAM

    _end = .;

    /DISCARD/ :
    {
        *(.ARM.attributes)
    }
}
