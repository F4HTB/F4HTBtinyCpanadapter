#include "F4HTB_RTL_Panadapter.h" 
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

#include <fftw3.h>
#include <rtl-sdr.h>
#include <complex.h>
#include <string.h>

//###########################global variables##########################
//##################################RTL variables
static rtlsdr_dev_t *rtl_dev;
struct rtlinfo{
int 	_samp_rate = 1024000,
			_dev_id=0, 
			_center_freq=143300000, 
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
	int _refresh_rate = 500;
}
fftw;

//##################################Video variables
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
char * framebuffer;
char * fbp = 0;
long int screensize = 0;
int fbfd = 0;

int ppx = 0;
int ppy = 0;

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
		int gains[gain_count], supported_gains = rtlsdr_get_tuner_gains(rtl_dev, gains);
		for (int i = 0; i < supported_gains; i++){
			if (gains[i] > 10 && gains[i] < 30)
				rtl._gain = gains[i];
			printf("%.1f ", gains[i] / 10.0);
		}
		printf("\n");
		printf("Gain set to %.1f\n", rtl._gain / 10.0);
		rtlsdr_set_tuner_gain(rtl_dev, rtl._gain);
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


//###########################Frame Buffer##########################
void fft_init(void) {
    
    fftw.in = (fftw_complex * ) fftw_malloc(sizeof(fftw_complex) * fftw.inlen);
    fftw.out = (fftw_complex * ) fftw_malloc(sizeof(fftw_complex) * fftw.outlen);

    fftw.plan = fftw_plan_dft_1d(fftw.outlen, fftw.in, fftw.out, FFTW_FORWARD, FFTW_MEASURE);

    fftw.currentLine = (double * ) malloc(sizeof(double) * fftw.outlen);
    memset(fftw.currentLine, 0, sizeof(double) * fftw.outlen);

    /* How many hertz does one "bin" comprise? */
    fftw.binWidth = (double) rtl._samp_rate / (double) fftw.outlen;
    printf("FFT Resolution = %f\nFFT out len %d\n", fftw.binWidth, fftw.outlen);
}

/* Free any FFTW resources. */
void fftwDeinit(void) {
    fftw_destroy_plan(fftw.plan);
    fftw_free(fftw.in);
    fftw_free(fftw.out);
    free(fftw.currentLine);
    fftw_cleanup();
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

    // Figure out the size of the screen in bytes
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	
	//set fft out lenght to width screen size
	fftw.outlen = vinfo.xres;
	
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

int scale = 1;
int pixelperdb = 3;
int dbbottom = 90;

void put_pixel_32bpp(int x, int y, int r, int g, int b, int t) {
    long int location = 0;
    location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) + (y + vinfo.yoffset) * finfo.line_length;
    *(framebuffer + location) = b; // Some blue
    *(framebuffer + location + 1) = g; // A little green
    *(framebuffer + location + 2) = r; // A lot of red
    *(framebuffer + location + 3) = t; // No transparency

}

void put_framebuffer_fbp() {
    memcpy(fbp, framebuffer, screensize);
}

void BK_init() {

    for (unsigned int x = 0; x < vinfo.xres; x++) put_pixel_32bpp(x, (ppy), 255, 255, 255, 0);
    put_framebuffer_fbp();
}



void setoneSPECline(char * values) {
        memset(framebuffer, 0, (screensize / 2 - finfo.line_length) * sizeof( * framebuffer));
        for (unsigned int i = 0; i < vinfo.xres; i++) {
            int y = (254 - * (values + i)) * (ppy -1) / 255;
            for ( int z = y; z < (ppy-1); z++) {
                put_pixel_32bpp(i, z, 255, 255, 255, 0);
            }
        }
}

static void async_read_callback(uint8_t *n_buf, uint32_t len, void *ctx){
	for (int i=0; i<(fftw.inlen*2); i+=2){
		fftw.in[i/2][0] = (n_buf[i]-127.34);
        fftw.in[i/2][1] = (n_buf[i+1]-127.34);
	}
	
	fftw_execute(fftw.plan);
	
	char * values = (char * ) malloc(sizeof(char) * (fftw.outlen));
	int p = 0;
	for (int i=0; i < fftw.outlen; i++){
		if (i < ((int)ppx)) {
			p = ppx - i;
		} else {
			p = fftw.outlen - i + ppx;
		}
				
		double val = (sqrt(fftw.out[i][0] * fftw.out[i][0] + fftw.out[i][1] * fftw.out[i][1]));
		val = 10 * log10((val)); //convert to db
		*(values + p) = (char)(val * pixelperdb + (dbbottom * pixelperdb + scale));
	}
	
	setoneSPECline(values);
	put_framebuffer_fbp();
}




//###########################Starting Setup##########################
void setup (void)
{

    FB_init();
	BK_init();
	rtlsdr_init();
	fft_init();
	ppx = vinfo.xres / 2;
    ppy = vinfo.yres / 2;

}

//################################main#################################
void help() {
    printf("F4HTB_RTL_Panadapter -r 1024000\n or DISAPLAY=:0 sudo ./F4HTB_RTL_Panadapter -r 1024000\n");
    exit(0);
}

int main(int argc, char * argv[]) {
    int c;

    while ((c = getopt(argc, argv, "mhd:r:c:")) != -1)
        switch (c) {
        case 'r':
            rtl._samp_rate = atoi(optarg);
            break;
            return 1;
        case 'h':
            help();
            exit(0);
            break;
        default:
            help();
            abort();
        }
		
	setup();
	
	rtlsdr_read_async(rtl_dev, async_read_callback, NULL, 0, fftw.inlen*2);
	
	fftwDeinit();
}