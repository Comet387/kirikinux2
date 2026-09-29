#include "launcher.h"

#include "game_library.h"
#include "storage_root.h"
#ifdef KRKR2_HAVE_TJS
#include "tjs_runtime.h"
#endif

#include <filesystem>
#include <string>

#ifdef KRKR2_HAVE_GTK3
#include <gtk/gtk.h>

namespace krkr2 {
namespace {

struct LauncherState {
  explicit LauncherState(const HostOptions &host_options)
      : options(host_options), library(GameLibrary::default_storage_path()) {}

  HostOptions options;
  GameLibrary library;
  GtkWidget *window = nullptr;
  GtkWidget *game_list = nullptr;
  GtkWidget *status = nullptr;
};

std::string display_name(const std::string &path) {
  const std::filesystem::path fs_path(path);
  std::string name = fs_path.filename().string();
  return name.empty() ? fs_path.string() : name;
}

bool probe_game(const std::string &path, std::string &error) {
  GamePath game;
  if (!resolve_game(path, game, error)) return false;
  StorageRoot storage;
  if (!storage.open(path, error)) return false;
  if (!storage.exists("startup.tjs") &&
      !storage.exists("System/Initialize.tjs")) {
    error = "The selection has neither startup.tjs nor System/Initialize.tjs";
    return false;
  }
  return true;
}

void set_status(LauncherState *state, const std::string &message,
                const char *style_class = nullptr) {
  gtk_label_set_text(GTK_LABEL(state->status), message.c_str());
  GtkStyleContext *context = gtk_widget_get_style_context(state->status);
  gtk_style_context_remove_class(context, "success");
  gtk_style_context_remove_class(context, "error");
  if (style_class) gtk_style_context_add_class(context, style_class);
}

void show_error(LauncherState *state, const std::string &title,
                const std::string &message) {
  set_status(state, message, "error");
  GtkWidget *dialog = gtk_message_dialog_new(
      GTK_WINDOW(state->window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
      GTK_BUTTONS_CLOSE, "%s", message.c_str());
  gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

void refresh_game_list(LauncherState *state);

void play_game(LauncherState *state, const std::string &path) {
  std::string error;
  if (!probe_game(path, error)) {
    show_error(state, "Cannot run game", error + "\n\n" + path);
    return;
  }
  if (!state->library.remember(path, error)) {
    show_error(state, "Cannot save game", error);
    return;
  }
  refresh_game_list(state);
  set_status(state, "Starting " + display_name(path) + " ...");
  while (gtk_events_pending()) gtk_main_iteration();
#ifdef KRKR2_HAVE_TJS
  const TjsRunResult result = execute_tjs_startup(path);
  if (!result.ok) {
    show_error(state, "Game startup stopped", result.error);
    return;
  }
  set_status(state, "Startup script completed for " + display_name(path) +
                        ". Native rendering is not complete yet.",
             "success");
#else
  show_error(state, "Game engine unavailable",
             "This build does not include the TJS2 interpreter.");
#endif
}

std::string button_path(GtkButton *button) {
  const char *path = static_cast<const char *>(
      g_object_get_data(G_OBJECT(button), "krkr2-game-path"));
  return path ? path : std::string();
}

void on_play_clicked(GtkButton *button, gpointer user_data) {
  play_game(static_cast<LauncherState *>(user_data), button_path(button));
}

void on_remove_clicked(GtkButton *button, gpointer user_data) {
  auto *state = static_cast<LauncherState *>(user_data);
  std::string error;
  if (!state->library.forget(button_path(button), error)) {
    show_error(state, "Cannot update game library", error);
    return;
  }
  refresh_game_list(state);
  set_status(state, "Game removed from the recent list.");
}

GtkWidget *make_game_row(LauncherState *state, const std::string &path) {
  GtkWidget *row = gtk_list_box_row_new();
  g_object_set_data_full(G_OBJECT(row), "krkr2-game-path",
                         g_strdup(path.c_str()), g_free);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width(GTK_CONTAINER(box), 12);
  gtk_container_add(GTK_CONTAINER(row), box);

  GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_hexpand(labels, TRUE);
  gtk_box_pack_start(GTK_BOX(box), labels, TRUE, TRUE, 0);

  gchar *escaped = g_markup_escape_text(display_name(path).c_str(), -1);
  const std::string markup =
      std::string("<span weight=\"bold\" size=\"large\">") + escaped +
      "</span>";
  g_free(escaped);
  GtkWidget *title = gtk_label_new(nullptr);
  gtk_label_set_markup(GTK_LABEL(title), markup.c_str());
  gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
  gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start(GTK_BOX(labels), title, FALSE, FALSE, 0);

  GtkWidget *full_path = gtk_label_new(path.c_str());
  gtk_label_set_xalign(GTK_LABEL(full_path), 0.0f);
  gtk_label_set_ellipsize(GTK_LABEL(full_path), PANGO_ELLIPSIZE_MIDDLE);
  gtk_style_context_add_class(gtk_widget_get_style_context(full_path), "path");
  gtk_box_pack_start(GTK_BOX(labels), full_path, FALSE, FALSE, 0);

  GtkWidget *remove = gtk_button_new_with_label("Remove");
  g_object_set_data_full(G_OBJECT(remove), "krkr2-game-path",
                         g_strdup(path.c_str()), g_free);
  g_signal_connect(remove, "clicked", G_CALLBACK(on_remove_clicked), state);
  gtk_box_pack_start(GTK_BOX(box), remove, FALSE, FALSE, 0);

  GtkWidget *play = gtk_button_new_with_label("Run");
  gtk_style_context_add_class(gtk_widget_get_style_context(play),
                              "suggested-action");
  g_object_set_data_full(G_OBJECT(play), "krkr2-game-path",
                         g_strdup(path.c_str()), g_free);
  g_signal_connect(play, "clicked", G_CALLBACK(on_play_clicked), state);
  gtk_box_pack_start(GTK_BOX(box), play, FALSE, FALSE, 0);
  return row;
}

void refresh_game_list(LauncherState *state) {
  GList *children =
      gtk_container_get_children(GTK_CONTAINER(state->game_list));
  for (GList *item = children; item; item = item->next)
    gtk_widget_destroy(GTK_WIDGET(item->data));
  g_list_free(children);

  if (state->library.games().empty()) {
    GtkWidget *empty = gtk_label_new(
        "No games added yet. Choose a game folder or an XP3 archive above.");
    gtk_label_set_line_wrap(GTK_LABEL(empty), TRUE);
    gtk_widget_set_margin_top(empty, 48);
    gtk_widget_set_margin_bottom(empty, 48);
    gtk_style_context_add_class(gtk_widget_get_style_context(empty), "empty");
    gtk_list_box_insert(GTK_LIST_BOX(state->game_list), empty, -1);
  } else {
    for (const std::string &path : state->library.games())
      gtk_list_box_insert(GTK_LIST_BOX(state->game_list),
                          make_game_row(state, path), -1);
  }
  gtk_widget_show_all(state->game_list);
}

void on_row_activated(GtkListBox *, GtkListBoxRow *row, gpointer user_data) {
  const char *path = static_cast<const char *>(
      g_object_get_data(G_OBJECT(row), "krkr2-game-path"));
  if (path) play_game(static_cast<LauncherState *>(user_data), path);
}

void choose_game(LauncherState *state, GtkFileChooserAction action) {
  const bool folder = action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
  GtkFileChooserNative *dialog = gtk_file_chooser_native_new(
      folder ? "Choose a Kirikiri game folder" : "Choose an XP3 archive",
      GTK_WINDOW(state->window), action, folder ? "Choose" : "Open", "Cancel");
  if (!folder) {
    GtkFileFilter *games = gtk_file_filter_new();
    gtk_file_filter_set_name(games, "Kirikiri archives (*.xp3, *.exe)");
    gtk_file_filter_add_pattern(games, "*.xp3");
    gtk_file_filter_add_pattern(games, "*.XP3");
    gtk_file_filter_add_pattern(games, "*.exe");
    gtk_file_filter_add_pattern(games, "*.EXE");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), games);
  }
  if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename =
        gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    if (filename) {
      play_game(state, filename);
      g_free(filename);
    }
  }
  g_object_unref(dialog);
}

void on_add_folder_clicked(GtkButton *, gpointer user_data) {
  choose_game(static_cast<LauncherState *>(user_data),
              GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
}

void on_add_archive_clicked(GtkButton *, gpointer user_data) {
  choose_game(static_cast<LauncherState *>(user_data),
              GTK_FILE_CHOOSER_ACTION_OPEN);
}

void on_drag_data_received(GtkWidget *, GdkDragContext *context, gint, gint,
                           GtkSelectionData *data, guint, guint time,
                           gpointer user_data) {
  auto *state = static_cast<LauncherState *>(user_data);
  bool accepted = false;
  gchar **uris = gtk_selection_data_get_uris(data);
  if (uris) {
    for (std::size_t index = 0; uris[index]; ++index) {
      gchar *filename = g_filename_from_uri(uris[index], nullptr, nullptr);
      if (!filename) continue;
      std::string error;
      if (probe_game(filename, error) && state->library.remember(filename, error))
        accepted = true;
      g_free(filename);
    }
    g_strfreev(uris);
  }
  refresh_game_list(state);
  set_status(state, accepted ? "Dropped games were added."
                             : "No valid Kirikiri game was found.",
             accepted ? "success" : "error");
  gtk_drag_finish(context, accepted, FALSE, time);
}

gboolean auto_start_game(gpointer user_data) {
  auto *state = static_cast<LauncherState *>(user_data);
  if (!state->options.game.empty()) play_game(state, state->options.game);
  return G_SOURCE_REMOVE;
}

void install_styles() {
  static const char css[] =
      "window { background: #f5f7fb; }"
      ".path,.empty,.status { color: #596579; }"
      ".status { padding: 10px 2px; }"
      ".status.success { color: #167447; }"
      ".status.error { color: #b42318; }"
      "list row { background: white; border-bottom: 1px solid #dfe4ec; }";
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, css, -1, nullptr);
  gtk_style_context_add_provider_for_screen(
      gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

} // namespace

int run_graphical_launcher(const HostOptions &options) {
  if (!gtk_init_check(nullptr, nullptr)) return kGraphicalLauncherUnavailable;
  install_styles();
  LauncherState state(options);
  std::string load_error;
  state.library.load(load_error);

  state.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(state.window), "Kirikiroid2 Game Library");
  gtk_window_set_default_size(GTK_WINDOW(state.window), options.width,
                              options.height);
  gtk_window_set_position(GTK_WINDOW(state.window), GTK_WIN_POS_CENTER);
  if (options.fullscreen) gtk_window_fullscreen(GTK_WINDOW(state.window));
  g_signal_connect(state.window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

  GtkWidget *header = gtk_header_bar_new();
  gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Kirikiroid2");
  gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Game Library");
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
  gtk_window_set_titlebar(GTK_WINDOW(state.window), header);
  GtkWidget *add_folder = gtk_button_new_with_label("Add Folder");
  g_signal_connect(add_folder, "clicked", G_CALLBACK(on_add_folder_clicked),
                   &state);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(header), add_folder);
  GtkWidget *add_archive = gtk_button_new_with_label("Add XP3");
  gtk_style_context_add_class(gtk_widget_get_style_context(add_archive),
                              "suggested-action");
  g_signal_connect(add_archive, "clicked", G_CALLBACK(on_add_archive_clicked),
                   &state);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(header), add_archive);

  GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width(GTK_CONTAINER(body), 18);
  gtk_container_add(GTK_CONTAINER(state.window), body);
  GtkWidget *description = gtk_label_new(
      "Add a folder containing startup.tjs, or select an XP3 archive. "
      "Double-click a recent game to run it.");
  gtk_label_set_xalign(GTK_LABEL(description), 0.0f);
  gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
  gtk_box_pack_start(GTK_BOX(body), description, FALSE, FALSE, 0);

  GtkWidget *scroller = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand(scroller, TRUE);
  gtk_box_pack_start(GTK_BOX(body), scroller, TRUE, TRUE, 0);
  state.game_list = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(state.game_list),
                                  GTK_SELECTION_NONE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(state.game_list), FALSE);
  g_signal_connect(state.game_list, "row-activated",
                   G_CALLBACK(on_row_activated), &state);
  gtk_container_add(GTK_CONTAINER(scroller), state.game_list);

  state.status = gtk_label_new(load_error.empty() ? "Ready." : load_error.c_str());
  gtk_label_set_xalign(GTK_LABEL(state.status), 0.0f);
  gtk_label_set_ellipsize(GTK_LABEL(state.status), PANGO_ELLIPSIZE_END);
  gtk_style_context_add_class(gtk_widget_get_style_context(state.status), "status");
  gtk_box_pack_start(GTK_BOX(body), state.status, FALSE, FALSE, 0);

  GtkTargetEntry targets[] = {
      {const_cast<gchar *>("text/uri-list"), 0, 0},
  };
  gtk_drag_dest_set(state.window, GTK_DEST_DEFAULT_ALL, targets, 1,
                    GDK_ACTION_COPY);
  g_signal_connect(state.window, "drag-data-received",
                   G_CALLBACK(on_drag_data_received), &state);

  refresh_game_list(&state);
  gtk_widget_show_all(state.window);
  if (!options.game.empty()) g_idle_add(auto_start_game, &state);
  gtk_main();
  return 0;
}

} // namespace krkr2

#else

namespace krkr2 {
int run_graphical_launcher(const HostOptions &) {
  return kGraphicalLauncherUnavailable;
}
} // namespace krkr2

#endif
