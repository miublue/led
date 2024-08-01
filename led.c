#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>

#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))
#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512
#define TAB_WIDTH 4
#define TABS_TO_SPACES TRUE

typedef struct {
	uint32_t start, end;
} line_t;

typedef line_t selection_t;

struct {
	size_t text_sz, text_alloc;
	size_t lines_sz, lines_alloc;
	int cur, sel, off, line, ww, wh;
	char *text, *file;
	line_t *lines;
} led;

void append_line(line_t line);
void count_lines();

void init_curses();
void quit_curses();
void exit_program();
void open_file(char *path);
void write_file(char *path);

void scroll_up();
void scroll_down();
void move_left();
void move_right();
void move_up();
void move_down();
void page_up();
void page_down();
void insert_char(int ch);
void insert_tab();
void remove_char();
void remove_sel();
void copy_sel();

void render_text();
void render_status();

void append_line(line_t line) {
	if (++led.lines_sz >= led.lines_alloc)
		led.lines = realloc(led.lines, sizeof(line_t) * (led.lines_alloc += ALLOC_SIZE));
	led.lines[led.lines_sz-1] = line;
}

void count_lines() {
	size_t line_start = led.lines_sz = 0;
	for (int i = 0; i < led.text_sz; ++i) {
		if (led.text[i] == '\n') {
			append_line((line_t) { line_start, i });
			line_start = i+1;
		}
	}
	if (!led.lines_sz) insert_char('\n');
}

void init_curses() {
	initscr();
	raw();
	noecho();
	curs_set(0);
	keypad(stdscr, TRUE);
}

void quit_curses() {
	endwin();
	curs_set(1);
}

void exit_program() {
	if (led.text) free(led.text);
	if (led.lines) free(led.lines);
	exit(0);
}

void open_file(char *path) {
	if (led.text) free(led.text);
	if (led.lines) free(led.lines);
	led.text = NULL;
	led.lines = malloc(sizeof(line_t) * (led.lines_alloc = ALLOC_SIZE));
	led.cur = led.sel = led.off = led.line = led.text_sz = led.lines_sz = 0;
	led.file = path;

	FILE *file = fopen(path, "r+");
	if (!file) {
		file = fopen(path, "w+");
		if (!file) goto open_file_fail;
	}
	fseek(file, 0, SEEK_END);
	led.text_alloc = 1 + (led.text_sz = ftell(file));
	rewind(file);
	led.text = calloc(1, led.text_alloc);
	if (!led.text) goto open_file_fail;
	if (fread(led.text, 1, led.text_sz, file) != led.text_sz) goto open_file_fail;
	count_lines();
	fclose(file);
	return;

open_file_fail:
	if (file) fclose(file);
	fprintf(stderr, "could not open file: %s\n", path);
	exit_program();
}

void write_file(char *path) {
	FILE *file = fopen(path, "w");
	fwrite(led.text, 1, led.text_sz, file);
	fclose(file);
}

static inline bool is_selecting() {
	return (led.cur != led.sel);
}

static inline selection_t get_selection() {
	return (selection_t) {
		.start = MIN(led.cur, led.sel),
		.end = MAX(led.cur, led.sel),
	};
}

void scroll_up() {
	if (led.line-led.off < 0 && led.off > 0)
		--led.off;
}

void scroll_down() {
	if (led.line-led.off > led.wh-2)
		++led.off;
}

void move_left() {
	if (led.cur == 0) return;
	if (led.text[--led.cur] == '\n')
		if (led.line > 0) --led.line;
	scroll_up();
}

void move_right() {
	if (led.cur >= led.text_sz-1) return;
	if (++led.cur == led.lines[led.line].end+1)
		if (led.line < led.lines_sz) ++led.line;
	scroll_down();
}

void move_up() {
	if (led.line == 0) return;
	--led.line;
	led.cur -= led.lines[led.line].end - led.lines[led.line].start + 1;
	if (led.cur > led.lines[led.line].end) led.cur = led.lines[led.line].end;
	scroll_up();
}

void move_down() {
	if (led.line >= led.lines_sz-1) return;
	led.cur += led.lines[led.line].end - led.lines[led.line].start + 1;
	++led.line;
	if (led.cur > led.lines[led.line].end) led.cur = led.lines[led.line].end;
	scroll_down();
}

void page_up() {
	for (int i = 0; i < led.wh-2; ++i)
		move_up();
}

void page_down() {
	for (int i = 0; i < led.wh-2; ++i)
		move_down();
}

void insert_char(int ch) {
	if (++led.text_sz >= led.text_alloc)
		led.text = realloc(led.text, (led.text_alloc += ALLOC_SIZE));
	memmove(led.text+led.cur+1, led.text+led.cur, led.text_sz-led.cur);
	led.text[led.cur] = ch;
	count_lines();
	move_right();
}

