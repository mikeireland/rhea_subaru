#include "zaber.h"
#include "thor_usb.h"
#include "uicoms.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

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
	error(MESSAGE, "exit\thelp\tstartcam\tstopcam\taoi\nfps\tpixelclock\tcamgain\tdestripe\nzdark\tsave\tsavecube\titime\tsetnframe\nzreadposfile\tzwriteposfile\tzgotofixed\nzreset\tzrenumber\tzhome\tzmovrel\tzmovabs\nzsetpos\tzgetpos\tdark\tzzero");
	return NOERROR;
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

	open_zaber_port("/dev/ttyUSB0");
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
				error(MESSAGE, "Unknown command: %s (strlen %d) \n", funct_strs[0], strlen(funct_strs[0]));
			} else {
				retval = (*(functions[function_index].function))(funct_argc, funct_argv);
			}
			if (client_socket == -1){
				printf("> ");
				fflush(stdout);
			} else {
				write(client_socket, "\n> ", 3);
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
