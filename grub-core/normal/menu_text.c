/* menu_text.c - Basic text menu implementation.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/normal.h>
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/loader.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/env.h>
#include <grub/menu_viewer.h>
#include <grub/i18n.h>
#include <grub/charset.h>
#include <grub/miray_bootcast.h>

static grub_uint8_t grub_color_menu_normal;
static grub_uint8_t grub_color_menu_highlight;
static grub_uint8_t grub_color_menu_highlight_warning;

struct menu_viewer_data
{
  int first, offset;
  struct grub_term_screen_geometry geo;
  enum { 
    TIMEOUT_UNKNOWN, 
    TIMEOUT_NORMAL,
    TIMEOUT_TERSE,
    TIMEOUT_TERSE_NO_MARGIN
  } timeout_msg;
  grub_menu_t menu;
  struct grub_term_output *term;
};

static void
miray_menu_init_page (grub_menu_t menu,
		     struct grub_term_screen_geometry *geo,
		     struct grub_term_output *term);

static grub_err_t
miray_menu_try_text (struct grub_term_output *term,
		    int entry, grub_menu_t menu);

static inline int
grub_term_cursor_x (const struct grub_term_screen_geometry *geo)
{
  return (geo->first_entry_x + geo->entry_width);
}

grub_size_t
grub_getstringwidth (grub_uint32_t * str, const grub_uint32_t * last_position,
		     struct grub_term_output *term)
{
  grub_ssize_t width = 0;

  while (str < last_position)
    {
      struct grub_unicode_glyph glyph;
      glyph.ncomb = 0;
      str += grub_unicode_aglomerate_comb (str, last_position - str, &glyph);
      width += grub_term_getcharwidth (term, &glyph);
      grub_unicode_destroy_glyph (&glyph);
    }
  return width;
}

static int
grub_print_message_indented_real_maxlines (const char *msg, int margin_left,
				  int margin_right,
				  struct grub_term_output *term, int dry_run, int max_lines)
{
  grub_uint32_t *unicode_msg;
  grub_uint32_t *last_position;
  grub_size_t msg_len = grub_strlen (msg) + 2;
  int ret = 0;

  unicode_msg = grub_calloc (msg_len, sizeof (grub_uint32_t));
 
  if (!unicode_msg)
    return 0;

  msg_len = grub_utf8_to_ucs4 (unicode_msg, msg_len,
			       (grub_uint8_t *) msg, -1, 0);
  
  last_position = unicode_msg + msg_len;
  *last_position = 0;

  if (dry_run)
    ret = grub_ucs4_count_lines (unicode_msg, last_position, margin_left,
				 margin_right, term);
  else
    grub_print_ucs4_menu (unicode_msg, last_position, margin_left,
			  margin_right, term, 0, max_lines, 0, 0);

  grub_free (unicode_msg);

  return ret;
}

static int
grub_print_message_indented_real (const char *msg, int margin_left,
				  int margin_right,
				  struct grub_term_output *term, int dry_run)
{
  return grub_print_message_indented_real_maxlines(msg, margin_left, margin_right, term, dry_run, -1);
}

void
grub_print_message_indented (const char *msg, int margin_left, int margin_right,
			     struct grub_term_output *term)
{
  grub_print_message_indented_real (msg, margin_left, margin_right, term, 0);
}

static void
draw_border (struct grub_term_output *term, const struct grub_term_screen_geometry *geo)
{
  int i;

  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);

  grub_term_gotoxy (term, (struct grub_term_coordinate) { 0, geo->first_entry_y - 1 });
  for (i = 0; i < geo->first_entry_x - 1; i++)
    grub_putcode(' ', term);
  grub_putcode (GRUB_UNICODE_CORNER_UL, term);
  for (i = 0; i < geo->entry_width + 1; i++)
    grub_putcode (GRUB_UNICODE_HLINE, term);
  grub_putcode (GRUB_UNICODE_CORNER_UR, term);
  for (i = geo->entry_width + 2; i < (int)grub_term_width(term); i++)
    grub_putcode (' ', term);

  for (i = 0; i < geo->num_entries; i++)
    {
      int k;
      grub_term_gotoxy (term, (struct grub_term_coordinate) { 0,
	    geo->first_entry_y + i });
      for (k = 0; k < geo->first_entry_x - 1; k++)
        grub_putcode(' ', term);
      grub_putcode (GRUB_UNICODE_VLINE, term);
      grub_term_gotoxy (term,
			(struct grub_term_coordinate) { geo->first_entry_x + geo->entry_width + 1,
			    geo->first_entry_y + i });
      grub_putcode (GRUB_UNICODE_VLINE, term);
      for (k = geo->first_entry_x + geo->entry_width + 1; k < (int)grub_term_width(term); k++)
        grub_putcode(' ', term);
    }

  grub_term_gotoxy (term,
		    (struct grub_term_coordinate) { 0,
			geo->first_entry_y - 1 + geo->num_entries + 1 });
  for (i = 0; i <  geo->first_entry_x - 1; i++)
    grub_putcode(' ', term);
  grub_putcode (GRUB_UNICODE_CORNER_LL, term);
  for (i = 0; i < geo->entry_width + 1; i++)
    grub_putcode (GRUB_UNICODE_HLINE, term);
  grub_putcode (GRUB_UNICODE_CORNER_LR, term);
  for (i = geo->entry_width + 2; i < (int)grub_term_width(term); i++)
    grub_putcode (' ', term);

  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);

  grub_term_gotoxy (term,
		    (struct grub_term_coordinate) { geo->first_entry_x - 1,
			(geo->first_entry_y - 1 + geo->num_entries
			 + GRUB_TERM_MARGIN + 1) });
}

static int
print_message (int nested, int edit, struct grub_term_output *term, int dry_run)
{
  int ret = 0;
  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);

  if (edit)
    {
      ret += grub_print_message_indented_real (_("Minimum Emacs-like screen editing is \
supported. TAB lists completions. Press Ctrl-x or F10 to boot, Ctrl-c or F2 for a \
command-line or ESC to discard edits and return to the GRUB menu."),
					       STANDARD_MARGIN, STANDARD_MARGIN,
					       term, dry_run);
    }
  else
    {
      char *msg_translated = 0;

      {
         const char * msg = grub_env_get("menu_footer1_gfx");
         if (msg != 0)
           msg_translated = grub_strdup(msg);
      }

      if (msg_translated == 0)
        msg_translated = grub_xasprintf (_("Use the %C and %C keys to select which "
					 "entry is highlighted."),
				       GRUB_UNICODE_UPARROW,
				       GRUB_UNICODE_DOWNARROW);
      if (!msg_translated)
	return 0;
      ret += grub_print_message_indented_real (msg_translated, STANDARD_MARGIN,
					       STANDARD_MARGIN, term, dry_run);

      grub_free (msg_translated);

      if (nested)
	{
      const char * footer_msg = grub_env_get ("menu_footer2");
      if (footer_msg == 0)
         footer_msg = _("Press enter to boot the selected OS, "
	       "`e' to edit the commands before booting "
	       "or `c' for a command-line. ESC to return previous menu.");
	  ret += grub_print_message_indented_real
	    (footer_msg,
	     STANDARD_MARGIN, STANDARD_MARGIN, term, dry_run);
	}
      else
	{
	  ret += grub_print_message_indented_real
	    (_("Press enter to boot the selected OS, "
	       "`e' to edit the commands before booting "
	       "or `c' for a command-line."),
	     STANDARD_MARGIN, STANDARD_MARGIN, term, dry_run);
	}	
    }
  return ret;
}

static void
print_entry (int y, int highlight, grub_menu_entry_t entry,
	     const struct menu_viewer_data *data)
{
  const char *title;
  grub_size_t title_len;
  grub_ssize_t len;
  grub_uint32_t *unicode_title;
  grub_ssize_t i;
  grub_uint8_t old_color_normal, old_color_highlight;

  title = entry ? entry->title : "";
  title_len = grub_strlen (title);
  unicode_title = grub_calloc (title_len, sizeof (*unicode_title));
  if (! unicode_title)
    /* XXX How to show this error?  */
    return;

  len = grub_utf8_to_ucs4 (unicode_title, title_len,
                           (grub_uint8_t *) title, -1, 0);
  if (len < 0)
    {
      /* It is an invalid sequence.  */
      grub_free (unicode_title);
      return;
    }

  old_color_normal = grub_term_normal_color;
  old_color_highlight = grub_term_highlight_color;
  grub_term_normal_color = grub_color_menu_normal;
  grub_term_highlight_color = grub_color_menu_highlight;
  grub_term_setcolorstate (data->term, highlight
			   ? GRUB_TERM_COLOR_HIGHLIGHT
			   : GRUB_TERM_COLOR_NORMAL);

  grub_term_gotoxy (data->term, (struct grub_term_coordinate) { 
      data->geo.first_entry_x, y });

  for (i = 0; i < len; i++)
    if (unicode_title[i] == '\n' || unicode_title[i] == '\b'
	|| unicode_title[i] == '\r' || unicode_title[i] == '\e')
      unicode_title[i] = ' ';

  if (data->geo.num_entries > 1)
    grub_putcode (' ', data->term);
    //grub_putcode (highlight ? '*' : ' ', data->term);

  grub_print_ucs4_menu (unicode_title,
			unicode_title + len,
			0,
			data->geo.right_margin,
			data->term, 0, 1,
			GRUB_UNICODE_RIGHTARROW, 0);

  grub_term_setcolorstate (data->term, GRUB_TERM_COLOR_NORMAL);
  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 
		      grub_term_cursor_x (&data->geo), y });

  grub_term_normal_color = old_color_normal;
  grub_term_highlight_color = old_color_highlight;

  grub_term_setcolorstate (data->term, GRUB_TERM_COLOR_NORMAL);
  grub_free (unicode_title);
}

