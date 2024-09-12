#include <iostream>
#ifndef _WIN32
    #include <termios.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
#endif
#include <chrono>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <sstream>
#include <vector>

class Cursor {
    int x = 1, y = 1;
    int background_color = 0;
    int text_color = 255;
    char content = 0;
    bool touched = false;
public:
    void display_cursor() {
        std::cout << "\033[?25h" << std::flush;
    }
    void hide_cursor() {
        std::cout << "\033[?25l" << std::flush;
    }

    void move(int x, int y) {
        this->x = x;
        this->y = y;
    }

    int get_x() const {
        return x;
    }

    int get_y() const {
        return y;
    }

    void set_content(char con) {
        this->content = con;
    }

    char get_content() const {
        return content;
    }

    void set_color(int col) {
        this->text_color = col;
    }

    void set_background(int col) {
        this->background_color = col;
    }

    void set_down() {
        touched = true;
    }

    void set_up() {
        touched = false;
    }

    bool is_down() {
        return touched;
    }

    std::string draw(char ch, int bg = 0, int color = 7) {
        std::stringstream ss;
        if(!touched) return ss.str();
        ss << "\033[" << y << ";" << x << "H"; // move cursor
        ss << "\x1B[38;5;" << color << "m";
        ss << "\x1B[48;5;" << bg << "m";
        ss << ch;
        ss << "\x1B[m" << std::flush; // ansi reset
        return ss.str();
    }
};

struct termios orig_term;

void raw(bool enable) {
    if(enable) {
        tcgetattr(STDIN_FILENO, &orig_term);

        struct termios raw = orig_term;
		raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
		raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON | ICRNL);
		raw.c_oflag &= ~(OPOST);
		raw.c_cflag |= (CS8);
		
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    } else {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
    }
}

void init() {
    std::cout << "\033[?1049h" << std::flush;
    raw(true);
}

void endwin() {
    std::cout << "\033[?1049l" << std::flush; 
    raw(false);
}

void clear() {
    std::cout << "\033[2J" << std::flush;
}

class FrameUnit {
    char char_;
    int color_;
    int bg_color_;
public:
    explicit FrameUnit(char ch, int color, int bg_color)
    : char_(ch) {
        this->set_color(color);
        this->set_bg_color(bg_color);
    }
    FrameUnit(char ch): FrameUnit(ch, 0) {}
    FrameUnit(char ch, int color): FrameUnit(ch, color, 0) {}

    void set_char(char ch) {
        this->char_ = ch;
    }

    void set_color(int color) {
        if(color >= 0 && color <= 255) {
            this->color_ = color;
        } else {
            this->color_ = 0;
        }
    }

    void set_bg_color(int bg_color) {
        if(bg_color >= 0 && bg_color <= 255) {
            this->bg_color_ = bg_color;
        } else {
            this->bg_color_ = 0;
        }
    }

    char get_char() const {
        return char_;
    }
    int get_color() const {
        return color_;
    }
    int get_bg_color() const {
        return bg_color_;
    }
};

class Frame {
    int width = 0, height = 0;
    std::vector<std::vector<FrameUnit*>> buffer;
public:
    explicit Frame(int width, int height) {
        this->width = width;
        this->height = height;
        buffer.resize(height, std::vector<FrameUnit*>(width, nullptr));
    }

    ~Frame() {
        for(const auto& row: buffer) {
            for(auto& unit: row) {
                delete unit;
            }
        }
    }

    int get_width() const {
        return width;
    }

    int get_height() const {
        return height;
    }

    FrameUnit* get_pixel(int x, int y) {
        if(width <= 0 && height <= 0) return nullptr;
        if(x < 0 || y < 0 || x >= width || y >= height) return nullptr; // pixel does not on screen
        if(buffer[y][x] == nullptr) {
            buffer[y][x] = new FrameUnit(0);
        }
        return buffer[y][x];
    }

    void clear() {
        for(auto& row: buffer) {
            for(size_t x = 0; x < row.size(); x++) {
                if(row[x] == nullptr) continue;
                delete row[x];
                row[x] = nullptr;
            }
        }
    }

    std::string render() {
        std::stringstream ss;
        FrameUnit* prev = nullptr;
        int prev_x = 0, prev_y = 0;
        for(size_t j = 0; j < buffer.size(); j++) {
            const auto& row = buffer[j];
            for(size_t i = 0; i < row.size(); i++) {
                const auto unit = row[i];
                if(unit == nullptr) {
                    continue;
                }
                // if(prev != nullptr) {
                //     // Neighbour unit
                //     if(i - prev_x == 1 && prev_y == j) {
                        
                //     }
                // }
                ss << "\033[" << j + 1 << ";" << i + 1 << "H"; // move cursor
                ss << "\033[38:5:" << unit->get_color() << "m";
                ss << "\033[48:5:" << unit->get_bg_color() << "m";
                ss << unit->get_char();
                ss << "\033[m"; // ansi reset

                // prev = unit;
                // prev_x = i;
                // prev_y = j;
            }
        }
        return ss.str();
    }
};

std::condition_variable cv;
std::mutex mtx;
std::atomic<bool> do_exit(false);

int main(void) {
    Cursor cur;

    std::stringstream frame_buffer;
    init();
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    Frame frm(w.ws_col, w.ws_row);
    cur.hide_cursor();
    std::thread input([&cur, &frm]() {
        char input_char = 0;
        int color = 7;
        int bg = 0;
        int x = 0, y = 0;
        while (!do_exit)
        {
            // Handle input
            read(STDIN_FILENO, &input_char, 1);
            // Actions
            int dir_x = 0, dir_y = 0;
            // stop program
            if(input_char == 'q') {
                do_exit = true;
                cv.notify_all();
                break;
            }

            if(input_char == 'w') {
                if(y > 0) y--;
            } else if(input_char == 'a') {
                if(x > 0) x--;
            } else if(input_char == 's') {
                if(y < frm.get_height() - 1) y++;
            } else if(input_char == 'd') {
                if(x < frm.get_width() - 1) x++;
            } else if(input_char == 'c') {
                frm.clear();
                cv.notify_all();
                continue;
            } else if(input_char == ' ') {
                cur.is_down() ? cur.set_up() : cur.set_down();
            } else if (input_char == 'x') {
                // increase background color value
                if(bg < 255) bg++;
                else bg = 0;
            } else if (input_char == 'z') {
                // decrease background color value
                if(bg > 0) bg--;
                else bg = 255;
            }

            if(cur.is_down()) {
                FrameUnit* unit = frm.get_pixel(x, y);
                unit->set_char(' ');
                unit->set_bg_color(bg);
                unit->set_color(color);
            }

            cv.notify_all();
        }
    });

    std::thread output([&cur, &frm]() {
        while (!do_exit)
        {
            std::unique_lock<std::mutex> l(mtx);
            cv.wait(l);
            // draw ...
            clear();
            std::cout << frm.render() << std::flush;
            // draw cursor position
            // std::cout << "\x1B[" << cur.get_y() << ";" << cur.get_x() - 1 << "m" << "\x1B[38;5;196m" << "- -" << "\x1B[m" << std::flush;
        }
        
    });

    input.join();
    output.join();
    cur.display_cursor();
    endwin();
}