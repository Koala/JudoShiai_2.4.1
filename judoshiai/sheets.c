/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2011 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <math.h>

#include <cairo.h>
#include <cairo-pdf.h>

#include "judoshiai.h"

#define SIZEX 630
#define SIZEY 891
#define W(_w) ((_w)*pd->paper_width)
#define H(_h) ((_h)*pd->paper_height)

static gint current_page = 0;

#define NAME_H H(0.02)
#define NAME_W W(0.1)
#define NAME_E W(0.05)
#define NAME_S (NAME_H*2.7)
#define CLUB_WIDTH W(0.16)

#define TEXT_OFFSET   W(0.01)
#define TEXT_HEIGHT   (NAME_H*0.6)

#define THIN_LINE     (pd->paper_width < 700.0 ? 1.0 : pd->paper_width/700.0)
#define THICK_LINE    (2*THIN_LINE)

#define ROW_WIDTH     W(0.9)
//#define ROW_HEIGHT    NAME_H

#define OFFSET_X      W(0.05)
#define OFFSET_Y      H(0.1)
#define POOL_JUDOKAS_X OFFSET_X
#define POOL_JUDOKAS_Y OFFSET_Y
#define POOL_MATCH_X   OFFSET_X
#define POOL_MATCH_Y   (OFFSET_Y + 8*ROW_HEIGHT)
#define POOL_RESULT_X  OFFSET_X
#define POOL_RESULT_Y  (POOL_MATCH_Y + 13*ROW_HEIGHT)
#define POOL_WIN_X     (ROW_WIDTH - W(0.15))
#define POOL_WIN_Y     POOL_JUDOKAS_Y

#define WRITE_TABLE(_t, _r, _c, _txt...) do {   \
        char _buf[100];                         \
        snprintf(_buf, sizeof(_buf)-1, _txt);   \
        write_table(pd, &_t, _r, _c, _buf);     \
    } while (0)  

#define WRITE_TABLE_H(_t, _r, _c, _del, _txt...) do {   \
        char _buf[100];                                 \
        snprintf(_buf, sizeof(_buf)-1, _txt);           \
        write_table_h(pd, &_t, _r, _c, _del, _buf);     \
    } while (0)  

//#define MY_FONT "Sans"
#define MY_FONT "Arial"
static gchar font_face[32];
static gint  font_slant = CAIRO_FONT_SLANT_NORMAL, font_weight = CAIRO_FONT_WEIGHT_NORMAL;
static GtkWidget *sheet_label = NULL;
static gdouble ROW_HEIGHT;

struct table {
    double position_x, position_y;
    int num_rows, num_cols;
    double columns[];
};

struct table judoka_table = {
    0, 0,
    0, 0,
    {0.03, 0.25, 0.08, 0.20, /* no name belt club */
     0.03, 0.03, 0.03, 0.03, 0.03, 0.03}
};

struct table judoka_2_table = {
    0, 0,
    0, 0,
    {0.03, 0.33, 0.20, /* no name club */
     0.03, 0.03, 0.03, 0.03, 0.03, 0.03}
};

struct table match_table = {
    0, 0,
    0, 7,
    {0.04, 0.3, 0.03, 0.03, 0.3, 0.07, 0.07}
};

struct table result_table = {
    0, 0,
    0, 2,
    {0.05, 0.40}
};

struct table result_table_2 = {
    0, 0,
    0, 2,
    {0.05, 0.25}
};

struct table win_table = {
    0, 0,
    0, 3,
    {0.04, 0.04, 0.04}
};


static void init_tables(struct paint_data *pd)
{
    judoka_table.position_x = judoka_2_table.position_x = POOL_JUDOKAS_X;
    judoka_table.position_y = judoka_2_table.position_y = POOL_JUDOKAS_Y;
    match_table.position_x = POOL_MATCH_X;
    match_table.position_y = POOL_MATCH_Y;
    result_table.position_x = POOL_RESULT_X;
    result_table.position_y = POOL_RESULT_Y;
    result_table_2.position_x = POOL_RESULT_X;
    result_table_2.position_y = POOL_RESULT_Y;
    win_table.position_x = POOL_WIN_X;
    win_table.position_y = POOL_WIN_Y;
}

#define PI 3.14159265

static double paint_comp(struct paint_data *pd, struct pool_matches *unused1, int pos, 
                         double blue_y, double white_y, 
                         int blue, int white, int blue_pts, int white_pts, 
                         gint flags, gint number_b, gint number_w, gint comp_num)
{
    double extra = (pos == 0) && (flags & F_REPECHAGE) == 0 ? CLUB_WIDTH : 0.0;
    double txtwidth = NAME_W + NAME_E;
    double x;
    double y = (white_y + blue_y)/2.0;
    double dy = (white_y - blue_y)/2.0;
    double py;
    char buf[100];
    struct judoka *j;
    cairo_text_extents_t extents;
    gboolean small = FALSE, only_last;
    gint sys = pd->systm.system - SYSTEM_FRENCH_8;
    gint table = pd->systm.table;
    double x1, y1, w1, r1, r2;
    gchar numbuf[4];
    cairo_text_extents_t extents1;
    gboolean crowded = FALSE;

    snprintf(numbuf, sizeof(numbuf), "%d", comp_num);
    cairo_text_extents(pd->c, numbuf, &extents1);
    r1 = extents1.height/2.0;
    r2 = 2.0*r1;

    /*if (flags & REPECHAGE)
      pos++;*/

    only_last = pos || (flags & F_SYSTEM_MASK) == SYSTEM_DPOOL || (flags & F_REPECHAGE);

    cairo_text_extents(pd->c, "Hj", &extents);
    //small = (white_y - blue_y) < 2.0*extents.height && pos == 0;

    if (system_is_french(pd->systm.system) && pd->systm.system >= SYSTEM_FRENCH_32) {
	if (pos == 0 && (flags & F_REPECHAGE) == 0)
	    small = TRUE;
	if (table != TABLE_DOUBLE_REPECHAGE && 
            table != TABLE_SWE_DUBBELT_AATERKVAL &&
            table != TABLE_DOUBLE_REPECHAGE_ONE_BRONZE &&
            pd->systm.system != SYSTEM_FRENCH_128) {
	    txtwidth *= 0.75;
	    //extra = NAME_W + NAME_E + CLUB_WIDTH - txtwidth;
	    crowded = TRUE;
	}
    }

    txtwidth *= 0.98;
    x = OFFSET_X + pos*txtwidth + (pos == 0  || (flags & F_REPECHAGE) ? 0.0 : CLUB_WIDTH);

    if (white_y < 0.0001) {
        gboolean back = FALSE;

        if ((x + NAME_W) > W(0.95)) {
            x = x - NAME_W - 2.0*r2;
            back = TRUE;
        }

        cairo_move_to(pd->c, x, blue_y);
        cairo_rel_line_to(pd->c, NAME_W, 0);
        cairo_stroke(pd->c);

        if (blue_pts || white_pts || 
            (blue == GHOST && white >= COMPETITOR) || 
            (blue >= COMPETITOR && white == GHOST)) {
            if (blue_pts || white == GHOST)
                j = get_data(blue);
            else
                j = get_data(white);
            if (j) {
                cairo_text_extents(pd->c, j->last, &extents);
                snprintf(buf, sizeof(buf)-1, "%s", j->last);
                cairo_move_to(pd->c, x + TEXT_OFFSET, 
                              blue_y - extents.height - extents.y_bearing - H(0.005));
                cairo_show_text(pd->c, buf);
                free_judoka(j);
            }
        }
        return blue_y;
    }

    if (small) {
        cairo_move_to(pd->c, x, y);
        cairo_rel_line_to(pd->c, txtwidth + extra, 0);
        cairo_rectangle(pd->c, x, y - extents.height - H(0.005), 
                        txtwidth + extra, 
                        2.0*extents.height + H(0.01));

        blue_y = y + H(0.002);
        white_y = blue_y + extents.height + H(0.007);
    } else {
	w1 = txtwidth + extra - r2;
	x1 = x + w1;
	y1 = blue_y + dy;

        cairo_move_to(pd->c, x, blue_y);
        cairo_rel_line_to(pd->c, w1 - r1, 0);
	cairo_arc(pd->c, x1 - r1, blue_y + r1, r1, 1.5*PI, 0.0);
        cairo_move_to(pd->c, x1, blue_y + r1);
        cairo_rel_line_to(pd->c, 0, dy - r1 - r2);
	cairo_stroke(pd->c);

        cairo_move_to(pd->c, x, white_y);
        cairo_rel_line_to(pd->c, w1 - r1, 0);

        cairo_move_to(pd->c, x1, white_y - r1);
	cairo_arc(pd->c, x1 - r1, white_y - r1, r1, 0.0, 0.5*PI);

        cairo_move_to(pd->c, x1, white_y - r1);
        cairo_rel_line_to(pd->c, 0, -dy + r1 + r2);
        cairo_stroke(pd->c);

        /* fill white circle */
        cairo_save(pd->c);
        cairo_set_source_rgb(pd->c, 1.0, 1.0, 1.0);
        cairo_move_to(pd->c, x1+r2, y1);
	cairo_arc(pd->c, x1, y1, r2, 0.0, 2*PI);
        cairo_fill(pd->c);
        cairo_restore(pd->c);

	cairo_move_to(pd->c, x1 - extents1.width/2, 
		      y1 - extents1.height/2 - extents1.y_bearing);
	cairo_show_text(pd->c, numbuf);

        cairo_move_to(pd->c, x1+r2, y1);
	cairo_arc(pd->c, x1, y1, r2, 0.0, 2*PI);
	/*
        cairo_move_to(pd->c, x1 + 2*r, y1);
        cairo_rel_line_to(pd->c, NAME_W - 2*r, 0);
	*/
        cairo_stroke(pd->c);

	if (pd->systm.system >= SYSTEM_FRENCH_8) {
	    gint looser = french_matches[table][sys][comp_num][0];
	    if (looser < 0) {
		cairo_save(pd->c);
		cairo_set_font_size(pd->c, 0.6*TEXT_HEIGHT);
		snprintf(numbuf, sizeof(numbuf), "%d", -looser);
		cairo_text_extents(pd->c, numbuf, &extents1);
		cairo_move_to(pd->c, x, 
			      blue_y - extents1.y_bearing + 1.0);
		cairo_show_text(pd->c, numbuf);
		cairo_restore(pd->c);
	    }
	    looser = french_matches[table][sys][comp_num][1];
	    if (looser < 0) {
		cairo_save(pd->c);
		cairo_set_font_size(pd->c, 0.6*TEXT_HEIGHT);
		snprintf(numbuf, sizeof(numbuf), "%d", -looser);
		cairo_text_extents(pd->c, numbuf, &extents1);
		cairo_move_to(pd->c, x, 
			      white_y - extents1.y_bearing + 1.0);
		cairo_show_text(pd->c, numbuf);
		cairo_restore(pd->c);
	    }
	}
    }

    cairo_save(pd->c);
    /*
    if (small || crowded)
        cairo_set_font_size(pd->c, 0.8*TEXT_HEIGHT);
    */
    if (blue >= COMPETITOR) {
        j = get_data(blue);
        if (j) {
            if (only_last)
                snprintf(buf, sizeof(buf)-1, "%s", j->last);
            else if (number_b) {
                if (j->belt && grade_visible)
                    snprintf(buf, sizeof(buf)-1, "%d. %s %s, %s, %s", 
                             number_b, j->first, j->last, belts[j->belt], 
			     get_club_text(j, CLUB_TEXT_ABBREVIATION));
                else if (club_text == CLUB_TEXT_COUNTRY)
                    snprintf(buf, sizeof(buf)-1, "%d. %s  %s, %s", 
                             number_b, get_club_text(j, CLUB_TEXT_ABBREVIATION),
                             j->last, j->first);
                else
                    snprintf(buf, sizeof(buf)-1, "%d. %s %s, %s", 
                             number_b, j->first, j->last, 
			     get_club_text(j, CLUB_TEXT_ABBREVIATION));
            } else {
                if (j->belt && grade_visible)
                    snprintf(buf, sizeof(buf)-1, "%s %s, %s, %s", 
                             j->first, j->last, belts[j->belt], 
			     get_club_text(j, CLUB_TEXT_ABBREVIATION));
                else
                    snprintf(buf, sizeof(buf)-1, "%s %s, %s", 
                             j->first, j->last, 
			     get_club_text(j, CLUB_TEXT_ABBREVIATION));
            }

            cairo_text_extents(pd->c, buf, &extents);
            py = blue_y - extents.height - extents.y_bearing - H(0.005);
            cairo_move_to(pd->c, x + TEXT_OFFSET, py);
            if ((j->deleted & HANSOKUMAKE) == 0 || blue_pts || white_pts) {
                cairo_show_text(pd->c, buf);
                if (j->deleted & HANSOKUMAKE) {
                    cairo_move_to(pd->c, x + TEXT_OFFSET, blue_y - extents.height/2.0 - H(0.005));
                    cairo_rel_line_to(pd->c, extents.width, 0);
                    cairo_stroke(pd->c);
                }
            }
            free_judoka(j);

            sprintf(buf, "%d", blue_pts);
            cairo_text_extents(pd->c, buf, &extents);
            cairo_move_to(pd->c, 
                          extra + 
                          x + txtwidth - r2 + 2.0 - extents.x_bearing, 
                          py + (small ? 0 : 1.5*extents.height));
            cairo_show_text(pd->c, buf);
        }
    }

    if (white >= COMPETITOR) {
        j = get_data(white);
        if (j) {
            if (only_last)
                sprintf(buf, "%s", j->last);
            else if (number_w) {
                if (j->belt && grade_visible)
                    sprintf(buf, "%d. %s %s, %s, %s", 
                            number_w, j->first, j->last, belts[j->belt], 
			    get_club_text(j, CLUB_TEXT_ABBREVIATION));
                else if (club_text == CLUB_TEXT_COUNTRY)
                    snprintf(buf, sizeof(buf)-1, "%d. %s  %s, %s", 
                             number_w, get_club_text(j, CLUB_TEXT_ABBREVIATION),
                             j->last, j->first);
                else
                    sprintf(buf, "%d. %s %s, %s", 
                            number_w, j->first, j->last, 
			    get_club_text(j, CLUB_TEXT_ABBREVIATION));
            } else {
                if (j->belt && grade_visible)
                    sprintf(buf, "%s %s, %s, %s", 
                            j->first, j->last, belts[j->belt], 
			    get_club_text(j, CLUB_TEXT_ABBREVIATION));
                else
                    sprintf(buf, "%s %s, %s", 
                            j->first, j->last, 
			    get_club_text(j, CLUB_TEXT_ABBREVIATION));
            }

            cairo_text_extents(pd->c, buf, &extents);
            py = white_y - extents.height - extents.y_bearing - H(0.005);
            cairo_move_to(pd->c, x + TEXT_OFFSET, py);
            if ((j->deleted & HANSOKUMAKE) == 0 || blue_pts || white_pts) {
                cairo_show_text(pd->c, buf);
                if (j->deleted & HANSOKUMAKE) {
                    cairo_move_to(pd->c, x + TEXT_OFFSET, white_y - extents.height/2.0 - H(0.005));
                    cairo_rel_line_to(pd->c, extents.width, 0);
                    cairo_stroke(pd->c);
                }
            }
            free_judoka(j);

            sprintf(buf, "%d", white_pts);
            cairo_text_extents(pd->c, buf, &extents);
            cairo_move_to(pd->c, 
                          extra +
			  x + txtwidth - r2 + 2.0 - extents.x_bearing,
                          py + (small ? 0 : extents.height));
            cairo_show_text(pd->c, buf);
        }
    }

    cairo_restore(pd->c);

    return y;
}