static void
print_entries (grub_menu_t menu, const struct menu_viewer_data *data)
{
  grub_menu_entry_t e;
  int i;

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 
		      data->geo.first_entry_x + data->geo.entry_width
			+ data->geo.border + 1,
			data->geo.first_entry_y });

  if (data->geo.num_entries != 1)
    {
      if (data->first)
	grub_putcode (GRUB_UNICODE_UPARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }
  e = grub_menu_get_entry (menu, data->first);

  for (i = 0; i < data->geo.num_entries; i++)
    {
      print_entry (data->geo.first_entry_y + i, data->offset == i,
		   e, data);
      if (e)
	e = e->next;
    }

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { data->geo.first_entry_x + data->geo.entry_width
			+ data->geo.border + 1,
			data->geo.first_entry_y + data->geo.num_entries - 1 });
  if (data->geo.num_entries == 1)
    {
      if (data->first && e)
	grub_putcode (GRUB_UNICODE_UPDOWNARROW, data->term);
      else if (data->first)
	grub_putcode (GRUB_UNICODE_UPARROW, data->term);
      else if (e)
	grub_putcode (GRUB_UNICODE_DOWNARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }
  else
    {
      if (e)
	grub_putcode (GRUB_UNICODE_DOWNARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });
}

/* Initialize the screen.  If NESTED is non-zero, assume that this menu
   is run from another menu or a command-line. If EDIT is non-zero, show
   a message for the menu entry editor.  */
