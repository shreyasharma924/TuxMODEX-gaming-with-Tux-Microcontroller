/*									tab:8
 *
 * photo.c - photo display functions
 *
 * "Copyright (c) 2011 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO 
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL 
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, 
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE 
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE, 
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:	    Steve Lumetta
 * Version:	    3
 * Creation Date:   Fri Sep  9 21:44:10 2011
 * Filename:	    photo.c
 * History:
 *	SL	1	Fri Sep  9 21:44:10 2011
 *		First written (based on mazegame code).
 *	SL	2	Sun Sep 11 14:57:59 2011
 *		Completed initial implementation of functions.
 *	SL	3	Wed Sep 14 21:49:44 2011
 *		Cleaned up code for distribution.
 */


#include <string.h>

#include "assert.h"
#include "modex.h"
#include "photo.h"
#include "photo_headers.h"
#include "world.h"
// static void coppyArray(pixels source[] , pixels destination, int size); 

 typedef struct pixels{ // using a struct here for every pixel containing values for RGB, count, index 
	unsigned long red; 
	unsigned long green; 
	unsigned long blue; 
	unsigned long count; 
	unsigned long index;
}pixels;

static pixels level_2[64]; // declaring an array for level 2, continaing 64 spaces (2^6)
static pixels level_4[4096];// declaring an array for level 4, continaing 4096 spaces (2^12)
int i;
int j;
int level2_size = 64;
int level4_size = 4096; 



/* types local to this file (declared in types.h) */

/* 
 * A room photo.  Note that you must write the code that selects the
 * optimized palette colors and fills in the pixel data using them as 
 * well as the code that sets up the VGA to make use of these colors.
 * Pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of
 * the second row, and so forth.  No padding should be used.
 */
struct photo_t {
    photo_header_t hdr;			/* defines height and width */
    uint8_t        palette[192][3];     /* optimized palette colors */
    uint8_t*       img;                 /* pixel data               */
};

/* 
 * An object image.  The code for managing these images has been given
 * to you.  The data are simply loaded from a file, where they have 
 * been stored as 2:2:2-bit RGB values (one byte each), including 
 * transparent pixels (value OBJ_CLR_TRANSP).  As with the room photos, 
 * pixel data are stored as one-byte values starting from the upper 
 * left and traversing the top row before returning to the left of the 
 * second row, and so forth.  No padding is used.
 */
struct image_t {
    photo_header_t hdr;			/* defines height and width */
    uint8_t*       img;                 /* pixel data               */
};



/* file-scope variables */

/* 
 * The room currently shown on the screen.  This value is not known to 
 * the mode X code, but is needed when filling buffers in callbacks from 
 * that code (fill_horiz_buffer/fill_vert_buffer).  The value is set 
 * by calling prep_room.
 */
static const room_t* cur_room = NULL; 


/* 
 * fill_horiz_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the leftmost 
 *                pixel of a line to be drawn on the screen, this routine 
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS: (x,y) -- leftmost pixel of line to be drawn 
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void
fill_horiz_buffer (int x, int y, unsigned char buf[SCROLL_X_DIM])
{
    int            idx;   /* loop index over pixels in the line          */ 
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgx;  /* loop index over pixels in object image      */ 
    int            yoff;  /* y offset into object image                  */ 
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo (cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_X_DIM; idx++) {
        buf[idx] = (0 <= x + idx && view->hdr.width > x + idx ?
		    view->img[view->hdr.width * y + x + idx] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate (cur_room); NULL != obj;
    	 obj = obj_next (obj)) {
	obj_x = obj_get_x (obj);
	obj_y = obj_get_y (obj);
	img = obj_image (obj);

        /* Is object outside of the line we're drawing? */
	if (y < obj_y || y >= obj_y + img->hdr.height ||
	    x + SCROLL_X_DIM <= obj_x || x >= obj_x + img->hdr.width) {
	    continue;
	}

	/* The y offset of drawing is fixed. */
	yoff = (y - obj_y) * img->hdr.width;

	/* 
	 * The x offsets depend on whether the object starts to the left
	 * or to the right of the starting point for the line being drawn.
	 */
	if (x <= obj_x) {
	    idx = obj_x - x;
	    imgx = 0;
	} else {
	    idx = 0;
	    imgx = x - obj_x;
	}

	/* Copy the object's pixel data. */
	for (; SCROLL_X_DIM > idx && img->hdr.width > imgx; idx++, imgx++) {
	    pixel = img->img[yoff + imgx];

	    /* Don't copy transparent pixels. */
	    if (OBJ_CLR_TRANSP != pixel) {
		buf[idx] = pixel;
	    }
	}
    }
}


