/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
/* Xilinx includes. */
#include "xil_printf.h"
#include "xparameters.h"

#include "xil_io.h"
#include "xgpio.h"

#include <stdio.h>
#include "audio.h"
#include "sleep.h"
#include "xiicps.h"

#include "math.h"


#define TIMER_ID	1
#define DELAY_10_SECONDS	10000UL
#define DELAY_1_SECOND		1000UL
#define TIMER_CHECK_THRESHOLD	9

//gpio0
#define RGB_CHANNEL 1
#define SW_CHANNEL 2
#define RGB 0xFF

//gpio1
#define LED_CHANNEL 2
#define BTN_CHANNEL 1
#define LEDS 0x0F
/*-----------------------------------------------------------*/

typedef short		Xint16;
typedef long		Xint32;

unsigned char IicConfig(unsigned int DeviceIdPS);
void AudioPllConfig();
void AudioWriteToReg(unsigned char u8RegAddr, unsigned char u8Data);
void AudioConfigureJacks();
void LineinLineoutConfig();
void audio_test();

/* The Tx and Rx tasks as described at the top of this file. */
static void prvTxTask( void *pvParameters );
static void prvRxTask( void *pvParameters );
static void vTimerCallback( TimerHandle_t pxTimer );
/*-----------------------------------------------------------*/

/* The queue used by the Tx and Rx tasks, as described at the top of this
file. */
static TaskHandle_t xTxTask;
static TaskHandle_t xRxTask;
static TaskHandle_t xAudioTask;

static QueueHandle_t xQueue = NULL;
static TimerHandle_t xTimer = NULL;
char HWstring[15] = "beware of noise";
long RxtaskCntr = 0;
int status;
float time = 1.0;
/* audio parameters  */
unsigned int frequency=75;
unsigned int signal_type=0;
float amplitude=0.3;
// ADSR instellingen
float attack_time = 0.1;
float decay_time = 0.3;
float sustain_level = 0.7;
float release_time = 0.3;
float adsr_envelope = 1;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
#define QUEUE_BUFFER_SIZE		100

uint8_t ucQueueStorageArea[ QUEUE_BUFFER_SIZE ];
StackType_t xStack1[ configMINIMAL_STACK_SIZE ];
StackType_t xStack2[ configMINIMAL_STACK_SIZE ];
StaticTask_t xTxBuffer,xRxBuffer;
StaticTimer_t xTimerBuffer;
static StaticQueue_t xStaticQueue;
#endif

XGpio Gpio0;
XGpio Gpio1;
XIicPs Iic;
#if LWIP_DHCP==1
#include "lwip/dhcp.h"
extern volatile int dhcp_timoutcntr;
err_t dhcp_start(struct netif *netif);
#endif

static int complete_nw_thread;

void print_app_header();
void start_application();

#define THREAD_STACKSIZE 1024

#define DEFAULT_IP_ADDRESS "192.168.1.10"
#define DEFAULT_IP_MASK "255.255.255.0"
#define DEFAULT_GW_ADDRESS "192.168.1.1"

struct netif server_netif;
#define QUEUE_SIZE 10

// Define the queue handle globally
QueueHandle_t xQueue;

static void print_ip(char *msg, ip_addr_t *ip)
{
	xil_printf(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
				ip4_addr3(ip), ip4_addr4(ip));
}

static void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
	print_ip("Board IP:       ", ip);
	print_ip("Netmask :       ", mask);
	print_ip("Gateway :       ", gw);
}

static void assign_default_ip(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
	int err;

	xil_printf("Configuring default IP %s \r\n", DEFAULT_IP_ADDRESS);

	err = inet_aton(DEFAULT_IP_ADDRESS, ip);
	if(!err)
		xil_printf("Invalid default IP address: %d\r\n", err);

	err = inet_aton(DEFAULT_IP_MASK, mask);
	if(!err)
		xil_printf("Invalid default IP MASK: %d\r\n", err);

	err = inet_aton(DEFAULT_GW_ADDRESS, gw);
	if(!err)
		xil_printf("Invalid default gateway address: %d\r\n", err);
}

void network_thread(void *p)
{
	int mscnt = 0;
	/* the mac address of the board. this should be unique per board */
	u8_t mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(&server_netif, NULL, NULL, NULL, mac_ethernet_address,
		PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\r\n");
		return;
	}

	netif_set_default(&server_netif);

	/* specify that the network if is up */
	netif_set_up(&server_netif);

	/* start packet receive thread - required for lwIP operation */
	sys_thread_new("xemacif_input_thread",
			(void(*)(void*))xemacif_input_thread, &server_netif,
			THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);

	complete_nw_thread = 1;

	dhcp_start(&server_netif);
	while (1) {
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		dhcp_fine_tmr();
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= DHCP_COARSE_TIMER_SECS*1000) {
			dhcp_coarse_tmr();
			mscnt = 0;
		}
	}

}

