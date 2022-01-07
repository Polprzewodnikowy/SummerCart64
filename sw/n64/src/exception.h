#ifndef EXCEPTION_H__
#define EXCEPTION_H__


#define VECTOR_LOCATION         (0xA0000000)
#define VECTOR_SIZE             (0x80)
#define VECTOR_NUM              (4)

#define HIT_INVALIDATE_I        ((4 << 2) | 0)

#define C0_STATUS               $12
#define C0_CAUSE                $13
#define C0_EPC                  $14

#define EXCEPTION_CODE_BIT      (2)
#define EXCEPTION_CODE_MASK     (0x007C)
#define INTERRUPT_PENDING_BIT   (8)
#define INTERRUPT_PENDING_MASK  (0xFF00)

#define AT_OFFSET               (8)
#define V0_OFFSET               (16)
#define V1_OFFSET               (24)
#define A0_OFFSET               (32)
#define A1_OFFSET               (40)
#define A2_OFFSET               (48)
#define A3_OFFSET               (56)
#define T0_OFFSET               (64)
#define T1_OFFSET               (72)
#define T2_OFFSET               (80)
#define T3_OFFSET               (88)
#define T4_OFFSET               (96)
#define T5_OFFSET               (104)
#define T6_OFFSET               (112)
#define T7_OFFSET               (120)
#define S0_OFFSET               (128)
#define S1_OFFSET               (136)
#define S2_OFFSET               (144)
#define S3_OFFSET               (152)
#define S4_OFFSET               (160)
#define S5_OFFSET               (168)
#define S6_OFFSET               (176)
#define S7_OFFSET               (184)
#define T8_OFFSET               (192)
#define T9_OFFSET               (200)
#define K0_OFFSET               (208)
#define K1_OFFSET               (216)
#define GP_OFFSET               (224)
#define SP_OFFSET               (232)
#define FP_OFFSET               (240)
#define RA_OFFSET               (248)
#define C0_STATUS_OFFSET        (256)
#define C0_CAUSE_OFFSET         (260)
#define C0_EPC_OFFSET           (264)
#define SAVE_REGISTERS_SIZE     (272)


#endif
