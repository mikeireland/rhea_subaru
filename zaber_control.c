
/************************************************************************/
/* zaber_control.c                                                      */
/*                                                                      */
/* Routines to control the Zaber's chainable stepper motors.		*/
/************************************************************************/
/*                                                                      */
/* Author : Mike Ireland                                               	*/
/* Date   : July 2015                                                   */
/************************************************************************/

#include "zaber.h" 
#include "uicoms.h"
#include <sys/time.h>

#define ZABER_TIME_OUT	10
#define IFU_X 1
#define IFU_Y 2
char *zaber_names[NUM_ZABER+1] = {"IFU_X", "IFU_Y", "FOCUS",NULL};

/* Globals declared in zaber.h */
int z_current_position[NUM_ZABER+1];
int z_saved_positions[NUM_ZABER+1][NUM_ZABER_FIXED];
int z_current_fixed_position[NUM_ZABER+1];

/* "local globals" */
struct sserial zaber_port;

/************************************************************************/
/* open_zaber_port()		         				*/
/*									*/
/* Opens the serial port for talking to a Zaber chain of stepper motors.*/
/* Returns the serial port structure. If it fails to open the port	*/
/* the port.fd is set to -1.						*/
/* Use close_zaber_port to close the port.         			*/
/************************************************************************/

int open_zaber_port(char *port_name)
{
	struct sserial port;
	char c;
	//	int i;
	
	zaber_port.fd=-1;
	/* Try and open the port in nonblocking mode */
	
	port = open_serial_port(port_name, FALSE);
	
	if (port.fd == -1) return error(WARNING, "Could not open serial port.");

	if (set_serial_baud_rate(&port, B9600) != NOERROR)
	{
		close_serial_port(port);
		port.fd = -1;
		return error(WARNING, "Could not set baud rate.");
	}

        if (set_serial_parity(&port, OFF) != NOERROR)
	{
		close_serial_port(port);
		port.fd = -1;
		return error(WARNING, "Could not set serial port parity.");
	}

        if (set_serial_bitlength(&port, 8) != NOERROR)
	{
		close_serial_port(port);
		port.fd = -1;
		return error(WARNING, "Could not set serial bitlength.");
	}

        if (set_serial_stopbits(&port, 1) != NOERROR)
	{
		close_serial_port(port);
		port.fd = -1;
		return error(WARNING, "Could not set serial stopbits.");
	}

	/* No handshaking */

        if (set_serial_xonxoff(&port, OFF) != NOERROR)
	{
		close_serial_port(port);
		port.fd = -1;
		return error(WARNING, "Could not set serial xonxoff.");
	}

        if (set_serial_hard_handshake(&port, OFF) != NOERROR)
	{
		close_serial_port(port);
		port.fd = -1;
		return error(WARNING, "Could not set serial handshake.");;
	}

	/* remove any characters there may be in the buffer */

	while(read(port.fd, &c, 1) == 1);

	/* Now lets set the zaber_port variable */
	zaber_port=port;

	/* Assuming that the Zaber motors have stayed powered-up, lets read
	 all of the positions. */
	//	for (i=1;i<=NUM_ZABER;i++)
	//  z_current_position[i] = zaber_get_position(i);

	return NOERROR;

} /* open_zaber_port() */

/************************************************************************/
/* zaber_read_position_file                                             */
/*                                                                      */
/* This function reads the Zaber position file. If no filenme is given, */
/* the  ZABER_POSITION_FILE is used                                     */
/************************************************************************/
int cmd_zreadposfile(int argc, char* argv[])
{
  int i;
  char fname[80], s[80];
  FILE *zaber_file;
  /* This is a bit of a kludge...*/
  if (NUM_ZABER_FIXED != 4) return error(ERROR, "NUM_ZABER_FIXED has changed. Please re-write code.");
  if (argc < 2) strcpy(fname, ZABER_POSITION_FILE);
  else strcpy(fname, argv[1]);
  if ( (zaber_file = fopen(fname,"r")) == NULL ) 
    return error(ERROR, "Could not open pavo Zaber file: %s.", fname);
  for (i=1;i<=NUM_ZABER;i++){
    if (fgets(s, 80, zaber_file) == NULL) return error(ERROR, "Could not read all lines in Zaber file.");
    if (sscanf(s, "%d %d %d %d",  &(z_saved_positions[i][0]), &(z_saved_positions[i][1]), 
	       &(z_saved_positions[i][2]), &(z_saved_positions[i][3])) < 1)
      return error(ERROR, "Could not parse Zaber file.");
  }
  fclose(zaber_file);
  return NOERROR;
} /* zaber_read_position_file() */