static double colpos(struct paint_data *pd, struct table *t, int col)
{
    int i;
    double x = 0.0;
    for (i = 0; i < col; i++)
        x += W(t->columns[i]);
    return x;
}

static void create_table(struct paint_data *pd, struct table *t)
{
    int i;

    if (pd->row_height == 0) {
        g_print("ROW HEIGHT == 0!\n");
        pd->row_height = 1;
    }

    cairo_set_line_width(pd->c, THICK_LINE);
    cairo_rectangle(pd->c, t->position_x, t->position_y, 
                    colpos(pd, t, t->num_cols), 
                    (t->num_rows*pd->row_height + 1) * ROW_HEIGHT);
    cairo_stroke(pd->c);

    cairo_set_line_width(pd->c, THIN_LINE);
        
    for (i = 1; i <= t->num_rows; i++) {
        cairo_move_to(pd->c, t->position_x, t->position_y + ROW_HEIGHT*(pd->row_height*(i - 1) + 1));
        cairo_rel_line_to(pd->c, colpos(pd, t, t->num_cols), 0);
    }

    for (i = 1; i < t->num_cols; i++) {
        cairo_move_to(pd->c, t->position_x + colpos(pd, t, i), t->position_y + ROW_HEIGHT);
        cairo_rel_line_to(pd->c, 0, t->num_rows*ROW_HEIGHT*pd->row_height);
    }

    cairo_stroke(pd->c);

}

static void write_table(struct paint_data *pd, struct table *t, int row, int col, char *txt)
{
    cairo_text_extents_t extents;
    double x = t->position_x + colpos(pd, t, col);
    double y = t->position_y + row*ROW_HEIGHT;

    //cairo_set_font_size(pd->c, TEXT_HEIGHT);
    cairo_text_extents(pd->c, txt, &extents);

    if (row == 0 || (txt[0] >= '0' && txt[0] <= '9'))
        x += (W(t->columns[col]) - extents.width)/2.0;
    else
        x += TEXT_OFFSET;

    y = y - extents.y_bearing + (ROW_HEIGHT-extents.height)/2.0;
    cairo_move_to(pd->c, x, y);

    cairo_save(pd->c);
    if (row == 0)
        cairo_select_font_face(pd->c, font_face,
                               font_slant,
                               CAIRO_FONT_WEIGHT_BOLD);
    cairo_show_text(pd->c, txt);
    cairo_restore(pd->c);
}

static void write_table_h(struct paint_data *pd, struct table *t, int row, int col, gint del, char *txt)
{
    cairo_text_extents_t extents;
    double x = t->position_x + colpos(pd, t, col);
    double y = t->position_y + row*ROW_HEIGHT;

    if (del & HANSOKUMAKE) {
        double x1 = t->position_x + colpos(pd, t, col+1);
        cairo_save(pd->c);
        cairo_set_line_width(pd->c, THIN_LINE);
        cairo_move_to(pd->c, x + TEXT_OFFSET, y + 0.5*ROW_HEIGHT);
        cairo_line_to(pd->c, x1 - TEXT_OFFSET, y + 0.5*ROW_HEIGHT);
        cairo_stroke(pd->c);
        cairo_restore(pd->c);
    }

    cairo_text_extents(pd->c, txt, &extents);

    if (row == 0 || (txt[0] >= '0' && txt[0] <= '9'))
        x += (W(t->columns[col]) - extents.width)/2.0;
    else
        x += TEXT_OFFSET;

    y = y - extents.y_bearing + (ROW_HEIGHT-extents.height)/2.0;
    cairo_move_to(pd->c, x, y);

    cairo_save(pd->c);
    if (row == 0)
        cairo_select_font_face(pd->c, font_face,
                               font_slant,
                               CAIRO_FONT_WEIGHT_BOLD);
    cairo_show_text(pd->c, txt);
    cairo_restore(pd->c);
}

