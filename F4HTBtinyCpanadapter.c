
#include <linux/fb.h> 
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <fftw3.h>
#include <rtl-sdr.h>
#include <complex.h>
#include <string.h>

#include "waterfallcolor.c"

#include "F4HTBtinyCpanadapter.h" 

void fft_deinit(void);
void setup_fft (void);
static void async_read_callback(uint8_t *n_buf, uint32_t len, void *ctx);
void put_pixel_24bpp(int x, int y, char r, char g, char b) ;

//###########################global variables##########################

struct video_calc{
	unsigned int refreshtime=16666; //refresh time, still wait befor set new spectrum line
	unsigned char scale_db=4; //set pixer per db
	unsigned char scale_freq=10; //Set zoom on frequency

	int wf_noise_floor_db = 0; //minimum lvl of watterfall
	int spc_noise_floor_db = 0; //minimum level of spectrum

	bool rev_frequency = 0; //reverse or not i/q
	unsigned char spect_smoothing_n=8; //number of spectrum accumulation
	unsigned char spect_smoothing_counter=0; //counter for spectrum accumulation
	char * spec_values; //buffer of spectrum accumulation
	int multip = 8; //scale rtl and fft buffer on multiple of screensize like n*512 for bether performance
}VC;

struct btr {
    unsigned char pin_in[3]={16,20,21}; //set gpio number rotary, rotary sens, selector button
	unsigned char pin_act=12; //Set pin activation is use a pin for voltage power or rotary. Used for maybe inibition
    pthread_t t[3];  //thread for rotary button survey
}btr;

//##################################RTL variables
static rtlsdr_dev_t *rtl_dev;
struct rtlinfo{
int 		_samp_rate = 1024000,
			_dev_id=0, 
			_center_freq=68330000, 
			_gain = 14, 
			_offset_tuning = 1;
}rtl;

//##################################FFT variables
struct fftwInfo {
    fftw_complex * in ;
    fftw_complex * out;
    fftw_plan plan;
    int outlen;
	int inlen = 1024;
    double binWidth;
    double * currentLine;
	float * window;
}fftw;

//##################################Video variables
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
char * framebuffer; //pointer to draw a frame
char * fbp = 0; //pointer to framebuffer
long int screensize = 0; //total screen size
int fbfd = 0;

unsigned int midle_screen_x = 0; //one time calculation of x possition
unsigned int midle_screen_y = 0; //one time calculation of y midle possition

//##################################Other variable

char tmpchar[20];
time_t secondtextshow = time(NULL);