/* 
 * fill_vert_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the top pixel of 
 *                a vertical line to be drawn on the screen, this routine 
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS: (x,y) -- top pixel of line to be drawn 
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void
fill_vert_buffer (int x, int y, unsigned char buf[SCROLL_Y_DIM])
{
    int            idx;   /* loop index over pixels in the line          */ 
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgy;  /* loop index over pixels in object image      */ 
    int            xoff;  /* x offset into object image                  */ 
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo (cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_Y_DIM; idx++) {
        buf[idx] = (0 <= y + idx && view->hdr.height > y + idx ?
		    view->img[view->hdr.width * (y + idx) + x] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate (cur_room); NULL != obj;
    	 obj = obj_next (obj)) {
	obj_x = obj_get_x (obj);
	obj_y = obj_get_y (obj);
	img = obj_image (obj);

        /* Is object outside of the line we're drawing? */
	if (x < obj_x || x >= obj_x + img->hdr.width ||
	    y + SCROLL_Y_DIM <= obj_y || y >= obj_y + img->hdr.height) {
	    continue;
	}

	/* The x offset of drawing is fixed. */
	xoff = x - obj_x;

	/* 
	 * The y offsets depend on whether the object starts below or 
	 * above the starting point for the line being drawn.
	 */
	if (y <= obj_y) {
	    idx = obj_y - y;
	    imgy = 0;
	} else {
	    idx = 0;
	    imgy = y - obj_y;
	}

	/* Copy the object's pixel data. */
	for (; SCROLL_Y_DIM > idx && img->hdr.height > imgy; idx++, imgy++) {
	    pixel = img->img[xoff + img->hdr.width * imgy];

	    /* Don't copy transparent pixels. */
	    if (OBJ_CLR_TRANSP != pixel) {
		buf[idx] = pixel;
	    }
	}
    }
}


/* 
 * image_height
 *   DESCRIPTION: Get height of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
image_height (const image_t* im)
{
    return im->hdr.height;
}


/* 
 * image_width
 *   DESCRIPTION: Get width of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
image_width (const image_t* im)
{
    return im->hdr.width;
}

/* 
 * photo_height
 *   DESCRIPTION: Get height of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
photo_height (const photo_t* p)
{
    return p->hdr.height;
}


/* 
 * photo_width
 *   DESCRIPTION: Get width of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
photo_width (const photo_t* p)
{
    return p->hdr.width;
}


/* 
 * prep_room
 *   DESCRIPTION: Prepare a new room for display.  You might want to set
 *                up the VGA palette registers according to the color
 *                palette that you chose for this room.
 *   INPUTS: r -- pointer to the new room
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes recorded cur_room for this file
 */
void
prep_room (const room_t* r)
{
    /* Record the current room. */
	set_palette(room_photo(r)->palette);
    cur_room = r;
}


/* 
 * read_obj_image
 *   DESCRIPTION: Read size and pixel data in 2:2:2 RGB format from a
 *                photo file and create an image structure from it.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the image
 */
image_t*
read_obj_image (const char* fname)
{
	// coppyArray(&level_4 , &ltemp, 4096); 
    FILE*    in;		/* input file               */
    image_t* img = NULL;	/* image structure          */
    uint16_t x;			/* index over image columns */
    uint16_t y;			/* index over image rows    */
    uint8_t  pixel;		/* one pixel from the file  */

    /* 
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the image pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen (fname, "r+b")) ||
	NULL == (img = malloc (sizeof (*img))) ||
	NULL != (img->img = NULL) || /* false clause for initialization */
	1 != fread (&img->hdr, sizeof (img->hdr), 1, in) ||
	MAX_OBJECT_WIDTH < img->hdr.width ||
	MAX_OBJECT_HEIGHT < img->hdr.height ||
	NULL == (img->img = malloc 
		 (img->hdr.width * img->hdr.height * sizeof (img->img[0])))) {
	if (NULL != img) {
	    if (NULL != img->img) {
	        free (img->img);
	    }
	    free (img);
	}
	if (NULL != in) {
	    (void)fclose (in);
	}
	return NULL;
    }



    /* 
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order (top to bottom).
     */
    for (y = img->hdr.height; y-- > 0; ) {

	/* Loop over columns from left to right. */
	for (x = 0; img->hdr.width > x; x++) {

	    /* 
	     * Try to read one 8-bit pixel.  On failure, clean up and 
	     * return NULL.
	     */
	    if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
		free (img->img);
		free (img);
	        (void)fclose (in);
		return NULL;
	    }

	    /* Store the pixel in the image data. */
	    img->img[img->hdr.width * y + x] = pixel;
	}
    }

    /* All done.  Return success. */
    (void)fclose (in);
    return img;
}