static void write_table_title(struct paint_data *pd, struct table *t, char *txt)
{
    cairo_text_extents_t extents;
    double x = t->position_x;
    double y = t->position_y;

    cairo_save(pd->c);
    cairo_set_font_size(pd->c, TEXT_HEIGHT*1.2);
    cairo_select_font_face(pd->c, font_face,
                           font_slant,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_text_extents(pd->c, txt, &extents);

    x = x + (colpos(pd, t, t->num_cols) - extents.width)/2.0;
    y = y - extents.height - TEXT_OFFSET - extents.y_bearing;

    cairo_move_to(pd->c, x, y);
    cairo_show_text(pd->c, txt);
    cairo_restore(pd->c);
}

static void paint_table_cell(struct paint_data *pd, struct table *t, int row, int col, 
                            gdouble red, gdouble green, gdouble blue)
{
    double x = t->position_x + colpos(pd, t, col);
    double y = t->position_y + row*ROW_HEIGHT;

    cairo_move_to(pd->c, x, y);

    cairo_save(pd->c);
    cairo_set_source_rgb(pd->c, red, green, blue);
    cairo_rectangle(pd->c, x, y, W(t->columns[col]), ROW_HEIGHT);
    cairo_fill(pd->c);

    cairo_restore(pd->c);
}

static void paint_pool(struct paint_data *pd, gint category, struct judoka *ctg, gint num_judokas, gboolean dpool2)
{
    struct pool_matches pm;
    gint i;

    pd->row_height = 1;

    fill_pool_struct(category, num_judokas, &pm, dpool2);

    if (dpool2)
        num_judokas = 4;

    if (pm.finished)
        get_pool_winner(num_judokas, pm.c, pm.yes, pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched);

    /* competitor table */
    judoka_table.position_y = judoka_2_table.position_y = POOL_JUDOKAS_Y;
    judoka_table.num_rows = judoka_2_table.num_rows = num_judokas;

    if (num_judokas == 2 && three_matches_for_two) {
        judoka_table.num_cols = 10;
        judoka_2_table.num_cols = 9;
    } else {
        judoka_table.num_cols = num_judokas + 4;
        judoka_2_table.num_cols = num_judokas + 3;
    }

    if (grade_visible)
        create_table(pd, &judoka_table);
    else
        create_table(pd, &judoka_2_table);

    write_table_title(pd, &judoka_table, _T(competitor));
    write_table(pd, &judoka_table, 0, 0, _T(number));
    write_table(pd, &judoka_table, 0, 1, _T(name));
    if (grade_visible)
        write_table(pd, &judoka_table, 0, 2, _T(grade));
    write_table(pd, &judoka_table, 0, 3, _T(club));
    for (i = 1; i <= num_judokas; i++) {
        char num[5];
        sprintf(num, "%d", i);
        write_table(pd, &judoka_table, 0, 3 + i, num);
        if (num_judokas == 2 && three_matches_for_two) {
            write_table(pd, &judoka_table, 0, 5 + i, num);
            write_table(pd, &judoka_table, 0, 7 + i, num);
        }
    }

    for (i = 1; i <= num_judokas; i++) {
        char num[5], name[60];

        if (pm.j[i] == NULL)
            continue;

        sprintf(num, "%d", i);
        if (weights_in_sheets)
            sprintf(name, "%s %s  (%d,%02d)", 
                    pm.j[i]->first, pm.j[i]->last,
                    pm.j[i]->weight/1000, (pm.j[i]->weight%1000)/10);
        else
            sprintf(name, "%s %s", pm.j[i]->first, pm.j[i]->last);
        write_table(pd, &judoka_table, i, 0, num);
        write_table_h(pd, &judoka_table, i, 1, pm.j[i]->deleted, name);
        if (grade_visible)
            write_table(pd, &judoka_table, i, 2, belts[pm.j[i]->belt]);
        write_table(pd, &judoka_table, i, 3, (char *)get_club_text(pm.j[i], 0));
    }

    /* match table */
    match_table.position_y = judoka_table.position_y + 
        (num_judokas + 3)*ROW_HEIGHT;
    match_table.num_rows = pm.num_matches;
    create_table(pd, &match_table);
    write_table_title(pd, &match_table, _T(matches));

    WRITE_TABLE(match_table, 0, 0, "%s", _T(match));
    WRITE_TABLE(match_table, 0, 1, "%s", info_white_first ? _T(white) : _T(blue));
    WRITE_TABLE(match_table, 0, 4, "%s", info_white_first ? _T(blue) : _T(white));
    WRITE_TABLE(match_table, 0, 5, "%s", _T(result));
    WRITE_TABLE(match_table, 0, 6, "%s", _T(time));

    for (i = 1; i <= pm.num_matches ; i++) {
        gint blue = pools[num_judokas][i-1][0];
        gint white = pools[num_judokas][i-1][1];

        WRITE_TABLE(match_table, i, 2, "%d", blue);
        WRITE_TABLE(match_table, i, 3, "%d", white);

        if (pm.j[blue] == NULL || pm.j[white] == NULL)
            continue;

        WRITE_TABLE(match_table, i, 0, "%d", i);
        WRITE_TABLE(match_table, i, 1, "%s %s", pm.j[blue]->first, pm.j[blue]->last);
        WRITE_TABLE(match_table, i, 4, "%s %s", pm.j[white]->first, pm.j[white]->last);
        if (pm.m[i].blue_points || pm.m[i].white_points)
            WRITE_TABLE(match_table, i, 5, "%d - %d", pm.m[i].blue_points, pm.m[i].white_points);
        if (pm.m[i].blue_points || pm.m[i].white_points)
            WRITE_TABLE(match_table, i, 6, "%d:%02d", pm.m[i].match_time/60, pm.m[i].match_time%60);

        if (pm.m[i].blue_points) {
#ifdef ONE_MATCH
            WRITE_TABLE(judoka_table, blue, white + 3, "%d", pm.m[i].blue_points);
#else
            if (num_judokas == 2 && three_matches_for_two) {
                WRITE_TABLE(judoka_table, blue, white + 1 + i*2, "%d", pm.m[i].blue_points);
            } else {
                WRITE_TABLE(judoka_table, blue, white + 3, "%d", pm.m[i].blue_points);
            }
#endif
        } else if (pm.m[i].white_points) {
#ifdef ONE_MATCH
            WRITE_TABLE(judoka_table, white, blue + 3, "%d", pm.m[i].white_points);
#else
            if (num_judokas == 2 && three_matches_for_two)
                WRITE_TABLE(judoka_table, white, blue + 1 + i*2, "%d", pm.m[i].white_points);
            else
                WRITE_TABLE(judoka_table, white, blue + 3, "%d", pm.m[i].white_points);
#endif
        }
    }

    /* win table */
    win_table.position_y = judoka_table.position_y;
    win_table.num_rows = num_judokas;
#ifdef ONE_MATCH
    win_table.position_x = colpos(pd, &judoka_table, num_judokas + 4) + judoka_table.position_x;
#else
    win_table.position_x = colpos(pd, &judoka_table, 
                                  (num_judokas==2 && three_matches_for_two)?10:(num_judokas + 4)) 
        + judoka_table.position_x;
#endif
    create_table(pd, &win_table);

    WRITE_TABLE(win_table, 0, 0, "%s", _T(win));
    WRITE_TABLE(win_table, 0, 1, "%s", _T(points));
    WRITE_TABLE(win_table, 0, 2, "%s", _T(position));

    for (i = 1; i <= num_judokas; i++) {
        if (pm.wins[i] || pm.finished)
            WRITE_TABLE(win_table, i, 0, "%d", pm.wins[i]);
        if (pm.pts[i] || pm.finished)
            WRITE_TABLE(win_table, i, 1, "%d", pm.pts[i]);
        if (pm.finished)
            WRITE_TABLE(win_table, pm.c[i], 2, "%d", draw_system == DRAW_SPANISH && i == 4 ? 3 : i);
    }


    /* results */
    result_table.position_y = match_table.position_y + 
        (pm.num_matches + 3)*ROW_HEIGHT;
    result_table.num_rows = num_judokas;
    create_table(pd, &result_table);
    write_table_title(pd, &result_table, _T(results));

    WRITE_TABLE(result_table, 0, 0, "%s", _T(position));
    WRITE_TABLE(result_table, 0, 1, "%s", _T(name));

    for (i = 1; i <= num_judokas; i++) {
        WRITE_TABLE(result_table, i, 0, "%d", draw_system == DRAW_SPANISH && i == 4 ? 3 : i);
        if (pm.finished == FALSE || pm.j[pm.c[i]] == NULL)
            continue;
        WRITE_TABLE_H(result_table, i, 1, pm.j[pm.c[i]]->deleted, "%s %s, %s", 
                      pm.j[pm.c[i]]->first, pm.j[pm.c[i]]->last, 
		      get_club_text(pm.j[pm.c[i]], 0));
    }

    /* clean up */
    empty_pool_struct(&pm);
}

static struct table pool_table_2 = {
    0, 0, 0, 0, 
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};
static double col_width_2[] = {0.02, 0.12, 0.02, 0.02, 0.02};

static void paint_pool_2(struct paint_data *pd, gint category, struct judoka *ctg, gint num_judokas)
{
    struct pool_matches pm;
    gint i;
    gdouble tot_width = 0, match_col_width;

    pd->row_height = 2;

    fill_pool_struct(category, num_judokas, &pm, FALSE);

    if (pm.finished)
        get_pool_winner(num_judokas, pm.c, pm.yes, pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched);

    pool_table_2.position_x = POOL_JUDOKAS_X;
    pool_table_2.position_y = POOL_JUDOKAS_Y;
    pool_table_2.num_rows = num_judokas;
    pool_table_2.num_cols = pm.num_matches + 5;
    pool_table_2.columns[0] = col_width_2[0];
    pool_table_2.columns[1] = col_width_2[1];
    pool_table_2.columns[pm.num_matches + 2] = col_width_2[2];
    pool_table_2.columns[pm.num_matches + 3] = col_width_2[3];
    pool_table_2.columns[pm.num_matches + 4] = col_width_2[4];

    for (i = 0; i < 5; i++)
        tot_width += col_width_2[i];
    match_col_width = (0.90 - tot_width)/21.0;

    for (i = 0; i < pm.num_matches; i++)
        pool_table_2.columns[2 + i] = match_col_width;

    create_table(pd, &pool_table_2);

    write_table(pd, &pool_table_2, 0, 0, _T(number));
    write_table(pd, &pool_table_2, 0, 1, _T(name));
    WRITE_TABLE(pool_table_2, 0, pm.num_matches + 2, "%s", _T(win));
    WRITE_TABLE(pool_table_2, 0, pm.num_matches + 3, "%s", _T(points));
    WRITE_TABLE(pool_table_2, 0, pm.num_matches + 4, "%s", _T(position));

    for (i = 0; i < pm.num_matches; i++) {
        gint j;
        WRITE_TABLE(pool_table_2, 0, i + 2, "%d", i+1);

        for (j = 1; j <= num_judokas; j++) {
            if (pools[num_judokas][i][0] != j &&
                pools[num_judokas][i][1] != j) {
                paint_table_cell(pd, &pool_table_2, j*2 - 1, i + 2, 0.4, 0.4, 0.4);
                paint_table_cell(pd, &pool_table_2, j*2, i + 2, 0.4, 0.4, 0.4);
            }
        }
    }
    
    for (i = 1; i <= num_judokas; i++) {
        char num[5], name[60];

        if (pm.j[i] == NULL)
            continue;

        sprintf(num, "%d", i);
        if (weights_in_sheets)
            sprintf(name, "%s %s  (%d,%02d)", 
                    pm.j[i]->first, pm.j[i]->last,
                    pm.j[i]->weight/1000, (pm.j[i]->weight%1000)/10);
        else
            sprintf(name, "%s %s", pm.j[i]->first, pm.j[i]->last);
        write_table(pd, &pool_table_2, 2*i-1, 0, num);
        write_table_h(pd, &pool_table_2, 2*i-1, 1, pm.j[i]->deleted, name);
        sprintf(name, "%s", get_club_text(pm.j[i], 0));
        write_table(pd, &pool_table_2, 2*i, 1, name);
        /*if (grade_visible)
          write_table(pd, &pool_table_2, i, 2, belts[pm.j[i]->belt]);
          write_table(pd, &pool_table_2, i, 3, (char *)get_club_text(pm.j[i], 0));*/
    }

    for (i = 1; i <= pm.num_matches ; i++) {
        gint blue = pools[num_judokas][i-1][0];
        gint white = pools[num_judokas][i-1][1];

        if (pm.j[blue] == NULL || pm.j[white] == NULL)
            continue;

        if (pm.m[i].blue_points)
            WRITE_TABLE(pool_table_2, blue*2-1, i+1, "%d", pm.m[i].blue_points);
        if (pm.m[i].white_points)
            WRITE_TABLE(pool_table_2, white*2-1, i+1, "%d", pm.m[i].white_points);
        /*if (pm.m[i].blue_points || pm.m[i].white_points)
          WRITE_TABLE(match_table, i, 6, "%d:%02d", pm.m[i].match_time/60, pm.m[i].match_time%60);*/
    }

    for (i = 1; i <= num_judokas; i++) {
        if (pm.wins[i] || pm.finished)
            WRITE_TABLE(pool_table_2, 2*i-1, 2+pm.num_matches, "%d", pm.wins[i]);
        if (pm.pts[i] || pm.finished)
            WRITE_TABLE(pool_table_2, 2*i-1, 3+pm.num_matches, "%d", pm.pts[i]);
        if (pm.finished)
            WRITE_TABLE(pool_table_2, 2*pm.c[i]-1, 4+pm.num_matches, "%d", draw_system == DRAW_SPANISH && i == 4 ? 3 : i);
    }

    pd->row_height = 1;
    
    /* results */
    result_table.position_y = H(0.6);
    result_table.num_rows = num_judokas;
    create_table(pd, &result_table);
    write_table_title(pd, &result_table, _T(results));

    WRITE_TABLE(result_table, 0, 0, "%s", _T(position));
    WRITE_TABLE(result_table, 0, 1, "%s", _T(name));

    for (i = 1; i <= num_judokas; i++) {
        WRITE_TABLE(result_table, i, 0, "%d", draw_system == DRAW_SPANISH && i == 4 ? 3 : i);
        if (pm.finished == FALSE || pm.j[pm.c[i]] == NULL)
            continue;
        WRITE_TABLE_H(result_table, i, 1, pm.j[pm.c[i]]->deleted, "%s %s, %s", 
                      pm.j[pm.c[i]]->first, pm.j[pm.c[i]]->last, 
		      get_club_text(pm.j[pm.c[i]], 0));
    }

    /* clean up */
    empty_pool_struct(&pm);
}


static gdouble row_height[11] = {
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.9, 0.8, 0.7
};

static void paint_dpool(struct paint_data *pd, gint category, struct judoka *ctg, gint num_judokas, gboolean dpool2)
{
    struct pool_matches pm;
    gint i;
    struct judoka *j1 = NULL;
    int num_pool_a, num_pool_b, pos_a, pos_b, gold = 0, silver = 0, bronze1 = 0, bronze2 = 0;
    double pos_match_a, pos_match_b, pos_match_f, x1, x2, pos_judoka_a, pos_judoka_b;
    gboolean yes_a[21], yes_b[21];
    gint c_a[21], c_b[21];

    pd->row_height = 1;

    ROW_HEIGHT *= row_height[num_judokas];

    fill_pool_struct(category, num_judokas, &pm, FALSE);

    num_pool_a = num_judokas - num_judokas/2;
    num_pool_b = num_judokas - num_pool_a;

    pos_judoka_a = POOL_JUDOKAS_Y;
    pos_match_a = pos_judoka_a + (num_pool_a + 3) * ROW_HEIGHT;
    pos_judoka_b = pos_match_a + (num_matches(SYSTEM_POOL, num_pool_a) + 3) * ROW_HEIGHT;
    pos_match_b = pos_judoka_b + (num_pool_b + 3) * ROW_HEIGHT;

    /* competitor table A */
    judoka_table.position_y = judoka_2_table.position_y = pos_judoka_a;
    judoka_table.num_rows = judoka_2_table.num_rows = num_pool_a;
    judoka_table.num_cols = num_pool_a + 4;
    judoka_2_table.num_cols = num_pool_a + 3;
    if (grade_visible)
        create_table(pd, &judoka_table);
    else
        create_table(pd, &judoka_2_table);

    write_table_title(pd, &judoka_table, _T(competitora));
    write_table(pd, &judoka_table, 0, 0, _T(number));
    write_table(pd, &judoka_table, 0, 1, _T(name));
    if (grade_visible)
        write_table(pd, &judoka_table, 0, 2, _T(grade));
    write_table(pd, &judoka_table, 0, 3, _T(club));
    for (i = 1; i <= num_pool_a; i++) {
        WRITE_TABLE(judoka_table, 0, 3 + i, "%d", i);
    }

    for (i = 1; i <= num_pool_a; i++) {
        if (pm.j[i] == NULL)
            continue;
        WRITE_TABLE(judoka_table, i, 0, "%d", i);
        if (weights_in_sheets)
            WRITE_TABLE_H(judoka_table, i, 1, pm.j[i]->deleted, "%s %s  (%d,%02d)", 
                          pm.j[i]->first, pm.j[i]->last,
                          pm.j[i]->weight/1000, (pm.j[i]->weight%1000)/10);
        else
            WRITE_TABLE_H(judoka_table, i, 1, pm.j[i]->deleted, "%s %s", 
                          pm.j[i]->first, pm.j[i]->last);
        if (grade_visible)
            WRITE_TABLE(judoka_table, i, 2, "%s", belts[pm.j[i]->belt]);
        WRITE_TABLE(judoka_table, i, 3, "%s", get_club_text(pm.j[i], 0));
    }

    /* competitor table B */
    judoka_table.position_y = judoka_2_table.position_y = pos_judoka_b;
    judoka_table.num_rows = judoka_2_table.num_rows = num_pool_b;
    judoka_table.num_cols = num_pool_b + 4;
    judoka_2_table.num_cols = num_pool_b + 3;
    if (grade_visible)
        create_table(pd, &judoka_table);
    else
        create_table(pd, &judoka_2_table);

    write_table_title(pd, &judoka_table, _T(competitorb));
    write_table(pd, &judoka_table, 0, 0, _T(number));
    write_table(pd, &judoka_table, 0, 1, _T(name));
    if (grade_visible)
        write_table(pd, &judoka_table, 0, 2, _T(grade));
    write_table(pd, &judoka_table, 0, 3, _T(club));
    for (i = num_pool_a+1; i <= num_judokas; i++) {
        WRITE_TABLE(judoka_table, 0, 3 + i - num_pool_a, "%d", i);
    }

    for (i = num_pool_a+1; i <= num_judokas; i++) {
        if (pm.j[i] == NULL)
            continue;

        WRITE_TABLE(judoka_table, i - num_pool_a, 0, "%d", i);
        if (weights_in_sheets)
            WRITE_TABLE_H(judoka_table, i - num_pool_a, 1, pm.j[i]->deleted, "%s %s  (%d,%02d)", 
                          pm.j[i]->first, pm.j[i]->last,
                          pm.j[i]->weight/1000, (pm.j[i]->weight%1000)/10);
        else
            WRITE_TABLE_H(judoka_table, i - num_pool_a, 1, pm.j[i]->deleted, "%s %s", 
                          pm.j[i]->first, pm.j[i]->last);
        if (grade_visible)
            WRITE_TABLE(judoka_table, i - num_pool_a, 2, "%s", belts[pm.j[i]->belt]);
        WRITE_TABLE(judoka_table, i - num_pool_a, 3, "%s", 
		    get_club_text(pm.j[i], 0));
    }

    /* match table A */
    match_table.num_rows = num_matches(SYSTEM_POOL, num_pool_a);
    match_table.position_y = pos_match_a;
    create_table(pd, &match_table);
    write_table_title(pd, &match_table, _T(matchesa));

    WRITE_TABLE(match_table, 0, 0, "%s", _T(match));
    WRITE_TABLE(match_table, 0, 1, "%s", info_white_first ? _T(white) : _T(blue));
    WRITE_TABLE(match_table, 0, 4, "%s", info_white_first ? _T(blue) : _T(white));
    WRITE_TABLE(match_table, 0, 5, "%s", _T(result));
    WRITE_TABLE(match_table, 0, 6, "%s", _T(time));

    /* match table B */
    match_table.num_rows = num_matches(SYSTEM_POOL, num_pool_b);
    match_table.position_y = pos_match_b;
    create_table(pd, &match_table);
    write_table_title(pd, &match_table, _T(matchesb));

    WRITE_TABLE(match_table, 0, 0, "%s", _T(match));
    WRITE_TABLE(match_table, 0, 1, "%s", info_white_first ? _T(white) : _T(blue));
    WRITE_TABLE(match_table, 0, 4, "%s", info_white_first ? _T(blue) : _T(white));
    WRITE_TABLE(match_table, 0, 5, "%s", _T(result));
    WRITE_TABLE(match_table, 0, 6, "%s", _T(time));

    /* matches */
    pos_a = pos_b = 1;

    for (i = 1; i <= num_matches(pd->systm.system, num_judokas) ; i++) {
        gint blue = poolsd[num_judokas][i-1][0];
        gint white = poolsd[num_judokas][i-1][1];
        gint ix = blue > num_pool_a ? pos_b++ : pos_a++;

        if (blue > num_pool_a) {
            judoka_table.position_y = pos_judoka_b;
            match_table.position_y = pos_match_b;
        } else {
            judoka_table.position_y = pos_judoka_a;
            match_table.position_y = pos_match_a;
        }
                                
        WRITE_TABLE(match_table, ix, 2, "%d", blue);
        WRITE_TABLE(match_table, ix, 3, "%d", white);

        if (pm.j[blue] && pm.j[white]) {
            WRITE_TABLE(match_table, ix, 0, "%d", i);
            WRITE_TABLE(match_table, ix, 1, "%s %s", pm.j[blue]->first, pm.j[blue]->last);
            WRITE_TABLE(match_table, ix, 4, "%s %s", pm.j[white]->first, pm.j[white]->last);
            if (pm.m[i].blue_points || pm.m[i].white_points)
                WRITE_TABLE(match_table, ix, 5, "%d - %d", pm.m[i].blue_points, pm.m[i].white_points);
            if (pm.m[i].blue_points || pm.m[i].white_points)
                WRITE_TABLE(match_table, ix, 6, "%d:%02d", pm.m[i].match_time/60, pm.m[i].match_time%60);
        }

        if (blue > num_pool_a) {
            blue -= num_pool_a;
            white -= num_pool_a;
        }
        if (pm.m[i].blue_points)
            WRITE_TABLE(judoka_table, blue, white + 3, "%d", pm.m[i].blue_points);
        else if (pm.m[i].white_points)
            WRITE_TABLE(judoka_table, white, blue + 3, "%d", pm.m[i].white_points);
    }

    if (!dpool2) {
        pos_match_f = pos_match_b + (num_matches(SYSTEM_POOL, num_pool_b) + 3) * ROW_HEIGHT;

        i = num_matches(pd->systm.system, num_judokas) + 1;

        cairo_text_extents_t extents;
        cairo_text_extents(pd->c, "B2", &extents);

        cairo_move_to(pd->c, OFFSET_X, pos_match_f + extents.height*1.2);
        cairo_show_text(pd->c, "A1");
        cairo_move_to(pd->c, OFFSET_X, pos_match_f + NAME_S*1 + extents.height*1.2);
        cairo_show_text(pd->c, "B2");
        cairo_move_to(pd->c, OFFSET_X, pos_match_f + NAME_S*2 + extents.height*1.2);
        cairo_show_text(pd->c, "B1");
        cairo_move_to(pd->c, OFFSET_X, pos_match_f + NAME_S*3 + extents.height*1.2);
        cairo_show_text(pd->c, "A2");


        /* first semifinal */
        x1 = paint_comp(pd, &pm, 0,
                        pos_match_f, pos_match_f + NAME_S, 
                        pm.m[i].blue, pm.m[i].white,
                        pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);

        if (pm.m[i].blue_points)
            bronze1 = pm.m[i].white;
        else
            bronze1 = pm.m[i].blue;

        i++;

        /* second semifinal */
        x2 = paint_comp(pd, &pm, 0, 
                        pos_match_f + 2*NAME_S, pos_match_f + 3*NAME_S, 
                        pm.m[i].blue, pm.m[i].white,
                        pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);

        if (pm.m[i].blue_points)
            bronze2 = pm.m[i].white;
        else
            bronze2 = pm.m[i].blue;

        i++;

        /* final */
        x2 = paint_comp(pd, &pm, 1, 
                        x1, x2,
                        pm.m[i].blue, pm.m[i].white,
                        pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);

        if (pm.m[i].blue_points || pm.m[i].white == GHOST) {
            gold = pm.m[i].blue;
            silver = pm.m[i].white;
        } else if (pm.m[i].white_points || pm.m[i].blue == GHOST) {
            gold = pm.m[i].white;
            silver = pm.m[i].blue;
        }

        x2 = paint_comp(pd, &pm, 2, 
                        x2, 0,
                        pm.m[i].blue, pm.m[i].white,
                        pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);

    }

    /* win table */

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
    
    get_pool_winner(num_pool_a, c_a, yes_a, pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched);
    get_pool_winner(num_pool_b, c_b, yes_b, pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched);

    win_table.position_y = pos_judoka_a;
    win_table.num_rows = num_pool_a;
    win_table.position_x = colpos(pd, &judoka_table, num_pool_a + 4) + judoka_table.position_x;
    create_table(pd, &win_table);

    WRITE_TABLE(win_table, 0, 0, "%s", _T(win));
    WRITE_TABLE(win_table, 0, 1, "%s", _T(points));
    WRITE_TABLE(win_table, 0, 2, "%s", _T(position));

    for (i = 1; i <= num_pool_a; i++) {
        if (pm.wins[i] || pm.finished)
            WRITE_TABLE(win_table, i, 0, "%d", pm.wins[i]);
        if (pm.pts[i] || pm.finished)
            WRITE_TABLE(win_table, i, 1, "%d", pm.pts[i]);
        if (pm.finished && c_a[i] <= num_pool_a)
            WRITE_TABLE(win_table, c_a[i], 2, "%d", i);
    }

    win_table.position_y = pos_judoka_b;
    win_table.num_rows = num_pool_b;
    win_table.position_x = colpos(pd, &judoka_table, num_pool_b + 4) + judoka_table.position_x;
    create_table(pd, &win_table);

    WRITE_TABLE(win_table, 0, 0, "%s", _T(win));
    WRITE_TABLE(win_table, 0, 1, "%s", _T(points));
    WRITE_TABLE(win_table, 0, 2, "%s", _T(position));

    for (i = num_pool_a+1; i <= num_judokas; i++) {
        gint line = c_b[i-num_pool_a]-num_pool_a;
        if (pm.wins[i] || pm.finished)
            WRITE_TABLE(win_table, i-num_pool_a, 0, "%d", pm.wins[i]);
        if (pm.pts[i] || pm.finished)
            WRITE_TABLE(win_table, i-num_pool_a, 1, "%d", pm.pts[i]);
        if (pm.finished && line >= 1 && line <= num_pool_b)
            WRITE_TABLE(win_table, line, 2, "%d", i-num_pool_a);
    }

    if (!dpool2) {
        /* results */
        result_table.num_rows = 4;
        result_table.position_y = pos_match_b + (num_matches(SYSTEM_POOL, num_pool_b) + 5) * ROW_HEIGHT + 3*NAME_S;
        create_table(pd, &result_table);
        write_table_title(pd, &result_table, _T(results));

        WRITE_TABLE(result_table, 0, 0, "%s", _T(position));
        WRITE_TABLE(result_table, 0, 1, "%s", _T(name));

        WRITE_TABLE(result_table, 1, 0, "1");
        WRITE_TABLE(result_table, 2, 0, "2");
        WRITE_TABLE(result_table, 3, 0, "3");
        WRITE_TABLE(result_table, 4, 0, "3");
                
        if (gold && (j1 = get_data(gold))) {
            WRITE_TABLE_H(result_table, 1, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
            free_judoka(j1);
        }
        if (gold && (j1 = get_data(silver))) {
            WRITE_TABLE_H(result_table, 2, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
            free_judoka(j1);
        }
        if (gold && (j1 = get_data(bronze1))) {
            WRITE_TABLE_H(result_table, 3, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
            free_judoka(j1);
        }
        if (gold && (j1 = get_data(bronze2))) {
            WRITE_TABLE_H(result_table, 4, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
            free_judoka(j1);
        }
    }

 out:
    /* clean up */
    empty_pool_struct(&pm);
}


static void paint_qpool(struct paint_data *pd, gint category, struct judoka *ctg, gint num_judokas)
{
    struct pool_matches pm;
    gint i, j, pool;
    struct judoka *j1 = NULL;
    gint pos[4], gold = 0, silver = 0, bronze1 = 0, bronze2 = 0;
    double pos_match[4], pos_match_f, pos_judoka[4];
    gboolean yes[4][21];
    gint c[4][21];
    gint pool_start[4];
    gint pool_size[4];
    gint size = num_judokas/4;
    gint rem = num_judokas - size*4;

    pd->row_height = 1;

    for (i = 0; i < 4; i++) {
        pool_size[i] = size;
        pos[i] = 1;
    }

    for (i = 0; i < rem; i++)
        pool_size[i]++;

    gint start = 0;
    for (i = 0; i < 4; i++) {
        pool_start[i] = start;
        start += pool_size[i];
    }

    //ROW_HEIGHT *= row_height[num_judokas];

    fill_pool_struct(category, num_judokas, &pm, FALSE);

    pos_judoka[0] = POOL_JUDOKAS_Y;
    pos_match[0] = pos_judoka[0] + (pool_size[0] + 3) * ROW_HEIGHT;

    pos_judoka[1] = pos_match[0] + (num_matches(SYSTEM_POOL, pool_size[0]) + 3) * ROW_HEIGHT;
    pos_match[1] = pos_judoka[1] + (pool_size[1] + 3) * ROW_HEIGHT;

    pos_judoka[2] = POOL_JUDOKAS_Y;
    pos_match[2] = pos_judoka[2] + (pool_size[2] + 3) * ROW_HEIGHT;

    pos_judoka[3] = pos_match[2] + (num_matches(SYSTEM_POOL, pool_size[2]) + 3) * ROW_HEIGHT;
    pos_match[3] = pos_judoka[3] + (pool_size[3] + 3) * ROW_HEIGHT;

    for (pool = 0; pool < 4; pool++) {
        if ((pd->page == 0 && (pool == 0 || pool == 1)) ||
            (pd->page == 1 && (pool == 2 || pool == 3))) {
            /* competitor table */
            judoka_table.position_y = judoka_2_table.position_y = pos_judoka[pool];
            judoka_table.num_rows = judoka_2_table.num_rows = pool_size[pool];
            judoka_table.num_cols = pool_size[pool] + 4;
            judoka_2_table.num_cols = pool_size[pool] + 3;
            if (grade_visible)
                create_table(pd, &judoka_table);
            else
                create_table(pd, &judoka_2_table);

            write_table_title(pd, &judoka_table, print_texts[competitoratext+pool][print_lang]);
            write_table(pd, &judoka_table, 0, 0, _T(number));
            write_table(pd, &judoka_table, 0, 1, _T(name));
            if (grade_visible)
                write_table(pd, &judoka_table, 0, 2, _T(grade));
            write_table(pd, &judoka_table, 0, 3, _T(club));
            for (i = 1; i <= pool_size[pool]; i++) {
                WRITE_TABLE(judoka_table, 0, 3 + i, "%d", i + pool_start[pool]);
            }

            for (i = 1; i <= pool_size[pool]; i++) {
                if (pm.j[i+pool_start[pool]] == NULL)
                    continue;

                WRITE_TABLE(judoka_table, i, 0, "%d", i+pool_start[pool]);
                if (weights_in_sheets)
                    WRITE_TABLE_H(judoka_table, i, 1, pm.j[i+pool_start[pool]]->deleted, "%s %s  (%d,%02d)", 
                                  pm.j[i+pool_start[pool]]->first, pm.j[i+pool_start[pool]]->last,
                                  pm.j[i+pool_start[pool]]->weight/1000, (pm.j[i+pool_start[pool]]->weight%1000)/10);
                else
                    WRITE_TABLE_H(judoka_table, i, 1, pm.j[i+pool_start[pool]]->deleted, "%s %s", 
                                  pm.j[i+pool_start[pool]]->first, pm.j[i+pool_start[pool]]->last);
                if (grade_visible)
                    WRITE_TABLE(judoka_table, i, 2, "%s", belts[pm.j[i+pool_start[pool]]->belt]);
                WRITE_TABLE(judoka_table, i, 3, "%s", get_club_text(pm.j[i+pool_start[pool]], 0));
            }

            /* win table */
            gint k;
            memset(yes, 0, sizeof(yes));
            memset(c, 0, sizeof(c));

            for (i = 1; i <= num_judokas; i++) {
                for (k = 3; k >= 0; k--) {
                    if (i-1 >= pool_start[k]) {
                        yes[k][i] = TRUE;
                        break;
                    }
                }
            }
    
            get_pool_winner(pool_size[pool], c[pool], yes[pool], pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched);
            gboolean pool_done = pool_finished(num_judokas, num_matches(pd->systm.system, num_judokas), 
                                               SYSTEM_QPOOL, yes[pool], &pm);

            win_table.position_y = pos_judoka[pool];
            win_table.num_rows = pool_size[pool];
            win_table.position_x = colpos(pd, &judoka_table, pool_size[pool] + 4) + judoka_table.position_x;
            create_table(pd, &win_table);

            WRITE_TABLE(win_table, 0, 0, "%s", _T(win));
            WRITE_TABLE(win_table, 0, 1, "%s", _T(points));
            WRITE_TABLE(win_table, 0, 2, "%s", _T(position));

            for (i = 1; i <= pool_size[pool]; i++) {
                if (pm.wins[i+pool_start[pool]] || pm.finished)
                    WRITE_TABLE(win_table, i, 0, "%d", pm.wins[i+pool_start[pool]]);
                if (pm.pts[i+pool_start[pool]] || pm.finished)
                    WRITE_TABLE(win_table, i, 1, "%d", pm.pts[i+pool_start[pool]]);
                if (pm.finished || pool_done)
                    WRITE_TABLE(win_table, c[pool][i] - pool_start[pool], 2, "%d", i);
            }

            /* matches */
            gint ix = 0;

            /* match table */
            match_table.num_rows = num_matches(SYSTEM_POOL, pool_size[pool]);
            match_table.position_y = pos_match[pool];
            create_table(pd, &match_table);
            write_table_title(pd, &match_table, print_texts[matchesatext+pool][print_lang]);

            WRITE_TABLE(match_table, 0, 0, "%s", _T(match));
            WRITE_TABLE(match_table, 0, 1, "%s", info_white_first ? _T(white) : _T(blue));
            WRITE_TABLE(match_table, 0, 4, "%s", info_white_first ? _T(blue) : _T(white));
            WRITE_TABLE(match_table, 0, 5, "%s", _T(result));
            WRITE_TABLE(match_table, 0, 6, "%s", _T(time));

            for (i = 1; i <= num_matches(pd->systm.system, num_judokas); i++) {
                gint blue = poolsq[num_judokas][i-1][0];
                gint white = poolsq[num_judokas][i-1][1];

                if (blue-1 < pool_start[pool] && pool == 3)
                    continue;

                if (pool < 3 && (blue-1 < pool_start[pool] || blue-1 >= pool_start[pool+1]))
                    continue;

                ix++;
                match_table.position_y = pos_match[pool];
                                
                WRITE_TABLE(match_table, ix, 2, "%d", blue);
                WRITE_TABLE(match_table, ix, 3, "%d", white);

                if (pm.j[blue] && pm.j[white]) {
                    WRITE_TABLE(match_table, ix, 0, "%d", i);
                    WRITE_TABLE(match_table, ix, 1, "%s %s", pm.j[blue]->first, pm.j[blue]->last);
                    WRITE_TABLE(match_table, ix, 4, "%s %s", pm.j[white]->first, pm.j[white]->last);
                    if (pm.m[i].blue_points || pm.m[i].white_points)
                        WRITE_TABLE(match_table, ix, 5, "%d - %d", pm.m[i].blue_points, pm.m[i].white_points);
                    if (pm.m[i].blue_points || pm.m[i].white_points)
                        WRITE_TABLE(match_table, ix, 6, "%d:%02d", pm.m[i].match_time/60, pm.m[i].match_time%60);
                }

                blue -= pool_start[pool];
                white -= pool_start[pool];

                if (pm.m[i].blue_points)
                    WRITE_TABLE(judoka_table, blue, white + 3, "%d", pm.m[i].blue_points);
                else if (pm.m[i].white_points)
                    WRITE_TABLE(judoka_table, white, blue + 3, "%d", pm.m[i].white_points);
            } // for i = 0...num_matches
        } // if
    } // for

    if (pd->page == 2) {
        double x[4];
        pos_match_f = POOL_JUDOKAS_Y;
        i = num_matches(pd->systm.system, num_judokas) + 1;
        cairo_text_extents_t extents;
        cairo_text_extents(pd->c, "B2", &extents);

        static const gchar *a1[] = {"A1", "D2", "B1", "C2", "C1", "B2", "D1", "A2"};
        for (j = 0; j < 8; j++) {
            cairo_move_to(pd->c, OFFSET_X, pos_match_f + NAME_S*j + extents.height*1.2);
            cairo_show_text(pd->c, a1[j]);
        }

        for (j = 0; j < 4; j++) {
            x[j] = paint_comp(pd, &pm, 0,
                              pos_match_f + NAME_S*j*2, pos_match_f + NAME_S*(j*2+1), 
                              pm.m[i].blue, pm.m[i].white,
                              pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);
            i++;
        }

        /* first semifinal */
        x[0] = paint_comp(pd, &pm, 1,
                          x[0], x[1],
                          pm.m[i].blue, pm.m[i].white,
                          pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);

        if (pm.m[i].blue_points)
            bronze1 = pm.m[i].white;
        else
            bronze1 = pm.m[i].blue;

        i++;

        /* second semifinal */
        x[1] = paint_comp(pd, &pm, 1,
                          x[2], x[3],
                          pm.m[i].blue, pm.m[i].white,
                          pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);

        if (pm.m[i].blue_points)
            bronze2 = pm.m[i].white;
        else
            bronze2 = pm.m[i].blue;

        i++;

        /* final */
        x[2] = paint_comp(pd, &pm, 2, 
                          x[0], x[1],
                          pm.m[i].blue, pm.m[i].white,
                          pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);

        if (pm.m[i].blue_points || pm.m[i].white == GHOST) {
            gold = pm.m[i].blue;
            silver = pm.m[i].white;
        } else if (pm.m[i].white_points || pm.m[i].blue == GHOST) {
            gold = pm.m[i].white;
            silver = pm.m[i].blue;
        }

        x[2] = paint_comp(pd, &pm, 3, 
                          x[2], 0,
                          pm.m[i].blue, pm.m[i].white,
                          pm.m[i].blue_points, pm.m[i].white_points, SYSTEM_DPOOL, 0, 0, i);

        /* results */
        result_table.num_rows = 4;
        result_table.position_y = pos_match_f + NAME_S*8;
        create_table(pd, &result_table);
        write_table_title(pd, &result_table, _T(results));

        WRITE_TABLE(result_table, 0, 0, "%s", _T(position));
        WRITE_TABLE(result_table, 0, 1, "%s", _T(name));

        WRITE_TABLE(result_table, 1, 0, "1");
        WRITE_TABLE(result_table, 2, 0, "2");
        WRITE_TABLE(result_table, 3, 0, "3");
        WRITE_TABLE(result_table, 4, 0, "3");
                
        if (gold && (j1 = get_data(gold))) {
            WRITE_TABLE_H(result_table, 1, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
            free_judoka(j1);
        }
        if (gold && (j1 = get_data(silver))) {
            WRITE_TABLE_H(result_table, 2, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
            free_judoka(j1);
        }
        if (gold && (j1 = get_data(bronze1))) {
            WRITE_TABLE_H(result_table, 3, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
            free_judoka(j1);
        }
        if (gold && (j1 = get_data(bronze2))) {
            WRITE_TABLE_H(result_table, 4, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
            free_judoka(j1);
        }
    } // page 2
    /* clean up */
    empty_pool_struct(&pm);
}

static gint first_matches[NUM_FRENCH] = {4, 8, 16, 32, 64};

#define PAINT_WINNER(_w, _f)						\
    paint_comp(pd, NULL, level[_w]+1,                                   \
               positions[_w], 0.0,                                      \
               m[_w].blue, m[_w].white,                                 \
               m[_w].blue_points, m[_w].white_points, _f, 0, 0, _w)

#define PAINT_GOLD(_w)							\
    gold = (m[_w].blue_points || m[_w].white==GHOST) ? m[_w].blue :     \
        ((m[_w].white_points || m[_w].blue==GHOST) ? m[_w].white : 0);  \
    silver = (m[_w].blue_points || m[_w].white==GHOST) ? m[_w].white :  \
        ((m[_w].white_points || m[_w].blue==GHOST) ? m[_w].blue : 0);   \
    PAINT_WINNER(_w, special_flags)

#define PAINT_BRONZE1(_w)                                               \
    bronze1 = (m[_w].blue_points || m[_w].white==GHOST) ? m[_w].blue :  \
        ((m[_w].white_points || m[_w].blue==GHOST) ? m[_w].white : 0);  \
    PAINT_WINNER(_w, table!=TABLE_NO_REPECHAGE ? F_REPECHAGE : 0)

#define PAINT_BRONZE2(_w)                                               \
    bronze2 = (m[_w].blue_points || m[_w].white==GHOST) ? m[_w].blue :  \
        ((m[_w].white_points || m[_w].blue==GHOST) ? m[_w].white : 0);  \
    PAINT_WINNER(_w, table!=TABLE_NO_REPECHAGE ? F_REPECHAGE : 0)

#define GET_FOURTH(_w)                                                  \
    fourth = (m[_w].blue_points && m[_w].white > GHOST) ? m[_w].white : \
        ((m[_w].white_points && m[_w].blue > GHOST) ? m[_w].blue : 0);

#define GET_FIFTH1(_w)                                                  \
    fifth1 = (m[_w].blue_points && m[_w].white > GHOST) ? m[_w].white : \
        ((m[_w].white_points && m[_w].blue > GHOST) ? m[_w].blue : 0);

#define GET_FIFTH2(_w)                                                  \
    fifth2 = (m[_w].blue_points && m[_w].white > GHOST) ? m[_w].white : \
        ((m[_w].white_points && m[_w].blue > GHOST) ? m[_w].blue : 0);

#define GET_WINNER_AND_LOSER(_w)							\
    winner = (m[_w].blue_points || m[_w].white==GHOST) ? m[_w].blue :     \
        ((m[_w].white_points || m[_w].blue==GHOST) ? m[_w].white : 0);  \
    loser = (m[_w].blue_points || m[_w].white==GHOST) ? m[_w].white :  \
        ((m[_w].white_points || m[_w].blue==GHOST) ? m[_w].blue : 0);   \


static void paint_french(struct paint_data *pd, gint category, struct judoka *ctg, 
                         gint num_judokas, struct compsys systm, gint pagenum)
{
    struct match m[NUM_MATCHES];
    gint i, level[NUM_MATCHES], max_level = 0;
    double pos_y, positions[NUM_MATCHES], last_pos;
    double text_h = TEXT_HEIGHT;
    double space = NAME_S;
    struct judoka *j1;
    gint gold = 0, silver = 0, bronze1 = 0, bronze2 = 0, fourth = 0, fifth1 = 0, fifth2 = 0;
    gint winner, loser;
    gint sysflag = 0;
    gint sys = systm.system - SYSTEM_FRENCH_8;
    gint table = systm.table;
    gint first_match = 1, last_match = first_matches[sys];

    pd->row_height = 1;

    /* read mathecs */
    memset(level, 0, sizeof(level));
    memset(m, 0, sizeof(m));
    db_read_category_matches(category, m);

    switch (sys) {
    case FRENCH_8:
        break;
    case FRENCH_16:
	if (table == TABLE_DOUBLE_REPECHAGE || 
            table == TABLE_SWE_DUBBELT_AATERKVAL ||
            table == TABLE_DOUBLE_REPECHAGE_ONE_BRONZE) {
	    space = NAME_S*0.59;
	} else {
	    space = NAME_S*0.55;
	}
        break;
    case FRENCH_32:
	if (table == TABLE_DOUBLE_REPECHAGE || 
            table == TABLE_SWE_DUBBELT_AATERKVAL ||
            table == TABLE_MODIFIED_DOUBLE_ELIMINATION ||
            table == TABLE_DOUBLE_REPECHAGE_ONE_BRONZE) {
	    space = NAME_S*0.35;
	    text_h = 0.9*TEXT_HEIGHT;
	} else {
	    space = NAME_S*0.28;
	    text_h = 0.8*TEXT_HEIGHT;
	}
        break;
    case FRENCH_64:
	if ((table == TABLE_EST_D_KLASS || table == TABLE_SWE_DIREKT_AATERKVAL) && 
	    pagenum == 2)
	    space = NAME_S*0.25;
	else
	    space = NAME_S*0.35;

        sysflag = SYSTEM_FRENCH_64;
        if (pagenum == 0)
            last_match = last_match/2;
        else if (pagenum == 1)
            first_match = last_match/2 + 1;
        else
            last_match = 0;
        break;
    case FRENCH_128:
        space = NAME_S*0.35;

        sysflag = SYSTEM_FRENCH_128;

        if (pagenum < 4) {
            first_match = 1 + pagenum*last_match/4;
            last_match = first_match + last_match/4 - 1;
        } else
            last_match = 0;
        break;
    }

    cairo_set_font_size(pd->c, text_h);

    pos_y = OFFSET_Y;

    for (i = first_match; i <= last_match; i++) {
        positions[i] = 
            paint_comp(pd, NULL, 0,
                       pos_y, pos_y + space,
                       m[i].blue, m[i].white,
                       m[i].blue_points, m[i].white_points, sysflag,
                       get_drawed_number(2*(i-1)+1, sys),
                       get_drawed_number(2*i, sys), i);
        level[i] = 0;

        pos_y += 2*space;
    }

    last_pos = pos_y - H(0.01);

    gint special_flags = 0;

    for (i = first_matches[sys] + 1; i <= french_num_matches[table][sys]; i++) {
        gint blue_match = french_matches[table][sys][i][0];
        gint white_match = french_matches[table][sys][i][1];
        gdouble blue_pos, white_pos;
        gint intval;
        gdouble doubleval, doubleval2;
        gint special_match;

        if (sys == FRENCH_64 && pagenum != french_64_matches_to_page[table][i])
            continue;
        if (sys == FRENCH_128 && pagenum != french_128_matches_to_page[table][i])
            continue;

        //if (blue_match < 0) blue_match = -blue_match;
        //if (white_match < 0) white_match = -white_match;

        special_match = is_special_match(systm, i, &intval, &doubleval, &doubleval2);
        special_flags = 0;

        if (special_match == SPECIAL_MATCH_X_Y) {
            blue_pos = H(doubleval);
            white_pos = H(doubleval2); 
            pos_y = white_pos + space;
            level[i] = intval;
            special_flags = F_REPECHAGE;
        } else if (special_match == SPECIAL_MATCH_START) {
            blue_pos = pos_y;
            white_pos = blue_pos + space; 
            pos_y += 2.0*space;
            level[i] = 0;
            special_flags = F_REPECHAGE;
        } else {
            if (repechage_start[table][sys][0] == i ||
                repechage_start[table][sys][1] == i) {
                pos_y += (sys == FRENCH_64 || sys == FRENCH_128) ? 3*space : 1.5*space;
            }

            if (blue_match <= 0/* || is_repecharge(sys, i, FALSE)*/) {
                blue_pos = pos_y;
                pos_y += 3.0*space;
                level[i] = 0;
            } else {
                blue_pos = positions[blue_match];
                level[i] = level[blue_match] + 1;
                if (level[i] > max_level)
                    max_level = level[i];
            }

            if (white_match <= 0 /*|| is_repecharge(sys, i, TRUE)*/) {
                if (table == TABLE_ESP_DOBLE_PERDIDA)
                    white_pos = blue_pos + space;
                else
                    white_pos = blue_pos + 1.5*space + 
                        ((level[i] >= 3 && sys == FRENCH_64) || (level[i] >= 4 && sys == FRENCH_128) ? 2*space : 0.0);
            } else
                white_pos = positions[white_match];
        }

        gboolean lastpage = 
            (sys == FRENCH_64 && pagenum == 2 && table != TABLE_MODIFIED_DOUBLE_ELIMINATION) ||
            (sys == FRENCH_64 && pagenum == 3 && table == TABLE_MODIFIED_DOUBLE_ELIMINATION) ||
            (sys == FRENCH_128 && pagenum == 4);

        positions[i] = 
            paint_comp(pd, NULL, level[i],
                       blue_pos, white_pos, 
                       m[i].blue, m[i].white,
                       m[i].blue_points, m[i].white_points,
                       ((last_pos < blue_pos && sys != 3) || lastpage ? F_REPECHAGE : 0) | 
                       sysflag | special_flags, 0, 0, i);

        if (special_match == SPECIAL_MATCH_STOP) {
            PAINT_WINNER(i, intval);
        } else if (special_match == SPECIAL_MATCH_FLAG) {
            special_flags = intval;
        }
    }

    gint final_blue = french_matches[table][sys][medal_matches[table][sys][2]][0];
    gint final_white = french_matches[table][sys][medal_matches[table][sys][2]][1];
    gint gold_match = get_abs_matchnum_by_pos(table, sys, 1, 1);

    if (sys == FRENCH_64 && pagenum == 0)
        PAINT_WINNER(final_blue, 0);
    else if (sys == FRENCH_64 && pagenum == 1)
        PAINT_WINNER(final_white, 0);
    else if (sys == FRENCH_128 && pagenum < 4)
        PAINT_WINNER(121+pagenum, 0);

    switch (sys) {
    case FRENCH_8:
    case FRENCH_16:
    case FRENCH_32:
        if (table == TABLE_MODIFIED_DOUBLE_ELIMINATION) {
            GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(table, sys, 1, 1));
            gold = winner;
            GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(table, sys, 2, 1));
            silver = winner;
            bronze1 = loser;
            PAINT_WINNER(get_abs_matchnum_by_pos(table, sys, 1, 1), 0);
            PAINT_WINNER(get_abs_matchnum_by_pos(table, sys, 2, 1), F_REPECHAGE);
        } else {
            PAINT_BRONZE1(get_abs_matchnum_by_pos(table, sys, 3, 1));

            if (one_bronze(table, sys)) {
                GET_FOURTH(get_abs_matchnum_by_pos(table, sys, 4, 1));
            } else {
                PAINT_BRONZE2(get_abs_matchnum_by_pos(table, sys, 3, 2));
            }

            PAINT_GOLD(gold_match);
            GET_FIFTH1(get_abs_matchnum_by_pos(table, sys, 5, 1));
            GET_FIFTH2(get_abs_matchnum_by_pos(table, sys, 5, 2));
        }
        break;
    case FRENCH_64:
        if ((table != TABLE_MODIFIED_DOUBLE_ELIMINATION && pagenum != 2) ||
            (table == TABLE_MODIFIED_DOUBLE_ELIMINATION && pagenum != 3))
            return;

        if (table == TABLE_MODIFIED_DOUBLE_ELIMINATION) {
            GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(table, sys, 1, 1));
            gold = winner;
            GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(table, sys, 2, 1));
            silver = winner;
            bronze1 = loser;
            special_flags = F_REPECHAGE;
            PAINT_WINNER(get_abs_matchnum_by_pos(table, sys, 1, 1), F_REPECHAGE);
            PAINT_WINNER(get_abs_matchnum_by_pos(table, sys, 2, 1), F_REPECHAGE);
        } else {
            PAINT_BRONZE1(get_abs_matchnum_by_pos(table, sys, 3, 1));

            if (one_bronze(table, sys)) {
                GET_FOURTH(get_abs_matchnum_by_pos(table, sys, 4, 1));
            } else {
                PAINT_BRONZE2(get_abs_matchnum_by_pos(table, sys, 3, 2));
            }

            pos_y += 6.0*space;
            gdouble pos_y1 = pos_y;
            pos_y += 3.0*space;
            gdouble pos_y2 = pos_y;

            positions[gold_match] = 
                paint_comp(pd, NULL, 2,
                           pos_y1, pos_y2, 
                           m[gold_match].blue, 
                           m[gold_match].white,
                           m[gold_match].blue_points, 
                           m[gold_match].white_points, 0, 0, 0, 
                           gold_match);
            level[gold_match] = 2;

            PAINT_GOLD(gold_match);
            GET_FIFTH1(get_abs_matchnum_by_pos(table, sys, 5, 1));
            GET_FIFTH2(get_abs_matchnum_by_pos(table, sys, 5, 2));
        }
        break;
    case FRENCH_128:
        if (pagenum != 4)
            return;

        if (table == TABLE_MODIFIED_DOUBLE_ELIMINATION) {
            /*
            GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(table, sys, 1, 1));
            gold = winner;
            GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(table, sys, 2, 1));
            silver = winner;
            bronze1 = loser;
            PAINT_WINNER(get_abs_matchnum_by_pos(table, sys, 1, 1), F_REPECHAGE);
            PAINT_WINNER(get_abs_matchnum_by_pos(table, sys, 2, 1), F_REPECHAGE);
            */
        } else {
            PAINT_BRONZE1(get_abs_matchnum_by_pos(table, sys, 3, 1));

            if (one_bronze(table, sys)) {
                GET_FOURTH(get_abs_matchnum_by_pos(table, sys, 4, 1));
            } else {
                PAINT_BRONZE2(get_abs_matchnum_by_pos(table, sys, 3, 2));
            }
#if 0
            gdouble pos_y1 = positions[final_blue];
            gdouble pos_y2 = positions[final_white];

            level[gold_match] = level[final_blue];
            positions[gold_match] = 
                paint_comp(pd, NULL, level[gold_match],
                           pos_y1, pos_y2, 
                           m[gold_match].blue, 
                           m[gold_match].white,
                           m[gold_match].blue_points, 
                           m[gold_match].white_points, 0, 0, 0, 
                           gold_match);

#endif
            special_flags = F_REPECHAGE;
            PAINT_GOLD(gold_match);
            GET_FIFTH1(get_abs_matchnum_by_pos(table, sys, 5, 1));
            GET_FIFTH2(get_abs_matchnum_by_pos(table, sys, 5, 2));
        }
        break;
    }

    if (table == TABLE_NO_REPECHAGE ||
        table == TABLE_ESP_DOBLE_PERDIDA) {
	bronze1 = fifth1;
	bronze2 = fifth2;
	fifth1 = fifth2 = 0;
    }

    /* results */
    result_table_2.num_rows = 6;
    result_table_2.position_x = W(0.96 - result_table_2.columns[0] - result_table_2.columns[1]);
    result_table_2.position_y = H(0.9) - 5*ROW_HEIGHT;
    if (result_y_position[table][sys]) {
	result_table_2.position_y = positions[result_y_position[table][sys]] + H(0.03);
    }
    create_table(pd, &result_table_2);
    write_table_title(pd, &result_table_2, _T(results));

    WRITE_TABLE(result_table_2, 0, 0, "%s", _T(position));
    WRITE_TABLE(result_table_2, 0, 1, "%s", _T(name));

    WRITE_TABLE(result_table_2, 1, 0, "1");
    WRITE_TABLE(result_table_2, 2, 0, "2");
    WRITE_TABLE(result_table_2, 3, 0, "3");
    if (table != TABLE_MODIFIED_DOUBLE_ELIMINATION) {
        WRITE_TABLE(result_table_2, 4, 0, one_bronze(table, sys) ? "4" : "3");
        WRITE_TABLE(result_table_2, 5, 0, "5");
        WRITE_TABLE(result_table_2, 6, 0, "5");
    }

    if (gold && (j1 = get_data(gold))) {
        WRITE_TABLE_H(result_table_2, 1, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(silver))) {
        WRITE_TABLE_H(result_table_2, 2, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(bronze1))) {
        WRITE_TABLE_H(result_table_2, 3, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
        free_judoka(j1);
    }
    if (gold && one_bronze(table, sys) == FALSE && (j1 = get_data(bronze2))) {
        WRITE_TABLE_H(result_table_2, 4, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
        free_judoka(j1);
    }
    if (gold && one_bronze(table, sys) && (j1 = get_data(fourth))) {
        WRITE_TABLE_H(result_table_2, 4, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(fifth1))) {
        WRITE_TABLE_H(result_table_2, 5, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(fifth2))) {
        WRITE_TABLE_H(result_table_2, 6, 1, j1->deleted, "%s %s, %s", j1->first, j1->last, get_club_text(j1, 0));
        free_judoka(j1);
    }
}

#define NUM_CAT_CLICK_AREAS 80
static struct {
    gint category;
    double x1, y1, x2, y2;
} category_click_areas[NUM_CAT_CLICK_AREAS];
static gint num_category_click_areas; 

static struct {
    double x1, y1, x2, y2;
} point_click_areas[NUM_TATAMIS][2][11];	

void paint_category(struct paint_data *pd)
{
    cairo_text_extents_t extents;
    struct compsys sys; 
    gint category = pd->category, num_judokas;
    gchar buf[200];
    struct judoka *ctg = NULL;
    gdouble  paper_width_saved;
    gdouble  paper_height_saved;
    gdouble  total_width_saved;
    gboolean landscape_saved;

    /* find system */
    sys = db_get_system(category);
    pd->systm = sys;
    num_judokas = sys.numcomp;

    ROW_HEIGHT = NAME_H;
	
    if (font_face[0] == 0)
        strcpy(font_face, MY_FONT);

    init_tables(pd);
    ctg = get_data(category);

    if (ctg == NULL)
        return;

    if (pd->rotate) {
        //c_saved = pd->c;
        paper_width_saved = pd->paper_width;
        paper_height_saved = pd->paper_height;
        total_width_saved = pd->total_width;
        pd->paper_width = paper_height_saved;
        pd->paper_height = paper_width_saved;
        pd->total_width = pd->paper_width;
        landscape_saved = pd->landscape;
        pd->landscape = TRUE;
        cairo_translate(pd->c, paper_width_saved*0.5, paper_height_saved*0.5);
        cairo_rotate(pd->c, -0.5*M_PI);
        cairo_translate(pd->c, -paper_height_saved*0.5, -paper_width_saved*0.5);
    }

    cairo_set_source_rgb(pd->c, 1.0, 1.0, 1.0);
    cairo_rectangle(pd->c, 0.0, 0.0, pd->paper_width, pd->paper_height);
    cairo_fill(pd->c);

    cairo_select_font_face(pd->c, font_face, font_slant, font_weight);
    cairo_set_font_size(pd->c, TEXT_HEIGHT);
    cairo_text_extents(pd->c, "AjKp", &extents);

    cairo_set_line_width(pd->c, THIN_LINE /*H(0.002f)*/);
    cairo_set_source_rgb(pd->c, 0.0, 0.0, 0.0);

    cairo_save(pd->c);
    if (use_logo != NULL) {
        cairo_surface_t *image;
        gint w, h;

        cairo_save(pd->c);
        image = cairo_image_surface_create_from_png(use_logo);
        w = cairo_image_surface_get_width(image);
        h = cairo_image_surface_get_height(image);
        cairo_scale(pd->c, pd->paper_width/w, pd->paper_width/w);
        cairo_set_source_surface(pd->c, image, 0, 0);
        cairo_paint(pd->c);
        cairo_surface_destroy(image);        
        cairo_restore(pd->c);
    }

    if (use_logo != NULL && print_headers == FALSE) { 
        snprintf(buf, sizeof(buf)-1, "%s", ctg->last);
        cairo_select_font_face(pd->c, font_face,
                               font_slant,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(pd->c, NAME_H*1.2);
        cairo_text_extents(pd->c, buf, &extents);
        cairo_move_to(pd->c, (W(0.95)-extents.width), H(0.05));
        cairo_show_text(pd->c, buf);
    } else {
        snprintf(buf, sizeof(buf)-1, "%s  %s  %s   %s", 
                 info_competition,
                 info_date,
                 info_place,
                 ctg->last);
        cairo_select_font_face(pd->c, font_face,
                               font_slant,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(pd->c, NAME_H*1.2);
        cairo_text_extents(pd->c, buf, &extents);
        cairo_move_to(pd->c, (W(1.0)-extents.width)/2.0, H(0.05));
        cairo_show_text(pd->c, buf);
    }
    cairo_restore(pd->c);

    switch (sys.system) {
    case SYSTEM_POOL:
        if (paint_pool_style_2(category))
            paint_pool_2(pd, category, ctg, num_judokas);
        else
            paint_pool(pd, category, ctg, num_judokas, FALSE);
        break;

    case SYSTEM_DPOOL:
        paint_dpool(pd, category, ctg, num_judokas, FALSE);
        break;

    case SYSTEM_DPOOL2:
        if (pd->page == 0)
            paint_dpool(pd, category, ctg, num_judokas, TRUE);
        else
            paint_pool(pd, category, ctg, num_judokas, TRUE);
        break;

    case SYSTEM_QPOOL:
        paint_qpool(pd, category, ctg, num_judokas);
        break;

    case SYSTEM_FRENCH_8:
    case SYSTEM_FRENCH_16:
    case SYSTEM_FRENCH_32:
        paint_french(pd, category, ctg, num_judokas, sys, 0);
        break;

    case SYSTEM_FRENCH_64:
    case SYSTEM_FRENCH_128:
        paint_french(pd, category, ctg, num_judokas, sys, pd->page);
        break;
    }

    /* clean up */
    free_judoka(ctg);

    if (pd->rotate) {
        pd->paper_width = paper_width_saved;
        pd->paper_height = paper_height_saved;
        pd->total_width = total_width_saved;
        pd->landscape = landscape_saved;
        cairo_translate(pd->c, paper_height_saved*0.5, paper_width_saved*0.5);
        cairo_rotate(pd->c, 0.5*M_PI);
        cairo_translate(pd->c, -paper_width_saved*0.5, -paper_height_saved*0.5);
    }
}


static void paint_point_box(struct paint_data *pd, gdouble horpos, gdouble verpos, 
			    gint points, gboolean blue, gint tatami)
{
    gchar txt[8];
    gdouble x = W(2.0 + horpos*0.06);
    gdouble y = H(verpos);
    gdouble x1 = x - 2;
    gdouble y1 = y - NAME_H;
    gdouble w = NAME_H*1.5;
    gdouble h = NAME_H*1.2;

    if (points < 0 || points > 10)
        return;

    if (points == 0)
        strcpy(txt, "X");
    else
        sprintf(txt, "%d", points);

    cairo_save(pd->c);
	

    if (points == 0)
        cairo_set_source_rgb(pd->c, 0.0, 0.0, 0.0);
    else if (blue)
        cairo_set_source_rgb(pd->c, 0.0, 0.0, 1.0);
    else
        cairo_set_source_rgb(pd->c, 1.0, 1.0, 1.0);

    cairo_rectangle(pd->c, x1, y1, w, h);
    cairo_fill(pd->c);

    if (points == 0)
        cairo_set_source_rgb(pd->c, 1.0, 1.0, 0.0);
    else if (blue)
        cairo_set_source_rgb(pd->c, 1.0, 1.0, 1.0);
    else
        cairo_set_source_rgb(pd->c, 0.0, 0.0, 0.0);

    cairo_move_to(pd->c, x, y);
    cairo_show_text(pd->c, txt);

    cairo_restore(pd->c);

    point_click_areas[tatami][blue ? 0 : 1][points].x1 = x1;
    point_click_areas[tatami][blue ? 0 : 1][points].y1 = y1;
    point_click_areas[tatami][blue ? 0 : 1][points].x2 = x1 + w;
    point_click_areas[tatami][blue ? 0 : 1][points].y2 = y1 + h;
}

void paint_next_matches(struct paint_data *pd)
{
    gchar buf[200];

    /* next matches */
    if (pd->total_width < pd->paper_width)
        return;

    int i;

    /* for next match area */
    cairo_set_source_rgb(pd->c, 0.8, 0.8, 0.8);
    cairo_rectangle(pd->c, pd->paper_width, 0.0, 
                    pd->total_width - pd->paper_width, pd->paper_height);
    cairo_fill(pd->c);

    /* for category click */
    cairo_set_source_rgb(pd->c, 0.9, 0.9, 0.9);
    cairo_rectangle(pd->c, pd->paper_width, 0.0, W(0.3), pd->paper_height);
    cairo_fill(pd->c);

    gdouble rowheight = 1.0/(gdouble)number_of_tatamis/*NUM_TATAMIS*//8.0;

    cairo_set_source_rgb(pd->c, 0.0, 0.0, 0.0);
    cairo_select_font_face(pd->c, MY_FONT,
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);

    for (i = 0; i < number_of_tatamis/*NUM_TATAMIS*/; i++) {
        gchar txt[200];
        gdouble verpos = 0.05 + (gdouble)i/(gdouble)number_of_tatamis/*NUM_TATAMIS*/;
        snprintf(buf, sizeof(buf)-1, "Tatami %d", i+1);
        cairo_set_font_size(pd->c, NAME_H*1.2);
        cairo_move_to(pd->c, W(1.4), H(verpos));
        cairo_show_text(pd->c, buf);
        cairo_set_font_size(pd->c, NAME_H*1.0);

        if (next_matches_info[i][0].won_catnum) {
            snprintf(buf, sizeof(buf)-1, "  (%s: %s: %s %s)",
                     _T(prevwinner),
                     next_matches_info[i][0].won_cat,
                     next_matches_info[i][0].won_first,
                     next_matches_info[i][0].won_last);
            cairo_show_text(pd->c, buf);
        }

        if (next_matches_info[i][0].category[0] == 0)
            continue;

        cairo_move_to(pd->c, W(1.4), H(verpos + 1.0*rowheight));
        snprintf(txt, sizeof(txt), "%s: %s", _T(match), next_matches_info[i][0].category);
        cairo_show_text(pd->c, txt);

        cairo_move_to(pd->c, W(1.4), H(verpos + 2.0*rowheight));
        snprintf(txt, sizeof(txt), "    %s %s - %s", 
                 next_matches_info[i][0].blue_first,
                 next_matches_info[i][0].blue_last,
                 next_matches_info[i][0].blue_club);
        cairo_show_text(pd->c, txt);

        cairo_move_to(pd->c, W(1.4), H(verpos + 3.0*rowheight));
        snprintf(txt, sizeof(txt), "    %s %s - %s", 
                 next_matches_info[i][0].white_first,
                 next_matches_info[i][0].white_last,
                 next_matches_info[i][0].white_club);
        cairo_show_text(pd->c, txt);

        cairo_move_to(pd->c, W(1.4), H(verpos + 4.0*rowheight));
        snprintf(txt, sizeof(txt), "%s: %s", _T(nextmatch), next_matches_info[i][1].category);
        cairo_show_text(pd->c, txt);

        cairo_move_to(pd->c, W(1.4), H(verpos + 5.0*rowheight));
        snprintf(txt, sizeof(txt), "    %s %s - %s", 
                 next_matches_info[i][1].blue_first,
                 next_matches_info[i][1].blue_last,
                 next_matches_info[i][1].blue_club);
        cairo_show_text(pd->c, txt);

        cairo_move_to(pd->c, W(1.4), H(verpos + 6.0*rowheight));
        snprintf(txt, sizeof(txt), "    %s %s - %s", 
                 next_matches_info[i][1].white_first,
                 next_matches_info[i][1].white_last,
                 next_matches_info[i][1].white_club);
        cairo_show_text(pd->c, txt);

        /* points */
        paint_point_box(pd, 1.0, verpos,                 0, TRUE, i);

        paint_point_box(pd, 1.0, verpos + 2.0*rowheight, 10, TRUE, i);
        paint_point_box(pd, 2.0, verpos + 2.0*rowheight, 7, TRUE, i);
        paint_point_box(pd, 3.0, verpos + 2.0*rowheight, 5, TRUE, i);
        paint_point_box(pd, 4.0, verpos + 2.0*rowheight, 3, TRUE, i);
        paint_point_box(pd, 5.0, verpos + 2.0*rowheight, 1, TRUE, i);

        paint_point_box(pd, 1.0, verpos + 3.0*rowheight, 10, FALSE, i);
        paint_point_box(pd, 2.0, verpos + 3.0*rowheight, 7, FALSE, i);
        paint_point_box(pd, 3.0, verpos + 3.0*rowheight, 5, FALSE, i);
        paint_point_box(pd, 4.0, verpos + 3.0*rowheight, 3, FALSE, i);
        paint_point_box(pd, 5.0, verpos + 3.0*rowheight, 1, FALSE, i);
    }

    /* category click */
    GtkTreeIter iter;
    gboolean ok;
    num_category_click_areas = 0;
    ok = gtk_tree_model_get_iter_first(current_model, &iter);
    while (ok) {
        gint index;
        gchar *cat = NULL;
        gtk_tree_model_get(current_model, &iter,
                           COL_INDEX, &index,
                           COL_LAST_NAME, &cat,
                           -1);

        double x = W((gint)((gint)num_category_click_areas/25)*0.1 + 1.0);
        double y = H((gint)((gint)num_category_click_areas % 25 + 1)*1.0/26.0);
        category_click_areas[num_category_click_areas].category = index;
        category_click_areas[num_category_click_areas].x1 = x;
        category_click_areas[num_category_click_areas].y2 = y + H(0.01);
        category_click_areas[num_category_click_areas].x2 = x + W(0.1);
        category_click_areas[num_category_click_areas].y1 = y - H(rowheight);
        cairo_move_to(pd->c, x, y);
        cairo_show_text(pd->c, cat);
        g_free(cat);
		
        num_category_click_areas++;
        if (num_category_click_areas >= NUM_CAT_CLICK_AREAS)
            break;

        ok = gtk_tree_model_iter_next(current_model, &iter);
    }
}

static GtkWidget *darea = NULL;
gdouble zoom = 1.0, orig_zoom;
static gdouble zoom_x_offset = 0.0, zoom_y_offset = 0.0, orig_x_offset, orig_y_offset;
static gdouble button_start_x, button_start_y;
static gboolean button_drag = FALSE;


static gboolean expose_cat(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
    struct paint_data *pd = userdata;
    static cairo_surface_t *print_icon = NULL;

    if (print_icon == NULL) {
        gchar *file = g_build_filename(installation_dir, "etc", "print.png", NULL);
        print_icon = cairo_image_surface_create_from_png(file);
        g_free(file);
    }

    if (pd->landscape)
        pd->paper_height = SIZEX*widget->allocation.width/SIZEY;
    else
        pd->paper_height = SIZEY*widget->allocation.width/SIZEX;
    pd->paper_width = widget->allocation.width;
    pd->total_width = widget->allocation.width;

    if (pd->scroll) {
        GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pd->scroll));
        //gdouble lower = gtk_adjustment_get_lower(GTK_ADJUSTMENT(adj));
        //gdouble upper = gtk_adjustment_get_upper(GTK_ADJUSTMENT(adj));
        //gdouble value = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
        gtk_adjustment_set_upper(GTK_ADJUSTMENT(adj), pd->paper_width*SIZEY/SIZEX);
        gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(pd->scroll), adj);
    }

    pd->c = gdk_cairo_create(widget->window);
    paint_category(pd);

    cairo_set_source_surface(pd->c, print_icon, 0, 0);
    cairo_paint(pd->c);

    cairo_show_page(pd->c);
    cairo_destroy(pd->c);

    return FALSE;
}

static gboolean delete_event_cat( GtkWidget *widget,
                                  GdkEvent  *event,
                                  gpointer   data )
{
    return FALSE;
}

static void destroy_cat( GtkWidget *widget,
                         gpointer   data )
{
    g_free(data);
}

static gboolean print_cat(GtkWidget *window, 
                          GdkEventButton *event, 
                          gpointer userdata)
{
    struct paint_data *pd = userdata;

    if (event->type == GDK_BUTTON_PRESS  &&  
        (event->button == 1 || event->button == 3)) {
        gint x = event->x, y = event->y;

        if (x < 32 && y < 32) {
            print_doc(NULL, (gpointer)(pd->category | PRINT_SHEET | PRINT_TO_PRINTER));
            return TRUE;
        } else  {
            pd->page = next_page(pd->category, pd->page);

            GtkWidget *widget;
            widget = GTK_WIDGET(window);
            if (widget->window) {
                GdkRegion *region = gdk_drawable_get_clip_region(GDK_DRAWABLE(widget->window));
                gdk_window_invalidate_region(GDK_WINDOW(widget->window), region, TRUE);
                gdk_window_process_updates(GDK_WINDOW(widget->window), TRUE);
            }
            return TRUE;
        }
    }

    return FALSE;
}

void category_window(gint cat)
{
    GdkScreen *scr = gdk_screen_get_default();
    gint scr_height = gdk_screen_get_height(scr);
    gint height_req = 800;

    if (height_req > scr_height && scr_height > 100)
        height_req = scr_height - 20;

    GtkWindow *window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(GTK_WINDOW(window), _("Category"));
    gtk_widget_set_size_request(GTK_WIDGET(window), SIZEX, height_req);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    GtkWidget *darea = gtk_drawing_area_new();
    gtk_widget_set_size_request(GTK_WIDGET(darea), SIZEX, 4*SIZEY);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 10);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), darea);

    gtk_container_add(GTK_CONTAINER(window), scrolled_window);
    gtk_widget_show_all(GTK_WIDGET(window));

    struct paint_data *pd = g_malloc(sizeof(*pd));
    memset(pd, 0, sizeof(*pd));
    pd->scroll = scrolled_window;
    pd->category = cat;

    if (print_landscape(cat)) {
        pd->paper_height = SIZEX*darea->allocation.width/SIZEY;
        pd->landscape = TRUE;
    } else {    
        pd->paper_height = SIZEY*darea->allocation.width/SIZEX;
    }
    pd->paper_width = darea->allocation.width;
    pd->total_width = darea->allocation.width;
    //XXXpd->row_height = 1;

    gtk_widget_add_events(darea, 
                          GDK_BUTTON_PRESS_MASK);

    g_signal_connect(G_OBJECT(darea), 
                     "button-press-event", G_CALLBACK(print_cat), pd);
    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event_cat), pd);
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (destroy_cat), pd);
 
    g_signal_connect(G_OBJECT(darea), 
                     "expose-event", G_CALLBACK(expose_cat), pd);
}

