#ifndef __FILEPICKER_H
#define __FILEPICKER_H

#include <dirent.h>

#ifndef FILEPICKER_PATH_MAX
#define FILEPICKER_PATH_MAX 1024
#endif

#ifndef FILEPICKER_FILES_MAX
#define FILEPICKER_FILES_MAX 1024
#endif

enum { PICKER_NONE, PICKER_FIND, PICKER_EXEC };
struct filepicker_entry { char is_dir, *name; };
struct filepicker {
    char path[FILEPICKER_PATH_MAX];
    struct filepicker_entry files[FILEPICKER_FILES_MAX];
    int num_files, cur, off, ww, wh, mode;
    struct inputbox input;
};

int picker_scan(struct filepicker *fp, char *path);
void picker_update(struct filepicker *fp, int ch);
void picker_render(struct filepicker *fp);
void picker_reset(struct filepicker *fp);

#ifdef FILEPICKER_IMPL

#include <stdlib.h>
#include "led.h"
#include "config.h"

static inline int _picker_filter_dirs(const struct dirent *ent) {
    return ent->d_type == DT_DIR;
}

static inline int _picker_filter_files(const struct dirent *ent) {
    return ent->d_type == DT_REG;
}

int picker_scan(struct filepicker *fp, char *path) {
    picker_reset(fp);
    if (path) strcpy(fp->path, path);
    struct dirent **dirs, **files;
    int num_dirs, num_files;
    num_dirs = scandir(fp->path, &dirs, _picker_filter_dirs, alphasort);
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
    fp->cur += dir;
    if (fp->cur < 0) fp->cur = 0;
    if (fp->cur >= fp->num_files) fp->cur = fp->num_files-1;
    if (fp->cur < fp->off) --fp->off;
    if (fp->wh != 0 && fp->cur-fp->off >= fp->wh-1) ++fp->off;
}

static void _picker_find_next(struct filepicker *fp, char *name) {
    int cur = fp->cur, off = fp->off, pos;
    for (pos = cur+1; pos < fp->num_files; ++pos)
        if (strcasestr(fp->files[pos].name, name)) goto jmp;
    for (pos = 0; pos < cur; ++pos)
        if (strcasestr(fp->files[pos].name, name)) goto jmp;
    fp->cur = cur, fp->off = off;
    return;
jmp:
    for (fp->cur = fp->off = 0; fp->cur < pos; _picker_move(fp, 1)){}
}

static void _picker_find(struct filepicker *fp) {
    _picker_find_next(fp, fp->input.text);
}

static void _picker_exec(struct filepicker *fp) {
    char cmd[4096], cur[PATH_MAX];
    strcpy(cur, fp->files[fp->cur].name);
    snprintf(cmd, sizeof(cmd), "cd %s && %s", fp->path, fp->input.text);
    system(cmd);
    picker_scan(fp, NULL);
    _picker_find_next(fp, cur);
}

static void _picker_update_mode(struct filepicker *fp, int ch, void (*fn)(struct filepicker*)) {
    if (ch == CTRL('q') || ch == CTRL('c')) {
        fp->mode = PICKER_NONE;
        return;
    }
    if (ch == '\n' && fp->input.text_sz) {
        fn(fp);
        fp->mode = PICKER_NONE;
    } else input_update(&fp->input, ch);
}

void picker_update(struct filepicker *fp, int ch) {
    switch (fp->mode) {
    case PICKER_FIND: _picker_update_mode(fp, ch, &_picker_find); return;
    case PICKER_EXEC: _picker_update_mode(fp, ch, &_picker_exec); return;
    default: break;
    }
    switch (ch) {
    case KEY_UP:
        _picker_move(fp, -1);
        break;
    case KEY_DOWN:
        _picker_move(fp, 1);
        break;
    case KEY_PPAGE:
        for (int i = 0; i < fp->wh-1; ++i) _picker_move(fp, -1);
        break;
    case KEY_NPAGE:
        for (int i = 0; i < fp->wh-1; ++i) _picker_move(fp, 1);
        break;
    case KEY_HOME:
        fp->cur = fp->off = 0;
        break;
    case KEY_END:
        for (fp->cur = fp->off = 0; fp->cur+1 < fp->num_files; ) _picker_move(fp, 1);
        break;
    case CTRL('f'):
        input_reset(&fp->input);
        fp->mode = PICKER_FIND;
        break;
    case CTRL('e'): /* idk if strcmp is the best idea but it works for now so whatevs */
        if (strcmp(fp->path, "*BUFFERS*") != 0) {
            input_reset(&fp->input);
            fp->mode = PICKER_EXEC;
        }
        break;
    case CTRL('n'): case 'n':
        if (fp->input.text_sz) _picker_find(fp);
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

void picker_reset(struct filepicker *fp) {
    if (fp->num_files) {
        for (int i = 0; i < fp->num_files; ++i)
            free(fp->files[i].name);
    }
    fp->cur = fp->off = fp->num_files = fp->mode = 0;
    input_reset(&fp->input);
}

#endif

#endif
