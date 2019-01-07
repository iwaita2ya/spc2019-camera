#ifndef OV7670_OV7670_H
#define OV7670_OV7670_H

#include "mbed.h"
#include "OV7670_Registers.h"

#define OV7670_WRITE (0x42)
#define OV7670_READ  (0x43)
#define OV7670_WRITEWAIT (20) // 書き込み後のWAIT値 (us)
#define OV7670_NOACK (0)
#define OV7670_REGMAX (201) // レジスタ範囲の最大値
#define OV7670_I2CFREQ (50000)

class OV7670 {
public:

    I2C camera;
    InterruptIn vsync,href;
    DigitalOut wen;
    BusIn data;
    DigitalOut rrst,oe,rclk;
    volatile int LineCounter;
    volatile int LastLines;
    volatile bool CaptureReq;
    volatile bool Busy;
    volatile bool Done;

    OV7670 (
            PinName sda,// Camera I2C port
            PinName scl,// Camera I2C port
            PinName vs, // VSYNC
            PinName hr, // HREF
            PinName we, // WEN
            PinName d7, // D7
            PinName d6, // D6
            PinName d5, // D5
            PinName d4, // D4
            PinName d3, // D3
            PinName d2, // D2
            PinName d1, // D1
            PinName d0, // D0
            PinName rt, // /RRST
            PinName o,  // /OE
            PinName rc  // RCLK
    ) : camera(sda,scl), vsync(vs), href(hr), wen(we), data(d0,d1,d2,d3,d4,d5,d6,d7), rrst(rt), oe(o), rclk(rc)
    {
        camera.stop();
        camera.frequency(OV7670_I2CFREQ);
        vsync.fall(this,&OV7670::VsyncHandler);
        href.rise(this,&OV7670::HrefHandler);
        CaptureReq = false;
        Busy = false;
        Done = false;
        LineCounter = 0;
        rrst = 1;
        oe = 1;
        rclk = 1;
        wen = 0;
    }

    // capture request
    void CaptureNext(void)
    {
        CaptureReq = true;
        Busy = true;
    }

    // capture done? (with clear)
    bool CaptureDone(void)
    {
        bool result;
        if (Busy) {
            result = false;
        } else {
            result = Done;
            Done = false;
        }
        return result;
    }

    // write to camera
    void WriteReg(int addr,int data)
    {
        // WRITE 0x42,ADDR,DATA
        camera.start();
        camera.write(OV7670_WRITE);
        wait_us(OV7670_WRITEWAIT);
        camera.write(addr);
        wait_us(OV7670_WRITEWAIT);
        camera.write(data);
        camera.stop();
    }

    // read from camera
    int ReadReg(int addr)
    {
        int data;

        // WRITE 0x42,ADDR
        camera.start();
        camera.write(OV7670_WRITE);
        wait_us(OV7670_WRITEWAIT);
        camera.write(addr);
        camera.stop();
        wait_us(OV7670_WRITEWAIT);

        // WRITE 0x43,READ
        camera.start();
        camera.write(OV7670_READ);
        wait_us(OV7670_WRITEWAIT);
        data = camera.read(OV7670_NOACK);
        camera.stop();

        return data;
    }

    // print register
    void PrintRegister(void) {
        printf("AD : +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F");
        for (int i=0;i<OV7670_REGMAX;i++) {
            int data;
            data = ReadReg(i); // READ REG
            if ((i & 0x0F) == 0) {
                printf("\r\n%02X : ",i);
            }
            printf("%02X ",data);
        }
        printf("\r\n");
    }

    void Reset(void) {
        WriteReg(REG_COM7,COM7_RESET); // RESET CAMERA
        wait_ms(200); // wait for 200ms
    }

    void InitForFIFOWriteReset(void) {
        WriteReg(REG_COM10, COM10_VS_NEG);
    }

    void InitSetColorbar(void)  {
        int reg_com7 = ReadReg(REG_COM7);
        // color bar
        WriteReg(REG_COM17, reg_com7|COM17_CBAR);
    }