void
grub_menu_init_page (int nested, int edit,
		     struct grub_term_screen_geometry *geo,
		     struct grub_term_output *term)
{
  grub_uint8_t old_color_normal, old_color_highlight;
  int msg_num_lines;
  int bottom_message = 1;
  int empty_lines = 1;
  int version_msg = 1;

  if (miray_isbootcast())
  {
     miray_menu_init_page(0,  geo, term);
     return;
  }

  geo->border = 1;
  geo->first_entry_x = 1 /* margin */ + 1 /* border */;
  geo->entry_width = grub_term_width (term) - 5;

  geo->first_entry_y = 2 /* two empty lines*/
    + 1 /* GNU GRUB version text  */ + 1 /* top border */;

  geo->timeout_lines = 2;

  /* 3 lines for timeout message and bottom margin.  2 lines for the border.  */
  geo->num_entries = grub_term_height (term) - geo->first_entry_y
    - 1 /* bottom border */
    - 1 /* empty line before info message*/
    - geo->timeout_lines /* timeout */
    - 1 /* empty final line  */;
  msg_num_lines = print_message (nested, edit, term, 1);
  if (geo->num_entries - msg_num_lines < 3
      || geo->entry_width < 10)
    {
      geo->num_entries += 4;
      geo->first_entry_y -= 2;
      empty_lines = 0;
      geo->first_entry_x -= 1;
      geo->entry_width += 1;
    }
  if (geo->num_entries - msg_num_lines < 3
      || geo->entry_width < 10)
    {
      geo->num_entries += 2;
      geo->first_entry_y -= 1;
      geo->first_entry_x -= 1;
      geo->entry_width += 2;
      geo->border = 0;
    }

  if (geo->entry_width <= 0)
    geo->entry_width = 1;

  if (geo->num_entries - msg_num_lines < 3
      && geo->timeout_lines == 2)
    {
      geo->timeout_lines = 1;
      geo->num_entries++;
    }

  if (geo->num_entries - msg_num_lines < 3)
    {
      geo->num_entries += 1;
      geo->first_entry_y -= 1;
      version_msg = 0;
    }

  if (geo->num_entries - msg_num_lines >= 2)
    geo->num_entries -= msg_num_lines;
  else
    bottom_message = 0;

  /* By default, use the same colors for the menu.  */
  old_color_normal = grub_term_normal_color;
  old_color_highlight = grub_term_highlight_color;
  grub_color_menu_normal = grub_term_normal_color;
  grub_color_menu_highlight = grub_term_highlight_color;

