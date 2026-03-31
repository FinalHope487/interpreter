#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <functional>
// #include <iterator>
// #include <map>
// #include <optional>
// #include <variant>

using namespace std;
// using Literal = variant<int, double, string, bool>;

// ========================================definition========================================

const string WHITESPACE = " \t\r";
const double ErrorValue = 1e-4;

struct token;
struct variable;
struct func_param;
struct func;

class Lexer;
class Parser;

bool is_float(double num);
bool is_in(const string& op, const unordered_set<string>& targets);
bool is_in(const string& op, const unordered_map<string, func>& targets);
bool is_in(const string& op, const unordered_map<string, variable>& targets);
string trim(const string& s);
string num_to_string(double num);
double string_to_num(string s);
variable convert_to_var(const token tk);
void print_var(const variable& var);
void print_ident_table(const unordered_map<string, variable>& ident_table);
void print_func_table(const unordered_map<string, func>& func_table);
unordered_map<string, variable> format_params(const vector<func_param>& params, const vector<variable>& args);

// 將這些函數加入map中
void ListAllVariables(const vector<variable>& variables); // just the names of the (global) variables, sorted (from smallest to greatest)
void ListAllFunctions(const vector<variable>& functions); // just the names of the (user-defined) functions, sorted
void ListVariable(const vector<variable>& variables); // the definition of a particular variable
void ListFunction(const vector<variable>& functions); // the definition of a particular function
void Done(); // exit the interpreter

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
    LBracket,
    RBracket,
    LBrace,
    RBrace,
    Increment,
    Decrement,
    IO,
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

enum States {
    Definition,
    NewDefinition,
    Statement,
};

struct ReturnState {
    States state;
    string var_name;
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
        case TokenType::LBracket: return "LBracket";
        case TokenType::RBracket: return "RBracket";
        case TokenType::LBrace: return "LBrace";
        case TokenType::RBrace: return "RBrace";
        case TokenType::Increment: return "Increment";
        case TokenType::Decrement: return "Decrement";
        case TokenType::IO: return "IO";
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

DataType DataType_to_enum(string type) {
    if (type == "int") return DataType::Int;
    if (type == "float") return DataType::Float;
    if (type == "char") return DataType::Char;
    if (type == "string") return DataType::String;
    if (type == "bool") return DataType::Bool;
    return DataType::Void;
}

// ========================================Structs Definition========================================

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
        else if (var1.type == DataType::Int || var1.type == DataType::Float) n1 = string_to_num(var1.val);
        else if (var1.type == DataType::Bool) n1 = var1.val == "false" ? 0 : 1;
        else throw runtime_error("Error in bool_evaluate with values: " + var1.val + " " + op + " " + var2.val);

        if (var2.type == DataType::Char && var2.val.length() == 1) n2 = (int)(var2.val[0]);
        else if (var2.type == DataType::Int || var2.type == DataType::Float) n2 = string_to_num(var2.val);
        else if (var2.type == DataType::Bool) n2 = var2.val == "false" ? 0 : 1;
        else throw runtime_error("Error in bool_evaluate with values: " + var1.val + " " + op + " " + var2.val);
        
        if (op == "==") {
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
            return {this->type, num_to_string(-string_to_num(this->val))};
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
            result = num_to_string(string_to_num(this->val) + string_to_num(var2.val));

        } else if ((this->type == DataType::Float && var2.type == DataType::Float) 
                   || (this->type == DataType::Int && var2.type == DataType::Float) 
                   || (this->type == DataType::Float && var2.type == DataType::Int)) {
            rtype = DataType::Float;
            result = num_to_string(string_to_num(this->val) + string_to_num(var2.val));

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
            result = num_to_string(string_to_num(this->val) - string_to_num(var2.val));

        } else if ((this->type == DataType::Float && var2.type == DataType::Float) 
                   || (this->type == DataType::Int && var2.type == DataType::Float) 
                   || (this->type == DataType::Float && var2.type == DataType::Int)) {
            rtype = DataType::Float;
            result = num_to_string(string_to_num(this->val) - string_to_num(var2.val));

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
            result = num_to_string(string_to_num(this->val) * string_to_num(var2.val));

        } else if ((this->type == DataType::Float && var2.type == DataType::Float) 
                   || (this->type == DataType::Int && var2.type == DataType::Float) 
                   || (this->type == DataType::Float && var2.type == DataType::Int)) {
            rtype = DataType::Float;
            result = num_to_string(string_to_num(this->val) * string_to_num(var2.val));

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
            auto tmp = string_to_num(this->val) / string_to_num(var2.val);
            // cout << tmp << endl;
            if (is_float(tmp)) rtype = DataType::Float;
            else rtype = DataType::Int;
            result = num_to_string(tmp);

        } else if ((this->type == DataType::Float || this->type == DataType::Float) 
                   || (this->type == DataType::Int && var2.type == DataType::Float) 
                   || (this->type == DataType::Float && var2.type == DataType::Int)) {
            if (var2.val == "0" || var2.val == "0.0") {
                throw runtime_error("> Error in operator/: division by zero");
            }
            rtype = DataType::Float;
            result = num_to_string(string_to_num(this->val) / string_to_num(var2.val));

        } else {
            throw runtime_error("Error in operator/");
        }

