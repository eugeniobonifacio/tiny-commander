/**
 * Tiny Commander - Un gestore file a doppio pannello essenziale, ispirato a Midnight Commander
 * Compilazione: gcc -o tyc tyc.c -lncurses
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <dirent.h>
 #include <sys/stat.h>
 #include <sys/types.h>
 #include <fcntl.h>
 #include <time.h>
 #include <ncurses.h>
 #include <signal.h>
 #include <locale.h>
 #include <pwd.h>
 #include <grp.h>
 
 #define MAX_PATH_LEN 1024
 #define MAX_FILENAME_LEN 256
 #define MAX_COMMAND_LEN 1024
 #define MAX_FILES 1000
 
 #ifndef GIT_VERSION
 #define GIT_VERSION "v0.0.0-dev"
 #endif

 // Struttura per rappresentare un file
 typedef struct {
     char name[MAX_FILENAME_LEN];
     off_t size;
     mode_t mode;
     time_t mtime;
     int is_dir;
 } FileEntry;
 
 // Struttura per rappresentare un pannello
 typedef struct {
     char current_path[MAX_PATH_LEN];
     FileEntry files[MAX_FILES];
     int num_files;
     int selected;
     int scroll_pos;
     int sort_by; // 0 = nome, 1 = dimensione, 2 = data
     int sort_order; // 0 = asc, 1 = desc
 } Panel;
 
 // Variabili globali
 Panel left_panel, right_panel;
 Panel *active_panel;
 int term_rows, term_cols;
 
 // Prototipi di funzione
 void init_panels();
 void read_directory(Panel *panel);
 void draw_interface();
 void draw_panel(Panel *panel, int x, int y, int width, int height);
 void handle_input();
 void execute_command(const char *command);
 void copy_file(const char *src, const char *dst);
 void move_file(const char *src, const char *dst);
 void delete_file(const char *path);
 void view_file(const char *path);
 void edit_file(const char *path);
 void open_shell();
 int compare_files(const void *a, const void *b, Panel *panel);
 void sort_files(Panel *panel);
 void change_directory(Panel *panel, const char *path);
 char *get_file_permissions(mode_t mode);
 void display_error(const char *message);
 void cleanup();
 
 // Funzione per inizializzare l'interfaccia ncurses
 void init_ncurses() {
     setlocale(LC_ALL, "");
     initscr();
     start_color();
     cbreak();
     noecho();
     keypad(stdscr, TRUE);
     curs_set(0);
     
     // Definizione delle coppie di colori
     init_pair(1, COLOR_WHITE, COLOR_BLUE);    // Pannelli
     init_pair(2, COLOR_BLACK, COLOR_CYAN);    // Barra di stato
     init_pair(3, COLOR_YELLOW, COLOR_BLUE);   // Directory
     init_pair(4, COLOR_GREEN, COLOR_BLUE);    // File eseguibili
     init_pair(5, COLOR_WHITE, COLOR_RED);     // Messaggi di errore
     init_pair(6, COLOR_BLACK, COLOR_WHITE);   // File selezionato
     
     // Ottieni dimensioni del terminale
     getmaxyx(stdscr, term_rows, term_cols);
     
     // Imposta gestore del segnale per il resize del terminale
     signal(SIGWINCH, (void (*)(int))cleanup);
 }
 
 // Funzione main
 int main() {
     init_ncurses();
     init_panels();
     
     // Leggi directory iniziale
     read_directory(&left_panel);
     read_directory(&right_panel);
     
     // Loop principale
     while (1) {
         draw_interface();
         handle_input();
     }
     
     cleanup();
     return 0;
 }
 
 // Inizializza i pannelli
 void init_panels() {
     getcwd(left_panel.current_path, MAX_PATH_LEN);
     strcpy(right_panel.current_path, left_panel.current_path);
     
     left_panel.selected = 0;
     left_panel.scroll_pos = 0;
     left_panel.num_files = 0;
     left_panel.sort_by = 0;
     left_panel.sort_order = 0;
     
     right_panel.selected = 0;
     right_panel.scroll_pos = 0;
     right_panel.num_files = 0;
     right_panel.sort_by = 0;
     right_panel.sort_order = 0;
     
     active_panel = &left_panel;
 }
 
 // Legge il contenuto di una directory
 void read_directory(Panel *panel) {
     DIR *dir;
     struct dirent *entry;
     struct stat st;
     char full_path[MAX_PATH_LEN];
     
     panel->num_files = 0;
     
     // Aggiungi solo l'entry per la directory padre ".."
     strcpy(panel->files[panel->num_files].name, "..");
     panel->files[panel->num_files].is_dir = 1;
     panel->num_files++;
     
     if ((dir = opendir(panel->current_path)) == NULL) {
         display_error("Impossibile aprire la directory");
         return;
     }
     
     while ((entry = readdir(dir)) != NULL && panel->num_files < MAX_FILES) {
         // Salta le entries "." e ".." perché abbiamo già aggiunto ".."
         // e non vogliamo visualizzare "."
         if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
             continue;
         
         snprintf(full_path, MAX_PATH_LEN, "%s/%s", panel->current_path, entry->d_name);
         
         if (stat(full_path, &st) == -1)
             continue;
         
         strncpy(panel->files[panel->num_files].name, entry->d_name, MAX_FILENAME_LEN - 1);
         panel->files[panel->num_files].name[MAX_FILENAME_LEN - 1] = '\0';
         panel->files[panel->num_files].size = st.st_size;
         panel->files[panel->num_files].mode = st.st_mode;
         panel->files[panel->num_files].mtime = st.st_mtime;
         panel->files[panel->num_files].is_dir = S_ISDIR(st.st_mode);
         
         panel->num_files++;
     }
     
     closedir(dir);
     
     // Ordina i file
     sort_files(panel);
 }
 
 // Confronta due file per l'ordinamento
 int file_compare(const void *a, const void *b) {
     FileEntry *fa = (FileEntry *)a;
     FileEntry *fb = (FileEntry *)b;
     
     // Mantieni sempre ".." in cima
     if (strcmp(fa->name, "..") == 0) return -1;
     if (strcmp(fb->name, "..") == 0) return 1;
     
     // Confronta in base al criterio di ordinamento attuale
     if (active_panel->sort_by == 0) { // Nome
         if (fa->is_dir && !fb->is_dir) return -1;
         if (!fa->is_dir && fb->is_dir) return 1;
         return active_panel->sort_order ? 
                -strcasecmp(fa->name, fb->name) : 
                strcasecmp(fa->name, fb->name);
     } else if (active_panel->sort_by == 1) { // Dimensione
         if (fa->is_dir && !fb->is_dir) return -1;
         if (!fa->is_dir && fb->is_dir) return 1;
         return active_panel->sort_order ? 
                (fb->size - fa->size) : 
                (fa->size - fb->size);
     } else { // Data
         if (fa->is_dir && !fb->is_dir) return -1;
         if (!fa->is_dir && fb->is_dir) return 1;
         return active_panel->sort_order ? 
                (fb->mtime - fa->mtime) : 
                (fa->mtime - fb->mtime);
     }
 }
 
 // Ordina i file
 void sort_files(Panel *panel) {
     // Non ordiniamo il primo elemento ("..")
     qsort(panel->files + 1, panel->num_files - 1, sizeof(FileEntry), file_compare);
 }
 
 // Disegna l'interfaccia utente
 void draw_interface() {
     clear();
     
     int panel_width = term_cols / 2;
     int panel_height = term_rows - 4; // Lascia spazio per l'intestazione e la barra di comando
     
     // Disegna intestazione
     attron(COLOR_PAIR(2));
     mvhline(0, 0, ' ', term_cols);
     mvprintw(0, 1, "Tiny Commander %s - Eugenio Bonifacio", GIT_VERSION);
     attroff(COLOR_PAIR(2));
     
     // Disegna pannelli
     draw_panel(&left_panel, 0, 1, panel_width, panel_height);
     draw_panel(&right_panel, panel_width, 1, panel_width, panel_height);
     
     // Disegna barra di comando
     attron(COLOR_PAIR(2));
     mvhline(term_rows - 3, 0, ' ', term_cols);
     mvprintw(term_rows - 3, 1, "F1-Aiuto F2-Menu F3-Vedi F4-Edit F5-Copia F6-Sposta F7-Mkdir F8-Elimina F9-Shell F10-Esci");
     attroff(COLOR_PAIR(2));
     
     // Disegna linea di stato
     mvhline(term_rows - 2, 0, ' ', term_cols);
     mvprintw(term_rows - 2, 1, "Current: %s", active_panel->current_path);
     
     // Disegna linea di comando
     mvhline(term_rows - 1, 0, ' ', term_cols);
     mvprintw(term_rows - 1, 0, "> ");
     
     refresh();
 }
 
 // Disegna un pannello
 void draw_panel(Panel *panel, int x, int y, int width, int height) {
     int i;
     int max_display = height;
     char size_str[20];
     char date_str[20];
     char perm_str[11];
     struct tm *tm_info;
     
     // Se il pannello è attivo, usa un colore diverso
     if (panel == active_panel) {
         attron(A_BOLD);
     }
     
     // Disegna intestazione del pannello
     attron(COLOR_PAIR(1));
     mvhline(y, x, ' ', width);
     mvprintw(y, x + 2, "%s", panel->current_path);
     attroff(COLOR_PAIR(1));
     
     // Regola scroll_pos se necessario
     if (panel->selected < panel->scroll_pos) {
         panel->scroll_pos = panel->selected;
     } else if (panel->selected >= panel->scroll_pos + max_display) {
         panel->scroll_pos = panel->selected - max_display + 1;
     }
     
     // Disegna file
     for (i = 0; i < max_display && i + panel->scroll_pos < panel->num_files; i++) {
         FileEntry *file = &panel->files[i + panel->scroll_pos];
         
         // Prepara stringa dimensione
         if (file->is_dir) {
             strcpy(size_str, "<DIR>");
         } else {
             if (file->size < 1024) {
                 sprintf(size_str, "%5ldB", file->size);
             } else if (file->size < 1024 * 1024) {
                 sprintf(size_str, "%5ldK", file->size / 1024);
             } else {
                 sprintf(size_str, "%5ldM", file->size / (1024 * 1024));
             }
         }
         
         // Prepara stringa data
         tm_info = localtime(&file->mtime);
         strftime(date_str, 20, "%Y-%m-%d %H:%M", tm_info);
         
         // Prepara stringa permessi
         char *perm = get_file_permissions(file->mode);
         strcpy(perm_str, perm);
         free(perm);
         
         // Colore in base al tipo di file
         if (i + panel->scroll_pos == panel->selected) {
             attron(COLOR_PAIR(6)); // File selezionato
         } else if (file->is_dir) {
             attron(COLOR_PAIR(3)); // Directory
         } else if (file->mode & S_IXUSR) {
             attron(COLOR_PAIR(4)); // File eseguibile
         } else {
             attron(COLOR_PAIR(1)); // File normale
         }
         
         mvhline(y + i + 1, x, ' ', width);
         mvprintw(y + i + 1, x + 1, "%-20s %10s %s %s", 
                  file->name, size_str, date_str, perm_str);
         
         if (i + panel->scroll_pos == panel->selected) {
             attroff(COLOR_PAIR(6));
         } else if (file->is_dir) {
             attroff(COLOR_PAIR(3));
         } else if (file->mode & S_IXUSR) {
             attroff(COLOR_PAIR(4));
         } else {
             attroff(COLOR_PAIR(1));
         }
     }
     
     // Riempi il resto del pannello con spazi vuoti
     attron(COLOR_PAIR(1));
     for (; i < max_display; i++) {
         mvhline(y + i + 1, x, ' ', width);
     }
     attroff(COLOR_PAIR(1));
     
     if (panel == active_panel) {
         attroff(A_BOLD);
     }
 }
 
 // Ottieni stringa di permessi in formato Unix
 char *get_file_permissions(mode_t mode) {
     char *perms = malloc(11);
     if (!perms) return NULL;
     
     perms[0] = (S_ISDIR(mode)) ? 'd' : '-';
     perms[1] = (mode & S_IRUSR) ? 'r' : '-';
     perms[2] = (mode & S_IWUSR) ? 'w' : '-';
     perms[3] = (mode & S_IXUSR) ? 'x' : '-';
     perms[4] = (mode & S_IRGRP) ? 'r' : '-';
     perms[5] = (mode & S_IWGRP) ? 'w' : '-';
     perms[6] = (mode & S_IXGRP) ? 'x' : '-';
     perms[7] = (mode & S_IROTH) ? 'r' : '-';
     perms[8] = (mode & S_IWOTH) ? 'w' : '-';
     perms[9] = (mode & S_IXOTH) ? 'x' : '-';
     perms[10] = '\0';
     
     return perms;
 }
 
 // Cambia directory
 void change_directory(Panel *panel, const char *path) {
     char new_path[MAX_PATH_LEN];
     
     // Gestione percorsi relativi e assoluti
     if (path[0] == '/') {
         strncpy(new_path, path, MAX_PATH_LEN - 1);
     } else {
         snprintf(new_path, MAX_PATH_LEN, "%s/%s", panel->current_path, path);
     }
     
     // Normalizza il percorso
     char *real_path = realpath(new_path, NULL);
     if (real_path) {
         strcpy(panel->current_path, real_path);
         free(real_path);
         panel->selected = 0;
         panel->scroll_pos = 0;
         read_directory(panel);
     } else {
         display_error("Directory non accessibile");
     }
 }
 
 // Gestisce l'input utente
 void handle_input() {
     int ch = getch();
     FileEntry *selected_file;
     Panel *inactive_panel;
     char full_path[MAX_PATH_LEN];
     char target_path[MAX_PATH_LEN];
     
     switch(ch) {
         case KEY_UP:
             if (active_panel->selected > 0) {
                 active_panel->selected--;
             }
             break;
             
         case KEY_DOWN:
             if (active_panel->selected < active_panel->num_files - 1) {
                 active_panel->selected++;
             }
             break;
             
         case KEY_LEFT:
         case '\t':
             // Cambia pannello attivo
             active_panel = (active_panel == &left_panel) ? &right_panel : &left_panel;
             break;
             
         case '\n': // Enter
             selected_file = &active_panel->files[active_panel->selected];
             if (selected_file->is_dir) {
                 change_directory(active_panel, selected_file->name);
             }
             break;
             
         case KEY_F(3): // View
             selected_file = &active_panel->files[active_panel->selected];
             if (!selected_file->is_dir) {
                 snprintf(full_path, MAX_PATH_LEN, "%s/%s", 
                          active_panel->current_path, selected_file->name);
                 view_file(full_path);
             }
             break;
             
         case KEY_F(4): // Edit
             selected_file = &active_panel->files[active_panel->selected];
             if (!selected_file->is_dir) {
                 snprintf(full_path, MAX_PATH_LEN, "%s/%s", 
                          active_panel->current_path, selected_file->name);
                 edit_file(full_path);
             }
             break;
             
         case KEY_F(5): // Copy
             selected_file = &active_panel->files[active_panel->selected];
             inactive_panel = (active_panel == &left_panel) ? &right_panel : &left_panel;
             
             snprintf(full_path, MAX_PATH_LEN, "%s/%s", 
                      active_panel->current_path, selected_file->name);
             snprintf(target_path, MAX_PATH_LEN, "%s/%s", 
                      inactive_panel->current_path, selected_file->name);
             
             copy_file(full_path, target_path);
             read_directory(inactive_panel);
             break;
             
         case KEY_F(6): // Move
             selected_file = &active_panel->files[active_panel->selected];
             inactive_panel = (active_panel == &left_panel) ? &right_panel : &left_panel;
             
             snprintf(full_path, MAX_PATH_LEN, "%s/%s", 
                      active_panel->current_path, selected_file->name);
             snprintf(target_path, MAX_PATH_LEN, "%s/%s", 
                      inactive_panel->current_path, selected_file->name);
             
             move_file(full_path, target_path);
             read_directory(active_panel);
             read_directory(inactive_panel);
             break;
             
         case KEY_F(8): // Delete
             selected_file = &active_panel->files[active_panel->selected];
             
             // Non eliminiamo ".."
             if (strcmp(selected_file->name, "..") == 0)
                 break;
                 
             snprintf(full_path, MAX_PATH_LEN, "%s/%s", 
                      active_panel->current_path, selected_file->name);
             
             delete_file(full_path);
             read_directory(active_panel);
             break;
             
         case KEY_F(9): // Shell
             open_shell();
             break;
             
         case KEY_F(7): // Mkdir
             // TODO: implementare creazione directory
             break;
             
         case KEY_F(1): // Help
             // TODO: mostra aiuto
             break;
             
         case KEY_F(2): // Menu
             // TODO: mostra menu
             break;
             
         case 's': // Cambia ordinamento (nome, dimensione, data)
             active_panel->sort_by = (active_panel->sort_by + 1) % 3;
             sort_files(active_panel);
             break;
             
         case 'r': // Inverte ordine
             active_panel->sort_order = !active_panel->sort_order;
             sort_files(active_panel);
             break;
         
         case 'q':
         case 'Q':  
         case KEY_F(10):  // F10
             cleanup();
             exit(0);
             break;
     }
 }
 
 // Visualizza un file usando il visualizzatore configurato
 void view_file(const char *path) {
     char command[MAX_COMMAND_LEN];
     
     // Ottieni il visualizzatore dalla variabile d'ambiente o usa 'less' di default
     char *viewer = getenv("PAGER");
     if (!viewer) viewer = "less";
     
     // Costruisci il comando
     snprintf(command, MAX_COMMAND_LEN, "%s \"%s\"", viewer, path);
     
     // Salva lo stato del terminale e ripristina modalità canonica
     def_prog_mode();
     endwin();
     
     // Esegui il comando
     system(command);
     
     // Ripristina lo stato del terminale
     reset_prog_mode();
     refresh();
 }
 
 // Modifica un file usando l'editor configurato
 void edit_file(const char *path) {
     char command[MAX_COMMAND_LEN];
     
     // Ottieni l'editor dalla variabile d'ambiente o usa 'vi' di default
     char *editor = getenv("EDITOR");
     if (!editor) editor = "vi";
     
     // Costruisci il comando
     snprintf(command, MAX_COMMAND_LEN, "%s \"%s\"", editor, path);
     
     // Salva lo stato del terminale e ripristina modalità canonica
     def_prog_mode();
     endwin();
     
     // Esegui il comando
     system(command);
     
     // Ripristina lo stato del terminale
     reset_prog_mode();
     refresh();
 }
 
 // Copia un file
 void copy_file(const char *src, const char *dst) {
     FILE *src_file, *dst_file;
     char buffer[4096];
     size_t bytes_read;
     
     // Non copiare ".."
     if (strcmp(src, "..") == 0)
         return;
     
     src_file = fopen(src, "rb");
     if (!src_file) {
         display_error("Impossibile aprire il file sorgente");
         return;
     }
     
     dst_file = fopen(dst, "wb");
     if (!dst_file) {
         fclose(src_file);
         display_error("Impossibile creare il file destinazione");
         return;
     }
     
     while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
         fwrite(buffer, 1, bytes_read, dst_file);
     }
     
     fclose(src_file);
     fclose(dst_file);
     
     // Copia anche i permessi
     struct stat st;
     if (stat(src, &st) == 0) {
         chmod(dst, st.st_mode);
     }
 }
 
 // Sposta un file
 void move_file(const char *src, const char *dst) {
     // Prova a rinominare (funziona solo se src e dst sono sullo stesso filesystem)
     if (rename(src, dst) == 0) {
         return;
     }
     
     // Se rename fallisce, copia e poi elimina
     copy_file(src, dst);
     delete_file(src);
 }
 
 // Elimina un file o directory
 void delete_file(const char *path) {
     struct stat st;
     
     if (stat(path, &st) != 0) {
         display_error("File non trovato");
         return;
     }
     
     if (S_ISDIR(st.st_mode)) {
         // È una directory, elimina ricorsivamente
         // Nota: per semplicità, questa implementazione non è ricorsiva
         // Usa rmdir solo per directory vuote
         if (rmdir(path) != 0) {
             display_error("Impossibile eliminare la directory (potrebbe non essere vuota)");
         }
     } else {
         // È un file normale
         if (unlink(path) != 0) {
             display_error("Impossibile eliminare il file");
         }
     }
 }
 
 // Apre una shell
 void open_shell() {
     char *shell = getenv("SHELL");
     if (!shell) shell = "/bin/sh";
     
     // Salva lo stato del terminale e ripristina modalità canonica
     def_prog_mode();
     endwin();
     
     // Mostra un prompt
     printf("Avvio della shell. Digita 'exit' per tornare a Tiny Commander.\n");
     
     // Esegui la shell
     system(shell);
     
     // Aspetta che l'utente prema un tasto
     printf("Premi un tasto per tornare a Tiny Commander...");
     getchar();
     
     // Ripristina lo stato del terminale
     reset_prog_mode();
     refresh();
 }
 
 // Mostra un messaggio di errore
 void display_error(const char *message) {
     attron(COLOR_PAIR(5));
     mvhline(term_rows - 1, 0, ' ', term_cols);
     mvprintw(term_rows - 1, 0, "Errore: %s", message);
     attroff(COLOR_PAIR(5));
     refresh();
     getch(); // Aspetta che l'utente prema un tasto
 }
 
 // Pulisce e chiude ncurses
 void cleanup() {
     endwin();
     printf("Grazie per aver usato Tiny Commander!\n");
 }