  /* Then give user a chance to replace them.  */
  grub_parse_color_name_pair (&grub_color_menu_normal,
			      grub_env_get ("menu_color_normal"));
  grub_parse_color_name_pair (&grub_color_menu_highlight,
			      grub_env_get ("menu_color_highlight"));

  if (version_msg)
    grub_normal_init_page (term, empty_lines);
  else
    grub_term_cls (term);

  grub_term_normal_color = grub_color_menu_normal;
  grub_term_highlight_color = grub_color_menu_highlight;
  if (geo->border)
    draw_border (term, geo);
  grub_term_normal_color = old_color_normal;
  grub_term_highlight_color = old_color_highlight;
  geo->timeout_y = geo->first_entry_y + geo->num_entries
    + geo->border + empty_lines;
  if (bottom_message)
    {
      grub_term_gotoxy (term,
			(struct grub_term_coordinate) { GRUB_TERM_MARGIN,
			    geo->timeout_y });

      print_message (nested, edit, term, 0);
      geo->timeout_y += msg_num_lines;
    }
  geo->right_margin = grub_term_width (term)
    - geo->first_entry_x
    - geo->entry_width - 1;
}

static void
menu_text_print_timeout (int timeout, void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  char *msg_translated = 0;

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 0, data->geo.timeout_y });

  if (data->timeout_msg == TIMEOUT_TERSE
      || data->timeout_msg == TIMEOUT_TERSE_NO_MARGIN)
    msg_translated = grub_xasprintf (_("%ds"), timeout);
  else
    msg_translated = grub_xasprintf (_("The highlighted entry will be executed automatically in %ds."), timeout);
  if (!msg_translated)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  if (data->timeout_msg == TIMEOUT_UNKNOWN)
    {
      data->timeout_msg = grub_print_message_indented_real (msg_translated,
							    3, 1, data->term, 1)
	<= data->geo.timeout_lines ? TIMEOUT_NORMAL : TIMEOUT_TERSE;
      if (data->timeout_msg == TIMEOUT_TERSE)
	{
	  grub_free (msg_translated);
	  msg_translated = grub_xasprintf (_("%ds"), timeout);
	  if (grub_term_width (data->term) < 10)
	    data->timeout_msg = TIMEOUT_TERSE_NO_MARGIN;
	}
    }

  grub_print_message_indented (msg_translated,
			       data->timeout_msg == TIMEOUT_TERSE_NO_MARGIN ? 0 : 3,
			       data->timeout_msg == TIMEOUT_TERSE_NO_MARGIN ? 0 : 1,
			       data->term);
  grub_free (msg_translated);

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 
		      grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });
  grub_term_refresh (data->term);
}

static void
menu_text_set_chosen_entry (int entry, void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  int oldoffset = data->offset;
  int complete_redraw = 0;

  data->offset = entry - data->first;
  if (data->offset > data->geo.num_entries - 1)
    {
      data->first = entry - (data->geo.num_entries - 1);
      data->offset = data->geo.num_entries - 1;
      complete_redraw = 1;
    }
  if (data->offset < 0)
    {
      data->offset = 0;
      data->first = entry;
      complete_redraw = 1;
    }
  if (complete_redraw)
    print_entries (data->menu, data);
  else
    {
      print_entry (data->geo.first_entry_y + oldoffset, 0,
		   grub_menu_get_entry (data->menu, data->first + oldoffset),
		   data);
      print_entry (data->geo.first_entry_y + data->offset, 1,
		   grub_menu_get_entry (data->menu, data->first + data->offset),
		   data);
    }
  grub_term_refresh (data->term);
}

static void
menu_text_fini (void *dataptr)
{
  struct menu_viewer_data *data = dataptr;

  grub_term_setcursor (data->term, 1);
  //grub_term_cls (data->term);
  grub_free (data);
}

static void
menu_text_clear_timeout (void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  int i;

  for (i = 0; i < data->geo.timeout_lines;i++)
    {
      grub_term_gotoxy (data->term, (struct grub_term_coordinate) {
	  0, data->geo.timeout_y + i });
      grub_print_spaces (data->term, grub_term_width (data->term) - 1);
    }
  if (data->geo.num_entries <= 5 && !data->geo.border)
    {
      grub_term_gotoxy (data->term,
			(struct grub_term_coordinate) { 
			  data->geo.first_entry_x + data->geo.entry_width
			    + data->geo.border + 1,
			    data->geo.first_entry_y + data->geo.num_entries - 1
			    });
      grub_putcode (' ', data->term);

      data->geo.timeout_lines = 0;
      data->geo.num_entries++;
      print_entries (data->menu, data);
    }
  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) {
		      grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });
  grub_term_refresh (data->term);
}

