/************************************************************************/
/* zmqcoms.c								*/
/*                                                                      */
/* Serial port control routines.					*/
/************************************************************************/
/*                                                                      */
/*                    CHARA ARRAY USER INTERFACE			*/
/*                 Based on the SUSI User Interface			*/
/*		In turn based on the CHIP User interface		*/
/*                                                                      */
/*            Center for High Angular Resolution Astronomy              */
/*              Mount Wilson Observatory, CA 91001, USA			*/
/*                                                                      */
/* Telephone: 1-626-796-5405                                            */
/* Fax      : 1-626-796-6717                                            */
/* email    : theo@chara.gsu.edu                                        */
/* WWW      : http://www.chara.gsu.edu			                */
/*                                                                      */
/* (C) This source code and its associated executable                   */
/* program(s) are copyright.                                            */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Author : Tony Johnson & Theo ten Brummelaar                          */
/* Date   : Original Version 1990 - ported to Linux 1998                */
/************************************************************************/

/*
 * Include files
 */

#include "zmqcoms.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include <netdb.h>
#include <resolv.h>
#include <sys/socket.h>
#include <sys/time.h>
//#include <signal.h>
#include <netinet/tcp.h>

#define MAX_CONNECTIONS 10
#define SERVER_PORT 3003
#define SERVER_SOCKET_BUFFER 65536

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))


/* 
 * Local globals
 */

static void *context;
static void *responder;
static int rc;

static int	message_socket = -1;	/* socket used for outside messages */
static struct {
		int  fd;		/* File descriptor */     
		char *machine; 	/* Name of connecting machine */
			} client_sockets[MAX_CONNECTIONS]; /* Active channels */
static char	server_computer_name[81];	/* This machine's name */
static int	server_port;		/* The port we are listening too */
static int	curr_client = 0;		/* Current socket to check */


//static int (*(message_jobs[MAX_JOBS]))(struct smessage *message);

static int current_socket = -1;

//static unsigned char client_signals[MAX_CONNECTIONS][MAX_SIGNAL];

//static bool sent_signal_message = FALSE;

static FILE *server_log_fd = NULL;

/* Globals */

int client_socket = -1;                 /*
                                         * The current client socket.
                                         * Set to -1 if no client socket.
                                         */

char *client_machine;                   /*
                                         * The current server machine.
                                         * Set to 0 if no server machine.
                                         */

char	*server_name;			/* What we call ourselves */

//static bool Messages_on = TRUE; 	/* Should we process messages? */

int message(char *fmt, ...)
{
    char err_string[4097];
    int sprintf_len;
    va_list args;
    va_start(args,fmt);
    sprintf_len = vsprintf(err_string, fmt, args);
    fprintf(stderr, "%s\n", err_string);
    return NOERROR;
}

void send_raw_message(char *message,int len)
{
    if (client_socket != -1){
        zmq_send(responder,message,len,0);
    }
}

int error(int level, char *fmt, ...)
{
	char err_string[4097];
	int sprintf_len, i;
	va_list args;
	va_start(args, fmt);
	sprintf_len = vsprintf(err_string, fmt, args);
	if (client_socket==-1){
		fprintf(stderr, "%s\n", err_string);
	} else {
        zmq_send(responder,err_string,strlen(err_string),0);
        /* Error checking??? */
	}
	return NOERROR;
}

int get_next_command(char *command)
{
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	int nr;
    
	/* Start off with no command */
	command[0]=0;
    client_socket=-1;
   
	/* Set up our file descriptor set. Stdin is file descriptor number 0 */
	FD_ZERO(&fds);
    FD_SET(0, &fds);
	if (select(1, &fds, NULL, NULL, &tv) > 0){
		fgets(command, COMMAND_BUFFERSIZE, stdin);
        nr = strlen(command);
	} else {
        /* Receive from ZMQ */
        nr = zmq_recv (responder, command, COMMAND_BUFFERSIZE-1, ZMQ_DONTWAIT);
        if (nr>0)
            client_socket=0;
    }
    /* Null terminate and tidy up */
    command[nr]=0;
    if (command[nr-1] == '\n' || command[nr-1] == '\r') command[nr-1]=0;
	if (command[nr-2] == '\r') command[nr-2]=0;
	
	return NOERROR;
}

/************************************************************************/
/* open_server_socket()	               					*/
/* 									*/
/* Opens a message socket that can be used for accepting and sending	*/
/* outside messages. 							*/
/************************************************************************/

