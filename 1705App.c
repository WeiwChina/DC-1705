/*************************************************************************
	> File Name: 1705App.c
	> Author:    www
	> Func:  
	> Created Time: 2018年11月01日 星期四 16时53分05秒
 ************************************************************************/

#include     <stdio.h>      /*标准输入输出定义*/
#include     <string.h>
#include     <stdlib.h>     /*标准函数库定义*/
#include     <unistd.h>     /*Unix标准函数定义*/
#include     <sys/types.h>  /**/
#include     <sys/stat.h>   /**/
#include     <fcntl.h>      /*文件控制定义*/
#include     <termios.h>    /*PPSIX终端控制定义*/
#include     <errno.h>      /*错误号定义*/
#include	 <sys/time.h>
#include	 <time.h>
#include     <pthread.h>

#define   LINK_CMD          0x30
#define   BREAK_CMD         0x35
#define   BT_CMD            0x36
#define   HEART_BEAT_CMD    0x33

int fd;
int ledMod;

unsigned char A3_Com(unsigned char *rBuff);
unsigned char A4_Com(unsigned char *rBuff);
unsigned char A5_Com_1(unsigned char *rBuff);
unsigned char A5_Com_2(unsigned char *rBuff);
unsigned char A7_Com_1(unsigned char *rBuff);
unsigned char A7_Com_2(unsigned char *rBuff);
unsigned char A8_Com(unsigned char *rBuff);
unsigned char A9_Com(unsigned char *rBuff);
unsigned char A11_Com(unsigned char *rBuff);
int RcvData(void);
int sendData(void);
void *LedFlash(void*);



 /******************************************************************************************************************
* Function Name  : main
* Description    : 自动判断接收数据还是发送数据
* Input          : None
* Return         : None
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
int main(int argc, char **argv)
{
	pthread_t tidp;
	//sysLed -> GPIO1_10
	//usb    -> GPIO1_12
	//net    -> GPIO1_15
	ledMod = 0;
	system("echo out > /sys/class/gpio/gpio10/direction"); 
	
	if ((pthread_create(&tidp, NULL, LedFlash, NULL)) == -1)
	{
		printf("create error!\n");
		return 1;
	}


	while(1)
	{
		ledMod = 0;
		RcvData();
		close(fd);
		system("echo 0 > /sys/class/gpio/gpio10/value"); 
		ledMod = 1;
		sendData();
		close(fd);
		system("echo 1 > /sys/class/gpio/gpio10/value"); 
	}
}

 /******************************************************************************************************************
* Function Name  : set_port
* Description    : 初始化
* Input          : None
* Return         : None
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
void InitSerial(char bt)
{
	int ret;
	struct termios stPara;
	bzero(&stPara,sizeof(struct termios));
	stPara.c_cflag|=CLOCAL|CREAD;	/*忽略调制解调器线路状态 | 使用接收器*/
	tcgetattr(fd,&stPara);
	stPara.c_cflag&=~CSIZE;  	  	/*字符长度，取值范围为CS5、CS6、CS7或CS8*/
	stPara.c_cflag|=CS8;			/*8位数据位*/
	stPara.c_cflag&=~PARENB;	 	/*使用无校验*/	
	stPara.c_cflag&=~CSTOPB;		/*设置一位停止位*/

	stPara.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	stPara.c_oflag &= ~OPOST;
	stPara.c_iflag &= ~(IXON | IXOFF | IXANY);
	stPara.c_iflag &= ~(INLCR | ICRNL | IGNCR);
	stPara.c_oflag &= ~(ONLCR | OCRNL);
	switch(bt)
	{
		case 0:
		cfsetispeed(&stPara,B1200);	/*返回 termios 结构中存储的输入波特率*/
		cfsetospeed(&stPara,B1200);	/*置termios_p指向的termios 结构中存储的输出波特率为speed*/ 
		break;
		case 1:
		cfsetispeed(&stPara,B115200);	/*返回 termios 结构中存储的输入波特率*/
		cfsetospeed(&stPara,B115200);	/*置termios_p指向的termios 结构中存储的输出波特率为speed*/ 
		break;
		default:
		break;
	}
	
	stPara.c_cc[VMIN] = 0;
	stPara.c_cc[VTIME] = 6;
	tcflush(fd,TCIOFLUSH);
	ret=tcsetattr(fd,TCSANOW,&stPara);
	if(ret!=0)
	{
		perror("tcsetattr fd");
		exit(1);
	}
}

