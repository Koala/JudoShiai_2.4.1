/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <emscripten.h>

#include "judoinfo.h"
#include "comm.h"

extern time_t next_update;
extern int configured_tatamis;

/* System-dependent definitions */
#ifndef WIN32
#define closesocket     close
#define SOCKET  int
#define INVALID_SOCKET  -1
#define SOCKET_ERROR    -1
#define SOCKADDR_BTH struct sockaddr_rc
#define AF_BTH PF_BLUETOOTH
#define BTHPROTO_RFCOMM BTPROTO_RFCOMM
#endif

void send_packet(struct message *msg);

time_t traffic_last_rec_time;

void copy_packet(struct message *msg)
{
    /* dummy function to make linker happy (defined in comm.c) */
}

gint timeout_callback(gpointer data)
{
    struct message msg;
    extern gint my_address;

    msg.type = MSG_DUMMY;
    msg.sender = my_address;
    msg.u.dummy.application_type = application_type();
    send_packet(&msg);

    return TRUE;
}

#define ASK_TABLE_LEN 1024
static gint ask_table[ASK_TABLE_LEN];
static gint put_ptr = 0, get_ptr = 0;

extern void refresh_window(void);

void onload_name(void *arg, void *buf, int len)
{
    char b[128], *p = buf, *p1;
    int index, ix = (int)arg;
    char *last, *first, *club;
    struct msg_match_info msg;
    int n;

    strncpy(b, buf, sizeof(b)-1);
    index = atoi(p);
    p = strchr(buf, '\t');
    if (!p) return;
    *p = 0;
    last = p + 1;
    p = strchr(last, '\t');
    if (!p) return;
    *p = 0;
    first = p + 1;
    p = strchr(first, '\t');
    if (!p) return;
    *p = 0;
    club = p + 1;
    p = strchr(club, '\r');
    if (!p) p = strchr(club, '\n');
    if (!p) return;
    *p = 0;

    avl_set_data(index, first, last, club);
    next_update = time(NULL) + 1;
}

void onerror_name(void *a)
{
    printf("onerror_name %d\n", (int)a);
}

gint timeout_ask_for_data(gpointer data)
{
    char url[64];
    extern gint my_address;

    if (get_ptr != put_ptr) {
	snprintf(url, sizeof(url), "nameinfo?c=%d", ask_table[get_ptr]);

	emscripten_async_wget_data(url, (void *)ask_table[get_ptr],
				   onload_name, onerror_name);

        if (++get_ptr >= ASK_TABLE_LEN)
            get_ptr = 0;
    }
    return 0;
}

static void ask_for_data(gint index)
{
    ask_table[put_ptr++] = index;
    if (put_ptr >= ASK_TABLE_LEN)
        put_ptr = 0;
}

gboolean msg_accepted(struct message *m)
{
    switch (m->type) {
    case MSG_MATCH_INFO:
    case MSG_11_MATCH_INFO:
    case MSG_NAME_INFO:
    case MSG_CANCEL_REST_TIME:
        return TRUE;
    }
    return FALSE;
}

void handle_info_msg(struct msg_match_info *input_msg)
{
    gint tatami;
    gint position;
    struct name_data *j;

    tatami = input_msg->tatami - 1;
    position = input_msg->position;
    if (tatami < 0 || tatami >= NUM_TATAMIS)
	return;
    if (position < 0 || position >= NUM_LINES)
	return;

    match_list[tatami][position].category = input_msg->category;
    match_list[tatami][position].number   = input_msg->number;
    match_list[tatami][position].blue     = input_msg->blue;
    match_list[tatami][position].white    = input_msg->white;
    match_list[tatami][position].flags    = input_msg->flags;
    match_list[tatami][position].rest_end = input_msg->rest_time + time(NULL);

    /***
	g_print("match info %d:%d b=%d w=%d\n",
	match_list[tatami][position].category,
	match_list[tatami][position].number,
	match_list[tatami][position].blue,
	match_list[tatami][position].white);
    ***/
    j = avl_get_data(match_list[tatami][position].category);
    if (j == NULL) {
	ask_for_data(match_list[tatami][position].category);
    }

    j = avl_get_data(match_list[tatami][position].blue);
    if (j == NULL) {
	ask_for_data(match_list[tatami][position].blue);
    }

    if (position > 0) {
	j = avl_get_data(match_list[tatami][position].white);
	if (j == NULL) {
	    ask_for_data(match_list[tatami][position].white);
	}
    }

    //refresh_window();

    if (show_tatami[tatami] == FALSE &&
	configured_tatamis == 0 &&
	match_list[tatami][position].blue &&
	match_list[tatami][position].white)
	show_tatami[tatami] = TRUE;
}

void msg_received(struct message *input_msg)
{
    gint i;

    if (input_msg->sender < 10)
        return;

    traffic_last_rec_time = time(NULL);
#if 0
    g_print("msg type = %d from %d\n",
            input_msg->type, input_msg->sender);
#endif
    switch (input_msg->type) {
    case MSG_MATCH_INFO:
	handle_info_msg(&input_msg->u.match_info);
        break;

    case MSG_11_MATCH_INFO:
	for (i = 0; i < 11; i++) {
	    handle_info_msg(&input_msg->u.match_info_11.info[i]);
	}
        break;

    case MSG_NAME_INFO:
        avl_set_data(input_msg->u.name_info.index,
                     input_msg->u.name_info.first,
                     input_msg->u.name_info.last,
                     input_msg->u.name_info.club);
        //refresh_window();
#if 0
        g_print("name info %d: %s %s, %s\n",
                input_msg->u.name_info.index,
                input_msg->u.name_info.first,
                input_msg->u.name_info.last,
                input_msg->u.name_info.club);
#endif
        break;
#if 0
    case MSG_CANCEL_REST_TIME:
        for (tatami = 0; tatami < NUM_TATAMIS; tatami++) {
            for (position = 0; position < NUM_LINES; position++) {
                if (match_list[tatami][position].category == input_msg->u.cancel_rest_time.category &&
                    match_list[tatami][position].number == input_msg->u.cancel_rest_time.number) {
                    match_list[tatami][position].rest_end = 0;
                    match_list[tatami][position].flags = 0;
                    //refresh_window();
                    return;
                }
            }
        }
        break;
#endif
    }
}


void send_packet(struct message *msg)
{
#if 0
    if (msg->type == MSG_RESULT) {
        printf("tatami=%d cat=%d match=%d min=%d blue=%x white=%x bv=%d wv=%d\n",
               msg->u.result.tatami,
               msg->u.result.category,
               msg->u.result.match,
               msg->u.result.minutes,
               msg->u.result.blue_score,
               msg->u.result.white_score,
               msg->u.result.blue_vote,
               msg->u.result.white_vote);
    }
#endif

    msg_to_queue(msg);
}

gboolean keep_connection(void)
{
    return TRUE;
}

gint get_port(void)
{
    return SHIAI_PORT;
}
