// Standalone Modbus Request
// Help from http://libmodbus.org/docs/v3.0.6/modbus_read_registers.html

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <modbus-tcp.h>

#define MODBUS_SERVER_ADR	"127.0.0.1" 				// Modbus server location (local)
#define MODBUS_SERVER_PORT	502					// Modbus port  
#define MODBUS_DEVICE_ID	34					// Modbus Device as Provided by Kim (12,34)
#define MODBUS_DEVICE_INST	3					// Modbus Device Instances as Provided by Kim

int main(void){
	int aquire,i;							// Integer for Aquiring data from modbus, counter value i (reuse)
	uint16_t  j[256];						// Buffer j for storing data, MODbus read wants 16 bit number
	modbus_t *ctx; 
	ctx = modbus_new_tcp(MODBUS_SERVER_ADR, MODBUS_SERVER_PORT);	// Create libmodbus content (First Input uses IPv4 Address)
	if (ctx == NULL)						// If ctx if empty
	{
		fprintf(stderr, "MODbus Could Not be Created\n");	// Error message if unable to execute command
		return -1;						// Return a value of -1 (Error)
 	}
	
	printf("MODbus was Created\n");					// Print Success to STDOUT

	if (modbus_connect(ctx) == -1)					// If a connection could not be established
	{
		fprintf(stderr, "Connection not Established\n"); 	// Tell the user
		modbus_free(ctx);					// Release the IP Address and Port	
		return -1;						// Return a value of -1 (Error)
	}

	else								// For all other circumstances
	{
		printf("Connection has been Established\n");		// Tell User Connection has been Made
	}

	aquire=modbus_read_registers(ctx, 34, 4, j);			// Aquire will be negative if there is an error

	if (aquire<0)							// Aquired Values are less then 0
	{
		printf("Could not Pull Data From Server\n");		// Print Errror Message
		modbus_free(ctx);					// Release IP and Port
		return -1;						// Return -1 (Error)
	}

	//printf("Current I Value is %d \nCurrent Aquired Status is %d",i,aquire); //debug

	for (i = 0; i < aquire; i++)
	{
		printf("reg[%d]=%d (0x%X)\n", i, j[i], j[i]);
	}
	
	modbus_close(ctx);						//	Close Connection
	modbus_free(ctx);						//	Free IP and Port

}