/* 
 * read_photo
 *   DESCRIPTION: Read size and pixel data in 5:6:5 RGB format from a
 *                photo file and create a photo structure from it.
 *                Code provided simply maps to 2:2:2 RGB.  You must
 *                replace this code with palette color selection, and
 *                must map the image pixels into the palette colors that
 *                you have defined.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the photo
 */
photo_t*
read_photo (const char* fname)
{
    FILE*    in;	/* input file               */
    photo_t* p = NULL;	/* photo structure          */
    uint16_t x;		/* index over image columns */
    uint16_t y;		/* index over image rows    */
    uint16_t pixel;	/* one pixel from the file  */

	unsigned long redp; 
	unsigned long greenp; 
	unsigned long bluep; 		
	unsigned long pixel_4;	
	unsigned long pixel_4_idx; 
	unsigned long pixel_2_idx;

    /* 
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the photo pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen (fname, "r+b")) ||
	NULL == (p = malloc (sizeof (*p))) ||
	NULL != (p->img = NULL) || /* false clause for initialization */
	1 != fread (&p->hdr, sizeof (p->hdr), 1, in) ||
	MAX_PHOTO_WIDTH < p->hdr.width ||
	MAX_PHOTO_HEIGHT < p->hdr.height ||
	NULL == (p->img = malloc 
		 (p->hdr.width * p->hdr.height * sizeof (p->img[0])))) {
	if (NULL != p) {
	    if (NULL != p->img) {
	        free (p->img);
	    }
	    free (p);
	}
	if (NULL != in) {
	    (void)fclose (in);
	}
	return NULL;
    }

