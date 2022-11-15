#include<stdio.h>
#include<stdlib.h>
#include<string.h>
// #include<strings.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include<time.h>

#define STRLEN 100
#define PORTLEN 28

void make_ip();
void openDataChannel();
void rcvData();
int handle_ls(char chaArg[]);
int rcvFile(int fsize, char* chaArg);
void sndFile(char* chaArg);
int handle_get(char chaArg[]);
int handle_put(char chaArg[]);
int handle_cd(char chaArg[]);
int handle_quit(char chaArg[]);
void processCommand();
void makeLogin();
int makeConnection(char* ip);
int getCode(char* message);

short cmdPort;
int cmdSock;
int dataSock;
char message[BUFSIZ];
struct _dataChannel{
	char ip[STRLEN];
	char chaIP[STRLEN];
	unsigned short port;
	char chaPort[STRLEN];
	int c_sock;
}dataChannel;

int hash = -1;

int main(int argc, char* argv[]){
	if(argc == 3){
		cmdPort = atoi(argv[2]);
	}else if(argc == 2){
		cmdPort = 21;
	}else{
		fprintf(stderr, "Usage : ftp server_address port_number\n");
		exit(-1);
	}

	cmdSock = makeConnection(argv[1]);
	makeLogin();
	while(1){
		processCommand();
	}
	close(cmdSock);
}

int makeConnection(char* ip){
	struct sockaddr_in s_addr;
	if((cmdSock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr, "socket open error\n");
		exit(-1);
	}
	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_addr.s_addr = inet_addr(ip);
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(cmdPort);

	if(connect(cmdSock, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0){
		fprintf(stderr, "connection error\n");
		exit(-1);
	}
	return cmdSock;
}

void makeLogin(){
	char *pUser, *pPass;
	int intCode;
	char chaCode[STRLEN];
	char sndBuf[BUFSIZ];
	int n;

	pUser = (char*)malloc(sizeof(char) * STRLEN);
	memset(pUser, 0, STRLEN);

	pPass = (char*)malloc(sizeof(char) * STRLEN);
	memset(pPass, 0, STRLEN);

	n = read(cmdSock, message, BUFSIZ);
	intCode = getCode(message);

	if(intCode == 220){
		printf("User : ");
		fgets(pUser, STRLEN, stdin);
		pUser[strlen(pUser) - 1] = '\0';
		sprintf(sndBuf, "USER %s\r\n", pUser);
		write(cmdSock, sndBuf, strlen(sndBuf));
	}else{
		fprintf(stderr, "login error");
		exit(-1);
	}
	n = read(cmdSock, message, BUFSIZ);
	intCode = getCode(message);
	if(intCode == 331){
		pPass = getpass("password : ");
		sprintf(sndBuf, "PASS %s\r\n", pPass);
		write(cmdSock, sndBuf, strlen(sndBuf));
	}else{
		fprintf(stderr, "passwd error");
		exit(-1);
	}
	n = read(cmdSock, message, BUFSIZ);
	intCode = getCode(message);

	if(intCode == 230){
		fprintf(stderr, "Login Success\n");
	}else{
		fprintf(stderr, "Login fail\n");
		exit(-1);
	}
	free(pUser);
	free(pPass);
}

void processCommand(){
	char command[BUFSIZ];
	char *pCommand;
	char chaCmd[STRLEN], chaArg[STRLEN];
	int nCmd;

	printf("ftp > ");
	fgets(command, BUFSIZ, stdin);

	if(!strcmp(command, "\n")){
		return;
	}
	if((pCommand = strchr(command, '\n')) != NULL){
		*pCommand = '\0';
	}
	if((pCommand = strchr(command, ' ')) == NULL){
		strcpy(chaCmd, command);
		strcpy(chaArg, "");
	}else{
		strncpy(chaCmd, command, pCommand - command);
		chaCmd[pCommand - command] = '\0';
		strcpy(chaArg, pCommand + 1);
	}

	if(!strcmp(chaCmd, "ls")){
		handle_ls(chaArg);
	}else if(!strcmp(chaCmd, "get")){
		handle_get(chaArg);
	}else if(!strcmp(chaCmd, "cd")){
		handle_cd(chaArg);
	}else if(!strcmp(chaCmd, "quit")){
		handle_quit(chaArg);
		exit(1);
	}else if(!strcmp(chaCmd, "put")){
		handle_put(chaArg);
	}else if(!strcmp(chaCmd, "hash")){
		hash = hash * -1;
		(hash > 0 ? printf("hash on\n") : printf("hash off\n"));
	}else{
		printf("command : ls|cd|get|put|hash|quit\n");
	}
}

int handle_quit(char chaArg[]){
	int intCode;
	int n;
	char chaCode[STRLEN];
	char sndBuf[BUFSIZ];
	char rcvBuf[BUFSIZ];

	sprintf(sndBuf, "QUIT %s\r\n", chaArg);
	n = write(cmdSock, sndBuf, strlen(sndBuf));

	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);

	close(cmdSock);
	return (intCode);
}

