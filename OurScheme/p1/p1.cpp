#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

using namespace std;
using int64 = long long;

bool DEBUG = false;

// 1. 結構體宣告與繼承
// 2. 推導指南 (Deduction Guide)
/*
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
*/

// ========================================Declarations========================================

struct Token;
class Lexer;

bool is_in(const string &op, const unordered_set<string> &targets);

enum TokenType {
    LEFT_PAREN, RIGHT_PAREN, INT, STRING, DOT, FLOAT, NIL, T, QUOTE, SYMBOL, CONS, EndOfFile, Undefined,
};

// ========================================Structs Definition========================================

struct Token {
    TokenType type;
    string val;
    int line = 1;
    int column = 1;
    size_t start_idx = 0;
    size_t end_idx = 0;
};

struct Node {
    // 如果是 Atom，存放字串或數字內容，
    // 當前token的內容指向佐子樹，剩餘內容指向柚子樹
    TokenType type;
    variant<monostate, int64, double, bool, string> val;
    shared_ptr<Node> car;
    shared_ptr<Node> cdr;

    // Default constructor
    Node() : type(TokenType::Undefined), val(monostate{}), car(nullptr), cdr(nullptr) {}
    // Atom constructor
    Node(TokenType t, const string &v) : type(t), car(nullptr), cdr(nullptr) {
        if (t == TokenType::INT) {
            val = stoll(v);
        } else if (t == TokenType::FLOAT) {
            val = stod(v);
        } else if (t == TokenType::T) {
            val = true;
        } else if (t == TokenType::NIL) {
            val = false;
        } else {
            val = v;
        }
    }
    // Cons constructor
    Node(shared_ptr<Node> l, shared_ptr<Node> r) : type(TokenType::CONS), val(monostate{}), car(l), cdr(r) {}
};

// ========================================Global Lists========================================

// 內建函數與特殊符號列表
const unordered_set<string> symbols = {
    "+", "-", "*", "/", "quote", "cons", "car", "cdr"
};

const unordered_set<string> keywords = ([]{
    unordered_set<string> s = {
        "nil", "#f", "t", "#t",
    };
    s.insert(symbols.begin(), symbols.end());
    return s;
}());

// ========================================Function Definition========================================

bool is_in(const string &str, const unordered_set<string> &targets) { return targets.find(str) != targets.end(); }

// ========================================Implementation========================================

class Lexer {
private:
    struct Checkpoint {
        size_t idx;
        int cur_line;
        size_t token_ptr;
    };

    const string text;
    size_t idx = 0;
    int cur_line = 1;
    Token cur_token;
    vector<int> token_line = {0};

    bool is_valid_int(const string &s) {
        if (s.empty()) return false;
        size_t start = 0;
        if (s[0] == '+' || s[0] == '-') {
            start = 1;
        }
        if (start == s.length()) return false;
        for (size_t i = start; i < s.length(); i++) {
            if (!isdigit(s[i])) return false;
        }
        return true;
    }

    bool is_valid_float(const string &s) {
        if (s.empty()) return false;
        size_t start = 0;
        if (s[0] == '+' || s[0] == '-') {
            start = 1;
        }
        if (start == s.length()) return false;
        
        size_t dot_count = 0;
        size_t digit_count = 0;
        for (size_t i = start; i < s.length(); i++) {
            if (s[i] == '.') {
                dot_count++;
            } else if (isdigit(s[i])) {
                digit_count++;
            } else {
                return false;
            }
        }
        return dot_count == 1 && digit_count > 0;
    }

    Token make_eol_error_token(int col, size_t start_idx) {
        int err_col = (int)idx - token_line[cur_line - 1] + 1;
        string err = "ERROR (no closing quote) : END-OF-LINE encountered at Line " +
                     to_string(cur_line) + " Column " + to_string(err_col);
        return Token{TokenType::Undefined, err, cur_line, col, start_idx, idx};
    }

