#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
// #include <iterator>
// #include <map>
// #include <optional>
// #include <variant>

using namespace std;
// using Literal = variant<int, double, string, bool>;

// ========================================definition========================================

const string WHITESPACE = " \t\n";
const double ErrorValue = 1e-4;

enum TokenType {
    Number,
    Point, // 小數後部分包含小數點 .1234 
    Ident,
    Str,
    Chr,
    Boolean,
    Operator,
    SignOperator,
    Assign,
    Sign, // 可以連著很多sign
    LParen,
    RParen,
    // LBracket,
    // RBracket,
    // LBrace,
    // RBrace,
    Increment,
    Decrement,
    Semicolon,
    EndOfFile, // 考慮更精確的名稱
    Null,
    Undefined,
};

enum DataType {
    Int,
    Float,
    Char,
    String,
    Bool,
    Void,
};

string enum_to_TokenType(int type) {
    switch (type) {
        case TokenType::Number: return "Number";
        case TokenType::Ident: return "Ident";
        case TokenType::Str: return "String";
        case TokenType::Chr: return "Char";
        case TokenType::Boolean: return "Boolean";
        case TokenType::Operator: return "Operator";
        case TokenType::SignOperator: return "SignOperator";
        case TokenType::Assign: return "Assign";
        case TokenType::Sign: return "Sign";
        case TokenType::LParen: return "LParen";
        case TokenType::RParen: return "RParen";
        // case TokenType::LBracket: return "LBracket";
        // case TokenType::RBracket: return "RBracket";
        // case TokenType::LBrace: return "LBrace";
        // case TokenType::RBrace: return "RBrace";
        case TokenType::Increment: return "Increment";
        case TokenType::Decrement: return "Decrement";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::EndOfFile: return "EOF";
        case TokenType::Null: return "Null";
        case TokenType::Undefined: return "Undefined";
        default: return "Void";
    }
}

string enum_to_DataType(int type) {
    switch (type) {
        case DataType::Int: return "Int";
        case DataType::Float: return "Float";
        case DataType::Char: return "Char";
        case DataType::String: return "String";
        case DataType::Bool: return "Bool";
        default: return "Void";
    }
}

struct token {
    TokenType type;
    string val;
};

struct variable {
    DataType type;
    string val;
};

// struct Array {
//     DataType type;
//     string vals;
// };

// 變數名: (資料型態, 變數值)
unordered_map<string, variable> ident_table;
const unordered_set<string> symbols = {
    "+", "-", "*", "/", "++", "--", 
    "==", "<>", ">", "<", ">=", "<=", "&&", "||", "!", 
    ":=", "+=", "-=", "*=", "/=", "(", ")", ",", ";", 
    // "{", "}", "[", "]", "<<", ">>", ":", "?"
};

// unexpected next token types
unordered_map<TokenType, vector<TokenType>> unexpected_types = {
    {TokenType::Number, {Sign, Assign, Increment, Decrement}},
    {TokenType::Ident, {Sign}},
    {TokenType::Str, {Sign, Assign, Increment, Decrement}},
    {TokenType::Chr, {Sign, Assign, Increment, Decrement}},
    {TokenType::Boolean, {Sign, Assign, Increment, Decrement}}, // 需要檢查
    {TokenType::Operator, {Operator, Assign, Increment, Decrement}},
    {TokenType::SignOperator, {Operator, Assign, Increment, Decrement}},
    {TokenType::Sign, {Operator, Assign, Increment, Decrement}},
    {TokenType::Assign, {Operator, Assign, Increment, Decrement}},
    {TokenType::Increment, {Operator, Assign, Increment, Decrement}},
    {TokenType::Decrement, {Operator, Assign, Increment, Decrement}},
    {TokenType::LParen, {Operator, Assign}},
    {TokenType::RParen, {Sign, Assign, Increment, Decrement}},
    // {TokenType::LBracket, {Operator, Assign}},
    // {TokenType::RBracket, {Sign}},
    // {TokenType::LBrace, {Operator, Assign}},
    // {TokenType::RBrace, {Sign, Assign, Increment, Decrement}},
    {TokenType::Semicolon, {}},
    {TokenType::EndOfFile, {}},
    {TokenType::Null, {}},
    {TokenType::Undefined, {}},
};

