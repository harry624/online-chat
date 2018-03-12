/**
 * @ubitname_assignment1
 * @author  hao wang <hwang67@buffalo.edu> yue wan <ywan3@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>


#include "../include/global.h"
#include "../include/logger.h"
#include "../include/client.h"
#include "../include/server.h"

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */

int main(int argc, char **argv)
{
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/*Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));

	/*Start Here*/
	//judge whether it start as a server or client
	char* server = "s";
	char* client = "c";

	//port number out of boundary
	if ((atoi(argv[2]) > 65535) && (atoi(argv[2]) < 1024)){
		cse4589_print_and_log("[%s:ERROR]\n", argv[2]);
		return 0;
	}

	if (strcmp(argv[1], server) == 0){
		//run as server
		if (runAsServer(argv[2]) < 0){
			return 0;
		}

	}else if (strcmp(argv[1], client) == 0){
		//run as client
		if (runAsClient(argv[2]) < 0){
			return 0;
		}
	}else{
		cse4589_print_and_log("[%s:ERROR]\n", argv[1]);
		exit(1);
	}
	return 0;
}
