#ifndef __FILEPICKER_H
#define __FILEPICKER_H

#include <dirent.h>

#ifndef FILEPICKER_PATH_MAX
#define FILEPICKER_PATH_MAX 1024
#endif

struct filepicker {
    char path[FILEPICKER_PATH_MAX];
    struct dirent **files;
    int num_files, cur, off, x, y, w, h;
};

void picker_scan(struct filepicker *fp, char *path);
int picker_find(struct filepicker *fp, char *name);
void picker_update(struct filepicker *fp, int ch);
void picker_render(struct filepicker *fp, int x, int y, int w, int h, int status);
void picker_reset(struct filepicker *fp);

#ifdef FILEPICKER_IMPL

#include <stdlib.h>

void picker_scan(struct filepicker *fp, char *path) {
    picker_reset(fp);
    strcpy(fp->path, path);
    fp->num_files = scandir(path, &fp->files, NULL, alphasort);
}

int picker_find(struct filepicker *fp, char *name) {
    for (int i = fp->cur+1; i < fp->num_files; ++i) {
        if (strcasestr(fp->files[i]->d_name, name))
            return i;
    }
    for (int i = 0; i < fp->cur; ++i) {
        if (strcasestr(fp->files[i]->d_name, name))
            return i;
    }

    return -1;
}

static void _picker_move(struct filepicker *fp, int dir) {
    fp->cur += dir;
    if (fp->cur < 0) fp->cur = 0;
    if (fp->cur >= fp->num_files) fp->cur = fp->num_files-1;
    if (fp->cur < fp->off) --fp->off;
    if (fp->h != 0 && fp->cur-fp->off >= fp->h) ++fp->off;
}

void picker_update(struct filepicker *fp, int ch) {
    switch (ch) {
    case KEY_UP:
        _picker_move(fp, -1);
        break;
    case KEY_DOWN:
        _picker_move(fp, 1);
        break;
    case KEY_PPAGE:
        for (int i = 0; i < fp->h-1; ++i) _picker_move(fp, -1);
        break;
    case KEY_NPAGE:
        for (int i = 0; i < fp->h-1; ++i) _picker_move(fp, 1);
        break;
    case KEY_HOME:
        fp->cur = fp->off = 0;
        break;
    case KEY_END:
        fp->cur = fp->num_files-1;
        fp->off = fp->num_files-fp->h;
        break;
    }
}

void picker_render(struct filepicker *fp, int x, int y, int w, int h, int status) {
    fp->x = x, fp->y = y, fp->w = w, fp->h = h;
    for (int i = fp->off; i < fp->off+fp->h; ++i) {
        if (i >= fp->num_files) break;
        const int attr = i == fp->cur? A_REVERSE : 0;
        struct dirent *ent = fp->files[i];
        attron(attr);
        mvprintw(y+i-fp->off, x, "%.*s", w, ent->d_name);
        attroff(attr);
    }
    if (status) mvprintw(y+h, x, " %d:%d %s ", fp->cur+1, fp->num_files, fp->path);
}

void picker_reset(struct filepicker *fp) {
    if (fp->files) {
        for (; fp->num_files > 0; --fp->num_files)
            free(fp->files[fp->num_files-1]);
        free(fp->files);
        fp->files = NULL;
    }
    fp->cur = fp->off = fp->num_files = 0;
}

#endif

#endif