/* This is called when we need to draw the windows contents */
static gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
    struct paint_data pd;
    memset(&pd, 0, sizeof(pd));
    pd.category = current_category;
    pd.page = current_page;
    pd.c = gdk_cairo_create(widget->window);
    //XXX pd.row_height = 1;

    gdouble orig_paper_height = pd.paper_height = widget->allocation.height;
    gdouble orig_paper_width = pd.paper_width = SIZEX*pd.paper_height/SIZEY;
    pd.total_width = widget->allocation.width;

    if (userdata == NULL) {
        if (zoom < 1.1) {
            paint_category(&pd);
        } else {
            pd.paper_width = zoom*orig_paper_width;
            pd.paper_height = zoom*orig_paper_height;

            if (zoom_x_offset > 0.0)
                zoom_x_offset = 0.0;
            if (zoom_y_offset > 0.0)
                zoom_y_offset = 0.0;
            if (zoom_x_offset < orig_paper_width - pd.paper_width)
                zoom_x_offset = orig_paper_width - pd.paper_width;
            if (zoom_y_offset < orig_paper_height - pd.paper_height)
                zoom_y_offset = orig_paper_height - pd.paper_height;

            cairo_surface_t *cs = 
                cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                           pd.paper_width, pd.paper_height);
            cairo_t *cr = pd.c;
            pd.c = cairo_create(cs);
            paint_category(&pd);
            cairo_show_page(pd.c);
            cairo_destroy(pd.c);
            pd.c = cr;
            cairo_save(pd.c);
            cairo_rectangle(pd.c, 0.0, 0.0, orig_paper_width, orig_paper_height);
            cairo_clip(pd.c);
            cairo_set_source_surface(pd.c, cs, zoom_x_offset, 
                                     zoom_y_offset);
            paint_category(&pd);
            cairo_restore(pd.c);
            cairo_surface_destroy(cs);
        }
    }

    pd.paper_width = orig_paper_width;
    pd.paper_height = orig_paper_height;

    if (button_drag == FALSE)
        paint_next_matches(&pd);

    cairo_show_page(pd.c);
    cairo_destroy(pd.c);

    return FALSE;
}