        return {rtype, result};
    }

    variable operator% (const variable& var2) {
        if (this->type != DataType::Int || var2.type != DataType::Int) {
            throw runtime_error("Error in operator%: operands must be integers");
        }
        if (var2.val == "0") {
            throw runtime_error("Error in operator%: division by zero");
        }
        int result = stoi(this->val) % stoi(var2.val);
        return {DataType::Int, to_string(result)};
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

    variable operator+=(const variable& var2) {
        return *this = *this + var2;
    }

    variable operator-=(const variable& var2) {
        return *this = *this - var2;
    }

    variable operator*=(const variable& var2) {
        return *this = *this * var2;
    }

    variable operator/=(const variable& var2) {
        return *this = *this / var2;
    }

    variable operator%=(const variable& var2) {
        return *this = *this % var2;
    }
};  

struct func_param {
    DataType type;
    string name;
};

struct func {
    DataType return_type;
    vector<func_param> params;
    string content; // 含外層大括號
};

// 變數名: (資料型態, 變數值)
unordered_map<string, variable> ident_table;
// struct func {
//     DataType return_type;
//     std::vector<std::pair<DataType, std::string>> params; // 假設參數是這樣定義的
//     std::string description;

//     // 定義構造函數
//     func(DataType rt, std::vector<std::pair<DataType, std::string>> p, std::string desc)
//         : return_type(rt), params(std::move(p)), description(std::move(desc)) {}
// };

// 初始化時
std::unordered_map<std::string, func> func_table = {
    {"ListAllVariables", func({DataType::Void, {}, ""})},
    {"ListAllFunctions", func({DataType::Void, {}, ""})},
    {"ListVariable",     func({DataType::Void, {{DataType::String, "name"}}, ""})},
    {"ListFunction",     func({DataType::Void, {{DataType::String, "name"}}, ""})},
    {"Done",             func({DataType::Void, {}, ""})}
};
// =, +=, -=, *=, /=, %=, ? :, &&, ||, !, ==, !=, <, >, <=, >=, <<, >>, +, -, *, /, %
const unordered_set<string> symbols = {
    "=", "+=", "-=", "*=", "/=", "%=", 
    "?", ":", "&&", "||", "!", "==", "!=", "<", ">", "<=", ">=", "<<", ">>", 
    "+", "-", "*", "/", "%", "(", ")", ",", ";", "[", "]", "{", "}", "\"", "'"
};

const unordered_set<string> data_types = {
    "int", "float", "char", "bool", "string", "void"
};

const unordered_set<string> keywords = ([] {
    unordered_set<string> combined = {"cin", "cout", "ListAllVariables", "ListAllFunctions", 
        "ListVariable", "ListFunction", "Done", "if", "else", "while", "for", "return", "break", "continue"
    };
    combined.insert(data_types.begin(), data_types.end());
    return combined;
}());

// unexpected next token types
unordered_map<TokenType, vector<TokenType>> unexpected_types = {
    {TokenType::Number, {Sign, Assign, Ident, LParen}},
    {TokenType::Point, {Sign, Assign, Ident, LParen}},
    {TokenType::Ident, {Sign}},
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
    {TokenType::LBracket, {Operator, Assign}},
    {TokenType::RBracket, {Sign}},
    {TokenType::LBrace, {Operator, Assign}},
    {TokenType::RBrace, {Sign, Assign, Increment, Decrement}},
    {TokenType::Semicolon, {}},
    {TokenType::EndOfFile, {}},
    {TokenType::Null, {}},
    {TokenType::Undefined, {}},
};

// ========================================Function Definition========================================

// const string& 傳引用(保護正本) const string 傳值(會複製一份副本且保護副本)
string trim(const string& s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    if (start == string::npos) return ""; 
    size_t end = s.find_last_not_of(WHITESPACE);
    return s.substr(start, end - start + 1); // 起點、長度
}

string num_to_string(double num) {
    stringstream ss;
    ss << setprecision(15) << num;
    return ss.str();
}

double string_to_num(string s) {
    try {
        return stod(s);
    } catch (const std::invalid_argument& ia) {
        throw runtime_error("Error in string_to_num with value: " + s);
    } catch (const std::out_of_range& oor) {
        throw runtime_error("Error in string_to_num with value: " + s);
    }
}

bool is_float(double num) {
    // 可能有點問題
    // cout << num << " " << floor(num) << endl;
    return (abs(num - floor(num)) > ErrorValue);
}

bool is_in(const string& str, const unordered_set<string>& targets) {
    return targets.find(str) != targets.end();
}

bool is_in(const string& str, const unordered_map<string, func>& targets) {
    return targets.find(str) != targets.end();
}

