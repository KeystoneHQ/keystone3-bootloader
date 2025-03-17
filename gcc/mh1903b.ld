_estack = 0x20100000;

MEMORY
{
	FLASH (rx)  : ORIGIN = 0x01001000, LENGTH = 0xF00000
	SRAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 1024K
}

SECTIONS
{
	.interrupts 0x01001000 :
	{
		. = ALIGN(4);
		KEEP(*(.isr_vector))
	} > FLASH

	.fixSegment 0x01002000 : {KEEP(*(.fixSection))} > FLASH
	.text :
	{
		. = 0x0;
		_stext = .;
		KEEP(*(.isr_vector))
		*(.text*)
		*(.rodata*)
		_etext = .;
	} > FLASH

	.ARM.exidx :
	{
		__exidx_start = .;
		*(.ARM.exidx*)
		*(.gnu.linkonce.armexidx.*)
		__exidx_end = .;
	} > FLASH

	_sidata = .;
	.data : AT(_sidata)
	{
		_sdata = .;
		*(.data*)
		_edata = .;
	} > SRAM

    .stack_guard :
    {
        . = ALIGN(4);
        . = . + 0x400;
        _stack_guard_start = .;
    } > SRAM

    .stack : 
    {
        . = ALIGN(4);
        _sstack = .;
        . = . + 0x10000;
        . = ALIGN(4);
        _estack = .;
    } > SRAM

	.bss :
	{
		_sbss = .;
		*(.bss*)
		*(COMMON)
		_ebss = .;
	} > SRAM

	/DISCARD/ :
	{
		libc.a (*)
		libm.a (*)
		libgcc.a (*)
	}
}

end = .;