int handle_cd(char chaArg[]){
	int intCode;
	int n;
	char chaCode[STRLEN];
	char sndBuf[BUFSIZ];
	char rcvBuf[BUFSIZ];

	sprintf(sndBuf, "CWS %s\r\n", chaArg);
	n = write(cmdSock, sndBuf, strlen(sndBuf));

	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);
	return (intCode);
}

int handle_put(char chaArg[]){
	char sndBuf[BUFSIZ];
	char rcvBuf[BUFSIZ];
	char chaCode[STRLEN];
	int intCode;
	int n, cnt;
	int fsize;

	make_ip();

	sprintf(sndBuf, "PORT %s\r\n", dataChannel.chaPort);
	n = write(cmdSock, sndBuf, strlen(sndBuf));

	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);

	if(intCode != 200){
		return (intCode);
	}
	if(!strcmp(chaArg, "")){
		return -1;
	}else if(access(chaArg, F_OK) != 0){
		return -1;
	}
	sprintf(sndBuf, "STOR %s\r\n", chaArg);
	n = write(cmdSock, sndBuf, strlen(sndBuf));

	openDataChannel();

	memset(rcvBuf, 0, BUFSIZ);
	cnt = 0;
	while(1){
		n = read(cmdSock, rcvBuf + cnt, 1);
		if(*(rcvBuf + cnt) == '\n'){
			break;
			cnt++;
		}
	}
	intCode = getCode(rcvBuf);
	if(intCode == 150){
		sndFile(chaArg);
	}else{
		return (intCode);
	}
	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);

	return (intCode);
}

int handle_get(char chaArg[]){
	char sndBuf[BUFSIZ];
	char rcvBuf[BUFSIZ];
	char chaCode[STRLEN];
	int intCode;
	int n, cnt;
	int fsize;

	sprintf(sndBuf, "SIZE %s\r\n", chaArg);
	n = write(cmdSock, sndBuf, strlen(sndBuf));

	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);

	if(intCode == 213){
		fsize = atoi(rcvBuf + 3);
	}else{
		return (intCode);
	}

	if(fsize == 0 || intCode == 550){
		fprintf(stderr, "File error\n");
		return (intCode);
	}

	make_ip();

	sprintf(sndBuf, "PORT %s\r\n", dataChannel.chaPort);
	n = write(cmdSock, sndBuf, strlen(sndBuf));

	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);

	sprintf(sndBuf, "RETR %s\r\n", chaArg);
	n = write(cmdSock, sndBuf, strlen(sndBuf));

	openDataChannel();

	memset(rcvBuf, 0, BUFSIZ);

	cnt = 0;
	while(1){
		n = read(cmdSock, rcvBuf + cnt, 1);
		if(*(rcvBuf + cnt) == '\n'){
			break;
		}
		cnt++;
	}
	intCode = getCode(rcvBuf);

	if(intCode == 150){
		rcvFile(fsize, chaArg);
	}else{
		return (intCode);
	}

	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);

	return (intCode);
 }

void sndFile(char* chaArg){
	char sndBuf[BUFSIZ];
	FILE* fp;
	int n;

	fp = fopen(chaArg, "r");

	while((n = fread(sndBuf, sizeof(char), BUFSIZ, fp)) > 0){
		write(dataChannel.c_sock, sndBuf, n);
		if(hash > 0){
			printf("#");
			printf("\n");
		}
	}

	fclose(fp);
	close(dataChannel.c_sock);
 }


int rcvFile(int fsize, char* chaArg){
	char rcvBuf[BUFSIZ];
	FILE* fp;
	int nRecv = 0;
	int n;

	if((fp = fopen(chaArg, "w+")) == 0){
		return (-1);
	}

	while(nRecv < fsize){
		n = read(dataChannel.c_sock, rcvBuf, BUFSIZ);
		if(hash > 0){
			printf("#");
		}
		fwrite(rcvBuf, n, 1, fp);
		nRecv += n;
	}
	printf("\n");
	fclose(fp);
	close(dataChannel.c_sock);
 }