    Token get_a_token() {
        while (idx < text.length()) {
            if (isspace(text[idx])) {
                if (text[idx] == '\n') {
                    cur_line++;
                    token_line.push_back((int)(idx + 1));
                }
                idx++;
            } else if (text[idx] == ';') {
                while (idx < text.length() && text[idx] != '\n') {
                    idx++;
                }
                if (idx < text.length() && text[idx] == '\n') {
                    cur_line++;
                    idx++;
                    token_line.push_back((int)idx);
                }
            } else {
                break;
            }
        }

        if (idx >= text.length()) {
            return Token{TokenType::EndOfFile, "", cur_line, (int)idx - token_line[cur_line - 1] + 1, idx, idx};
        }

        size_t start_idx = idx;
        int col = (int)idx - token_line[cur_line - 1] + 1;

        if (text[idx] == '(') {
            idx++;
            return Token{TokenType::LEFT_PAREN, "(", cur_line, col, start_idx, idx};
        }
        if (text[idx] == ')') {
            idx++;
            return Token{TokenType::RIGHT_PAREN, ")", cur_line, col, start_idx, idx};
        }
        if (text[idx] == '\'') {
            idx++;
            return Token{TokenType::QUOTE, "'", cur_line, col, start_idx, idx};
        }

        if (text[idx] == '"') {
            string val = "\"";
            idx++;
            while (idx < text.length() && text[idx] != '"') {
                if (text[idx] == '\n') return make_eol_error_token(col, start_idx);
                if (text[idx] == '\\') {
                    idx++;
                    if (idx < text.length()) {
                        if (text[idx] == 'n') val += "\n";
                        else if (text[idx] == 't') val += "\t";
                        else if (text[idx] == '"') val += "\"";
                        else if (text[idx] == '\\') val += "\\";
                        else {
                            val += "\\";
                            val += string(1, text[idx]);
                        }
                        idx++;
                    } else {
                        return make_eol_error_token(col, start_idx);
                    }
                } else {
                    val += text[idx];
                    idx++;
                }
            }
            if (idx < text.length() && text[idx] == '"') {
                val += "\"";
                idx++;
                return Token{TokenType::STRING, val, cur_line, col, start_idx, idx};
            } else {
                return make_eol_error_token(col, start_idx);
            }
        }

        string val = "";
        while (idx < text.length()) {
            char c = text[idx];
            if (isspace(c) || c == '(' || c == ')' || c == '\'' || c == '"' || c == ';') {
                break;
            }
            val += c;
            idx++;
        }

        if (val == ".") {
            return Token{TokenType::DOT, val, cur_line, col, start_idx, idx};
        }
        if (val == "nil" || val == "#f") {
            return Token{TokenType::NIL, val, cur_line, col, start_idx, idx};
        }
        if (val == "t" || val == "#t") {
            return Token{TokenType::T, val, cur_line, col, start_idx, idx};
        }
        if (is_valid_int(val)) {
            return Token{TokenType::INT, val, cur_line, col, start_idx, idx};
        }
        if (is_valid_float(val)) {
            return Token{TokenType::FLOAT, val, cur_line, col, start_idx, idx};
        }
        
        return Token{TokenType::SYMBOL, val, cur_line, col, start_idx, idx};
    }

public:
    Lexer(const string &input) : text(input), idx(0), token_line({0}) {}

    size_t get_idx() const { return idx; }

    Token get_next_token() {
        Token tk = get_a_token();
        cur_token = tk;
        return tk;
    }

    void skip_to_newline() {
        while (idx < text.length() && text[idx] != '\n') {
            idx++;
        }
        if (idx < text.length() && text[idx] == '\n') {
            cur_line++;
            idx++;
            token_line.push_back((int)idx);
        }
    }

    void reset_pos_tracking(size_t start_idx) {
        size_t temp_idx = start_idx;
        while (temp_idx < text.length()) {
            if (isspace(text[temp_idx])) {
                if (text[temp_idx] == '\n') {
                    break;
                }
                temp_idx++;
            } else if (text[temp_idx] == ';') {
                while (temp_idx < text.length() && text[temp_idx] != '\n') {
                    temp_idx++;
                }
            } else {
                break;
            }
        }
        if (temp_idx < text.length() && text[temp_idx] == '\n') {
            temp_idx++;
            idx = temp_idx;
            cur_line = 1;
            token_line = {(int)temp_idx};
        } else {
            idx = temp_idx;
            cur_line = 1;
            token_line = {(int)start_idx};
        }
    }