static char font[95][8] = {
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00 },	/*   (032) 000 */
    { 0x08,0x08,0x08,0x08,0x08,0x00,0x08 },	/* ! (033) 001 */
    { 0x66,0x66,0x00,0x00,0x00,0x00,0x00 },	/* " (034) 002 */
    { 0x24,0x24,0xFF,0x24,0xFF,0x24,0x24 },	/* # (035) 003 */
    { 0x08,0x3E,0x28,0x3E,0x0A,0x3E,0x08 },	/* $ (036) 004 */
    { 0x01,0x62,0x64,0x08,0x13,0x23,0x40 },	/* % (037) 005 */
    { 0x1C,0x22,0x20,0x1D,0x42,0x42,0x3D },	/* & (038) 006 */
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00 },	/*   (039) 007 */
    { 0x02,0x04,0x08,0x08,0x08,0x04,0x02 },	/* ( (040) 008 */
    { 0x40,0x20,0x10,0x10,0x10,0x20,0x40 },	/* ) (041) 009 */
    { 0x00,0x49,0x2A,0x1C,0x2A,0x49,0x00 },	/* * (042) 010 */
    { 0x00,0x08,0x08,0x3E,0x08,0x08,0x00 },	/* + (043) 011 */
    { 0x00,0x00,0x00,0x00,0x60,0x20,0x40 },	/* , (044) 012 */
    { 0x00,0x00,0x00,0x7E,0x00,0x00,0x00 },	/* - (045) 013 */
    { 0x00,0x00,0x00,0x00,0x00,0x60,0x60 },	/* . (046) 014 */
    { 0x01,0x02,0x04,0x08,0x10,0x20,0x40 },	/* / (047) 015 */
    { 0x3C,0x42,0x46,0x4A,0x52,0x62,0x3C },	/* 0 (048) 016 */
    { 0x06,0x0A,0x12,0x02,0x02,0x02,0x02 },	/* 1 (049) 017 */
    { 0x7E,0x02,0x02,0x7E,0x40,0x40,0x7E },	/* 2 (050) 018 */
    { 0x7E,0x02,0x02,0x7E,0x02,0x02,0x7E },	/* 3 (051) 019 */
    { 0x42,0x42,0x42,0x7E,0x02,0x02,0x02 },	/* 4 (052) 020 */
    { 0x7E,0x40,0x40,0x7E,0x02,0x02,0x7E },	/* 5 (053) 021 */
    { 0x7E,0x40,0x40,0x7E,0x42,0x42,0x7E },	/* 6 (054) 022 */
    { 0x7E,0x02,0x04,0x08,0x10,0x20,0x40 },	/* 7 (055) 023 */
    { 0x7E,0x42,0x42,0x7E,0x42,0x42,0x7E },	/* 8 (056) 024 */
    { 0x7E,0x42,0x42,0x7E,0x02,0x02,0x7E },	/* 9 (057) 025 */
    { 0x00,0x00,0x18,0x00,0x18,0x00,0x00 },	/* : (058) 026 */
    { 0x00,0x00,0x18,0x00,0x18,0x08,0x10 },	/* ; (059) 027 */
    { 0x04,0x08,0x10,0x20,0x10,0x08,0x04 },	/* < (060) 028 */
    { 0x00,0x00,0x7E,0x00,0x7E,0x00,0x00 },	/* = (061) 029 */
    { 0x20,0x10,0x08,0x04,0x08,0x10,0x20 },	/* > (062) 030 */
    { 0x3E,0x41,0x02,0x04,0x08,0x00,0x08 },	/* ? (063) 031 */
    { 0x3E,0xC3,0x99,0xA5,0xA6,0xD8,0x7E },	/* @ (064) 032 */
    { 0x3C,0x42,0x42,0x7E,0x42,0x42,0x42 },	/* A (065) 033 */
    { 0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C },	/* B (066) 034 */
    { 0x7E,0x40,0x40,0x40,0x40,0x40,0x7E },	/* C (067) 035 */
    { 0x7C,0x42,0x42,0x42,0x42,0x42,0x7C },	/* D (068) 036 */
    { 0x7E,0x40,0x40,0x7E,0x40,0x40,0x7E },	/* E (069) 037 */
    { 0x7E,0x40,0x40,0x7E,0x40,0x40,0x40 },	/* F (070) 038 */
    { 0x7E,0x40,0x40,0x5E,0x42,0x42,0x7E },	/* G (071) 039 */
    { 0x42,0x42,0x42,0x7E,0x42,0x42,0x42 },	/* H (072) 040 */
    { 0x10,0x10,0x10,0x10,0x10,0x10,0x10 },	/* I (073) 041 */
    { 0x02,0x02,0x02,0x02,0x02,0x02,0x3C },	/* J (074) 042 */
    { 0x42,0x44,0x48,0x70,0x48,0x44,0x42 },	/* K (075) 043 */
    { 0x40,0x40,0x40,0x40,0x40,0x40,0x7E },	/* L (076) 044 */
    { 0x42,0x66,0x5A,0x42,0x42,0x42,0x42 },	/* M (077) 045 */
    { 0x41,0x61,0x51,0x49,0x45,0x43,0x41 },	/* N (078) 046 */
    { 0x3C,0x42,0x42,0x42,0x42,0x42,0x3C },	/* O (079) 047 */
    { 0x7F,0x41,0x41,0x7F,0x40,0x40,0x40 },	/* P (080) 048 */
    { 0x3C,0x42,0x42,0x42,0x42,0x42,0x3D },	/* Q (081) 049 */
    { 0x7C,0x42,0x42,0x7C,0x44,0x42,0x41 },	/* R (082) 050 */
    { 0x3E,0x40,0x40,0x3C,0x02,0x02,0x7C },	/* S (083) 051 */
    { 0x7F,0x08,0x08,0x08,0x08,0x08,0x08 },	/* T (084) 052 */
    { 0x41,0x41,0x41,0x41,0x41,0x41,0x3E },	/* U (085) 053 */
    { 0x41,0x41,0x41,0x41,0x22,0x14,0x08 },	/* V (086) 054 */
    { 0x41,0x49,0x49,0x49,0x49,0x49,0x36 },	/* W (087) 055 */
    { 0x41,0x22,0x14,0x1C,0x14,0x22,0x41 },	/* X (088) 056 */
    { 0x41,0x22,0x14,0x08,0x08,0x08,0x08 },	/* Y (089) 057 */
    { 0x7F,0x02,0x04,0x08,0x10,0x20,0x7F },	/* Z (090) 058 */
    { 0x0F,0x08,0x08,0x08,0x08,0x08,0x0F },	/* [ (091) 059 */
    { 0x80,0x40,0x28,0x10,0x08,0x04,0x02 },	/* \ (092) 060 */
    { 0xF8,0x08,0x08,0x08,0x08,0x08,0xF8 },	/* ] (093) 061 */
    { 0x00,0x14,0x22,0x00,0x00,0x00,0x00 },	/* ^ (094) 062 */
    { 0x00,0x00,0x00,0x00,0x00,0x00,0xFF },	/* _ (095) 063 */
    { 0x80,0x20,0x00,0x00,0x00,0x00,0x00 },	/* ` (096) 064 */
    { 0x3C,0x42,0x42,0x7E,0x42,0x42,0x42 },	/* a (097) 065 */
    { 0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C },	/* b (098) 066 */
    { 0x7E,0x40,0x40,0x40,0x40,0x40,0x7E },	/* c (099) 067 */
    { 0x7C,0x42,0x42,0x42,0x42,0x42,0x7C },	/* d (100) 068 */
    { 0x7E,0x40,0x40,0x7E,0x40,0x40,0x7E },	/* e (101) 069 */
    { 0x7E,0x40,0x40,0x7E,0x40,0x40,0x40 },	/* f (102) 070 */
    { 0x7E,0x40,0x40,0x5E,0x42,0x42,0x7E },	/* g (103) 071 */
    { 0x42,0x42,0x42,0x7E,0x42,0x42,0x42 },	/* h (104) 072 */
    { 0x10,0x10,0x10,0x10,0x10,0x10,0x10 },	/* i (105) 073 */
    { 0x02,0x02,0x02,0x02,0x02,0x02,0x3C },	/* j (106) 074 */
    { 0x42,0x44,0x48,0x70,0x48,0x44,0x42 },	/* k (107) 075 */
    { 0x40,0x40,0x40,0x40,0x40,0x40,0x7E },	/* l (108) 076 */
    { 0x42,0x66,0x5A,0x42,0x42,0x42,0x42 },	/* m (109) 077 */
    { 0x41,0x61,0x51,0x49,0x45,0x43,0x41 },	/* n (110) 078 */
    { 0x3C,0x42,0x42,0x42,0x42,0x42,0x3C },	/* o (111) 079 */
    { 0x7F,0x41,0x41,0x7F,0x40,0x40,0x40 },	/* p (112) 080 */
    { 0x3C,0x42,0x42,0x42,0x42,0x42,0x3D },	/* q (113) 081 */
    { 0x7C,0x42,0x42,0x7C,0x44,0x42,0x41 },	/* r (114) 082 */
    { 0x3E,0x40,0x40,0x3C,0x02,0x02,0x7C },	/* s (115) 083 */
    { 0x7F,0x08,0x08,0x08,0x08,0x08,0x08 },	/* t (116) 084 */
    { 0x41,0x41,0x41,0x41,0x41,0x41,0x3E },	/* u (117) 085 */
    { 0x41,0x41,0x41,0x41,0x22,0x14,0x08 },	/* v (118) 086 */
    { 0x41,0x49,0x49,0x49,0x49,0x49,0x36 },	/* w (119) 087 */
    { 0x41,0x22,0x14,0x1C,0x14,0x22,0x41 },	/* x (120) 088 */
    { 0x41,0x22,0x14,0x08,0x08,0x08,0x08 },	/* y (121) 089 */
    { 0x7F,0x02,0x04,0x08,0x10,0x20,0x7F },	/* z (122) 090 */
    { 0x06,0x08,0x08,0x70,0x08,0x08,0x06 },	/* { (123) 091 */
    { 0x18,0x18,0x18,0x18,0x18,0x18,0x18 },	/* | (124) 092 */
    { 0x60,0x10,0x10,0x0E,0x10,0x10,0x60 },	/* } (125) 093 */
    { 0x00,0x00,0x60,0x99,0x06,0x00,0x70 },	/* ~ (126) 094 */
};