    void InitDefaultReg(void) {
        // Gamma curve values
        WriteReg(0x7a, 0x20);
        WriteReg(0x7b, 0x10);
        WriteReg(0x7c, 0x1e);
        WriteReg(0x7d, 0x35);
        WriteReg(0x7e, 0x5a);
        WriteReg(0x7f, 0x69);
        WriteReg(0x80, 0x76);
        WriteReg(0x81, 0x80);
        WriteReg(0x82, 0x88);
        WriteReg(0x83, 0x8f);
        WriteReg(0x84, 0x96);
        WriteReg(0x85, 0xa3);
        WriteReg(0x86, 0xaf);
        WriteReg(0x87, 0xc4);
        WriteReg(0x88, 0xd7);
        WriteReg(0x89, 0xe8);

        // AGC and AEC parameters.  Note we start by disabling those features,
        //then turn them only after tweaking the values.
        WriteReg(REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT);
        WriteReg(REG_GAIN, 0);
        WriteReg(REG_AECH, 0);
        WriteReg(REG_COM4, 0x40);
        // magic reserved bit
        WriteReg(REG_COM9, 0x18);
        // 4x gain + magic rsvd bit
        WriteReg(REG_BD50MAX, 0x05);
        WriteReg(REG_BD60MAX, 0x07);
        WriteReg(REG_AEW, 0x95);
        WriteReg(REG_AEB, 0x33);
        WriteReg(REG_VPT, 0xe3);
        WriteReg(REG_HAECC1, 0x78);
        WriteReg(REG_HAECC2, 0x68);
        WriteReg(0xa1, 0x03);
        // magic
        WriteReg(REG_HAECC3, 0xd8);
        WriteReg(REG_HAECC4, 0xd8);
        WriteReg(REG_HAECC5, 0xf0);
        WriteReg(REG_HAECC6, 0x90);
        WriteReg(REG_HAECC7, 0x94);
        WriteReg(REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC);

        // Almost all of these are magic "reserved" values.
        WriteReg(REG_COM5, 0x61);
        WriteReg(REG_COM6, 0x4b);
        WriteReg(0x16, 0x02);
        WriteReg(REG_MVFP, 0x07);
        WriteReg(0x21, 0x02);
        WriteReg(0x22, 0x91);
        WriteReg(0x29, 0x07);
        WriteReg(0x33, 0x0b);
        WriteReg(0x35, 0x0b);
        WriteReg(0x37, 0x1d);
        WriteReg(0x38, 0x71);
        WriteReg(0x39, 0x2a);
        WriteReg(REG_COM12, 0x78);
        WriteReg(0x4d, 0x40);
        WriteReg(0x4e, 0x20);
        WriteReg(REG_GFIX, 0);
        WriteReg(0x6b, 0x0a);
        WriteReg(0x74, 0x10);
        WriteReg(0x8d, 0x4f);
        WriteReg(0x8e, 0);
        WriteReg(0x8f, 0);
        WriteReg(0x90, 0);
        WriteReg(0x91, 0);
        WriteReg(0x96, 0);
        WriteReg(0x9a, 0);
        WriteReg(0xb0, 0x84);
        WriteReg(0xb1, 0x0c);
        WriteReg(0xb2, 0x0e);
        WriteReg(0xb3, 0x82);
        WriteReg(0xb8, 0x0a);

        // More reserved magic, some of which tweaks white balance
        WriteReg(0x43, 0x0a);
        WriteReg(0x44, 0xf0);
        WriteReg(0x45, 0x34);
        WriteReg(0x46, 0x58);
        WriteReg(0x47, 0x28);
        WriteReg(0x48, 0x3a);
        WriteReg(0x59, 0x88);
        WriteReg(0x5a, 0x88);
        WriteReg(0x5b, 0x44);
        WriteReg(0x5c, 0x67);
        WriteReg(0x5d, 0x49);
        WriteReg(0x5e, 0x0e);
        WriteReg(0x6c, 0x0a);
        WriteReg(0x6d, 0x55);
        WriteReg(0x6e, 0x11);
        WriteReg(0x6f, 0x9f);
        // "9e for advance AWB"
        WriteReg(0x6a, 0x40);
        WriteReg(REG_BLUE, 0x40);
        WriteReg(REG_RED, 0x60);
        WriteReg(REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC|COM8_AWB);

        // Matrix coefficients
        WriteReg(0x4f, 0x80);
        WriteReg(0x50, 0x80);
        WriteReg(0x51, 0);
        WriteReg(0x52, 0x22);
        WriteReg(0x53, 0x5e);
        WriteReg(0x54, 0x80);
        WriteReg(0x58, 0x9e);

        WriteReg(REG_COM16, COM16_AWBGAIN);
        WriteReg(REG_EDGE, 0);
        WriteReg(0x75, 0x05);
        WriteReg(0x76, 0xe1);
        WriteReg(0x4c, 0);
        WriteReg(0x77, 0x01);
        WriteReg(0x4b, 0x09);
        WriteReg(0xc9, 0x60);
        WriteReg(REG_COM16, 0x38);
        WriteReg(0x56, 0x40);

        WriteReg(0x34, 0x11);
        WriteReg(REG_COM11, COM11_EXP|COM11_HZAUTO_ON);
        WriteReg(0xa4, 0x88);
        WriteReg(0x96, 0);
        WriteReg(0x97, 0x30);
        WriteReg(0x98, 0x20);
        WriteReg(0x99, 0x30);
        WriteReg(0x9a, 0x84);
        WriteReg(0x9b, 0x29);
        WriteReg(0x9c, 0x03);
        WriteReg(0x9d, 0x4c);
        WriteReg(0x9e, 0x3f);
        WriteReg(0x78, 0x04);

        // Extra-weird stuff.  Some sort of multiplexor register
        WriteReg(0x79, 0x01);
        WriteReg(0xc8, 0xf0);
        WriteReg(0x79, 0x0f);
        WriteReg(0xc8, 0x00);
        WriteReg(0x79, 0x10);
        WriteReg(0xc8, 0x7e);
        WriteReg(0x79, 0x0a);
        WriteReg(0xc8, 0x80);
        WriteReg(0x79, 0x0b);
        WriteReg(0xc8, 0x01);
        WriteReg(0x79, 0x0c);
        WriteReg(0xc8, 0x0f);
        WriteReg(0x79, 0x0d);
        WriteReg(0xc8, 0x20);
        WriteReg(0x79, 0x09);
        WriteReg(0xc8, 0x80);
        WriteReg(0x79, 0x02);
        WriteReg(0xc8, 0xc0);
        WriteReg(0x79, 0x03);
        WriteReg(0xc8, 0x40);
        WriteReg(0x79, 0x05);
        WriteReg(0xc8, 0x30);
        WriteReg(0x79, 0x26);
    }