void write_sheet_to_stream(gint cat, cairo_write_func_t write_func, void *closure)
{
    struct paint_data pd;
    cairo_surface_t *cs;

    memset(&pd, 0, sizeof(pd));
    //XXXpd.row_height = 1;
    if (print_landscape(cat)) {
        pd.paper_height = SIZEX;
        pd.paper_width = SIZEY;
        //XXXpd.row_height = 2;
        pd.landscape = TRUE;
    } else {
        pd.paper_height = SIZEY;
        pd.paper_width = SIZEX;
    }
    pd.total_width = 0;
    pd.category = cat;

    cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pd.paper_width, pd.paper_height);
    pd.c = cairo_create(cs);

    paint_category(&pd);

    cairo_show_page(pd.c);
    cairo_destroy(pd.c);

    cairo_surface_write_to_png_stream(cs, write_func, closure);
    cairo_surface_destroy(cs);
}

void refresh_sheet_display(gboolean forced)
{
    if (darea == NULL || current_category == 0)
        return;

    current_page = 0;

    expose(darea, 0, (gpointer)(forced == 0 && automatic_sheet_update == 0));

    if (automatic_web_page_update) {
        write_png(NULL, (gpointer)current_category);
        make_next_matches_html();
    }
}

static void screen_changed(GtkWidget *widget, GdkScreen *old_screen, gpointer userdata)
{
#if 0
    /* To check if the display supports alpha channels, get the colormap */
    GdkScreen *screen = gtk_widget_get_screen(widget);
    GdkColormap *colormap = gdk_screen_get_rgba_colormap(screen);
#endif
}