struct menu_click {
    unsigned short actual=0; //Actual menu selected
	char names[5][16] = { 	"0.Scale freq   ",
							"1.SPC Scale LvL",
							"2.SPC RF Lvl   ",
							"3.WF RF Lvl    ",
							"4.WF Color     "};
}menu;

//##################################Other

void print_char_time(char c[], int timesss) {
    if (strlen(c) > 0) strcpy(tmpchar, c);
    if (timesss > 0) secondtextshow = time(NULL) + timesss;
}



void print_char(char c[], unsigned int zoom = 2, int x = 0, int y = 0) {

    if (x == -1 || y == -1) {
        x = midle_screen_x - ((strlen(c) * 8 * zoom) / 2);
        y = (midle_screen_y / 2) - ((8 * zoom) / 2);
    }

    int numascii = 0;

    for (unsigned int f = 0; f < strlen(c); f++) {
        numascii = (int) c[f] - 32;

        for (unsigned int i = 0; i < (8 * zoom); i++) {
            for (unsigned int j = 0; j < (8 * zoom); j++) {
                if ((font[numascii][i / zoom] >> (7 - j / zoom)) & 0x01) {
                    put_pixel_24bpp(x + f * 8 * zoom + j, y + i, 255, 255, 255);

                } else {
                    put_pixel_24bpp(x + f * 8 * zoom + j, y + i, 0, 0, 0);
                }
            }
        }

    }
}

