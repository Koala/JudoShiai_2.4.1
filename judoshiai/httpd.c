/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2010 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#if defined(__WIN32__) || defined(WIN32)

#define  __USE_W32_SOCKETS
//#define Win32_Winsock

#include <windows.h>
#include <stdio.h>
#include <initguid.h>
#include <winsock2.h>

#else /* UNIX */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <fcntl.h>
#include <signal.h>

#endif /* WIN32 */

#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include "judoshiai.h"
#include "httpp.h"

void index_html(http_parser_t *parser, gchar *txt);

/* System-dependent definitions */
#ifndef WIN32
#define closesocket     close
#define SOCKET          int
#define INVALID_SOCKET  -1
#define SOCKET_ERROR    -1
#define SOCKADDR_BTH    struct sockaddr_rc
#define AF_BTH          PF_BLUETOOTH
#define BTHPROTO_RFCOMM BTPROTO_RFCOMM
#endif

int sigreceived = 0;

struct judoka unknown_judoka = {
    .index = 0,
    .last = "?",
    .first = "?",
    .club = "?",
    .country = "",
    .regcategory = "?",
    .category = "?",
    .id = ""
};

#define NUM_CONNECTIONS 16
static struct {
    guint fd;
    gulong addr;
    gboolean closed;
} connections[NUM_CONNECTIONS];

void sighandler(int sig)
{
    printf("sig received\n");
    sigreceived = 1;
}

static gint mysend(SOCKET s, gchar *p, gint len)
{
    gint n;
#if 0
    g_print("-->");
    for (n = 0; n < len; n++)
        g_print("%c", p[n]);
    g_print("<--\n");
#endif
    while (len > 0) {
        n = send(s, p, len, 0);
        if (n < 0) {
            g_print("mysend: cannot send (%d)\n", n);
            return n;
        }
        len -= n;
        p += n;
        if (len) {
            g_print("mysend: only %d bytes sent\n", n);
        }
    }
    return len;
}

gint sendf(SOCKET s, gchar *fmt, ...)
{
    gint ret;
    gchar *r = NULL;
    va_list ap;
    va_start(ap, fmt);
    gint n = g_vasprintf(&r, fmt, ap);
    va_end(ap);
    ret = mysend(s, r, n);
    g_free(r);
    return ret;
}

#define NUM_ADDRESSES 24
static gulong ipaddresses[NUM_ADDRESSES] = {0};

static void remove_accepted(gulong addr)
{
    gint i;
    for (i = 0; i < NUM_ADDRESSES; i++)
        if (ipaddresses[i] == addr)
            ipaddresses[i] = FALSE;
}

static void add_accepted(gulong addr)
{
    gint i;

    remove_accepted(addr);

    for (i = 0; i < NUM_ADDRESSES; i++)
        if (ipaddresses[i] == 0) {
            ipaddresses[i] = addr;
            return;
        }
    g_print("ERROR: No more space for accepted IP addresses!\n");
}

static gboolean is_accepted(gulong addr)
{
    gint i;
    if (webpwcrc32 == 0)
        return TRUE;
    for (i = 0; i < NUM_ADDRESSES; i++)
        if (ipaddresses[i] == addr) {
            return TRUE;
        }
    return FALSE;
}

#define HIDDEN(x) do {							\
        if (x) h_##x = x;                                               \
        if (h_##x) {                                                    \
            sendf(s, "<input type=\"hidden\" name=\"h_" #x "\" value=\"%s\">\r\n", h_##x); \
        }                                                               \
    } while (0)

void send_point_buttons(SOCKET s, gint catnum, gint matchnum, gboolean blue)
{
    gchar m[64];
    gchar c[64];

    sprintf(m, "M%c-%d-%d", blue?'B':'W', catnum, matchnum);
    if (blue)
        sprintf(c, "style=\"background:#0000ff none; color:#ffffff; \"");
    else
        sprintf(c, "style=\"background:#ffffff none; color:#000000; \"");

    sendf(s, 
          "<table><tr>"
          "<td><input type=\"submit\" name=\"%s-10\" value=\"10\" %s></td>"
          "<td><input type=\"submit\" name=\"%s-7\" value=\"7\" %s></td>"
          "<td><input type=\"submit\" name=\"%s-5\" value=\"5\" %s></td>"
          "<td><input type=\"submit\" name=\"%s-3\" value=\"3\" %s></td>"
          "<td><input type=\"submit\" name=\"%s-1\" value=\"1\" %s></td>"
          "</tr></table>",
          m, c, m, c, m, c, m, c, m, c);
}