int open_server_socket(void)
{
    char tcp_string[40];

    /* Straight out of ZMQ tutorial*/
    context = zmq_ctx_new ();
    responder = zmq_socket (context, ZMQ_REP);
    sprintf(tcp_string, "tcp://*:%d", SERVER_PORT);
    rc = zmq_bind (responder, tcp_string);
    if (rc != 0)
        return error(FATAL,"ZMQ socket failure.");

	/* Reset current socket pointer. */
    client_socket=-1;

	return NOERROR;

} /* open_message_socket() */

/************************************************************************/
/* close_server_socket()						*/
/* 									*/
/* Close the server socket if it's open. Remember this could cause     */
/* trouble if you do this while the channel is server. If the message	*/
/* socket is not open this function will do nothing.			*/
/*									*/
/************************************************************************/

void close_server_socket(void)
{
	int i;

	/* Do nothing if it isn't open */
	if (responder == NULL) return;

	zmq_close(responder);

} /* close_message_socket() */

/************************************************************************/
/* open_serial_port()							*/
/* 									*/
/* Opens a serial port, returns and sets up serial structure.		*/
/* Second variable set's blocking or non-blocking. TRUE for blocking,	*/
/* FALSE for non-blocking.						*/
/* Structure will contain a copy of the termios data which you		*/
/* can manipulate with various functions like set_serial_baud_rate().	*/
/* The original termios structure is saved and can be restored. See	*/
/* close_serial_port().							*/
/* Returns the struct sserial structure.				*/
/* On error file discriptor set to -1.					*/
/* The user should also setup baud rates etc. 				*/
/************************************************************************/

struct sserial open_serial_port(char *port_name, bool blocking)
{
	struct sserial  port;	/* The return value */
	struct stat stat_buf;
	int	     lock_fd;
	char	     s[81];

	/* Construct the lock_file_name */

	lock_file_name(port_name, port.lock_file);

	/* Does this file exist already? */

	if (stat(port.lock_file, &stat_buf) == 0)
		error(MESSAGE, "Device %s seemed to be blocked, but I ignored it.",port_name);
/*	sprintf(s,"Device %s seems to be blocked.",port_name);
	if (stat(port.lock_file, &stat_buf) == 0 &&
		!ask_yes_no(s,"Shall we go on as if this is a stale block?"))
	{
		port.fd = -1;
		return port;
	}*/

	/* OK, better lock the file up */

	if ((lock_fd = open(port.lock_file,
		O_WRONLY | O_CREAT | 0x550)) == -1)
	{
		error(WARNING,
	"Could not create lock file %s (errno = %d).\nPort %s not opened.",
			port.lock_file, errno, port_name);
		port.fd = -1;
		return port;
	}
	chmod(port.lock_file, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP);
	close(lock_fd);

	/* Copy the name */

	strncpy(port.name,port_name,80);

	/* Try and open the port */

	if (blocking)
	{
		if ((port.fd = open(port_name,O_RDWR)) == -1)
		{
			port.fd = -1;
			unlink(port.lock_file);
			error(WARNING,"Could not open serial port %s.",
				port_name);
			return port;
		}
	}
	else 
	{
		if ((port.fd = open(port_name,O_RDWR | O_NONBLOCK)) == -1)
		{
			port.fd = -1;
			unlink(port.lock_file);
			error(WARNING,"Could not open serial port %s.",
				port_name);
			return port;
		}
	}
	
	/* Get the termios structure into orig_termios */

	if (tcgetattr(port.fd, &(port.orig_termios)) == -1)
	{
		error(WARNING,"Could not get termios for serial port %s.",
			port_name);
		close(port.fd);
		port.fd = -1;
		unlink(port.lock_file);
		return port;
	}
	
	/* Get the termios structure into curr_termios */

	if (tcgetattr(port.fd, &(port.curr_termios)) == -1)
	{
		error(WARNING,"Could not get termios for serial port %s.",
			port_name);
		close(port.fd);
		port.fd = -1;
		unlink(port.lock_file);
		return port;
	}
	
	/* Setinto raw, no echo mode, not canonical */

	port.curr_termios.c_iflag = IGNBRK;
	port.curr_termios.c_lflag = 0;
	port.curr_termios.c_oflag = 0;
	port.curr_termios.c_cflag |= (CLOCAL | CREAD);

	/* No hardware control */

