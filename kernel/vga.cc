#include "vga.h"
#include "machine.h"
#include "threads.h"
#include "debug.h"

#define COLOR_BLACK 0x0
#define COLOR_GREEN 0x2
#define COLOR_PURPLE 0xf

#define B 0x0
#define W 0x7

unsigned char g_320x200x256[] =
{
/* MISC */
	0x63,
/* SEQ */
	0x03, 0x01, 0x0F, 0x00, 0x0E,
/* CRTC */
	0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
	0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x9C, 0x0E, 0x8F, 0x28,	0x40, 0x96, 0xB9, 0xA3,
	0xFF,
/* GC */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
	0xFF,
/* AC */
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x41, 0x00, 0x0F, 0x00,	0x00
};

void draw_rectangle(int x, int y, int width, int height, unsigned short color) {
	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			vga_plot_pixel(x+i, y+j, color);
		}
	}
}

void draw_happy_face(int x, int y) {
	// eye
	vga_plot_pixel(x,y,COLOR_PURPLE);
	// eye
	vga_plot_pixel(x+10,y,COLOR_PURPLE);
	// mouth
	vga_plot_pixel(x,	y+8,COLOR_PURPLE);
	vga_plot_pixel(x+1,	y+9,COLOR_PURPLE);
	vga_plot_pixel(x+2,	y+10,COLOR_PURPLE);
	vga_plot_pixel(x+3,	y+10,COLOR_PURPLE);
	vga_plot_pixel(x+4,	y+10,COLOR_PURPLE);
	vga_plot_pixel(x+5,	y+10,COLOR_PURPLE);
	vga_plot_pixel(x+6,	y+10,COLOR_PURPLE);
	vga_plot_pixel(x+7,	y+10,COLOR_PURPLE);
	vga_plot_pixel(x+8,	y+10,COLOR_PURPLE);
	vga_plot_pixel(x+9,	y+9,COLOR_PURPLE);
	vga_plot_pixel(x+10,y+8,COLOR_PURPLE);

    Debug::printf("*** just drew a smiley face\n");
}

void vga_clear_screen() {
    for (int i = 0; i < 320; i++) {
        for (int j = 0; j < 200; j++) {
            vga_plot_pixel(i,j,COLOR_BLACK);
        }
    }
}

void vga_plot_pixel(int x, int y, unsigned short color) {
    // Debug::printf("%d", sizeof(unsigned short));
    unsigned short offset = x + 320 * y;
    unsigned char *VGA = (unsigned char*) VGA_ADDRESS;
    VGA[offset] = color;
}

void draw_image(unsigned short* image, int x, int y, int width, int height, int scale) {
    for (int cur_height = 0; cur_height < height; cur_height++) {
        for (int cur_width = 0; cur_width < width; cur_width++) {
            draw_rectangle(x+cur_width*scale, y+cur_height*scale, scale, scale, image[width*cur_height+cur_width]);
        }
    }

    Debug::printf("*** drew the image\n");
}

void draw_animation(unsigned short* image, int x, int y, int width, int height, int scale, int num_frames) {
	for (int cur_frame = 0; cur_frame < num_frames; cur_frame++) {
		vga_clear_screen();
		draw_image(image + (cur_frame*width*height), x, y, width, height, scale);

		volatile int i = 0;
		for (int j = 0; j < 200000000; j++) {i+=1;} // pause between each frame
	}
}

// Begin copied code
// Source: https://files.osdev.org/mirrors/geezer/osd/graphics/modes.c
// Changes:
// - Initial: only grabbed code I thought was relevant to changing mode and displaying our first pixel (`write_regs`)
// - Changed inb, outb funcs to instead point to inb, outb funcs defined in kernel.asm
void write_regs(unsigned char *regs)
{
	unsigned i;

/* write MISCELLANEOUS reg */
	outb(VGA_MISC_WRITE, *regs);
	regs++;
/* write SEQUENCER regs */
	for(i = 0; i < VGA_NUM_SEQ_REGS; i++)
	{
		outb(VGA_SEQ_INDEX, i);
		outb(VGA_SEQ_DATA, *regs);
		regs++;
	}
/* unlock CRTC registers */
	outb(VGA_CRTC_INDEX, 0x03);
	outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
	outb(VGA_CRTC_INDEX, 0x11);
	outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);
/* make sure they remain unlocked */
	regs[0x03] |= 0x80;
	regs[0x11] &= ~0x80;
/* write CRTC regs */
	for(i = 0; i < VGA_NUM_CRTC_REGS; i++)
	{
		outb(VGA_CRTC_INDEX, i);
		outb(VGA_CRTC_DATA, *regs);
		regs++;
	}
