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
#include <iomanip>
#include <fstream>
#include <sstream>
// #include <iterator>
// #include <map>
// #include <optional>
// #include <variant>

using namespace std;
// using Literal = variant<int, double, string, bool>;

// ========================================definition========================================

const string WHITESPACE = " \t\r";
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

// ========================================Non-Variable Function Declarations========================================
bool is_float(double num);
bool is_in(const string& op, const unordered_set<string> targets);
string trim(const string& s);
string num_to_string(double num);
bool parse_wrapper(string cmd);

// ========================================Struct Definition========================================

struct token {
    TokenType type;
    string val;
};

struct variable {
    DataType type;
    string val;

private:
    bool is_comparable(const variable& var1, const variable& var2) {
        if ((var1.type == DataType::String && var2.type == DataType::String)
            || ((var1.type == DataType::Char || var1.type == DataType::Int 
            || var1.type == DataType::Float || var1.type == DataType::Bool) 
            && (var2.type == DataType::Char || var2.type == DataType::Int 
            || var2.type == DataType::Float || var2.type == DataType::Bool))) {
            return true;
        }
        return false;
    }

    variable bool_evaluate(const variable& var1, const string& op, const variable& var2) {
        double n1, n2;
        if (var1.type == DataType::Char && var1.val.length() == 1) n1 = (int)(var1.val[0]);
        else if (var1.type == DataType::Int || var1.type == DataType::Float) n1 = stod(var1.val);
        else if (var1.type == DataType::Bool) n1 = var1.val == "false" ? 0 : 1;
        else throw runtime_error("Error in bool_evaluate with values: " + var1.val + " " + op + " " + var2.val);
        if (var2.type == DataType::Char && var2.val.length() == 1) n2 = (int)(var2.val[0]);
        else if (var2.type == DataType::Int || var2.type == DataType::Float) n2 = stod(var2.val);
        else if (var2.type == DataType::Bool) n2 = var2.val == "false" ? 0 : 1;
        else throw runtime_error("Error in bool_evaluate with values: " + var1.val + " " + op + " " + var2.val);
        
        if (op == "=") {
            if (abs(n1 - n2) <= ErrorValue) return {DataType::Bool, "true"};
            else return {DataType::Bool, "false"};
        } else if (op == "!=") {
            if (abs(n1 - n2) > ErrorValue) return {DataType::Bool, "true"};
            else return {DataType::Bool, "false"};
        } else if (op == "<") {
            if (n1 + ErrorValue < n2) return {DataType::Bool, "true"};
            else return {DataType::Bool, "false"};
        } else if (op == ">") {
            if (n1 > n2 + ErrorValue) return {DataType::Bool, "true"};
            else return {DataType::Bool, "false"};
        } else if (op == "<=") {
            if (n1 + ErrorValue <= n2) return {DataType::Bool, "true"};
            else return {DataType::Bool, "false"};
        } else if (op == ">=") {
            if (n1 >= n2 + ErrorValue) return {DataType::Bool, "true"};
            else return {DataType::Bool, "false"};
        } else {
            throw runtime_error("Error in bool_evaluate with values: " + var1.val + " " + op + " " + var2.val);
        }
    }

public:
    explicit operator bool() const {
        if ((this->type == DataType::Bool && this->val == "false")
            || (this->type == DataType::Int && this->val == "0")
            || (this->type == DataType::Float && this->val == "0.0")
            || (this->type == DataType::Char && this->val == "\0")
            || (this->type == DataType::String && this->val == "")) {
            return false;
        } else {
            return true;
        }
    }

    variable operator+() {
        if (DataType::Int == this->type || DataType::Float == this->type) {
            return {this->type, this->val};
        } else {
            throw runtime_error("Error in operator unary +");
        }
    }

    variable operator-() {
        if (DataType::Int == this->type || DataType::Float == this->type) {
            return {this->type, num_to_string(-stod(this->val))};
        } else {
            throw runtime_error("Error in operator unary -");
        }
    }

