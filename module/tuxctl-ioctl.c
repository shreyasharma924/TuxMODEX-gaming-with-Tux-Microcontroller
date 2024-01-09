/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

void tux_button_calc (unsigned long b_var , unsigned long c_var );
int tux_innit(struct tty_struct* tty);
int tux_set_led(struct tty_struct* tty, unsigned long arg);
unsigned char setting_val(unsigned long number);
void tux_button_calc (unsigned long b_var , unsigned long c_var );
int tux_set_button (struct tty_struct* tty, unsigned long arg);

unsigned long *buff;
unsigned long b_c ; 
int flag; 
unsigned char setting_val(unsigned long number);
unsigned long restore; 

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)

/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */

 /*
 * tuxctl_handle_packet
 *   DESCRIPTION:  Handle Tux Controller packets received by the specified TTY device. Depending on the packet type, it perform various actions
 *                 used by the switch cases
 *                
 *   INPUTS: pointer to the tty structure, pointer to received data packet associated with the tux
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: calls for set_led , tux intializing, and setting buttons
 */   
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;
    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];
 
    //printk("packet : %x %x %x\n", a, b, c); 
		
	switch(a){
		
		case MTCP_ACK:
			flag = 1; 
			break; 
		case MTCP_BIOC_EVENT: 
			tux_button_calc(b ,c); // calls the button helper function 
		case MTCP_RESET:
			tux_innit(tty);// initializes tux for reset 
			tux_set_led(tty , restore); // calls set led and passes the restored led vals for displaying 
			break;
		default:
			break;
	}


	
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/

/*
 * tuxctl_ioctl
 *   DESCRIPTION: Handle IOCTL commands for the Tux
 *   INPUTS:
 *     tty  - Pointer to the TTY structure
 *     file - Pointer to the file structure
 *     cmd  - The IOCTL command code
 *     arg  - Argument 
 *   OUTPUTS: None
 *   RETURN VALUE:
 *     - 0 on success or appropriate error code on failure.
 *   SIDE EFFECTS: Performs actions based on the IOCTL command.
 */


int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
	case TUX_INIT:
		return tux_innit(tty); // returns the tux_set_innit
	case TUX_BUTTONS:
		return  tux_set_button (tty, arg);  // returns the tux_set_button
	case TUX_SET_LED:
		return tux_set_led(tty, arg); // returns the tux_set_led 
	case TUX_LED_ACK:
		return flag; // returns the flag
	case TUX_LED_REQUEST:
		return -1; // returns -1
	case TUX_READ_LED:
		return -1; // returns -1
	default:
	    return -EINVAL; // returns -EINVAL
    }
}

/*
 * tux_innit
 *   DESCRIPTION: Initialize the Tux Controller by setting its operating mode
 *                and turning on button interrupts.
 *   INPUTS:
 *     tty - Pointer to the TTY structure 
 *   OUTPUTS: None
 *   RETURN VALUE:
 *     - 0 on success.
 *   SIDE EFFECTS: Initializes the Tux Controller operating mode and button interrupts.
 */

int tux_innit(struct tty_struct* tty){
	unsigned char op[2]; //initalizing a buffer to sace the opcodes
	op[1] = MTCP_LED_USR; // storing the opcode MTCP_USR in op[0]
	op[0] = MTCP_BIOC_ON; // storing the opcode MTCP_USR in op[1]
	flag = 0;
	tuxctl_ldisc_put(tty, op, 2); 
	return 0; 
}


/*
 * tux_set_led
 *   DESCRIPTION: Set the LEDs on the Tux Controller according to the specified arguments
 *   INPUTS:
 *     tty - Pointer to the TTY structure
 *     arg - Argument containing LED configuration, number, and decimal point 
 *   OUTPUTS: Updates the Tux Controller LEDs
 *   RETURN VALUE:
 *     - 0 on success
 *     - -EINVAL if there is an error or if `flag` is not set
 *   SIDE EFFECTS: Sets the LEDs on the Tux Controller
 */