	port.curr_termios.c_cflag &= (~CRTSCTS);

	/* Now send this stuff away to the port. */

	if (tcsetattr(port.fd, TCSADRAIN, &(port.curr_termios)) == -1)
	{
		error(WARNING,
			"Could not write termios for defaults port %s.",
			port.name);
		close(port.fd);
		port.fd = -1;
		unlink(port.lock_file);
		return port;
	}

	/* Return the structure */

	return port;

} /* open_serial_port() */

/************************************************************************/
/* lock_file_name()							*/
/*									*/
/* Constructs the name of a lock file. Puts it in the directory		*/
/* LOCK_DIR and uses LOCK_START and the last part of path name of file	*/
/* sent it to create the file name.					*/
/* Places name in string s, which is also the return value.		*/
/* The string's needs to have enough space to do this....		*/
/************************************************************************/

char *lock_file_name(char *path, char *s)
{
	char 	*pc;

	/* Find the last name in the path */

	for (pc = path+strlen(path); pc != path && *pc != '/'; pc--);
	if (pc != path) pc++;

	/* Now build the lock file name */

	sprintf(s,"%s/%s%s",LOCK_DIR,LOCK_START,pc);

	/* That is all */

	return s;

} /* lock_file_name() */

/************************************************************************/
/* close_serial_port()							*/
/* 									*/
/* Closes a serial port.						*/
/* The original termios structure is restored. 				*/
/* close_serial_port().							*/
/************************************************************************/

int	close_serial_port(struct sserial port)
{

	/* Put back the old termios data */

	if (tcsetattr(port.fd, TCSADRAIN, &(port.orig_termios)) == -1)
	{
		/* If we fail, let the user know, but go on anyway. */

		error(WARNING,"Could not write termios for serial port %s.",
			port.name);
	}

	/* Close the port */

	if (close(port.fd) == -1)
	{
		/* If we fail, let the user know, but go on anyway. */

		error(WARNING,"Could not close serial port %s.",port.name);
	}

	/* Remove the lock file */

	unlink(port.lock_file);

	/* Make sure the FD is set to -1 */

	port.fd = -1;

	return NOERROR;

} /* close_serial_port() */

/************************************************************************/
/* set_serial_baud_rate()						*/
/*									*/
/* Set's up the baud rate for a serial port.				*/
/* Returns NOERROR if all OK, error level otherwise.			*/
/* The speeds are defined in the termios.h header file, for example	*/
/* you could use B9600 for 9600 baud.					*/
/************************************************************************/

int set_serial_baud_rate(struct sserial *port, speed_t speed)
{
	/* Set up the speed in the current termios */

	if (cfsetospeed(&(port->curr_termios), speed) == -1)
	{
		error(WARNING,
			"Could not set output baud rate for serial port %s.",
			port->name);
		return WARNING;
	}

	if (cfsetispeed(&(port->curr_termios), speed) == -1) /* Same speed */
	{
		error(WARNING,
			"Could not set input baud rate for serial port %s.",
			port->name);
		return WARNING;
	}

	if (tcsetattr(port->fd, TCSADRAIN, &(port->curr_termios)) == -1)
	{
		error(WARNING,
			"Could not write termios for baud rate change port %s.",
			port->name);
		return WARNING;
	}

	return NOERROR;

} /* set_serial_baud_rate() */

/************************************************************************/
/* set_serial_parity()							*/
/*									*/
/* Set's a port's parity bit in it's termios.c_cflag on or off.		*/
/* Argument can be either OFF, EVEN or ODD.				*/
/* Returns NOERROR of all OK or error level otherwise.			*/
/************************************************************************/

int set_serial_parity(struct sserial *port, int parity)
{

	if (parity != ODD && parity != EVEN && parity != OFF)
	{
		error(WARNING,
		"Invalid argument to set_serial_parity(). No changes made.");
		return WARNING;
	}

	/* Make the appropriate change to the c_cflag entry */

	if (parity == OFF)
	{
		port->curr_termios.c_cflag &= (~PARENB);
	}
	else
	{
		port->curr_termios.c_cflag |= PARENB;

		if (parity == EVEN)
		{
			port->curr_termios.c_cflag &= (~PARODD);
		}
		else
		{
			port->curr_termios.c_cflag |= PARODD;
		}
	}

	/* Now write the changed termios to the port */

	if (tcsetattr(port->fd, TCSADRAIN, &(port->curr_termios)) == -1)
	{
		error(WARNING,
			"Could not write termios for parity change port %s.",
			port->name);
		return WARNING;
	}

	return NOERROR;

} /* set_serial_parity() */