bool is_in(const string& str, const unordered_map<string, variable>& targets) {
    return targets.find(str) != targets.end();
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

void print_var(const variable& var) {
    if (var.type == DataType::Int) {
        cout << "> " << stoi(var.val) << endl;
    } else if (var.type == DataType::Float) {
        cout << fixed << setprecision(3) << "> " << string_to_num(var.val) << endl;
    } else {
        cout << "> " << var.val << endl;
    }
}

void print_ident_table(const unordered_map<string, variable>& ident_table) {
    cout << "Ident table: " << endl;
    for (auto const& [key, val] : ident_table) {
        cout << key << " : " << val.val << endl;
    }
}

void print_func_table(const unordered_map<string, func>& func_table) {
    cout << "Function table: " << endl;
    for (auto const& [key, val] : func_table) {
        cout << key << endl;
    }
}

unordered_map<string, variable> format_params(const vector<func_param>& params, const vector<variable>& args) {
    unordered_map<string, variable> formatted_params;
    if (params.size() != args.size()) {
        throw runtime_error("> Invalid function call: expected " + to_string(params.size()) + " arguments, got " + to_string(args.size()));
    }
    for (int i = 0; i < params.size(); i++) {
        if (params[i].type != args[i].type) {
            throw runtime_error("> Invalid function call: expected " + enum_to_DataType(params[i].type) + " arguments, got " + enum_to_DataType(args[i].type));
        }
        formatted_params[params[i].name] = args[i];
    }
    return formatted_params;
}

// ========================================Implementation========================================

class Lexer {
private:
    string text;
    size_t idx = 0;
    vector<int> checkpoints;

    // 執行後取得一個token並將指標移到下一個token
    token get_a_token(int skip_tokens = 1) {
        token tk;
        for (int i = 0; i < skip_tokens; i++) {
            while (idx < text.length()) {
                if (isspace(text[idx])) {
                    idx++;
                } else if (idx + 1 < text.length() && text[idx] == '/' && text[idx + 1] == '/') {
                    while (idx < text.length() && text[idx] != '\n') {
                        idx++;
                    }
                } else {
                    break;
                }
            }
            if (idx >= text.length()) tk = {TokenType::EndOfFile, ""};

            // char
            if (text[idx] == '\'') {
                string chr_str;
                idx++;
                // 此時檢查斜線後是否有東西
                if (idx < text.length() && text[idx] == '\\') {
                    chr_str += '\\'; idx++;
                    if (idx < text.length()) {
                        chr_str += text[idx]; idx++;
                    } else {
                        throw runtime_error("> Invalid char: missing closing quote");
                    }
                } else {
                    chr_str += text[idx]; idx++;
                }
                if (text[idx] != '\'') throw runtime_error("> Invalid char: missing closing quote");
                idx++;
                tk = {TokenType::Chr, chr_str};

            // string
            } else if (text[idx] == '"') {
                string str_str;
                idx++;
                while (text[idx] != '"') {
                    if (idx >= text.length()) throw runtime_error("> Invalid string: missing closing quote");
                    if (text[idx] == '\\') {
                        str_str += '\\'; idx++;
                        if (idx < text.length()) {
                            str_str += text[idx]; idx++;
                        }
                    } else {
                        str_str += text[idx]; idx++;
                    }
                }
                if (text[idx] != '"') throw runtime_error("> Invalid string: missing closing quote");
                idx++;
                // cout << str_str << endl;
                tk = {TokenType::Str, str_str};

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
                    tk = {TokenType::Point, num_str};
                } else if (isdigit(text[idx])) {
                    // If it starts with number
                    while (idx < text.length() && isdigit(text[idx])) {
                        num_str += text[idx];
                        idx++;
                    }
                    tk = {TokenType::Number, num_str};
                }
            
            // boolean
            } else if (text.compare(idx, 4, "true") == 0) {
                idx += 4;
                tk = {TokenType::Boolean, "true"};

            } else if (text.compare(idx, 5, "false") == 0) {
                idx += 5;
                tk = {TokenType::Boolean, "false"};

            // ident
            } else if (isalpha(text[idx]) || text[idx] == '_') {
                string ident_str;
                while (idx < text.length() && (isalnum(text[idx]) || text[idx] == '_')) {
                    ident_str += text[idx];
                    idx++;
                }
                // 處理bool
                tk = {TokenType::Ident, ident_str};

            // operators
            // 需要大幅修改邏輯
            } else {
                // 長度為1或2 Assign Operator Sign LParen RParen
                // 特別處理長度為2的運算符即可 剩下歸類於長度為1
                if (idx + 1 < text.length()) {
                    string s = string("") + text[idx] + text[idx + 1];

                    if (s == "+=") {idx += 2; tk = {TokenType::Assign, "+="}; continue;}
                    else if (s == "-=") {idx += 2; tk = {TokenType::Assign, "-="}; continue;}
                    else if (s == "*=") {idx += 2; tk = {TokenType::Assign, "*="}; continue;}
                    else if (s == "/=") {idx += 2; tk = {TokenType::Assign, "/="}; continue;}
                    else if (s == "==") {idx += 2; tk = {TokenType::Operator, "=="}; continue;}
                    else if (s == ">=") {idx += 2; tk = {TokenType::Operator, ">="}; continue;}
                    else if (s == "<=") {idx += 2; tk = {TokenType::Operator, "<="}; continue;}
                    else if (s == "!=") {idx += 2; tk = {TokenType::Operator, "!="}; continue;}
                    else if (s == "&&") {idx += 2; tk = {TokenType::Operator, "&&"}; continue;}
                    else if (s == "||") {idx += 2; tk = {TokenType::Operator, "||"}; continue;}
                    else if (s == "++") {idx += 2; tk = {TokenType::Increment, "++"}; continue;}
                    else if (s == "--") {idx += 2; tk = {TokenType::Decrement, "--"}; continue;}
                    else if (s == "<<") {idx += 2; tk = {TokenType::IO, "<<"}; continue;}
                    else if (s == ">>") {idx += 2; tk = {TokenType::IO, ">>"}; continue;}
                } 
                if (idx < text.length()) { 
                    char c = text[idx];
                    // 給parser解析 +- sign/operator
                    if (c == '+') {idx += 1; tk = {TokenType::SignOperator, "+"}; continue;}
                    else if (c == '-') {idx += 1; tk = {TokenType::SignOperator, "-"}; continue;}
                    else if (c == '*') {idx += 1; tk = {TokenType::Operator, "*"}; continue;}
                    else if (c == '/') {idx += 1; tk = {TokenType::Operator, "/"}; continue;}
                    else if (c == '=') {idx += 1; tk = {TokenType::Operator, "="}; continue;}
                    else if (c == '>') {idx += 1; tk = {TokenType::Operator, ">"}; continue;}
                    else if (c == '<') {idx += 1; tk = {TokenType::Operator, "<"}; continue;}
                    else if (c == '!') {idx += 1; tk = {TokenType::Sign, "!"}; continue;}
                    else if (c == '(') {idx += 1; tk = {TokenType::LParen, "("}; continue;}
                    else if (c == ')') {idx += 1; tk = {TokenType::RParen, ")"}; continue;}
                    else if (c == ';') {idx += 1; tk = {TokenType::Semicolon, ";"}; continue;}
                    else if (c == '[') {idx += 1; return {TokenType::LBracket, "["};}   
                    else if (c == ']') {idx += 1; return {TokenType::RBracket, "]"};}
                    else if (c == '{') {idx += 1; return {TokenType::LBrace, "{"};}
                    else if (c == '}') {idx += 1; return {TokenType::RBrace, "}"};}
                    else {idx += 1; tk = {TokenType::Undefined, string("") + c}; continue;} // 返回未定義token

                } else {
                    string s;
                    while (!strchr("+-*/><()[]{}=", text[idx])) {
                        s += text[idx];
                        idx++;
                    }
                    tk = {TokenType::Undefined, s};
                }
            }
        }
        
        return tk;
    }

public:
    Lexer(const string& input) : text(input), idx(0) {}

    token get_next_token(int skip_tokens = 1) {
        // get token 並改變idx
        token tk = get_a_token(skip_tokens);
        // cout << "tk: " << tk.val << endl;
        return tk;
    }

    token peek_token(int skip_tokens = 1) {
        // get token 但不改變idx
        int start_idx = idx;
        token tk = get_a_token(skip_tokens);
        idx = start_idx;
        return tk;
    }

    vector<token> traverse() {
        // get tokens 但不改變idx
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

    void skip_to_newline() {
        while (idx < text.length() && text[idx] != '\n') {
            idx++;
        }
        if (idx < text.length() && text[idx] == '\n') {
            idx++;
        }
    }

    void skip_a_block() {
        token tk = peek_token();
        if (tk.val == "{") {
            int brace_count = 0;
            while (idx < text.length()) {
                if (text[idx] == '{') brace_count++;
                else if (text[idx] == '}') brace_count--;
                if (brace_count == 0) break;
                idx++;
            }
        } else {
            throw runtime_error("> Expected '{' but got '" + tk.val + "'");
        }
    }

    string get_a_block() {
        token tk = peek_token();
        if (tk.val == "{") {
            int brace_count = 0;
            string block_str;
            idx++;
            while (idx < text.length()) {
                if (text[idx] == '{') brace_count++;
                else if (text[idx] == '}') brace_count--;
                block_str += text[idx];
                if (brace_count == 0) break;
                idx++;
            }
            return block_str;
        } else {
            throw runtime_error("> Expected '{' but got '" + tk.val + "'");
        }
    }

    string pretty_print_block() {
        token tk = peek_token();
        if (tk.val == "{") {
            int brace_count = 0;
            string block_str;
            idx++;
            while (idx < text.length()) {
                if (text[idx] == '{') brace_count++;
                else if (text[idx] == '}') brace_count--;
                for (int i = 0; i < brace_count; i++) {
                    block_str += "    ";
                }
                block_str += text[idx];
                if (brace_count == 0) break;
                idx++;
            }
            return block_str;
        } else {
            throw runtime_error("> Expected '{' but got '" + tk.val + "'");
        }        
    }

    void push_checkpoint() {
        checkpoints.push_back(idx);
    }

    void pop_checkpoint() {
        checkpoints.pop_back();
    }

    void back_to_checkpoint() {
        idx = checkpoints.back();
    }
};

// for nums (因為函數宣告問題)
class Parser {
private:
    Lexer lexer;
    token cur_token;

    void next(int skip_tokens = 1) {
        // 如果 "下一個" 不是預期的token就根據問題丟出報錯 只處理報錯
        auto ue_types = unexpected_types[cur_token.type];
        token next_token = lexer.peek_token(skip_tokens);

        // 符號未定義
        if (cur_token.type == TokenType::Undefined) {
            throw runtime_error("> 1Unrecognized token with first char : '" + cur_token.val + "'");

        // 後接非法符號
        } else if (find(ue_types.begin(), ue_types.end(), 
                        next_token.type) != ue_types.end()) {
            throw runtime_error("> 1Unexpected token : '" + cur_token.val + "' -> '" + next_token.val + "'");

        // ident未定義且後面非賦值
        } else if (cur_token.type == TokenType::Ident 
                   && ident_table.find(cur_token.val) == ident_table.end()
                //    && next_token.val != "="
                   && keywords.find(cur_token.val) == keywords.end()) {
            throw runtime_error("> Undefined identifier : '" + cur_token.val + "'");
        }
        cur_token = lexer.get_next_token(skip_tokens);
        return;
    }

    variable parse_factor() {
        // num, (), sign
        variable result;
        token next_token = lexer.peek_token();
        if (cur_token.val == "(") {
            next();
            // (is_return_bool) ? val = parse_bool_exp() : val = parse_exp();
            result = parse_exp();
            if (cur_token.val == ")") {
                next();
            } else {
                throw runtime_error("> 2Unexpected token : '" + cur_token.val + "'");
            }
        
        // cout << "cur: " + cur_token.val + " | next: " + lexer.peek_token().val << endl;
        } else if (is_in(cur_token.val, {"+", "-", "!"})) {
            if (cur_token.val == "++" && next_token.type == TokenType::Ident) {
                // ++a
                next();
                result = parse_factor() + variable{result.type, "1"};
            } else if (cur_token.val == "--" && next_token.type == TokenType::Ident) {
                // --a
                next();
                result = parse_factor() - variable{result.type, "1"};
            } else if (cur_token.val == "+") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("> 3Unexpected token : '" + cur_token.val + "'");
                }
                result = parse_factor();
            } else if (cur_token.val == "-") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("> 4Unexpected token : '" + cur_token.val + "'");
                }
                result = -parse_factor();
            } else if (cur_token.val == "!") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("> 5Unexpected token : '" + cur_token.val + "'");
                }
                result = !parse_factor();
            }
        } else if (cur_token.type == TokenType::Number) {
            auto num_tk = cur_token;
            if (lexer.peek_token().type == TokenType::Point) {
                next();
                num_tk.val += cur_token.val;
                auto next_token = lexer.peek_token();
                // cout << next_token.type << " " << next_token.val << endl;
                if (next_token.type == TokenType::Point) {
                    throw runtime_error("> 6Unexpected token : '" + next_token.val + "'");
                }
            }
            result = convert_to_var(num_tk);
            next();
            
        } else if (cur_token.type == TokenType::Point) {
            auto num_tk = cur_token;
            auto next_token = lexer.peek_token();
            // cout << next_token.type << " " << next_token.val << endl;
            if (next_token.type == TokenType::Point) {
                throw runtime_error("> 7Unexpected token : '" + next_token.val + "'");
            }
            result = convert_to_var(num_tk);
            next();

        } else if (cur_token.type == TokenType::Ident) {
            token id_token = cur_token;
            next(); // 通過測試
            // 取值
            // 需要錯誤處理
            result = ident_table[id_token.val];
            // cout << "result: " << id_token.val << " | " << result.val << endl;
            if (cur_token.val == "++") {
                ident_table[id_token.val] = ident_table[id_token.val] + variable{result.type, "1"};
                next();
            } else if (cur_token.val == "--") {
                ident_table[id_token.val] = ident_table[id_token.val] - variable{result.type, "1"};
                next();
            }
            
        } else if (cur_token.type == TokenType::Chr) {
            result = variable{DataType::Char, cur_token.val};
            next();

        } else if (cur_token.type == TokenType::Str) {
            result = variable{DataType::String, cur_token.val};
            next();

        } else if (cur_token.type == TokenType::Semicolon) {
            return variable{DataType::Void, ""};

        } else {
            if (symbols.find(cur_token.val) == symbols.end()) { // not found
                throw runtime_error("> 2Unrecognized token with first char : '" + cur_token.val + "'");
            } else {
                throw runtime_error("> 8Unexpected token : '" + cur_token.val + "'");
            }
        }
        return result;
    }

    variable parse_term() {
        // *, /
        variable result = parse_factor();
        // if (cur_token.type == TokenType::EndOfFile) return val;

        while (is_in(cur_token.val, unordered_set<string>{"*", "/"})) {
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
        variable result = parse_term();
        // if (cur_token.type == TokenType::EndOfFile) return val;
        
        // cout << "prev: " + prev_token.val + " | cur: " + cur_token.val << endl;
        while (is_in(cur_token.val, unordered_set<string>{"+", "-"})) {
            if (cur_token.val == "+") {
                next();
                result = result + parse_term();
            } 
            if (cur_token.val == "-") {
                next();
                result = result - parse_term();
            }
        }

        return result;
    }

    variable parse_relation_exp() {
        // < <= > >=
        variable result = parse_exp();
        if (cur_token.type == TokenType::EndOfFile) return result;
        
        static const unordered_map<string, function<variable(variable, variable)>> op_map = {
            {">", [](variable a, variable b) { return a > b; }},
            {"<", [](variable a, variable b) { return a < b; }},
            {">=", [](variable a, variable b) { return a >= b; }},
            {"<=", [](variable a, variable b) { return a <= b; }},
            {"==", [](variable a, variable b) { return a == b; }},
            {"!=", [](variable a, variable b) { return a != b; }}
        };
        auto it = op_map.find(cur_token.val);
        if (it != op_map.end()) {
            next();
            return it->second(result, parse_exp());
        } 
        return result;
    } 
    variable parse_equal_exp() {
        // == <>
        variable result = parse_relation_exp();
        if (cur_token.type == TokenType::EndOfFile) return result;
        
        while (is_in(cur_token.val, unordered_set<string>{"==", "!="})) {
            if (cur_token.val == "==") {
                next();
                result = result == parse_relation_exp();
            } 
            if (cur_token.val == "!=") {
                next();
                result = result != parse_relation_exp();
            }
        }
        return result;
    } 
    variable parse_and_exp() {
        // &&
        variable result = parse_relation_exp();
        if (cur_token.type == TokenType::EndOfFile) return result;
        
        while (is_in(cur_token.val, {"&&"})) {
            if (cur_token.val == "&&") {
                next();
                result = result && parse_relation_exp();
            }
        }
        return result;
    } 
    variable parse_or_exp() {
        // ||
        variable result = parse_and_exp();
        if (cur_token.type == TokenType::EndOfFile) return result;
        
        while (is_in(cur_token.val, {"||"})) {
            if (cur_token.val == "||") {
                next();
                result = result || parse_and_exp();
            } 
        }
        return result;
    } 

    variable parse_bool_exp() {
        return parse_or_exp();
    } 

    variable parse_io(const string& type) {
        // cin >> ident >> ident ...
        // cout << bool_exp | exp << bool_exp | exp ...
        variable result;
        if (type == "cin") {
            next();
            if (cur_token.val != ">>") {
                throw runtime_error("> Invalid IO statement: expected >>, got " + cur_token.val);
            }

            while (cur_token.val == ">>") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    if (ident_table.find(cur_token.val) == ident_table.end()) {
                        throw runtime_error("> Invalid IO statement: identifier not found");
                    }
                    cin >> ident_table[cur_token.val].val;
                    next();
                } else {
                    throw runtime_error("> Invalid IO statement: unexpected token");
                }
            }
            // cout << "> Statement executed ..." << endl;
            result = {DataType::Int, ""};

        } else if (type == "cout") {
            next();
            if (cur_token.val != "<<") {
                throw runtime_error("> Invalid IO statement: expected <<, got " + cur_token.val);
            }
            while (cur_token.val == "<<") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    if (ident_table.find(cur_token.val) == ident_table.end()) {
                        throw runtime_error("> Invalid IO statement: identifier not found");
                    }
                    // cout << ident_table[cur_token.val].val;
                    next();
                } else {
                    result = parse_exp();
                    // cout << result.val;
                }
            }
            // cout << "> Statement executed ..." << endl;
        } else {
            throw runtime_error("> Invalid IO statement, got " + type);
        }
        return result;
    }

    bool parse_condition() {
        if (cur_token.val != "(") {
            throw runtime_error("> Invalid if statement: expected (, got " + cur_token.val);
        }
        next();
        variable result = parse_bool_exp();
        if (cur_token.val != ")") {
            throw runtime_error("> Invalid if statement: expected ), got " + cur_token.val);
        }
        next();
        return bool(result);
    }

    void parse_if_else() {
        bool condition;
        if (cur_token.val == "if") {
            next();
            condition = parse_condition();
            if (condition) parse_statement();
            else lexer.skip_a_block();
        } else if (cur_token.val == "else" && lexer.peek_token().val == "if") {
            next();
            next();
            condition = parse_condition();
            if (condition) parse_statement();
            else lexer.skip_a_block();
        } else if (cur_token.val == "else") {
            next();
            parse_statement();
        } else {
            throw runtime_error("> Invalid if statement: expected if or else, got " + cur_token.val);
        }
    }

    void parse_while() {
        bool condition;
        if (cur_token.val == "while") {
            next();
            condition = parse_condition(); // ( condition )
            lexer.push_checkpoint();
            while (condition) {
                parse_statement(); // block
                lexer.back_to_checkpoint(); 
                next(); // while
                condition = parse_condition();
            }
            lexer.pop_checkpoint();
        } else {
            throw runtime_error("> Invalid while statement: expected while, got " + cur_token.val);
        }
    }

    void parse_block() {
        // <Block> ::= "{" <Statement> "}" | "{" "}"
        // 處理 block statement
        if (cur_token.val == "{") {
            next();
            while (cur_token.val != "}") {
                parse_statement();
            }
            next();
        }
    }

    ReturnState parse_variable_declaration() {
        // cur_token is type
        DataType type = DataType_to_enum(cur_token.val);
        States state = States::Definition;
        string name = lexer.peek_token().val;
        next(2);
        if (cur_token.val != ";") {
            throw runtime_error("> Invalid variable declaration: expected ;, got " + cur_token.val);
        }
        if (ident_table.find(name) != ident_table.end()) {
            state = States::NewDefinition;
        }
        if (type == DataType::String) {
            ident_table[name] = {type, ""};
        } else if (type == DataType::Char) {
            ident_table[name] = {type, "\0"};
        } else if (type == DataType::Bool) {
            ident_table[name] = {type, "false"};
        } else if (type == DataType::Int) {
            ident_table[name] = {type, "0"};
        } else if (type == DataType::Float) {
            ident_table[name] = {type, "0.0"};
        } else {
            throw runtime_error("> Invalid variable declaration: expected type, got " + cur_token.val);
        }
        return {state, name};
    }

    vector<func_param> parse_function_declaration_params() {
        // <Params> ::= ( <Type> <Ident> { , <Type> <Ident> } | <Empty> )
        // 處理宣告時的參數
        vector<func_param> params;
        if (cur_token.val == "(") {
            next();
            // 無參數
            if (cur_token.val == ")") {
                next();
                return params;
            // 有參數宣告
            } else {
                if (data_types.find(cur_token.val) != data_types.end()) {
                    DataType type = DataType_to_enum(cur_token.val);
                    next();
                    if (cur_token.type != TokenType::Ident) {
                        throw runtime_error("> Invalid function parameters in declaration: expected identifier, got " + cur_token.val);
                    }
                    params.push_back({type, cur_token.val});
                    next();
                    while (cur_token.val == ",") {
                        next();
                        if (data_types.find(cur_token.val) != data_types.end()) {
                            DataType type = DataType_to_enum(cur_token.val);
                            next();
                            if (cur_token.type != TokenType::Ident) {
                                throw runtime_error("> Invalid function parameters in declaration: expected identifier, got " + cur_token.val);
                            }
                            params.push_back({type, cur_token.val});
                            next();
                        } else {
                            throw runtime_error("> Invalid function parameters in declaration: expected type, got " + cur_token.val);
                        }
                    }
                } else {
                    throw runtime_error("> Invalid function parameters in declaration: expected type, got " + cur_token.val);
                }
            }
        } else {
            throw runtime_error("> Invalid function parameters in declaration: expected (, got " + cur_token.val);
        }
        return params;
    }

    // 寫到文法符合放著去看spec
    vector<variable> parse_function_params() {
        // <Params> ::= ( <Ident> { , <Ident> } | <Empty> )
        // 只處理調用時的參數 用於準備輸入函數
        vector<variable> params;
        if (cur_token.val == "(") {
            next();
            // 無參數
            if (cur_token.val == ")") {
                next();
                return params;
            // 有參數調用
            } else {
                params.push_back(parse_bool_exp());
                while (cur_token.val == ",") {
                    next();
                    params.push_back(parse_bool_exp());
                }
                if (cur_token.val != ")") {
                    throw runtime_error("> Invalid function parameters in call: expected ), got " + cur_token.val);
                }
                next();
            }
        } else {
            throw runtime_error("> Invalid function parameters in call: expected (, got " + cur_token.val);
        }
        return params;
    }

    ReturnState parse_function_declaration() {
        // <Function> ::= <Type> <Ident> "(" <Params> {"," <Params>} ")" (<Block> | <Statement>) | <BuiltInFunction>
        DataType type = DataType_to_enum(cur_token.val);
        next();
        if (cur_token.type != TokenType::Ident || keywords.find(cur_token.val) != keywords.end()) {
            throw runtime_error("> Invalid function declaration: expected identifier, got " + cur_token.val);
        }
        States state = States::Definition;
        string name = cur_token.val;
        next();
        if (func_table.find(name) != func_table.end()) {
            state = States::NewDefinition;
        }
        vector<func_param> params = parse_function_declaration_params();
        string block_str = lexer.get_a_block();
        func_table[name] = {type, params, block_str};
        return {state, name};
    }

    void parse_function_call() {
        // <Function> ::= <Type> <Ident> "(" <Params> {"," <Params>} ")" (<Block> | <Statement>) | <BuiltInFunction>
        // <BuiltInFunction> ::= "ListAllVariables" | "ListAllFunctions" | "ListVariable" | "ListFunction" | "Done"
        string function_name = cur_token.val;
        next();
        vector<variable> params = parse_function_params();
        if (function_name == "ListAllVariables") {
            ListAllVariables(params);
        } else if (function_name == "ListAllFunctions") {
            ListAllFunctions(params);
        } else if (function_name == "ListVariable") {
            ListVariable(params);
        } else if (function_name == "ListFunction") {
            ListFunction(params);
        } else if (function_name == "Done") {
            Done();
        } else if (func_table.find(function_name) != func_table.end()) {
            vector<variable> args = parse_function_params();
            unordered_map<string, variable> formatted_params = format_params(func_table[function_name].params, args);
            
        } else {
            throw runtime_error("> Invalid built-in function: " + function_name);
        }
    }

    ReturnState parse_statement() {
        // <Statement> ::= <If> | <While> | <Block> | <Expr> | <FunctionCall> | <FunctionDeclaration> | <VariableDeclaration>
        static const unordered_map<string, DataType> type_map = {
            {"int", DataType::Int}, {"float", DataType::Float},
            {"bool", DataType::Bool}, {"char", DataType::Char}, {"string", DataType::String}
        };
        ReturnState return_state = {States::Statement, ""};
        // 處理 <FunctionDeclaration> | <VariableDeclaration>
        // <FunctionDeclaration> ::= <Type> <Ident> "(" <Params> {"," <Params>} ")" <Block> 
        // <VariableDeclaration> ::= <Type> <Ident> ";"
        
        // 如果當前排列為 <Type> <Ident>
        // 接著 <LParen> 則為 <FunctionDeclaration>
        // 接著 <Semicolon> 則為 <VariableDeclaration>
        token next_token1 = lexer.peek_token(1), next_token2 = lexer.peek_token(2);
        // cout << cur_token.val << " | " << next_token1.val << " | " << next_token2.val << endl;
        if (cur_token.type == TokenType::Ident && type_map.find(cur_token.val) != type_map.end() 
            && next_token1.type == TokenType::Ident && keywords.find(next_token1.val) == keywords.end()) {
            if (next_token2.val == "(") {
                return_state = parse_function_declaration();
            } else if (next_token2.val == ";") {
                return_state = parse_variable_declaration();
            } else {
                throw runtime_error("> Invalid statement at parse_statement() : expected identifier but got '" + cur_token.val + "'");
            }
        
        // ident = exp
        } else if (cur_token.type == TokenType::Ident && lexer.peek_token().val == "="
                   && ident_table.find(cur_token.val) != ident_table.end()) {
            token id_token = cur_token; 
            next(2);
            variable result = parse_bool_exp();
            ident_table[id_token.val] = result;
        
        // if
        } else if (cur_token.val == "if") {
            parse_if_else();
        // while
        } else if (cur_token.val == "while") {
            parse_while();
        // block
        } else if (cur_token.val == "{") {
            parse_block();
        // cin / cout
        } else if (cur_token.val == "cin" || cur_token.val == "cout") {
            parse_io(cur_token.val);
        // function call
        } else if (cur_token.type == TokenType::Ident && func_table.find(cur_token.val) != func_table.end()) {
            parse_function_call();
        } else {
            parse_bool_exp();
        }

        if (cur_token.type == TokenType::Semicolon) {
            next();
        }
        // } else {
        //    throw runtime_error("> Invalid statement");
        // } 
        return return_state;
    }

