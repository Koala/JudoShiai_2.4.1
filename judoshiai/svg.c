/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2012 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <math.h>

#include <cairo.h>
#include <cairo-pdf.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#include "judoshiai.h"

#define WINNER(_a) (m[_a].blue_points ? m[_a].blue :			\
		      (m[_a].white_points ? m[_a].white :		\
		       (m[_a].blue == GHOST ? m[_a].white :		\
			(m[_a].white == GHOST ? m[_a].blue :            \
			 (db_has_hansokumake(m[_a].blue) ? m[_a].white : \
			  (db_has_hansokumake(m[_a].white) ? m[_a].blue : NO_COMPETITOR))))))

#define LOSER(_a) (m[_a].blue_points ? m[_a].white :			\
		     (m[_a].white_points ? m[_a].blue :			\
		      (m[_a].blue == GHOST ? m[_a].blue :		\
		       (m[_a].white == GHOST ? m[_a].white :		\
			(db_has_hansokumake(m[_a].blue) ? m[_a].blue :  \
			 (db_has_hansokumake(m[_a].white) ? m[_a].white : NO_COMPETITOR))))))

#define MATCHED(_a) (m[_a].blue_points || m[_a].white_points)

static gboolean debug = TRUE;

#define WRITE2(_s, _l)                                                     \
    do { if (dfile) fwrite(_s, 1, _l, dfile);                            \
        if (!rsvg_handle_write(handle, (guchar *)_s, _l, &err)) {        \
            g_print("\nERROR %s: %s %d\n",                              \
                    err->message, __FUNCTION__, __LINE__); err = NULL; return TRUE; } } while (0)

#define WRITE1(_s, _l)                                                  \
    do { gint _i; for (_i = 0; _i < _l; _i++) {                         \
        if (_s[_i] == '&')                                              \
            WRITE2("&amp;", 5);                                         \
        else if (_s[_i] == '<')                                         \
            WRITE2("&lt;", 4);                                          \
        else if (_s[_i] == '>')                                         \
            WRITE2("&gt;", 4);                                          \
        else                                                            \
            WRITE2(&_s[_i], 1);                                         \
        }} while (0)

#define WRITE(_a) WRITE1(_a, strlen(_a))
#if 0
    do { if (dfile) fwrite(_a, 1, strlen(_a), dfile);                   \
        if (!rsvg_handle_write(handle, (guchar *)_a, strlen(_a), &err)) { \
            g_print("\nERROR %s: %s %d\n",                              \
                    err->message, __FUNCTION__, __LINE__); err = NULL; return TRUE; } } while (0)
#endif

#define IS_LABEL_CHAR(_x) ((_x >= 'a' && _x <= 'z') || _x == 'C' || _x == 'M' || _x == '#' || _x == '=')
#define IS_VALUE_CHAR(_x) (_x >= '0' && _x <= '9')

#define IS_SAME(_a, _b) (!strcmp((char *)_a, (char *)_b))

#define CODE_LEN 16

static struct {
    guchar code[CODE_LEN];
    gint codecnt;
    gint value;
} attr[16];
static gint cnt = 0;

#define NUM_SVG 200

static struct svg_cache {
    gint key;
    gchar *data;
    guint datalen;
    gint width;
    gint height;
} svg_data[NUM_SVG];

static struct svg_props {
    gint key;
    gint pages;
    gint width;
    gint height;
} svg_info[NUM_SVG];

gchar *svg_directory = NULL;
static gint num_svg, num_svg_info;

static struct svg_props *find_svg_info(gint key)
{
    gint i;

    for (i = 0; i < num_svg_info; i++)
        if ((svg_info[i].key & 0xfff0) == key)
            return &svg_info[i];

    return NULL;
}

static gint make_key(struct compsys systm, gint pagenum)
{
    if (systm.system == SYSTEM_POOL || 
        systm.system == SYSTEM_DPOOL ||
        systm.system == SYSTEM_QPOOL ||
        systm.system == SYSTEM_DPOOL2)
        return (systm.system<<24)|(systm.numcomp<<8)|pagenum;

    return (systm.system<<24)|(systm.table<<16)|pagenum;
}