/* write GRAPHICS CONTROLLER regs */
	for(i = 0; i < VGA_NUM_GC_REGS; i++)
	{
		outb(VGA_GC_INDEX, i);
		outb(VGA_GC_DATA, *regs);
		regs++;
	}
/* write ATTRIBUTE CONTROLLER regs */
	for(i = 0; i < VGA_NUM_AC_REGS; i++)
	{
		(void)inb(VGA_INSTAT_READ);
		outb(VGA_AC_INDEX, i);
		outb(VGA_AC_WRITE, *regs);
		regs++;
	}
/* lock 16-color palette and unblank display */
	(void)inb(VGA_INSTAT_READ);
	outb(VGA_AC_INDEX, 0x20);
}
// End copied code

void vga_test() {
    write_regs(g_320x200x256);
	vga_clear_screen();

	// 320x200, scale 20 16x10
	// 10fps


	// TODO

	// get first 1 minute of bad apple
	// convert bad apple into 10fps frames
	// convert frames into ascii txt
	// convert ascii txt into B or W array

	// clean up test case dir

    unsigned short frame_1[1200] = {
        B,B,B,B,B,B,B,B,
		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,B,

		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,B,
		B,B,B,B,B,B,B,B,
		B,B,B,B,B,B,B,B,
		B,B,B,B,B,B,B,B,

		B,B,B,B,B,W,W,W,
		B,B,B,B,B,W,W,W,
		B,B,B,B,B,W,W,W,
		B,B,B,B,B,B,W,W,
		B,B,B,B,B,B,W,W,

		B,B,B,B,B,W,W,W,
		B,B,B,B,B,W,W,W,
		B,B,B,B,B,B,W,W,
		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,W,

		B,B,B,B,B,B,W,W,
		B,B,B,B,B,B,W,W,
		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,B,

		W,B,B,B,B,B,W,W,
		B,B,B,B,B,B,W,W,
		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,B,

		W,B,B,B,B,B,W,W,
		W,B,B,B,B,B,W,W,
		W,B,B,B,B,B,B,W,
		W,B,B,B,B,B,B,W,
		B,B,B,B,B,B,B,B,

		W,W,B,B,B,B,B,W,
		W,W,B,B,B,B,W,W,
		W,W,B,B,B,B,B,W,
		W,B,B,B,B,B,B,W,
		W,B,B,B,B,B,B,B,

		W,W,B,B,B,B,B,W,
		W,W,W,B,B,B,B,W,
		W,W,B,B,B,B,B,W,
		W,W,B,B,B,B,B,B,
		W,B,B,B,B,B,B,B,

		W,W,W,B,B,B,B,W,
		W,W,W,B,B,B,B,W,
		W,W,W,B,B,B,B,W,
		W,W,B,B,B,B,B,B,
		W,W,B,B,B,B,B,B,

		W,W,W,B,B,B,B,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,B,W,
		W,W,W,B,B,B,B,B,
		W,W,B,B,B,B,B,B,

		W,W,W,B,B,B,B,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,B,W,
		W,W,W,B,B,B,B,W,
		W,W,B,B,B,B,B,W,

		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,
		W,W,B,B,B,B,B,W,
		W,W,B,B,B,B,B,W,

		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,
		W,W,B,B,B,B,W,W,
		W,W,B,B,B,B,B,W,

		W,W,W,B,B,B,W,W,
		W,W,W,B,B,W,W,W,
		W,W,B,B,B,B,W,W,
		W,W,B,B,B,B,W,W,
		W,W,B,B,B,B,W,W,

		W,W,W,B,B,B,W,W,
		W,W,W,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,B,W,W,
		W,B,B,B,B,B,W,W,

		W,W,B,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,B,W,W,
		W,B,B,B,B,B,W,W,

		W,W,B,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,B,B,B,B,B,W,W,

		W,W,B,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,B,B,B,B,W,W,W,

		W,W,B,B,B,W,W,W,
		W,W,B,B,W,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,B,B,B,B,W,W,W,

		W,W,B,B,B,W,W,W,
		W,W,B,B,W,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,W,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,B,W,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,B,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,B,B,B,W,W,W,
		W,W,B,B,B,B,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,B,B,B,B,W,W,
		W,W,B,B,B,B,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,W,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,W,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,

		W,W,W,B,B,W,W,W,
		W,W,W,W,B,W,W,W,
		W,W,W,B,B,W,W,W,
		W,W,W,B,B,B,W,W,
		W,W,W,B,B,B,W,W,
    };

    draw_animation(frame_1, 0, 0, 8, 5, 40, 30);
	

    while (true) {
    }
}