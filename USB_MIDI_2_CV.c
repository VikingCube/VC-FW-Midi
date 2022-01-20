#include "USB_MIDI_2_CV.h"

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

    /*
     Board pins
     * D0-D7 - Vcc for Leds - X
     * D8-D13, a0, a1 - GND for Leds - Y
     * 
     * Mapping to Ports
     * D0      - PD2 xx
     * D1      - PD3 xx
     * D2      - PD1 xx
     * D3      - PD0 xx
     * D4/A6   - PD4 xx
     * D5      - PC6 xx
     * D6/A7   - PD7 xx
     * D7      - PE6 xx
     * 
     * D8      - PB4 xa
     * D9/A8   - PB5 xa
     * D10     - PB6 xa
     * D11     - PB7 xa
     * D12/A10 - PD6 xa
     * D13     - PC7 x
     * 
     * A0 - PF7 o
     * A1 - PF6 o
     * A2 - PF5 -
     * A3 - PF4 -
     * A4 - PF1 -
     * A5 - PF0 -
     * 
     * PD5 - LED on 0
     * PB0 - LED on 0
     */

int main(void)
{
	SetupHardware();
	GlobalInterruptEnable(); //Check if we need this for USB

	DDRD |= 0b00100000;
	PORTD |= 0x20; //LED OFF

	DDRB |= 0b00000001;
	PORTB |= 0x01;

	for (;;)
	{
		MIDI_EventPacket_t ReceivedMIDIEvent;
		if (MIDI_Device_ReceiveEventPacket(&Keyboard_MIDI_Interface, &ReceivedMIDIEvent))
		{
			if ((ReceivedMIDIEvent.Event == MIDI_EVENT(0, MIDI_COMMAND_NOTE_ON)) && ((ReceivedMIDIEvent.Data1 & 0x0F) == 0))
			{
				PORTD &= ~0x20;					
			}
			else if ((ReceivedMIDIEvent.Event == MIDI_EVENT(0, MIDI_COMMAND_NOTE_OFF)) && ((ReceivedMIDIEvent.Data1 & 0x0F) == 0))
			{
				PORTD |= 0x20;
			}
			else if ((ReceivedMIDIEvent.Event == MIDI_EVENT(1, MIDI_COMMAND_NOTE_ON)) && ((ReceivedMIDIEvent.Data1 & 0x0F) == 0))
			{
				PORTB &= ~0x01;
			}
			else if ((ReceivedMIDIEvent.Event == MIDI_EVENT(1, MIDI_COMMAND_NOTE_OFF)) && ((ReceivedMIDIEvent.Data1 & 0x0F) == 0))
			{
				PORTB |= 0x01;
			}
		}

		MIDI_Device_USBTask(&Keyboard_MIDI_Interface);
		USB_USBTask();
	}
}

void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	/* Hardware Initialization */
	USB_Init();
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

