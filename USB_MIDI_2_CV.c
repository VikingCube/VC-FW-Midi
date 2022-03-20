#include "USB_MIDI_2_CV.h"
#include <avr/cpufunc.h>


#define set_bit(__prt, __bit) (__prt |= _BV(__bit))
#define clr_bit(__prt, __bit) (__prt &= ~(_BV(__bit)))

#define DATA_BUS PORTD
#define CS_BUS PORTB
#define GATE_BUS PORTF
#define CS1 PORTB0 //Note PORTB is hardcoded in the code, these are just to help readabilty not for portability.
#define CS2 PORTB1
#define CS3 PORTB2
#define CS4 PORTB4
#define DACAB PORTC6

//Add subtract 60 from the midi tone
const uint8_t vperoct[] = { 
0,     4,   9,  13,  17,  21,  26,  30,  34,  38,  43,  47,
51,   55,  60,  64,  68,  73,  77,  81,  85,  90,  94,  98,
102, 107, 111, 115, 119, 124, 128, 132, 137, 141, 145, 149,
154, 158, 162, 166, 171, 175, 179, 183, 188, 192, 196, 201,
205
};

/*const uint8_t sins[] = {
127, 129, 131, 134, 136, 138, 140, 142, 145, 147, 149, 151,
153, 156, 158, 160, 162, 164, 166, 168, 170, 173, 175, 177,
179, 181, 183, 185, 187, 189, 191, 192, 194, 196, 198, 200,
202, 203, 205, 207, 209, 210, 212, 214, 215, 217, 218, 220,
221, 223, 224, 226, 227, 228, 230, 231, 232, 234, 235, 236,
237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 246, 247,
248, 248, 249, 250, 250, 251, 251, 252, 252, 252, 253, 253,
253, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
253, 253, 253, 252, 252, 252, 251, 251, 250, 250, 249, 248,
248, 247, 246, 246, 245, 244, 243, 242, 241, 240, 239, 238,
237, 236, 235, 234, 232, 231, 230, 228, 227, 226, 224, 223,
221, 220, 218, 217, 215, 214, 212, 210, 209, 207, 205, 203, 
202, 200, 198, 196, 194, 192, 191, 189, 187, 185, 183, 181,
179, 177, 175, 173, 170, 168, 166, 164, 162, 160, 158, 156,
153, 151, 149, 147, 145, 142, 140, 138, 136, 134, 131, 129,
127, 125, 123, 120, 118, 116, 114, 112, 109, 107, 105, 103,
101,  98,  96,  94,  92,  90,  88,  86,  84,  81,  79,  77,
 75,  73,  71,  69,  67,  65,  64,  62,  60,  58,  56,  54,
 52,  51,  49,  47,  45,  44,  42,  40,  39,  37,  36,  34,
 33,  31,  30,  28,  27,  26,  24,  23,  22,  20,  19,  18,
 17,  16,  15,  14,  13,  12,  11,  10,   9,   8,   8,   7,
  6,   6,   5,   4,   4,   3,   3,   2,   2,   2,   1,   1,
  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  1,   1,   1,   2,   2,   2,   3,   3,   4,   4,   5,   6,
  6,   7,   8,   8,   9,  10,  11,  12,  13,  14,  15,  16,
 17,  18,  19,  20,  22,  23,  24,  26,  27,  28,  30,  31,
 33,  34,  36,  37,  39,  40,  42,  44,  45,  47,  49,  51,
 52,  54,  56,  58,  60,  62,  63,  65,  67,  69,  71,  73,
 75,  77,  79,  81,  84,  86,  88,  90,  92,  94,  96,  98,
101, 103, 105, 107, 109, 112, 114, 116, 118, 120, 123, 125
};
*/
enum channel_mode_t { MIDI, USBCO };

struct t_config {
	uint8_t 			config_magic_number; //This used to check if there is a config already in the eeprom - TODO
	uint8_t 			velocity_mult; //By default Velocity is from 0-127, by setting this 1 we multiply the value by 2.
	enum channel_mode_t ch_mode[4];
};

//Okay kids never use globals. Except here, because I know what I'm doin' - said noone ever before
struct t_config config = {
	.config_magic_number = 42,
	.velocity_mult       = 1,
	.ch_mode			 = {MIDI,MIDI,MIDI,MIDI},
};

//Magic pointer to jump into boot mode
typedef void (*AppPtr_t)(void) __attribute__((noreturn));
        const AppPtr_t jump_boot = (AppPtr_t) 0x3800;