static struct svg_cache *get_cache(gint key)
{
    gint i;
    for (i = 0; i < num_svg; i++)
        if ((svg_data[i].key) == key)
            return & svg_data[i];
    return NULL;
}

gboolean get_svg_page_size(gint index, gint pagenum, gint *width, gint *height)
{
    gint key;
    struct compsys systm = get_cat_system(index);

    key = make_key(systm, pagenum);
    struct svg_cache *info = get_cache(key);
    if (info) {
        *width = info->width;
        *height = info->height;
        return TRUE;
    }
    return FALSE;
}

gboolean svg_landscape(gint ctg, gint page)
{
    gint svgw, svgh;
    if (get_svg_page_size(ctg, page, &svgw, &svgh))
        return (svgw > svgh);

    return FALSE;
}

gint get_num_svg_pages(struct compsys systm)
{
    gint key, i, pages = 0;

    key = make_key(systm, 0);

    for (i = 0; i < num_svg; i++) {
        if ((svg_data[i].key & 0xffffff00) == key)
            pages++;
    }

    return pages;
}

gint write_judoka(RsvgHandle *handle, gint start, struct judoka *j, FILE *dfile)
{
    gint i;
    GError *err = NULL;
    gchar buf[64];

    WRITE2("<tspan>", 7);

    for (i = start; i < cnt; i++) {
        if (attr[i].code[0] == '\'') {
            // quoted text
            WRITE1((attr[i].code+1), attr[i].codecnt - 1);
        } else if (IS_SAME(attr[i].code, "first")) {
            WRITE(j->first);
        } else if (IS_SAME(attr[i].code, "last")) {
            WRITE(j->last);
        } else if (IS_SAME(attr[i].code, "grd")) {
            WRITE(belts[j->belt]);
        } else if (IS_SAME(attr[i].code, "club")) {
            WRITE(j->club);
        } else if (IS_SAME(attr[i].code, "country")) {
            WRITE(j->country);
        } else if (IS_SAME(attr[i].code, "weight")) {
            snprintf(buf, sizeof(buf), "%d.%02d", j->weight/1000, (j->weight%1000)/10);
            WRITE(buf);
        } else if (IS_SAME(attr[i].code, "hm")) {
            if (j->deleted & HANSOKUMAKE) {
                if (attr[i].value == 1)
                    WRITE2("</tspan><tspan style='text-decoration:strikethrough'>", 53);
                else if (attr[i].value == 2)
                    goto out; // stop writing
                else 
                    WRITE2("</tspan><tspan>", 15);
            } else if (j->deleted & POOL_TIE3) {
                WRITE2("</tspan><tspan style='fill:red'>", 32);
            }
        } else if (IS_SAME(attr[i].code, "s")) {
            WRITE(" ");
        }
    }

 out:
    WRITE2("</tspan>", 8);

    return 0;
}