/************************************************************************/
/* zaber_write_position_file                                            */
/*                                                                      */
/* This function updates the current z_saved_positions and writes       */
/* z_saved_positions to file. I tried using system() to rename the file */
/* but this didn't work...                                              */ 
/************************************************************************/
int cmd_zwriteposfile(int argc, char* argv[])
{
  int i;
  char fname[200], s[200];
  FILE *zaber_file;
  /* This is a bit of a kludge...*/
  if (NUM_ZABER_FIXED != 4) return error(ERROR, "NUM_ZABER_FIXED has changed. Please re-write code.");
  if (argc < 2) strcpy(fname, ZABER_POSITION_FILE);
  else strcpy(fname, argv[1]);
  /* Firstly, update z_saved_positions */
  for (i=1;i<=NUM_ZABER;i++)
    z_saved_positions[i][z_current_fixed_position[i]] = z_current_position[i];
  /* Rename the file. */
  sprintf(s, "mv %s %s.`date +%%y%%m%%d`",fname, fname);
  system(s);
  /* Open the file for writing */
  fprintf(stderr, "%s\n", fname);
  
  if ( (zaber_file = fopen(fname,"w")) == NULL ) 
    return error(ERROR, "Could not open pavo Zaber file: %s.", fname);
  for (i=1;i<=NUM_ZABER;i++)
    fprintf(zaber_file, "%d %d %d %d\n",  z_saved_positions[i][0], z_saved_positions[i][1], 
	    z_saved_positions[i][2], z_saved_positions[i][3]);
  if (fclose(zaber_file)) return error(ERROR, "Could not close the Zaber positions file!");

  return error(MESSAGE, "Zaber file written");
} /* zaber_write_position_file() */

/************************************************************************/
/* zaber_goto_fixed_position                                            */
/*                                                                      */
/* This function sends a zaber motor to a new fixed position, and in    */
/* doing so automatically over-writes z_saved_positions in memory. That */
/* is, unless the _same_ fixed position is selected. In that case the   */
/* motor reverts to the previously saved position.                      */
/************************************************************************/
int zaber_goto_fixed_position(unsigned char unit, int new_fixed)
{
  if (new_fixed < 0 || new_fixed >= NUM_ZABER_FIXED)
    return error(ERROR, "Fixed position between 0 and %d", NUM_ZABER_FIXED);
  if (unit == ZABER_ALL_UNITS)
    return error(ERROR, "Cannot goto fixed position for ZABER_ALL_UNITS.");
  /* So we can come back to where we are, update the saved_position */
  if (new_fixed != z_current_fixed_position[unit])
    z_saved_positions[unit][z_current_fixed_position[unit]] = z_current_position[unit];
  /* Now actually move the unit */
  zaber_move_abs(unit, z_saved_positions[unit][new_fixed]);
  /* ... and update where we are. */
  z_current_fixed_position[unit]=new_fixed;

  return NOERROR;
} /* zaber_goto_fixed_position() */

/************************************************************************/
/* cmd_zgotofixed                                                      */
/*                                                                      */
/* This is the user version of zaber_goto_fixed_position.               */
/************************************************************************/