/*功能  :打开对应的串口设备
 *参数  :
 *返回值:
 */
 /******************************************************************************************************************
* Function Name  : set_port
* Description    : 打开串口并初始化
* Input          : None
* Return         : None
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
void SetPort(char bt)
{
	if((fd=open("/dev/ttymxc4",O_RDWR|O_NOCTTY)) < 0)
	{
		perror("open port");
		exit(1);
	}
	InitSerial(bt);
}

/******************************************************************************************************************
* Function Name  : SaveData
* Description    : 按日期保存数据
* Input          : @*p 数据指针，必须为ASII码，数据尾必须以‘\0'结束
                   @len 要保存数据长度
* Return         : @-1 文件打开失败或写失败
                   @>=0 写字符个数 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
int SaveData(char board, char *p, unsigned int len)
{
	FILE *filp = NULL;
	time_t timep;
	struct tm *tm_p;
	char   rcvBuff[1024];
	char   filePath[100]="/home/dataSave/";
	int    ret=0;
	
	time(&timep);
	tm_p = localtime(&timep);
	/*
	printf("%d-%d-%d %d:%d:%d\n\r", (1900 + tm_p->tm_year), ( 1 + tm_p->tm_mon), tm_p->tm_mday, \
	                              (tm_p->tm_hour + 12), tm_p->tm_min, tm_p->tm_sec); 
	*/
	//按照板卡存储
	//sprintf(rcvBuff,"%sA%d_%d-%d-%d.txt", filePath,board,(1900 + tm_p->tm_year), ( 1 + tm_p->tm_mon), tm_p->tm_mday); 
	//顺序存储
	sprintf(rcvBuff,"%s%d-%d-%d.txt", filePath,(1900 + tm_p->tm_year), ( 1 + tm_p->tm_mon), tm_p->tm_mday); 
	
	//printf("open file:%s",rcvBuff);
	
	//第一步打开文件
	filp = fopen(rcvBuff,"a+");
	if(filp==NULL)
	{
		printf("open失败.\n\r");
		return -1;
	}
	// 第二步：写文件
    ret = fwrite(p, 1,strlen(p),filp);
	//fseek(filp, 8.1, SEEK_END);
    if (ret < 0)
    {
        printf("write失败.\n\r");
		return ret;
    }
    else
    {
        printf("write成功，写入了%d个字符\n\r", ret);
    }
	fclose(filp);
	filp = NULL;
	
	return ret;
}
/*   */
/******************************************************************************************************************
* Function Name  : CheckSum
* Description    : 校验和
* Input          : @*buff 数据指针
                   @len 长度
* Return         : @a 校验和 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char CheckSum(unsigned char *buff, int len)
{
	int i=0;
	unsigned char a = 0;
	
	for(i=0; i<len; i++)
	{
		a += buff[i];
	}
	return a;
}

/******************************************************************************************************************
* Function Name  : CheckSum
* Description    : 校验和
* Input          : @*buff 数据指针
                   @len 长度
* Return         : @a 校验和 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
int RcvData(void)
{
	int    fd_file;
	int nread;
	unsigned char buff[512];
	unsigned char fbuff[1024];
	unsigned char p[100]="1--------";
	char otFlag = 0;
	//char *dev ="/dev/ttySAC1"; 

	int n=0,i=0,ret, comNum;
	unsigned char retU8;
    const char* dev   = NULL;
	
	time_t timep;
	struct tm *tm_p;
	
	unsigned char cn;  //校验和
	unsigned char bn = 0;  //板卡编号
	unsigned char index = 0;
	unsigned int  linkErrNum = 0;
	


    //p   = argv[1];
	//memcpy(p, argv[1], sizeof(argv[1]));
	dev = "/dev/ttymxc4";
	
	/*  打开串口  设置波特率1200*/
	SetPort(0);
	
	printf("\n\rserial init success!");
	memset(buff,0,sizeof(buff));
	//连接机器
	buff[0] = 0x02;
	buff[1] = LINK_CMD;
	buff[2] = 0;
	buff[3] = CheckSum(buff,3);
	write(fd, buff, 4);
	printf("\n\rsend：0x%02X 0x%02X 0x%02X 0x%02X",buff[0],buff[1],buff[2],buff[3]);
	usleep(200000);
	comNum = 0;
	while(1)
	{
		memset(buff, 0, sizeof(buff));
		n = read(fd, buff, 5);
		printf("\n\rrcv：num=%d,0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",n,buff[0],buff[1],buff[2],buff[3],buff[4]);
		if(n!=0)
		{
			cn = CheckSum(buff,4);
			if(buff[4]==cn && buff[2] == LINK_CMD|0x80)
			{
				printf("\r\nlink OK!");
				break;
			}
		}
		comNum++;
		if(comNum>3)
		{
			close(fd);
			printf("\r\nlink fault!");
			return -1;
		}
		buff[0] = 0x02;
		buff[1] = LINK_CMD;
		buff[2] = 0;
		buff[3] = CheckSum(buff,3);
		write(fd, buff, 4);
		printf("\n\rsend：0x%02X 0x%02X 0x%02X 0x%02X",buff[0],buff[1],buff[2],buff[3]);
		usleep(200000);
	}
	
	//设置波特率115200；
	buff[0] = 0x02;
	buff[1] = BT_CMD;
	buff[2] = 1;
	buff[3] = 7;  //115200
	buff[4] = CheckSum(buff,4);
	write(fd, buff, 5);
	usleep(200000);
	comNum = 0;
	while(1)
	{
		n = read(fd, buff, 5);
		if(n!=0)
		{
			cn = CheckSum(buff,4);
			if(buff[4]==cn && buff[2] == BT_CMD|0x80)
			{
				printf("\r\nset btl OK!");
				break;
			}
		}
		comNum++;
		if(comNum>3)
		{
			printf("\r\nset btl fault!");
			close(fd);
			return -1;
		}
		buff[0] = 0x02;
		buff[1] = BT_CMD;
		buff[2] = 1;
		buff[3] = 7;
		buff[4] = CheckSum(buff,3);
		write(fd, buff, 5);
		usleep(200000);
	}

	/*  打开串口  设置波特率115200*/
	SetPort(1);
	
	//循环请求各板卡数据
	index = 8;
	while(1)
	{
		index++;
		if(index>8) index=0;
		if(linkErrNum==0x01FF) return -1;
		
		switch(index)
		{
			/* ----- A3 --------*/
			case 0:
			retU8 = A3_Com(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0001;
				continue;
			}
			else
			{
				bn = 3;
			}
			break;
			/* ---- A4 --------- */
			case 1:
			retU8 = A4_Com(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0002;
				continue;
			}
			else
			{
				bn = 4;
			}
			break;
			/* --- A5_1 --------- */
			case 2:
			retU8 = A5_Com_1(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0004;
				continue;
			}
			else
			{
				bn = 5;
			}
			break;
			/* --- A5_2 --------- */
			case 3:
			retU8 = A5_Com_2(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0008;
				continue;
			}
			else
			{
				bn = 5;
			}
			break;
			/* --- A7_1 --------- */
			case 4:
			retU8 = A7_Com_1(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0010;
				continue;
			}
			else
			{
				bn = 7;
			}
			break;
			/* --- A7_2 --------- */
			case 5:
			retU8 = A7_Com_2(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0020;
				continue;
			}
			else
			{
				bn = 7;
			}
			break;
			
			/* --- A8 --------- */
			case 6:
			retU8 = A8_Com(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0040;
				continue;
			}
			else
			{
				bn = 8;
			}
			break;
			
			/* --- A9 --------- */
			case 7:
			retU8 = A9_Com(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0080;
				continue;
			}
			else
			{
				bn = 9;
			}
			break;
			
			/* --- A11 --------- */
			case 8:
			retU8 = A11_Com(buff);
			
			if(retU8==0)
			{
				linkErrNum |= 0x0100;
				continue;
			}
			else
			{
				bn = 11;
			}
			break;
			
			default:
			continue;
			break;
		}
		
		/*    读取成功后存储   */
		linkErrNum = 0x0000;  //有一个成功，标致置0
		//获取时间；
		time(&timep);
		tm_p = localtime(&timep);
		printf("\n\r%04d-%02d-%02d %02d:%02d:%02d#", (1900 + tm_p->tm_year), ( 1 + tm_p->tm_mon), tm_p->tm_mday, \
									  (tm_p->tm_hour), tm_p->tm_min, tm_p->tm_sec); 
		//\r\n1900-10-24 15:08:00#//22个
		sprintf(fbuff,"\r\n%04d-%02d-%02d %02d:%02d:%02d#",(1900 + tm_p->tm_year), ( 1 + tm_p->tm_mon), tm_p->tm_mday, \
									   (tm_p->tm_hour), tm_p->tm_min, tm_p->tm_sec); 
		
		for(i=0; i<retU8; i++)
		{
			printf("%02X ",buff[i]);
			sprintf(fbuff+22+i*3, "%02x ",buff[i]);
		}
		fbuff[22+i*3+1] = 0;
		SaveData(bn, fbuff, i);
		memset(buff,0,retU8);
		sleep(1);
				
		/*
		if(n!=0)
		{
			printf("Read %d byte:",n,buff);
			if(otFlag)
			{
				otFlag = 0;

				time(&timep);
				tm_p = localtime(&timep);
				printf("%d-%d-%d %d:%d:%d\n\r", (1900 + tm_p->tm_year), ( 1 + tm_p->tm_mon), tm_p->tm_mday, \
											  (tm_p->tm_hour + 12), tm_p->tm_min, tm_p->tm_sec); 
				sprintf(fbuff,"\r\n%d-%d-%d %d:%d:%d: ",(1900 + tm_p->tm_year), ( 1 + tm_p->tm_mon), tm_p->tm_mday, \
											   (tm_p->tm_hour + 12), tm_p->tm_min, tm_p->tm_sec); 
				// fbuff[0] = '\r';
				// fbuff[1] = '\n';
				// fbuff[2] = '\0';
				SaveData(fbuff, 1);
			}
			for(i=0; i<n; i++)
			{
				printf("%02X ",buff[i]);
				sprintf(fbuff+i*3, "%02x ",buff[i]);
			}
			fbuff[i*3+1] = 0;
			SaveData(fbuff, i);
			memset(buff,0,n);
			n = 0;
			printf("\n\r");
		}
		else
		{
			otFlag = 1;
		}
		*/
	}
}
/******************************************************************************************************************
* Function Name  : DisDataHex
* Description    : 以hex显示数据
* Input          : @*buff 数据指针
                   @len 长度
* Return         : @a 校验和 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
void DisDataHex(char *buff, char len)
{
	char i;
	
	for(i=0; i<len; i++)
	{
		printf("0x%02X ",buff[i]);
	}
}
/******************************************************************************************************************
* Function Name  : A3_Com
* Description    : 和A3板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A3_Com(unsigned char *rBuff)
{
	unsigned char wBuff[14] = {0x02,0x60,0x09,0x01,0x03,0x07,0x05,0x00,0x08,0x03,0x06,0x03,0x8f}; //13
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 13);
	usleep(200000);
	n = read(fd, buff, 24);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,23);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A3 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA3 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}
/******************************************************************************************************************
* Function Name  : A4_Com
* Description    : 和A4板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A4_Com(unsigned char *rBuff)
{
	unsigned char wBuff[18] = {0x02,0x60,0x0D,0x01,0x04,0x07,0x09,0x00,0x05,0x04,0x04,0x04,0x08,0x04,0x0C,0x04,0xB1}; //17
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 17);
	usleep(200000);
	n = read(fd, buff, 5);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,i-1);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A4 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA4 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}
/******************************************************************************************************************
* Function Name  : A5_Com_1
* Description    : 和A5板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A5_Com_1(unsigned char *rBuff)
{
	unsigned char wBuff[18] = {0x02,0x60,0x0B,0x01,0x05,0x07,0x07,0x00,0x08,0x05,0x17,0x05,0x11,0x05,0xC0}; //15
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 15);
	usleep(200000);
	n = read(fd, buff, 5);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,i-1);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A5_1 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA5_1 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}
/******************************************************************************************************************
* Function Name  : A5_Com_2
* Description    : 和A5板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A5_Com_2(unsigned char *rBuff)
{
	unsigned char wBuff[18] = {0x02,0x60,0x09,0x01,0x05,0x07,0x05,0x00,0x17,0x05,0x11,0x05,0xAF}; //13
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 13);
	usleep(200000);
	n = read(fd, buff, 5);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,i-1);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A5_2 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA5_2 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}
/******************************************************************************************************************
* Function Name  : A7_Com_1
* Description    : 和A7板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A7_Com_1(unsigned char *rBuff)
{
	unsigned char wBuff[18] = {0x02,0x60,0x07,0x01,0x05,0x07,0x03,0x00,0x03,0x07,0x83}; //11
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 11);
	usleep(200000);
	n = read(fd, buff, 5);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,i-1);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A7_1 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA7_1 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}
/******************************************************************************************************************
* Function Name  : A7_Com_2
* Description    : 和A7板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A7_Com_2(unsigned char *rBuff)
{
	unsigned char wBuff[18] = {0x02,0x60,0x07,0x01,0x04,0x07,0x03,0x00,0x0B,0x07,0x8A}; //11
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 11);
	usleep(200000);
	n = read(fd, buff, 5);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,i-1);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A7_2 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA7_2 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}
/******************************************************************************************************************
* Function Name  : A8_Com
* Description    : 和A7板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A8_Com(unsigned char *rBuff)
{
	unsigned char wBuff[18] = {0x02,0x60,0x07,0x01,0x05,0x07,0x03,0x00,0x03,0x08,0x84}; //11
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 11);
	usleep(200000);
	n = read(fd, buff, 5);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,i-1);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A8 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA8 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}
/******************************************************************************************************************
* Function Name  : A9_Com
* Description    : 和A9板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A9_Com(unsigned char *rBuff)
{
	unsigned char wBuff[18] = {0x02,0x60,0x09,0x01,0x03,0x07,0x05,0x00,0x04,0x09,0x12,0x09,0xA3}; //13
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 13);
	usleep(200000);
	n = read(fd, buff, 5);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,i-1);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A9 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA9 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}
/******************************************************************************************************************
* Function Name  : A11_Com
* Description    : 和A11板卡通信
* Input          : @*buff 数据指针
* Return         : @13+i 数据长度 
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
unsigned char A11_Com(unsigned char *rBuff)
{
	unsigned char wBuff[18] = {0x02,0x60,0x09,0x01,0x03,0x07,0x05,0x00,0x03,0x0B,0x0D,0x0B,0xA1}; //13
	unsigned char qBuff[5] = {0x02,0x63,0x00,0x65};
	unsigned char buff[100] = {0};
	unsigned char cm = 0;
	unsigned char i,n,errNum;
	char flag = 0; //发送成功标识
	
	write(fd, wBuff, 13);
	usleep(200000);
	n = read(fd, buff, 5);
	printf("\r\nrcv %d:",n);
	DisDataHex(buff,n);	
	if(buff[0]==0x02 && buff[1] ==0xE0 && buff[2]==0x01)
	{
		flag = 1;
	}
	
	errNum = 0;
	
	while(flag)
	{
		write(fd, qBuff, 4);
		usleep(200000);
		i = 0;
		while(1)
		{
			n = read(fd, buff+i, 1);
			printf(" %d",n);
			if(0==n)
			{
				break;
			}
			i++;
			if(i>90)
			{
				printf("rcv data over long:%d",i);
				return 0;
			}
		}
		printf("\r\nrcv %d:",i);
		DisDataHex(buff,i);	
		if(i!=0)
		{
			if(buff[2]==(i-4) && buff[2]>2)
			{
				cm = CheckSum(buff,i-1);
				if(buff[i-1]==cm && buff[1] == 0xE3)
				{
					printf("\r\nrcv A11 OK!");
					memcpy(rBuff,wBuff,13);
					memcpy(rBuff+13,buff,i);
					return 13+i;
				}
			}
		}
		errNum++;
		if(errNum>5)
		{
			printf("\r\nA11 rcv fault!");
			flag = 0;
			break;
		}
		sleep(1);
	}
	return 0;
}

/******************************************************************************************************************
* Function Name  : sendData
* Description    : 向上位机发送数据
* Input          : None
* Return         : @-1 没有接收命令
                   @0  发送成功
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
int sendData(void)
{
	FILE *filp = NULL;
	char   filePath[100]="/home/dataSave/";
	char   filePath1[100];
	int    fd_file;
	int nread;
	char buff[512];
	char nameBuff[100];
	char fbuff[1024];
	char p[100]="1--------";
	char otFlag = 0;
	struct tm *tm_p;
	time_t timep;
	//char *dev ="/dev/ttySAC1"; 

	int n=0,ret;
	long i=0, j=0;
	long offset = 0;
    const char* dev   = NULL;
	int yy,mm,nn;

	dev = "/dev/ttymxc4";
	

	
	/*  打开串口 */
	SetPort(1);
	
	printf("serial init success!\n\r");
	while(read(fd, buff, 1));
	printf("等待命令!\n\r");
	i = 0;
	
	while(1)
	{
		buff[0] = 0;
		buff[1] = 0;
		i=0;
		do
		{
			n = read(fd, buff+i, 1);
			i++;
			if(i>2) break;
		}while(n);
		printf("cmd:0x%02X,0x%02X %d\r\n",buff[0],buff[1],n);
		if(buff[0]==0x55 && buff[1]==0xAA)
		{
			break;
		}
		j++;
		if(j>4)
		{
			return -1;
		}
		
		usleep(100000);
	}
	memset(buff,0,sizeof(buff));

	printf("正在查询：\n\r");
    while(1)
    {
		time(&timep);
		tm_p = localtime(&timep);
		
		for(yy=2018; yy<=(1900 + tm_p->tm_year); yy++)
		{
			for(mm=1; mm <= 12; mm++)
			{
				for(nn=1; nn <= 31; nn++)
				{
					memset(filePath1, 0, sizeof(filePath1));
					sprintf(nameBuff, "%d-%d-%d.txt",  yy, mm, nn); 	
					strcat(filePath1,filePath);
					strcat(filePath1,nameBuff);
					//printf("open file path:%s",filePath1);
					
					//第一步打开文件
					filp = fopen(filePath1,"r");
					if(filp==NULL)
					{
						//printf("->not\n\r");
						continue;
					}
					fseek(filp, 0, SEEK_END);    //将文件的位置指针放在最后
					long len = ftell(filp);    //得到文件中所有文本的长度
					rewind(filp);     //重新恢复位置指针的位置，回到文件的开头
					offset = 0;
					printf("sending file path:%s\r\n",filePath1);
					for (i = 0; i < len; i++) //完成对文件的发送
					{
						printf(".");
						fflush(stdout);
						memset(fbuff, 0, sizeof(fbuff));
						ret = fread(fbuff, 1, 200, filp);  //先读取100个数据
						//printf("fread: %d %d\n\r",ret,i); //
						//printf("fread %d:%s\n\r",ret, fbuff);
						
						if(ret==200) 
						{
							int j = 0;
							char a = 0,b = 0;
							char flag = 0;
							for(n=0; n<200; n++)
							{
								if(fbuff[n]=='\n')
								{
									n++;
									yy = (fbuff[n]-0x30)*1000 + (fbuff[n+1]-0x30)*100 + (fbuff[n+2]-0x30)*10 + (fbuff[n+3]-0x30);
									buff[0] = (char)((yy>>8) & 0xFF);
									buff[1] = (char)(yy & 0xFF);
									buff[2] = (fbuff[n+5]-0x30)*10 + fbuff[n+6]-0x30;
									buff[3] = (fbuff[n+8]-0x30)*10 + fbuff[n+9]-0x30;
									buff[4] = (fbuff[n+11]-0x30)*10 + fbuff[n+12]-0x30;
									buff[5] = (fbuff[n+14]-0x30)*10 + fbuff[n+15]-0x30;
									buff[6] = (fbuff[n+17]-0x30)*10 + fbuff[n+18]-0x30;
									buff[7] = fbuff[n+19]; //add '#'
									n+=19;
									n++;  //下个数据
									j=8;
									//printf("\n\rfbuff[%d]:%c fbuff[%d]:%c",n,fbuff[n],n+1,fbuff[n+1]);
									break;
								}
							}
							if(n>=200) break; 
							
							for(; n<200; n++) //在一千个数据里查找一帧
							{
								//printf("fbuff[%d]:%c\n\r",n,fbuff[n]);
								if(0==flag)
								{
									if('0'<=fbuff[n] && fbuff[n]<='9')
									{
										a = (fbuff[n] - '0');
										flag++;
									}
									else if('a'<=fbuff[n] && fbuff[n]<='f')
									{
										a = (fbuff[n] - 'a'+10);
										flag++;
									}
									else if('A'<=fbuff[n] && fbuff[n]<='F')
									{
										a = (fbuff[n] - 'A'+10);
										flag++;
									}
								}
								else if(1==flag)
								{
									if('0'<=fbuff[n] && fbuff[n]<='9')
									{
										b = (fbuff[n] - '0');
										flag++;
									}
									else if('a'<=fbuff[n] && fbuff[n]<='f')
									{
										b = (fbuff[n] - 'a'+10);
										flag++;
									}
									else if('A'<=fbuff[n] && fbuff[n]<='F')
									{
										b = (fbuff[n] - 'A'+10);
										flag++;
									}
									else
									{
										flag = 0;
									}
									
								}
								if(2==flag)
								{
									if(n<2)
									{
										if(fbuff[n+1]==' ' || fbuff[n+1]=='\r' || fbuff[n+1]=='\n')
										{
											buff[j] = (a<<4) | b;
											j++;
										}
									}
									else
									{
										if((fbuff[n-2]==' ' || fbuff[n-2]=='#') && (fbuff[n+1]==' ' || fbuff[n+1]=='\r' || fbuff[n+1]=='\n'))
										{
											buff[j] = (a<<4) | b;
											j++;
										}
									}
									flag = 0;
								}
								
								if(j>0 && (fbuff[n]=='\r' || fbuff[n]=='\n')) //一帧完成
								{
									write(fd, buff, j);
									//printf("send num:%d\n\r",n);
									
									usleep(100000);
									
									break;
								}
							}
							offset += n;
							fseek(filp,offset,SEEK_SET);
							//printf("1000 offset:%d\n\r",offset);
						}
						else if(0<ret && ret < 200)
						{
							int lastLen = strlen(fbuff);

							int j = 0;
							char a = 0,b = 0;
							char flag = 0;
							for(n=0; n<lastLen; n++)
							{
								if(fbuff[n]=='\n')
								{
									n++;
									yy = (fbuff[n]-0x30)*1000 + (fbuff[n+1]-0x30)*100 + (fbuff[n+2]-0x30)*10 + (fbuff[n+3]-0x30);
									buff[0] = (char)((yy>>8) & 0xFF);
									buff[1] = (char)(yy & 0xFF);
									buff[2] = (fbuff[n+5]-0x30)*10 + fbuff[n+6]-0x30;
									buff[3] = (fbuff[n+8]-0x30)*10 + fbuff[n+9]-0x30;
									buff[4] = (fbuff[n+11]-0x30)*10 + fbuff[n+12]-0x30;
									buff[5] = (fbuff[n+14]-0x30)*10 + fbuff[n+15]-0x30;
									buff[6] = (fbuff[n+17]-0x30)*10 + fbuff[n+18]-0x30;
									buff[7] = fbuff[n+19]; //add '#'
									n+=19;
									n++;  //下个数据
									j=8;
									break;
								}
							}
							if(n>=lastLen) break; 
							
							for(; n<lastLen; n++) //在一百个数据里查找一帧
							{
								if(0==flag)
								{
									if('0'<=fbuff[n] && fbuff[n]<='9')
									{
										a = (fbuff[n] - '0');
										flag++;
									}
									else if('a'<=fbuff[n] && fbuff[n]<='f')
									{
										a = (fbuff[n] - 'a'+10);
										flag++;
									}
									else if('A'<=fbuff[n] && fbuff[n]<='F')
									{
										a = (fbuff[n] - 'A'+10);
										flag++;
									}
								}
								else if(1==flag)
								{
									if('0'<=fbuff[n] && fbuff[n]<='9')
									{
										b = (fbuff[n] - '0');
										flag++;
									}
									else if('a'<=fbuff[n] && fbuff[n]<='f')
									{
										b = (fbuff[n] - 'a'+10);
										flag++;
									}
									else if('A'<=fbuff[n] && fbuff[n]<='F')
									{
										b = (fbuff[n] - 'A'+10);
										flag++;
									}
									else
									{
										flag = 0;
									}
									
								}
								if(2==flag)
								{
									if(n<2)
									{
										if(fbuff[n+1]==' ' || fbuff[n+1]=='\r' || fbuff[n+1]=='\n')
										{
											buff[j] = (a<<4) | b;
											j++;
										}
									}
									else
									{
										if((fbuff[n-2]==' ' || fbuff[n-2]=='#') && (fbuff[n+1]==' ' || fbuff[n+1]=='\r' || fbuff[n+1]=='\n'))
										{
											buff[j] = (a<<4) | b;
											j++;
										}
									}
									flag = 0;
								}
								
								if((j>0 && (fbuff[n]=='\r' || fbuff[n]=='\n'))||(n==(lastLen-1))) //一帧完成
								{
									write(fd, buff, j);
									//printf("last num:%d \n\r",n);
									j = 0;
									flag = 0;
									usleep(100000);
								}
							}
							usleep(100000);
							//printf("send finish once read:%d!\n\r",n);
							break;
						}
					}
					fclose(filp);
					filp = NULL;
					printf("\r\ncompleted %s send!",filePath1);
				}//dd
			}//mm
		}//yy
		close(fd);
		printf("\r\n");
		return 0;
    }//while(1)
}

/******************************************************************************************************************
* Function Name  : LedFlash
* Description    : 200ms\800ms闪烁一次LED
* Input          : None
* Return         : None
* Author&Version : 2015-07-02 by www;
******************************************************************************************************************/
void *LedFlash(void* x)
{
	//sysLed -> GPIO1_10
	//usb    -> GPIO1_12
	//net    -> GPIO1_15

	while(1)
	{
		if(ledMod==0)
		{
			usleep(850000);
			system("echo 0 > /sys/class/gpio/gpio10/value"); 
			usleep(150000);
			system("echo 1 > /sys/class/gpio/gpio10/value"); 
		}
		else
		{
			usleep(150000);
			system("echo 0 > /sys/class/gpio/gpio10/value"); 
			usleep(850000);
			system("echo 1 > /sys/class/gpio/gpio10/value"); 
		}
	}
	return NULL;
}
