#define DEBUG

#include "mbed.h"
#include "OV7670.h"
#include "SDFileSystem.h"

// 画像フォーマット
enum COLOR_FORMATS {
    RGB444 = 1,
    RGB555 = 2,
    RGB565 = 3,
    YUV    = 4,
    BAYER  = 5,
};

// 画像サイズ
enum IMAGE_SIZE {
    VGA_640x480   = 1, //Bayer ONLY
    MAX_544x360   = 2,
    VGA_480x360   = 3,
    QVGA_320x240  = 4,
    QQVGA_160x120 = 5,
};

uint8_t colorFormat = BAYER;    //MEMO: カラーフォーマット
uint8_t imageSize = MAX_544x360;//MEMO: 画像サイズ

// 状態管理
enum DeviceState {
    INIT     = 0x00,
    IDLE     = 0x01,
    ACTIVE   = 0x02,
    BUSY     = 0x03,
    ERROR    = 0x80
};

uint8_t currentStatus = INIT;
uint8_t isCameraBusy = 0;

/**
 * OV7670 FIFO Specification
 *
 * 3V3      Power Supply (3.3v)
 * GND      Ground (0v)
 * SIOC     SCCB Clock              INPUT
 * SIOD     SCCB Data               INPUT/OUTPUT
 * VSYNC    垂直同期信号              OUTPUT
 * HREF     水平同期信号              OUTPUT
 * D7-D0    Data Bus                INPUT/OUTPUT
 * /RST     RESET                   INPUT
 * PWDN     POWER DOWN              INPUT
 * STR      LED/STROBE control      OUTPUT
 * RCK      FIFO READ CLOCK (RCLK)  INPUT
 * /WR      FIFO Write enable (WEN) INPUT
 * /OE      FIFO Output enable (CE) INPUT
 * /WRST    FIFO Write reset        INPUT
 * /RRST    FIFO Read reset         INPUT
 */
OV7670 camera (
        p28,p27,       // SDA(SIOD),SCL(SIOC)
        p12,p11,p10,   // VSYNC,HREF,WEN(FIFO)
        p24,p15,p25,p16,p26,p17,p29,p18, // D7-D0
        p20,p30,p19,p23); // RRST,OE,RCLK,WRST

/**
 * SD Card
 */
//LocalFileSystem local("local");
SDFileSystem sd(p5, p6, p7, p8, "sd"); //mosi, miso, sclk, cs, mount point

/**
 * Serial
 */
Serial serial(USBTX,USBRX) ;

/**
 * image Size
 */
int sizex = 0;
int sizey = 0;

/**
 * flags
 */
uint8_t isActive;

/**
 * LED
 */
DigitalOut *led1; // LED

/**
 * Buttons
 */
InterruptIn *enableDevice;    // センサ開始トリガ

#ifdef DEBUG
#define DEBUG_PRINT(fmt) serial.printf(fmt)
#define DEBUG_PRINTF(fmt, ...) serial.printf(fmt, __VA_ARGS__)
#define DEBUG_PUTC(x) serial.putc(x)
#else
#define DEBUG_PRINT(fmt)
#define DEBUG_PRINTF(fmt, ...)
#define DEBUG_PUTC(x)
#endif

// Function Prototypes --------------------------------------------------------
#define FILEHEADERSIZE 14   //ファイルヘッダのサイズ
#define INFOHEADERSIZE 40   //情報ヘッダのサイズ
#define HEADERSIZE (FILEHEADERSIZE+INFOHEADERSIZE)

int create_header(FILE *fp, int width, int height);
uint8_t sdCardWriteTest();
uint8_t captureImage();

static void startCapture();
static void stopCapture();

// Main  ----------------------------------------------------------------------

