#ifndef OV7670_REGS_H
#define OV7670_REGS_H

/* OV7670 SCCB (I2C-compatible) 7-bit address */
#define OV7670_I2C_ADDR         0x21

/* ─── Core control registers ─────────────────────────────── */
#define OV7670_REG_GAIN         0x00  /* AGC gain bits[7:0] */
#define OV7670_REG_BLUE         0x01  /* AWB blue channel gain */
#define OV7670_REG_RED          0x02  /* AWB red channel gain */
#define OV7670_REG_VREF         0x03  /* Vertical frame control */
#define OV7670_REG_COM1         0x04  /* Common control 1 */
#define OV7670_REG_BAVE         0x05  /* U/B average level */
#define OV7670_REG_GbAVE        0x06  /* Y/Gb average level */
#define OV7670_REG_AECHH        0x07  /* Exposure value [15:10] */
#define OV7670_REG_RAVE         0x08  /* V/R average level */
#define OV7670_REG_COM2         0x09  /* Common control 2 */
#define OV7670_REG_PID          0x0A  /* Product ID MSB (0x76) */
#define OV7670_REG_VER          0x0B  /* Product ID LSB (0x73) */
#define OV7670_REG_COM3         0x0C  /* Common control 3 */
#define OV7670_REG_COM4         0x0D  /* Common control 4 */
#define OV7670_REG_COM5         0x0E  /* Common control 5 */
#define OV7670_REG_COM6         0x0F  /* Common control 6 */
#define OV7670_REG_AECH         0x10  /* Exposure value [9:2] */
#define OV7670_REG_CLKRC        0x11  /* Clock rate control */
#define OV7670_REG_COM7         0x12  /* Common control 7 */
#define OV7670_REG_COM8         0x13  /* Common control 8 (AEC/AGC/AWB) */
#define OV7670_REG_COM9         0x14  /* Common control 9 (max gain) */
#define OV7670_REG_COM10        0x15  /* Common control 10 (PCLK/HREF/VSYNC polarity) */
#define OV7670_REG_HSTART       0x17  /* Horizontal frame start */
#define OV7670_REG_HSTOP        0x18  /* Horizontal frame stop */
#define OV7670_REG_VSTRT        0x19  /* Vertical frame start */
#define OV7670_REG_VSTOP        0x1A  /* Vertical frame stop */
#define OV7670_REG_PSHFT        0x1B  /* Pixel delay after HREF */
#define OV7670_REG_MIDH         0x1C  /* Manufacturer ID MSB (0x7F) */
#define OV7670_REG_MIDL         0x1D  /* Manufacturer ID LSB (0xA2) */
#define OV7670_REG_MVFP         0x1E  /* Mirror/flip */
#define OV7670_REG_LAEC         0x1F  /* Reserved */
#define OV7670_REG_ADCCTR0      0x20  /* ADC control */
#define OV7670_REG_ADCCTR1      0x21
#define OV7670_REG_ADCCTR2      0x22
#define OV7670_REG_ADCCTR3      0x23
#define OV7670_REG_AEW          0x24  /* AGC/AEC upper limit */
#define OV7670_REG_AEB          0x25  /* AGC/AEC lower limit */
#define OV7670_REG_VPT          0x26  /* AGC/AEC fast mode operating region */
#define OV7670_REG_BBIAS        0x27  /* B channel signal output bias */
#define OV7670_REG_GbBIAS       0x28  /* Gb channel signal output bias */
#define OV7670_REG_EXHCH        0x2A  /* Dummy pixel insert MSB */
#define OV7670_REG_EXHCL        0x2B  /* Dummy pixel insert LSB */
#define OV7670_REG_RBIAS        0x2C  /* R channel signal output bias */
#define OV7670_REG_ADVFL        0x2D  /* LSB of insert dummy lines in vertical direction */
#define OV7670_REG_ADVFH        0x2E  /* MSB of insert dummy lines */
#define OV7670_REG_YAVE         0x2F  /* Y/G channel average value */
#define OV7670_REG_HSYST        0x30  /* HSYNC rising edge delay */
#define OV7670_REG_HSYEN        0x31  /* HSYNC falling edge delay */
#define OV7670_REG_HREF         0x32  /* HREF control */
#define OV7670_REG_CHLF         0x33  /* Array current control */
#define OV7670_REG_ARBLM        0x34  /* Array reference control */
#define OV7670_REG_ADC          0x37  /* ADC control */
#define OV7670_REG_ACOM         0x38  /* ADC and analog common mode control */
#define OV7670_REG_OFON         0x39  /* ADC offset control */
#define OV7670_REG_TSLB         0x3A  /* Line buffer test option */
#define OV7670_REG_COM11        0x3B  /* Common control 11 */
#define OV7670_REG_COM12        0x3C  /* Common control 12 */
#define OV7670_REG_COM13        0x3D  /* Common control 13 (gamma/UV) */
#define OV7670_REG_COM14        0x3E  /* Common control 14 (DCW/PCLK scaling) */
#define OV7670_REG_EDGE         0x3F  /* Edge enhancement adjustment */
#define OV7670_REG_COM15        0x40  /* Common control 15 (output range/RGB565) */
#define OV7670_REG_COM16        0x41  /* Common control 16 (edge/denoise) */
#define OV7670_REG_COM17        0x42  /* Common control 17 (AEC window) */
#define OV7670_REG_AWBC1        0x43
#define OV7670_REG_AWBC2        0x44
#define OV7670_REG_AWBC3        0x45
#define OV7670_REG_AWBC4        0x46
#define OV7670_REG_AWBC5        0x47
#define OV7670_REG_AWBC6        0x48
#define OV7670_REG_REG4B        0x4B
#define OV7670_REG_DNSTH        0x4C  /* Denoise threshold */
#define OV7670_REG_MTX1         0x4F  /* Matrix coefficient 1 */
#define OV7670_REG_MTX2         0x50
#define OV7670_REG_MTX3         0x51
#define OV7670_REG_MTX4         0x52
#define OV7670_REG_MTX5         0x53
#define OV7670_REG_MTX6         0x54
#define OV7670_REG_BRIGHT       0x55  /* Brightness control */
#define OV7670_REG_CONTRAS      0x56  /* Contrast control */
#define OV7670_REG_CONTRAS_CTR  0x57  /* Contrast center */
#define OV7670_REG_MTXS         0x58  /* Matrix coefficient sign */
#define OV7670_REG_LCC1         0x62  /* Lens correction option 1 */
#define OV7670_REG_LCC2         0x63
#define OV7670_REG_LCC3         0x64
#define OV7670_REG_LCC4         0x65
#define OV7670_REG_LCC5         0x66
#define OV7670_REG_MANU         0x67  /* Manual U value */
#define OV7670_REG_MANV         0x68  /* Manual V value */
#define OV7670_REG_GFIX         0x69  /* Fix gain control */
#define OV7670_REG_GGAIN        0x6A  /* G channel AWB gain */
#define OV7670_REG_DBLV         0x6B  /* PLL control / regulator */
#define OV7670_REG_AWBCTR3      0x6C
#define OV7670_REG_AWBCTR2      0x6D
#define OV7670_REG_AWBCTR1      0x6E
#define OV7670_REG_AWBCTR0      0x6F
#define OV7670_REG_SCALING_XSC  0x70
#define OV7670_REG_SCALING_YSC  0x71
#define OV7670_REG_SCALING_DCWCTR 0x72
#define OV7670_REG_SCALING_PCLK_DIV 0x73
#define OV7670_REG_REG74        0x74
#define OV7670_REG_REG75        0x75
#define OV7670_REG_REG76        0x76
#define OV7670_REG_REG77        0x77
#define OV7670_REG_SLOP         0x7A  /* Gamma curve highest segment slope */
#define OV7670_REG_GAM_BASE     0x7B  /* Gamma curve base (0x7B..0x7E = GAM1..GAM4) */
#define OV7670_REG_GAM1         0x7B
#define OV7670_REG_GAM2         0x7C
#define OV7670_REG_GAM3         0x7D
#define OV7670_REG_GAM4         0x7E
#define OV7670_REG_GAM5         0x7F
#define OV7670_REG_GAM6         0x80
#define OV7670_REG_GAM7         0x81
#define OV7670_REG_GAM8         0x82
#define OV7670_REG_GAM9         0x83
#define OV7670_REG_GAM10        0x84
#define OV7670_REG_GAM11        0x85
#define OV7670_REG_GAM12        0x86
#define OV7670_REG_GAM13        0x87
#define OV7670_REG_GAM14        0x88
#define OV7670_REG_GAM15        0x89
#define OV7670_REG_RGB444       0x8C  /* RGB 444 control */
#define OV7670_REG_DM_LNL       0x92  /* Dummy line low 8 bits */
#define OV7670_REG_DM_LNH       0x93  /* Dummy line high 8 bits */
#define OV7670_REG_LCC6         0x94
#define OV7670_REG_LCC7         0x95
#define OV7670_REG_BD50ST       0x9D  /* 50Hz banding filter */
#define OV7670_REG_BD60ST       0x9E  /* 60Hz banding filter */
#define OV7670_REG_HAECC1       0x9F
#define OV7670_REG_HAECC2       0xA0
#define OV7670_REG_SCALING_PCLK_DELAY 0xA2
#define OV7670_REG_NT_CTRL      0xA4
#define OV7670_REG_BD50MAX      0xA5  /* 50Hz banding step limit */
#define OV7670_REG_HAECC3       0xA6
#define OV7670_REG_HAECC4       0xA7
#define OV7670_REG_HAECC5       0xA8
#define OV7670_REG_HAECC6       0xA9
#define OV7670_REG_HAECC7       0xAA  /* AEC algorithm control */
#define OV7670_REG_BD60MAX      0xAB  /* 60Hz banding step limit */
#define OV7670_REG_STR_OPT      0xAC
#define OV7670_REG_STR_R        0xAD  /* R gain for LED output frame */
#define OV7670_REG_STR_G        0xAE
#define OV7670_REG_STR_B        0xAF
#define OV7670_REG_ABLC1        0xB1
#define OV7670_REG_THL_ST       0xB3  /* ABLC target */
#define OV7670_REG_SATCTR       0xC9  /* Saturation control */
#define OV7670_REG_COM_LAST     0xFF  /* Sentinel – end of register table */

