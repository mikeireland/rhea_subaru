//#define ZABER_SERIAL "/dev/serial/by-path/pci-0000:00:1d.0-usb-0:1.8.1.3.4.2:1.0-port0"
//#define ZABER_SERIAL "/dev/serial/by-path/pci-0000:00:14.0-usb-0:1:1.0-port0"
//#define ZABER_SERIAL "/dev/serial/by-path/pci-0000:00:1d.0-usb-0:1.8.1.3.4.1:1.0-port0"
#define ZABER_SERIAL "/dev/serial/by-path/pci-0000:00:1a.0-usb-0:1.3.4.1:1.0-port0"

#include "zaber.h"
#include "thor_usb.h"
#include "uicoms.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <zmq.h>

/* 
 * Error messages
 */

#ifndef _ERRDEFS_
#define _ERRDEFS_

#define MESSAGE 1 /* Use error system for putting up a message */
#define NOERROR	0 /* No error has occured */
#define WARNING (-1) /* A warning, nothing too dangerous has happened */
#define ERROR	(-2) /* Some kind of major error.Prog does not have to reboot */
#define FATAL	(-3) /* A fatal error has occured. The programme must exit(1)*/ 
#endif

int cmd_exit(int argc, char **argv){
	return FATAL;
}

int cmd_help(int argc, char **argv){
	/* Insert output of "sed '{:q;N;s/\n/\\n/g;t q}' cmds" */
	return error(MESSAGE, "exit\thelp\tstartcam\tstopcam\taoi\nfps\tpixelclock\tcamgain\tdestripe\nzdark\tsave\tsavecube\titime\tsetnframe\nzreadposfile\tzwriteposfile\tzgotofixed\nzreset\tzrenumber\tzhome\tzmovrel\tzmovabs\nzsetpos\tzgetpos\tdark\tzzero\nxy\txyf\tzgetfixed\tstatus\nled\tobject\tconfigure\nsetndemod");
}

/************************************************************************/
/* cmd_status                                                           */
/*                                                                      */
/* Get the status of everything important.                              */
/************************************************************************/
int cmd_status(int argc, char **argv){
    char retstring[512];
    char tempstr[80];
    int i;
    /* Construct a return string containing a (manual!) json encoding
        of the status. */
    strcpy(retstring, "status ");
    strcat(retstring, "{\"pos\": [");
    for (i=1;i<=NUM_ZABER;i++){
        sprintf(tempstr, "%d", z_current_position[i]);
        strcat(retstring, tempstr);
        if (i<NUM_ZABER) strcat(retstring, ", ");
    }
    strcat(retstring, "], \"fixed\": [");
    for (i=1;i<=NUM_ZABER;i++){
        sprintf(tempstr, "%d", z_current_fixed_position[i]);
        strcat(retstring, tempstr);
        if (i<NUM_ZABER) strcat(retstring, ", ");
    }
    sprintf(tempstr, "], \"fps\": %5.1f, \"exptime\": %5.1f, \"aoi\": [%d, %d, %d, %d]}",
        usb_camera.fps, usb_camera.exptime, usb_camera.x, usb_camera.y, usb_camera.dx, usb_camera.dy);
    strcat(retstring, tempstr);
    return error(MESSAGE, retstring);
}

/************************************************************************/
/* cmd_configure                                                        */
/*                                                                      */
/* Configure the SCExAO bench for RHEA or VAMPIRES.                     */
/************************************************************************/
int cmd_configure(int argc, char **argv){
    if (argc != 2)
        return error(ERROR, "Useage: configure [rhea|vampires]");
    if (strcmp(argv[1], "rhea")==0){
        if (system("ssh lestat@vampires '/home/lestat/code/script/conex 2 pa 143'"))
            return error(ERROR, "Could not execute command on vampires.");
        //if (execlp("rhea_pickoff", "rhea_pickoff", "in", (char *)NULL))
        //if (system("/home/scexao/bin/devices/rhea_pickoff in"))
        //    return error(ERROR, "Could not move RHEA pickoff.");
    } else if (strcmp(argv[1], "vampires")==0){
        if (system("ssh lestat@vampires '/home/lestat/code/script/conex 2 pa 0'"))
            return error(ERROR, "Could not execute command on vampires.");
        //if (execlp("rhea_pickoff", "rhea_pickoff", "out", (char *)NULL))
        //if (system("/home/scexao/bin/devices/rhea_pickoff out"))
        //    return error(ERROR, "Could not move RHEA pickoff.");
    } else {
        return error(ERROR, "Useage: configure [rhea|vampires]");
    }
    return error(MESSAGE, "move rhea_pickoff manually due to PATH issues.");
}