int main()
{
    //-------------------------------------
    // Init
    //-------------------------------------

    //TODO: get actual time from RTC
    set_time(1546268400);  // 2019-01-01 00:00:00

    // set active flag
    isActive = 1;

    /**
     * Init Serial
     */
    serial.baud(115200);

    /**
     * Init SD Card
     */
    DEBUG_PRINT("SD Card Write Test\r\n");
    sdCardWriteTest();

    /**
     * Init Camera
     */
    // カメラリセット（ソフトウェアリセット）
    DEBUG_PRINT("Camera resetting..\r\n");
    camera.Reset();

    // 初期化前のレジスタの値を出力する
    DEBUG_PRINT("Print Register Before Initialization...\r\n");
    camera.PrintRegister();

    // カラーフォーマット選択
    switch (colorFormat) {
        case RGB444:
            camera.InitRGB444();
            break;
        case RGB555:
            camera.InitRGB555();
            break;
        case RGB565:
            camera.InitRGB565();
            break;
        case YUV:
            camera.InitYUV();
            break;
        case BAYER:
        default:
            camera.InitBayerRGB();
            break;
    }

    // 画像サイズ選択
    switch (imageSize) {
        case VGA_640x480:
            sizex = 640;
            sizey = 480;
            camera.InitVGA();
            break;
        case MAX_544x360:
            sizex = 544;
            sizey = 360;
            camera.InitFIFO_2bytes_color_nealy_limit_size();
            break;
        case VGA_480x360:
            sizex = 480;
            sizey = 360;
            camera.InitVGA_3_4();
            break;
        case QVGA_320x240:
            sizex = 320;
            sizey = 240;
            camera.InitQVGA();
            break;
        case QQVGA_160x120:
        default:
            sizex = 160;
            sizey = 120;
            camera.InitQQVGA();
            break;
    }

//    camera.InitForFIFOWriteReset();
    camera.InitDefaultReg();

    // 初期化後のレジスタの値を出力する
    DEBUG_PRINT("Print Register After Initialization...\r\n");
    camera.PrintRegister();

    /**
     * Init Buttons
     */
    enableDevice = new InterruptIn(p9);     // センサ開始／停止信号
    enableDevice->mode(PullUp);
    enableDevice->fall(&startCapture);
    enableDevice->rise(&stopCapture);

    /**
     * Init LED
     */
    led1 = new DigitalOut(LED1);
    led1->write(0); // set led off

    // CAPTURE and SEND LOOP
    currentStatus = IDLE;
    while(isActive)
    {
        if(currentStatus == ACTIVE && !isCameraBusy) {
            captureImage();
        }

        // blink led
        led1->write(!led1->read());
        wait(0.5f);
    }
}

// Functions -------------------------------------------------------------------
int create_header(FILE *fp, int width, int height) {
    int real_width;
    unsigned char header_buf[HEADERSIZE]; //ヘッダを格納する
    unsigned int file_size;
    unsigned int offset_to_data;
    unsigned long info_header_size;
    unsigned int planes;
    unsigned int color;
    unsigned long compress;
    unsigned long data_size;
    long xppm;
    long yppm;

    real_width = width*3 + width%4;

    //ここからヘッダ作成
    file_size = height * real_width + HEADERSIZE;
    offset_to_data = HEADERSIZE;
    info_header_size = INFOHEADERSIZE;
    planes = 1;
    color = 24;
    compress = 0;
    data_size = height * real_width;
    xppm = 1;
    yppm = 1;

    header_buf[0] = 'B';
    header_buf[1] = 'M';
    memcpy(header_buf + 2, &file_size, sizeof(file_size));
    header_buf[6] = 0;
    header_buf[7] = 0;
    header_buf[8] = 0;
    header_buf[9] = 0;
    memcpy(header_buf + 10, &offset_to_data, sizeof(offset_to_data));
    memcpy(header_buf + 14, &info_header_size, sizeof(info_header_size));
    memcpy(header_buf + 18, &width, sizeof(width));
    height = height * -1; // データ格納順が逆なので、高さをマイナスとしている
    memcpy(header_buf + 22, &height, sizeof(height));
    memcpy(header_buf + 26, &planes, sizeof(planes));
    memcpy(header_buf + 28, &color, sizeof(color));
    memcpy(header_buf + 30, &compress, sizeof(compress));
    memcpy(header_buf + 34, &data_size, sizeof(data_size));
    memcpy(header_buf + 38, &xppm, sizeof(xppm));
    memcpy(header_buf + 42, &yppm, sizeof(yppm));
    header_buf[46] = 0;
    header_buf[47] = 0;
    header_buf[48] = 0;
    header_buf[49] = 0;
    header_buf[50] = 0;
    header_buf[51] = 0;
    header_buf[52] = 0;
    header_buf[53] = 0;

    //ヘッダの書き込み
    fwrite(header_buf, sizeof(unsigned char), HEADERSIZE, fp);

    return 0;
}

