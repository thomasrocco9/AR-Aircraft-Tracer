#include <stdio.h>
#include <stdint.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>

#pragma comment (lib, "Ws2_32.lib")

#define BROADCAST_PORT 21221 
#define _USE_MATH_DEFINES // For MSVC
#include <math.h>

#include "json.h"

#include "json.cpp"

#define array_count(a) (sizeof(a) / sizeof((a)[0]))

inline float to_radians(float degrees) {
	float radians = degrees * (M_PI / 180.0f);
	return radians;
}

inline float lerp(float a, float b, float t) {
	float result = (1.0f - t) * a + t * b;
	return result;
}

struct Aircraft_State {
	uint32_t icao24; // icao24 is used as a hash
	float    longitude;
	float    latitude;
	float	 altitude;
};

#define MAX_STATES 10000

#define SEND_TIME_THRESHOLD 10.0f

// Hashed by icao24;
static Aircraft_State* get_state(Aircraft_State* states, int hash) {
	Aircraft_State* state = NULL;

	for (int i = 0; i < MAX_STATES; ++i) {
		int state_idx = hash % MAX_STATES;
		state = states + state_idx;
		if (state->icao24) {
			if (state->icao24 == hash) {
				break;
			}
		}
		else {
			state->icao24 = hash;
			break;
		}

		++hash;
	}

	return state;
}

enum Request_Context_State {
	Request_Context_State_None,
	Request_Context_State_Send,
	Request_Context_State_Send_Completed,
	Request_Context_State_Read,
	Request_Context_State_Read_Completed,
	Request_Context_State_Parse,
	Request_Context_State_Copy,
};

#define READ_BUF_SIZE BUFSIZ

struct Request_Context {
	HINTERNET             http_file;
	Request_Context_State state;

	String json;

	DWORD bytes_read;
	char* read_buf;
};

void update_json(Request_Context* ctx) {
	char* too_many_requests = "Too many requests";
	if (ctx->bytes_read == 0 || strncmp(ctx->read_buf, too_many_requests, sizeof(too_many_requests)) == 0) {
		ctx->state = Request_Context_State_Parse;
	}
	else {
		if (ctx->json.len == 0) {
			ctx->json.buf = (char*)malloc(ctx->bytes_read);
		}
		else {
			ctx->json.buf = (char*)realloc(ctx->json.buf, ctx->json.len + ctx->bytes_read);
		}

		memcpy(ctx->json.buf + ctx->json.len, ctx->read_buf, ctx->bytes_read);
		ctx->json.len += ctx->bytes_read;
	}
}

