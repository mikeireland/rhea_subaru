/************************************************************************/
/* zaber.h                                                              */
/*                                                                      */
/* Genera Header File.                                  		*/
/************************************************************************/
/*                                                                      */
/*     Center for High Angular Resolution Astronomy                     */
/* Georgia State University, Atlanta GA 30303-3083, U.S.A.              */
/*                                                                      */
/*                                                                      */
/* Telephone: 1-404-651-1882                                            */
/* Fax      : 1-404-651-1389                                            */
/* email    : antoine@chara.gsu.edu                                     */
/* WWW      : http://www.chara.gsu.edu/~merand                          */
/*                                                                      */
/* (C) This source code and its associated executable                   */
/* program(s) are copyright.                                            */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Author : Antoine Merand and Mike Ireland                        	*/
/* Date   : Oct 2007                                                    */
/************************************************************************/

/* ------------- ZABER COMMANDS for FIRMWARE 2.93 --------------- */

#define NUM_ZABER 3
#define NUM_ZABER_FIXED 4

extern char *zaber_names[NUM_ZABER+1];

#define ZABER_POSITION_FILE "./zaber_positions"

/* The zaber units. */
#define ZABER_ALL_UNITS 0x00

/* commands: */
#define ZABER_RESET             0x00
#define ZABER_HOME              0x01
#define ZABER_RENUMBER          0x02
#define ZABER_MOVE_ABS          0x14
#define ZABER_MOVE_REL          0x15
#define ZABER_MOVE_CONST_SPEED  0x16
#define ZABER_STOP              0x17
#define ZABER_RESTORE_FACT_SET  0x24
#define ZABER_SET_MODE          0x28
#define ZABER_SET_STEP_T        0x29
#define ZABER_SET_TARGET_STEP_T 0x2a
#define ZABER_SET_ACCELERATION  0x2b
#define ZABER_SET_MAX_RANGE_TRV 0x2c
#define ZABER_SET_CURRENT_POS   0x2d
#define ZABER_SET_MAX_REL_MOVE  0x2e
#define ZABER_SET_ALIAS         0x30
#define ZABER_RETURN_DEVICE_ID  0x32
#define ZABER_RETURN_FIRMWARE_V 0x33
#define ZABER_POWER_VOLTAGE     0x34
#define ZABER_RETURN_SETTING    0x35
#define ZABER_RETURN_POSITION   0x3c

/* modes */
#define ZABER_DISABLE_AUTO_REPLY            1
#define ZABER_ENABLE_ANTI_BACKLASH          2 
#define ZABER_ENABLE_ANTI_STICKTION         4
#define ZABER_DISABLE_POTENTIOMETER         8
#define ZABER_ENABLE_POSITION_TRACKING      16
#define ZABER_DISABLE_MAN_POSITION_TRACKING 32
#define ZABER_ENABLE_LOGIC_CHAN_COMM_MODE   64
#define ZABER_HOME_STATUS                   128

/* replies */
#define ZABER_CONSTANT_SPEED_POSITION_TRACKING 8
#define ZABER_MANUAL_POSITIN_CHANGE 10
#define ZABER_POWER_SUPPLY_OUT_OF_RANGE 14
#define ZABER_COMMAND_DATA_OUT_OF_RANGE 255

struct zaber_message {
	unsigned char unit;
	unsigned char type;
	int data;
};

/* Globals */
extern int z_current_position[NUM_ZABER+1];
extern int z_saved_positions[NUM_ZABER+1][NUM_ZABER_FIXED];
extern int z_current_fixed_position[NUM_ZABER+1];

/* ----------------- zaber_control.c --------------------------- */
extern int open_zaber_port(char *port_name);
extern int close_zaber_port(void);
extern int zaber_init(void);
extern unsigned char zaber_send_command(struct zaber_message zm, struct zaber_message *answer); 
extern unsigned char zaber_reset_all(void);
extern unsigned char zaber_renumber(void);
extern unsigned char zaber_home(unsigned char unit);
extern unsigned char zaber_move_rel(unsigned char unit, int Nsteps);
extern unsigned char zaber_move_abs(unsigned char unit, int position);
extern unsigned char zaber_set_current_position(unsigned char unit, int position);
extern int zaber_goto_fixed_position(unsigned char unit, int position);
extern int zaber_get_position(char unit);
extern int cmd_zreset(int argc, char* argv[]);
extern int cmd_zrenumber(int argc, char* argv[]);
extern int cmd_zhome(int argc, char* argv[]);
extern int cmd_zmovrel(int argc, char* argv[]);
extern int cmd_zmovabs(int argc, char* argv[]);
extern int cmd_zsetpos(int argc, char* argv[]);
extern int cmd_zgotofixed(int argc, char * argv[]);
extern int cmd_zgetpos(int argc, char *argv[]);
extern int cmd_zzero(int argc, char* argv[]);
extern int cmd_zreadposfile(int argc, char* argv[]);
extern int cmd_zwriteposfile(int argc, char* argv[]);