int cmd_zgotofixed(int argc, char * argv[])
{
  int new_fixed;
  unsigned char unit;
  /* If only the motor number is given, we'll revert to the saved position */
  if (argc < 2) return error(ERROR, "Usage: zgoto [unit] [new_fixed]");
  sscanf(argv[1], "%hhu", &unit);
  if (argc == 2) {
    new_fixed=z_current_fixed_position[unit];
  } else if (argc == 3){
    sscanf(argv[2], "%d", &new_fixed);
  }
  return zaber_goto_fixed_position(unit, new_fixed);
} /* user_zaber_goto_fixed_position() */

/************************************************************************/
/* zaber_zero_all                                                       */
/*                                                                      */
/* This needs to be called whenever the power on the zaber units is     */
/* cycled. It uses the ZABER_POSITION_FILE to set all motors to         */
/* position 0.                                                          */
/************************************************************************/
int cmd_zzero(int argc, char* argv[])
{
  int i, postot=1, num_failures=0;
  if (argc > 1) return error(ERROR, "Usage: zzero");
  /* Starg by homing all of the motors*/
  zaber_home(0);
  message("All motors now homing. Waiting for them to be at 0...");
  while (postot != 0){
    sleep(1);
    postot=0;
    /* !!!! This next line is a bit of a hack as it assumes that the first half of
     the zaber motors have their home position at 0 !!!! */
    for (i=1;i<NUM_ZABER/2;i++) {
      z_current_position[i] = zaber_get_position(i);
      postot += z_current_position[i];
      usleep(20000);
    }
    if (num_failures > 20) return error(ERROR, "Could not home in time.");   
  }
  /* Now read in the positions */
  cmd_zreadposfile(0, NULL);
  /* Finally, move the motors one at a time. */
  for (i=1;i<=NUM_ZABER;i++){
    z_current_fixed_position[i]=0;
    usleep(200000); /* Hopefully more reliable? usleeps increased 14 Dec 2011 to help */
    zaber_move_abs(i, z_saved_positions[i][0]);
    message("Motor %d at fixed 0 position.", i);
  }
  return error(MESSAGE, "All motors homed!");
} /* zaber_zero_all */

/************************************************************************/
/* close_zaber_port()		         				*/
/*									*/
/* Closes the serial port for talking to a Zaber chain of stepper       */
/* motors                                                               */
/* 			                                                */
/************************************************************************/

int close_zaber_port(void)
{
  close_serial_port(zaber_port);
  return NOERROR;
} /* close_zaber_port */

/************************************************************************/
/* zaber_init()	              	         				*/
/*									*/
/* Finds the current position of all motors and sets the globals        */
/* appropriately                                                        */
/* 			                                                */
/************************************************************************/

int zaber_init(void)
{
  int i,j;
  struct zaber_message zm, answer; 
  if (zaber_port.fd==-1) return error(ERROR, "Zaber port not opened: cannot initialize.");
  for (i=1;i<=NUM_ZABER;i++){
  /* Set the current position to where the zaber stage thinks we are... */
    z_current_position[i]=zaber_get_position(i);
   /* Zero the current fixed position */
    z_current_fixed_position[i]=0;
    /* If we are at a current fixed position, then set that appropriately. */
    for (j=NUM_ZABER_FIXED-1;j>0;j--)
      if (abs(z_current_position[i] -z_saved_positions[i][j]) < 100)
	z_current_fixed_position[i]=j;
  }
  zm.unit    = 0;
  zm.type    = ZABER_SET_MODE;
  zm.data    = ZABER_ENABLE_ANTI_BACKLASH | ZABER_ENABLE_ANTI_STICKTION | ZABER_DISABLE_POTENTIOMETER | ZABER_DISABLE_POWER_LED | ZABER_DISABLE_SERIAL_LED;
  zaber_send_command(zm, &answer);
	
  return NOERROR;
} /* zaber_init */

