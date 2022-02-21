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


struct t_config {
	uint8_t config_magic_number; //This used to check if there is a config already in the eeprom
	uint8_t number_of_channels;  //Default configuration 4 channels.
	uint8_t velocity_mult; //By default Velocity is from 0-127, by setting this 1 we multiply the value by 2.
};

//Okay kids never use globals. Except here, because I know what I'm doin' - said noone ever before
struct t_config config = {
	.config_magic_number = 42,
	.number_of_channels  = 4,
	.velocity_mult       = 0,
};
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

}

void EVENT_USB_Device_Disconnect(void)
{

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

	for (;;)
	{
		MIDI_EventPacket_t ReceivedMIDIEvent;
		if (MIDI_Device_ReceiveEventPacket(&Keyboard_MIDI_Interface, &ReceivedMIDIEvent))
		{
			uint8_t channel = ReceivedMIDIEvent.Data1 & 0x0F; //0-15
			if (ReceivedMIDIEvent.Event == MIDI_EVENT(0, MIDI_COMMAND_NOTE_ON))
			{
				//Calculate the right chip / A||B from the channel, note Ableton counts from 1-16 - so what is 2 here that's 3 in Ableton
				uint8_t dac = channel & 0b00000001;
				uint8_t chip_cv  = (channel & 0b00000010) >> 1;
				uint8_t chip_vel = chip_cv + 2;

				//Set CV For the given channel [C3-C7 is the max range]
				uint8_t midi_note = ReceivedMIDIEvent.Data2-60;
				if ((midi_note >= 0) && (midi_note < 49)) {
					set_analog_output(chip_cv,dac,vperoct[midi_note]);
				}
				//Set Velocity for the given channel
				set_analog_output(chip_vel,dac,ReceivedMIDIEvent.Data3);
				//Set the gate signal on
				set_bit(GATE_BUS, channel);
			}
			else if (ReceivedMIDIEvent.Event == MIDI_EVENT(0, MIDI_COMMAND_NOTE_OFF))
			{
				//Clear the gate when the note goes off
				clr_bit(GATE_BUS, channel);
			}
		}

		MIDI_Device_USBTask(&Keyboard_MIDI_Interface);
		USB_USBTask();
	}
}