    variable operator!() {
        if (DataType::Bool == this->type) {
            return {this->type, this->val == "true" ? "false" : "true"};
        } else {
            throw runtime_error("Error in operator unary !");
        }
    }

    variable operator+(const variable& var2) {
        DataType rtype;
        string result;
        if (this->type == DataType::Int && var2.type == DataType::Int) {
            rtype = DataType::Int;
            result = num_to_string(stod(this->val) + stod(var2.val));

        } else if ((this->type == DataType::Float && var2.type == DataType::Float) 
                   || (this->type == DataType::Int && var2.type == DataType::Float) 
                   || (this->type == DataType::Float && var2.type == DataType::Int)) {
            rtype = DataType::Float;
            result = num_to_string(stod(this->val) + stod(var2.val));

        } else if (this->type == DataType::String && (var2.type == DataType::String || var2.type == DataType::Char)
                   || (this->type == DataType::Char && var2.type == DataType::String)) {
            rtype = DataType::String;
            result = this->val + var2.val;
        
        } else if (this->type == DataType::Char && var2.type == DataType::Char) {
            rtype = DataType::Char;
            result = num_to_string(this->val[0] + var2.val[0]);

        } else {
            throw runtime_error("Error in operator+");
        }

        return {rtype, result};
    }

    variable operator-(const variable& var2) {
        DataType rtype;
        string result;
        if (this->type == DataType::Int && var2.type == DataType::Int) {
            rtype = DataType::Int;
            result = num_to_string(stod(this->val) - stod(var2.val));

        } else if ((this->type == DataType::Float && var2.type == DataType::Float) 
                   || (this->type == DataType::Int && var2.type == DataType::Float) 
                   || (this->type == DataType::Float && var2.type == DataType::Int)) {
            rtype = DataType::Float;
            result = num_to_string(stod(this->val) - stod(var2.val));

        } else {
            throw runtime_error("Error in operator-");
        }

        return {rtype, result};
    }

    variable operator*(const variable& var2) {
        DataType rtype;
        string result;
        if (this->type == DataType::Int && var2.type == DataType::Int) {
            rtype = DataType::Int;
            result = num_to_string(stod(this->val) * stod(var2.val));

        } else if ((this->type == DataType::Float && var2.type == DataType::Float) 
                   || (this->type == DataType::Int && var2.type == DataType::Float) 
                   || (this->type == DataType::Float && var2.type == DataType::Int)) {
            rtype = DataType::Float;
            result = num_to_string(stod(this->val) * stod(var2.val));

        } else {
            throw runtime_error("Error in operator*");
        }

        return {rtype, result};
    }

    variable operator/(const variable& var2) {
        DataType rtype;
        string result;
        if (this->type == DataType::Int && var2.type == DataType::Int) {
            if (var2.val == "0") {
                throw runtime_error("> Error"); // 為符合題目要求
            }
            auto tmp = stod(this->val) / stod(var2.val);
            // cout << tmp << endl;
            if (is_float(tmp)) rtype = DataType::Float;
            else rtype = DataType::Int;
            result = num_to_string(tmp);

        } else if ((this->type == DataType::Float || this->type == DataType::Float) 
                   || (this->type == DataType::Int && var2.type == DataType::Float) 
                   || (this->type == DataType::Float && var2.type == DataType::Int)) {
            if (var2.val == "0" || var2.val == "0.0") {
                throw runtime_error("> Error"); // 為符合題目要求
            }
            rtype = DataType::Float;
            result = num_to_string(stod(this->val) / stod(var2.val));

        } else {
            throw runtime_error("Error in operator/");
        }

        return {rtype, result};
    }