    bool is_at_eof() {
        size_t temp_idx = idx;
        while (temp_idx < text.length()) {
            if (isspace(text[temp_idx])) {
                temp_idx++;
            } else if (text[temp_idx] == ';') {
                while (temp_idx < text.length() && text[temp_idx] != '\n') {
                    temp_idx++;
                }
                if (temp_idx < text.length() && text[temp_idx] == '\n') {
                    temp_idx++;
                }
            } else {
                return false;
            }
        }
        return true;
    }
};

class Parser {
private:
    Lexer lexer;
    Token cur_token;
    shared_ptr<Node> root;
    size_t prev_end_idx = 0;
    bool need_next_token = true;

    void next() {
        prev_end_idx = cur_token.end_idx;
        cur_token = lexer.get_next_token();
    }

    void throw_unexpected_error(const Token &tk, const string &expected) {
        if (tk.type == TokenType::EndOfFile) {
            throw runtime_error("ERROR (no more input) : END-OF-FILE encountered");
        }
        string err = "ERROR (unexpected token) : " + expected + " expected when token at Line " +
                     to_string(tk.line) + " Column " + to_string(tk.column) + " is >>" + tk.val + "<<";
        throw runtime_error(err);
    }

    shared_ptr<Node> parse_s_exp_internal() {
        if (cur_token.type == TokenType::Undefined) {
            throw runtime_error(cur_token.val);
        }
        if (cur_token.type == TokenType::EndOfFile) {
            throw_unexpected_error(cur_token, "atom or '('");
        }

        if (cur_token.type == TokenType::LEFT_PAREN) {
            next();
            if (cur_token.type == TokenType::RIGHT_PAREN) {
                next();
                return make_shared<Node>(TokenType::NIL, "nil");
            }
            shared_ptr<Node> car = parse_s_exp_internal();
            shared_ptr<Node> head = make_shared<Node>(car, nullptr);
            shared_ptr<Node> cur_node = head;
            while (cur_token.type != TokenType::RIGHT_PAREN && cur_token.type != TokenType::DOT && cur_token.type != TokenType::EndOfFile) {
                shared_ptr<Node> next_el = parse_s_exp_internal();
                shared_ptr<Node> next_node = make_shared<Node>(next_el, nullptr);
                cur_node->cdr = next_node;
                cur_node = next_node;
            }
            if (cur_token.type == TokenType::DOT) {
                next();
                shared_ptr<Node> last_el = parse_s_exp_internal();
                cur_node->cdr = last_el;
                if (cur_token.type != TokenType::RIGHT_PAREN) {
                    throw_unexpected_error(cur_token, "')'");
                }
            }
            if (cur_token.type != TokenType::RIGHT_PAREN) {
                throw_unexpected_error(cur_token, "')'");
            }
            next();
            return head;
        }

        if (cur_token.type == TokenType::QUOTE) {
            next();
            shared_ptr<Node> sub = parse_s_exp_internal();
            shared_ptr<Node> quote_sym = make_shared<Node>(TokenType::SYMBOL, "quote");
            shared_ptr<Node> sub_cons = make_shared<Node>(sub, make_shared<Node>(TokenType::NIL, "nil"));
            return make_shared<Node>(quote_sym, sub_cons);
        }

        if (cur_token.type == TokenType::INT || cur_token.type == TokenType::FLOAT ||
            cur_token.type == TokenType::STRING || cur_token.type == TokenType::NIL ||
            cur_token.type == TokenType::T || cur_token.type == TokenType::SYMBOL) {
            shared_ptr<Node> atom = make_shared<Node>(cur_token.type, cur_token.val);
            next();
            return atom;
        }

        throw_unexpected_error(cur_token, "atom or '('");
        return nullptr;
    }

public:    
    Parser(const string &input) : lexer(input) {
        prev_end_idx = 0;
        need_next_token = true;
    }

    shared_ptr<Node> parse_s_exp() {
        if (need_next_token) {
            cur_token = lexer.get_next_token();
            need_next_token = false;
        }
        if (cur_token.type == TokenType::EndOfFile) {
            throw runtime_error("ERROR (no more input) : END-OF-FILE encountered");
        }
        return parse_s_exp_internal();
    }

    bool is_eof() {
        return cur_token.type == TokenType::EndOfFile;
    }