public:
    Parser(const string& input) : lexer(input) {
        cur_token = lexer.get_next_token();
    } 

    bool is_eof() const {
        return cur_token.type == TokenType::EndOfFile;
    }

    string get_rest_str() {
        return lexer.get_rest_str();
    }

    void skip_to_newline() {
        lexer.skip_to_newline();
        if (!lexer.get_rest_str().empty()) {
            cur_token = lexer.get_next_token();
        } else {
            cur_token = {TokenType::EndOfFile, ""};
        }
    }

    void parse_cmd() {
        auto return_state = parse_statement();
        if (return_state.state == States::Definition) {
            cout << "> Definition of " << return_state.var_name << " entered ..." << endl;
        } else if (return_state.state == States::NewDefinition) {
            cout << "> New definition of " << return_state.var_name << " entered ..." << endl;
        } else {
            cout << "> Statement executed ..." << endl;
        }
        // cout << "\ncur_token.type: " << enum_to_TokenType(cur_token.type) << endl;
        if (cur_token.type == TokenType::EndOfFile) {
            return;
        // } else if (is_in(cur_token.val, symbols)){
        //     throw runtime_error("> 9Unexpected token : '" + cur_token.val + "'");
        // } else {
        //     throw runtime_error("> 3Unrecognized token with first char : '" + cur_token.val + "'");
        }
    }
};

