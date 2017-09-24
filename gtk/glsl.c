/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gio/gunixoutputstream.h>

#include <unistd.h>

#include "gsk/gskslnodeprivate.h"

static GBytes *
bytes_new_from_file (const char  *filename,
                     GError     **error)
{
  gchar *data;
  gsize length;

  if (!g_file_get_contents (filename,
                            &data, &length,
                            error))
    return FALSE;

  return g_bytes_new_take (data, length);
}

static gboolean
compile (GOutputStream *output,
         const char    *filename)
{
  GBytes *bytes;
  GskSlNode *program;
  GError *error = NULL;

  bytes = bytes_new_from_file (filename, &error);
  if (bytes == NULL)
    {
      g_print (error->message);
      g_error_free (error);
      return FALSE;
    }

  program = gsk_sl_node_new_program (bytes, NULL);
  g_bytes_unref (bytes);
  if (program == NULL)
    return FALSE;

  bytes = gsk_sl_node_compile (program);
  if (!g_output_stream_write_all (output, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL, NULL, &error))
    {
      g_print (error->message);
      g_error_free (error);
      g_bytes_unref (bytes);
      gsk_sl_node_unref (program);
      return FALSE;
    }

  g_bytes_unref (bytes);
  gsk_sl_node_unref (program);

  return TRUE;
}

static gboolean
dump (GOutputStream *output,
      const char    *filename)
{
  GBytes *bytes;
  GString *string;
  GskSlNode *program;
  GError *error = NULL;

  bytes = bytes_new_from_file (filename, &error);
  if (bytes == NULL)
    {
      g_print (error->message);
      g_error_free (error);
      return FALSE;
    }

  program = gsk_sl_node_new_program (bytes, NULL);
  g_bytes_unref (bytes);
  if (program == NULL)
    return FALSE;

  string = g_string_new (NULL);
  gsk_sl_node_print (program, string);
  if (!g_output_stream_write_all (output, string->str, string->len, NULL, NULL, &error))
    {
      g_print (error->message);
      g_error_free (error);
      g_string_free (string, TRUE);
      gsk_sl_node_unref (program);
      return FALSE;
    }

  g_string_free (string, TRUE);
  gsk_sl_node_unref (program);

  return TRUE;
}

static void
usage (GOptionContext *ctx)
{
  char *help = g_option_context_get_help (ctx, TRUE, NULL);

  g_print ("%s", help);
  g_free (help);
  exit (EXIT_FAILURE);
}

int
main (int argc, char *argv[])
{
  GOptionContext *ctx;
  char **filenames = NULL;
  char *output_file = NULL;
  gboolean print = FALSE;
  const GOptionEntry entries[] = {
    { "print", 'p', 0, G_OPTION_ARG_NONE, &print, "Print instead of compiling", NULL },
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_file, "Output filename", "FILE" },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, "List of input files", "FILE [FILE...]" },
    { NULL, }
  };
  GError *error = NULL;
  GOutputStream *output;
  gboolean success = TRUE;
  guint i;

  g_set_prgname ("gtk-glsl");

  gtk_init ();

  ctx = g_option_context_new (NULL);
  g_option_context_add_main_entries (ctx, entries, NULL);

  if (!g_option_context_parse (ctx, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (filenames == NULL)
    usage (ctx);

  g_option_context_free (ctx);

  if (output_file == NULL)
    {
      output = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
    }
  else
    {
      GFile *file = g_file_new_for_path (output_file);
      output = G_OUTPUT_STREAM (g_file_replace (file,
                                                NULL, FALSE,
                                                G_FILE_CREATE_REPLACE_DESTINATION,
                                                NULL,
                                                &error));
      g_object_unref (file);
      if (output == NULL)
        {
          g_printerr ("Error creating output file: %s\n", error->message);
          g_error_free (error);
          exit (1);
        }
    }

  for (i = 0; success && filenames[i] != NULL; i++)
    {
      if (print)
        success = dump (output, filenames[i]);
      else
        success = compile (output, filenames[i]);
    }

 if (!g_output_stream_close (output, NULL, &error))
  {
    g_printerr ("%s\n", error->message);
    g_error_free (error);
    success = FALSE;
  }

  g_strfreev (filenames);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
