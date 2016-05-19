// Tyson Skiba 2016
// BACnet Server with MODbus Client | ProjectPart5

#include <stdio.h>									// Standard Input Output Library
#include <libbacnet/address.h>								// BACnet Libraries
#include <libbacnet/device.h>
#include <libbacnet/handlers.h>
#include <libbacnet/datalink.h>
#include <libbacnet/bvlc.h>
#include <libbacnet/client.h>
#include <libbacnet/txbuf.h>
#include <libbacnet/tsm.h>
#include <libbacnet/ai.h>
#include "bacnet_namespace.h"

#include <stdlib.h>									// Standard Library
#include <sys/socket.h>									// MODbus Libraries
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <modbus-tcp.h>

#include "list.h"									// L.LIST Linked List Library

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
#define MODBUS_DEVICE_ID	34							// Modbus Device as Provided by Kim (12 testing,34 ME)
#define MODBUS_DEVICE_INST	3							// Modbus Device Instances as Provided by Kim

struct linked_list									// L.LIST Structure to Hold List Elements
{											// L.LIST Structure with 2 members
  struct list_head list;								// L.LIST Pointer to the Head
  int   data_value;    									// L.LIST Object Data 
};

struct linked_list ll;									// L.LIST Create Initial Object
struct linked_list* ll_current_item; 							// L.LIST Create Pointer to the Current Item

static void ll_free_item(struct linked_list* item,struct linked_list ll_list){		// L.LIST Function to Delete Element
    item = list_entry(ll_list.list.next,struct linked_list,list);			// Get the target element
    list_del(&item->list);								// Remove using this function
    free(item);										// Free memory
}

static uint16_t test_data[] = {								// Define a static test data array
    0xA4EC, 0x6E39, 0x8740, 0x1065, 0x9134, 0xFC8C };					// Test data to provide to client
#define NUM_TEST_DATA (sizeof(test_data)/sizeof(test_data[0]))

static pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;				// Timer Lock
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;				// List Lock
static pthread_cond_t list_data_ready = PTHREAD_COND_INITIALIZER;			// Condition Signaling Data Avaliable