gint paint_svg(struct paint_data *pd)
{
    struct compsys systm = pd->systm;
    gint pagenum = pd->page;
    gint category = pd->category;
    gint num_judokas = systm.numcomp;
    gint table = systm.table;
    gint sys = systm.system - SYSTEM_FRENCH_8;
    struct pool_matches pm, pm2;
    struct match fm[NUM_MATCHES];
    struct match *m;
    gchar buf[64];
    GError *err = NULL;
    gint i;
    
    //gint pos[4];
    gboolean yes[4][21];
    gboolean pool_done[4];
    gint c[4][21];
    gint pool_start[4];
    gint pool_size[4];
    gint size = num_judokas/4;
    gint rem = num_judokas - size*4;
    gint start = 0;
    gint num_pool_a, num_pool_b;
    gboolean yes_a[21], yes_b[21];
    gint c_a[21], c_b[21];
    gint key, svgwidth;
    FILE *dfile = NULL;
    gchar *svgdata = NULL;

    key = make_key(systm, pagenum);

    for (i = 0; i < num_svg; i++) {
        if (svg_data[i].key == key) {
            svgdata = svg_data[i].data;
            //svgdatalen = svg_data[i].datalen;
            svgwidth = svg_data[i].width;
            //svgheight = svg_data[i].height;
            break;
        } 
    }

    if (!svgdata)
        return FALSE;

    cairo_set_source_rgb(pd->c, 1.0, 1.0, 1.0);
    cairo_rectangle(pd->c, 0.0, 0.0, pd->paper_width, pd->paper_height);
    cairo_fill(pd->c);

    RsvgHandle *handle = rsvg_handle_new();

    switch (systm.system) {
    case SYSTEM_POOL:
        fill_pool_struct(category, num_judokas, &pm, FALSE);
        m = pm.m;
        if (pm.finished)
            get_pool_winner(num_judokas, pm.c, pm.yes, pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched, pm.tie);
        break;

    case SYSTEM_DPOOL:
    case SYSTEM_DPOOL2:
        num_pool_a = num_judokas - num_judokas/2;
        num_pool_b = num_judokas - num_pool_a;
        memset(yes_a, 0, sizeof(yes_a));
        memset(yes_b, 0, sizeof(yes_b));
        memset(c_a, 0, sizeof(c_a));
        memset(c_b, 0, sizeof(c_b));

        for (i = 1; i <= num_judokas; i++) {
            if (i <= num_pool_a)
                yes_a[i] = TRUE;
            else
                yes_b[i] = TRUE;
        }
        fill_pool_struct(category, num_judokas, &pm, FALSE);
        get_pool_winner(num_pool_a, c_a, yes_a, pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched, pm.tie);
        get_pool_winner(num_pool_b, c_b, yes_b, pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched, pm.tie);
        m = pm.m;

        if (systm.system == SYSTEM_DPOOL2) {
            fill_pool_struct(category, num_judokas, &pm2, TRUE);
            if (pm2.finished)
                get_pool_winner(4, pm2.c, pm2.yes, pm2.wins, pm2.pts, pm2.mw, pm2.j, pm2.all_matched, pm2.tie);
        }
        break;

    case SYSTEM_QPOOL:
        for (i = 0; i < 4; i++) {
            pool_size[i] = size;
            //pos[i] = 1;
        }

        for (i = 0; i < rem; i++)
            pool_size[i]++;

        for (i = 0; i < 4; i++) {
            pool_start[i] = start;
            start += pool_size[i];
        }

        fill_pool_struct(category, num_judokas, &pm, FALSE);
        m = pm.m;

        memset(c, 0, sizeof(c));
        memset(yes, 0, sizeof(yes));

        for (i = 0; i < 4; i++) {
            gint j, k;

            for (j = 1; j <= num_judokas; j++) {
                for (k = 3; k >= 0; k--) {
                    if (j-1 >= pool_start[k]) {
                        yes[k][j] = TRUE;
                        break;
                    }
                }
            }

            get_pool_winner(pool_size[i], c[i], yes[i], pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched, pm.tie);

            pool_done[i] = pool_finished(num_judokas, num_matches(pd->systm.system, num_judokas), 
                                            SYSTEM_QPOOL, yes[i], &pm);
        }        
        break;

    case SYSTEM_FRENCH_8:
    case SYSTEM_FRENCH_16:
    case SYSTEM_FRENCH_32:
    case SYSTEM_FRENCH_64:
    case SYSTEM_FRENCH_128:
        memset(fm, 0, sizeof(fm));
        db_read_category_matches(category, fm);
        m = fm;
        break;
    }

    if (pd->filename) {
        dfile = fopen(pd->filename, "w");
        if (!dfile)
            perror("svgout");
        g_print("filename='%s', dfile =%p\n", pd->filename, dfile);
    } else if (debug)
        dfile = fopen("debug.svg", "w");

    guchar *p = (guchar *)svgdata;
    while(*p) {
        gboolean delayed = FALSE;
#if 0
        if (*p == '>' && *(p+1) == '%') { // dont send '>' yet
            delayed = TRUE;
            p++;
        }
#endif
        if (*p == '%') {
            memset(attr, 0, sizeof(attr));
            cnt = 0;
            p++;

            while (IS_LABEL_CHAR(*p) || IS_VALUE_CHAR(*p) || *p == '-' || *p == '\'') {
                while (IS_LABEL_CHAR(*p))
                    attr[cnt].code[attr[cnt].codecnt++] = *p++;
                
                if (*p == '-') p++;
            
                while (IS_VALUE_CHAR(*p))
                    attr[cnt].value = attr[cnt].value*10 + *p++ - '0';

                if (*p == '-') p++;

                if (*p == '\'') {
                    cnt++;
                    p++;
                    attr[cnt].code[0] = '\'';
                    attr[cnt].codecnt = 1;
                    while (*p && *p != '\'') {
                        attr[cnt].code[attr[cnt].codecnt] = *p++;
                        if (attr[cnt].codecnt < CODE_LEN)
                            attr[cnt].codecnt++;
                    }
                    if (*p == '\'')
                        p++;
                }

                cnt++;
            }

#if 0
            g_print("\n");
            for (i = 0; i < cnt; i++)
                g_print("i=%d code='%s' val=%d\n",
                        i, attr[i].code, attr[i].value);
            g_print("\n");
#endif

            if (delayed && attr[0].code[0] != 'h') {
                WRITE(">");
                delayed = FALSE;
            }

            if (attr[0].code[0] == 'm' || attr[0].code[0] == 'M') {
                gboolean dp2 = attr[0].code[0] == 'M';
                gint fight = attr[0].value;

                if (dp2)
                    m = pm2.m;
                
                if (attr[1].code[0] == 0) { // name
                    gint who = attr[1].value;
                    gint ix;
                    struct judoka *j = NULL;

                    if (who == 1) ix = m[fight].blue;
                    else ix = m[fight].white;

                    j = get_data(ix);

                    if (j) {
                        write_judoka(handle, 2, j, dfile);
                            free_judoka(j);
                    }
                } else if (attr[1].code[0] == 'p') {
                    gint who = attr[1].value;
                    gboolean ifmatched = FALSE;
                    gint points = who == 1 ? m[fight].blue_points : m[fight].white_points;
                    gint next = 2;
                    if ((cnt > next) && attr[next].code[0] == 0) {
                        if (attr[next].value & 1)
                            ifmatched = TRUE;
                        next++;
                    }
                    if ((cnt > next) && attr[next].code[0] == '=') {
                        if (attr[next].value == points)
                            snprintf(buf, sizeof(buf), "%s", attr[next+1].code + 1);
                        else if (attr[next+2].value == 0)
                            snprintf(buf, sizeof(buf), "%d", points);
                        else
                            buf[0] = 0;
                    } else
                        snprintf(buf, sizeof(buf), "%d", points);
                    
                    if ((ifmatched == FALSE || MATCHED(fight)) && buf[0])
                        WRITE(buf);
                } else if (attr[1].code[0] == 't') {
                    if (m[fight].match_time) {
                        snprintf(buf, sizeof(buf), "%d:%02d", m[fight].match_time/60, m[fight].match_time%60);
                        WRITE(buf);
                    }
                } else if (attr[1].code[0] == '#') {
                    sprintf(buf, "%d", fight);
                    WRITE(buf);
                } else if (IS_SAME(attr[1].code, "winner")) {
                    struct judoka *j = get_data(WINNER(fight));
                    if (j) {
                        write_judoka(handle, 2, j, dfile);
                        free_judoka(j);
                    }
                }
            } else if (attr[0].code[0] == 'c' ||
                       attr[0].code[0] == 'C') {
                gboolean dp2 = attr[0].code[0] == 'C';
                gint comp = attr[0].value;
                struct judoka *j = NULL;
                struct pool_matches *pmp = dp2 ? &pm2 : &pm;

                if (systm.system == SYSTEM_POOL || 
                    systm.system == SYSTEM_QPOOL ||
                    systm.system == SYSTEM_DPOOL ||
                    systm.system == SYSTEM_DPOOL2)
                    j = pmp->j[comp];

                if (!j || j->index >= 10000) {
                    gchar b[32];
                    sprintf(b,"COMP %d j=%p", comp, j);
                    WRITE(b);
                    continue;
                }

                if (attr[1].code[0] && attr[1].code[1] == 0) { // one letter codes
                    if (attr[1].code[0] == 'w') { // number of wins
                        if (pmp->wins[comp] || pmp->finished) {
                            snprintf(buf, sizeof(buf), "%d", pmp->wins[comp]);
                            WRITE(buf);
                        }
                    } else if (attr[1].code[0] == 'p') { // number of points
                        if (pmp->pts[comp] || pmp->finished) {
                            snprintf(buf, sizeof(buf), "%d", pmp->pts[comp]);
                            WRITE(buf);
                        }
                    } else if (attr[1].code[0] == 'r') { // pool result
                        if (systm.system == SYSTEM_POOL || dp2) {
                            if (pmp->finished) {
                                gint k;
                                for (k = 1; k <= dp2 ? 4 : num_judokas; k++) { 
                                    if (pmp->c[k] == comp) { 
                                        snprintf(buf, sizeof(buf), "%d", k);
                                        WRITE(buf);
                                        break;
                                    }
                                }
                            }
                        } else if (systm.system == SYSTEM_DPOOL || systm.system == SYSTEM_DPOOL2) {
                            gint k;
                            if (comp <= num_pool_a) {
                                if (pmp->finished) {
                                    for (k = 1; k <= num_pool_a; k++) { 
                                        if (c_a[k] == comp) { 
                                            snprintf(buf, sizeof(buf), "%d", k);
                                            WRITE(buf);
                                            break;
                                        }
                                    }
                                }
                            } else {
                                if (pmp->finished) {
                                    for (k = 1; k <= num_pool_b; k++) { 
                                        if (c_b[k] == comp) { 
                                            snprintf(buf, sizeof(buf), "%d", k);
                                            WRITE(buf);
                                            break;
                                        }
                                    }
                                }
                            }
                        } else if (systm.system == SYSTEM_QPOOL) {
                            for (i = 0; i < 4; i++) {
                                if (comp > pool_start[i] && comp <= (pool_start[i] + pool_size[i])) {
                                    if (TRUE || pool_done[i]) {
                                        gint k;
                                        for (k = 1; k <= pool_size[i]; k++) {
                                            if (c[i][k] == comp) {
                                                snprintf(buf, sizeof(buf), "%d", k);
                                                WRITE(buf);
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                } else { // strings
                    write_judoka(handle, 1, j, dfile);
                }
            } else if (attr[0].code[0] == 'r') { // results
                gint res = attr[0].value;

                if (systm.system == SYSTEM_POOL) {
                    struct judoka *j = pm.j[pm.c[res]];
                    if (j)
                        write_judoka(handle, 1, j, dfile);
                } else if (systm.system == SYSTEM_DPOOL2) {
                    struct judoka *j = pm2.j[pm2.c[res]];
                    if (j)
                        write_judoka(handle, 1, j, dfile);
                } else if (systm.system == SYSTEM_DPOOL || systm.system == SYSTEM_QPOOL) {
                    gint ix = 0;
                    gint mnum = num_matches(systm.system, num_judokas);
                    mnum += (systm.system == SYSTEM_DPOOL) ? 1 : 5;
                    switch (res) {
                    case 1: ix = WINNER(mnum + 2); break;
                    case 2: ix = LOSER(mnum + 2); break;
                    case 3: ix = LOSER(mnum); break;
                    case 4: ix = LOSER(mnum + 1); break;
                    }
                    struct judoka *j = get_data(ix);
                    if (j) {
                        write_judoka(handle, 1, j, dfile);
                        free_judoka(j);
                    }
                } else {
                    gint ix = 0;

                    switch (res) {
                    case 1: 
                        ix = WINNER(get_abs_matchnum_by_pos(systm, 1, 1)); 
                        break;
                    case 2:
                        ix = LOSER(get_abs_matchnum_by_pos(systm, 1, 1)); 
                        break;
                    case 3:
                        if (table == TABLE_NO_REPECHAGE || table == TABLE_ESP_DOBLE_PERDIDA)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 3, 1));
                        else if (table == TABLE_MODIFIED_DOUBLE_ELIMINATION)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 2, 1));
                        else if (one_bronze(table, sys))
                            ix = WINNER(get_abs_matchnum_by_pos(systm, 3, 1));
                        else if (table == TABLE_DOUBLE_LOST)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 3, 1));
                        else
                            ix = WINNER(get_abs_matchnum_by_pos(systm, 3, 1));
                        break;
                    case 4:
                        if (table == TABLE_NO_REPECHAGE || table == TABLE_ESP_DOBLE_PERDIDA)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 3, 2));
                        else if (one_bronze(table, sys))
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 3, 1));
                        else if (table == TABLE_DOUBLE_LOST)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 3, 2));
                        else 
                            ix = WINNER(get_abs_matchnum_by_pos(systm, 3, 2));
                        break;
                    case 5:
                        if (one_bronze(table, sys))
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 5, 1));
                        else if (table == TABLE_DOUBLE_LOST)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 5, 1));
                        else 
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 3, 1));
                        break;
                    case 6:
                        if (one_bronze(table, sys))
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 5, 2));
                        else if (table == TABLE_DOUBLE_LOST)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 5, 2));
                        else
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 3, 2));
                        break;
                    case 7:
                        if (table == TABLE_DOUBLE_LOST)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 7, 1));
                        else
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 7, 1));
                        break;
                    case 8:
                        if (table == TABLE_DOUBLE_LOST)
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 7, 2));
                        else
                            ix = LOSER(get_abs_matchnum_by_pos(systm, 7, 2));
                        break;
                    }

                    struct judoka *j = get_data(ix);
                    if (j) {
                        write_judoka(handle, 1, j, dfile);
                        free_judoka(j);
                    }
                }
            } else if (attr[0].code[0] == 'i') {
                if (IS_SAME(attr[1].code, "competition"))
                    WRITE(info_competition);
                else if (IS_SAME(attr[1].code, "date"))
                    WRITE(info_date);
                else if (IS_SAME(attr[1].code, "place"))
                    WRITE(info_place);
                else if (IS_SAME(attr[1].code, "catname")) {
                    struct judoka *ctg = get_data(category);
                    if (ctg) {
                        WRITE(ctg->last);
                        free_judoka(ctg);
                    }
                }
            } else if (attr[0].code[0] == 't') {
                g_print("teksti='%s'\n", print_texts[attr[0].value][print_lang]);
                WRITE(print_texts[attr[0].value][print_lang]);
            } else if (attr[0].code[0] == 'h') {
                if (attr[1].code[0] == 'm') {
                    gint fight = attr[1].value;
                    gint who = attr[2].value;
                    struct judoka *j = get_data(who == 1 ? m[fight].blue : m[fight].white);
                    if (j) {
                        if (j->deleted & HANSOKUMAKE)
                            WRITE(" text-decoration='strikethrough'");
                        free_judoka(j);
                    }
                }
            }

            if (delayed)
                WRITE1(">", 1);
        } else {
            //g_print("%c", *p);
            WRITE2(p, 1);
            p++;
        }
    }

    if (dfile) {
        fclose(dfile);
        dfile = NULL;
    }

    switch (systm.system) {
    case SYSTEM_POOL:
    case SYSTEM_DPOOL:
    case SYSTEM_DPOOL2:
    case SYSTEM_QPOOL:
        /* clean up */
        empty_pool_struct(&pm);

        if (systm.system == SYSTEM_DPOOL2)
            empty_pool_struct(&pm2);
        break;

    case SYSTEM_FRENCH_8:
    case SYSTEM_FRENCH_16:
    case SYSTEM_FRENCH_32:
        break;

    case SYSTEM_FRENCH_64:
    case SYSTEM_FRENCH_128:
        break;
    }

    rsvg_handle_close(handle, NULL);

    gdouble  paper_width_saved;
    gdouble  paper_height_saved;

    if (pd->rotate) {
        paper_width_saved = pd->paper_width;
        paper_height_saved = pd->paper_height;
        pd->paper_width = paper_height_saved;
        pd->paper_height = paper_width_saved;
        pd->total_width = pd->paper_width;
        pd->landscape = TRUE;
        cairo_translate(pd->c, paper_width_saved*0.5, paper_height_saved*0.5);
        cairo_rotate(pd->c, -0.5*M_PI);
        cairo_translate(pd->c, -paper_height_saved*0.5, -paper_width_saved*0.5);
    }

    cairo_save(pd->c);
    cairo_scale(pd->c, pd->paper_width/svgwidth, pd->paper_width/svgwidth);
    rsvg_handle_render_cairo(handle, pd->c);
    cairo_restore(pd->c);

    rsvg_handle_free(handle);

    return TRUE;
}

