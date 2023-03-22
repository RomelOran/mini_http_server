#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<ctype.h>
#include<arpa/inet.h>
#include<errno.h>
#include<sys/stat.h>


#define SERVER_PORT 80


int do_http_request(int client_sock);
int get_line(int sock, char *buffer, int size);
void do_http_response(int client_sock, char *path);
void do_http_not_found(int client_sock);
char* reply = "";
char* content = "";
void reply_http_status(int client_sock, FILE* resource);
void reply_client_html(int client_sock, FILE * resource);
void do_http_inner_error(int client_sock);


int main(void){
	int sock; 
	struct sockaddr_in server_addr;
	
	
	sock = socket(AF_INET, SOCK_STREAM, 0);//服务器sock
	bzero(&server_addr, sizeof(server_addr));

	//服务器IP和端口号监听
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	server_addr.sin_port = htons(SERVER_PORT);

	//绑定端口号
	bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));


	listen(sock, 64);
	printf("等待客户端的连接...\n");

	int done = 1;

	//一直监听
	while(done){
		//客户端sock
		int client_sock, len;
		struct sockaddr_in client_addr;
		char client_ip[64];
		char buffer[256];

		//accept
		socklen_t client_addr_len;
		client_addr_len = sizeof(client_addr);
		client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
		
		printf("client ip: %s\t port: %d\n",
			inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(client_ip)),
			ntohs(client_addr.sin_port));

		//请求和相应
		do_http_request(client_sock);
		
		close(client_sock);
	}
	
	close(sock);
	return 0;

}

//请求
int do_http_request(int client_sock) {
	int len = 0;
	char buffer[256];
	char method[16];
	char url[64];
	char http_v[16];
	char path[128];
	struct stat st;
	int server_status = 0;

	//先得到第一行
	len = get_line(client_sock, buffer, sizeof(buffer) - 1);

	if (len > 0) {
		int i = 0,j = 0;
		while (!isspace(buffer[i]) && j < (sizeof(method) - 1)) {
			method[j] = buffer[i];
			i++;
			j++;
		}
		if (strncasecmp(method, "GET", j) == 0) {
			method[j] = '\0';
			printf("method:%s\n", method);
		} else {
			method[j] = '\0';
			fprintf(stderr, "warning! other request[%s]", method);
		}
		j = 0;
		i++;
		//读入文件
		while (!isspace(buffer[i]) && j < (sizeof(url) - 1)) {
			url[j] = buffer[i];
			i++;
			j++;
		}
		char *findchr = strchr(url, '?');
		if (findchr != NULL) {
			*findchr = '\0';
			printf("url:%s\n", url);
		} else {
			url[j] = '\0';
			snprintf(path, 127, "/root/http_server%s",url);
			//执行HTTP响应，如果存在就做出相应， 发送指定的html文件， 不存在就返回404

			if (stat(path, &st) == -1) {
				
				server_status = 0;
				fprintf(stderr, "stat %s faild. reason:%s\n", path, strerror(errno));
				do_http_not_found(client_sock);
				

			}else{
				if (S_ISDIR(st.st_mode)) {
					strcat(path, "/index.html");
				}

				do_http_response(client_sock, path);
				server_status = 1;
				
			}


		}
		j = 0;
		i++;
		//读入http协议
		while (!isspace(buffer[i]) && j < (sizeof(http_v) - 1)) {
			http_v[j] = buffer[i];
			i++;
			j++;
		}
			http_v[j] = '\0';
			printf("http_v:%s\n", http_v);
		

	} else if (len < 0) {
		printf("\n");

	} else {

	}
	//后面的行
	while (len > 0){
		len = get_line(client_sock, buffer, sizeof(buffer) - 1);
		printf("%s\n", buffer);
	}
	return server_status;
}

void reply_http_status(int client_sock, FILE* resource) {
	reply = "HTTP/1.0 200 OK\nServer:Ubuntu Server\n\
				Content-Type:text/html\nConnection:Close\n";
	struct stat st;
	int fileid = 0;
	fileid = fileno(resource);
	
	if (fstat(fileid, &st) == -1) {
		do_http_inner_error(client_sock);
	}
	int len = write(client_sock, reply, strlen(reply));
	if (len > 0) { fprintf(stdout, "write[%d]:%s", len, reply); }
}
void reply_client_html(int client_sock, FILE *resource) {
	int content_length = 0;
	char send_buffer[64];
	char cont[128];
	while (!feof(resource)) {
		fgets(cont, sizeof(cont), resource);
		int cont_len = strlen(cont);
		int len = write(client_sock, cont, cont_len);
		if (len > 0) { fprintf(stdout, "write[%d]:%s", len, content); 
		printf("%s\n", cont);
		}

		
	}
	
	snprintf(send_buffer, 64, "Content_length:%d\r\n\r\n", content_length);
}

//做出相应
void do_http_response(int client_sock, char *path) {

	FILE* resource = NULL;
	resource = fopen(path, "r");


	if (resource == NULL) {

		do_http_not_found(client_sock);
	}
	
	reply_http_status(client_sock, resource);

	reply_client_html(client_sock, resource);

}

//读文件， 每次调用得到每一行
int get_line(int sock, char* buffer, int size) {
	int count = 0;
	int len = 0;
	int ch = '\0';

	//遇到换行或者超出界返回
	while (count < size && ch != '\n') {
		len = read(sock, &ch, 1);
		if (len == 1) {
			if (ch == '\r') {
				continue;
			} else if(ch == '\n') {
				buffer[count] = '\0';
				break;
			}
				buffer[count] = ch;
				count++;
		//没有读到
		} else if (len == -1) {
			perror("read error!\n");
			count = -1;
			break;
		} else {
			fprintf(stderr, "client close\n");
			count = -1; 
			break;
		}

	}
	return count;
}

//404页面
void do_http_not_found(int client_sock) {
	content = "<html><head>\
		<title>404 Not Found</title>\
		</head><body>\
		<h1>Not Found</h1>\
		<p>The requested URL / 404 / was not found on this server.</p>\
		<hr>\
		<address>Apache Server at rocketship.com.au Port 80 </address>\
		</body> </html>";
	int content_length = 0;
	char send_buffer[64];
	int len = write(client_sock, content, strlen(content));
	if (len > 0) { fprintf(stdout, "write[%d]:%s", len, content); }
	content_length = len;
	
	snprintf(send_buffer, 64, "Content_length:%d\r\n\r\n", content_length);
}

void do_http_inner_error(int client_sock) {
	content = "<html><head>\
		<title>502 服务器内部出错</title>\
		</head><body>\
		<h1>Not Found</h1>\
		<p>The requested URL / 404 / was not found on this server.</p>\
		<hr>\
		<address>Apache Server at rocketship.com.au Port 80 </address>\
		</body> </html>";

}