grub_err_t 
grub_menu_try_text (struct grub_term_output *term, 
		    int entry, grub_menu_t menu, int nested)
{
  struct menu_viewer_data *data;
  struct grub_menu_viewer *instance;

  if (miray_isbootcast())
  {
     return miray_menu_try_text(term, entry, menu);
  }

  instance = grub_zalloc (sizeof (*instance));
  if (!instance)
    return grub_errno;

  data = grub_zalloc (sizeof (*data));
  if (!data)
    {
      grub_free (instance);
      return grub_errno;
    }

  data->term = term;
  instance->data = data;
  instance->set_chosen_entry = menu_text_set_chosen_entry;
  instance->print_timeout = menu_text_print_timeout;
  instance->clear_timeout = menu_text_clear_timeout;
  instance->fini = menu_text_fini;

  data->menu = menu;

  data->offset = entry;
  data->first = 0;

  grub_term_setcursor (data->term, 0);
  grub_menu_init_page (nested, 0, &data->geo, data->term);

  if (data->offset > data->geo.num_entries - 1)
    {
      data->first = data->offset - (data->geo.num_entries - 1);
      data->offset = data->geo.num_entries - 1;
    }

  print_entries (menu, data);
  grub_term_refresh (data->term);
  grub_menu_register_viewer (instance);

  return GRUB_ERR_NONE;
}

/*
 * Miray Custom
 */

static void
miray_reset_cursor(struct menu_viewer_data *data)
{
  grub_term_gotoxy (data->term,
			(struct grub_term_coordinate) { GRUB_TERM_MARGIN,
			    data->geo.first_entry_y + data->geo.num_entries});
}

static int
miray_center_margin(const char * msg, struct grub_term_output *term)
{
  grub_ssize_t msg_len;
  grub_uint32_t *unicode_msg;
  grub_uint32_t *last_position;
  int ret;

  if (!msg)
    return 0;

  msg_len = grub_utf8_to_ucs4_alloc (msg,
				     &unicode_msg, &last_position);

  if (msg_len < 0)
    return 0;

  ret = grub_getstringwidth (unicode_msg, last_position, term);
  if (ret < (int) grub_term_width (term))
  {
    ret = ((int) grub_term_width (term) - 1 - ret) / 2;
  }
  else
    ret = 0;

  grub_free (unicode_msg);
  return ret;
}

static void
miray_bootcast_init_page (struct grub_term_output *term,
		       int y)
{
  grub_ssize_t msg_len;
  int posx;
  const char *msg_formatted;
  grub_uint32_t *unicode_msg;
  grub_uint32_t *last_position;

  //grub_term_cls (term);

  msg_formatted = grub_env_get("menu_title");
  if (msg_formatted == 0)
    msg_formatted = "";

  msg_len = grub_utf8_to_ucs4_alloc (msg_formatted,
				     &unicode_msg, &last_position);

  if (msg_len < 0)
    return;

  posx = grub_getstringwidth (unicode_msg, last_position, term);
  posx = ((int) grub_term_width (term) - posx) / 2;
  if (posx < 0)
    posx = 0;
  grub_term_gotoxy (term, (struct grub_term_coordinate) { posx, y });

  grub_print_ucs4 (unicode_msg, last_position, 0, 0, term);
  grub_putcode ('\n', term);
  grub_putcode ('\n', term);
  grub_free (unicode_msg);
}


static void
miray_text_print_notimeout (void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  const char * msg;
  int margin = 0;

  msg = grub_env_get("menu_prompt");

  if (!msg)
    return;

  margin = miray_center_margin(msg, data->term);

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 0, data->geo.timeout_y });

  grub_print_message_indented (msg, margin, 0, data->term);

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) {
		      grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });

  miray_reset_cursor(data);
  grub_term_refresh (data->term);
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static void
miray_text_print_timeout (int timeout, void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  char *msg_translated = 0;
  char *msg_combined = 0;
  const char * msg_prompt;
  const char * msg_timeout;
  int margin = 0;

  msg_prompt = grub_env_get("menu_prompt");
  msg_timeout = grub_env_get("menu_timeout");

  if (!msg_prompt)
    return;

  // Check if timeout is set and if the format string is supported
  if (!msg_timeout)
  {
    miray_text_print_notimeout(dataptr);
    return;
  }

  msg_translated = grub_xasprintf (msg_timeout, timeout);
  if (!msg_translated)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  msg_combined = grub_xasprintf("%s %s", msg_prompt, msg_translated);
  if (!msg_combined)
    {
      grub_free(msg_translated);
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  margin = miray_center_margin(msg_combined, data->term);

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 0, data->geo.timeout_y });

  grub_print_message_indented (msg_combined, margin, 0, data->term);
  grub_free (msg_translated);
  grub_free (msg_combined);

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) {
		      grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });

  miray_reset_cursor(data);
  grub_term_refresh (data->term);
}