int tux_set_led(struct tty_struct* tty, unsigned long arg){
	

	
	unsigned long hex_mask = 0xF; // used for bitmasking last 4 bits 
	unsigned char buff[6];
	unsigned long num = arg & 0xFFFF; // bitmasking last 16 bits
	unsigned long led_on = (arg >> 16) & hex_mask; 
	unsigned long dp = (arg >> 24) & hex_mask; //decimal points for 7 seg display, from 24 to 27
	unsigned long current_led_val;
	unsigned int i;
	buff[0]= MTCP_LED_SET; // storing opcode MTCP_LED_SET in buff[0]
	buff[1]= 0xff; //used for turning on all the leds 
	i = 0;
	restore = arg;
	if(flag==0){
		return -EINVAL;
	}
	for (i=0 ; i< 4; i++){ // iterating 4 times for 4 led digits  
		if((led_on & 0x1) == 0x1){ // checks if the led bit is equal to 1 - look at one but in led at a time so mask it with 0x1  
				current_led_val = setting_val(num & hex_mask);  
				if((dp & 0x1) == 0x1){ // checks if the dp bit is equal to 1 - look at one but in dp at a time so mask it with 0x1  
					current_led_val = 0x10| current_led_val; 
			}
			buff[i+2]=  current_led_val;
		}
		else{
			//printk("off");
			buff[i+2]= 0;
		}
		led_on = led_on >>1;
		dp = dp >> 1; // shifts dp right by one to check for the next led light 
		num = num >> 4; // shifts num right by four to check for the next digit  
	}
	flag = 0; 
	if(tuxctl_ldisc_put(tty, buff , 6)){
		return -EINVAL;
	}
	return 0;
}


/*
 * setting_val
 *   DESCRIPTION: Get the 7-segment display hexadecimal value for a given number (0-9).
 *   INPUTS:
 *     number - The input number (0-9) for which to retrieve the 7-segment display value.
 *   OUTPUTS: None
 *   RETURN VALUE:
 *     - The 7-segment display hexadecimal value corresponding to the input number.
 *     - 0xE7 as the default value for invalid input.
 *   SIDE EFFECTS: None
 */

unsigned char setting_val(unsigned long number){
	switch(number){
		case 0x0:
			return 0xE7; // if 0, hex value to print 0 in the 7 segment display
		
		case 0x1:
			return 0x6; // if 1, hex value to print 1 in the 7 segment display

		case 0x2:
			return 0xCB; // if 2, value to print 2 in the 7 segment display

		case 0x3:
			return 0x8F; //if 3,  value to print 3 in the 7 segment display

		case 0x4:
			return 0x2E; //if 4,  value to print 3 in the 4 segment display

		case 0x5:
			return 0xAD; //if 5,  value to print 3 in the 5 segment display

		case 0x6:
			return 0xED; //if 6,  value to print 3 in the 6 segment display

		case 0x7:
			return 0x86; //if 7,  value to print 3 in the 7 segment display
		
		case 0x8:
			return 0xEF; //if 8,  value to print 3 in the 8 segment display

		case 0x9:
			return 0xAF; //if 9,  value to print 3 in the 9 segment display

		default:
			return 0xe7; // defaul case			
	}
}


/*
 * tux_button_calc
 *   DESCRIPTION: Calculate and update the Tux Controller button state based on input values.
 *   INPUTS:
 *     b_var - Value representing button press status.
 *     c_var - Value representing additional button press status.
 *   OUTPUTS: Updates the Tux Controller button state (`b_c` global variable).
 *   RETURN VALUE: None
 *   SIDE EFFECTS: Modifies the `b_c` global variable with the calculated button state.
 */

void tux_button_calc (unsigned long b_var , unsigned long c_var ){
	unsigned long temp1; 
	unsigned long temp2;
	b_c = 0;
	b_c = c_var & 0x9; // masks with 0x9 to store the values of up and down in the button (vals that arent changing)- 1001

	temp1 = c_var & 0x4; // temporary variable to store the down var - 0100
	temp2 = c_var & 0x2; // temporary variable to store the left var - 0010
	temp1 = temp1 >> 1; // right shifting down by 1 to store in correct place 
	temp2 = temp2 << 1; // left shifting left by 1 to store in correct place 
	temp1 = temp1 | temp2;
	b_c = b_c | temp1; 
	b_c = b_c << 4; // left shifts b_c val to get it to the last 4 bits 
	b_c = b_c | (b_var & 0xf); // or-ing so get the c,b,a,start in first 4 bits 


}

/*
 * tux_set_button
 *   DESCRIPTION: Copy the current button state to the user-provided memory location.
 *   INPUTS:
 *     tty - Pointer to the TTY structure 
 *     arg - Pointer to user-provided memory location 
 *   OUTPUTS: Copies the button state to the user-provided memory.
 *   RETURN VALUE:
 *     - 0 on success.
 *     - -EINVAL if `arg` is NULL.
 *   SIDE EFFECTS: Copies the button state to user memory.
 */

int tux_set_button (struct tty_struct* tty, unsigned long arg){

	if(&arg == NULL){
		return -EINVAL; // return -EINVAL is arg is null
	}
	copy_to_user((int*)arg, &b_c, 1); // calling cop to user- copies 1 byte of data to the user space  
	return 0;
}
