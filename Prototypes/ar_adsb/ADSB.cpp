#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <wininet.h>
#include "stringlib.h"
#include <iostream>
#include <fstream>

#define _USE_MATH_DEFINES
#define READ_BUF_SIZE BUFSIZ

enum Request_Context_State {
	Request_Context_State_Send,
	Request_Context_State_Send_Completed,
	Request_Context_State_Read,
	Request_Context_State_Read_Completed,
};

struct Request_Context {
	HINTERNET             http_file;
	Request_Context_State state;

	String json;

	DWORD bytes_read;
	char* read_buf;
};

void update_json(Request_Context* ctx) {
	char* too_many_requests = "Too many requests";
		if (ctx->json.len == 0) {
			ctx->json.buf = (char*)malloc(ctx->bytes_read);
		}
		else {
			ctx->json.buf = (char*)realloc(ctx->json.buf, ctx->json.len + ctx->bytes_read);
		}
		memcpy(ctx->json.buf + ctx->json.len, ctx->read_buf, ctx->bytes_read);
		ctx->json.len += ctx->bytes_read;
}

// DWORD = unsigned int (0 to 4,294,967,295)
// DWORD_PTR = unsigned int pointer
// LPVOID = pointer to a void object
// HINTERNET = WinINet struct to connect to internet
// Internet_Status_Callback = event tracking that sends data for every status change

void internet_status_callback(HINTERNET internet, DWORD_PTR user_data, DWORD status, LPVOID status_info, DWORD status_info_len) {
	Request_Context* ctx = (Request_Context*)user_data;

	INTERNET_ASYNC_RESULT* async_result = (INTERNET_ASYNC_RESULT*)status_info;

	switch (status) {
		case INTERNET_STATUS_REQUEST_COMPLETE:
			switch (ctx->state) {
				case Request_Context_State_Read_Completed:
					if (!async_result->dwResult) {
						DWORD error = async_result->dwError;
						break;
					}
					update_json(ctx);
					ctx->state = Request_Context_State_Read;

					break;

				case Request_Context_State_Send_Completed:
					if (async_result->dwResult) {
						ctx->state = Request_Context_State_Read;
					} else {
						DWORD error = async_result->dwError;
					}
					break;

				default:
					break;
			}
			break;
		
		default:
			break;
	}
}

void send_request(Request_Context* ctx) {
	if (!HttpSendRequest(ctx->http_file, NULL, 0, 0, 0)) {
		if (GetLastError() == ERROR_IO_PENDING) {
			ctx->state = Request_Context_State_Send_Completed;
		} else {
			return;
		}
	}
}


int main(int argc, char** argv) {

	HINTERNET internet = InternetOpen("Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);
	if (!internet) {
		printf("No Internet...\n");
		return 0;
	}
	else {
		printf("Connected to Browser...\n");
	}

	INTERNET_STATUS_CALLBACK callback = InternetSetStatusCallback(internet, internet_status_callback);
	if (callback == INTERNET_INVALID_STATUS_CALLBACK) {
		printf("Invalid Internet Callback...\n");
		return 0;
	}
	else {
		printf("Internet Callback Received...\n");
	}

	Request_Context ctx = {};
	ctx.read_buf = (char*)malloc(READ_BUF_SIZE);

	// Put username and password here for opensky-network.org
	char* username = NULL;
	char* password = NULL;
	HINTERNET connect = InternetConnect(internet, "opensky-network.org", INTERNET_DEFAULT_HTTPS_PORT, username, password, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)&ctx);
	if (!connect) {
		printf("Not Connected to OpenSky Network...\n");
		return 0;
	}
	else {
		printf("Connected to OpenSky Network...\n");
	}



	ctx.http_file = HttpOpenRequest(connect, "GET", "/api/states/all", NULL, NULL, NULL, INTERNET_FLAG_SECURE, (DWORD_PTR)&ctx);
	if (!ctx.http_file) {
		printf("There is no file...\n");
		return 0;
	}
	else {
		printf("Received JSON File...\n");
	}

	send_request(&ctx);

	bool running = true;
	while (running) {
		while (ctx.state == Request_Context_State_Read) {
			if (!InternetReadFile(ctx.http_file, ctx.read_buf, READ_BUF_SIZE, &ctx.bytes_read)) {
				int error = GetLastError();
				if (error == ERROR_IO_PENDING) {
					ctx.state = Request_Context_State_Read_Completed;
				}
				else {
					return 0;
				}
			}
			else {
				if (ctx.bytes_read == 0) {
					running = false;
					break;
				}
				else {
					update_json(&ctx);
				}
			}
		}
		if (running == false) {
			break;
		}
	}
	printf("File Created...\n");
	FILE* file = NULL;
	file = fopen("jsonTest.txt", "w");
	if (file) {
		fprintf(file, "%.*s\n", ctx.json.len, ctx.json.buf);
	}
	fclose(file);
	printf("File Printed.\n");
}