void updatetextshow() {
    if (time(NULL) < secondtextshow) print_char(tmpchar);
}

//##################################RTL SDR
static int rtlsdr_init(){
	int device_count = rtlsdr_get_device_count();
	if (!device_count) {
		printf("No supported devices found.\n");
		exit(1);
	}
	printf("Starting rtl_map ~\n");
	printf("Found %d device(s):\n", device_count);
	for(int i = 0; i < device_count; i++){
		printf("#%d: %s\n", i, rtlsdr_get_device_name(i));
	}
	int dev_open = rtlsdr_open(&rtl_dev, rtl._dev_id);
	if (dev_open < 0) {
		printf("Failed to open RTL-SDR device #%d\n", rtl._dev_id);
		exit(1);
	}else{
		printf("Using device: #%d\n", dev_open);
	}
	if(!rtl._gain){
		rtlsdr_set_tuner_gain_mode(rtl_dev, rtl._gain);
		printf("Gain mode set to auto.\n");
	}else{
		rtlsdr_set_tuner_gain_mode(rtl_dev, 1);
		int gain_count = rtlsdr_get_tuner_gains(rtl_dev, NULL);
		printf("Supported gain values (%d): ", gain_count);
		int gains[gain_count], supported_gains = rtlsdr_get_tuner_gains(rtl_dev, gains), target_gain=0;
		for (int i = 0; i < supported_gains; i++){
			if (gains[i] < rtl._gain*10){
				target_gain = gains[i];
			printf("%.1f ", gains[i] / 10.0);}
		}
		printf("\n");
		printf("Gain set to %.1f\n", target_gain / 10.0);
		rtlsdr_set_tuner_gain(rtl_dev, target_gain);
	}
	rtlsdr_set_offset_tuning(rtl_dev, rtl._offset_tuning);
	rtlsdr_set_center_freq(rtl_dev, rtl._center_freq);
	rtlsdr_set_sample_rate(rtl_dev, rtl._samp_rate);
	printf("Center frequency set to %d Hz.\n", rtl._center_freq);
	printf("Sampling at %d S/s\n", rtl._samp_rate);
	int r = rtlsdr_reset_buffer(rtl_dev);
	if (r < 0){
		printf("Failed to reset buffers.\n");
		return 1;
	}
	return 0;
}


//###########################FFT##########################
void fft_init(void) {
    
    fftw.in = (fftw_complex * ) fftw_malloc(sizeof(fftw_complex) * fftw.inlen);
    fftw.out = (fftw_complex * ) fftw_malloc(sizeof(fftw_complex) * fftw.outlen);

    fftw.plan = fftw_plan_dft_1d(fftw.outlen, fftw.in, fftw.out, FFTW_FORWARD, FFTW_MEASURE);

    fftw.currentLine = (double * ) malloc(sizeof(double) * fftw.outlen);
    memset(fftw.currentLine, 0, sizeof(double) * fftw.outlen);

    /* How many hertz does one "bin" comprise? */
    fftw.binWidth = (double) rtl._samp_rate / (double) fftw.outlen;
    printf("FFT Resolution = %.1fhz/px\nFFT bandwidth %.1fhz\nFFT out len %d samples\n", fftw.binWidth, fftw.binWidth*vinfo.xres,fftw.outlen);
}

/* Free any FFTW resources. */
void fft_deinit(void) {
    fftw_destroy_plan(fftw.plan);
    fftw_free(fftw.in);
    fftw_free(fftw.out);
    free(fftw.currentLine);
    fftw_cleanup();
}