    variable operator==(const variable& var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator==: incomparable variable: " + enum_to_DataType(this->type) + ", " + enum_to_DataType(var2.type));
        }
        DataType rtype;
        string result;
        if (this->type == DataType::String && var2.type == DataType::String) {
            if (this->val == var2.val) {
                rtype = DataType::Bool;
                result = "true";
            } else {
                rtype = DataType::Bool;
                result = "false";
            }
        } else {
            variable v = bool_evaluate(*this, "=", var2);
            rtype = v.type;
            result = v.val;
        }
        return {rtype, result};
    }

    variable operator!=(const variable& var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator!=: incomparable variable");
        }
        DataType rtype;
        string result;
        if (this->type == DataType::String && var2.type == DataType::String) {
            if (this->val != var2.val) {
                rtype = DataType::Bool;
                result = "true";
            } else {
                rtype = DataType::Bool;
                result = "false";
            }
        } else {
            variable v = bool_evaluate(*this, "!=", var2);
            rtype = v.type;
            result = v.val;
        }
        return {rtype, result};
    }

    variable operator>=(const variable& var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator>=: incomparable variable");
        }
        DataType rtype;
        string result;
        if (this->type == DataType::String && var2.type == DataType::String) {
            if (this->val >= var2.val) {
                rtype = DataType::Bool;
                result = "true";
            } else {
                rtype = DataType::Bool;
                result = "false";
            }
        } else {
            variable v = bool_evaluate(*this, ">=", var2);
            rtype = v.type;
            result = v.val;
        }
        return {rtype, result};
    }

    variable operator<=(const variable& var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator<=: incomparable variable");
        }
        DataType rtype;
        string result;
        if (this->type == DataType::String && var2.type == DataType::String) {
            if (this->val <= var2.val) {
                rtype = DataType::Bool;
                result = "true";
            } else {
                rtype = DataType::Bool;
                result = "false";
            }
        } else {
            variable v = bool_evaluate(*this, "<=", var2);
            rtype = v.type;
            result = v.val;
        }
        return {rtype, result};
    }

    variable operator>(const variable& var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator>: incomparable variable");
        }
        DataType rtype;
        string result;
        if (this->type == DataType::String && var2.type == DataType::String) {
            if (this->val > var2.val) {
                rtype = DataType::Bool;
                result = "true";
            } else {
                rtype = DataType::Bool;
                result = "false";
            }
        } else {
            variable v = bool_evaluate(*this, ">", var2);
            rtype = v.type;
            result = v.val;
        }
        return {rtype, result};
    }

    variable operator<(const variable& var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator<: incomparable variable");
        }
        DataType rtype;
        string result;
        if (this->type == DataType::String && var2.type == DataType::String) {
            if (this->val < var2.val) {
                rtype = DataType::Bool;
                result = "true";
            } else {
                rtype = DataType::Bool;
                result = "false";
            }
        } else {
            variable v = bool_evaluate(*this, "<", var2);
            rtype = v.type;
            result = v.val;
        }
        return {rtype, result};
    }

    variable operator&&(const variable& var2) {
        // 兩者皆為 true 才為 true
        if (bool(*this) && bool(var2)) return {DataType::Bool, "true"};
        else return {DataType::Bool, "false"};
    }

    variable operator||(const variable& var2) {
        if (bool(*this) || bool(var2)) return {DataType::Bool, "true"};
        else return {DataType::Bool, "false"};
    }
};  

// struct Array {
//     DataType type;
//     string vals;
// };

// 變數名: (資料型態, 變數值)
unordered_map<string, variable> ident_table;
const unordered_set<string> symbols = {
    "+", "-", "*", "/", "++", "--", 
    "=", "<>", ">", "<", ">=", "<=", "&&", "||", "!", 
    ":=", "+=", "-=", "*=", "/=", "(", ")", ",", ";", 
    // "{", "}", "[", "]", "<<", ">>", ":", "?"
};