#pragma GCC diagnostic error "-Wformat-nonliteral"

static int
miray_print_message_env (struct grub_term_output *term, const char * env, int max_lines)
{
  int margin = 0;
  const char * msg;
  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);

  if (!env)
    return 0;

  msg = grub_env_get(env);

  if (!msg)
   return 0;

  if (max_lines == 1)
    margin = miray_center_margin(msg, term);

  return grub_print_message_indented_real_maxlines (msg, margin,
					   margin, term, 0, max_lines);
}

static int
miray_check_lines_env(struct grub_term_output *term, const char * env, int max_lines)
{
  int ret;
  const char * msg;

  if (!env)
    return 0;

  msg = grub_env_get(env);

  if (!msg)
   return 0;
  ret = grub_print_message_indented_real (msg, STANDARD_MARGIN,
					   STANDARD_MARGIN, term, 1);
  return ret > max_lines ? max_lines : ret;
}

static int
print_warning_message (struct grub_term_output *term, int dry_run, int max_lines)
{
  int margin = 0;
  int ret = 0;
  const char * msg;
  grub_uint8_t old_color_highlight = grub_term_highlight_color;

  msg = grub_env_get("menu_warning");

  if (!msg)
   return 0;

  grub_term_highlight_color = grub_color_menu_highlight_warning;
  grub_term_setcolorstate (term, GRUB_TERM_COLOR_HIGHLIGHT);

  if (max_lines == 1)
    margin = miray_center_margin(msg, term);

  ret += grub_print_message_indented_real_maxlines (msg, margin,
					   margin, term, dry_run, max_lines);

  grub_term_highlight_color = old_color_highlight;

  return ret;
}

static char * miray_entry_string(grub_menu_entry_t entry)
{
  char * ret;

  if (entry)
  {
     if (entry->hotkey != 0)
       ret = grub_xasprintf("<%C> %s", entry->hotkey, entry->title);
     else
       ret = grub_xasprintf("    %s", entry->title);
  }
  else
  {
    ret = grub_strdup("");
  }

  return ret;
}

static int get_max_entry_len(grub_menu_t menu)
{
   int ret = 0;
   grub_menu_entry_t e;

   if (menu == 0)
     return 0;

   e = menu->entry_list;

   while (e != 0)
   {
     char * text = miray_entry_string(e);
     int len = grub_strlen(text);

     if (len > ret)
       ret = len;

     grub_free(text);

     e = e->next;
   }

   return ret + 1;
}

void
miray_menu_init_page (grub_menu_t menu,
		     struct grub_term_screen_geometry *geo,
		     struct grub_term_output *term)
{
  static const int title_margin = 2;
  /* <title> */
  static const int top_space = 2;
  /* <optional custom top message> */
  int top_msg_lines = 2;
  int top_msg_spacer = 1;
  /* <please select + timeout> */
  static const int timeout_spacer = 1;
  /* Entries */
  static const int entries_spacer = 1;
  /* <optional custom bottom message> */
  int bottom_msg_lines = 3;
  int bottom_msg_spacer = 1;
  /* <warning / error message> */
  static const int warning_msg_lines = 1;
  static const int warning_msg_spacer = 1;
  /* <footer> */
  static const int footer_margin = 1;

  static const int min_vmargin = 3;
  int max_entry_len = 0;

  int top_msg_pos;
  int warning_msg_pos;

  grub_uint8_t old_color_normal, old_color_highlight;

  geo->border = 0;

  geo->first_entry_x = min_vmargin;
  geo->entry_width = grub_term_width (term) - 2 - 2 * min_vmargin;

  max_entry_len = get_max_entry_len(menu);
  if (max_entry_len > 0 && max_entry_len < geo->entry_width)
  {
    geo->entry_width = max_entry_len;
    geo->first_entry_x = (grub_term_width (term) - geo->entry_width - 2) / 2;
  }

  top_msg_lines = miray_check_lines_env(term, "menu_msg_custom_top",  top_msg_lines);
  bottom_msg_lines = miray_check_lines_env(term, "menu_msg_custom_bottom", bottom_msg_lines);

  top_msg_pos = title_margin + 1 /* Title line  */ + top_space;

  geo->timeout_lines = 1;
  geo->timeout_y = top_msg_pos;
  if (top_msg_lines > 0)
    geo->timeout_y += top_msg_lines + top_msg_spacer;

  geo->first_entry_y = geo->timeout_y + geo->timeout_lines + timeout_spacer;
  geo->num_entries = grub_term_height (term) - geo->first_entry_y
    - entries_spacer
    - warning_msg_lines - warning_msg_spacer
    - 1 - footer_margin;