/************************************************************************/
/* zaber_readchar()	              	                                */
/*									*/
/* Tries to read a character from the serial port. Returns TRUE if we   */
/* have a character, FALSE if there was a timeout.                      */
/* 			                                                */
/************************************************************************/
int zaber_readchar(unsigned char * achar, int timeout)
{
  struct timezone tz;
  struct timeval start, now;
  if (zaber_port.fd==-1) return FALSE;
  tz.tz_minuteswest=0;
  tz.tz_dsttime=0;
  gettimeofday(&start, &tz);
  while(read(zaber_port.fd, achar, 1) != 1){
    gettimeofday(&now, &tz);
    if (now.tv_usec - start.tv_usec + 
	1000000*(now.tv_sec - start.tv_sec) > 1000*timeout){
      error(ERROR, "Could not read from Zaber motor. \n Are all motors plugged in?");
      return FALSE;
    }
    usleep(1000);
  }
  return TRUE;
} /* zaber_readchar() */


/************************************************************************/
/* zaber_send_command()		         				*/
/*									*/
/* This is the heart of the communication with the stepper motors.      */
/* It casts the binary message and sends it to the serial port          */
/* 			                                                */
/************************************************************************/
	
unsigned char zaber_send_command(struct zaber_message zm,
	struct zaber_message * answer)
{
	char *c=NULL;
	unsigned char tmp;
	int  i, timeout=10000;
	if (zaber_port.fd == -1) return -1;
	
	/* remove any characters there may be in the buffer */
	while(read(zaber_port.fd, &tmp, 1) == 1); //fprintf(stderr, "%3d \n", tmp);
	
	/* break the data into 4 bytes */ 
	c = (char *)&(zm.data);
	
	/* Renumbering takes 500ms see ZABER documentation. Actually, it
	 seems that it takes a little more...*/
	if (zm.type==ZABER_RENUMBER) timeout=1200;
	if (zm.type==ZABER_SET_MODE) timeout=10000;
	/* send command message */
	serial_putchar(zaber_port, zm.unit);
	serial_putchar(zaber_port, zm.type);
	serial_putchar(zaber_port, *c);
	serial_putchar(zaber_port, *(c+1));
	serial_putchar(zaber_port, *(c+2));
	serial_putchar(zaber_port, *(c+3));
	usleep(5000);

	/* get answer */
	if (!zaber_readchar(&(answer->unit),timeout)) return -1;
	if (!zaber_readchar(&(answer->type),10)) return -1;
	i=0;
	if (!zaber_readchar(&tmp,10)) return -1;
	i += (int)tmp;
	if (!zaber_readchar(&tmp,10)) return -1;
	i += ((int)tmp)<<8;
	if (!zaber_readchar(&tmp,10)) return -1;
	i += ((int)tmp)<<16;
	if (!zaber_readchar(&tmp,10)) return -1;
	i += ((int)tmp)<<24;
	answer->data=i;
	//fprintf(stderr, "Done with command to unit: %d\n", zm.unit);
	
	/* Check for a power out of range error */
	if (answer->type == ZABER_POWER_SUPPLY_OUT_OF_RANGE)
	  error(WARNING, "Power supply voltage is out of range.");

	return answer->type;
}


/************************************************************************/
/* zaber_get_position()                                                 */
/*                                                                      */
/* Gets the position for a given unit.                                  */
/************************************************************************/

int zaber_get_position(char unit)
{
	struct zaber_message zm, answer; 
	zm.unit    = unit;
	zm.type    = ZABER_RETURN_POSITION;
	zm.data    = 0;
	zaber_send_command(zm, &answer);
	return answer.data;
} /* zaber_get_position()*/


/************************************************************************/
/* zaber_reset_all()                                                    */
/*                                                                      */
/* sends a RESET command 0 0 0 0 0 0                                    */
/************************************************************************/

unsigned char zaber_reset_all(void)
{
	struct zaber_message zm, answer; 
	zm.unit    = ZABER_ALL_UNITS;
	zm.type    = ZABER_RESET;
	zm.data    = 0;
	return zaber_send_command(zm, &answer);
}

/************************************************************************/
/* zaber_renumber()                                                     */
/*                                                                      */
/* sends a renumber command to a chain                                  */
/************************************************************************/

