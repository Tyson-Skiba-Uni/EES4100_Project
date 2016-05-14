// Tyson Skiba 2016
// BACnet Server with MODbus Client | ProjectPart3
// Based Heavily Upon https://github.com/kmtaylor/EES4100_Testbench/blob/master/src/bacnet_server.c
// Based Heavily Upon http://libmodbus.org/docs/v3.0.6/modbus_read_registers.html

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

#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <modbus-tcp.h>

//BACnet Define
											
#define BACNET_INSTANCE_NO	    34							// BACnet Provided by University
#define BACNET_PORT		    0xBAC1						// BACnet Port 1
#define BACNET_INTERFACE	    "lo"						// BACnet Local Interface
#define BACNET_DATALINK_TYPE	    "bvlc"						// BACnet Layer Type
#define BACNET_SELECT_TIMEOUT_MS    1	    						// BACnet 1 milliseond timeout

#define RUN_AS_BBMD_CLIENT	    1							// BACnet Run as BBMD Client (Bool)

#if RUN_AS_BBMD_CLIENT									 
#define BACNET_BBMD_PORT	    0xBAC0						// BACnet Port 0
#define BACNET_BBMD_ADDRESS	    "127.0.0.1"						// BACnet Broadcast Table IP Address is Local Host
#define BACNET_BBMD_TTL		    90							// BACnet Time to Live set to 90 seconds
#endif

//MODbus Define

#define MODBUS_SERVER_ADR	"127.0.0.1" 						// Modbus server location (local)
#define MODBUS_SERVER_PORT	502							// Modbus port  
#define MODBUS_DEVICE_ID	12							// Modbus Device as Provided by Kim (12,34)
#define MODBUS_DEVICE_INST	3							// Modbus Device Instances as Provided by Kim

struct list_object_s {									// L.LIST Structure with 3 members
    char *string;                   							// L.LIST String Pointer
    int strlen;                     							// L.LIST String Length 
    struct list_object_s *next;     							// L.LIST Dynamic Pointer to Next List Item
};


static uint16_t test_data[] = {								// Define a static test data array
    0xA4EC, 0x6E39, 0x8740, 0x1065, 0x9134, 0xFC8C };					// Test data to provide to client
#define NUM_TEST_DATA (sizeof(test_data)/sizeof(test_data[0]))

static struct list_object_s *list_head;							// L.LIST Global Structure pointing to list head
static pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;				// Initialise PThread
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;				// 
static pthread_cond_t list_data_ready = PTHREAD_COND_INITIALIZER;			// 
static pthread_cond_t list_data_flush = PTHREAD_COND_INITIALIZER;			// 

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

void *MODBUS_client(void *arg) {								// MODbus Thread Function
	int aquire,i;										// Integer for Aquiring data from modbus, counter value i (reuse)
	uint16_t  j[256];									// Buffer j for storing data, MODbus read wants 16 bit number
	modbus_t *ctx; 

	reinit:											// No Error Control for Modbus so must be implemented, GoTo Loop
		ctx = modbus_new_tcp(MODBUS_SERVER_ADR, MODBUS_SERVER_PORT);			// Create libmodbus content (First Input uses IPv4 Address)

	
		if (ctx == NULL)								// If ctx if empty
		{
			fprintf(stderr, "MODbus Could Not be Created\n");			// Error message if unable to execute command
			modbus_free(ctx); 							// Free MODbus IP and Port 
	      		modbus_close(ctx);							// Close MODbus
			sleep(1);								// Stop this thread for 1 second
			goto reinit;								// GoTo the reinit point
		}
		else
		{
			printf("MODbus was Created\n");						// Print Success to STDOUT
		}

		if (modbus_connect(ctx) == -1)							// If a connection could not be established
		{					
			fprintf(stderr, "Connection not Established\n"); 			// Tell the user
			modbus_free(ctx);							// Release the IP Address and Port	
			modbus_free(ctx); 							// Free MODbus IP and Port 
	      		modbus_close(ctx);							// Close MODbus
			sleep(1);								// Stop this thread for 1 second
			goto reinit;								// GoTo the reinit point
		}

		else										// For all other circumstances
		{
			printf("Connection has been Established\n");				// Tell User Connection has been Made
		}

	aquire=modbus_read_registers(ctx, 34, 4, j);						// Aquire will be negative if there is an error

	if (aquire<0)										// Aquired Values are less then 0
	{
		printf("Could not Pull Data From Server\n");					// Print Errror Message
		modbus_free(ctx);								// Release IP and Port
		return arg;									// Return -1 (Error)
	}
	
	//printf("Current I Value is %d \nCurrent Aquired Status is %d",i,aquire); //debug

	for (i = 0; i < aquire; i++)
	{
		printf("reg[%d]=%d (0x%X)\n", i, j[i], j[i]);
		// Try to Add Data into the Arese EndList Here
	}
	
	modbus_close(ctx);									// Close Connection
	modbus_free(ctx);									// Free IP and Port
	return arg;										// Has to return something arg will work
}

