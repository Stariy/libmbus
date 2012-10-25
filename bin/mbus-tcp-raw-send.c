//------------------------------------------------------------------------------
// Copyright (C) 2012, Robert Johansson, Raditex AB
// All rights reserved.
//
// rSCADA 
// http://www.rSCADA.se
// info@rscada.se
//
//------------------------------------------------------------------------------

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <mbus/mbus.h>

static int debug = 0;

//------------------------------------------------------------------------------
// Execution starts here:
//------------------------------------------------------------------------------
int
main(int argc, char **argv)
{
    mbus_frame reply, request;
    mbus_frame_data reply_data;
    mbus_handle *handle = NULL;

    char *host, *addr_str, matching_addr[16], *file = NULL;
    int port, address, fd, len, i, result;
  	u_char raw_buff[4096], buff[4096], *ptr, *endptr;
 
    memset((void *)&reply, 0, sizeof(mbus_frame));
    memset((void *)&reply_data, 0, sizeof(mbus_frame_data));
     
    if (argc == 4)
    {
        host = argv[1];   
        port = atoi(argv[2]);
        addr_str = argv[3];
        debug = 0;
    }
    else if (argc == 5 && strcmp(argv[1], "-d") == 0)
    {
        host = argv[2];   
        port = atoi(argv[3]);
        addr_str = argv[4];
        debug = 1;
    }
    else if (argc == 5)
    {
        host = argv[1];   
        port = atoi(argv[2]);
        addr_str = argv[3];
        file = argv[4];
        debug = 0;
    }    
    else if (argc == 6 && strcmp(argv[1], "-d") == 0)
    {
        host = argv[2];   
        port = atoi(argv[3]);
        addr_str = argv[4];
        file = argv[5];
        debug = 1;
    }
    else
    {
        fprintf(stderr, "usage: %s [-d] host port mbus-address [file]\n", argv[0]);
        fprintf(stderr, "    optional flag -d for debug printout\n");
        fprintf(stderr, "    optional argument file for file. if omitted read from stdin\n");
        return 0;
    }
    
    if (debug)
    {
        mbus_register_send_event(&mbus_dump_send_event);
        mbus_register_recv_event(&mbus_dump_recv_event);
    }
    
    if ((handle = mbus_context_tcp(host, port)) == NULL)
    {
        fprintf(stderr, "Could not initialize M-Bus context: %s\n",  mbus_error_str());
        return 1;
    }

    if (mbus_connect(handle) == -1)
    {
        fprintf(stderr, "Failed to setup connection to M-bus gateway\n");
        return 1;
    }

    if (strlen(addr_str) == 16)
    {
        // secondary addressing
        int ret;

        ret = mbus_select_secondary_address(handle, addr_str);

        if (ret == MBUS_PROBE_COLLISION)
        {
            fprintf(stderr, "%s: Error: The address mask [%s] matches more than one device.\n", __PRETTY_FUNCTION__, addr_str);
            return 1;
        }
        else if (ret == MBUS_PROBE_NOTHING)
        {
            fprintf(stderr, "%s: Error: The selected secondary address does not match any device [%s].\n", __PRETTY_FUNCTION__, addr_str);
            return 1;
        }
        else if (ret == MBUS_PROBE_ERROR)
        {
            fprintf(stderr, "%s: Error: Failed to select secondary address [%s].\n", __PRETTY_FUNCTION__, addr_str);
            return 1;    
        }
        address = 253;
    } 
    else
    {
        // primary addressing
        address = atoi(addr_str);
    }   
    
    //
    // read hex data from file or stdin
    //
    if (file != NULL)
    {
      	if ((fd = open(file, O_RDONLY, 0)) == -1)
        {
	    	fprintf(stderr, "%s: failed to open '%s'\n", __PRETTY_FUNCTION__, file);
            return 1;
        }
    }
    else
    {
        fd = 0; // stdin
    }

	memset(raw_buff, 0, sizeof(raw_buff));
	len = read(fd, raw_buff, sizeof(raw_buff));
	close(fd);

    ptr = 0;
    endptr = raw_buff;
    for (i = 0; i < sizeof(buff)-1; i++)
    {
        ptr = endptr;
        buff[i] = (u_char)strtol(ptr, (char **)&endptr, 16);
        
        // abort at non hex value 
        if (ptr == endptr)
            break;
    }
    
    //
    // attempt to parse the input data
    //
  	result = mbus_parse(&request, buff, i);
    
	if (result < 0)
	{
	    fprintf(stderr, "mbus_parse: %s\n", mbus_error_str());
        return 1;
    }
    else if (result > 0)
    {
        fprintf(stderr, "mbus_parse: need %d more bytes\n", result);
        return 1;
    }    
    
    //
    // send the request
    //
    if (mbus_send_frame(handle, &request) == -1)
    {
        fprintf(stderr, "Failed to send mbus frame: %s\n", mbus_error_str());
        return 1;
    }    
       
    //
    // this should be option:
    //
    if (mbus_recv_frame(handle, &reply) != MBUS_RECV_RESULT_OK)
    {
        fprintf(stderr, "Failed to receive M-Bus response frame: %s\n", mbus_error_str());
        return 1;
    }

    mbus_disconnect(handle);
    mbus_context_free(handle);
    return 0;
}


