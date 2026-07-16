# Chip definition for (using ARM tap)
#
source [find bitsbytes.tcl]
source [find cpu/arm/cortex_a9.tcl]
source [find memory.tcl]
source [find mmr_helpers.tcl]

set CHIP_MAKER	vendor
set CHIP_FAMILY CHIP
set CHIPNAME    CHIP

# how many flash regions.
set N_FLASH                1
set FLASH(0,CHIPSELECT)    -1
# To do, figure out where flash is mapped. Expect 0.
set FLASH(0,BASE)          0x00100000  
set FLASH(0,LEN)           $__128K
set FLASH(0,HUMAN)         "internal flash"
set FLASH(0,TYPE)          "flash"
set FLASH(0,RWX)           $RWX_R_X
set FLASH(0,ACCESS_WIDTH)  $ACCESS_WIDTH_ANY
# how many ram regions.   TO
set N_RAM                  1
set RAM(0,CHIPSELECT)      -1
set RAM(0,BASE)            0x00000000
set RAM(0,LEN)             0x00005000
set RAM(0,HUMAN)           "internal SRAM"
set RAM(0,TYPE)            "ram"
set RAM(0,RWX)             $RWX_RWX
set RAM(0,ACCESS_WIDTH)    $ACCESS_WIDTH_ANY

# I AM LAZY... I create 1 region for all MMRs.
set N_MMREGS    1
set MMREGS(0,CHIPSELECT)      -1
set MMREGS(0,BASE)            0x10000000
set MMREGS(0,LEN)             0x000fffff
set MMREGS(0,HUMAN)           "mm-regs"
set MMREGS(0,TYPE)            "mmr"
set MMREGS(0,RWX)             $RWX_RW
set MMREGS(0,ACCESS_WIDTH)    $ACCESS_WIDTH_ANY

# no external memory
set N_XMEM 0


set OTP_BASE         0x10180000
set OTP_CTRL         $OTP_BASE
set OTP_TP           [expr $OTP_BASE + 0x00000004]
set OTP_RADDR        [expr $OTP_BASE + 0x00000008]
set OTP_RPDATA       [expr $OTP_BASE + 0x0000000C]
set OTP_STATUS0      [expr $OTP_BASE + 0x00000010]
set OTP_STATUS1      [expr $OTP_BASE + 0x00000014]
set OTP_CHANGE_MODE  [expr $OTP_BASE + 0x00000018]