//my code starts here 

	for(i=0; i< level2_size; i++){ // loop till 64 to fill in all values of level 2 array with 0 (all elements: RGB, count, index)
		level_2[i].red = 0;
		level_2[i].green = 0;
		level_2[i].blue = 0;
		level_2[i].count = 0;
		level_2[i].index = 0;
	}
	for(i=0; i< level4_size; i++){ // loop till 4096 to fill in all values of level 4 array with 0 (all elements: RGB, count, index)
		level_4[i].red = 0;
		level_4[i].green = 0;
		level_4[i].blue = 0;
		level_4[i].count = 0;
		level_4[i].index = 0;
	}


    /* 
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order (top to bottom).
     */
    for (y = p->hdr.height; y-- > 0; ) {

	/* Loop over columns from left to right. */
	for (x = 0; p->hdr.width > x; x++) {

	    /* 
	     * Try to read one 16-bit pixel.  On failure, clean up and 
	     * return NULL.
	     */
	    if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
		free (p->img);
		free (p);
	        (void)fclose (in);
		return NULL;

	    }
	    /* 
	     * 16-bit pixel is coded as 5:6:5 RGB (5 bits red, 6 bits green,
	     * and 6 bits blue).  We change to 2:2:2, which we've set for the
	     * game objects.  You need to use the other 192 palette colors
	     * to specialize the appearance of each photo.
	     *
	     * In this code, you need to calculate the p->palette values,
	     * which encode 6-bit RGB as arrays of three uint8_t's.  When
	     * the game puts up a photo, you should then change the palette 
	     * to match the colors needed for that photo.
	     */
	    // p->img[p->hdr.width * y + x] = (((pixel >> 14) << 4) |
		// 			    (((pixel >> 9) & 0x3) << 2) |
		// 			    ((pixel >> 3) & 0x3));
		
		redp = (pixel >> 11) << 1 ; // saves all values of the red bits in pixel (16 bit value), shifts 1 left to make it 6 bit and saved in redp, and shifts right by 11 initially to get rid of other bits 
		greenp =  ((pixel >> 5) & 0x3F); // saves all values of the green bits in pixel (16 bit value), masks the other bits in the pixel (3F)
		bluep = (pixel & 0x1F)<< 1 ; // saves all values of the blue bits in pixel (16 bit value), shifts 1 left to make it 6 bit and saved in bluep,  masks the other bits in the pixel (1F) 

		

		pixel_4 = ((redp >> 2) << 8) | ((greenp >> 2) << 4) | (bluep >> 2) ;/* concatenates the first 4 bits of all the RGB pixels into pixel_4. 
		do this by first shifting right by 2 bits for every pixel to get rid of the last 2 bits extra bits and shifting left by 8,4,0 depending on 
		red, green , blue pixel to concatenate in RRRRGGGGBBBB*/


		level_4[pixel_4].red += redp; // sums up all the vales of redp, greenp, bluep in level array
		level_4[pixel_4].green += greenp;
		level_4[pixel_4].blue += bluep; 
		level_4[pixel_4].count += 1; // adds up the count of the number of pixels in that index
		level_4[pixel_4].index = pixel_4; // eaquates it to the index value 

	}
    }

	
	qsort(level_4, level4_size, sizeof(pixels), compare);// quicksort (on the basis of count) in the level 4 array 

	for(i = 0; i< 128; i++){ // iterates through the highest 128 values in the level 4 array 
		if(level_4[i].count != 0){
		p->palette[i][0] = level_4[i].red/level_4[i].count; //  saves the mean of all the colors in the color palette 
		p->palette[i][1] = level_4[i].green/level_4[i].count; 
		p->palette[i][2] = level_4[i].blue/level_4[i].count; 
		}
	}
	int t; 
	for(i =128; i< level4_size; i++){// iterates through the rest pf the colors in level 4 array (after 128 till 4096)
		t = ((level_4[i].index >> 10) << 4) | (((level_4[i].index >> 6)&0x3)<<2)| ((level_4[i].index >> 2)&0x3);// shifts bits accordingly to save a level4 index into a level 2 index
		/*do this by first shifting right by 10,6,2 bits depending on  red, green , blue pixel to get rid of the last 2 bits on the level 4 index and shifting left by 4,2,0 depending on 
		red, green , blue pixel to concatenate in RRGGBB. Simultaneously mask values for green and blue to get rid of previos bits in the index - 0x3 */
		level_2[t].red += level_4[i].red; // sums up the red, green , blue values in level 2 array from level 4 array 
		level_2[t].green += level_4[i].green;
		level_2[t].blue += level_4[i].blue; 
		level_2[t].count += level_4[i].count; // sums up the count in level 2 array from level 4 array
		level_2[t].index = t; // stores that index as value
	}

	for (j = 0; j< level2_size; j++){ // iterating uptill 64 values - vlength of level 2 array 
		if(level_2[j].count != 0){
		p->palette[j+ 128][0] = level_2[j].red/level_2[j].count;  // //  saves the mean of all the colors in the color palette - maps it to right position in palette hence +128
		p->palette[j+ 128][1] = level_2[j].green/level_2[j].count; 
		p->palette[j+ 128][2] = level_2[j].blue/level_2[j].count; 
		}
	}

	if(fseek(in, 4, SEEK_SET)==0){  // seek to the position 4 bytes from the beginning of the file
 		for (y = p->hdr.height; y-- > 0; ) {   // loop through the image rows 
			for (x = 0; p->hdr.width > x; x++) {  // loop through the pixels in the current row from left to right
				
				if (1 != fread (&pixel, sizeof (pixel), 1, in)) { // read a single pixel from the input file - if not successful, free memory and return NULL
					free (p->img);
					free (p);
	        		(void)fclose (in);
					return NULL;
				}

				redp = (pixel >> 11) << 1; // saves all values of the red bits in pixel (16 bit value), shifts 1 left to make it 6 bit and saved in redp, and shifts right by 11 initially to get rid of other bits 
				greenp =  ((pixel >> 5) & 0x3F); // saves all values of the green bits in pixel (16 bit value), masks the other bits in the pixel (3F)
				bluep = (pixel & 0x1F)<< 1;  // saves all values of the blue bits in pixel (16 bit value), shifts 1 left to make it 6 bit and saved in bluep,  masks the other bits in the pixel (1F) 
				pixel_2_idx = ((redp >> 4)<< 4) | ((greenp >> 4) << 2) | (bluep >> 4) ; // same calculation done before to get RRGGBB
				pixel_4_idx = ((redp >> 2) << 8) | ((greenp >> 2) << 4) | (bluep >> 2) ; // same calculation done before to get RRRRGGGGBBBB 
				int k;
				int chk;
				chk=0; 
				for(k=0; k<128; k++){
					
				if(level_4[k].index ==  pixel_4_idx){ // search for a matching pixel index in the level_4 array
					p->img[p->hdr.width * y + x] =  k+ 64; // adding 64, since these indexes are saved after 64 in the palette
					chk = 1; 	
				}
				
				}
				if(chk == 0){  // If no match was found, use the pixel_2_idx for indexing
					p->img[p->hdr.width * y + x] =  pixel_2_idx + 128+ 64; // adding 128+64, since these indexes are saved after 192 in the palette
					
				 }

		}
	}

	}
	
    /* All done.  Return success. */
    (void)fclose (in);
    return p;

}

int compare(const void *a, const void *b){
		// const int *A=a, *B=b; 
		// return (*A > *B) - (*A < *B); 
		const struct pixels* first = a;
		const struct pixels* sec = b;
		return (sec->count - first->count);
		
}