token get_next_token();
token peek_token();
double parse_factor();
double parse_term();
double parse_exp();
double parse_bool_exp();
vector<string> split_by_semicolon(const string& content);
string trim(const string& s);
variable convert_to_var(const token tk);
bool is_float(double num);
bool is_in(const string& op, const unordered_set<string> targets);

// ========================================Implementation========================================

class Lexer {
private:
    string text;
    size_t idx = 0;
    token prev_token;

    token get_a_token() {
        while (isspace(text[idx])) idx++;
        if (idx >= text.length()) return {TokenType::EndOfFile, ""};
        
        // string
        if (text[idx] == '"') {
            string str_str;
            while (idx < text.length() && text[idx] != '"') {
                str_str += text[idx];
                idx++;
            }
            return {TokenType::Str, str_str};

        // char
        } else if (idx + 2 < text.length() && text[idx] == '\'' && text[idx + 2] == '\'') {
            string chr_str = string("") + text[idx + 1];
            idx += 3;
            return {TokenType::Chr, chr_str};

        // num (以小數點切割)
        } else if (isdigit(text[idx]) || text[idx] == '.') {
            string num_str;
            bool is_f = false;
            if (text[idx] == '.') is_f = true;
            while (idx < text.length() && (isdigit(text[idx]) || text[idx] == '.')) {
                num_str += text[idx];
                idx++;
                if (idx < text.length() && text[idx] == '.') {
                    if (is_f) return {TokenType::Point, num_str};
                    else return {TokenType::Number, num_str};
                }
            }
            return {TokenType::Number, num_str};
        
        // boolean
        } else if (text.compare(idx, 4, "true") == 0) {
            idx += 4;
            return {TokenType::Boolean, "true"};

        } else if (text.compare(idx, 5, "false") == 0) {
            idx += 5;
            return {TokenType::Boolean, "false"};

        // ident
        } else if (isalpha(text[idx]) || text[idx] == '_') {
            string ident_str;
            while (idx < text.length() && (isalnum(text[idx]) || text[idx] == '_')) {
                ident_str += text[idx];
                idx++;
            }
            // 處理bool
            return {TokenType::Ident, ident_str};

        // operators
        // 需要大幅修改邏輯
        } else {
            // 長度為1或2 Assign Operator Sign LParen RParen
            // 特別處理長度為2的運算符即可 剩下歸類於長度為1
            if (idx + 1 < text.length()) {
                string s = string("") + text[idx] + text[idx + 1];

                if (s == ":=") {idx += 2; return {TokenType::Assign, ":="};}
                else if (s == "+=") {idx += 2; return {TokenType::Assign, "+="};}
                else if (s == "-=") {idx += 2; return {TokenType::Assign, "-="};}
                else if (s == "*=") {idx += 2; return {TokenType::Assign, "*="};}
                else if (s == "/=") {idx += 2; return {TokenType::Assign, "/="};}
                else if (s == "==") {idx += 2; return {TokenType::Operator, "=="};}
                else if (s == ">=") {idx += 2; return {TokenType::Operator, ">="};}
                else if (s == "<=") {idx += 2; return {TokenType::Operator, "<="};}
                else if (s == "<>") {idx += 2; return {TokenType::Operator, "<>"};}
                else if (s == "&&") {idx += 2; return {TokenType::Operator, "&&"};}
                else if (s == "||") {idx += 2; return {TokenType::Operator, "||"};}
                else if (s == "++") {idx += 2; return {TokenType::Increment, "++"};}
                else if (s == "--") {idx += 2; return {TokenType::Decrement, "--"};}
            }
            if (idx < text.length()) { 
                char c = text[idx];
                // 給parser解析 +- sign/operator
                if (c == '+') {idx += 1; return {TokenType::SignOperator, "+"};}
                else if (c == '-') {idx += 1; return {TokenType::SignOperator, "-"};}
                else if (c == '*') {idx += 1; return {TokenType::Operator, "*"};}
                else if (c == '/') {idx += 1; return {TokenType::Operator, "/"};}
                else if (c == '>') {idx += 1; return {TokenType::Operator, ">"};}
                else if (c == '<') {idx += 1; return {TokenType::Operator, "<"};}
                else if (c == '!') {idx += 1; return {TokenType::Sign, "!"};}
                else if (c == '(') {idx += 1; return {TokenType::LParen, "("};}
                else if (c == ')') {idx += 1; return {TokenType::RParen, ")"};}
                else if (c == ';') {idx += 1; return {TokenType::Semicolon, ";"};}
                // else if (c == '[') {idx += 1; return {TokenType::LBracket, "["};}   
                // else if (c == ']') {idx += 1; return {TokenType::RBracket, "]"};}
                // else if (c == '{') {idx += 1; return {TokenType::LBrace, "{"};}
                // else if (c == '}') {idx += 1; return {TokenType::RBrace, "}"};}
                else {idx += 1; return {TokenType::Undefined, string("") + c};} // 返回未定義token

            } else {
                string s;
                while (!strchr("+-*/><()[]{}=", text[idx])) {
                    s += text[idx];
                    idx++;
                }
                return {TokenType::Undefined, s};
            }
        }
        return {TokenType::Undefined, ""};
    }

public:
    Lexer(const string& input) : text(input), idx(0) {
        prev_token = token{Null, "-1"};
    }