// ========================================Built-in Functions========================================

void ListAllVariables(const vector<variable>& variables) {
    vector<string> var_names;
    for (const auto& pair : ident_table) {
        var_names.push_back(pair.first);
    }
    sort(var_names.begin(), var_names.end());
    cout << "> ";
    for (const auto& name : var_names) {
        cout << name << endl;
    }
    // cout << "" << endl;
}

void ListAllFunctions(const vector<variable>& functions) {
    vector<string> func_names;
    for (const auto& pair : func_table) {
        func_names.push_back(pair.first);
    }
    sort(func_names.begin(), func_names.end());
    cout << "> ";
    for (const auto& name : func_names) {
        cout << name << "( ";
        for (int i = 0; i < func_table[name].params.size(); i++) {
            cout << enum_to_DataType(func_table[name].params[i].type) << " " << func_table[name].params[i].name;
            if (i < func_table[name].params.size() - 1) {
                cout << ", ";
            }
        }
        cout << " )" << endl;
    }
    // cout << "Statement executed ..." << endl;
}

void ListVariable(const vector<variable>& variables) {

    string name = format_params(func_table["ListVariable"].params, variables)["name"].val;
    if (ident_table.find(name) != ident_table.end()) {
        variable var = ident_table[name];
        cout << "> " << enum_to_DataType(var.type) << " " << name << " ;" << endl;
        // cout << "Statement executed ..." << endl;
    } else {
        cout << "> Undefined variable : '" << name << "'" << endl;
    }
}

