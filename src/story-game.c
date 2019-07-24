/* Copyright (C) 2006-2009, 2010, 2011, 2012, 2014, 2015 P. F. Chimento
 * This file is part of GNOME Inform 7.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libchimara/chimara-glk.h>
#include <libchimara/chimara-if.h>
#include "story.h"
#include "story-private.h"
#include "error.h"
#include "node.h"
#include "skein.h"

/* Methods of I7Story having to do with the Story (formerly called Game) pane:
 - callbacks for when the compiler tool chain is finished
 - methods for communicating with the Chimara interpreter
*/

/* Compile finished action: whatever the final product of the compiling process
 * was, tell Chimara to run it. */
void
i7_story_run_compiler_output(I7Story *story)
{
	I7_STORY_USE_PRIVATE(story, priv);
	GError *err = NULL;

	/* Rewind the Skein to the beginning */
	i7_skein_reset(priv->skein, TRUE);

	I7StoryPanel side = i7_story_choose_panel(story, I7_PANE_STORY);
	ChimaraIF *glk = CHIMARA_IF(story->panel[side]->tabs[I7_PANE_STORY]);

	/* Load and start the interpreter */
	if(!chimara_if_run_game_file(glk, priv->compiler_output_file, &err)) {
		error_dialog(GTK_WINDOW(story), err, _("Could not load interpreter: "));
	}

	/* Display and set the focus to the interpreter */
	i7_story_show_pane(story, I7_PANE_STORY);
	gtk_widget_grab_focus(GTK_WIDGET(glk));
}

/* Compile finished action: run the output and feed it the command "test me" */
void
i7_story_test_compiler_output(I7Story *story)
{
	I7_STORY_USE_PRIVATE(story, priv);
	GError *err = NULL;

	I7StoryPanel side = i7_story_choose_panel(story, I7_PANE_STORY);
	ChimaraIF *glk = CHIMARA_IF(story->panel[side]->tabs[I7_PANE_STORY]);

	/* Load and start the interpreter */
	if(!chimara_if_run_game_file(glk, priv->compiler_output_file, &err)) {
		error_dialog(GTK_WINDOW(story), err, _("Could not load interpreter: "));
	}

	/* Tell the interpreter to "test me" */
	i7_skein_reset(priv->skein, TRUE);
	chimara_glk_feed_line_input(CHIMARA_GLK(glk), "test me");

	/* Display and set the focus to the interpreter */
	i7_story_show_pane(story, I7_PANE_STORY);
	gtk_widget_grab_focus(GTK_WIDGET(glk));
}

/* Finish setting up the interpreter when forced input is done processing;
 * this signal is set up in play_commands() because the interpreter processes
 * the commands you feed it asynchronously. */
static void
on_waiting(ChimaraGlk *glk)
{
	if(!chimara_glk_is_line_input_pending(glk)) {
		chimara_glk_set_interactive(glk, TRUE);

		/* Set focus to the interpreter */
		gtk_widget_grab_focus(GTK_WIDGET(glk));

		/* Disconnect this signal handler - have to do it this way, because
		 a gulong can't be packed into a pointer */
		gulong handler = g_signal_handler_find(glk, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
			0, 0, NULL, on_waiting, NULL);
		g_signal_handler_disconnect(glk, handler);
	}
}

/* Force input (listed in @commands) to the interpreter. If @start_interpreter
 * is TRUE, run the compiler's finished product and reset the skein to the
 * start knot. */
static void
play_commands(I7Story *story, GSList *commands, gboolean start_interpreter)
{
	I7_STORY_USE_PRIVATE(story, priv);
	GError *err = NULL;

	I7StoryPanel side = i7_story_choose_panel(story, I7_PANE_STORY);
	ChimaraIF *glk = CHIMARA_IF(story->panel[side]->tabs[I7_PANE_STORY]);

	/* Set non-interactive if there are commands, because we don't want to
	 scroll through screens full of -- more -- prompts */
	if(commands)
		chimara_glk_set_interactive(CHIMARA_GLK(glk), FALSE);

	/* Load and start the interpreter */
	if(start_interpreter) {
		if(!chimara_if_run_game_file(glk, priv->compiler_output_file, &err)) {
			error_dialog(GTK_WINDOW(story), err, _("Could not load interpreter: "));
		}
		i7_skein_reset(priv->skein, TRUE);
	}

	/* Display the interpreter */
	i7_story_show_pane(story, I7_PANE_STORY);

	/* Feed the commands up to the "played" pointer in the skein into the
	interpreter */
	GSList *iter;
	for(iter = commands; iter; iter = g_slist_next(iter))
		chimara_glk_feed_line_input(CHIMARA_GLK(glk), (gchar *)iter->data);

	/* Finish the rest when the input is done being processed */
	g_signal_connect(glk, "waiting", G_CALLBACK(on_waiting), NULL);
}

