#ifndef __MDSS_I2C_CMD_H_
#define __MDSS_I2C_CMD_H_
/*struct that store a i2c command*/
struct i2c_ctrl_hdr {
	int rw_flags; /*0: read, 1: write*/
	int reg;
	int val;
};

/*i2c command*/
struct i2c_cmd_list {
	struct i2c_ctrl_hdr *cmds;
	int cmds_cnt;
};

/*get the operation of i2c*/
struct i2c_client *mdss_get_icn6202_i2c_client(void);

#endif /*__MDSS_I2C_CMD_H_*/