float * windowinginit(int N) {

    float * w;

    w = (float * ) calloc(N, sizeof(float));

    //blackman harris windowing values
    float a0 = 0.35875;
    float a1 = 0.48829;
    float a2 = 0.14128;
    float a3 = 0.01168;

    //create blackman harris window
    for (int i = 0; i < N; i++) {
        w[i] = a0 - (a1 * cos((2.0 * M_PI * i) / ((N) - 1))) + (a2 * cos((4.0 * M_PI * i) / ((N) - 1))) - (a3 * cos((6.0 * M_PI * i) / ((N) - 1)));
        //w[i] = 0.5 * (1 - cos((2*M_PI)*i/(NUM_SAMPLES/2))); //hamming window
    }

    return w;

}

//###########################Frame Buffer##########################
void FB_init() {
    // Open the file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        exit(1);
    }
    printf("The framebuffer device was opened successfully.\n");

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, & finfo) == -1) {
        perror("Error reading fixed information");
        exit(2);
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, & vinfo) == -1) {
        perror("Error reading variable information");
        exit(3);
    }

    printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
	if(vinfo.bits_per_pixel < 24){printf("Please configure the Framebuffer for 24 bit minimum.%d\n", vinfo.bits_per_pixel);}
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	
    framebuffer = (char * ) calloc((screensize), sizeof(char)); //allocate memory which is same size as framebuffer

    // Map the device to memory
    fbp = (char * ) mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED,
                         fbfd, 0);
    if ((int) fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        exit(4);
    }
    printf("The framebuffer device was mapped to memory successfully.\n");
}

void put_pixel_24bpp(int x, int y, char r, char g, char b) {
    unsigned int location;
    location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) + (y + vinfo.yoffset) * finfo.line_length;
    *(framebuffer + location) = b; // Some blue
    *(framebuffer + location+1) = g; // A little green
    *(framebuffer + location+2) = r; // A lot of red
}

void put_framebuffer_fbp() {
    memcpy(fbp, framebuffer, screensize);
}

void BK_init() {

    for (unsigned int x = 0; x < vinfo.xres; x++) put_pixel_24bpp(x, (midle_screen_y), 255, 255, 255);
    put_framebuffer_fbp();
}

void setoneSPECline(char * values) {
        memset(framebuffer, 0, (screensize / 2 - finfo.line_length) * sizeof( * framebuffer));
		for (unsigned int i = 0; i < midle_screen_y; i++) {
			put_pixel_24bpp(midle_screen_x, i, 0, 255, 0);
			}
        for (unsigned int i = 0; i < vinfo.xres; i++) {
            int y = (254 - * (values + i)) * (midle_screen_y -1) / 255;
            for ( unsigned int z = y; z < (midle_screen_y-1); z++) {
				put_pixel_24bpp(i, z, colormap_rainbow[midle_screen_y-z][0], colormap_rainbow[midle_screen_y-z][1], colormap_rainbow[midle_screen_y-z][2]);
			}
        }
}

void pointer_shift(int shift, int offset) {
    shift *= vinfo.xres * vinfo.bits_per_pixel / 8;
    memmove(framebuffer + shift + offset, framebuffer + offset, (screensize - shift - offset) * sizeof( * framebuffer));
}

void setoneFFTline(char * values) {
    pointer_shift(1, (screensize / 2 + finfo.line_length));
	for (unsigned int i = 0; i < vinfo.xres; i++) {
		int y = * (values + i);
		put_pixel_24bpp(i, midle_screen_y+1, colormap_rainbow[y][0], colormap_rainbow[y][1], colormap_rainbow[y][2]);
    }
}

//###########################GPIO##########################

void checkrotary(char trtype) {
	if(trtype==0){
		menu.actual = menu.actual > 4 ? 0 : menu.actual+1;
		print_char_time(menu.names[menu.actual], 3);
		}
	else {
		char word[20];
		switch(menu.actual) {
			case 0 :
				if(trtype==1){
					VC.scale_freq++;
					if(VC.scale_freq>VC.multip){rtlsdr_cancel_async(rtl_dev);}
				}
				else if (trtype==2){
					VC.scale_freq--;
					if(VC.scale_freq==(VC.multip/2)){rtlsdr_cancel_async(rtl_dev);}
				}
				snprintf(word, sizeof(word), "scale freq %d", VC.scale_freq);
				break;
			case 1 :
				if(trtype==1){VC.scale_db++;}else if (trtype==2){VC.scale_db--;}
				snprintf(word, sizeof(word), "LvL scale %dpx/db", VC.scale_db);
				break;
			case 2 :
				if(trtype==1){VC.spc_noise_floor_db+=2;}else if (trtype==2){VC.spc_noise_floor_db-=2;}
				snprintf(word, sizeof(word), "LvL noise floor %ddb", VC.spc_noise_floor_db);
				break;
			case 3 :
				if(trtype==1){VC.wf_noise_floor_db+=2;}else if (trtype==2){VC.wf_noise_floor_db-=2;}
				snprintf(word, sizeof(word), "LvL noise floor %ddb", VC.wf_noise_floor_db);
				break;
			case 4 :
				if(trtype==1){indexlistofcolorfile++;}else if (trtype==2){indexlistofcolorfile--;}
				if(indexlistofcolorfile > indexlistofcolorfilemax)indexlistofcolorfile=0;
				if(indexlistofcolorfile < 0)indexlistofcolorfile=indexlistofcolorfilemax;
                char fName [128] ;
                snprintf(fName, sizeof fName, "./%s%s", listofcolorfile[indexlistofcolorfile],".256");
                printf("%s\n",fName);
                read_csv(fName);
                strcpy( word, listofcolorfile[indexlistofcolorfile]);
				break;
			default :
				printf("Rotary Encoder fault\n" );
		}
		print_char_time(word, 2);
	}
}