/* Compile finished action: run the compiler output and feed commands from the
 * Skein leading up to the last play point. */
void
i7_story_run_compiler_output_and_replay(I7Story *story)
{
	I7_STORY_USE_PRIVATE(story, priv);

	/* Get a list of the commands that need to be fed in */
	GSList *commands = i7_skein_get_commands(priv->skein);
	play_commands(story, commands, TRUE);
	g_slist_foreach(commands, (GFunc)g_free, NULL);
	g_slist_free(commands);
}

/* Compile finished action: run the compiler output and feed commands from the
 * Skein leading up to a certain knot @node. */
void
i7_story_run_compiler_output_and_play_to_node(I7Story *story, I7Node *node)
{
	I7_STORY_USE_PRIVATE(story, priv);

	/* Get a list of the commands that need to be fed in */
	GSList *commands = i7_skein_get_commands_to_node(priv->skein, i7_skein_get_root_node(priv->skein), node);
	play_commands(story, commands, TRUE);
	g_slist_foreach(commands, (GFunc)g_free, NULL);
	g_slist_free(commands);
}

/* Don't restart the game, but feed commands from the Skein leading from the
 * currently played knot to another knot @node. */
void
i7_story_run_commands_from_node(I7Story *story, I7Node *node)
{
	I7_STORY_USE_PRIVATE(story, priv);

	I7Node *played = i7_skein_get_played_node(priv->skein);
	g_assert(g_node_is_ancestor(played->gnode, node->gnode));

	/* Get a list of the commands that need to be fed in */
	GSList *commands = i7_skein_get_commands_to_node(priv->skein, played, node);
	play_commands(story, commands, FALSE);
	g_slist_foreach(commands, (GFunc)g_free, NULL);
	g_slist_free(commands);
}

/* One-off data structure for passing variables to the handlers below */
struct RunSkeinData {
	I7Story *story;
	I7Skein *skein;
	ChimaraGlk *glk;
	GFile *file_to_run;

	GSList *commands;
	GSList *blessed_nodes;
	GSList *node;
	unsigned long started_handler, waiting_handler;
	gboolean finished; /* don't have to use a GCond because this communication
	is within the same thread and only one way? */
};

/* callback to stop interpreter after all on_game_command_idle() calls finish */
static gboolean
oneshot_chimara_glk_stop(struct RunSkeinData *data)
{
	chimara_glk_stop(data->glk);
	data->finished = TRUE;
	return FALSE; /* one-shot */
}

/* Helper function: stop the interpreter when forced input is done processing;
this signal is set up in run_entire_skein_loop() because the interpreter
processes the commands you feed it asynchronously. */
static void
on_waiting_stop_interpreter(ChimaraGlk *glk, struct RunSkeinData *data)
{
	if(!chimara_glk_is_line_input_pending(glk)) {
		/* Stop the interpreter */
		g_idle_add((GSourceFunc)oneshot_chimara_glk_stop, data);

		/* Disconnect this handler */
		g_signal_handler_disconnect(data->glk, data->waiting_handler);
	}
}

/* callback to call i7_story_show_pane from the main thread */
static gboolean
idle_story_show_pane(I7Story* story)
{
	i7_story_show_pane(story, I7_PANE_STORY);
	return FALSE; /* one-shot */
}

/* Helper function: feed the commands to the interpreter after the game has
started; this signal is set up in run_entire_skein_loop() because it's not clear
how soon the game is ready to accept input after the call to
chimara_if_run_game_file(). */
static void
on_started_feed_commands(ChimaraGlk *glk, struct RunSkeinData *data)
{
	/* Display the interpreter */
	gdk_threads_add_idle((GSourceFunc)idle_story_show_pane, data->story);

	/* Feed the commands into the interpreter */
	GSList *iter;
	for(iter = data->commands; iter; iter = g_slist_next(iter)) {
		chimara_glk_feed_line_input(glk, (char *)iter->data);
	}

	/* Disconnect this handler */
	g_signal_handler_disconnect(data->glk, data->started_handler);
}