/* ─── COM7 bits ──────────────────────────────────────────── */
#define OV7670_COM7_RESET       BIT(7) /* SCCB register reset */
#define OV7670_COM7_FMT_QVGA    BIT(4) /* QVGA (320×240) */
#define OV7670_COM7_FMT_CIF     BIT(5) /* CIF (352×288) */
#define OV7670_COM7_FMT_VGA     0x00   /* VGA (640×480) */
#define OV7670_COM7_RGB         BIT(2) /* Output format: RGB */
#define OV7670_COM7_YUV         0x00   /* Output format: YUV */
#define OV7670_COM7_BAYER       BIT(0) /* Raw Bayer output */
#define OV7670_COM7_PBAYER      (BIT(0)|BIT(2))

/* ─── COM3 bits ──────────────────────────────────────────── */
#define OV7670_COM3_SWAP        BIT(6) /* Output data MSB/LSB swap */
#define OV7670_COM3_SCALEEN     BIT(3) /* Scale enable */
#define OV7670_COM3_DCWEN       BIT(2) /* DCW enable */

/* ─── COM8 bits ──────────────────────────────────────────── */
#define OV7670_COM8_FASTAEC     BIT(7)
#define OV7670_COM8_AECSTEP     BIT(6)
#define OV7670_COM8_BANDING     BIT(5)
#define OV7670_COM8_AGC         BIT(2) /* AGC enable */
#define OV7670_COM8_AWB         BIT(1) /* AWB enable */
#define OV7670_COM8_AEC         BIT(0) /* AEC enable */

/* ─── COM10 bits ─────────────────────────────────────────── */
#define OV7670_COM10_VSYNC_NEG  BIT(1) /* VSYNC negative polarity */
#define OV7670_COM10_HREF_REV   BIT(3) /* HREF reverse */
#define OV7670_COM10_PCLK_FREE  BIT(5) /* PCLK free-running */

/* ─── COM15 bits ─────────────────────────────────────────── */
#define OV7670_COM15_R00FF      BIT(7) /* Output range: [00] to [FF] */
#define OV7670_COM15_RGB565     BIT(4) /* RGB565 output */
#define OV7670_COM15_RGB555     BIT(5) /* RGB555 output */

/* ─── MVFP bits ──────────────────────────────────────────── */
#define OV7670_MVFP_MIRROR      BIT(5) /* Mirror image */
#define OV7670_MVFP_FLIP        BIT(4) /* Vertical flip */

/* ─── TSLB bits ──────────────────────────────────────────── */
#define OV7670_TSLB_YLAST       BIT(2) /* UYVY vs YUYV byte order */

/* ─── Chip ID ────────────────────────────────────────────── */
#define OV7670_PID_MAGIC        0x76
#define OV7670_VER_MAGIC        0x73

#endif /* OV7670_REGS_H */