static gboolean motion_notify(GtkWidget *sheet_page, 
                              GdkEventMotion *event, 
                              gpointer userdata)
{
    if (button_drag == FALSE)
        return FALSE;

    if (event->state & GDK_BUTTON3_MASK) {
        zoom = orig_zoom + (event->y - button_start_y)*0.005;
        if (zoom < 1.0)
            zoom = 1.0;
        else if (zoom > 3.0)
            zoom = 3.0;
    } else {
        if (zoom < 1.1)
            return FALSE;

        zoom_x_offset = orig_x_offset + event->x - button_start_x;
        zoom_y_offset = orig_y_offset + event->y - button_start_y;
    }
    expose(darea, 0, 0);
	
    return TRUE;
}

static gboolean release_notify(GtkWidget *sheet_page, 
			       GdkEventButton *event, 
			       gpointer userdata)
{
    button_drag = FALSE;
    return FALSE;
}

gboolean change_current_page(GtkWidget *sheet_page, 
			     GdkEventButton *event, 
			     gpointer userdata)
{
    /* single click with the right mouse button? */
    if (event->type == GDK_BUTTON_PRESS  &&  
        (event->button == 1 || event->button == 3)) {
        gint x = event->x, y = event->y, i, j, t;

        /* points clicked */
        for (t = 0; t < number_of_tatamis/*NUM_TATAMIS*/; t++) {
            if (next_matches_info[t][0].catnum == 0)
                continue;

            for (j = 0; j < 2; j++) {
                for (i = 0; i < 11; i++) {
                    if (x >= point_click_areas[t][j][i].x1 &&
                        x <= point_click_areas[t][j][i].x2 &&
                        y >= point_click_areas[t][j][i].y1 &&
                        y <= point_click_areas[t][j][i].y2) {
                        cairo_t *c = gdk_cairo_create(sheet_page->window);
                        cairo_set_source_rgb(c, 0.0, 1.0, 0.0);
                        cairo_rectangle(c, point_click_areas[t][j][i].x1,
                                        point_click_areas[t][j][i].y1,
                                        point_click_areas[t][j][i].x2
                                        - point_click_areas[t][j][i].x1,
                                        point_click_areas[t][j][i].y2
                                        - point_click_areas[t][j][i].y1);
                        cairo_fill(c);
                        cairo_show_page(c);
                        cairo_destroy(c);

                        guint data;
                        if (i > 0) {
                            data = (next_matches_info[t][0].catnum << 18) | 
                                (next_matches_info[t][0].matchnum << 5) | 
                                (j == 0 ? 16 : 0) |
                                i;
                            set_points(NULL, (gpointer)data);
                        } else if (next_matches_info[t][0].won_catnum >= 10000) {
                            gint wcat = next_matches_info[t][0].won_catnum;
                            gint wnum = next_matches_info[t][0].won_matchnum;
                            data = (wcat << 18) | (wnum << 5);
                            set_points(NULL, (gpointer)data);
                            db_set_comment(next_matches_info[t][1].catnum,
                                           next_matches_info[t][1].matchnum,
                                           COMMENT_EMPTY);
                            db_set_comment(next_matches_info[t][0].catnum,
                                           next_matches_info[t][0].matchnum,
                                           COMMENT_MATCH_2);
                            db_set_comment(wcat, wnum, COMMENT_MATCH_1);
                            struct compsys sys = db_get_system(wcat);
                            update_matches(wcat, sys, db_find_match_tatami(wcat, wnum));
                            next_matches_info[t][0].won_catnum = 0;
                        }
                        return TRUE;
                    }
                }
            }
        }

        /* category clicked */
        for (i = 0; i < num_category_click_areas; i++) {
            if (x >= category_click_areas[i].x1 &&
                x <= category_click_areas[i].x2 &&
                y >= category_click_areas[i].y1 &&
                y <= category_click_areas[i].y2) {
                current_page = 0;
                current_category = category_click_areas[i].category;
                expose(darea, 0, 0);
                return TRUE;
            }
        }

        button_start_x = event->x;
        button_start_y = event->y;
        orig_x_offset = zoom_x_offset;
        orig_y_offset = zoom_y_offset;
        orig_zoom = zoom;
        button_drag = TRUE;

        if (event->button == 1)
            current_page = next_page(current_category, current_page);

        expose(darea, 0, 0);

        return TRUE;
    }

    return FALSE;
}