/* Helper function: Run the compiler output and feed the commands from the
Skein up to a certain knot @node. Wait until the interpreter is done and stop
it in preparation for the next knot. */
static gboolean
run_entire_skein_loop(struct RunSkeinData *data)
{
	GError *err = NULL;

	if(!data->node) {
		g_slist_free(data->blessed_nodes);

		chimara_glk_set_interactive(data->glk, TRUE);

		g_object_unref(data->file_to_run);
		g_slice_free(struct RunSkeinData, data);

		return FALSE; /* done */
	}

	data->commands = i7_skein_get_commands_to_node(data->skein, i7_skein_get_root_node(data->skein), data->node->data);

	i7_skein_reset(data->skein, TRUE);

	/* Set up signals to finish the actions when the input is done being
	processed */
	data->started_handler = g_signal_connect_after(data->glk, "started",
	    G_CALLBACK(on_started_feed_commands), data);
	data->waiting_handler = g_signal_connect_after(data->glk, "waiting",
	    G_CALLBACK(on_waiting_stop_interpreter), data);
	data->finished = FALSE;

	/* Start the interpreter */
	if(!chimara_if_run_game_file(CHIMARA_IF(data->glk), data->file_to_run, &err)) {
		error_dialog(GTK_WINDOW(data->story), err, _("Could not load interpreter: "));
		g_signal_handler_disconnect(data->glk, data->waiting_handler);
		g_signal_handler_disconnect(data->glk, data->started_handler);
		goto finally;
	}

	/* This will run until all the line input forced in
	on_started_feed_commands() is finished processing */
	while(!data->finished)
		gtk_main_iteration_do(FALSE); /* don't block */

	/* This should block on the chimara_glk_stop() call in
	on_waiting_stop_interpreter() */
	chimara_glk_wait(data->glk);

finally:
	g_slist_foreach(data->commands, (GFunc)g_free, NULL);
	g_slist_free(data->commands);

	data->node = data->node->next;
	return TRUE; /* not done */
}

/*
 * i7_story_run_compiler_output_and_entire_skein:
 * @story: the story
 *
 * Callback for when compiling is finished. Plays through as many threads as
 * necessary to visit each blessed knot in the skein at least once.
 */
void
i7_story_run_compiler_output_and_entire_skein(I7Story *story)
{
	I7_STORY_USE_PRIVATE(story, priv);

	struct RunSkeinData *data = g_slice_new0(struct RunSkeinData);
	data->story = story;
	data->skein = priv->skein;
	data->file_to_run = g_object_ref(priv->compiler_output_file);

	/* Make sure the interpreter is non-interactive */
	I7StoryPanel side = i7_story_choose_panel(story, I7_PANE_STORY);
	data->glk = CHIMARA_GLK(story->panel[side]->tabs[I7_PANE_STORY]);
	chimara_glk_set_interactive(data->glk, FALSE);
	
	data->blessed_nodes = i7_skein_get_blessed_thread_ends(data->skein);
	data->node = data->blessed_nodes;
	gdk_threads_add_idle((GSourceFunc)run_entire_skein_loop, data);
}

/* Helper function: stop the game in @panel if it is running */
static void
panel_stop_running_game(I7Story *story, I7Panel *panel)
{
	ChimaraGlk *glk = CHIMARA_GLK(panel->tabs[I7_PANE_STORY]);
	chimara_glk_stop(glk);
	chimara_glk_wait(glk); /* Seems to be necessary? */
	chimara_glk_unload_plugin(glk);
}

/* Stop the currently running game in either panel */
void
i7_story_stop_running_game(I7Story *story)
{
	i7_story_foreach_panel(story, (I7PanelForeachFunc)panel_stop_running_game, NULL);
}

/* Returns whether a game is running in either panel */
gboolean
i7_story_get_game_running(I7Story *story)
{
	return chimara_glk_get_running(CHIMARA_GLK(story->panel[LEFT]->tabs[I7_PANE_STORY]))
		|| chimara_glk_get_running(CHIMARA_GLK(story->panel[RIGHT]->tabs[I7_PANE_STORY]));
}

