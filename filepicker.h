#include <dirent.h>
#include <stdlib.h>
#include "led.h"
#include "config.h"

#ifndef FILEPICKER_PATH_MAX
#define FILEPICKER_PATH_MAX 1024
#endif

#ifndef FILEPICKER_FILES_MAX
#define FILEPICKER_FILES_MAX 1024
#endif

enum { PICKER_NONE = 0, PICKER_FIND, PICKER_MOVE, PICKER_COPY, PICKER_DELETE };
struct filepicker_entry { char is_dir, *name; };
struct filepicker {
    char path[FILEPICKER_PATH_MAX];
    struct filepicker_entry files[FILEPICKER_FILES_MAX];
    int num_files, cur, off, ww, wh, mode;
    struct inputbox input;
};

static inline int _picker_filter_dirs(const struct dirent *ent) {
    return ent->d_type == DT_DIR;
}

static inline int _picker_filter_files(const struct dirent *ent) {
    return ent->d_type == DT_REG;
}

void picker_reset(struct filepicker *fp) {
    if (fp->num_files) {
        for (int i = 0; i < fp->num_files; ++i)
            free(fp->files[i].name);
    }
    fp->cur = fp->off = fp->num_files = fp->mode = 0;
    input_reset(&fp->input);
}

int picker_scan(struct filepicker *fp, char *path) {
    picker_reset(fp);
    if (path) strcpy(fp->path, path);
    struct dirent **dirs, **files;
    int num_dirs = scandir(fp->path, &dirs, _picker_filter_dirs, alphasort),
        num_files = scandir(fp->path, &files, _picker_filter_files, alphasort);
    for (int i = 0; i < num_dirs; ++i) {
        if (strcmp(dirs[i]->d_name, ".") == 0) goto f;
        if (fp->num_files < FILEPICKER_FILES_MAX) {
            fp->files[fp->num_files++] = (struct filepicker_entry) {
                .name = strdup(dirs[i]->d_name),
                .is_dir = 1,
            };
        }
f:      free(dirs[i]);
    }
    for (int i = 0; i < num_files; ++i) {
        if (fp->num_files < FILEPICKER_FILES_MAX) {
            fp->files[fp->num_files++] = (struct filepicker_entry) {
                .name = strdup(files[i]->d_name),
                .is_dir = 0,
            };
        }
        free(files[i]);
    }
    if (dirs) free(dirs);
    if (files) free(files);
    return fp->num_files;
}

static void _picker_move(struct filepicker *fp, int dir) {
    int c = fp->cur + dir, h = fp->wh-2;
    fp->cur = c<0? 0 : c>=fp->num_files? fp->num_files-1 : c;
    c=fp->cur, fp->off = c<fp->off? c : c-fp->off>=h? c-h : fp->off;
}

static void _picker_find_next(struct filepicker *fp, char *name) {
    int cur = fp->cur, off = fp->off, pos;
    for (pos = cur+1; pos < fp->num_files; ++pos)
        if (strcasestr(fp->files[pos].name, name)) goto j;
    for (pos = 0; pos < cur; ++pos)
        if (strcasestr(fp->files[pos].name, name)) goto j;
    fp->cur = cur, fp->off = off;
    return;
j:  fp->cur = fp->off = 0;
    _picker_move(fp, pos);
}

static void _picker_exec(struct filepicker *fp) {
    if (fp->mode == PICKER_FIND) {
        _picker_find_next(fp, fp->input.text);
        return;
    }
    char cmd[4096] = {0}, new[INPUTBOX_TEXT_SIZE] = {0};
    int cur = fp->cur;
    strcpy(new, fp->input.text);
    if (fp->mode != PICKER_DELETE) {
        snprintf(cmd, sizeof(cmd), "%s \"%s/%s\" \"%s/%s\"",
            fp->mode == PICKER_MOVE? "mv" : "cp", fp->path,
            fp->files[fp->cur].name, fp->path, new);
    } else snprintf(cmd, sizeof(cmd), "rm -rf \"%s/%s\"", fp->path, new);
    system(cmd);
    picker_scan(fp, NULL);
    if (fp->mode == PICKER_DELETE) _picker_move(fp, cur);
    else _picker_find_next(fp, new);
}

static void _picker_mode(struct filepicker *fp, int mode) {
    input_reset(&fp->input);
    if ((fp->mode = mode) > PICKER_FIND) {
        strcpy(fp->input.text, fp->files[fp->cur].name);
        fp->input.pos = fp->input.text_sz = strlen(fp->files[fp->cur].name);
    }
}

void picker_update(struct filepicker *fp, int ch) {
    if (fp->mode) {
        switch (ch) {
        case '\n': if (!fp->input.text_sz) break; _picker_exec(fp); /* FALLTHROUGH */
        case CTRL('q'): case CTRL('c'): fp->mode = PICKER_NONE; break;
        default: input_update(&fp->input, ch); break;
        }
        return;
    }
    switch (ch) {
    case KEY_UP:    _picker_move(fp, -1); break;
    case KEY_DOWN:  _picker_move(fp, +1); break;
    case KEY_PPAGE: _picker_move(fp, -(fp->wh-2)); break;
    case KEY_NPAGE: _picker_move(fp, +(fp->wh-2)); break;
    case KEY_END:   _picker_move(fp, +fp->num_files); break;
    case KEY_HOME:  _picker_move(fp, -fp->num_files); break;
    case 'm': _picker_mode(fp, PICKER_MOVE); break;
    case 'c': _picker_mode(fp, PICKER_COPY); break;
    case 'd': _picker_mode(fp, PICKER_DELETE); break;
    case CTRL('f'): case 'f': case '/': _picker_mode(fp, PICKER_FIND); break;
    case CTRL('n'): case 'n':
        if (fp->input.text_sz) _picker_find_next(fp, fp->input.text);
        break;
    }
}

void picker_render(struct filepicker *fp) {
    getmaxyx(stdscr, fp->wh, fp->ww);
    for (int i = fp->off; i < fp->off+fp->wh; ++i) {
        if (i >= fp->num_files) break;
        const int attr = (i == fp->cur)? CFG_ATTRSELECT : 0;
        struct filepicker_entry ent = fp->files[i];
        attron(attr);
        char *ent_name = get_filename(ent.name, CFG_PICKERPATH);
        mvprintw(i-fp->off, 0, "%.*s%s", fp->ww, ent_name, ent.is_dir? "/" : "");
        free(ent_name);
        attroff(attr);
    }
}