/************************************************************************/
/* set_serial_xonxoff()							*/
/*									*/
/* Sets a port's xonxoff in it's termios.c_cflag on or off.		*/
/* Argument can be either OFF or OFF.					*/
/* Returns NOERROR of all OK or error level otherwise.			*/
/************************************************************************/

int set_serial_xonxoff(struct sserial *port, int xonxoff)
{
	if (xonxoff != OFF && xonxoff != ON)
	{
		error(WARNING,
		"Invalid argument to set_serial_xonxoff(). No changes made.");
		return WARNING;
	}

	/* Make the appropriate change to the c_iflag entry */

	if (xonxoff == ON)
	{
		port->curr_termios.c_iflag |= IXON;
		port->curr_termios.c_iflag |= IXOFF;
	}
	else
	{
		port->curr_termios.c_iflag &= (~IXON);
		port->curr_termios.c_iflag &= (~IXOFF);
	}

	/* Now write the changed termios to the port */

	if (tcsetattr(port->fd, TCSADRAIN, &(port->curr_termios)) == -1)
	{
		error(WARNING,
			"Could not write termios for xonxoff change port %s.",
			port->name);
		return WARNING;
	}

	return NOERROR;

} /* set_serial_xonxoff() */

/************************************************************************/
/* set_serial_hard_handshake()						*/
/*									*/
/* Turns hardware handshaking on or off.  				*/
/* Argument can be either OFF or OFF.					*/
/* Returns NOERROR of all OK or error level otherwise.			*/
/************************************************************************/

int set_serial_hard_handshake(struct sserial *port, int hard_handshake)
{
	if (hard_handshake != OFF && hard_handshake != ON)
	{
		error(WARNING,
	"Invalid argument to set_serial_hard_handshake(). No changes made.");
		return WARNING;
	}

	/* Make the appropriate change to the c_iflag entry */

	if (hard_handshake == ON)
	{
		port->curr_termios.c_cflag |= CRTSCTS;
		port->curr_termios.c_cflag &= (~CLOCAL);
	}
	else
	{
		port->curr_termios.c_cflag &= (~CRTSCTS);
		port->curr_termios.c_cflag |= CLOCAL;
	}

	/* Now write the changed termios to the port */

	if (tcsetattr(port->fd, TCSADRAIN, &(port->curr_termios)) == -1)
	{
		error(WARNING,
			"Could not write termios for handshake change port %s.",
			port->name);
		return WARNING;
	}

	return NOERROR;

} /* set_serial_hard_handshake() */

/************************************************************************/
/* set_serial_bitlength()						*/
/*									*/
/* Sets a port's bit length in it's termios.c_cflag.			*/
/* Argument can be either 5, 6, 7 or 8.					*/
/* Returns NOERROR of all OK or error level otherwise.			*/
/************************************************************************/

int set_serial_bitlength(struct sserial *port, int bits)
{
	int mask;

	/* Make the appropriate change to the c_iflag entry */

	switch(bits)
	{
		case 5: mask = CS5;
		        break;

		case 6: mask = CS6;
		        break;

		case 7: mask = CS7;
		        break;

		case 8: mask = CS8;
		        break;

		default: error(WARNING,
	"Invalid argument to set_serial_bitlength(). No changes made.");
			return WARNING;
	}

	port->curr_termios.c_cflag &= (~CSIZE);
	port->curr_termios.c_cflag |= mask;

	/* Now write the changed termios to the port */

	if (tcsetattr(port->fd, TCSADRAIN, &(port->curr_termios)) == -1)
	{
		error(WARNING,
		    "Could not write termios for bit length change port %s.",
		    port->name);
		return WARNING;
	}

	return NOERROR;

} /* set_serial_bitlength() */

/************************************************************************/
/* set_serial_stopbits()						*/
/*									*/
/* Sets a port's stop bits in it's termios.c_cflag.			*/
/* Argument can be either 1 or 2.					*/
/* Returns NOERROR of all OK or error level otherwise.			*/
/************************************************************************/

