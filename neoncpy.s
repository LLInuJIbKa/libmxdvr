.fpu neon
.ALIGN

.equ    NUL, 0 

.GLOBAL  neoncpy

.TYPE   neoncpy,function

neoncpy:
STMFD   sp!, {r4-r11, lr}
NEONCopyPLD:
      PLD [r1, #0xC0]
      VLDM r1!,{d0-d7}
      VSTM r0!,{d0-d7}
      SUBS r2,r2,#0x40
      BGT NEONCopyPLD
LDMFD   sp!, {r4-r11, pc}

.fend2:

.SIZE   neoncpy,.fend2-neoncpy