int handle_ls(char chaArg[]){
	char sndBuf[BUFSIZ];
	char rcvBuf[BUFSIZ];
	char chaCode[STRLEN];
	int intCode;
	int n, cnt;

	make_ip();

	sprintf(sndBuf, "PORT %s\r\n", dataChannel.chaPort);
	printf("%s", sndBuf);
	n = write(cmdSock, sndBuf, strlen(sndBuf));

	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);
	printf("%d", intCode);
	if(intCode == 200){
		sprintf(sndBuf, "LIST %s\r\n", chaArg);
		write(cmdSock, sndBuf, strlen(sndBuf));
		openDataChannel();
	}else{
		return intCode;
	}

	memset(rcvBuf, 0, BUFSIZ);
	cnt = 0;
	while(1){
		n = read(cmdSock, rcvBuf + cnt, 1);
		if(*(rcvBuf + cnt) == '\n'){
			break;
		}
		cnt++;
	}
	intCode = getCode(rcvBuf);

	if(intCode == 150){
		rcvData();
	}else{
		return intCode;
	}

	memset(rcvBuf, 0, BUFSIZ);
	n = read(cmdSock, rcvBuf, BUFSIZ);
	intCode = getCode(rcvBuf);
	return intCode;
}

void rcvData(){
	int n = 0;
	char rcvBuf[BUFSIZ] = {0, };

	memset(rcvBuf, 0, BUFSIZ);
	while((n = read(dataChannel.c_sock, rcvBuf, BUFSIZ)) > 0){
		printf("%s", rcvBuf);
		memset(rcvBuf, 0, BUFSIZ);
	}
	close(dataChannel.c_sock);
}

void openDataChannel(){
	int option = 8;
	int optlen;
	struct sockaddr_in data_addr, c_addr;
	int len;

	if((dataSock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr, "data socket open error\n");
		return;
	}
	optlen = sizeof(option);
	setsockopt(dataSock, SOL_SOCKET, SO_REUSEADDR, &option, optlen);

	memset(&data_addr, 0, sizeof(data_addr));
	data_addr.sin_family = AF_INET;
	data_addr.sin_addr.s_addr = inet_addr(dataChannel.ip);
	data_addr.sin_port = htons(dataChannel.port);

	if(bind(dataSock, (struct sockaddr*)&data_addr, sizeof(data_addr)) == -1){
		fprintf(stderr, "bind error\n");
		return;
	}

	if(listen(dataSock, 1) == -1){
		fprintf(stderr, "listen error\n");
		return;
	}

	len = sizeof(c_addr);
	if((dataChannel.c_sock = accept(dataSock, (struct sockaddr*)&c_addr, &len))== -1){
		fprintf(stderr, "accept error\n");
		return;
	}
}

void make_ip(){
	unsigned short np1, np2, np3;
	char chaPort1[PORTLEN], chaPort2[PORTLEN];
	const int MAX_NIC = 10;
	const char *localip = "127.0.0.1";
	struct ifconf ifc;
	struct ifreq ifr[MAX_NIC];
	int nIF;
	int s;
	int cmd = SIOCGIFCONF;
	char ip[PORTLEN], chaIP[PORTLEN];
	int i;
	struct in_addr addr;

	ifc.ifc_len = sizeof(ifr);
	ifc.ifc_ifcu.ifcu_req = ifr;
	if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr, "socket open error");
	}
	if(ioctl(s, cmd, &ifc) < 0){
		fprintf(stderr, "IOCTL error");
	}
	close(s);

	nIF = ifc.ifc_len / sizeof(struct ifreq);
	strcpy(ip, localip);

	for(i = 0; i < nIF; i++){
		if(ifc.ifc_ifcu.ifcu_req[i].ifr_ifru.ifru_addr.sa_family != AF_INET){
			continue;
		}
		addr = ((struct sockaddr_in*)&ifc.ifc_ifcu.ifcu_req[i].ifr_ifru.ifru_addr)->sin_addr;
		if(addr.s_addr == htonl(0x7f000001)){
			continue;
		}
		strcpy(ip, inet_ntoa(addr));
	}
	strcpy(dataChannel.ip, ip);

	i = 0;
	while(*(ip + i) != '\0'){
		if(*(ip + i) == '.'){
			chaIP[i] = ',';
		}else{
			chaIP[i] = *(ip + i);
		}
		i++;
	}
	chaIP[i] = '\0';
	strcpy(ip, chaIP);
	strcpy(dataChannel.chaIP, ip);

	np1 = np2 = np3 = 0;

	srand(rand()%(time(NULL)));

	np1 = (random() % 248) + 7;
	np2 = random() % 255;
	np3 = (np1 * 256) + np2;

	dataChannel.port = np3;
	sprintf(chaPort1, ",%d", np1);
	sprintf(chaPort2, ",%d", np2);

	strcat(chaPort1, chaPort2);
	strcat(ip, chaPort1);
	strcpy(dataChannel.chaPort, ip);
}

int getCode(char* message){
	char chaCode[5];

	strncpy(chaCode, message, 3);
	strcat(chaCode, " ");
	chaCode[4] = '\0';
	return (atoi(chaCode));
}