int set_serial_stopbits(struct sserial *port, int bits)
{
	/* Make the appropriate change to the c_iflag entry */

	switch(bits)
	{
		case 1: port->curr_termios.c_cflag &= (~CSTOPB);
		        break;

		case 2: port->curr_termios.c_cflag |= CSTOPB;
		        break;

		default: error(WARNING,
	"Invalid argument to set_serial_stopbits(). No changes made.");
			return WARNING;
	}

	/* Now write the changed termios to the port */

	if (tcsetattr(port->fd, TCSADRAIN, &(port->curr_termios)) == -1)
	{
		error(WARNING,
		    "Could not write termios for bit length change port %s.",
		    port->name);
		return WARNING;
	}

	return NOERROR;

} /* set_serial_stopbits() */

/************************************************************************/
/* serial_getchar()							*/
/*									*/
/* Reads a character from a serial port.				*/
/* Returns the character or -1 on error or 0 if there is no character.	*/
/************************************************************************/

unsigned char serial_getchar(struct sserial port)
{
	unsigned char c;

	if (read(port.fd,&c,1) != 1)
	{
		if (errno == EAGAIN)
		{
			return 0;
		}
		else
		{
			error(WARNING,"Error reading from port %s.\n",
				port.name);
			return -1;
		}
	}
	return c;

} /* serial_getchar() */

/************************************************************************/
/* serial_putchar()							*/
/*									*/
/* Writes a character to a serial port.					*/
/* Returns character sent or -1 on error.				*/
/************************************************************************/

unsigned char serial_putchar(struct sserial port, unsigned char c)
{
	if (write(port.fd,&c,1) != 1)
	{
		error(WARNING,"Error writing to port %s.\n",port.name);
		return -1;
	}
	return c;

} /* serial_putchar() */

/************************************************************************/
/* serial_print() 							*/
/*									*/
/* This is the equivalent of a printf command but sends the result	*/
/* to the given serial port. 						*/
/* Returns the number of characters printed.				*/
/************************************************************************/

int	serial_print(struct sserial port, char *fmt, ...)
{
	char	s[BUFFER_SIZE];
	va_list args;
	int	len;
	char	err_string[256];

	/* Check that the string is not too long */

        if (strlen(fmt) > sizeof(s) - sizeof((char)0))
        {
                strcpy(err_string,"Serial message too long\n");
                strcat(err_string,"Consult software supplier");
                error(FATAL,err_string);
        }

        /* Create the string */

        va_start(args, fmt);
        len = vsprintf(s, fmt, args);

        if (len > sizeof(s) - sizeof((char)0))
        {
                strcpy(err_string,"Serial message too long\n");
                strcat(err_string,"Consult software supplier");
                error(FATAL,err_string);
        }

	/* Write the message to the serial */

	if (write(port.fd, s, strlen(s)) != strlen(s))
	{
		error(WARNING,
			"Failed to write message to port %s.",port.name);
		return 0;
	}

	return len;

} /* serial_print() */

/************************************************************************/
/* serial_scan() 							*/
/*									*/
/* This is the equivalent of a scanf command but reads the data		*/
/* from the given serial. 						*/
/* Returns the number of items actually read.				*/
/************************************************************************/

int	serial_scan(struct sserial port, char *fmt, ...)
{
	char	s[BUFFER_SIZE];
	va_list args;

	/* Get a line of text from the serial. */

	if (serial_gets(port, s, BUFFER_SIZE) == (char *) EOF) return 0;

        /* Create the string */

        va_start(args, fmt);
        return vsscanf(s, fmt, args);

} /* serial_scan() */

/************************************************************************/
/* serial_gets() 							*/
/* 									*/
/* Equivalent to the normal gets() function, this function tries	*/
/* to get a string from the given serial. The return character (if	*/
/* there is one) is removed. The return value is a pointer to the 	*/
/* sent as an argument or EOF on end of file or error.			*/
/* In nonblocking mode, the string is empty if there was nothing	*/
/* to get.								*/
/* Will read up to n-1 characters leaving room for null at end.		*/
/************************************************************************/

char *serial_gets(struct sserial port, char *s, int n)
{

	int	len;

	/* Get the line of text */

	if ((len = read(port.fd, s, n-1)) <= 0) 
	{
		if (errno == EAGAIN)
		{
			*s = 0;
			return s;
		}
		else
		{
			return (char *)EOF;
		}
	}

	/* Terminate the string */

	s[len] = 0;

	/* Remove the final return character */

	if (s[len-1] == '\n') s[len-1] = 0;

	return s;

} /* serial_gets() */