    void InitRGB444(void){
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7, reg_com7|COM7_RGB);
        WriteReg(REG_RGB444, RGB444_ENABLE|RGB444_XBGR);
        WriteReg(REG_COM15, COM15_R01FE|COM15_RGB444);

        WriteReg(REG_COM1, 0x40);                          // Magic reserved bit
        WriteReg(REG_COM9, 0x38);                          // 16x gain ceiling; 0x8 is reserved bit
        WriteReg(0x4f, 0xb3);                              // "matrix coefficient 1"
        WriteReg(0x50, 0xb3);                              // "matrix coefficient 2"
        WriteReg(0x51, 0x00);                              // vb
        WriteReg(0x52, 0x3d);                              // "matrix coefficient 4"
        WriteReg(0x53, 0xa7);                              // "matrix coefficient 5"
        WriteReg(0x54, 0xe4);                              // "matrix coefficient 6"
        WriteReg(REG_COM13, COM13_GAMMA|COM13_UVSAT|0x2);  // Magic rsvd bit

        WriteReg(REG_TSLB, 0x04);
    }

    void InitRGB555(void){
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7, reg_com7|COM7_RGB);
        WriteReg(REG_RGB444, RGB444_DISABLE);
        WriteReg(REG_COM15, COM15_RGB555|COM15_R00FF);

        WriteReg(REG_TSLB, 0x04);

        WriteReg(REG_COM1, 0x00);
        WriteReg(REG_COM9, 0x38);      // 16x gain ceiling; 0x8 is reserved bit
        WriteReg(0x4f, 0xb3);          // "matrix coefficient 1"
        WriteReg(0x50, 0xb3);          // "matrix coefficient 2"
        WriteReg(0x51, 0x00);          // vb
        WriteReg(0x52, 0x3d);          // "matrix coefficient 4"
        WriteReg(0x53, 0xa7);          // "matrix coefficient 5"
        WriteReg(0x54, 0xe4);          // "matrix coefficient 6"
        WriteReg(REG_COM13, COM13_GAMMA|COM13_UVSAT);
    }

    void InitRGB565(void){
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7, reg_com7|COM7_RGB);
        WriteReg(REG_RGB444, RGB444_DISABLE);
        WriteReg(REG_COM15, COM15_R00FF|COM15_RGB565);

        WriteReg(REG_TSLB, 0x04);

        WriteReg(REG_COM1, 0x00);
        WriteReg(REG_COM9, 0x38);      // 16x gain ceiling; 0x8 is reserved bit
        WriteReg(0x4f, 0xb3);          // "matrix coefficient 1"
        WriteReg(0x50, 0xb3);          // "matrix coefficient 2"
        WriteReg(0x51, 0x00);          // vb
        WriteReg(0x52, 0x3d);          // "matrix coefficient 4"
        WriteReg(0x53, 0xa7);          // "matrix coefficient 5"
        WriteReg(0x54, 0xe4);          // "matrix coefficient 6"
        WriteReg(REG_COM13, COM13_GAMMA|COM13_UVSAT);
    }

    void InitYUV(void){
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7, reg_com7|COM7_YUV);
        WriteReg(REG_RGB444, RGB444_DISABLE);
        WriteReg(REG_COM15, COM15_R00FF);

        WriteReg(REG_TSLB, 0x04);