/* Helper function: set the Chimara interpreter in @panel to prefer Git for
 * Glulx games */
static void
panel_set_use_git(I7Story *story, I7Panel *panel, gpointer data)
{
	ChimaraIFInterpreter interpreter = GPOINTER_TO_INT(data)? CHIMARA_IF_INTERPRETER_GIT : CHIMARA_IF_INTERPRETER_GLULXE;
	ChimaraIF *glk = CHIMARA_IF(panel->tabs[I7_PANE_STORY]);
	chimara_if_set_preferred_interpreter(glk, CHIMARA_IF_FORMAT_GLULX, interpreter);
}

/* Set all Chimara interpreters to prefer Git for Glulx games */
void
i7_story_set_use_git(I7Story *story, gboolean use_git)
{
	i7_story_foreach_panel(story, (I7PanelForeachFunc)panel_set_use_git, GINT_TO_POINTER(use_git));
}

/* Blorb resource load callback: look up the file name for a resource in the
 manifest.plist file, and search for it in the Materials folder. */
gchar *
load_blorb_resource(ChimaraResourceType usage, guint32 resnum, I7Story *story)
{
	I7_STORY_USE_PRIVATE(story, priv);
	g_return_val_if_fail(priv->manifest, NULL);

	/* Look up the filename in the manifest */
	gchar *resstring = g_strdup_printf("%d", resnum);
	const gchar *restype;
	switch(usage) {
		case CHIMARA_RESOURCE_SOUND:
			restype = "Sounds";
			break;
		case CHIMARA_RESOURCE_IMAGE:
		default:
			restype = "Graphics";
			break;
	}
	PlistObject *manifest_entry = plist_object_lookup(priv->manifest, restype, resstring, -1);
	g_return_val_if_fail(manifest_entry, NULL);
	g_free(resstring);

	/* Build the full path */
	gchar *filename = g_strdup(manifest_entry->string.val);
	GFile *materials_file = i7_story_get_materials_file(story);
	GFile *resource_file = g_file_get_child(materials_file, filename);
	g_free(filename);
	g_object_unref(materials_file);

	char *retval = g_file_get_path(resource_file);
	g_object_unref(resource_file);

	return retval;
}

/* SIGNAL HANDLERS */

/* Set the "stop" action to be sensitive when the game starts */
void
on_game_started(ChimaraGlk *game, I7Story *story)
{
	I7_STORY_USE_PRIVATE(story, priv);
	GtkAction *stop = gtk_action_group_get_action(priv->story_action_group, "stop");
	gtk_action_set_sensitive(stop, TRUE);
}

/* Set the "stop" action to be insensitive when the game finishes */
void
on_game_stopped(ChimaraGlk *game, I7Story *story)
{
	I7_STORY_USE_PRIVATE(story, priv);
	GtkAction *stop = gtk_action_group_get_action(priv->story_action_group, "stop");
	gtk_action_set_sensitive(stop, FALSE);
}

/* Temporary data for the below callback */
typedef struct {
	gchar *input;
	gchar *response;
	I7Story *story;
} CommandData;

void destroy_command_data(CommandData* data)
{
	g_free(data->input);
	g_free(data->response);
	g_slice_free(CommandData, data);
}

/* Grab commands entered by the user and store them in the skein */
gboolean on_game_command_idle(CommandData* data)
{
	I7_STORY_USE_PRIVATE(data->story, priv);
	if(!data->input) {
		/* If no input, then this was either the text printed before the first 
		 prompt, or a keypress of Enter in response to character input. */
		I7Node *root = i7_skein_get_root_node(priv->skein);
		if(i7_skein_get_current_node(priv->skein) == root) {
			i7_node_set_transcript_text(root, data->response);
		}
		return FALSE; /* one-shot */
	} 
	I7Node *node = i7_skein_new_command(priv->skein, data->input);
	i7_node_set_transcript_text(node, data->response);
	return FALSE; /* one-shot */
}

/* But actually do it in the main thread because creation of new nodes interacts in a few places with GooCanvas */
void
on_game_command(ChimaraIF *game, gchar *input, gchar *response, I7Story *story)
{
	CommandData* data = g_slice_new0(CommandData);
	data->input = g_strdup(input);
	data->response = g_strdup(response);
	data->story = story;
	gdk_threads_add_idle_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)on_game_command_idle, data, (GDestroyNotify)destroy_command_data);
}