    void recover_after_error() {
        lexer.skip_to_newline();
        lexer.reset_pos_tracking(lexer.get_idx());
        need_next_token = true;
    }

    void reset_for_next_s_exp() {
        lexer.reset_pos_tracking(prev_end_idx);
        need_next_token = true;
    }
};

// ========================================Built-in Functions========================================



// ========================================Helper Functions========================================

void print_s_exp(shared_ptr<Node> node, int indent, bool first_on_line) {
    if (node == nullptr) return;
    
    if (node->type != TokenType::CONS) {
        // Atom
        if (first_on_line) {
            cout << string(indent, ' ');
        }
        // Handle special print representations
        if (node->type == TokenType::NIL) {
            cout << "nil";
        } else if (node->type == TokenType::T) {
            cout << "#t";
        } else if (node->type == TokenType::INT) {
            if (holds_alternative<int64>(node->val)) {
                cout << get<int64>(node->val);
            } else if (holds_alternative<string>(node->val)) {
                try {
                    long long val = stoll(get<string>(node->val));
                    cout << val;
                } catch (...) {
                    cout << get<string>(node->val);
                }
            }
        } else if (node->type == TokenType::FLOAT) {
            // always print 3 decimal places
            if (holds_alternative<double>(node->val)) {
                cout << fixed << setprecision(3) << get<double>(node->val);
            } else if (holds_alternative<string>(node->val)) {
                try {
                    double val = stod(get<string>(node->val));
                    cout << fixed << setprecision(3) << val;
                } catch (...) {
                    cout << get<string>(node->val);
                }
            }
        } else {
            if (holds_alternative<string>(node->val)) {
                cout << get<string>(node->val);
            }
        }
        cout << "\n";
    } else {
        // CONS cell
        if (first_on_line) {
            cout << string(indent, ' ');
        }
        cout << "( ";
        
        // Traverse and print elements
        shared_ptr<Node> cur_node = node;
        // Print first element (on the same line as the parent's `( `)
        print_s_exp(cur_node->car, indent + 2, false);
        
        // Subsequent elements
        while (cur_node->cdr != nullptr && cur_node->cdr->type == TokenType::CONS) {
            cur_node = cur_node->cdr;
            print_s_exp(cur_node->car, indent + 2, true);
        }
        
        // If dotted pair
        if (cur_node->cdr != nullptr && cur_node->cdr->type != TokenType::NIL) {
            cout << string(indent + 2, ' ') << ".\n";
            print_s_exp(cur_node->cdr, indent + 2, true);
        }
        
        cout << string(indent, ' ') << ")\n";
    }
}

void print_s_exp(shared_ptr<Node> root) {
    print_s_exp(root, 0, true);
}

bool is_exit_command(shared_ptr<Node> node) {
    if (node == nullptr) return false;
    // (exit) is a CONS cell where car is "exit" and cdr is NIL/nil
    if (node->car != nullptr && node->car->type != TokenType::CONS) {
        if (holds_alternative<string>(node->car->val) && get<string>(node->car->val) == "exit" && (node->cdr == nullptr || node->cdr->type == TokenType::NIL)) {
            return true;
        }
    }
    return false;
}

void parse_wrapper(Parser &parser) {
    cout << "Welcome to OurScheme!\n\n";
    
    while (true) {
        cout << "> ";
        cout.flush();
        try {
            shared_ptr<Node> root = parser.parse_s_exp();
            if (root == nullptr) {
                break;
            }
            if (is_exit_command(root)) {
                cout << endl;
                break;
            }
            print_s_exp(root, 0, false);
            cout << endl; // Prints a blank line after output
            parser.reset_for_next_s_exp();
        } catch (const exception &e) {
            cout << e.what() << endl;
            parser.recover_after_error();
            if (parser.is_eof()) {
                break;
            }
            cout << endl;
        }
    }
    
    cout << "Thanks for using OurScheme!" << endl;
}

int main() {
    cout << fixed << setprecision(3);

    string content, _;
    cin >> _; // 忽略題號
    cin.ignore();
    char c;
    while (cin.get(c)) {
        content += c;
    }
    
    Parser parser(content);
    parse_wrapper(parser);
    return 0;
}