// unexpected next token types
unordered_map<TokenType, vector<TokenType>> unexpected_types = {
    {TokenType::Number, {Sign, Assign, Ident, LParen}},
    {TokenType::Point, {Sign, Assign, Ident, LParen}},
    {TokenType::Ident, {Sign, Ident, LParen}},
    {TokenType::Str, {Sign, Assign, Increment, Decrement, LParen}},
    {TokenType::Chr, {Sign, Assign, LParen}},
    {TokenType::Boolean, {Sign, Assign, Increment, Decrement, LParen}}, // 需要檢查
    {TokenType::Operator, {Assign, Operator, Assign, Increment, Decrement}},
    {TokenType::SignOperator, {Operator, Assign, Increment, Decrement}},
    {TokenType::Sign, {Operator, Assign, Increment, Decrement}},
    {TokenType::Assign, {Operator, Assign, Increment, Decrement}},
    {TokenType::Increment, {Operator, Assign, Increment, Decrement, LParen}},
    {TokenType::Decrement, {Operator, Assign, Increment, Decrement, LParen}},
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

// ========================================Variable Function Declarations========================================

vector<string> split_by_semicolon(const string& content);
variable convert_to_var(const token tk);
void print_var(const variable& var);

// ========================================Implementation========================================

class Lexer {
private:
    string text;
    size_t idx = 0;

    // 執行後取得一個token並將指標移到下一個token
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

            if (text[idx] == '.') {
                // If it starts with '.', treat as point
                num_str += text[idx];
                idx++;
                while (idx < text.length() && isdigit(text[idx])) {
                    num_str += text[idx];
                    idx++;
                }
                return {TokenType::Point, num_str};
            } else if (isdigit(text[idx])) {
                // If it starts with number
                while (idx < text.length() && isdigit(text[idx])) {
                    num_str += text[idx];
                    idx++;
                }
                return {TokenType::Number, num_str};
            }
        
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
                else if (c == '=') {idx += 2; return {TokenType::Operator, "="};}
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
    Lexer(const string& input) : text(input), idx(0) {}

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
                   && ident_table.find(cur_token.val) == ident_table.end()
                   && next_token.val != ":=") {
            throw runtime_error("> Undefined identifier : '" + cur_token.val + "'");
        }
        cur_token = lexer.get_next_token();
        return;
    }

    variable parse_factor() {
        // num, (), sign
        variable result;

        if (cur_token.val == "(") {
            next();
            // (is_return_bool) ? val = parse_bool_exp() : val = parse_exp();
            result = parse_exp();
            if (cur_token.val == ")") {
                next();
            } else {
                throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
            }
        
        // cout << "cur: " + cur_token.val + " | next: " + lexer.peek_token().val << endl;
        } else if (is_in(cur_token.val, {"+", "-", "!"})) {
            // 此版本中只有num錢可以接sign
            if (cur_token.val == "+") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
                }
                result = parse_factor();
            } else if (cur_token.val == "-") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
                }
                result = -parse_factor();
            } else if (cur_token.val == "!") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
                }
                result = !parse_factor();
            }
        } else {
            if (cur_token.type == TokenType::Number) {
                auto num_tk = cur_token;
                if (lexer.peek_token().type == TokenType::Point) {
                    next();
                    num_tk.val += cur_token.val;
                    auto next_token = lexer.peek_token();
                    // cout << next_token.type << " " << next_token.val << endl;
                    if (next_token.type == TokenType::Point) {
                        throw runtime_error("> Unexpected token : '" + next_token.val + "'");
                    }
                }
                result = convert_to_var(num_tk);
                next();
            
            } else if (cur_token.type == TokenType::Point) {
                auto num_tk = cur_token;
                auto next_token = lexer.peek_token();
                // cout << next_token.type << " " << next_token.val << endl;
                if (next_token.type == TokenType::Point) {
                    throw runtime_error("> Unexpected token : '" + next_token.val + "'");
                }
                result = convert_to_var(num_tk);
                next();

            } else if (cur_token.type == TokenType::Ident) {
                token id_token = cur_token;
                next(); // 通過測試
                // 取值
                result = ident_table[id_token.val];

            } else {
                if (symbols.find(cur_token.val) == symbols.end()) { // not found
                    throw runtime_error("> Unrecognized token with first char : '" + cur_token.val + "'");
                } else {
                    throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
                }
            }
        }
        return result;
    }

    variable parse_term() {
        // *, /
        variable result = parse_factor();
        // if (cur_token.type == TokenType::EndOfFile) return val;

        while (is_in(cur_token.val, {"*", "/"})) {
            if (cur_token.val == "*") {
                next();
                result = result * parse_factor();
            } 
            if (cur_token.val == "/") {
                next();
                auto a = parse_factor();
                // cout << result.val << " / " << a.val << endl;
                result = result / a;
                // if (stod(a.val) != 0) val = val / a;
                // else throw runtime_error("> Error");
            }
        }

        return result;
    }

    variable parse_exp() {
        // +, -
        variable val = parse_term();
        // if (cur_token.type == TokenType::EndOfFile) return val;
        
        // cout << "prev: " + prev_token.val + " | cur: " + cur_token.val << endl;
        while (is_in(cur_token.val, {"+", "-"})) {
            if (cur_token.val == "+") {
                next();
                val = val + parse_term();
            } 
            if (cur_token.val == "-") {
                next();
                val = val - parse_term();
            }
        }

        return val;
    }

    variable parse_relation_exp() {
        // < <= > >=
        variable result = parse_exp();
        if (cur_token.type == TokenType::EndOfFile) return result;
        
        if (is_in(cur_token.val, {"<=", ">=", "<", ">", "=", "<>"})) {
            if (cur_token.val == ">") {
                next();
                result = result > parse_exp();
            } else if (cur_token.val == "<") {
                next();
                result = result < parse_exp();
            } else if (cur_token.val == ">=") {
                next();
                result = result >= parse_exp();
            } else if (cur_token.val == "<=") {
                next();
                result = result <= parse_exp();
            } else if (cur_token.val == "=") {
                next();
                result = result == parse_exp();
            } else if (cur_token.val == "<>") {
                next();
                result = result != parse_exp();
            }
        }
        return result;
    } 
    // variable parse_equal_exp() {
    //     // == <>
    //     variable result = parse_relation_exp();
    //     if (cur_token.type == TokenType::EndOfFile) return result;
        
    //     while (is_in(cur_token.val, {"=", "<>"})) {
    //         if (cur_token.val == "=") {
    //             next();
    //             result = result == parse_relation_exp();
    //         } 
    //         if (cur_token.val == "<>") {
    //             next();
    //             result = result != parse_relation_exp();
    //         }
    //     }
    //     return result;
    // } 
    // variable parse_and_exp() {
    //     // &&
    //     variable result = parse_relation_exp();
    //     if (cur_token.type == TokenType::EndOfFile) return result;
        
    //     while (is_in(cur_token.val, {"&&"})) {
    //         if (cur_token.val == "&&") {
    //             next();
    //             result = result && parse_relation_exp();
    //         }
    //     }
    //     return result;
    // } 
    // variable parse_or_exp() {
    //     // ||
    //     variable result = parse_and_exp();
    //     if (cur_token.type == TokenType::EndOfFile) return result;
        
    //     while (is_in(cur_token.val, {"||"})) {
    //         if (cur_token.val == "||") {
    //             next();
    //             result = result || parse_and_exp();
    //         } 
    //     }
    //     return result;
    // } 

    variable parse_bool_exp() {
        return parse_relation_exp();
    } 

    variable parse_statement() {
        if (cur_token.type == TokenType::Ident && lexer.peek_token().val == ":=") {
            token id_token = cur_token; // 不論是否被定義都直接賦值
            next();
            next();
            variable result = parse_exp();
            ident_table[id_token.val] = result;
            // cout << id_token.val << " " << ident_table[id_token.val].val << endl;
            return result;
        } else {
           throw runtime_error("> Invalid statement");
        } 
    }