int pin_read(int pin)
{
    char path[30];
    char value_str[3];
    int fd;

    snprintf(path, 30, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return(-1);
    }

    if (-1 == read(fd, value_str, 3)) {
        fprintf(stderr, "Failed to read value!\n");
        return(-1);
    }

    close(fd);

    return(atoi(value_str));
}

void *t_lecture_rotation(void *)
{ 
    fd_set fds;
    char buffer[2];

    unsigned int enc_a_val,enc_b_val;

	int fd;
    char fName [128] ;
	sprintf (fName, "/sys/class/gpio/gpio%d/value", btr.pin_in[0]);
	
    if ((fd = open(fName, O_RDONLY)) < 0)
    {
        perror("pin");
        return 0;
    }

	while(1)
	{
		FD_ZERO(&fds);
		FD_SET(fd,&fds);

		if(select(fd+1,NULL,NULL,&fds,NULL) <0)
		{
			perror("select");
			break;
		}

		lseek(fd,0,0);
		if(read(fd,&buffer,2)!=2)
		{
			perror("read");
			break;
		}

		enc_a_val = atoi((const char*) buffer);
		enc_b_val = pin_read(btr.pin_in[1]);

		if (enc_a_val == 1) // rising edge
		{
			printf("rotary ");
			if (enc_b_val == 0) {
				printf("ticks--\n");
				checkrotary(1);
			}
			else {
				printf("ticks++\n");
				checkrotary(2);
			}
		}
	}
	return NULL;
}

void *t_lecture_selection(void *)
{
    fd_set fds;
    char buffer[2];

    unsigned int enc_a_val;

	int fd;
    char fName [128] ;
	sprintf (fName, "/sys/class/gpio/gpio%d/value", btr.pin_in[2]);
	
    if ((fd = open(fName, O_RDONLY)) < 0)
    {
        perror("pin");
        return 0;
    }

	while(1)
	{
		FD_ZERO(&fds);
		FD_SET(fd,&fds);

		if(select(fd+1,NULL,NULL,&fds,NULL) <0)
		{
			perror("select");
			break;
		}

		lseek(fd,0,0);
		if(read(fd,&buffer,2)!=2)
		{
			perror("read");
			break;
		}

		enc_a_val = atoi((const char*) buffer);


		if (enc_a_val == 1) // rising edge
		{
			printf("select");
			checkrotary(0);
		}
	}
	return NULL;
}



void set_pin_value(int pin,bool status) {
    FILE *fd ;
    char fName [128] ;
    sprintf (fName, "/sys/class/gpio/gpio%d/value", pin) ;

    if ((fd = fopen (fName, "w")) == NULL)
    {
        fprintf (stderr, "Unable to open GPIO direction interface for pin %d\n", pin) ;
        exit (1) ;
    }
    fprintf (fd, "%d\n",status) ;
    fclose (fd) ;

}

void pin_read_all(){
	printf("pin %d: %d,",btr.pin_in[0],pin_read(btr.pin_in[0]));
	printf("pin %d: %d,",btr.pin_in[1],pin_read(btr.pin_in[1]));
	printf("pin %d: %d\n",btr.pin_in[2],pin_read(btr.pin_in[2]));
}

void set_export_pin(int pin)
{
    FILE *fd ;
    if ((fd = fopen ("/sys/class/gpio/export", "w")) == NULL)
    {
        fprintf (stderr, "Unable to open GPIO export interface: %d\n", pin) ;
        exit (1) ;
    }

    fprintf (fd, "%d\n", pin) ;
    fclose (fd) ;
}