    token get_next_token() {
        // get token 並改變idx
        return get_a_token();
    }

    token peek_token() {
        // get token 但不改變idx
        int start_idx = idx;
        token tk = get_a_token();
        idx = start_idx;
        return tk;
    }

    vector<token> traverse() {
        // get token 但不改變idx
        int start_idx = idx;
        token tk = get_next_token();
        vector<token> tks;
        while (tk.type != EndOfFile) {
            tks.push_back(tk);
            tk = get_next_token();
        }
        idx = start_idx;
        return tks;
    }

    string get_rest_str() {
        if (idx < text.length()) {
            return text.substr(idx);
        }
        return "";
    }
};

// for nums (因為函數宣告問題)
class Parser {
private:
    Lexer lexer;
    token cur_token;
    token prev_token;

    void next() {
        // 如果 "下一個" 不是預期的token就根據問題丟出報錯 只處理報錯
        auto ue_types = unexpected_types[cur_token.type];
        token next_token = lexer.peek_token();

        // 符號未定義
        if (cur_token.type == TokenType::Undefined) {
            throw runtime_error("> Unrecognized token with first char : '" + cur_token.val + "'");

        // 後接非法符號
        } else if (find(ue_types.begin(), ue_types.end(), 
                        next_token.type) != ue_types.end()) {
            throw runtime_error("> Unexpected token : '" + next_token.val + "'");

        // ident未定義且後面非賦值
        } else if (cur_token.type == TokenType::Ident 
                   && ident_table.find(cur_token.val) != ident_table.end()
                   && next_token.type != TokenType::Assign) {
            throw runtime_error("> Undefined identifier : '" + cur_token.val + "'");
        }
        prev_token = cur_token;
        cur_token = lexer.get_next_token();
        return;
    }

    double parse_factor() {
        // num, (), sign
        double val = 0;

        if (cur_token.val == "(") {
            next();
            // (is_return_bool) ? val = parse_bool_exp() : val = parse_exp();
            val = parse_exp();
            if (cur_token.val == ")") {
                next();
            } else {
                throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
            }
        
        // cout << "cur: " + cur_token.val + " | next: " + lexer.peek_token().val << endl;
        } else if (is_in(cur_token.val, {"+", "-", "!"})) {
            if (cur_token.val == "+") {
                next();
                val = parse_factor();
            } else if (cur_token.val == "-") {
                next();
                val = -parse_factor();
            } else if (cur_token.val == "!") {
                next();
                val = !parse_factor();
            }
        } else {
            if (cur_token.type == TokenType::Number) {
                variable var = convert_to_var(cur_token);
                next();
                val = stod(var.val);

            } else if (cur_token.type == TokenType::Ident) {
                token id_token = cur_token;
                next(); // 通過測試
                // 取值
                val = stod(ident_table[id_token.val].val);

            } else {
                if (symbols.find(cur_token.val) == symbols.end()) { // not found
                    throw runtime_error("> Unrecognized token with first char : '" + cur_token.val + "'");
                } else {
                    throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
                }
            }
        }
        return val;
    }