unsigned char zaber_renumber(void)
{
	struct zaber_message zm, answer; 
	char tmp;
	zm.unit    = ZABER_ALL_UNITS;
	zm.type    = ZABER_RENUMBER;
	zm.data    = 0;
	tmp = zaber_send_command(zm, &answer);

	return tmp;
}

/************************************************************************/
/* zaber_home()                                                         */
/*                                                                      */
/* Home a given unit. Note that this command does *not* wait for the    */
/* unit to be homed.                                                    */
/************************************************************************/

unsigned char zaber_home(unsigned char unit)
{
  struct zaber_message zm, answer; 
  unsigned char retval;
  zm.unit    = unit;
  zm.type    = ZABER_HOME;
  zm.data    = 0;
  retval = zaber_send_command(zm, &answer);
  if (unit > 0) z_current_position[unit]=answer.data;
  return retval;
}

/************************************************************************/
/* zaber_move_rel()                                                     */
/*                                                                      */
/* Move a unit by a given number of steps                               */
/************************************************************************/

unsigned char zaber_move_rel(unsigned char unit, int Nsteps)
{
  unsigned char retval;
  struct zaber_message zm, answer; 
  zm.unit    = unit;
  zm.type    = ZABER_MOVE_REL;
  zm.data    = Nsteps;
  retval = zaber_send_command(zm, &answer);
  z_current_position[unit]=answer.data;
  return retval;
}

/************************************************************************/
/* zaber_move_abs()                                                     */
/*                                                                      */
/* Move a unit to a given position                                      */
/************************************************************************/

unsigned char zaber_move_abs(unsigned char unit, int position)
{
  unsigned char retval;
  struct zaber_message zm, answer; 
  zm.unit    = unit;
  zm.type    = ZABER_MOVE_ABS;
  zm.data    = position;
  retval = zaber_send_command(zm, &answer);
  z_current_position[unit]=answer.data;
  return retval;
}


/************************************************************************/
/* zaber_set_current_position()                                         */
/*                                                                      */
/* set postion of a given motor                                         */
/************************************************************************/
unsigned char zaber_set_current_position(unsigned char unit, int position)
{
	struct zaber_message zm, answer; 
	zm.unit    = unit;
	zm.type    = ZABER_SET_CURRENT_POS;
	zm.data    = position;
	return zaber_send_command(zm, &answer);
}

/************************************************************************/
/* user_zaber_reset()                                                   */
/*                                                                      */
/* User version of zaber_reset_all()                                    */
/************************************************************************/
int cmd_zreset(int argc, char* argv[])
{
  if (argc > 1) return error(ERROR, "No arguments for zreset!");
  zaber_reset_all(); /* !!! Enable error checking */
  return NOERROR;
} /* user_zaber_reset() */

/************************************************************************/
/* user_zaber_renumber()                                                */
/*                                                                      */
/* User version of zaber_renumber()                                     */
/************************************************************************/
int cmd_zrenumber(int argc, char* argv[])
{
  if (argc > 1) return error(ERROR, "No arguments for zrenumber!");
  zaber_renumber(); /* !!! Enable error checking */  
  return NOERROR;
} /* user_zaber_renumber() */

/************************************************************************/
/* user_zaber_home()                                                    */
/*                                                                      */
/* User version of zaber_home()                                         */
/************************************************************************/
int cmd_zhome(int argc, char* argv[])
{
  unsigned char unit;
  if (argc != 2)  return error(ERROR, "Usage: zhome [unit]");
  sscanf(argv[1], "%hhu", &unit);
  zaber_home(unit);
  return NOERROR;
} /* user_zaber_home() */

