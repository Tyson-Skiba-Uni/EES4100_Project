// Tyson Skiba 2016
// BACnet Server (Project Part 2)
// Based Heavily Upon https://github.com/kmtaylor/EES4100_Testbench/blob/master/src/bacnet_server.c

#include <stdio.h>
#include <libbacnet/address.h>
#include <libbacnet/device.h>
#include <libbacnet/handlers.h>
#include <libbacnet/datalink.h>
#include <libbacnet/bvlc.h>
#include <libbacnet/client.h>
#include <libbacnet/txbuf.h>
#include <libbacnet/tsm.h>
#include <libbacnet/ai.h>
#include "bacnet_namespace.h"

#define BACNET_INSTANCE_NO	    34							// Provided by University
#define BACNET_PORT		    0xBAC1						// BACnet Port 1
#define BACNET_INTERFACE	    "lo"						// Local Interface
#define BACNET_DATALINK_TYPE	    "bvlc"						// BAcnet Layer Type
#define BACNET_SELECT_TIMEOUT_MS    1	    						// 1 milliseond timeout

#define RUN_AS_BBMD_CLIENT	    1							// Run as BBMD Client (Bool)

#if RUN_AS_BBMD_CLIENT									 
#define BACNET_BBMD_PORT	    0xBAC0						// BACnet Port 0
#define BACNET_BBMD_ADDRESS	    "127.0.0.1"						// Broadcast Table IP Address is Local Host
#define BACNET_BBMD_TTL		    90							// Time to Live set to 90 seconds
#endif


static uint16_t test_data[] = {								// Define a static test data array
    0xA4EC, 0x6E39, 0x8740, 0x1065, 0x9134, 0xFC8C };					// Test data to provide to client
#define NUM_TEST_DATA (sizeof(test_data)/sizeof(test_data[0]))

static pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;				// Initialise PThread

static int Update_Analog_Input_Read_Property( BACNET_READ_PROPERTY_DATA *rpdata) {	// Function for Analogue Input

    static int index;									// Initialise Index Value
    int instance_no = bacnet_Analog_Input_Instance_To_Index(rpdata->object_instance);   // Get Instance Number from rpdata.object 

    if (rpdata->object_property != bacnet_PROP_PRESENT_VALUE) goto not_pv;		// If present value does not equal the data from rpdata go to -->

    printf("Instance %i: \n", instance_no);						// Print String to STDOUT
    /* Update the values to be sent to the BACnet client here.
     * The data should be read from the head of a linked list. You are required		// Work is required here to implement a linked list
     * to implement this list functionality.
     *
     * bacnet_Analog_Input_Present_Value_Set() 
     *     First argument: Instance No
     *     Second argument: data to be sent
     *
     * Without reconfiguring libbacnet, a maximum of 4 values may be sent */
    bacnet_Analog_Input_Present_Value_Set(0, test_data[index++]);
    /* bacnet_Analog_Input_Present_Value_Set(1, test_data[index++]); */
    /* bacnet_Analog_Input_Present_Value_Set(2, test_data[index++]); */
    
    if (index == NUM_TEST_DATA) index = 0;

    not_pv:										// <------ Here
    return bacnet_Analog_Input_Read_Property(rpdata);					// Return the Data
}

static bacnet_object_functions_t server_objects[] = {					// Define the objects
    {bacnet_OBJECT_DEVICE,
	    NULL,
	    bacnet_Device_Count,
	    bacnet_Device_Index_To_Instance,
	    bacnet_Device_Valid_Object_Instance_Number,
	    bacnet_Device_Object_Name,
	    bacnet_Device_Read_Property_Local,
	    bacnet_Device_Write_Property_Local,
	    bacnet_Device_Property_Lists,
	    bacnet_DeviceGetRRInfo,
	    NULL, /* Iterator */
	    NULL, /* Value_Lists */
	    NULL, /* COV */
	    NULL, /* COV Clear */
	    NULL  /* Intrinsic Reporting */
    },
    {bacnet_OBJECT_ANALOG_INPUT,
            bacnet_Analog_Input_Init,
            bacnet_Analog_Input_Count,
            bacnet_Analog_Input_Index_To_Instance,
            bacnet_Analog_Input_Valid_Instance,
            bacnet_Analog_Input_Object_Name,
            Update_Analog_Input_Read_Property,
            bacnet_Analog_Input_Write_Property,
            bacnet_Analog_Input_Property_Lists,
            NULL /* ReadRangeInfo */ ,
            NULL /* Iterator */ ,
            bacnet_Analog_Input_Encode_Value_List,
            bacnet_Analog_Input_Change_Of_Value,
            bacnet_Analog_Input_Change_Of_Value_Clear,
            bacnet_Analog_Input_Intrinsic_Reporting},
    {MAX_BACNET_OBJECT_TYPE}
};

static void register_with_bbmd(void) {								// Funtion to register on Broadcast Table
#if RUN_AS_BBMD_CLIENT
    /* Thread safety: Shares data with datalink_send_pdu */
    bacnet_bvlc_register_with_bbmd(
	    bacnet_bip_getaddrbyname(BACNET_BBMD_ADDRESS), 
	    htons(BACNET_BBMD_PORT),
	    BACNET_BBMD_TTL);
#endif
}