void select_svg_dir(GtkWidget *menu_item, gpointer data)
{
    GtkWidget *dialog, *do_svg;
    gboolean ok;

    dialog = gtk_file_chooser_dialog_new(_("Choose a directory"),
                                         NULL,
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);

    do_svg = gtk_check_button_new_with_label(_("Use SVG Templates"));
    //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(do_svg), TRUE);
    gtk_widget_show(do_svg);

    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), do_svg);

    if (svg_directory)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                            svg_directory);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(do_svg), num_svg);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog);
        return;
    }

    ok = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(do_svg));
        
    if (ok) {
        g_free(svg_directory);
        svg_directory = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_key_file_set_string(keyfile, "preferences", "svgdir", svg_directory);
    } else
        g_key_file_set_string(keyfile, "preferences", "svgdir", "");

    gtk_widget_destroy (dialog);

    read_svg_files(ok);
}

void read_svg_files(gboolean ok)
{
    gint i;

    // free old data
    for (i = 0; i < NUM_SVG; i++) {
        if (svg_data[i].data)
            g_free(svg_data[i].data);
    }
    memset(svg_data, 0, sizeof(svg_data));
    memset(svg_info, 0, sizeof(svg_info));
               
    num_svg = 0;
    num_svg_info = 0;

    if (ok == FALSE || svg_directory == NULL)
        return;

    GDir *dir = g_dir_open(svg_directory, 0, NULL);
    if (dir) {
        const gchar *fname = g_dir_read_name(dir);
        while (fname) {
            gchar *fullname = g_build_filename(svg_directory, fname, NULL);
            if (strstr(fname, ".svg") && num_svg < NUM_SVG) {
                gint a, b, c;
                gint n = sscanf(fname, "%d-%d-%d.svg", &a, &b, &c);
                if (n == 3) {
                    if (!g_file_get_contents(fullname, &svg_data[num_svg].data, &svg_data[num_svg].datalen, NULL))
                        g_print("CANNOT OPEN '%s'\n", fullname);
                    else  {
                        struct compsys systm = wish_to_system(a, b);
                        gint key = make_key(systm, c-1);
                        svg_data[num_svg].key = key;
                        RsvgHandle *h = rsvg_handle_new_from_data((guchar *)svg_data[num_svg].data, 
                                                                  svg_data[num_svg].datalen, NULL);
                        RsvgDimensionData dim;
                        rsvg_handle_get_dimensions(h, &dim);
                        svg_data[num_svg].width = dim.width;
                        svg_data[num_svg].height = dim.height;
                        rsvg_handle_free(h);

                        g_print("read key=0x%x pos=%d file=%s w=%d h=%d\n", 
                                key, num_svg, fname, svg_data[num_svg].width, svg_data[num_svg].height);
                        num_svg++;

                        struct svg_props *info = find_svg_info(key);
                        if (!info) {
                            info = &svg_info[num_svg_info];
                            if (num_svg_info < NUM_SVG)
                                num_svg_info++;
                            info->key = key;
                        }
                        info->pages++;
                    }
                }
            }
            g_free(fullname);
            fname = g_dir_read_name(dir);
        }
        g_dir_close(dir);
    }
}