void get_categories(http_parser_t *parser)
{
    SOCKET s = parser->sock;
    gchar buf[1024];
    gint row;

    gchar *tatami = NULL;
    gchar *h_tatami = NULL;
    gchar *id = NULL;
    const gchar *h_id = NULL;
    gchar *win = NULL;

    avl_node *node;
    http_var_t *var;
    node = avl_get_first(parser->queryvars);
    while (node) {
        var = (http_var_t *)node->key;
        if (var) {
            //printf("Query variable: '%s'='%s'\n", var->name, var->value);
            if (var->name[0] == 'X')
                id = var->name+1;
            else if (var->name[0] == 'M')
                win = var->name+1;
            if (!strcmp(var->name, "h_id"))
                h_id = var->value;
            else if (!strncmp(var->name, "tatami", 6))
                tatami = var->name+6;
            else if (!strcmp(var->name, "h_tatami"))
                h_tatami = var->value;
        }
        node = avl_get_next(node);
    }

    if (win && h_tatami) {
        gchar catbuf[64];
        gint cat, num, pts;
        gint t = atoi(h_tatami) - 1;
        sscanf(win+1, "-%d-%d-%d", &cat, &num, &pts);
        //g_print("%d %d %d\n", cat, num, pts);

        if (win[0] == 'R') {
            db_set_comment(next_matches_info[t][1].catnum,
                           next_matches_info[t][1].matchnum,
                           COMMENT_EMPTY);
            db_set_comment(next_matches_info[t][0].catnum,
                           next_matches_info[t][0].matchnum,
                           COMMENT_MATCH_2);
            db_set_comment(cat, num, COMMENT_MATCH_1);
        }

        struct message msg;
        memset(&msg, 0, sizeof(msg));

        msg.type = MSG_SET_POINTS;
        msg.u.set_points.category     = cat;
        msg.u.set_points.number       = num;
        msg.u.set_points.sys          = 0;
        msg.u.set_points.blue_points  = (win[0] == 'B') ? pts : 0;
        msg.u.set_points.white_points = (win[0] == 'W') ? pts : 0;

        struct message *msg2 = put_to_rec_queue(&msg);
        time_t start = time(NULL);
        while ((time(NULL) < start + 5) && (msg2->type != 0))
            g_usleep(10000);

        if (win[0] == 'R')
            next_matches_info[t][0].won_catnum = 0;

        sprintf(catbuf, "%d", cat);
        httpp_set_query_param(parser, "h_id", catbuf);
        h_id = httpp_get_query_param(parser, "h_id");
    }

    sendf(s, "HTTP/1.0 200 OK\r\n");
    sendf(s, "Content-Type: text/html\r\n");
    sendf(s, "\r\n");
    sendf(s, "<html><head><title>JudoShiai</title></head><body>\r\n");
    sendf(s, "<form name=\"catform\" action=\"categories\" method=\"get\">");

    HIDDEN(id);
    HIDDEN(tatami);

    gchar *style0 = "style=\"width:6em; \"";
    gchar *style1 = "style=\"width:6em; font-weight:bold; \"";

    sendf(s, "<table><tr>"
          "<td><input type=\"submit\" name=\"tatami1\" value=\"Tatami 1\" %s></td>\r\n"
          "<td><input type=\"submit\" name=\"tatami2\" value=\"Tatami 2\" %s></td>\r\n"
          "<td><input type=\"submit\" name=\"tatami3\" value=\"Tatami 3\" %s></td>\r\n"
          "<td><input type=\"submit\" name=\"tatami4\" value=\"Tatami 4\" %s></td>\r\n"
          "</tr></table>\r\n",
          (h_tatami && h_tatami[0]=='1') ? style1 : style0,
          (h_tatami && h_tatami[0]=='2') ? style1 : style0,
          (h_tatami && h_tatami[0]=='3') ? style1 : style0,
          (h_tatami && h_tatami[0]=='4') ? style1 : style0);

    gint numrows = 0;

    sendf(s, "<table border=\"1\" valign=\"top\">"
          "<tr><th>%s</th><th>%s</th><th>%s</th></tr>"
          "<tr><td valign=\"top\">\r\n", _("Matches"), _("Categories"), _("Sheet"));
	
    /* next matches */
    if (h_tatami) {
        gint i = atoi(h_tatami) - 1, k;
        gsize x;
        gchar *first = NULL, *last = NULL;
        gchar *xf, *xl, *xc;

        sendf(s, "<table border=\"0\">");

        if (next_matches_info[i][0].won_catnum) {
            xf = next_matches_info[i][0].won_first;
            xl = next_matches_info[i][0].won_last;
            xc = next_matches_info[i][0].won_cat;
        } else {
            xf = "";
            xl = "";
            xc = "";
        }

        first = g_convert(xf, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
        last = g_convert(xl, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
        sendf(s, "<tr><td>%s:<br>", _("Previous winner"));
        if (is_accepted(parser->address))
            sendf(s, "<input type=\"submit\" name=\"MR-%d-%d-0\" value=\"%s\">",
                  next_matches_info[i][0].won_catnum, next_matches_info[i][0].won_matchnum,
                  _("Cancel the match"));
        sendf(s, "</td><td>%s<br>%s %s<br>&nbsp;</td>", xc, first, last);
        g_free(first); first = NULL;
        g_free(last);  last  = NULL;

        g_static_mutex_lock(&next_match_mutex);
        struct match *m = get_cached_next_matches(i+1);

        for (k = 0; m[k].number != 1000 && k < NEXT_MATCH_NUM; k++) {
            gchar *bgcolor = (k & 1) ? "bgcolor=\"#a0a0a0\"" : "bgcolor=\"#f0f0f0\"";
            struct judoka *blue, *white, *cat;
            blue = get_data(m[k].blue);
            white = get_data(m[k].white);
            cat = get_data(m[k].category);

            if (!blue)
                blue = &unknown_judoka;

            if (!white)
                white = &unknown_judoka;

            if (!cat)
                cat = &unknown_judoka;

            gchar *b_first = g_convert(blue->first, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
            gchar *b_last = g_convert(blue->last, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
            gchar *b_club = g_convert(get_club_text(blue, 0), -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
            gchar *w_first = g_convert(white->first, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
            gchar *w_last = g_convert(white->last, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
            gchar *w_club = g_convert(get_club_text(white, 0), -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);

            sendf(s, "<tr %s><td><b>%s %d:</b></td><td><b>%s</b></td></tr>", 
                  bgcolor, _("Match"), k+1, cat->last);

            if (k == 0 && is_accepted(parser->address)) {
                sendf(s, "<tr %s><td>", bgcolor);
                send_point_buttons(s, m[k].category, m[k].number, TRUE);
                sendf(s, "</td><td>");
                send_point_buttons(s, m[k].category, m[k].number, FALSE);
                sendf(s, "</td></tr>");
            }

            sendf(s, "<tr %s><td>%s %s<br>%s<br>&nbsp;</td><td>%s %s<br>%s<br>&nbsp;</td></tr>", 
                  bgcolor, b_first, b_last, get_club_text(blue, 0), 
		  w_first, w_last, get_club_text(white, 0));
			
            g_free(b_first);
            g_free(b_last);
            g_free(w_first);
            g_free(w_last);
            g_free(b_club);
            g_free(w_club);
            if (blue != &unknown_judoka)
                free_judoka(blue);
            if (white != &unknown_judoka)
                free_judoka(white);
            if (cat != &unknown_judoka)
                free_judoka(cat);
        }
        g_static_mutex_unlock(&next_match_mutex);
        sendf(s, "</table>\r\n");
    } else { //!h_tatami
        sendf(s, _("Select a tatami"));
    }

    sendf(s, "</td><td valign=\"top\">");

    /* category list */
    if (h_tatami) {
        sprintf(buf, "select * from categories where \"tatami\"=%s and "
                "\"deleted\"=0 order by \"category\" asc", 
                h_tatami);
        numrows = db_get_table(buf);

        if (numrows < 0)
            return;

        sendf(s, "<table>\r\n");

        for (row = 0; row < numrows; row++) {
            gchar *cat = db_get_data(row, "category");
            gchar *id1 = db_get_data(row, "index");
            sendf(s, "<tr><td><input type=\"submit\" name=\"X%s\" "
                  "value=\"%s\" %s></td></tr>\r\n",
                  id1, cat, (h_id && strcmp(h_id, id1)==0) ? style1 : style0);
        }
        sendf(s, "</table>\r\n");

        db_close_table();
    }
    sendf(s, "</td><td valign=\"top\">");

    /* picture */
    sendf(s, "<img src=\"sheet%s\">", h_id ? h_id : "0");
    sendf(s, "</td></tr></table></form></body></html>\r\n\r\n", "?");
}

static cairo_status_t write_to_http(void *closure, const unsigned char *data, unsigned int length)
{
    mysend((SOCKET)closure, (gchar *)data, length);
    return CAIRO_STATUS_SUCCESS;
}

void get_sheet(http_parser_t *parser)
{
    SOCKET s = parser->sock;
    gint catid = atoi(parser->uri + 6);

    sendf(s, "HTTP/1.0 200 OK\r\n");
    sendf(s, "Content-Type: image/png\r\n");
    sendf(s, "\r\n");
    write_sheet_to_stream(catid, write_to_http, (void *)s);
}

static gint pts2int(const gchar *p)
{
    gint r = 0;
    gint x = p[0] - 'A';

    if (x >= 2)
        r = 0x10000;
    else if (x)
        r = 0x01000;

    r |= ((p[1]-'A') << 8) | ((p[2]-'A') << 4) | (p[3]-'A');

    return r;
}

void check_password(http_parser_t *parser)
{
    SOCKET s = parser->sock;
    const gchar *auth = httpp_getvar(parser, "authorization");
    gboolean accepted = FALSE;
    static time_t last_time = 0;
    time_t now;
	
    now = time(NULL);
    if (now > last_time + 20)
        goto verdict;
	
    if (webpwcrc32 == 0) {
        accepted = TRUE;
        goto verdict;
    }

    if (!auth)
        goto verdict;

    gchar *p = strstr(auth, "Basic ");
    if (!p)
        goto verdict;

    p += 6;

    gchar n, bufin[64];
    gint cnt = 0, ch[4], eqcnt = 0, inpos = 0;
    while ((n = *p++) && inpos < sizeof(bufin) - 6) {
        if ((n >= 'a' && n <= 'z') || (n >= 'A' && n <= 'Z') || 
            (n >= '0' && n <= '9') || n == '+' || n == '/' || n == '=') {
            gint x;
            if (n >= 'a' && n <= 'z')
                x = n - 'a' + 26;
            else if (n >= 'A' && n <= 'Z')
                x = n - 'A';
            else if (n >= '0' && n <= '9')
                x = n - '0' + 52;
            else if (n == '+')
                x = 62;
            else if (n == '/')
                x = 63;
            else { /* n == '=' */
                x = 0;
                eqcnt++;
            }
            ch[cnt++] = x;
            if (cnt == 4) {
                bufin[inpos++] = (ch[0] << 2) | (ch[1] >> 4);
                if (eqcnt <= 1) {
                    bufin[inpos++] = (ch[1] << 4) | (ch[2] >> 2);
                }
                if (eqcnt == 0) {
                    bufin[inpos++] = (ch[2] << 6) | ch[3];
                }
                cnt = 0;
                eqcnt = 0;
            }
        }
    }
    bufin[inpos] = 0;

    p = strchr(bufin, ':');
    if (!p)
        goto verdict;

    *p = 0;
    p++;
    if (pwcrc32((guchar *)p, strlen(p)) == webpwcrc32)
        accepted = TRUE;

verdict:
    last_time = now;

    if (accepted) {
        add_accepted(parser->address);
        index_html(parser, _("Password accepted.<br>"));
#if 0
        sendf(s, "HTTP/1.0 200 OK\r\n");
        sendf(s, "Content-Type: text/html\r\n\r\n");
        sendf(s, "<html><head><title>JudoShiai</title></head><body>\r\n");
        sendf(s, "<p>%s: %s. %s OK. <a href=\"index.html\">%s.</a></p></body></html>\r\n\r\n", 
              _("User"), bufin, _("Password"), _("Back"));
#endif
    } else {
        remove_accepted(parser->address);
        //index_html(parser, "<b>Salasana virheellinen!</b><br>");
#if 1
        sendf(s, "HTTP/1.0 401 Unauthorized\r\n");
        sendf(s, "WWW-Authenticate: Basic realm=\"JudoShiai\"\r\n");
        sendf(s, "Content-Type: text/html\r\n\r\n");
        sendf(s, "<html><head><title>JudoShiai</title></head><body>\r\n");
        sendf(s, "<p>%s. <a href=\"index.html\">%s.</a></p></body></html>\r\n\r\n",
              _("Wrong password"), _("Back"));
#endif
    }
}

void logout(http_parser_t *parser)
{
    SOCKET s = parser->sock;
    remove_accepted(parser->address);
    index_html(parser, "");
    return;
    sendf(s, "HTTP/1.0 200 OK\r\n");
    sendf(s, "Content-Type: text/html\r\n\r\n");
    sendf(s, "<html><head><title>JudoShiai</title></head><body>\r\n");
    sendf(s, "<p>OK. <a href=\"index.html\">%s.</a></p></body></html>\r\n\r\n", _("Back"));
}

void index_html(http_parser_t *parser, gchar *txt)
{
    SOCKET s = parser->sock;

    sendf(s, "HTTP/1.0 200 OK\r\n");
    sendf(s, "Content-Type: text/html\r\n\r\n");
    sendf(s, "<html><head><title>JudoShiai</title></head><body>\r\n");
    sendf(s, "%s", txt);
    sendf(s, "<a href=\"categories\">%s</a><br>", _("Match controlling and browsing"));
    sendf(s, "<a href=\"competitors\">%s</a><br>", _("Competitors"));

    if (webpwcrc32 == 0)
        sendf(s, "<hr>%s<br>", _("Password not in use"));
    else if (is_accepted(parser->address))
        sendf(s, "<hr><a href=\"logout\">%s</a><br>", _("Logout"));
    else {
        sendf(s, "<hr><p>%s, %s. %s.",
              _("Login is required"), _("if you want to report match results"),
              _("User name can be anything"));
        sendf(s, "<br><a href=\"login\">%s</a><br>", _("Login"));
    }

    sendf(s, "</body></html>\r\n\r\n");
}

void run_judotimer(http_parser_t *parser)
{
    SOCKET s = parser->sock;
    //avl_node *node;
    //http_var_t *var;
    gint tatami = 0, cat = 0, match = 0, bvote = 0, wvote = 0, bhkm = 0, whkm = 0, tmo = 0;
    const gchar *bpts = NULL, *wpts = NULL, *tmp;
    gint b = 0, w = 0;

#if 0
    node = avl_get_first(parser->queryvars);
    while (node) {
        var = (http_var_t *)node->key;
        if (var) {
            g_print("Query variable: '%s'='%s'\n", var->name, var->value);
        }
        node = avl_get_next(node);
    }
#endif
    tmp = httpp_get_query_param(parser, "tatami");
    if (tmp) tatami = atoi(tmp);

    tmp = httpp_get_query_param(parser, "cat");
    if (tmp) cat = atoi(tmp);

    tmp = httpp_get_query_param(parser, "match");
    if (tmp) match = atoi(tmp);

    tmp = httpp_get_query_param(parser, "bvote");
    if (tmp) bvote = atoi(tmp);

    tmp = httpp_get_query_param(parser, "wvote");
    if (tmp) wvote = atoi(tmp);

    tmp = httpp_get_query_param(parser, "bhkm");
    if (tmp) bhkm = atoi(tmp);

    tmp = httpp_get_query_param(parser, "whkm");
    if (tmp) whkm = atoi(tmp);

    tmp = httpp_get_query_param(parser, "time");
    if (tmp) tmo = atoi(tmp);

    bpts = httpp_get_query_param(parser, "bpts");
    if (bpts) b = pts2int(bpts);
    wpts = httpp_get_query_param(parser, "wpts");
    if (wpts) w = pts2int(wpts);

    if ((b != w || bvote != wvote || bhkm != whkm) && tatami && is_accepted(parser->address)) {
        struct message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = MSG_RESULT;
        msg.u.result.tatami = tatami;
        msg.u.result.category = cat;
        msg.u.result.match = match;
        msg.u.result.minutes = tmo;
        msg.u.result.blue_score = b;
        msg.u.result.white_score = w;
        msg.u.result.blue_vote = bvote; 
        msg.u.result.white_vote = wvote;
        msg.u.result.blue_hansokumake = bhkm;
        msg.u.result.white_hansokumake = whkm;

        struct message *msg2 = put_to_rec_queue(&msg);
        time_t start = time(NULL);
        while ((time(NULL) < start + 5) && (msg2->type != 0))
            g_usleep(10000);
    }

    sendf(s, "HTTP/1.0 200 OK\r\n");
    sendf(s, "Content-Type: text/plain\r\n\r\n");

    if (tatami == 0)
        return;

    g_static_mutex_lock(&next_match_mutex);
    struct match *m = get_cached_next_matches(tatami);
    int k;

    if (!m) {
        g_static_mutex_unlock(&next_match_mutex);
        return;
    }

    sendf(s, "%d %d\r\n", m[0].category, m[0].number);

    for (k = 0; m[k].number != 1000 && k < 2; k++) {
        gsize x;
        struct judoka *blue, *white, *cat;
        blue = get_data(m[k].blue);
        white = get_data(m[k].white);
        cat = get_data(m[k].category);

        if (!blue)
            blue = &unknown_judoka;

        if (!white)
            white = &unknown_judoka;

        if (!cat)
            cat = &unknown_judoka;

        if (k == 0 && 
            strcmp(next_matches_info[tatami-1][0].blue_first, blue->first))
            g_print("NEXT MATCH ERR: %s %s\n", 
                    next_matches_info[tatami-1][0].blue_first, blue->first);

        gchar *b_first = g_convert(blue->first, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
        gchar *b_last = g_convert(blue->last, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
        gchar *w_first = g_convert(white->first, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
        gchar *w_last = g_convert(white->last, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
        gchar *b_club = g_convert(get_club_text(blue, 0), -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
        gchar *w_club = g_convert(get_club_text(white, 0), -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);

        sendf(s, "%s\r\n", cat->last);
        if (k == 0 && !is_accepted(parser->address)) {
            sendf(s, "%s!\r\n", _("No rights to give results"));
            sendf(s, "%s.\r\n", _("Login"));
        } else {
            sendf(s, "%s %s %s\r\n", b_first, b_last, b_club);
            sendf(s, "%s %s %s\r\n", w_first, w_last, w_club);
            //sendf(s, "%s %s %s\r\n", blue->first, blue->last, blue->club);
            //sendf(s, "%s %s %s\r\n", white->first, white->last, white->club);
        }

        g_free(b_first);
        g_free(b_last);
        g_free(w_first);
        g_free(w_last);
        g_free(b_club);
        g_free(w_club);

        if (blue != &unknown_judoka)
            free_judoka(blue);
        if (white != &unknown_judoka)
            free_judoka(white);
        if (cat != &unknown_judoka)
            free_judoka(cat);
    }
    g_static_mutex_unlock(&next_match_mutex);
    sendf(s, "\r\n");
}

static gint my_atoi(gchar *s) { return (s ? atoi(s) : 0); }

#define GET_INT(_x) gint _x = new_comp ? 0 : my_atoi(db_get_data(row, #_x))
#define GET_STR(_x) gchar *_x = new_comp ? "" : db_get_data(row, #_x)

#define SEARCH_FIELD "<form method=\"get\" action=\"getcompetitor\" name=\"search\">Search (bar code):<input type=\"text\" name=\"index\"></form>\r\n"

void get_competitor(http_parser_t *parser)
{
    SOCKET s = parser->sock;
    gchar buf[1024];
    gint row;
    gint i;
    gboolean new_comp = FALSE;
    gchar *ix = NULL;

    avl_node *node;
    http_var_t *var;
    node = avl_get_first(parser->queryvars);
    while (node) {
        var = (http_var_t *)node->key;
        if (var) {
            //printf("Query variable: '%s'='%s'\n", var->name, var->value);
            if (!strcmp(var->name, "index")) {
                ix = var->value;
                if (!strcmp(ix, "0"))
                    new_comp = TRUE;
            }
        }
        node = avl_get_next(node);
    }

    sendf(s, "HTTP/1.0 200 OK\r\n");
    sendf(s, "Content-Type: text/html; charset=utf-8\r\n");
    sendf(s, "\r\n");
    sendf(s, "<html><head><title>JudoShiai</title></head><body onLoad=\"document.valtable.weight.focus()\">\r\n");

    sprintf(buf, "select * from competitors where \"index\"=%s", ix);
    gint numrows = db_get_table(buf);

    if (numrows < 0) {
        sendf(s, "<h1>%s</h1></body></html>\r\n\r\n", _("No competitor found!"));
        return;
    } else if (numrows == 0)
        new_comp = TRUE;

    row = 0;

    GET_INT(index);
    GET_STR(last);
    GET_STR(first);
    GET_INT(birthyear);
    GET_STR(club);
    GET_STR(regcategory);
    GET_INT(belt);
    GET_INT(weight);
    //GET_INT(visible); xxx not used
    GET_STR(category);
    GET_INT(deleted);
    GET_STR(country);
    GET_STR(id);
    gint seeding = (deleted >> 2) & 7;

#define HTML_ROW_STR(_h, _x)                                            \
    sendf(s, "<tr><td>%s:</td><td><input type=\"text\" name=\"%s\" value=\"%s\"/></td></tr>\r\n", \
          _h, #_x, _x)
#define HTML_ROW_INT(_h, _x)                                            \
    sendf(s, "<tr><td>%s:</td><td><input type=\"text\" name=\"%s\" value=\"%d\"/></td></tr>\r\n", \
          _h, #_x, _x)

    sendf(s, "<form method=\"get\" action=\"setcompetitor\" name=\"valtable\">"
          "<input type=\"hidden\" name=\"index\" value=\"%d\">"
          "<input type=\"hidden\" name=\"category\" value=\"%s\">"
          "<table>\r\n", index, category);

    HTML_ROW_STR("Last Name", last);
    HTML_ROW_STR("First Name", first);
    HTML_ROW_INT("Year of Birth", birthyear);

    sendf(s, "<tr><td>%s:</td><td><select name=\"belt\" value=\"\">", "Grade");
    for (i = 0; belts[i]; i++)
        sendf(s, "<option %s>%s</option>", belt == i ? "selected" : "", belts[i]);
    sendf(s, "</select></td></tr>\r\n");

    HTML_ROW_STR("Club", club);
    HTML_ROW_STR("Country", country);
    HTML_ROW_STR("Reg.Category", regcategory);

    sendf(s, "<tr><td>%s:</td><td>%s</td></tr>\r\n", "Category", category);

    sendf(s, "<tr><td>%s:</td><td><input type=\"text\" name=\"weight\" value=\"%d.%02d\"/></td></tr>\r\n", \
          "Weight", weight/1000, (weight%1000)/10);

    sendf(s, "<tr><td>%s:</td><td><select name=\"seeding\" value=\"\">", "Seeding");
    for (i = 0; i <= 4; i++)
        sendf(s, "<option %s>%d</option>", seeding == i ? "selected" : "", i);
    sendf(s, "</select></td></tr>\r\n");

    sendf(s, "<tr><td>%s:</td><td><input type=\"checkbox\" name=\"hansokumake\" "
          "value=\"yes\" %s/></td></tr>\r\n", "Hansoku-make", deleted&2 ? "checked" : "");

    HTML_ROW_STR("ID", id);

    if (category[0] == '?')
        sendf(s, "<tr><td>%s:</td><td><input type=\"checkbox\" name=\"delete\" "
              "value=\"yes\" %s/></td></tr>\r\n", "Delete", deleted&1 ? "checked" : "");

    sendf(s, "</table>\r\n");

    if (is_accepted(parser->address))
        sendf(s, "<input type=\"submit\" value=\"%s\">", "OK");

    sendf(s, "</form><br><a href=\"competitors\">Competitors</a>"
          SEARCH_FIELD
          "</body></html>\r\n\r\n");

    db_close_table();
}

#define GET_HTML_STR(_x) \
    else if (!strcmp(var->name, #_x)) _x = var->value
#define GET_HTML_INT(_x) \
    else if (!strcmp(var->name, #_x)) _x = my_atoi(var->value)
#define DEF_STR(_x) gchar *_x = ""
#define DEF_INT(_x) gint _x = 0

void set_competitor(http_parser_t *parser)
{
    SOCKET s = parser->sock;

    DEF_INT(index);
    DEF_STR(last);
    DEF_STR(first);
    DEF_INT(birthyear);
    DEF_STR(club);
    DEF_STR(regcategory);
    DEF_STR(belt);
    DEF_STR(weight);
    DEF_INT(visible);
    DEF_STR(category);
    DEF_INT(deleted);
    DEF_INT(seeding);
    DEF_STR(country);
    DEF_STR(hansokumake);
    DEF_STR(id);
    DEF_STR(delete);

    avl_node *node;
    http_var_t *var;
    node = avl_get_first(parser->queryvars);
    while (node) {
        var = (http_var_t *)node->key;
        if (var) {
            //printf("Query variable: '%s'='%s'\n", var->name, var->value);
            if (0) { }
            GET_HTML_INT(index);
            GET_HTML_STR(last);
            GET_HTML_STR(first);
            GET_HTML_INT(birthyear);
            GET_HTML_STR(club);
            GET_HTML_STR(regcategory);
            GET_HTML_STR(belt);
            GET_HTML_STR(weight);
            GET_HTML_INT(visible);
            GET_HTML_STR(category);
            //GET_HTML_INT(deleted);
            GET_HTML_INT(seeding);
            GET_HTML_STR(country);
            GET_HTML_STR(hansokumake);
            GET_HTML_STR(id);
            GET_HTML_STR(delete);
        }
        node = avl_get_next(node);
    }

    if (!strcmp(delete, "yes"))
        deleted = 1;

    if (!strcmp(hansokumake, "yes"))
        deleted |= 2;

    deleted |= seeding << 2;

    gint beltval;
    gchar *b = belt;
    gboolean kyu = strchr(b, 'k') != NULL || strchr(b, 'K') != NULL;
    while (*b && (*b < '0' || *b > '9')) b++;
    gint grade = atoi(b);
    if (grade) beltval = kyu ? 7 - grade : 6 + grade;
    if (beltval < 0 || beltval > 13) beltval = 0;

#define STRCPY(_x)                                                      \
    strncpy(msg.u.edit_competitor._x, _x, sizeof(msg.u.edit_competitor._x)-1)

#define STRCPY_UTF8(_x)                                                      \
    do { gsize _sz;                                                       \
        gchar *_s = g_convert(_x, -1, "UTF-8", "ISO-8859-1", NULL, &_sz, NULL); \
        strncpy(msg.u.edit_competitor._x, _s, sizeof(msg.u.edit_competitor._x)-1); \
        g_free(_s);                                                      \
    } while (0)

    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = MSG_EDIT_COMPETITOR;
    msg.u.edit_competitor.index = index;
    STRCPY(last);
    STRCPY(first);
    msg.u.edit_competitor.birthyear = birthyear;
    STRCPY(club);
    STRCPY(regcategory);
    msg.u.edit_competitor.belt = beltval;
    msg.u.edit_competitor.weight = weight_grams(weight);
    msg.u.edit_competitor.visible = TRUE;
    STRCPY(category);
    msg.u.edit_competitor.deleted = deleted;
    STRCPY(country);
    STRCPY(id);

    struct message *msg2 = put_to_rec_queue(&msg);
    time_t start = time(NULL);
    while ((time(NULL) < start + 5) && (msg2->type != 0))
        g_usleep(10000);

    sendf(s, "HTTP/1.0 200 OK\r\n");
    sendf(s, "Content-Type: text/html; charset=utf-8\r\n");
    sendf(s, "\r\n");
    sendf(s, "<html><head><title>JudoShiai</title></head>"
          "<body onLoad=\"document.search.index.focus()\"><h1>OK</h1>"
          "%s %s saved.<br>"
          "<a href=\"competitors\">Show competitors</a>\r\n"
          SEARCH_FIELD
          "</body></html>\r\n\r\n", first, last);
}

void get_competitors(http_parser_t *parser, gboolean show_deleted)
{
    SOCKET s = parser->sock;
    gint row;
    gchar *order = "country";
    avl_node *node;
    http_var_t *var;
    node = avl_get_first(parser->queryvars);
    while (node) {
        var = (http_var_t *)node->key;
        if (var) {
            //printf("Query variable: '%s'='%s'\n", var->name, var->value);
            if (!strcmp(var->name, "order"))
                order = var->value;
        }
        node = avl_get_next(node);
    }

    gint numrows;
    if (!strcmp(order, "club"))
        numrows = db_get_table("select * from competitors order by \"club\",\"last\",\"first\" asc");
    else if (!strcmp(order, "last"))
        numrows = db_get_table("select * from competitors order by \"last\",\"first\" asc");
    else if (!strcmp(order, "regcat"))
        numrows = db_get_table("select * from competitors order by \"regcategory\",\"last\",\"first\" asc");
    else
        numrows = db_get_table("select * from competitors order by \"country\",\"club\",\"last\",\"first\" asc");

    if (numrows < 0)
        return;

    sendf(s, "HTTP/1.0 200 OK\r\n");
    sendf(s, "Content-Type: text/html; charset=utf-8\r\n");
    sendf(s, "\r\n");
    sendf(s, "<html><head><title>JudoShiai</title></head>"
          "<body onLoad=\"document.search.index.focus()\"><h1>%s</h1>\r\n", 
          show_deleted ? _("Deleted Competitors") : _("Competitors"));

    if (show_deleted)
        sendf(s, "<a href=\"competitors\"><b>Show competitors</b></a><br>\r\n");
    else {
        sendf(s, "<a href=\"delcompetitors\">Show deleted</a><br>\r\n");
        sendf(s, "<a href=\"getcompetitor?index=0\"><b>Add new competitor</b></a><br>\r\n");
    }
    sendf(s, SEARCH_FIELD);

    sendf(s, "<table>\r\n");
    sendf(s, "<tr><td><a href=\"?order=country\"><b>%s</b></a></td>"
          "<td><a href=\"?order=club\"><b>%s</b></a></td>"
          "<td><a href=\"?order=last\"><b>%s</b></a></td>"
          "<td><b>%s</b></td>"
          "<td><a href=\"?order=regcat\"><b>%s</b></a></td>"
          "<td></td></tr>\r\n",
          "Country", "Club", "Last Name", "First Name", "Reg.Category");

    for (row = 0; row < numrows; row++) {
        gchar *last = db_get_data(row, "last");
        gchar *first = db_get_data(row, "first");
        gchar *club = db_get_data(row, "club");
        gchar *country = db_get_data(row, "country");
        gchar *cat = db_get_data(row, "regcategory");
        gchar *id = db_get_data(row, "index");
        gint   deleted = my_atoi(db_get_data(row, "deleted"));

        if (((deleted&1) && show_deleted) ||
            ((deleted&1) == 0 && show_deleted == FALSE)) {
            sendf(s, "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td>"
                  "<td>%s</td>"
                  "<td><a href=\"getcompetitor?index=%s\">%s</a></td></tr>\r\n",
                  country, club, last, first, cat, id, "Edit");
        }
    }	

    sendf(s, "</table></body></html>\r\n\r\n");

    db_close_table();
}

void get_file(http_parser_t *parser)
{
    SOCKET s = parser->sock;
    gchar bufo[512];
    gint n, w;

    char *mime = NULL;
    char *p = parser->uri + 1;
    if (*p == 0)
        p = "index.html";

    char *p2 = strrchr(p, '.');
    if (p2) {
        p2++;
        if (!strcmp(p2, "html")) mime = "text/html";
        else if (!strcmp(p2, "htm")) mime = "text/html";
        else if (!strcmp(p2, "css")) mime = "text/css";
        else if (!strcmp(p2, "txt")) mime = "text/plain";
        else if (!strcmp(p2, "png")) mime = "image/png";
        else if (!strcmp(p2, "jpg")) mime = "image/jpg";
        else if (!strcmp(p2, "class")) mime = "application/x-java-applet";
        else if (!strcmp(p2, "jar")) mime = "application/java-archive";
        else if (!strcmp(p2, "pdf")) mime = "application/pdf";
        else if (!strcmp(p2, "swf")) mime = "application/x-shockwave-flash";
        else if (!strcmp(p2, "ico")) mime = "image/vnd.microsoft.icon";
        else if (!strcmp(p2, "js")) mime = "text/javascript";
    }
    gchar *docfile = g_build_filename(installation_dir, "etc", p, NULL);
    //g_print("GET %s\n", docfile);

    FILE *f = fopen(docfile, "rb");
    g_free(docfile);

    if (!f) {
        sendf(s, "HTTP/1.0 404 NOK\r\n\r\n");
        return;
    }

    sendf(s, "HTTP/1.0 200 OK\r\n");
    if (mime)
        sendf(s, "Content-Type: %s\r\n\r\n", mime);

    while ((n = fread(bufo, 1, sizeof(bufo), f)) > 0)
        if ((w = mysend(s, bufo, n)) < 0)
            break;
    fclose(f);
}

gpointer analyze_http(gpointer param)
{
    http_parser_t *parser = param;
    gint i;

    if (parser->req_type == httpp_req_get) {
        //g_print("GET %s (fd=%d)\n", parser->uri, parser->sock);
					
        if (!strcmp(parser->uri, "/competitors"))
            get_competitors(parser, FALSE);
        else if (!strcmp(parser->uri, "/delcompetitors"))
            get_competitors(parser, TRUE);
        else if (!strcmp(parser->uri, "/getcompetitor"))
            get_competitor(parser);
        else if (!strcmp(parser->uri, "/setcompetitor"))
            set_competitor(parser);
        else if (!strcmp(parser->uri, "/categories"))
            get_categories(parser);
        else if (!strcmp(parser->uri, "/judotimer"))
            run_judotimer(parser);
        else if (!strncmp(parser->uri, "/sheet", 6))
            get_sheet(parser);
        else if (!strcmp(parser->uri, "/password"))
            check_password(parser);
        else if (!strcmp(parser->uri, "/login"))
            check_password(parser);
        else if (!strcmp(parser->uri, "/logout"))
            logout(parser);
        else if (!strcmp(parser->uri, "/index.html"))
            index_html(parser, "");
        else if (!strcmp(parser->uri, "/"))
            index_html(parser, "");
        else
            get_file(parser);
    } else 	if (parser->req_type == httpp_req_post) {
        g_print("POST %s\n", parser->uri);

    }
    
    for (i = 0; i < NUM_CONNECTIONS; i++)
        if (connections[i].fd == parser->sock) {
#if defined(__WIN32__) || defined(WIN32)
            shutdown(parser->sock, SD_SEND);
#endif
            connections[i].closed = TRUE;
            break;
        }

    httpp_destroy(parser);

    return NULL;
}

gpointer httpd_thread(gpointer args)
{
    SOCKET serv_fd, tmp_fd;
    unsigned int alen;
    struct sockaddr_in my_addr, caller;
    fd_set read_fd, fds;
    int reuse = 1;

    g_print("HTTP thread started\n");

    if ((serv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("serv socket");
        g_print("Cannot open http socket!\n");
        g_thread_exit(NULL);    /* not required just good pratice */
        return NULL;
    }

    if (setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse)) < 0) {
        perror("setsockopt (SO_REUSEADDR)");
    }

    //signal(SIGPIPE, sighandler);

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(8088);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serv_fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0) {
        perror("serv bind");
        g_print("Cannot bind http socket!\n");
        return 0;
    }

    listen(serv_fd, 1);

    FD_ZERO(&read_fd);
    FD_SET(serv_fd, &read_fd);

    g_print("Listening http socket\n");

    for ( ; *((gboolean *)args); )   /* exit loop when flag is cleared */
    {
        int r, i;
        static char buf[1024];
        struct timeval timeout;

        fds = read_fd;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        r = select(32, &fds, NULL, NULL, &timeout);

        for (i = 0; i < NUM_CONNECTIONS; i++) {
            if (connections[i].fd && connections[i].closed) {
                //g_print("Closed: connection %d fd=%d closed\n", i, connections[i].fd);
                FD_CLR(connections[i].fd, &read_fd);
                closesocket(connections[i].fd);
                connections[i].fd = 0;
            }
        }

        if (r <= 0)
            continue;

        if (FD_ISSET(serv_fd, &fds)) {
            alen = sizeof(caller);
            if ((tmp_fd = accept(serv_fd, (struct sockaddr *)&caller, &alen)) < 0) {
                perror("serv accept");
                continue;
            }

            for (i = 0; i < NUM_CONNECTIONS; i++)
                if (connections[i].fd == 0)
                    break;

            if (i >= NUM_CONNECTIONS) {
                g_print("Node cannot accept new connections!\n");
                closesocket(tmp_fd);
                continue;
            }

            connections[i].fd = tmp_fd;
            connections[i].addr = caller.sin_addr.s_addr;
            connections[i].closed = FALSE;
#if 0
            const int nodelayflag = 1;
            if (setsockopt(tmp_fd, IPPROTO_TCP, TCP_NODELAY, 
                           (const void *)&nodelayflag, sizeof(nodelayflag))) {
                g_print("CANNOT SET TCP_NODELAY (2)\n");
            }
#endif
            /*g_print("Node: new connection[%d]: fd=%d addr=%lx\n", 
              i, tmp_fd, caller.sin_addr.s_addr);*/
            FD_SET(tmp_fd, &read_fd);
        }

        for (i = 0; i < NUM_CONNECTIONS; i++) {
            if (connections[i].fd == 0)
                continue;

            if (!(FD_ISSET(connections[i].fd, &fds)))
                continue;

            r = recv(connections[i].fd, buf, sizeof(buf)-1, 0);
            if (r > 0) {
                http_parser_t *parser;
			
                buf[r] = 0;
                //g_print("DATA='%s'\n", buf);

                parser = httpp_create_parser();
                httpp_initialize(parser, NULL);
                parser->sock = connections[i].fd;
                parser->address = connections[i].addr;

                if (httpp_parse(parser, buf, r)) {
                    analyze_http(parser);
                } else {
                    g_print("http req error\n");
                    httpp_destroy(parser);
                }
            } else {
                //g_print("Received 0: connection %d fd=%d closed\n", i, connections[i].fd);
                closesocket(connections[i].fd);
                FD_CLR(connections[i].fd, &read_fd);
                connections[i].fd = 0;
            }
        }
    }

    g_print("httpd exit\n");
    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}