void set_pin_as_input(int pin) {
    FILE *fd ;
    char fName [128] ;
    sprintf (fName, "/sys/class/gpio/gpio%d/direction", pin) ;

    if ((fd = fopen (fName, "w")) == NULL)
    {
        fprintf (stderr, "Unable to open GPIO direction interface for pin %d\n", pin) ;
        exit (1) ;
    }
    fprintf (fd, "in\n") ;
    fclose (fd) ;

}

void set_pin_as_output(int pin) {
    FILE *fd ;
    char fName [128] ;
    sprintf (fName, "/sys/class/gpio/gpio%d/direction", pin) ;

    if ((fd = fopen (fName, "w")) == NULL)
    {
        fprintf (stderr, "Unable to open GPIO direction interface for pin %d\n", pin) ;
        exit (1) ;
    }
    fprintf (fd, "out\n") ;
    fclose (fd) ;

}

void set_pin_active_low(int pin) {
    FILE *fd ;
    char fName [128] ;
    sprintf (fName, "/sys/class/gpio/gpio%d/active_low", pin) ;

    if ((fd = fopen (fName, "w")) == NULL)
    {
        fprintf (stderr, "Unable to open GPIO active interface for pin %d\n", pin) ;
        exit (1) ;
    }
    fprintf (fd, "1\n") ;
    fclose (fd) ;

}

void set_pin_edge_trig(int pin) {
    FILE *fd ;
    char fName [128] ;
    sprintf (fName, "/sys/class/gpio/gpio%d/edge", pin) ;

    if ((fd = fopen (fName, "w")) == NULL)
    {
        fprintf (stderr, "Unable to open GPIO edge interface for pin %d\n", pin) ;
        exit (1) ;
    }
    fprintf (fd, "rising\n") ;
    fclose (fd) ;

}

void setup_IO_pin() {
	


    set_export_pin(btr.pin_in[0]);
    set_pin_as_input(btr.pin_in[0]);
    set_pin_active_low(btr.pin_in[0]);
    set_pin_edge_trig(btr.pin_in[0]);
    pthread_create(&btr.t[0],NULL,t_lecture_rotation,NULL);

    set_export_pin(btr.pin_in[1]);
    set_pin_as_input(btr.pin_in[1]);
    set_pin_active_low(btr.pin_in[1]);

    set_export_pin(btr.pin_in[2]);
    set_pin_as_input(btr.pin_in[2]);
    set_pin_active_low(btr.pin_in[2]);
    set_pin_edge_trig(btr.pin_in[2]);
    pthread_create(&btr.t[2],NULL,t_lecture_selection,NULL);
	
    set_export_pin(btr.pin_act);
    set_pin_as_output(btr.pin_act);
    set_pin_value(btr.pin_act,1);
	
	usleep(10000);
}

//###########################main callback##########################
static void async_read_callback(uint8_t *n_buf, uint32_t len, void *ctx){
	unsigned int time_spent = VC.refreshtime;
	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);
	
	for (int i=0; i<(fftw.inlen*2); i+=2){
		fftw.in[i/2][0] = (n_buf[i]-127.34)*fftw.window[i/2];
        fftw.in[i/2][1] = (n_buf[i+1]-127.34)*fftw.window[i/2];
	}
	
	fftw_execute(fftw.plan);
	
	char * values = (char * ) malloc(sizeof(char) * (vinfo.xres));
	
	int k = 0;//fft position
	float kk = (float)VC.multip/(float)VC.scale_freq;//position scale adjustement
	float u = (float)fftw.outlen-((float)vinfo.xres*(float)kk); //précalculation of the fft buffer shift
	int p = 0;//p: position in line scaled and frequency centered
	for (unsigned int i=0; i < vinfo.xres; i++){
		if(VC.rev_frequency){
			if (i < midle_screen_x) {
				p = midle_screen_x - i;
				k=(float)i*(float)kk;
				
			} else {
				p = vinfo.xres - i + midle_screen_x;
				k=(float)u+((float)i*(float)kk);
			}
		}
		else{
			if (i < midle_screen_x) {
				p = midle_screen_x + i;
				k=i*kk;
			} else {
				p = i - midle_screen_x;
				k=u+(i*kk);
			}			
		}
		
		double val = (sqrt(fftw.out[k][0] * fftw.out[k][0] + fftw.out[k][1] * fftw.out[k][1]));
		
		val = val < 1.0 ? 1.0 : val;
		val = 10 * log10((val)) * VC.scale_db ; //convert to db
		*(values + p) = (char)(val + VC.wf_noise_floor_db);

		*(VC.spec_values + p) += (val + VC.spc_noise_floor_db)/VC.spect_smoothing_n; //spectral value smoothing

	}
	
	VC.spect_smoothing_counter++;
	if(VC.spect_smoothing_counter==VC.spect_smoothing_n){setoneSPECline(VC.spec_values);VC.spect_smoothing_counter=0;memset(VC.spec_values, 0, vinfo.xres * sizeof(char));}
	
	setoneFFTline(values);
	put_framebuffer_fbp();
	
	// pin_read_all();
	updatetextshow();
	

	
	clock_gettime(CLOCK_REALTIME, &end);
	time_spent -= ((end.tv_nsec - start.tv_nsec)/1000);
    if(time_spent > 0 && time_spent < VC.refreshtime){usleep(time_spent);} 
	
}