/** LUFA MIDI Class driver interface configuration and state information. This structure is
 *  passed to all MIDI Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_MIDI_Device_t Keyboard_MIDI_Interface =
	{
		.Config =
			{
				.StreamingInterfaceNumber = INTERFACE_ID_AudioStream,
				.DataINEndpoint           =
					{
						.Address          = MIDI_STREAM_IN_EPADDR,
						.Size             = MIDI_STREAM_EPSIZE,
						.Banks            = 1,
					},
				.DataOUTEndpoint           =
					{
						.Address          = MIDI_STREAM_OUT_EPADDR,
						.Size             = MIDI_STREAM_EPSIZE,
						.Banks            = 1,
					},
			},
	};

void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	/* Hardware Initialization */
	USB_Init(); //TODO

	/* Setup DATA Bus */
	DDRD=0xFF;
	PORTD=0x00;

	/* Setup Chip selec pins and L1-L3 */
	DDRB=0xFF;
	PORTB=0x0F; //L1-L3 off, _CS all high

	/* Setup DACA/B pin */
	set_bit(DDRC, DACAB);
	clr_bit(PORTC, DACAB);

	/* Setup G1-4 pins */
	DDRF=0xFF; //PIN6-7 unused
	PORTF=0x00;

	/* Setup prog button */
	clr_bit(DDRE, PE2);
}

/*

	| Chip | DAC   | Output |
	|------|-------|--------|
	|  0   |  A(0) | CV 1   |  *NOTE:* in the code we start to count them from 0 on the Circuit diagramm they start from 1
	|  0   |  B(1) | CV 2   |
	|  1   |  A(0) | CV 3   |
	|  1   |  B(1) | CV 4   |
	|  2   |  A(0) | Vel 1  |
	|  2   |  B(1) | Vel 2  |
	|  3   |  A(0) | Vel 3  |
	|  3   |  B(1) | Vel 4  |

*/
void set_analog_output(uint8_t chip, uint8_t dacab, uint8_t  value)
{
	//Set Data
	DATA_BUS = value;
	//Select DAC A/B
	if (dacab > 0) {
		set_bit(PORTC, DACAB);
	} else {
		clr_bit(PORTC, DACAB);
	}
	
	//Select Chip & Write data (as WR fixed to 0)
	CS_BUS &= ~(1 << chip); //CS needs 50ns based on the datasheet, we run on 16 MHz one instruction takes 62,5 ns so should be fine. Otherwise add a minimum delay below, may be a nop.
	CS_BUS |= 0x0F; //Set all CS to high again
}

void EVENT_USB_Device_Connect(void)
{
	set_bit(PORTB,PB7);
}

void EVENT_USB_Device_Disconnect(void)
{
	clr_bit(PORTB,PB7);
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;
	ConfigSuccess &= MIDI_Device_ConfigureEndpoints(&Keyboard_MIDI_Interface);
}

void EVENT_USB_Device_ControlRequest(void)
{
	MIDI_Device_ProcessControlRequest(&Keyboard_MIDI_Interface);
}