    double parse_term() {
        // *, /
        double val = parse_factor();
        // if (cur_token.type == TokenType::EndOfFile) return val;

        while (is_in(cur_token.val, {"*", "/"})) {
            if (cur_token.val == "*") {
                next();
                val = val * parse_factor();
            } else if (cur_token.val == "/") {
                next();
                double a = parse_factor();
                if (a != 0) val = val / a;
                else throw runtime_error("Error");
            }
        }

        return val;
    }

    double parse_exp() {
        // +, -
        double val = parse_term();
        // if (cur_token.type == TokenType::EndOfFile) return val;
        
        // cout << "prev: " + prev_token.val + " | cur: " + cur_token.val << endl;
        while (is_in(cur_token.val, {"+", "-"})) {
            if (cur_token.val == "+") {
                next();
                val = val + parse_term();
            } else if (cur_token.val == "-") {
                next();
                val = val - parse_term();
            }
        }

        return val;
    }

    bool parse_bool_exp() {
        // == >= <> <= && ||
        double val = parse_exp();
        if (cur_token.type == TokenType::EndOfFile) return val;
        
        while (is_in(cur_token.val, {"==", "<=", ">=", "<>", "&&", "||", "<", ">"})) {
            if (cur_token.val == "==") {
                next();
                val = val == parse_exp();
            } else if (cur_token.val == ">=") {
                next();
                val = val >= parse_exp();
            } else if (cur_token.val == "<=") {
                next();
                val = val <= parse_exp();
            } else if (cur_token.val == "<>") {
                next();
                val = val != parse_exp();
            } else if (cur_token.val == "&&") {
                next();
                val = val && parse_exp();
            } else if (cur_token.val == "||") {
                next();
                val = val || parse_exp();
            } else if (cur_token.val == "<") {
                next();
                val = val < parse_exp();
            } else if (cur_token.val == ">") {
                next();
                val = val > parse_exp();
            } 
        }
        return val;
    } 

    double parse_statement() {
        double val = parse_exp();
        if (is_float(val)) {
            ident_table[cur_token.val] = variable{DataType::Float, to_string(val)};
        } else {
            ident_table[cur_token.val] = variable{DataType::Int, to_string(val)};
        }
        return val;
    }

public:
    // bool is_return_bool;
    Parser(const string& input) : lexer(input) {
        cur_token = lexer.get_next_token();
        prev_token = {TokenType::Null, "-1"};
    }

    string get_rest_str() {
        return lexer.get_rest_str();
    }

    void parse_cmd() {
        // statement | bool_exp | exp 
        // first token
        // statement
        if (cur_token.type == TokenType::Ident && lexer.peek_token().val == ":=") {
            next();
            next();
            parse_statement();

        } else {
            bool is_return_bool = false;
            auto tokens = lexer.traverse();
            for (int i = 0; i < tokens.size(); i++) {
                if (is_in(tokens[i].val, {"==", "<=", ">=", "<>", "&&", "||", "<", ">"})) {
                    is_return_bool = true;
                    break;
                }
            }
            // bool exp
            if (is_return_bool) {
                bool result = parse_bool_exp();
                if (cur_token.type != TokenType::Semicolon) {
                    if (is_in(cur_token.val, symbols)){
                        throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
                    } else {
                        throw runtime_error("> Unrecognized token with first char : '" + cur_token.val + "'");
                    }
                }
                if (result) {
                    cout << "> true" << endl;
                } else {
                    cout << "> false" << endl;
                }
            // exp
            } else {
                double result = parse_exp();
                if (cur_token.type != TokenType::Semicolon) {
                    if (is_in(cur_token.val, symbols)){
                        throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
                    } else {
                        throw runtime_error("> Unrecognized token with first char : '" + cur_token.val + "'");
                    }
                }
                cout << "> " << result << endl;
            }
        }
    }

