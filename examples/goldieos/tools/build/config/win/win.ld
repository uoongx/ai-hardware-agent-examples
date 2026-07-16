/* win.ld - Windows PE 格式链接器脚本 */
OUTPUT_FORMAT(pei-i386)
ENTRY(_mainCRTStartup)

SECTIONS
{
    . = 0x00400000;

    .text : {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    }

    .data : {
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
    }

    .rdata : {
        *(.rdata)
        *(.rdata.*)
        *(.rodata)
        *(.rodata.*)
    }

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /* HiHope 初始化表段 - 确保符号被导出 */
    .hihope_init_table : {
		. = ALIGN(4);
		__start_hihope_init_table = .;
		. = ALIGN(4);
		__stop_hihope_init_table = .;
    }

    /* 确保符号在全局符号表中可见 */
    PROVIDE(__start_hihope_init_table = ADDR(.hihope_init_table));
    PROVIDE(__stop_hihope_init_table = ADDR(.hihope_init_table) + SIZEOF(.hihope_init_table));
}