/************************************************************************/
/* user_zaber_move_rel()                                                */
/*                                                                      */
/* User version of zaber_move_rel()                                     */
/************************************************************************/
int cmd_zmovrel(int argc, char* argv[])
{
  unsigned char unit=0;
  int nsteps=0;
  if (argc != 3)  return error(ERROR, "Usage: zmovrel [unit] [nsteps]");
  if (sscanf(argv[1], "%hhu", &unit) != 1) return error(ERROR, "Error parsing unit.");
  if (sscanf(argv[2], "%d", &nsteps) != 1) return error(ERROR, "Error parsing posn.");
  if (unit < 1 || unit > NUM_ZABER) return error(ERROR, "Unit between 1 and %d only.", NUM_ZABER);
  zaber_move_rel(unit, nsteps);
  return NOERROR;
} /* user_zaber_move_rel() */

/************************************************************************/
/* user_zaber_move_abs()                                                */
/*                                                                      */
/* User version of zaber_move_abs()                                     */
/************************************************************************/
int cmd_zmovabs(int argc, char* argv[])
{
  unsigned char unit=0;
  int posn=0;
  if (argc != 3)  return error(ERROR, "Usage: zmovabs [unit] [posn]");
  if (sscanf(argv[1], "%hhu", &unit) != 1) return error(ERROR, "Error parsing unit.");
  if (sscanf(argv[2], "%d", &posn) != 1)   return error(ERROR, "Error parsing posn.");
  if (unit < 1 || unit > NUM_ZABER) return error(ERROR, "Unit between 1 and %d only.", NUM_ZABER);
  zaber_move_abs(unit, posn);
  return NOERROR;
} /* user_zaber_move_abs() */

/************************************************************************/
/* user_zaber_set_current_position()                                    */
/*                                                                      */
/* User version of zaber_set_current_position()                         */
/************************************************************************/

int cmd_zsetpos(int argc, char* argv[])
{
  unsigned char unit;
  int posn;
  if (argc != 3)  return error(ERROR, "Usage: zsetposn [unit] [posn]");
  sscanf(argv[1], "%hhu", &unit);
  sscanf(argv[2], "%d", &posn);
  zaber_set_current_position(unit, posn);
  return NOERROR;
} /* user_zaber_set_current_position() */

/************************************************************************/
/* user_zaber_get_position()                                            */
/*                                                                      */
/* User version of zaber_get_position.                                  */
/************************************************************************/

int cmd_zgetpos(int argc, char *argv[])
{
  unsigned char unit;
  if (argc == 1) return error(ERROR, "Usage: zgetposn [unit]");
  sscanf(argv[1], "%hhu", &unit);
  return error(MESSAGE, "Unit %d has position %d.", unit,zaber_get_position(unit));
} /* user_zaber_get_position()*/

/************************************************************************/
/* cmd_xy()                                                             */
/*                                                                      */
/* Do two relative moves.                                               */
/************************************************************************/

int cmd_xy(int argc, char *argv[])
{
  int nxsteps=0,nysteps=0;
  if (argc != 3)  return error(ERROR, "Usage: xy [xsteps] [ysteps]");
  if (sscanf(argv[1], "%d", &nxsteps) != 1) return error(ERROR, "Error parsing xsteps.");
  if (sscanf(argv[2], "%d", &nysteps) != 1) return error(ERROR, "Error parsing ysteps.");
  zaber_move_rel(IFU_X, nxsteps);
  usleep(1000); /* Just in case needed. */
  zaber_move_rel(IFU_Y, nysteps);
  return NOERROR;
} /* cmd_xy()*/

/************************************************************************/
/* cmd_xyf()                                                             */
/*                                                                      */
/* Do two relative moves, to a fixed position                           */
/************************************************************************/

int cmd_xyf(int argc, char *argv[])
{
  int new_fixed=0;
  if (argc != 2)  return error(ERROR, "Usage: xyf [fixed_position]");
  if (sscanf(argv[1], "%d", &new_fixed) != 1) return error(ERROR, "Error parsing new_fixed.");
  zaber_goto_fixed_position(IFU_X, new_fixed);
  usleep(1000); /* Just in case needed. */
  zaber_goto_fixed_position(IFU_Y, new_fixed);
  return NOERROR;
} /* cmd_xyf()*/