//###########################Setups##########################
void setup_display (void)
{
	scandirfilecolor(); //get avalable palette color in same direcotry
    FB_init();
	BK_init();
	
}

void setup_rtl (void)
{
	rtlsdr_init();
}

void setup_fft (void)
{	
	//set fft out lenght to width screen size
	VC.multip=8;
	while((VC.scale_freq>VC.multip) && ((vinfo.xres*VC.multip)%256==0)){VC.multip+=VC.multip;} //get best performance value for fft len , must be multiple of 512 *16 > vinfo.xres*scale_freq but we use fftw.inlen = fftw.outlen and fftw.inlen * 2 because I/Q
	printf("FFT buffer multiplier=%d\n FFT buffer size is %d\n",VC.multip, vinfo.xres*VC.multip);
	fftw.outlen = vinfo.xres*VC.multip;
	fftw.inlen = fftw.outlen;
	VC.spec_values = (char * ) calloc(vinfo.xres, sizeof(char)); //set buffer for spectrum smoothing

	fft_init();
	fftw.window = windowinginit(fftw.inlen); //calculing one time windowings values
	midle_screen_x = vinfo.xres / 2; //calculing one time for this usefull value
	midle_screen_y = vinfo.yres / 2; //calculing one time for this usefull value
	
	char word[] = "recall";
    print_char_time(word, 2);
	
}

void setup_pin (void)
{
	setup_IO_pin();
}

//################################main#################################
void help() {
    printf("F4HTB_RTL_Panadapter -r 1024000\n or DISAPLAY=:0 sudo ./F4HTB_RTL_Panadapter -r 1024000\n");
    exit(0);
}

int main(int argc, char * argv[]) {
    int c;
	
	static struct option long_options[] =
        {
			{"help",  required_argument, 0, 'h'},
			{"setcolor",     no_argument,       0, 'c'},
			{"samplerate",  no_argument,       0, 's'},
			{"gain",  required_argument, 0, 'g'},
			{"fi_frequency",    required_argument, 0, 'f'},
			{"scale_db",    required_argument, 0, 'x'},
			{"noise_floor_db",    required_argument, 0, 'n'},
			{"refresh_frequency",    required_argument, 0, 'z'},
			{"revers_freq",    no_argument, 0, 'r'},
			{0, 0, 0, 0}
        };
	int option_index = 0;

    while ((c = getopt_long(argc, argv, "hc:s:g:f:y:n:z:rx:", long_options, &option_index)) != -1){
        switch (c) {
        case 'h':
            help();
            exit(0);
            break;
        case 'c':
            read_csv(optarg);
            break;
            return 1;
        case 's':
            rtl._samp_rate = atoi(optarg);
            break;
            return 1;
        case 'g':
            rtl._gain = atoi(optarg);
            break;
            return 1;
        case 'f':
            rtl._center_freq = atoi(optarg);
            break;
            return 1;
        case 'y':
            VC.scale_db = atoi(optarg);
            break;
            return 1;
        case 'n':
            VC.wf_noise_floor_db = atoi(optarg);
			VC.spc_noise_floor_db = atoi(optarg);
            break;
            return 1;
        case 'z':
            VC.refreshtime = 1000000/atoi(optarg);
            break;
            return 1;
        case 'r':
            VC.rev_frequency=1;
            break;
            return 1;
        case 'x':
            VC.scale_freq=atoi(optarg);
            break;
            return 1;
        default:
            help();
            abort();
        }
	}
		
	setup_display();
	setup_rtl();
	setup_fft();
	setup_pin();
	
	while(1){
		rtlsdr_read_async(rtl_dev, async_read_callback, NULL, 1, fftw.inlen*2);
		fft_deinit();
		setup_fft();
	}
	fft_deinit();
}