static void *minute_tick(void *arg) {								// Minute Function
    while (1) {
	pthread_mutex_lock(&timer_lock);							// Take timer thread lock
	bacnet_address_cache_timer(60);								// Kill after 60 seconds
	register_with_bbmd();									// Re register once TTL has expired

	/* Update addresses for notification class recipient list 
	 * Requred for INTRINSIC_REPORTING
	 * bacnet_Notification_Class_find_recipient(); */
	pthread_mutex_unlock(&timer_lock);							// Release the lock, SLEEP IS A BLOCKING FUNCTION
	sleep(60);										// Sleep for 60 Seconds (1 Minute)
    }
    return arg;											// Return the same argument just 60 seconds later
}

static void *second_tick(void *arg) {								// Second Function
    while (1) {											// Ever Loop, Keeps running
	pthread_mutex_lock(&timer_lock);							// Take the timer thread lock
	bacnet_bvlc_maintenance_timer(1);							// Invalidates stale BBMD foreign device table entries 
	bacnet_tsm_timer_milliseconds(1000);							// Transmission State Machine control Acknowlegement and ReTransmissions

	/* Re-enables communications after DCC_Time_Duration_Seconds
	 * Required for SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL
	 * bacnet_dcc_timer_seconds(1); */

	/* State machine for load control object
	 * Required for OBJECT_LOAD_CONTROL
	 * bacnet_Load_Control_State_Machine_Handler(); */

	/* Expires any COV subscribers that have finite lifetimes
	 * Required for SERVICE_CONFIRMED_SUBSCRIBE_COV
	 * bacnet_handler_cov_timer_seconds(1); */

	/* Monitor Trend Log uLogIntervals and fetch properties
	 * Required for OBJECT_TRENDLOG
	 * bacnet_trend_log_timer(1); */
	
	/* Run [Object_Type]_Intrinsic_Reporting() for all objects in device
	 * Required for INTRINSIC_REPORTING
	 * bacnet_Device_local_reporting(); */
	
	pthread_mutex_unlock(&timer_lock);							// Release Lock
	sleep(1);										// Sleep for 1 Second
    }
    return arg;
}

static void ms_tick(void) {									// millisecond function
    /* Updates change of value COV subscribers.
     * Required for SERVICE_CONFIRMED_SUBSCRIBE_COV
     * bacnet_handler_cov_task(); */
}

#define BN_UNC(service, handler) \
    bacnet_apdu_set_unconfirmed_handler(		\
		    SERVICE_UNCONFIRMED_##service,	\
		    bacnet_handler_##handler)
#define BN_CON(service, handler) \
    bacnet_apdu_set_confirmed_handler(			\
		    SERVICE_CONFIRMED_##service,	\
		    bacnet_handler_##handler)

int main(int argc, char **argv) {								// Main Function, Execution will begin here 
    uint8_t rx_buf[bacnet_MAX_MPDU];								// Initialise recieved data buffer
    uint16_t pdu_len;										// Initialise pdu length
    BACNET_ADDRESS src;
    pthread_t minute_tick_id, second_tick_id;							// Declare Threads

    bacnet_Device_Set_Object_Instance_Number(BACNET_INSTANCE_NO);				// Set the Instance Number
    bacnet_address_init();									// Initialise BACnet

    bacnet_Device_Init(server_objects);								// Initialise BACnet Objects
    BN_UNC(WHO_IS, who_is);									// Populate Object Field who is
    BN_CON(READ_PROPERTY, read_property);							// Populate Object Field read property

    bacnet_BIP_Debug = true;									// Enable BACnet IP Debugging
    bacnet_bip_set_port(htons(BACNET_PORT));							// Convert to Network Style IP and Set IP 
    bacnet_datalink_set(BACNET_DATALINK_TYPE);							// Set the Datalink Type
    bacnet_datalink_init(BACNET_INTERFACE);							// Set the Interface Type
    atexit(bacnet_datalink_cleanup);								// Run bacnet_datalink_cleanup when program is terminated
    memset(&src, 0, sizeof(src));								// Fill src memory with 0

    register_with_bbmd();									// Register on the BBMD (TTL is 60)

    bacnet_Send_I_Am(bacnet_Handler_Transmit_Buffer);						// Send out I AM

    pthread_create(&minute_tick_id, 0, minute_tick, NULL);					// Create the minute thread
    pthread_create(&second_tick_id, 0, second_tick, NULL);					// Create the second thread
    
    /* Start another thread here to retrieve your allocated registers from the
     * modbus server. This thread should have the following structure (in a
     * separate function):
     *
     * Initialise:
     *	    Connect to the modbus server
     *
     * Loop:
     *	    Read the required number of registers from the modbus server
     *	    Store the register data into the tail of a linked list 
     */

    while (1) {
	pdu_len = bacnet_datalink_receive(							// Set Packet Data Unit Length
		    &src, rx_buf, bacnet_MAX_MPDU, BACNET_SELECT_TIMEOUT_MS);

	if (pdu_len) {										// If Packet Data has a Length
	    pthread_mutex_lock(&timer_lock);							// Take the timer thread lock
	    bacnet_npdu_handler(&src, rx_buf, pdu_len);						// Handle Messaging
	    pthread_mutex_unlock(&timer_lock);							// Release Lock
	}

	ms_tick();										// Run the millisecond tick function
    }

    return 0;											// Return 0
}