  warning_msg_pos = geo->first_entry_y + geo->num_entries + entries_spacer;

  if (bottom_msg_lines > 0)
    geo->num_entries -= (bottom_msg_lines + bottom_msg_spacer);

  if (geo->border)
  {
    if (geo->first_entry_x > 0)
    {
      geo->first_entry_x += 1;
      geo->entry_width -= 2;
    }

    geo->first_entry_y += 1;
    geo->num_entries -= 2;
  }

  if (geo->entry_width <= 0)
      geo->entry_width = 1;

  /* By default, use the same colors for the menu.  */
  old_color_normal = grub_term_normal_color;
  old_color_highlight = grub_term_highlight_color;
  grub_color_menu_normal = grub_term_normal_color;
  grub_color_menu_highlight = grub_term_highlight_color;
  grub_color_menu_highlight_warning = grub_term_highlight_color;

  grub_parse_color_name_pair (&grub_color_menu_highlight, "white/light-blue");
  grub_parse_color_name_pair (&grub_color_menu_highlight_warning, "yellow/black");

  /* Then give user a chance to replace them.  */
  grub_parse_color_name_pair (&grub_color_menu_normal,
			      grub_env_get ("menu_color_normal"));
  grub_parse_color_name_pair (&grub_color_menu_highlight,
			      grub_env_get ("menu_color_highlight"));

  grub_term_cls (term);
  miray_bootcast_init_page (term, title_margin);

   if (top_msg_lines > 0)
    {
      grub_term_gotoxy (term,
			(struct grub_term_coordinate) { GRUB_TERM_MARGIN,
			    top_msg_pos});

      miray_print_message_env(term, "menu_msg_custom_top", top_msg_lines);
    }

   if (bottom_msg_lines > 0)
    {
      grub_term_gotoxy (term,
			(struct grub_term_coordinate) { GRUB_TERM_MARGIN,
			    warning_msg_pos - bottom_msg_lines - bottom_msg_spacer});

      miray_print_message_env(term, "menu_msg_custom_bottom", top_msg_lines);
    }

   if (warning_msg_lines > 0)
   {
      grub_term_gotoxy (term,
			(struct grub_term_coordinate) { GRUB_TERM_MARGIN,
			    warning_msg_pos});

      print_warning_message (term, 0, warning_msg_lines);
   }

   {
      grub_term_gotoxy (term,
			(struct grub_term_coordinate) { GRUB_TERM_MARGIN,
			    warning_msg_pos + warning_msg_lines + warning_msg_spacer});

      miray_print_message_env (term, "menu_footer", 1);
   }

  grub_term_normal_color = grub_color_menu_normal;
  grub_term_highlight_color = grub_color_menu_highlight;
  if (geo->border)
    draw_border (term, geo);
  grub_term_normal_color = old_color_normal;
  grub_term_highlight_color = old_color_highlight;

  geo->right_margin = grub_term_width (term)
    - geo->first_entry_x
    - geo->entry_width - 1;
}

static void
miray_print_entry (int y, int highlight, grub_menu_entry_t entry,
	     const struct menu_viewer_data *data)
{
  char * title;
  grub_size_t title_len;
  grub_ssize_t len;
  grub_uint32_t *unicode_title;
  grub_ssize_t i;
  grub_uint8_t old_color_normal, old_color_highlight;

  title = miray_entry_string(entry);
  if (!title)
    return;

  title_len = grub_strlen (title);
  unicode_title = grub_malloc (title_len * sizeof (*unicode_title));
  if (! unicode_title)
  {
    /* XXX How to show this error?  */
    grub_free(title);
    return;
  }

  len = grub_utf8_to_ucs4 (unicode_title, title_len,
                           (grub_uint8_t *) title, -1, 0);
  if (len < 0)
    {
      /* It is an invalid sequence.  */
      grub_free (unicode_title);
      grub_free (title);
      return;
    }

  old_color_normal = grub_term_normal_color;
  old_color_highlight = grub_term_highlight_color;
  grub_term_normal_color = grub_color_menu_normal;
  grub_term_highlight_color = grub_color_menu_highlight;
  grub_term_setcolorstate (data->term, highlight
			   ? GRUB_TERM_COLOR_HIGHLIGHT
			   : GRUB_TERM_COLOR_NORMAL);

  grub_term_gotoxy (data->term, (struct grub_term_coordinate) {
      data->geo.first_entry_x, y });

  for (i = 0; i < len; i++)
    if (unicode_title[i] == '\n' || unicode_title[i] == '\b'
	|| unicode_title[i] == '\r' || unicode_title[i] == '\e')
      unicode_title[i] = ' ';

  if (data->geo.num_entries > 1)
    grub_putcode (' ', data->term);
    //grub_putcode (highlight ? '*' : ' ', data->term);

  grub_print_ucs4_menu (unicode_title,
			unicode_title + len,
			0,
			data->geo.right_margin,
			data->term, 0, 1,
			GRUB_UNICODE_RIGHTARROW, 0);

  grub_term_setcolorstate (data->term, GRUB_TERM_COLOR_NORMAL);
  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) {
		      grub_term_cursor_x (&data->geo), y });

  grub_term_normal_color = old_color_normal;
  grub_term_highlight_color = old_color_highlight;

  grub_term_setcolorstate (data->term, GRUB_TERM_COLOR_NORMAL);
  grub_free (unicode_title);
  grub_free (title);
}