void set_sheet_page(GtkWidget *notebook)
{
    sheet_label = gtk_label_new(_("Sheets"));
    //GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);
    darea = gtk_drawing_area_new();
    GTK_WIDGET_SET_FLAGS(darea, GTK_CAN_FOCUS);
    gtk_widget_add_events(darea, 
                          GDK_BUTTON_PRESS_MASK | 
                          GDK_BUTTON_RELEASE_MASK |
                          /*GDK_POINTER_MOTION_MASK |*/
                          GDK_POINTER_MOTION_HINT_MASK |
                          GDK_BUTTON_MOTION_MASK);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), darea, sheet_label);
    //gtk_container_add (GTK_CONTAINER (main_window), darea);
    gtk_widget_show(darea);
#if 0
    g_signal_connect(G_OBJECT(main_window), 
                     "expose-event", G_CALLBACK(expose), NULL);
#endif
    g_signal_connect(G_OBJECT(darea), 
                     "expose-event", G_CALLBACK(expose), NULL);
    g_signal_connect(G_OBJECT(darea), 
                     "screen-changed", G_CALLBACK(screen_changed), NULL);
    g_signal_connect(G_OBJECT(darea), 
                     "button-press-event", G_CALLBACK(change_current_page), NULL);
    g_signal_connect(G_OBJECT(darea), 
                     "button-release-event", G_CALLBACK(release_notify), NULL);
    g_signal_connect(G_OBJECT(darea), 
                     "motion-notify-event", G_CALLBACK(motion_notify), NULL);

    gtk_widget_show_all(notebook);
}