int main_thread()
{

	int mscnt = 0;
	/* initialize lwIP before calling sys_thread_new */
	lwip_init();

	/* any thread using lwIP should be created using sys_thread_new */
	sys_thread_new("nw_thread", network_thread, NULL,
			THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);

	while(!complete_nw_thread)
		usleep(50);

	while (1) {
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		if (server_netif.ip_addr.addr) {
			xil_printf("DHCP request success\r\n");
			break;
		}
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= 10000) {
			xil_printf("ERROR: DHCP request timed out\r\n");
			assign_default_ip(&(server_netif.ip_addr),
						&(server_netif.netmask),
						&(server_netif.gw));
			break;
		}
	}

	print_ip_settings(&(server_netif.ip_addr), &(server_netif.netmask),
				&(server_netif.gw));
	xil_printf("\r\n");

	/* print all application headers */
	print_app_header();
	xil_printf("\r\n");

	/* start the application*/
	start_application();

	vTaskDelete(NULL);
	return 0;
}

void audio_thread(void *pvParameters)
{
    (void)pvParameters;


    	//Configure the IIC data structure
    	//IicConfig(XPAR_XIICPS_0_DEVICE_ID);
        audio_synthesizer();
}

int main( void )
{
	const TickType_t x10seconds = pdMS_TO_TICKS( DELAY_10_SECONDS );

	XGpio_Initialize(&Gpio0, XPAR_AXI_GPIO_0_DEVICE_ID);
	XGpio_Initialize(&Gpio1, XPAR_AXI_GPIO_1_DEVICE_ID);

	XGpio_SetDataDirection(&Gpio0, RGB_CHANNEL, ~RGB);
	XGpio_SetDataDirection(&Gpio1, LED_CHANNEL, ~LEDS);

	//Configure the IIC data structure
	IicConfig(XPAR_XIICPS_0_DEVICE_ID);

	//Configure the Audio Codec's PLL
	AudioPllConfig();

	//Configure the Line in and Line out ports.
	//Call LineInLineOutConfig() for a configuration that
	//enables the HP jack too.
	AudioConfigureJacks();
	LineinLineoutConfig();

#if ( configSUPPORT_STATIC_ALLOCATION == 0 ) /* Normal or standard use case */
	/* Create the two tasks.  The Tx task is given a lower priority than the
	Rx task, so the Rx task will leave the Blocked state and pre-empt the Tx
	task as soon as the Tx task places an item in the queue. */



	xTaskCreate(audio_test, "AudioTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &xAudioTask);


	/* Create the queue used by the tasks.  The Rx task has a higher priority
	than the Tx task, so will preempt the Tx task and remove values from the
	queue as soon as the Tx task writes to the queue - therefore the queue can
	never have more than one item in it. */
	xQueue = xQueueCreate( 	1,						/* There is only one space in the queue. */
							sizeof( HWstring ) );	/* Each space in the queue is large enough to hold a uint32_t. */

	/* Check the queue was created. */
	configASSERT( xQueue );

	/* Create a timer with a timer expiry of 10 seconds. The timer would expire
	 after 10 seconds and the timer call back would get called. In the timer call back
	 checks are done to ensure that the tasks have been running properly till then.
	 The tasks are deleted in the timer call back and a message is printed to convey that
	 the example has run successfully.
	 The timer expiry is set to 10 seconds and the timer set to not auto reload. */
	xTimer = xTimerCreate( (const char *) "Timer",
							x10seconds,
							pdFALSE,
							(void *) TIMER_ID,
							vTimerCallback);
	/* Check the timer was created. */
	configASSERT( xTimer );

#else /* Use case where memories for tasks/queues/timers etc are provided statically by the users */
	xil_printf( "Using static memory for tasks, queue and timer creations. \r\n" );
	xTxTask = xTaskCreateStatic( 	prvTxTask, 				/* The function that implements the task. */
						( const char * ) "Tx", 					/* Text name for the task, provided to assist debugging only. */
						configMINIMAL_STACK_SIZE, 							/* The stack allocated to the task. */
						( void * ) NULL, 						/* The task parameter is not used, so set to NULL. */
						tskIDLE_PRIORITY,						/* The task runs at the idle priority. */
						xStack1,								/* Array to use the task's stack  */
						&xTxBuffer );               			/* variable to hold the task data structure */
	xRxTask =  xTaskCreateStatic( prvRxTask,
				 ( const char * ) "Rx",
				 configMINIMAL_STACK_SIZE,
				 ( void * ) NULL,
				 tskIDLE_PRIORITY + 1,
				 xStack2,
				 &xRxBuffer );

	xQueue = xQueueCreateStatic( 1,				/* Number of items in the queue. */
								sizeof( HWstring ),			/*size for each item to be stored in queue */
								ucQueueStorageArea,         /* Buffer to store the queue items*/
								&xStaticQueue);				/* Each space in the queue is large enough to hold a 1 byte. */
	/* Check the queue was created. */
	configASSERT( xQueue );
	xTimer = xTimerCreateStatic( (const char *) "Timer",
							x10seconds,
							pdFALSE,
							(void *) TIMER_ID,
							vTimerCallback,
							&xTimerBuffer);
	/* Check the timer was created. */
	configASSERT( xTimer );

#endif

	/* start the timer with a block time of 0 ticks. This means as soon
	   as the schedule starts the timer will start running and will expire after
	   10 seconds */
	xTimerStart( xTimer, 0 );

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	for( ;; );
}

void calculate_adsr_envelope() {
    if (time < attack_time) {
        // Attack
        adsr_envelope = time / attack_time;
    } else if (time < (attack_time + decay_time)) {
        // Decay
        adsr_envelope = 1.0 - (1.0 - sustain_level) * ((time - attack_time) / decay_time);
    } else {
        // Sustain (constant)
        adsr_envelope = sustain_level;

        // Release
        if (release_time > 0 && time > (attack_time + decay_time)) {
            adsr_envelope *= 1.0 - ((time - attack_time - decay_time) / release_time);
        }
    }
}

/*-----------------------------------------------------------*/
void audio_test(){
	unsigned long u32DataL, u32DataR;
	unsigned long u32Temp;
	float sample_rate = 44100.0;  // Voorbeeld: 44.1 kHz sample rate
	float audio_sample=0;
	int i =0;
	vTaskDelay(pdMS_TO_TICKS(4000));

	int waveform = 1;
	//audio parameters
	unsigned int signal_type = 3;
	float amplitude = 0.3;
	unsigned int frequency = 100;
	//ADSR instellingen
	float attack_time = 0.05;
	float decay_time = 0.02;
	float sustain_level = 0.5;
	float release_time = 0.3;

	char recvData[1500];

	while (1)
	{
		do{ //wait for RX data to become available

			u32Temp = Xil_In32(I2S_STATUS_REG);
		} while ( u32Temp == 0);

		Xil_Out32(I2S_STATUS_REG, 0x00000001); //Clear data rdy bit

		calculate_adsr_envelope();

		//ontvang data
		if(xQueueReceive(xQueue, recvData, 0) == pdTRUE)
				{
					xil_printf("Received data from queue: %s\n\r", recvData);
					sscanf(recvData, "waveform: %d ; freq: %lf ; amp: %lf ; phase: %lf",
											&(waveform), &(frequency),
											&(amplitude), &(phase));

				}

		//welke wave signal
		if (signal_type == 0){
			// Sinusgolf
			XGpio_DiscreteSet(&Gpio1, 2, 0x1);
			audio_sample = sin(2 * M_PI * frequency * time);
		}
		else if (signal_type == 1) {
		    // Driehoekgolf
			XGpio_DiscreteSet(&Gpio1, 2, 0x2);
			float period = 1.0 / frequency;
			float phase = fmod(time, period) / period;
			audio_sample = 2.0 * (fabs(phase - 0.5) - 0.25);
		}
		else if (signal_type == 2) {
		    // Zaagtandgolf
			XGpio_DiscreteSet(&Gpio1, 2, 0x4);
		    float period = 1.0 / frequency;
		    float phase = fmod(time, period) / period;
			if (phase < attack_time) {
				audio_sample = (2.0 * amplitude * phase / attack_time) - amplitude;
			}
			else {
				audio_sample = amplitude * (1.0 - (2.0 * (phase - attack_time) / (1.0 - attack_time)));
			}
		}
		else if (signal_type == 3) {
		    // Vierkantsgolf
			XGpio_DiscreteSet(&Gpio1, 2, 0x8);
		    float period = 1.0 / frequency;
		    float phase = fmod(time, period) / period;
		    audio_sample = (phase < 0.5) ? amplitude : -amplitude;
		}

		i++;
		if (i>1000000)
		{
			signal_type = 1;
		}
		if (i>2000000)
		{
			signal_type = 2;
		}
		if (i>3000000)
		{
			signal_type = 3;
		}


		audio_sample *= adsr_envelope;

		u32DataL = (unsigned long)(audio_sample * 32767.0);
		u32DataR = u32DataL;  // Voorbeeld: mono signaal

		time += 1.0 / sample_rate;

		Xil_Out32(I2S_DATA_TX_L_REG, u32DataL);
		Xil_Out32(I2S_DATA_TX_R_REG, u32DataR);
		XGpio_DiscreteClear(&Gpio1, 2, 0xF);
	}
}

/*-----------------------------------------------------------*/
unsigned char IicConfig(unsigned int DeviceIdPS)
{
	XIicPs_Config *Config;
	int Status;

	//Initialize the IIC driver so that it's ready to use
	//Look up the configuration in the config table, then initialize it.
	Config = XIicPs_LookupConfig(DeviceIdPS);
	if(NULL == Config) {
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if(Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	//Set the IIC serial clock rate.
	XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);

	return XST_SUCCESS;
}

/*-----------------------------------------------------------*/
void AudioPllConfig() {

	unsigned char u8TxData[8], u8RxData[6];

	AudioWriteToReg(R0_CLOCK_CONTROL, 0x0E);

	// Write 6 bytes to R1
	u8TxData[0] = 0x40;
	u8TxData[1] = 0x02;
	u8TxData[2] = 0x02; // byte 1
	u8TxData[3] = 0x71; // byte 2
	u8TxData[4] = 0x02; // byte 3
	u8TxData[5] = 0x3C; // byte 4
	u8TxData[6] = 0x21; // byte 5
	u8TxData[7] = 0x01; // byte 6


	XIicPs_MasterSendPolled(&Iic, u8TxData, 8, (IIC_SLAVE_ADDR >> 1));
	while(XIicPs_BusIsBusy(&Iic));

	// Poll PLL Lock bit
	u8TxData[0] = 0x40;
	u8TxData[1] = 0x02;

	do {
		XIicPs_MasterSendPolled(&Iic, u8TxData, 2, (IIC_SLAVE_ADDR >> 1));
		while(XIicPs_BusIsBusy(&Iic));
		XIicPs_MasterRecvPolled(&Iic, u8RxData, 6, (IIC_SLAVE_ADDR >> 1));
		while(XIicPs_BusIsBusy(&Iic));
	}
	while((u8RxData[5] & 0x02) == 0);

	AudioWriteToReg(R0_CLOCK_CONTROL, 0x0F);//COREN
}

/*-----------------------------------------------------------*/
void AudioWriteToReg(unsigned char u8RegAddr, unsigned char u8Data) {

	unsigned char u8TxData[3];

	u8TxData[0] = 0x40;
	u8TxData[1] = u8RegAddr;
	u8TxData[2] = u8Data;

	XIicPs_MasterSendPolled(&Iic, u8TxData, 3, (IIC_SLAVE_ADDR >> 1));
	while(XIicPs_BusIsBusy(&Iic));
}

/*-----------------------------------------------------------*/
void AudioConfigureJacks()
{
	AudioWriteToReg(R4_RECORD_MIXER_LEFT_CONTROL_0, 0x01); //enable mixer 1
	AudioWriteToReg(R5_RECORD_MIXER_LEFT_CONTROL_1, 0x07); //unmute Left channel of line in into mxr 1 and set gain to 6 db
	AudioWriteToReg(R6_RECORD_MIXER_RIGHT_CONTROL_0, 0x01); //enable mixer 2
	AudioWriteToReg(R7_RECORD_MIXER_RIGHT_CONTROL_1, 0x07); //unmute Right channel of line in into mxr 2 and set gain to 6 db
	AudioWriteToReg(R19_ADC_CONTROL, 0x13); //enable ADCs

	AudioWriteToReg(R22_PLAYBACK_MIXER_LEFT_CONTROL_0, 0x21); //unmute Left DAC into Mxr 3; enable mxr 3
	AudioWriteToReg(R24_PLAYBACK_MIXER_RIGHT_CONTROL_0, 0x41); //unmute Right DAC into Mxr4; enable mxr 4
	AudioWriteToReg(R26_PLAYBACK_LR_MIXER_LEFT_LINE_OUTPUT_CONTROL, 0x05); //unmute Mxr3 into Mxr5 and set gain to 6db; enable mxr 5
	AudioWriteToReg(R27_PLAYBACK_LR_MIXER_RIGHT_LINE_OUTPUT_CONTROL, 0x11); //unmute Mxr4 into Mxr6 and set gain to 6db; enable mxr 6
	AudioWriteToReg(R29_PLAYBACK_HEADPHONE_LEFT_VOLUME_CONTROL, 0x00);//Mute Left channel of HP port (LHP)
	AudioWriteToReg(R30_PLAYBACK_HEADPHONE_RIGHT_VOLUME_CONTROL, 0x00); //Mute Right channel of HP port (LHP)
	AudioWriteToReg(R31_PLAYBACK_LINE_OUTPUT_LEFT_VOLUME_CONTROL, 0xE6); //set LOUT volume (0db); unmute left channel of Line out port; set Line out port to line out mode
	AudioWriteToReg(R32_PLAYBACK_LINE_OUTPUT_RIGHT_VOLUME_CONTROL, 0xE6); // set ROUT volume (0db); unmute right channel of Line out port; set Line out port to line out mode

	AudioWriteToReg(R35_PLAYBACK_POWER_MANAGEMENT, 0x03); //enable left and right channel playback (not sure exactly what this does...)
	AudioWriteToReg(R36_DAC_CONTROL_0, 0x03); //enable both DACs

	AudioWriteToReg(R58_SERIAL_INPUT_ROUTE_CONTROL, 0x01); //Connect I2S serial port output (SDATA_O) to DACs
	AudioWriteToReg(R59_SERIAL_OUTPUT_ROUTE_CONTROL, 0x01); //connect I2S serial port input (SDATA_I) to ADCs

	AudioWriteToReg(R65_CLOCK_ENABLE_0, 0x7F); //Enable clocks
	AudioWriteToReg(R66_CLOCK_ENABLE_1, 0x03); //Enable rest of clocks
}

/*-----------------------------------------------------------*/
void LineinLineoutConfig() {

	AudioWriteToReg(R17_CONVERTER_CONTROL_0, 0x06);//96 kHz
	AudioWriteToReg(R64_SERIAL_PORT_SAMPLING_RATE, 0x06);//96 kHz
	AudioWriteToReg(R19_ADC_CONTROL, 0x13);
	AudioWriteToReg(R36_DAC_CONTROL_0, 0x03);
	AudioWriteToReg(R35_PLAYBACK_POWER_MANAGEMENT, 0x03);
	AudioWriteToReg(R58_SERIAL_INPUT_ROUTE_CONTROL, 0x01);
	AudioWriteToReg(R59_SERIAL_OUTPUT_ROUTE_CONTROL, 0x01);
	AudioWriteToReg(R65_CLOCK_ENABLE_0, 0x7F);
	AudioWriteToReg(R66_CLOCK_ENABLE_1, 0x03);

	AudioWriteToReg(R4_RECORD_MIXER_LEFT_CONTROL_0, 0x01);
	AudioWriteToReg(R5_RECORD_MIXER_LEFT_CONTROL_1, 0x05);//0 dB gain
	AudioWriteToReg(R6_RECORD_MIXER_RIGHT_CONTROL_0, 0x01);
	AudioWriteToReg(R7_RECORD_MIXER_RIGHT_CONTROL_1, 0x05);//0 dB gain

	AudioWriteToReg(R22_PLAYBACK_MIXER_LEFT_CONTROL_0, 0x21);
	AudioWriteToReg(R24_PLAYBACK_MIXER_RIGHT_CONTROL_0, 0x41);
	AudioWriteToReg(R26_PLAYBACK_LR_MIXER_LEFT_LINE_OUTPUT_CONTROL, 0x03);//0 dB
	AudioWriteToReg(R27_PLAYBACK_LR_MIXER_RIGHT_LINE_OUTPUT_CONTROL, 0x09);//0 dB
	AudioWriteToReg(R29_PLAYBACK_HEADPHONE_LEFT_VOLUME_CONTROL, 0xE7);//0 dB
	AudioWriteToReg(R30_PLAYBACK_HEADPHONE_RIGHT_VOLUME_CONTROL, 0xE7);//0 dB
	AudioWriteToReg(R31_PLAYBACK_LINE_OUTPUT_LEFT_VOLUME_CONTROL, 0x00);//0 dB
	AudioWriteToReg(R32_PLAYBACK_LINE_OUTPUT_RIGHT_VOLUME_CONTROL, 0x00);//0 dB
}