uint8_t sdCardWriteTest() {

    FILE *fp = fopen("/sd/ov7670_sd_write_test.txt", "w");

    if (NULL != fp) {
        DEBUG_PRINT("File opened. Start writing...\r\n");
        fprintf(fp, "SD card file write test done by OV7670");
        fclose(fp);
    } else {
        DEBUG_PRINT("Failed to file open. Exiting...\r\n");
        error("File Open failed.\r\n" );
    }

    DEBUG_PRINT("SD card file write test completed.\r\n");

    return 0;
}

uint8_t captureImage () {

    // set flag as busy
    isCameraBusy = 1;

    int real_width = sizex*3 + sizey%4;

    unsigned char *bmp_line_data; //画像バッファ
    if((bmp_line_data = (unsigned char *)malloc(sizeof(unsigned char)*real_width)) == NULL){
        fprintf(stderr, "Error: Allocation error.\n");
        return 1;
    }

    //RGB情報を4バイトの倍数に合わせている　MEMO:ゼロフィルしている？何となく不要な気もする
    for(int i=sizex*3; i<real_width; i++){
        bmp_line_data[i] = 0;
    }

    FILE *fp;

    // set filename
    //    const char *filename = "/sd/test.bmp"; //TODO: Use timestamp string for unique filename
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char filename[128];
    sprintf(filename, "/sd/image_%d%d%d%d%d%d.bmp", tm.tm_year+1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    DEBUG_PRINTF("Filename:%s\r\n", filename);

    if((fp = fopen(filename, "wb")) == NULL){
        serial.printf("Error: %s could not open.", filename);
        return 1;
    }

    create_header(fp, sizex, sizey);
    camera.InitForFIFOWriteReset();
    camera.CaptureNext();
    while(camera.CaptureDone() == false);
    camera.ReadStart();

    int r=0, g=0, b=0, d1, d2;

    /**
     * - Color Formats -
     * RGB444 = 1,
        RGB555 = 2,
        RGB565 = 3,
        YUV    = 4,
        BAYER  = 5,
     */
    switch (colorFormat) {
        case RGB444: //FIXME: ise emi,
        case RGB555:
        case RGB565:
            for (int y=0; y<sizey; y++) {
                for (int x=0; x<sizex; x++) {
                    d1 = camera.ReadOneByte();
                    d2 = camera.ReadOneByte();

                    switch (colorFormat) {
                        case RGB444:
                            // RGB444 to RGB888
                            b = (d1 & 0x0F) << 4;
                            g = (d2 & 0xF0);
                            r = (d2 & 0x0F) << 4;
                            break;
                        case RGB555:
                            // RGB555 to RGB888
                            b = (d1 & 0x1F) << 3;
                            g = (((d1 & 0xE0) >> 2) | ((d2 & 0x03) << 6));
                            r = (d2 & 0x7c) << 1;
                            break;
                        case RGB565:
                            // RGB565 to RGB888
                            b = (d1 & 0x1F) << 3;
                            g = (((d1 & 0xE0) >> 3) | ((d2 & 0x07) << 5));
                            r = (d2 & 0xF8);
                            break;
                        default:
                            break;
                    }
                    bmp_line_data[x*3]     = (unsigned char)b;
                    bmp_line_data[x*3 + 1] = (unsigned char)g;
                    bmp_line_data[x*3 + 2] = (unsigned char)r;
                }
                fwrite(bmp_line_data, sizeof(unsigned char), real_width, fp);
            }
            break;

        case YUV: {
            int index = 0;
            for (int y = 0; y < sizey; y++) {
                int U0 = 0, Y0 = 0, V0 = 0, Y1 = 0;
                for (int x = 0; x < sizex; x++) {
                    if (index % 2 == 0) {
                        U0 = camera.ReadOneByte();
                        Y0 = camera.ReadOneByte();
                        V0 = camera.ReadOneByte();
                        Y1 = camera.ReadOneByte();

                        b = Y0 + 1.77200 * (U0 - 128);
                        g = Y0 - 0.34414 * (U0 - 128) - 0.71414 * (V0 - 128);
                        r = Y0 + 1.40200 * (V0 - 128);
                    } else {
                        b = Y1 + 1.77200 * (U0 - 128);
                        g = Y1 - 0.34414 * (U0 - 128) - 0.71414 * (V0 - 128);
                        r = Y1 + 1.40200 * (V0 - 128);
                    }

                    b = min(max(b, 0), 255);
                    g = min(max(g, 0), 255);
                    r = min(max(r, 0), 255);

                    bmp_line_data[x * 3] = (unsigned char) b;
                    bmp_line_data[x * 3 + 1] = (unsigned char) g;
                    bmp_line_data[x * 3 + 2] = (unsigned char) r;
                    index++;
                }
                fwrite(bmp_line_data, sizeof(unsigned char), (size_t) real_width, fp);
            }
        }
            break;

        case BAYER: {
            unsigned char *bayer_line[2];
            unsigned char *bayer_line_data[2]; //画像1行分のRGB情報を格納する2行分

            for (int i = 0; i < 2; i++) {
                if ((bayer_line_data[i] = (unsigned char *) malloc(sizeof(unsigned char) * sizex)) == NULL) {
                    fprintf(stderr, "Error: Allocation error.\n");
                    return 1;
                }
            }

            for (int x = 0; x < sizex; x++) {
                // odd line BGBG... even line GRGR...
                bayer_line_data[0][x] = (unsigned char) camera.ReadOneByte();
            }
            bayer_line[1] = bayer_line_data[0];

            for (int y = 1; y < sizey; y++) {
                int line = y % 2;

                for (int x = 0; x < sizex; x++) {
                    // odd line BGBG... even line GRGR...
                    bayer_line_data[line][x] = (unsigned char) camera.ReadOneByte();
                }
                bayer_line[0] = bayer_line[1];
                bayer_line[1] = bayer_line_data[line];

                for (int x = 0; x < sizex - 1; x++) {
                    if (y % 2 == 1) {
                        if (x % 2 == 0) {
                            // BG
                            // GR
                            b = bayer_line[0][x];
                            g = ((int) bayer_line[0][x + 1] + bayer_line[1][x]) >> 1;
                            r = bayer_line[1][x + 1];
                        } else {
                            // GB
                            // RG
                            b = bayer_line[0][x + 1];
                            g = ((int) bayer_line[0][x] + bayer_line[1][x + 1]) >> 1;
                            r = bayer_line[1][x];
                        }
                    } else {
                        if (x % 2 == 0) {
                            // GR
                            // BG
                            b = bayer_line[1][x];
                            g = ((int) bayer_line[0][x] + bayer_line[1][x + 1]) >> 1;
                            r = bayer_line[0][x + 1];
                        } else {
                            // RG
                            // GB
                            b = bayer_line[1][x + 1];
                            g = ((int) bayer_line[0][x + 1] + bayer_line[1][x]) >> 1;
                            r = bayer_line[0][x];
                        }
                    }
                    bmp_line_data[x * 3] = (unsigned char) b;
                    bmp_line_data[x * 3 + 1] = (unsigned char) g;
                    bmp_line_data[x * 3 + 2] = (unsigned char) r;
                }

                fwrite(bmp_line_data, sizeof(unsigned char), real_width, fp);
            }

            for (int i = 0; i < 2; i++) {
                free(bayer_line_data[i]);
            }
            fwrite(bmp_line_data, sizeof(unsigned char), real_width, fp);
        }
            break;

        default:
            break;
    }

    camera.ReadStop();

    free(bmp_line_data);
    fclose(fp);

    // clear
    isCameraBusy = 0;

    return 0;
}

static void startCapture() {
    currentStatus = ACTIVE;
}

static void stopCapture() {
    currentStatus = IDLE;
}