//        WriteReg(REG_TSLB, 0x14);
//        WriteReg(REG_MANU, 0x00);
//        WriteReg(REG_MANV, 0x00);

        WriteReg(REG_COM1, 0x00);
        WriteReg(REG_COM9, 0x18);     // 4x gain ceiling; 0x8 is reserved bit
        WriteReg(0x4f, 0x80);         // "matrix coefficient 1"
        WriteReg(0x50, 0x80);         // "matrix coefficient 2"
        WriteReg(0x51, 0x00);         // vb
        WriteReg(0x52, 0x22);         // "matrix coefficient 4"
        WriteReg(0x53, 0x5e);         // "matrix coefficient 5"
        WriteReg(0x54, 0x80);         // "matrix coefficient 6"
        WriteReg(REG_COM13, COM13_GAMMA|COM13_UVSAT|COM13_UVSWAP);
    }

    void InitBayerRGB(void){
        int reg_com7 = ReadReg(REG_COM7);

        // odd line BGBG... even line GRGR...
        WriteReg(REG_COM7, reg_com7|COM7_BAYER);
        // odd line GBGB... even line RGRG...
        //WriteReg(REG_COM7, reg_com7|COM7_PBAYER);

        WriteReg(REG_RGB444, RGB444_DISABLE);
        WriteReg(REG_COM15, COM15_R00FF);

        WriteReg(REG_COM13, 0x08); /* No gamma, magic rsvd bit */
        WriteReg(REG_COM16, 0x3d); /* Edge enhancement, denoise */
        WriteReg(REG_REG76, 0xe1); /* Pix correction, magic rsvd */

        WriteReg(REG_TSLB, 0x04);
    }

    void InitVGA(void) {
        // VGA
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7,reg_com7|COM7_VGA);

        WriteReg(REG_HSTART,HSTART_VGA);
        WriteReg(REG_HSTOP,HSTOP_VGA);
        WriteReg(REG_HREF,HREF_VGA);
        WriteReg(REG_VSTART,VSTART_VGA);
        WriteReg(REG_VSTOP,VSTOP_VGA);
        WriteReg(REG_VREF,VREF_VGA);
        WriteReg(REG_COM3, COM3_VGA);
        WriteReg(REG_COM14, COM14_VGA);
        WriteReg(REG_SCALING_XSC, SCALING_XSC_VGA);
        WriteReg(REG_SCALING_YSC, SCALING_YSC_VGA);
        WriteReg(REG_SCALING_DCWCTR, SCALING_DCWCTR_VGA);
        WriteReg(REG_SCALING_PCLK_DIV, SCALING_PCLK_DIV_VGA);
        WriteReg(REG_SCALING_PCLK_DELAY, SCALING_PCLK_DELAY_VGA);
    }

    void InitFIFO_2bytes_color_nealy_limit_size(void) {
        // nealy FIFO limit 544x360
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7,reg_com7|COM7_VGA);

        WriteReg(REG_HSTART,HSTART_VGA);
        WriteReg(REG_HSTOP,HSTOP_VGA);
        WriteReg(REG_HREF,HREF_VGA);
        WriteReg(REG_VSTART,VSTART_VGA);
        WriteReg(REG_VSTOP,VSTOP_VGA);
        WriteReg(REG_VREF,VREF_VGA);
        WriteReg(REG_COM3, COM3_VGA);
        WriteReg(REG_COM14, COM14_VGA);
        WriteReg(REG_SCALING_XSC, SCALING_XSC_VGA);
        WriteReg(REG_SCALING_YSC, SCALING_YSC_VGA);
        WriteReg(REG_SCALING_DCWCTR, SCALING_DCWCTR_VGA);
        WriteReg(REG_SCALING_PCLK_DIV, SCALING_PCLK_DIV_VGA);
        WriteReg(REG_SCALING_PCLK_DELAY, SCALING_PCLK_DELAY_VGA);

        WriteReg(REG_HSTART, 0x17);
        WriteReg(REG_HSTOP, 0x5b);
        WriteReg(REG_VSTART, 0x12);
        WriteReg(REG_VSTOP, 0x6c);
    }

    void InitVGA_3_4(void) {
        // VGA 3/4 -> 480x360
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7,reg_com7|COM7_VGA);

        WriteReg(REG_HSTART,HSTART_VGA);
        WriteReg(REG_HSTOP,HSTOP_VGA);
        WriteReg(REG_HREF,HREF_VGA);
        WriteReg(REG_VSTART,VSTART_VGA);
        WriteReg(REG_VSTOP,VSTOP_VGA);
        WriteReg(REG_VREF,VREF_VGA);
        WriteReg(REG_COM3, COM3_VGA);
        WriteReg(REG_COM14, COM14_VGA);
        WriteReg(REG_SCALING_XSC, SCALING_XSC_VGA);
        WriteReg(REG_SCALING_YSC, SCALING_YSC_VGA);
        WriteReg(REG_SCALING_DCWCTR, SCALING_DCWCTR_VGA);
        WriteReg(REG_SCALING_PCLK_DIV, SCALING_PCLK_DIV_VGA);
        WriteReg(REG_SCALING_PCLK_DELAY, SCALING_PCLK_DELAY_VGA);

        WriteReg(REG_HSTART, 0x1b);
        WriteReg(REG_HSTOP, 0x57);
        WriteReg(REG_VSTART, 0x12);
        WriteReg(REG_VSTOP, 0x6c);
    }

    void InitQVGA(void) {
        // QQVGA
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7,reg_com7|COM7_QVGA);

        WriteReg(REG_HSTART,HSTART_QVGA);
        WriteReg(REG_HSTOP,HSTOP_QVGA);
        WriteReg(REG_HREF,HREF_QVGA);
        WriteReg(REG_VSTART,VSTART_QVGA);
        WriteReg(REG_VSTOP,VSTOP_QVGA);
        WriteReg(REG_VREF,VREF_QVGA);
        WriteReg(REG_COM3, COM3_QVGA);
        WriteReg(REG_COM14, COM14_QVGA);
        WriteReg(REG_SCALING_XSC, SCALING_XSC_QVGA);
        WriteReg(REG_SCALING_YSC, SCALING_YSC_QVGA);
        WriteReg(REG_SCALING_DCWCTR, SCALING_DCWCTR_QVGA);
        WriteReg(REG_SCALING_PCLK_DIV, SCALING_PCLK_DIV_QVGA);
        WriteReg(REG_SCALING_PCLK_DELAY, SCALING_PCLK_DELAY_QVGA);
    }

    void InitQQVGA(void) {
        // QQVGA
        int reg_com7 = ReadReg(REG_COM7);

        WriteReg(REG_COM7,reg_com7|COM7_QQVGA);

        WriteReg(REG_HSTART,HSTART_QQVGA);
        WriteReg(REG_HSTOP,HSTOP_QQVGA);
        WriteReg(REG_HREF,HREF_QQVGA);
        WriteReg(REG_VSTART,VSTART_QQVGA);
        WriteReg(REG_VSTOP,VSTOP_QQVGA);
        WriteReg(REG_VREF,VREF_QQVGA);
        WriteReg(REG_COM3, COM3_QQVGA);
        WriteReg(REG_COM14, COM14_QQVGA);
        WriteReg(REG_SCALING_XSC, SCALING_XSC_QQVGA);
        WriteReg(REG_SCALING_YSC, SCALING_YSC_QQVGA);
        WriteReg(REG_SCALING_DCWCTR, SCALING_DCWCTR_QQVGA);
        WriteReg(REG_SCALING_PCLK_DIV, SCALING_PCLK_DIV_QQVGA);
        WriteReg(REG_SCALING_PCLK_DELAY, SCALING_PCLK_DELAY_QQVGA);
    }

    // vsync handler
    void VsyncHandler(void)
    {
        // Capture Enable
        if (CaptureReq) {
            wen = 1;
            Done = false;
            CaptureReq = false;
        } else {
            wen = 0;
            if (Busy) {
                Busy = false;
                Done = true;
            }
        }

        // Hline Counter
        LastLines = LineCounter;
        LineCounter = 0;
    }

    // href handler
    void HrefHandler(void)
    {
        LineCounter++;
    }

    // Data Read
    int ReadOneByte(void)
    {
        int result;
        rclk = 1;
//        wait_us(1);
        result = data;
        rclk = 0;
        return result;
    }

    // Data Start
    void ReadStart(void)
    {
        rrst = 0;
        oe = 0;
        wait_us(1);
        rclk = 0;
        wait_us(1);
        rclk = 1;
        wait_us(1);
        rrst = 1;
    }

    // Data Stop
    void ReadStop(void)
    {
        oe = 1;
        ReadOneByte();
        rclk = 1;
    }
};
#endif //OV7670_OV7670_H