static void *minute_tick(void *arg) {								// Minute Thread Function
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

static void *second_tick(void *arg) {								// Second Thread Function
    while (1) {											// Ever Loop, Keeps running
	pthread_mutex_lock(&timer_lock);							// Take the timer thread lock
	bacnet_bvlc_maintenance_timer(1);							// Invalidates stale BBMD foreign device table entries 
	bacnet_tsm_timer_milliseconds(1000);							// Transmission State Machine control Acknowlegement and ReTransmissions

	pthread_mutex_unlock(&timer_lock);							// Release Lock
	sleep(1);										// Sleep for 1 Second
    }
    return arg;
}

static void ms_tick(void) {									
}

#define BN_UNC(service, handler) \
    bacnet_apdu_set_unconfirmed_handler(		\
		    SERVICE_UNCONFIRMED_##service,	\
		    bacnet_handler_##handler)
#define BN_CON(service, handler) \
    bacnet_apdu_set_confirmed_handler(			\
		    SERVICE_CONFIRMED_##service,	\
		    bacnet_handler_##handler)

static void add_to_list(char *input) {								// Input is a Charecter Pointer
    struct list_object_s *last_item;								// Point to the last item
    struct list_object_s *new_item = malloc(sizeof(struct list_object_s));			// Alloc Memory for the Size of the Structure
    if (!new_item) {										// If there are no new items
        fprintf(stderr, "Memory Alocation has Failed, Aborting\n");				// Print Memory Aloocation Error to Standard Error
        exit(1);										// Exit the Application
    }
												// -> is member of a structure
    new_item->string = strdup(input);								// place value of input into new_item.string
    new_item->strlen = strlen(input);								// place value of input length into new_item.strlen
    new_item->next = NULL;

    pthread_mutex_lock(&list_lock);								// Take the lock to protect from corruption as data is about to be modified

    if (list_head == NULL) {									// If no list head exists (first run)...
        list_head = new_item;									// ...Create an Object
    } else {											// For Every Other Possible Condition ie Data Exists...
        last_item = list_head;									// Move the pointer to the current item (list head)
        while (last_item->next) last_item = last_item->next;
        last_item->next = new_item;								// Make the new item the bottom of the list
    }

    pthread_cond_signal(&list_data_ready);							// Tell the function that Data can now be free'd
    pthread_mutex_unlock(&list_lock);								// Release Thread Lock
}	

static struct list_object_s *list_get_first(void) {				
    struct list_object_s *first_item;								// Define struture with one element						

    first_item = list_head;									// Make the top item in the list the list head pointer
    list_head = list_head->next;								// Make the first item the next value

    return first_item;										// Return the first_item value
}

int main(int argc, char **argv) {								// Main Function, Execution will begin here 
    uint8_t rx_buf[bacnet_MAX_MPDU];								// BACnet Initialise recieved data buffer
    uint16_t pdu_len;										// BACnet Initialise pdu length
    BACNET_ADDRESS src;
    pthread_t minute_tick_id, second_tick_id;							// BACnet Declare Threads
    pthread_t MODBUS_client_thread;									// MODbus Thread

    bacnet_Device_Set_Object_Instance_Number(BACNET_INSTANCE_NO);				// BACnet Set the Instance Number
    bacnet_address_init();									// BACnet Initialise BACnet

    bacnet_Device_Init(server_objects);								// BACnet Initialise BACnet Objects
    BN_UNC(WHO_IS, who_is);									// BACnet Populate Object Field who is
    BN_CON(READ_PROPERTY, read_property);							// BACnet Populate Object Field read property

    bacnet_BIP_Debug = true;									// BACnet Enable BACnet IP Debugging
    bacnet_bip_set_port(htons(BACNET_PORT));							// BACnet Convert to Network Style IP and Set IP 
    bacnet_datalink_set(BACNET_DATALINK_TYPE);							// BACnet Set the Datalink Type
    bacnet_datalink_init(BACNET_INTERFACE);							// BACnet Set the Interface Type
    atexit(bacnet_datalink_cleanup);								// BACnet Run bacnet_datalink_cleanup when program is terminated
    memset(&src, 0, sizeof(src));								// BACnet Fill src memory with 0

    register_with_bbmd();									// BACnet Register on the BBMD (TTL is 60)

    bacnet_Send_I_Am(bacnet_Handler_Transmit_Buffer);						// BACnet Send out I AM

    pthread_create(&minute_tick_id, 0, minute_tick, NULL);					// BACnet Create the minute thread
    pthread_create(&second_tick_id, 0, second_tick, NULL);					// BACnet Create the second thread
    pthread_create(&MODBUS_client_thread,0, MODBUS_client, NULL);				// MODbus Create its Thread
    
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
	MODBUS_client(NULL);									// Keep Running Modbus Thread
    }

    return 0;											// Return 0
}