public:
    Parser(const string& input) : lexer(input) {
        cur_token = lexer.get_next_token();
    }

    bool is_eof() const {
        return cur_token.type == TokenType::EndOfFile;
    }

    bool is_quit() const {
        // 將quit直接當成ident 如果在cmd第一個時調用成功 則當作quit指令
        return cur_token.type == TokenType::Ident && cur_token.val == "quit";
    }

    string get_rest_str() {
        return lexer.get_rest_str();
    }

    void parse_cmd() {
        variable result;
        // statement | bool_exp | exp 
        // first token
        // statement
        if (cur_token.type == TokenType::Ident && lexer.peek_token().val == ":=") {
            result = parse_statement();

        } else {
            bool is_return_bool = false;
            auto tokens = lexer.traverse();
            for (int i = 0; i < tokens.size(); i++) {
                if (is_in(tokens[i].val, {"=", "<=", ">=", "<>", "&&", "||", "<", ">"})) {
                    is_return_bool = true;
                    break;
                }
            }
            // bool exp
            if (is_return_bool) {
                result = parse_bool_exp();
                if (result.type != DataType::Bool) throw runtime_error("Something wrong in parse_cmd()");
            // exp
            } else {
                result = parse_exp();
            }
        }

        if (cur_token.type != TokenType::Semicolon) {
            if (is_in(cur_token.val, symbols)){
                throw runtime_error("> Unexpected token : '" + cur_token.val + "'");
            } else {
                throw runtime_error("> Unrecognized token with first char : '" + cur_token.val + "'");
            }
        }
        print_var(result);
        return;
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
        if (trimmed.length() > 0) exps.push_back(trimmed);
        str = "";
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
    if (tk.type == TokenType::Number) {
        for (int i = 0; i < tk.val.length(); i++) {
            if (tk.val[i] == '.') {
                return {DataType::Float, tk.val};
            }
        }
        return {DataType::Int, tk.val};
    } else if (tk.type == TokenType::Point) {
        return {DataType::Float, "0" + tk.val};
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

string num_to_string(double num) {
    stringstream ss;
    ss << setprecision(15) << num;
    return ss.str();
}

bool is_float(double num) {
    // 可能有點問題
    // cout << num << " " << floor(num) << endl;
    return (abs(num - floor(num)) > ErrorValue);
}

bool is_in(const string& str, const unordered_set<string> targets) {
    return targets.find(str) != targets.end();
}

bool parse_wrapper(string cmd) {
    // 輸出為 true 代表「目前錯誤尚未找到換行能恢復」
    Parser parser(cmd);
    try {
        if (!parser.is_eof()) {
            parser.parse_cmd();
        }
        return false;

    } catch (const std::exception& e) {
        cout << e.what() << std::endl;
        string rest_str = parser.get_rest_str();
        auto start = rest_str.find_first_of("\n");
        if (rest_str.length() > 0 && start != string::npos) {
            return parse_wrapper(rest_str.substr(start));
        }
        return true;
    }
}

void print_var(const variable& var) {
    if (var.type == DataType::Int) {
        cout << "> " << stoi(var.val) << endl;
    } else if (var.type == DataType::Float) {
        cout << fixed << setprecision(3) << "> " << stod(var.val) << endl;
    } else {
        cout << "> " << var.val << endl;
    }
}

int main() {
    // ios::sync_with_stdio(false);
    // cin.tie(0);
    std::cout << std::fixed << std::setprecision(3);
    cout << "Program starts..." << endl;

    // ifstream file("test/data.txt"); // 本機測試
    // stringstream ss;
    // ss << file.rdbuf(); // 將整個檔案緩衝區讀入 stringstream
    // string content_ = ss.str();
    // auto start = content_.find_first_of("\n") + 1, end = content_.length();
    // string content = content_.substr(start, end - start + 1);

    string content, _; // 上傳測試
    cin >> _; // 去除題號

    char c;
    while (cin.get(c)) {
        content += c;
    }

    auto cmds = split_by_semicolon(content);

    bool skip_until_newline = false;
    for (int i = 0; i < cmds.size(); i++) {
        if (skip_until_newline) {
            auto pos = cmds[i].find_first_of("\n");
            if (pos != string::npos) {
                Parser p(cmds[i].substr(pos));
                if (p.is_quit()) break;
                skip_until_newline = parse_wrapper(cmds[i].substr(pos));
            }
        } else {
            Parser p(cmds[i]);
            if (p.is_quit()) break;
            skip_until_newline = parse_wrapper(cmds[i]);
        }
    }

    cout << "> Program exits..." << endl;
    return 0;
}