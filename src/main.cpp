// define/undef
#define BITMAPFILE
#undef BAYERBITMAPFILE
#undef HEXFILE
#define COLOR_BAR

#include "mbed.h"
#include "OV7670.h"
#include "SDFileSystem.h"

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
 * RCK      FIFO READ CLOCK         INPUT
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

//LocalFileSystem local("local");
SDFileSystem sd(p5, p6, p7, p8, "sd"); //mosi, miso, sclk, cs, mount point

Serial pc(USBTX,USBRX) ;

int sizex = 0;
int sizey = 0;

#if defined(BITMAPFILE) || defined(BAYERBITMAPFILE)

#define FILEHEADERSIZE 14                   //ファイルヘッダのサイズ
#define INFOHEADERSIZE 40                   //情報ヘッダのサイズ
#define HEADERSIZE (FILEHEADERSIZE+INFOHEADERSIZE)

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
    int headerSIze = fwrite(header_buf, sizeof(unsigned char), HEADERSIZE, fp);
    pc.printf("Header Size: %d\r\n", headerSIze);

    return 0;
}
#endif

int main()
{

    pc.baud(115200);

    // カメラリセット（ソフトウェアリセット）F
    pc.printf("Camera resetting..\r\n");
    camera.Reset();

    // 初期化前のレジスタの値を出力する
    pc.printf("Before Init...\r\n");
    camera.PrintRegister();

    // カラーフォーマット選択
    pc.printf("Select color format.\r\n");
    pc.printf("1: RGB444.\r\n");
    pc.printf("2: RGB555.\r\n");
    pc.printf("3: RGB565.\r\n");
    pc.printf("4: YUV(UYVY).\r\n");
    pc.printf("5: Bayer RGB(BGBG... GRGR...).\r\n");

    // 入力待ち(1-5)
    while(!pc.readable());

    // カラーフォーマット設定
    char color_format = pc.getc();
    switch (color_format) {
        case '1':
            camera.InitRGB444();
            break;
        case '2':
            camera.InitRGB555();
            break;
        case '3':
            camera.InitRGB565();
            break;
        case '4':
            camera.InitYUV();
            break;
        case '5':
            camera.InitBayerRGB();
            break;
    }
    pc.printf("select %c\r\n", color_format);

    // 画像サイズ設定
    pc.printf("Select screen size.\r\n") ;
    switch (color_format) {
        case '5':
            pc.printf("1: VGA(640x480).\r\n");
        case '1':
        case '2':
        case '3':
        case '4':
            pc.printf("2: FIFO nealy limit(544x360).\r\n");
            pc.printf("3: VGA*3/4(480x360).\r\n");
            pc.printf("4: QVGA(320x240).\r\n");
            pc.printf("5: QQVGA(160x120).\r\n");
            break;
        default:
            //TODO: 範囲外の値入力を考慮
            break;
    }

    // 入力待ち(1-5)
    while(!pc.readable());

    // 画像サイズ設定
    char screen_size = pc.getc() ;
    switch (screen_size) {
        case '1':
            sizex = 640;
            sizey = 480;
            camera.InitVGA();
            break;
        case '2':
            sizex = 544;
            sizey = 360;
            camera.InitFIFO_2bytes_color_nealy_limit_size();
            break;
        case '3':
            sizex = 480;
            sizey = 360;
            camera.InitVGA_3_4();
            break;
        case '4':
            sizex = 320;
            sizey = 240;
            camera.InitQVGA();
            break;
        case '5':
            sizex = 160;
            sizey = 120;
            camera.InitQQVGA();
            break;
        default:
            //TODO: 範囲外の値入力を考慮
            break;
    }
    pc.printf("select %c\r\n", screen_size);

//    camera.InitForFIFOWriteReset();
    camera.InitDefaultReg();

#ifdef COLOR_BAR
    // カラーバー機能を有効にする
    camera.InitSetColorbar();
#endif

    // 初期化後のレジスタの値を出力する
    pc.printf("After Init...\r\n");
    camera.PrintRegister();

    // CAPTURE and SEND LOOP
    while(1)
    {
#if defined(BITMAPFILE) || defined(BAYERBITMAPFILE) || defined(HEXFILE)
        pc.printf("Hit Any Key %dx%d Capture Data.\r\n", sizex, sizey) ;
        while(!pc.readable());
        pc.printf("*\r\n");
        pc.getc();

        int real_width = sizex*3 + sizey%4;

        unsigned char *bmp_line_data; //画像1行分のRGB情報を格納する
        if((bmp_line_data = (unsigned char *)malloc(sizeof(unsigned char)*real_width)) == NULL){
           fprintf(stderr, "Error: Allocation error.\n");
           return 1;
        }

        //RGB情報を4バイトの倍数に合わせている
        for(int i=sizex*3; i<real_width; i++){
            bmp_line_data[i] = 0;
        }
#endif
#if defined(BITMAPFILE) || defined(BAYERBITMAPFILE)
        FILE *fp;
//        const char *filename = "/local/test.bmp";
        const char *filename = "/sd/test.bmp";
        if((fp = fopen(filename, "wb")) == NULL){
            pc.printf("Error: %s could not open.", filename);
            return 1;
        }

        create_header(fp, sizex, sizey);
#endif
#ifdef HEXFILE
        FILE *fp2;
//        const char *filename2 = "/local/test.txt";
        const char *filename2 = "/sd/test.txt";
        if((fp2 = fopen(filename2, "w")) == NULL){
            pc.printf("Error: %s could not open.", filename2);
            return 1;
        }
#endif

        camera.InitForFIFOWriteReset();
        camera.CaptureNext();
        while(camera.CaptureDone() == false);
        camera.ReadStart();

        int r=0, g=0, b=0, d1, d2;

        switch (color_format) {
            case '1':
            case '2':
            case '3':
                for (int y=0; y<sizey; y++) {
                    for (int x=0; x<sizex; x++) {
                        d1 = camera.ReadOneByte();
                        d2 = camera.ReadOneByte();

                        switch (color_format) {
                            case '1':
                                // RGB444 to RGB888
                                b = (d1 & 0x0F) << 4;
                                g = (d2 & 0xF0);
                                r = (d2 & 0x0F) << 4;
                                break;
                            case '2':
                                // RGB555 to RGB888
                                b = (d1 & 0x1F) << 3;
                                g = (((d1 & 0xE0) >> 2) | ((d2 & 0x03) << 6));
                                r = (d2 & 0x7c) << 1;
                                break;
                            case '3':
                                // RGB565 to RGB888
                                b = (d1 & 0x1F) << 3;
                                g = (((d1 & 0xE0) >> 3) | ((d2 & 0x07) << 5));
                                r = (d2 & 0xF8);
                                break;
                        }
#if defined(BITMAPFILE) || defined(HEXFILE)
                        bmp_line_data[x*3]     = (unsigned char)b;
                        bmp_line_data[x*3 + 1] = (unsigned char)g;
                        bmp_line_data[x*3 + 2] = (unsigned char)r;
#endif
/*
                        // RGB
                        pc.printf ("%2X%2X%2X", r, g, b) ;
*/
                    }
#ifdef BITMAPFILE
                    fwrite(bmp_line_data, sizeof(unsigned char), real_width, fp);
#endif
#ifdef HEXFILE
                    for(int i=0; i<sizex*3; i++){
                        fprintf(fp2, "%02X", bmp_line_data[i]);
                    }
#endif
//                    pc.printf("\r\n") ;
                }
                break;

            case '4': {
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

#if defined(BITMAPFILE) || defined(HEXFILE)
                        bmp_line_data[x * 3] = (unsigned char) b;
                        bmp_line_data[x * 3 + 1] = (unsigned char) g;
                        bmp_line_data[x * 3 + 2] = (unsigned char) r;
#endif
/*
                        // RGB
                        pc.printf ("%2X%2X%2X", r, g, b) ;
*/
                        index++;
                    }
#ifdef BITMAPFILE
                    fwrite(bmp_line_data, sizeof(unsigned char), (size_t) real_width, fp);
#endif
#ifdef HEXFILE
                    for(int i=0; i<sizex*3; i++){
                        fprintf(fp2, "%02X", bmp_line_data[i]);
                    }
#endif
//                    pc.printf("\r\n") ;
                }
            }
                break;

            case '5': {
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
#ifdef BAYERBITMAPFILE
                    bmp_line_data[x*3]     = (unsigned char)bayer_line_data[0][x];
                    bmp_line_data[x*3 + 1] = (unsigned char)bayer_line_data[0][x];
                    bmp_line_data[x*3 + 2] = (unsigned char)bayer_line_data[0][x];
#endif
                }
#ifdef BAYERBITMAPFILE
                fwrite(bmp_line_data, sizeof(unsigned char), real_width, fp);
#endif
                bayer_line[1] = bayer_line_data[0];

                for (int y = 1; y < sizey; y++) {
                    int line = y % 2;

                    for (int x = 0; x < sizex; x++) {
                        // odd line BGBG... even line GRGR...
                        bayer_line_data[line][x] = (unsigned char) camera.ReadOneByte();
#ifdef BAYERBITMAPFILE
                        bmp_line_data[x*3]     = (unsigned char)bayer_line_data[line][x];
                        bmp_line_data[x*3 + 1] = (unsigned char)bayer_line_data[line][x];
                        bmp_line_data[x*3 + 2] = (unsigned char)bayer_line_data[line][x];
#endif
                    }
#ifdef BAYERBITMAPFILE
                    fwrite(bmp_line_data, sizeof(unsigned char), real_width, fp);
#endif
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
#if defined(BITMAPFILE) || defined(HEXFILE)
                        bmp_line_data[x * 3] = (unsigned char) b;
                        bmp_line_data[x * 3 + 1] = (unsigned char) g;
                        bmp_line_data[x * 3 + 2] = (unsigned char) r;
#endif
                    }

#ifdef BITMAPFILE
                    fwrite(bmp_line_data, sizeof(unsigned char), real_width, fp);
#endif

#ifdef HEXFILE
                    for(int i=0; i<sizex*3; i++){
                        fprintf(fp2, "%02X", bmp_line_data[i]);
                    }
#endif
                }

                for (int i = 0; i < 2; i++) {
                    free(bayer_line_data[i]);
                }
#ifdef BITMAPFILE
                fwrite(bmp_line_data, sizeof(unsigned char), real_width, fp);
#endif
            }
                break;

            default:
                //TODO: 範囲外の値入力を考慮
                break;


        }
        camera.ReadStop();

#if defined(BITMAPFILE) || defined(BAYERBITMAPFILE)
        free(bmp_line_data);
        fclose(fp);
#endif
#ifdef HEXFILE
        fclose(fp2);
#endif

    }
}