static void
miray_print_entries (grub_menu_t menu, const struct menu_viewer_data *data)
{
  grub_menu_entry_t e;
  int i;
  int highlight = 1;

  {
    const char * msg = grub_env_get("menu_nohighlight");
    if (msg != 0)
      highlight = 0;
  }

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) {
		      data->geo.first_entry_x + data->geo.entry_width
			+ data->geo.border + 1,
			data->geo.first_entry_y });

  if (data->geo.num_entries != 1)
    {
      if (data->first)
	grub_putcode (GRUB_UNICODE_UPARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }
  e = grub_menu_get_entry (menu, data->first);

  for (i = 0; i < data->geo.num_entries; i++)
    {
      miray_print_entry (data->geo.first_entry_y + i, highlight && data->offset == i,
		   e, data);
      if (e)
	e = e->next;
    }

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { data->geo.first_entry_x + data->geo.entry_width
			+ data->geo.border + 1,
			data->geo.first_entry_y + data->geo.num_entries - 1 });
  if (data->geo.num_entries == 1)
    {
      if (data->first && e)
	grub_putcode (GRUB_UNICODE_UPDOWNARROW, data->term);
      else if (data->first)
	grub_putcode (GRUB_UNICODE_UPARROW, data->term);
      else if (e)
	grub_putcode (GRUB_UNICODE_DOWNARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }
  else
    {
      if (e)
	grub_putcode (GRUB_UNICODE_DOWNARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });
}


static void
miray_text_set_chosen_entry (int entry, void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  int oldoffset = data->offset;
  int complete_redraw = 0;

  {
    const char * msg = grub_env_get("menu_nohighlight");
    if (msg != 0)
      return;
  }

  data->offset = entry - data->first;
  if (data->offset > data->geo.num_entries - 1)
    {
      data->first = entry - (data->geo.num_entries - 1);
      data->offset = data->geo.num_entries - 1;
      complete_redraw = 1;
    }
  if (data->offset < 0)
    {
      data->offset = 0;
      data->first = entry;
      complete_redraw = 1;
    }
  if (complete_redraw)
    miray_print_entries (data->menu, data);
  else
    {
      miray_print_entry (data->geo.first_entry_y + oldoffset, 0,
		   grub_menu_get_entry (data->menu, data->first + oldoffset),
		   data);
      miray_print_entry (data->geo.first_entry_y + data->offset, 1,
		   grub_menu_get_entry (data->menu, data->first + data->offset),
		   data);
    }

  miray_reset_cursor(data);
  grub_term_refresh (data->term);
}

grub_err_t
miray_menu_try_text (struct grub_term_output *term,
		    int entry, grub_menu_t menu)
{
  struct menu_viewer_data *data;
  struct grub_menu_viewer *instance;

  instance = grub_zalloc (sizeof (*instance));
  if (!instance)
    return grub_errno;

  data = grub_zalloc (sizeof (*data));
  if (!data)
    {
      grub_free (instance);
      return grub_errno;
    }

  data->term = term;
  instance->data = data;
  instance->set_chosen_entry = miray_text_set_chosen_entry;
  instance->print_timeout = miray_text_print_timeout;
  instance->clear_timeout = miray_text_print_notimeout;
  instance->fini = menu_text_fini;

  data->menu = menu;

  data->offset = entry;
  data->first = 0;

  grub_term_setcursor (data->term, 0);
  miray_menu_init_page (menu, &data->geo, data->term);

  if (data->offset > data->geo.num_entries - 1)
    {
      data->first = data->offset - (data->geo.num_entries - 1);
      data->offset = data->geo.num_entries - 1;
    }

  miray_print_entries (menu, data);
  miray_text_print_notimeout(data);
  miray_reset_cursor(data);
  grub_term_refresh (data->term);
  grub_menu_register_viewer (instance);

  return GRUB_ERR_NONE;
}