void ListFunction(const vector<variable>& functions) {
    string name = format_params(func_table["ListFunction"].params, functions)["name"].val;
    if (func_table.find(name) != func_table.end()) {
        struct func f = func_table[name];
        cout << "> " << enum_to_DataType(f.return_type) << " " << name << "( ";
        for (int i = 0; i < f.params.size(); i++) {
            cout << enum_to_DataType(f.params[i].type) << " " << f.params[i].name;
            if (i < f.params.size() - 1) {
                cout << ", ";
            }
        }
        cout << " ) " << endl; 
        Lexer lexer(f.content);
        cout << lexer.pretty_print_block() << endl;
        // cout << "Statement executed ..." << endl;
    } else {
        cout << "> Undefined function : '" << name << "'" << endl;
    }
}

void Done() {
    cout << "> Our-C exited ..." << endl;
    exit(0);
}

void parse_wrapper(Parser& parser) {
    while (!parser.is_eof()) {
        try {
            parser.parse_cmd(); // 這裡會自動處理分號 每次處理一個 statement
        } catch (const exception& e) {
            cout << e.what() << endl;
            parser.skip_to_newline();
        }
    }
}

int main() {
    // ios::sync_with_stdio(false);
    // cin.tie(0);
    cout << fixed << setprecision(3);
    cout << "Our-C running ..." << endl;

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

    Parser parser(content);
    parse_wrapper(parser);

    // cout << "> Program exits..." << endl;
    return 0;
}