/* As we're doing this quickly... skip a header file for now */
struct {
		char 	*name;
		int	(*function)(int argc, char **argv);
	} functions[] = 

	{
		{"exit",	cmd_exit},
		{"help",	cmd_help},
		{"startcam",	cmd_startcam},
		{"stopcam",	cmd_stopcam},
		{"aoi",	cmd_aoi},
		{"optimalcam",	cmd_optimalcam},
		{"fps",	cmd_fps},
		{"pixelclock",	cmd_pixelclock},
		{"camgain",	cmd_camgain},
		{"destripe",	cmd_destripe},
		{"dark",	cmd_dark},
		{"zdark",	cmd_zdark},
		{"save",	cmd_save},
		{"savecube",	cmd_savecube},
		{"itime",	cmd_itime},
		{"setnframe",	cmd_setnframe},
        {"setndemod",	cmd_setndemod},
		{"image",       cmd_image},
		{"zreadposfile",	cmd_zreadposfile},
		{"zwriteposfile",	cmd_zwriteposfile},
		{"zgotofixed",	cmd_zgotofixed},
		{"zzero",	cmd_zzero},
		{"zreset",	cmd_zreset},
		{"zrenumber",	cmd_zrenumber},
		{"zhome",	cmd_zhome},
		{"zmovrel",	cmd_zmovrel},
		{"zmovabs",	cmd_zmovabs},
		{"zsetpos",	cmd_zsetpos},
		{"zgetpos",	cmd_zgetpos},
		{"xy",	cmd_xy},
		{"xyf", cmd_xyf},
        {"zgetfixed", cmd_zgetfixed},
        {"status", cmd_status},
        {"led", cmd_led},
        {"object", cmd_object},
        {"configure", cmd_configure},
		/* NULL to terminate */
		{NULL,		NULL}};

#define MAX_ARGS 10


/* The main program */
int main(int argc, char **argv)
{
	int retval=NOERROR;
	char input[COMMAND_BUFFERSIZE];
	int funct_argc=0, loop=0, function_index=0, i;
	char funct_strs[MAX_ARGS][MAX_COMMAND_SIZE];
	char *funct_argv[MAX_ARGS];
	char *astr;

	/* Initialize stuff */
	for (i=0;i<MAX_ARGS;i++){
		funct_argv[i] = (char *)funct_strs[i];
	}

	open_zaber_port(ZABER_SERIAL);
	cmd_zreadposfile(0,NULL);
	zaber_init();
	if (open_usb_camera() != NOERROR) fprintf(stderr, "Could not open USB camera");

	/* Get ready for accepting connections... */
	open_server_socket();
	printf("> ");
	fflush(stdout);

	/* Our infinite loop. */
	while (retval != FATAL) 
	{
		/* The following lines could be replaced with a long scanf line.
		This is probably safer. */
		/* scanf("%s", input); */
		get_next_command(input);
	//	fgets(input, BUFFERSIZE, stdin);
		if (input[0] != 0){
			funct_argc=0;
			funct_strs[0][0]=0;
			astr=strtok(input, " \n\t");
			while (astr != NULL){
				strcpy((char *)funct_strs[funct_argc], astr);
				funct_argc++;
				astr=strtok(NULL," \n\t");
			}	
			/* Now try to find the function */
			loop=0;
			while(functions[loop].name != NULL){
				if (strcmp(funct_strs[0],functions[loop].name) == 0){
					function_index = loop;
					break;
				}
				loop++;
			}
			if (functions[loop].function == NULL){
				retval = error(MESSAGE, "Unknown command: %s (strlen %d) \n", funct_strs[0], strlen(funct_strs[0]));
			} else {
				retval = (*(functions[function_index].function))(funct_argc, funct_argv);
			}
			if (client_socket == -1){
				printf("> ");
				fflush(stdout);
			} else {
                error(MESSAGE,"\n");
				client_socket =-1; 
			}
		} else usleep(1000);
		/* Now do the background tasks. NB, these could be an array of tasks only done
		sometimes...  */
		bgnd_usb_camera();
		bgnd_complete_fits_cube();
	}
	close_server_socket();
	close_zaber_port();
	close_usb_camera();
	return 0;
}