void insert_tab() {
	if (TABS_TO_SPACES) {
		for (int i = 0; i < TAB_WIDTH; ++i)
			insert_char(' ');
	}
	else {
		insert_char('\t');
	}
}

void remove_char() {
	if (led.text_sz == 0 || led.cur >= led.text_sz-1) return;
	memmove(led.text+led.cur, led.text+led.cur+1, led.text_sz-led.cur);
	--led.text_sz;
	count_lines();
}

static void goto_start_of_selection() {
	selection_t sel = get_selection();
	if (led.cur == sel.start) return;
	while (led.cur > sel.start) move_left();
}

void remove_sel() {
	if (!is_selecting()) {
		led.cur = led.lines[led.line].start;
		led.sel = led.lines[led.line].end;
	}
	selection_t sel = get_selection();
	goto_start_of_selection();
	for (int i = sel.start; i <= sel.end; ++i)
		remove_char();
}

void copy_sel() {
	if (!is_selecting()) {
		led.cur = led.lines[led.line].start;
		led.sel = led.lines[led.line].end;
	}
	selection_t sel = get_selection();
	char path[1024] = {0};
	char cmd[2048] = {0};
	sprintf(path, "%s/.ledsel", getenv("HOME"));
	FILE *file = fopen(path, "w");
	fwrite(led.text+sel.start, 1, sel.end-sel.start+1, file);
	fclose(file);
	sprintf(cmd, "cat '%s' | xsel -b", path);
	system(cmd);
}

static inline bool is_selected(int i) {
	selection_t sel = get_selection();
	return (is_selecting() && i >= sel.start && i <= sel.end);
}

static void render_line(int l) {
	line_t line = led.lines[l];
	int sz = 0;
	for (int i = line.start; i <= line.end; ++i) {
		if (led.cur == i || is_selected(i)) attron(A_REVERSE);
		mvprintw(l-led.off, sz, "%c", isprint(led.text[i])? led.text[i] : ' ');
		if (led.text[i] == '\t') {
			if (is_selected(i)) {
				for (int j = 0; j < TAB_WIDTH; ++j)
					mvprintw(l-led.off, sz+j, " ");
			}
			sz += TAB_WIDTH;
		}
		else sz++;
		attroff(A_REVERSE);
	}
}

void render_text() {
	erase();
	for (int i = led.off; i < led.off+led.wh-1; ++i) {
		if (i >= led.lines_sz) break;
		render_line(i);
	}
}

void render_status() {
	char status[ALLOC_SIZE] = {0};
	sprintf(status, " %d %d:%ld %s ",
		led.cur-led.lines[led.line].start+1,
		led.line+1, led.lines_sz, led.file);
	mvprintw(led.wh-1, led.ww-strlen(status), status);
}

// keys you press when selecting text
static inline bool selection_keys(int c) {
	return (c == KEY_SLEFT || c == KEY_SRIGHT || c == KEY_SR || c == KEY_SF
		|| c == KEY_SHOME || c == KEY_SEND || c == KEY_SPREVIOUS || c == KEY_SNEXT);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		return 1;
	}
	open_file(argv[1]);
	init_curses();
	for (;;) {
		getmaxyx(stdscr, led.wh, led.ww);
		render_text();
		render_status();
		int ch = getch();
		switch (ch) {
		case CTRL('q'):
			quit_curses();
			exit_program();
			break;
		case CTRL('s'):
			write_file(led.file);
			break;
		case CTRL('c'):
			copy_sel();
			break;
		case CTRL('x'):
			copy_sel();
			remove_sel();
			break;
		case KEY_SLEFT:
		case KEY_LEFT:
			move_left();
			break;
		case KEY_SRIGHT:
		case KEY_RIGHT:
			move_right();
			break;
		case KEY_SR:
		case KEY_UP:
			move_up();
			break;
		case KEY_SF:
		case KEY_DOWN:
			move_down();
			break;
		case KEY_SHOME:
		case KEY_HOME:
			led.cur = led.lines[led.line].start;
			break;
		case KEY_SEND:
		case KEY_END:
			led.cur = led.lines[led.line].end;
			break;
		case KEY_SPREVIOUS:
		case KEY_PPAGE:
			page_up();
			break;
		case KEY_SNEXT:
		case KEY_NPAGE:
			page_down();
			break;
		case '\n':
			insert_char('\n');
			break;
		case '\t':
			insert_tab();
			break;
		case KEY_DC:
			if (is_selecting()) remove_sel();
			else remove_char();
			break;
		case KEY_BACKSPACE:
			if (is_selecting()) remove_sel();
			else {
                if (led.cur == 0) break;
				move_left();
				remove_char();
			}
			break;
		default:
			if (isprint(ch)) insert_char(ch);
			break;
		}
		if (!selection_keys(ch)) led.sel = led.cur;
	}
	quit_curses();
	return 0;
}