static void ll_add_to_tail(struct linked_list* ptr, int data_value)  			// L.LIST Function to Add Data to Base of List          
{
   struct linked_list* tmp;								// Create a new Structure Called tmp
   tmp = (struct linked_list*)malloc(sizeof(struct linked_list));			// Allocate Memory, retrun a value into tmp
   if(!tmp) {										// If no vale in tmp no memory could be allocated
     printf("Memory Could Not be Alocated, Aborting.\n");				// Print an Error Message
     exit(1);										// Exit
   }
   tmp->data_value = data_value;							// The Element is now the value of the data_value input
   pthread_mutex_lock(&list_lock);							// Lock Thread to prevent data corruption
   list_add_tail( &(tmp->list), &(ptr->list) );						// Add data to bottom of the list
   pthread_mutex_unlock(&list_lock);							// Unlock thread
   pthread_cond_signal(&list_data_ready);						// Signal that Data is avaliable
}

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
     * Without reconfiguring libbacnet, a maximum of 4 values may be sent */ //////////////////////////////////DO I ITERATE THROUGH A LOOP HERE FOR 1,2,3 or 4 Instances????????????
    
    
    pthread_mutex_lock(&list_lock);							// L.LIST Lock Thread to prevent possible modification while read
    if(!list_empty(&ll.list)){								// L.LIST Check for Avaliable Data
	list_for_each_entry(ll_current_item,&ll.list,list) 				//L.LIST Get Data From Current Object
	{
           bacnet_Analog_Input_Present_Value_Set(instance_no, ll_current_item->data_value); // BACnet Send Over Bacnet
	}
        ll_free_item(ll_current_item,ll);						// Free the current Item					
    }

    pthread_mutex_unlock(&list_lock);							// L.LIST Unlock the Thread

    
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
	while(1){										// MODbus Everloop
initialise:											// Label for goto
		ctx = modbus_new_tcp(MODBUS_SERVER_ADR, MODBUS_SERVER_PORT);			// Create libmodbus content (First Input uses IPv4 Address)
		if (ctx == NULL)								// If ctx if empty
		{
			fprintf(stderr, "MODbus Could Not be Created\n");			// Error message if unable to execute command
			modbus_free(ctx);							// Free Modbus Open Ports and IP
			modbus_close(ctx);							// Close Modbus
			sleep(1);								// Do Nothing for 1 second
			goto initialise;							// Return to the label 'initialise'
 		}
	
		printf("MODbus was Created\n");							// Print Success to STDOUT

		if (modbus_connect(ctx) == -1)							// If a connection could not be established
		{				
			fprintf(stderr, "Connection not Established\n"); 			// Tell the user
			modbus_free(ctx);							// Free Modbus Open Ports and IP
			modbus_close(ctx);							// Close Modbus
			sleep(1);								// Do Nothing for 1 second
			goto initialise;							// Return to the label 'initialise'
		}

		else										// For all other circumstances
		{
			printf("Connection has been Established\n");				// Tell User Connection has been Made
		}

		aquire=modbus_read_registers(ctx, 34, 4, j);					// Aquire will be negative if there is an error

		if (aquire<0)									// Aquired Values are less then 0
		{
			printf("Could not Pull Data From Server\n");				// Print Errror Message
			modbus_free(ctx);							// Free Modbus Open Ports and IP
			modbus_close(ctx);							// Close Modbus
			goto initialise;							// Return to the label 'initialise'
		}

	//printf("Current I Value is %d \nCurrent Aquired Status is %d",i,aquire); //debug

		for (i = 0; i < aquire; i++)
		{
			ll_add_to_tail(&ll, j[i]);
			printf("reg[%d]=%d (0x%X)\n", i, j[i], j[i]);
		}

	
		modbus_close(ctx);								// Close Connection
		modbus_free(ctx);								// Free IP and Port
		sleep(1);									// Sleep for 100 ms on Final Project 
	}
	return NULL;										// Nothing to return
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
    uint8_t rx_buf[bacnet_MAX_MPDU];								// BACnet Initialise recieved data buffer
    uint16_t pdu_len;										// BACnet Initialise pdu length
    BACNET_ADDRESS src;
    pthread_t minute_tick_id, second_tick_id;							// BACnet Declare Threads
    pthread_t MODBUS_client_thread;								// MODbus Thread

    bacnet_Device_Set_Object_Instance_Number(BACNET_INSTANCE_NO);				// BACnet Set the Instance Number
    bacnet_address_init();									// BACnet Initialise BACnet

    bacnet_Device_Init(server_objects);								// BACnet Initialise BACnet Objects
    BN_UNC(WHO_IS, who_is);									// BACnet Populate Object Field who is
    BN_CON(READ_PROPERTY, read_property);							// BACnet Populate Object Field read property

    bacnet_BIP_Debug = true;									// BACnet Enable BACnet IP Debugging
    bacnet_bip_set_port(htons(BACNET_PORT));							// BACnet Convert to Network Style IP and Set IP 
    bacnet_datalink_set(BACNET_DATALINK_TYPE);							// BACnet Set the Datalink Type
    bacnet_datalink_init(BACNET_INTERFACE);							// BACnet Set the Interface Type
    atexit(bacnet_datalink_cleanup);								// BACnet Run bacnet_datalink_cleanup when program is terminated // Put LList Clean Here
    memset(&src, 0, sizeof(src));								// BACnet Fill src memory with 0

    register_with_bbmd();									// BACnet Register on the BBMD (TTL is 60)

    bacnet_Send_I_Am(bacnet_Handler_Transmit_Buffer);						// BACnet Send out I AM

    pthread_create(&minute_tick_id, 0, minute_tick, NULL);					// BACnet Create the minute thread
    pthread_create(&second_tick_id, 0, second_tick, NULL);					// BACnet Create the second thread
    pthread_create(&MODBUS_client_thread,0, MODBUS_client, NULL);				// MODbus Create its Thread

  
    INIT_LIST_HEAD(&ll.list);									// L.LIST Initialise the List

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