    void check_cmd() {
        vector<token> tokens;
        while (cur_token.type != EndOfFile) {
            tokens.push_back(cur_token);
            next();
        }
        // for (int i = 0; i < tokens.size(); i++) {
        //     cout << "type: " << enum_to_TokenType(tokens[i].type) << " | val: " << tokens[i].val << endl;
        // }
    }
};

// ========================================utils========================================

vector<string> split_by_semicolon(const string& content) {
    vector<string> exps;
    string str = "";

    int idx = 0, len = content.length();
    while (idx < len) {
        while (content[idx] != ';' && idx < len) {
            if (idx + 1 < len && content[idx] == '/' && content[idx + 1] == '/') {
                while (content[idx] != '\n' && idx < len) {
                    idx++;
                }
            } else {
                str += content[idx];
                idx++;
            }
        } 
        if (idx < len && content[idx] == ';') {
            str += content[idx];
            idx++;
        }
        string trimmed = trim(str);
        if (trimmed.compare(0, 4, "quit") == 0) {
            break;
        } else {
            if (trimmed.length() > 0) exps.push_back(trimmed);
            str = "";
        }
    }
    return exps;
}

// const string& 傳引用(保護正本) const string 傳值(會複製一份副本且保護副本)
string trim(const string& s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    if (start == string::npos) return ""; 
    size_t end = s.find_last_not_of(WHITESPACE);
    return s.substr(start, end - start + 1); // 起點、長度
}

variable convert_to_var(const token tk) {
    // 假設輸入至多一個小數點
    if (tk.type == TokenType::Number) {
        for (int i = 0; i < tk.val.length(); i++) {
            if (tk.val[i] == '.') {
                return {DataType::Float, tk.val};
            }
        }
        return {DataType::Int, tk.val};
    } else if (tk.type == TokenType::Str) {
        return {DataType::String, tk.val};
    } else if (tk.type == TokenType::Chr) {
        return {DataType::Char, tk.val};
    } else if (tk.type == TokenType::Boolean) {
        return {DataType::Bool, tk.val};
    } else {
        throw runtime_error("Error in convert_to_var()");
    }
}

bool is_float(double num) {
    return (abs(num - floor(num)) < ErrorValue);
}

bool is_in(const string& op, const unordered_set<string> targets) {
    return targets.find(op) != targets.end();
}

void parse_wrapper(string cmd) {
    Parser parser(cmd);
    try {
        if (cmd.length() > 0) {
            parser.parse_cmd();
        }

    } catch (const std::exception& e) {
        cout << e.what() << std::endl;
        string rest_str = parser.get_rest_str();
        auto start = rest_str.find_first_of("\n");
        if (rest_str.length() > 0 && start != string::npos) {
            parse_wrapper(rest_str.substr(start));
        }
    }
}

#include <fstream>
#include <sstream>

int main() {
    // ios::sync_with_stdio(false);
    // cin.tie(0);
    cout << "Program starts..." << endl;

    ifstream file("test/data.txt"); // 本機測試
    stringstream ss;
    ss << file.rdbuf(); // 將整個檔案緩衝區讀入 stringstream
    string content_ = ss.str();
    auto start = content_.find_first_of("\n") + 1, end = content_.length();
    string content = content_.substr(start, end - start + 1);

    // string content, _; // 上傳測試
    // cin >> _; // 去除題號

    // char c;
    // while (cin.get(c)) {
    //     content += c;
    // }

    auto cmds = split_by_semicolon(content);

    for (int i = 0; i < cmds.size(); i++) {
        parse_wrapper(cmds[i]);
    }

    cout << "> Program exits..." << endl;
    return 0;
}