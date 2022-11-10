#ifndef VR4300_H__
#define VR4300_H__


#define HIT_INVALIDATE_I            ((4 << 2) | 0)
#define HIT_WRITE_BACK_INVALIDATE_D ((5 << 2) | 1)
#define HIT_WRITE_BACK_D            ((6 << 2) | 1)

#define CACHE_LINE_SIZE_I           (32)
#define CACHE_LINE_SIZE_D           (16)

#define C0_BADVADDR                 $8
#define C0_COUNT                    $9
#define C0_COMPARE                  $11
#define C0_STATUS                   $12
#define C0_CAUSE                    $13
#define C0_EPC                      $14


#define C0_SR_IE                    (1 << 0)
#define C0_SR_EXL                   (1 << 1)
#define C0_SR_EXR                   (1 << 2)
#define C0_SR_KSU0                  (1 << 3)
#define C0_SR_KSU1                  (1 << 4)
#define C0_SR_UX                    (1 << 5)
#define C0_SR_SX                    (1 << 6)
#define C0_SR_KX                    (1 << 7)
#define C0_SR_IM0                   (1 << 8)
#define C0_SR_IM1                   (1 << 9)
#define C0_SR_IM2                   (1 << 10)
#define C0_SR_IM3                   (1 << 11)
#define C0_SR_IM4                   (1 << 12)
#define C0_SR_IM5                   (1 << 13)
#define C0_SR_IM6                   (1 << 14)
#define C0_SR_IM7                   (1 << 15)
#define C0_SR_DS_DE                 (1 << 16)
#define C0_SR_DS_CE                 (1 << 17)
#define C0_SR_DS_CH                 (1 << 18)
#define C0_SR_DS_SR                 (1 << 20)
#define C0_SR_DS_TS                 (1 << 21)
#define C0_SR_DS_BEV                (1 << 22)
#define C0_SR_DS_ITS                (1 << 24)
#define C0_SR_RE                    (1 << 25)
#define C0_SR_FR                    (1 << 26)
#define C0_SR_RP                    (1 << 27)
#define C0_SR_CU0                   (1 << 28)
#define C0_SR_CU1                   (1 << 29)
#define C0_SR_CU2                   (1 << 30)
#define C0_SR_CU3                   (1 << 31)


#define C0_CR_EC0                   (1 << 2)
#define C0_CR_EC1                   (1 << 3)
#define C0_CR_EC2                   (1 << 4)
#define C0_CR_EC3                   (1 << 5)
#define C0_CR_EC4                   (1 << 6)
#define C0_CR_IP0                   (1 << 8)
#define C0_CR_IP1                   (1 << 9)
#define C0_CR_IP2                   (1 << 10)
#define C0_CR_IP3                   (1 << 11)
#define C0_CR_IP4                   (1 << 12)
#define C0_CR_IP5                   (1 << 13)
#define C0_CR_IP6                   (1 << 14)
#define C0_CR_IP7                   (1 << 15)
#define C0_CR_CE0                   (1 << 28)
#define C0_CR_CE1                   (1 << 29)
#define C0_CR_BD                    (1 << 31)

#define C0_CR_EC_MASK               (C0_CR_EC4 | C0_CR_EC3 | C0_CR_EC2 | C0_CR_EC1 | C0_CR_EC0)
#define C0_CR_EC_BIT                (2)
#define C0_CR_IP_MASK               (C0_CR_IP7 | C0_CR_IP6 | C0_CR_IP5 | C0_CR_IP4 | C0_CR_IP3 | C0_CR_IP2 | C0_CR_IP1 | C0_CR_IP0)
#define C0_CR_IP_BIT                (8)


#endif