void parse_font_text(gchar *font, gchar *face, gint *slant, gint *weight, gdouble *size)
{
    gchar *italic = strstr(font, "Italic");
    gchar *bold = strstr(font, "Bold");
    gchar *num = strrchr(font, ' ');

    if (!num) {
        g_print("ERROR: malformed font string '%s'\n", font);
        return;
    }

    num++;

    gchar *end = num;

    if (italic && italic < end)
        end = italic;

    if (bold && bold < end)
        end = bold;

    end--;

    gchar *p = font;
    gchar *d = face;
    while (p < end)
        *d++ = *p++;
    *d = 0;

    if (italic)
        *slant = CAIRO_FONT_SLANT_ITALIC;
    else
        *slant = CAIRO_FONT_SLANT_NORMAL;

    if (bold)
        *weight = CAIRO_FONT_WEIGHT_BOLD;
    else
        *weight = CAIRO_FONT_WEIGHT_NORMAL;

    if (num && size)
        *size = atof(num);
}

void set_font(gchar *font)
{
    parse_font_text(font, font_face, &font_slant, &font_weight, NULL);
    g_key_file_set_string(keyfile, "preferences", "sheetfont", font);
}

gchar *get_font_face()
{
    static gchar buf[64];
    sprintf(buf, "%s 12", font_face); 
    return buf;
}

void set_sheet_titles(void)
{
    if (sheet_label)
        gtk_label_set_text(GTK_LABEL(sheet_label), _("Sheets"));
}