void send_request(Request_Context* ctx) {
	if (!HttpSendRequest(ctx->http_file, NULL, 0, 0, 0)) {
		if (GetLastError() == ERROR_IO_PENDING) {
			ctx->state = Request_Context_State_Send_Completed;
		}
		else {
			return;
		}
	}
}

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
			ctx->state = Request_Context_State_Read; // Keep reading and overwrite update_json() decided state

			break;

		case Request_Context_State_Send_Completed:
			if (async_result->dwResult) {
				ctx->state = Request_Context_State_Read;
			}
			else {
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

#define WORK_QUEUE_CALLBACK(name) void (name)(void *user_params, int idx)
typedef WORK_QUEUE_CALLBACK(*Work_Queue_Callback);

struct Work_Queue_Entry {
	Work_Queue_Callback callback;
	void* user_params;
};

// Single producer multiple consumer
struct Work_Queue {
	volatile uint32_t front, back;
	volatile uint32_t num_entries;
	Work_Queue_Entry  entries[256];

	HANDLE semaphore;
};

bool do_work(Work_Queue* queue, int thread_idx) {
	bool did_work = false;

	int expected_front = queue->front;
	int new_front = (queue->front + 1) % array_count(queue->entries);
	if (expected_front != queue->back) {
		int front = InterlockedCompareExchange(&queue->front, new_front, expected_front);
		if (front == expected_front) {
			Work_Queue_Entry* entry = queue->entries + front;
			entry->callback(entry->user_params, thread_idx);
			InterlockedDecrement(&queue->num_entries);
			did_work = true;
		}
	}

	return did_work;
}

// Should only be called from main thread
void push_work(Work_Queue* queue, Work_Queue_Callback callback, void* user_params) {
	if (queue->num_entries < array_count(queue->entries)) {
		Work_Queue_Entry* entry = queue->entries + queue->back;
		entry->callback = callback;
		entry->user_params = user_params;

		queue->back = (queue->back + 1) % array_count(queue->entries);

		InterlockedIncrement(&queue->num_entries);

		LONG prev_count;
		ReleaseSemaphore(queue->semaphore, 1, &prev_count);
	}
}

void work_until_done(Work_Queue* queue, int thread_idx) {
	while (queue->num_entries > 0) {
		do_work(queue, thread_idx);
	}
}

struct Thread_Info {
	int        idx;
	Work_Queue* queue;
};

static DWORD WINAPI thread_proc(LPVOID param) {
	Thread_Info* info = (Thread_Info*)param;

	DWORD id = GetCurrentThreadId();

	for (;;) {
		if (!do_work(info->queue, info->idx)) {
			WaitForSingleObject(info->queue->semaphore, INFINITE); // Put thread to sleep and wait for signal to wake up
		}
	}

	return 0;
}

struct Parse_JSON_Work {
	Request_Context* ctx;
	int             num_hashes;
	int* hashes;
	Aircraft_State* states;
};

WORK_QUEUE_CALLBACK(parse_json) {
	Parse_JSON_Work* work = (Parse_JSON_Work*)user_params;

	Lexer lex;
	init_lexer(&lex, work->ctx->json);

	begin_json_obj(&lex);
	for (String key = get_first_json_key(&lex); key.buf; key = get_next_json_key(&lex)) {
		if (strings_are_equal(key, "states")) {
			work->num_hashes = get_json_array_count(&lex);
			work->hashes = (int*)calloc(work->num_hashes, sizeof(int));

			begin_json_array(&lex);
			for (int hash_idx = 0; hash_idx < work->num_hashes; ++hash_idx) {
				int num_fields = get_json_array_count(&lex);

				Aircraft_State* state = NULL;

				begin_json_array(&lex);
				for (int field = 0; field < num_fields; ++field) {
					if (field == 0) {
						int hash = string_to_hex(get_json_string(&lex));
						state = get_state(work->states, hash);
						work->hashes[hash_idx] = hash;
					}
					else if (field == 5) {
						if (!get_json_null(&lex)) {
							state->longitude = get_json_float(&lex);
						}
					}
					else if (field == 6) {
						if (!get_json_null(&lex)) {
							state->latitude = get_json_float(&lex);
						}
					}
					else if (field == 7){
						if (!get_json_null(&lex)) {
							state->altitude  = get_json_float(&lex);
						}
					}
					else {
						skip_json_value(&lex);
					}

					next_json_array_entry(&lex);
				}

				next_json_array_entry(&lex);
			}
		} else {
			skip_json_value(&lex);
		}
	}
	work->ctx->state = Request_Context_State_Copy;
}

int main(int argc, char** argv) {
	Thread_Info thread_infos[3] = {};

	int num_threads = array_count(thread_infos);

	Work_Queue work_queue = {};
	work_queue.semaphore = CreateSemaphore(NULL, 0, num_threads, NULL);

	for (int i = 0; i < num_threads; ++i) {
		Thread_Info* info = thread_infos + i;
		info->idx = i + 1;
		info->queue = &work_queue;

		DWORD id;
		HANDLE handle = CreateThread(NULL, 0, thread_proc, info, 0, &id);
		CloseHandle(handle);
	}

	HINTERNET internet = InternetOpen("Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);
	if (!internet) {
		return 0;
	}

	INTERNET_STATUS_CALLBACK callback = InternetSetStatusCallback(internet, internet_status_callback);
	if (callback == INTERNET_INVALID_STATUS_CALLBACK) {
		return 0;
	}

	Request_Context ctx = {};
	ctx.read_buf = (char*)malloc(READ_BUF_SIZE);

	// Put username and password here for opensky-network.org
	char* username = "mlavelle";
	char* password = "XPlane123";
	HINTERNET connect = InternetConnect(internet, "opensky-network.org", INTERNET_DEFAULT_HTTPS_PORT, username, password, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)&ctx);
	if (!connect) {
		return 0;
	}

	ctx.http_file = HttpOpenRequest(connect, "GET", "/api/states/all?lamin=46.0000&lomin=-123.0000&lamax=46.5000&lomax=-122.5000", NULL, NULL, NULL, INTERNET_FLAG_SECURE, (DWORD_PTR)&ctx);
	if (!ctx.http_file) {
		return 0;
	}

	send_request(&ctx);

	int num_hashes = 0;
	int* hashes = NULL;

	Aircraft_State* prev_states = (Aircraft_State*)calloc(MAX_STATES, sizeof(Aircraft_State));
	Aircraft_State* curr_states = (Aircraft_State*)calloc(MAX_STATES, sizeof(Aircraft_State));

	float dt = 0.0f;
	float timer = 0.0f;

	Parse_JSON_Work work = {};

	uint32_t icao24 = 0;
	uint32_t saved_icao24 = 0;
	int hash_idx = 0;

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER prev_count;
	QueryPerformanceCounter(&prev_count);

	WSADATA wsaData;
	struct sockaddr_in outgoing;
	// Initialize Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		exit(0);
	}

	// Create UDP socket
	SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpSocket == SOCKET_ERROR) {
		WSACleanup();
		return 1;
	}
	BOOL bOptVal = TRUE;
	// Set socket options for broadcasting
	setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, sizeof(int));
	// Setup destination address
	outgoing.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	outgoing.sin_port = htons(BROADCAST_PORT);
	outgoing.sin_family = AF_INET;

	for (;;) {
		timer += dt;

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
				update_json(&ctx);
			}
		}

		if (ctx.state == Request_Context_State_Parse) {
			work.ctx = &ctx;
			work.states = (Aircraft_State*)calloc(MAX_STATES, sizeof(Aircraft_State));

			ctx.state = Request_Context_State_None;
			push_work(&work_queue, parse_json, &work);
		}

		if (ctx.state == Request_Context_State_Copy) {
			if (hashes) {
				free(hashes);
				hashes = NULL;
				num_hashes = 0;
			}

			num_hashes = work.num_hashes;
			hashes = (int*)malloc(num_hashes * sizeof(int));
			memcpy(hashes, work.hashes, num_hashes * sizeof(int));

			memcpy(prev_states, curr_states, MAX_STATES * sizeof(Aircraft_State));
			memcpy(curr_states, work.states, MAX_STATES * sizeof(Aircraft_State));

			free(work.hashes);
			work.hashes = NULL;
			work.num_hashes = 0;

			free(work.states);
			work.states = NULL;

			if (ctx.json.buf) {
				free(ctx.json.buf);
				ctx.json.buf = NULL;
				ctx.json.len = 0;
			}

			timer = 0.0f;

			ctx.state = Request_Context_State_Send;
		}

		if (ctx.state == Request_Context_State_Send) {
			if (timer >= SEND_TIME_THRESHOLD) {
				send_request(&ctx);
			}
		}

		SHORT left_key_state = GetAsyncKeyState(VK_LEFT);
		bool left_key_is_down = left_key_state & (1 << 15);
		bool left_key_was_pressed = left_key_state & (1 << 0);

		if (left_key_was_pressed && hash_idx > 0) {
			--hash_idx;
		}

		SHORT right_key_state = GetAsyncKeyState(VK_RIGHT);
		bool right_key_is_down = right_key_state & (1 << 15);
		bool right_key_was_pressed = right_key_state & (1 << 0);

		if (right_key_was_pressed && hash_idx < num_hashes) {
			++hash_idx;
		}

		SHORT space_key_state = GetAsyncKeyState(VK_SPACE);
		bool space_key_is_down = space_key_state & (1 << 15);
		bool space_key_was_pressed = space_key_state & (1 << 0);
		if (space_key_was_pressed) {
			// Toggle
			if (saved_icao24) {
				saved_icao24 = 0;
			}
			else {
				saved_icao24 = icao24;
			}
		}

		Aircraft_State prev = {};
		Aircraft_State curr = {};

		if (hashes) {
			icao24 = hashes[hash_idx];
			if (saved_icao24) {
				for (int i = 0; i < num_hashes; ++i) {
					if (hashes[i] == saved_icao24) {
						icao24 = hashes[i];
						break;
					}
				}
			}

			curr = *get_state(curr_states, icao24);
			prev = *get_state(prev_states, icao24);
		}
		
		float lng, lat, alt;

		for (int i = 0; i < num_hashes; i++) {
			char buffer[4096] = { 0 };
			uint32_t icao24 = hashes[i];
			Aircraft_State current = *get_state(curr_states, icao24);
			Aircraft_State previous = *get_state(prev_states, icao24);

			if (icao24) {
				float lng = lerp(previous.longitude, current.longitude, timer / SEND_TIME_THRESHOLD);
				float lat = lerp(previous.latitude, current.latitude, timer / SEND_TIME_THRESHOLD);
				float alt = lerp(previous.altitude, current.altitude, timer / SEND_TIME_THRESHOLD);

				snprintf(buffer, sizeof(buffer), "n%un192.168.1.112n%fn%fn%f\n", icao24, lat, lng, alt);
				sendto(udpSocket, buffer, strlen(buffer) + 1, 0, (struct sockaddr*)&outgoing, sizeof(struct sockaddr_in));
				// char temp[256];
				// int len = snprintf(temp, sizeof(temp), "n%un192.168.1.112n%fn%fn3050.43534\n", icao24, lat, lng);

				// if (strlen(buffer) + len < sizeof(buffer) - 1) {
				// 	strncat(buffer, temp, sizeof(buffer) - strlen(buffer) - 1);
				// }
				// else {
				// 	break;
				// }
			}
		}

		// sendto(udpSocket, buffer, strlen(buffer) + 1, 0, (struct sockaddr*)&outgoing, sizeof(struct sockaddr_in));
		printf("\rICAO24: %u, Prev: { %f, %f }, Curr: { %f, %f }, Lerp: { %f, %f }", icao24, prev.longitude, prev.latitude, curr.longitude, curr.latitude, lng, lat);
		fflush(stdout);

		LARGE_INTEGER curr_count;
		QueryPerformanceCounter(&curr_count);

		LARGE_INTEGER elapsed_count;
		elapsed_count.QuadPart = curr_count.QuadPart - prev_count.QuadPart;

		prev_count = curr_count;

		dt = (float)elapsed_count.QuadPart / frequency.QuadPart;
	}

	closesocket(udpSocket);
	WSACleanup();

#if 0
	free(hashes);
	free(curr_states);
	free(prev_states);
	free(ctx.json.buf);

	InternetCloseHandle(ctx.http_file);
	InternetCloseHandle(connect);
	InternetCloseHandle(internet);
#endif

	return 0;
}