int main(void)
{
	SetupHardware();
	GlobalInterruptEnable(); //Check if we need this for USB

	//Set power LED
	set_bit(PORTB, PB6);

	//Variables for CH mode
	uint8_t channel   = 0;
	uint8_t dac 	  = 0;
	uint8_t chip_cv   = 0;
	uint8_t chip_vel  = 0;
	uint8_t midi_note = 0;
	
	//Variables for poly mode
	uint8_t ch_in_use[4] = {255, 255, 255, 255}; //255 stands for "used"
	uint8_t gate 		 = 0;

	//var for data led
	uint16_t datacnt = 0;
	for(;;) {
		//4 channel mode loop, we do this to minimaze the number of branches
		for (;;)
		{
			if (datacnt >= 3200) {
				clr_bit(PORTB, PB5);
				datacnt = 0;
			}
			++datacnt;
			//Handle prog button
			if (bit_is_clear(PINE, PE2)) {
				clr_bit(PORTB,PB7);
				clr_bit(PORTB,PB6);
				set_bit(PORTB,PB5);
				jump_boot();
			}
			MIDI_EventPacket_t ReceivedMIDIEvent;
			if (MIDI_Device_ReceiveEventPacket(&Keyboard_MIDI_Interface, &ReceivedMIDIEvent))
			{
				set_bit(PORTB,PB5);
				channel = ReceivedMIDIEvent.Data1 & 0x0F; //0-15
				if (channel == 4) break; //Channel 5 means we switch to poly mode
				if (channel < 4) { //We have only 4 channels
						switch (ReceivedMIDIEvent.Event) {
							case MIDI_EVENT(0, MIDI_COMMAND_NOTE_ON):
								//Calculate the right chip / A||B from the channel, note Ableton counts from 1-16 - so what is 2 here that's 3 in Ableton
								dac = channel & 0b00000001;
								chip_cv  = (channel & 0b00000010) >> 1;
								chip_vel = chip_cv + 2;

								//Set CV For the given channel [C3-C7 is the max range]
								midi_note = ReceivedMIDIEvent.Data2-60;
								if ((midi_note >= 0) && (midi_note < 49)) {
									set_analog_output(chip_cv,dac,vperoct[midi_note]);
								}
								//Set Velocity for the given channel
								set_analog_output(chip_vel,dac,ReceivedMIDIEvent.Data3 << config.velocity_mult);
								//Set the gate signal on
								if (channel > 1) channel+=2;
								set_bit(GATE_BUS, channel);
								break;

							case MIDI_EVENT(0, MIDI_COMMAND_NOTE_OFF):
								if (channel > 1) channel+=2;
								//Clear the gate when the note goes off
								clr_bit(GATE_BUS, channel);
								break;
						}
				}
			}

			MIDI_Device_USBTask(&Keyboard_MIDI_Interface);
			USB_USBTask();
		}
		//************************************************************************************************************************
		//Poly mode loop
		for (;;)
		{
			if (datacnt >= 3200) {
				clr_bit(PORTB, PB5);
				datacnt = 0;
			}
			++datacnt;
			//Handle prog button
			if (bit_is_clear(PINE, PE2)) {
				clr_bit(PORTB,PB7);
				clr_bit(PORTB,PB6);
				set_bit(PORTB,PB5);
				jump_boot();
			}
			MIDI_EventPacket_t ReceivedMIDIEvent;
			if (MIDI_Device_ReceiveEventPacket(&Keyboard_MIDI_Interface, &ReceivedMIDIEvent))
			{
				set_bit(PORTB,PB5);
				channel = ReceivedMIDIEvent.Data1 & 0x0F; //0-15
				if (channel < 4) {  //if CH1-4 used we switch back to 4 channel mode
					//Free up channels
					for ( int i = 0 ; i < 4 ; i++ ) {
						ch_in_use[i] = 255;
					}
					//Set all gates low
					GATE_BUS = 0x00;
					break;
				}
				if (channel == 4) { //Above 4 we ignore
						switch (ReceivedMIDIEvent.Event) {
							case MIDI_EVENT(0, MIDI_COMMAND_NOTE_ON):
								//Look for a free channel
								for ( int i = 0 ; i < 4 ; i++ ) {
									if (ch_in_use[i] == 255) {
										//We have a free channel, let's use it
										midi_note = ReceivedMIDIEvent.Data2-60;
										ch_in_use[i] = midi_note;
										switch (i)
										{
											case 0:
												dac 	  = 0;
												chip_cv   = 0;
												chip_vel  = 2;
												break;
											case 1:
												dac 	  = 1;
												chip_cv   = 0;
												chip_vel  = 2;
												break;
											case 2:
												dac 	  = 0;
												chip_cv   = 1;
												chip_vel  = 3;
												break;
											case 3:
												dac 	  = 1;
												chip_cv   = 1;
												chip_vel  = 3;
												break;
										}
										if ((midi_note >= 0) && (midi_note < 49)) {
											set_analog_output(chip_cv,dac,vperoct[midi_note]);
										}
										//Set Velocity for the given channel
										set_analog_output(chip_vel,dac,ReceivedMIDIEvent.Data3 << config.velocity_mult);
										gate = i;
										if (gate > 1) gate+=2;
										set_bit(GATE_BUS, gate);
										break;
									}
								}
								break;

							case MIDI_EVENT(0, MIDI_COMMAND_NOTE_OFF):
								//Find the channel used for this note
								midi_note = ReceivedMIDIEvent.Data2-60;
								for ( int i = 0 ; i < 4; i++ ) {
									if (ch_in_use[i] == midi_note) {
										//Found it!
										gate = i;
										if (gate > 1) gate+=2;
										//Clear the gate when the note goes off
										clr_bit(GATE_BUS, gate);
										//Free up the channel
										ch_in_use[i] = 255;
										break;
									}
								}
								break;
						}
				}
			}

			MIDI_Device_USBTask(&Keyboard_MIDI_Interface);
			USB_USBTask();
		}
	}
}