#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

// 1. 結構體宣告與繼承
// 2. 推導指南 (Deduction Guide)
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// ========================================definition========================================

const string WHITESPACE = " \t\r";
const double ErrorValue = 1e-4;

struct Token;
struct Variable;
struct FunctionParam;
struct Function;
struct Environment;
struct ReturnState;

class Lexer;
class Parser;

bool is_float(double num);
bool is_in(const string &op, const unordered_set<string> &targets);
bool is_in(const string &op, const unordered_map<string, Function> &targets);
bool is_in(const string &op, const unordered_map<string, Variable> &targets);
string trim(const string &s);
string num_to_string(double num);
double string_to_num(string s);
Variable convert_to_var(const Token tk);
void print_var(const Variable &var);
void print_ident_table(const unordered_map<string, Variable> &ident_table);
void print_func_table(const unordered_map<string, Function> &func_table);
unordered_map<string, Variable>
format_params(const vector<FunctionParam> &params, const vector<Variable> &args);

// 將這些函數加入map中
void ListAllVariables(const vector<Variable> &variables); // variables sorted (from smallest to greatest)
void ListAllFunctions(const vector<Variable> &functions); // functions sorted
void ListVariable(const vector<Variable> &variables); // the definition of a particular variable
void ListFunction(const vector<Variable> &functions); // the definition of a particular function
void Done(); // exit the interpreter

enum TokenType {
    Number,
    Point, // 獨立的小數點 token "."
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
    Comma,
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

enum State { 
    Definition, 
    NewDefinition, 
    Statement, 
    Error 
};

typedef pair<string, State> StatePair;

struct ReturnState {
private:
    State state;
    string var_name;

public:
    vector<StatePair> states;

    void clear() {
        if (states.empty())
            return;
        states.clear();
    }

    void push(StatePair state_pair) { states.push_back(state_pair); }

    void pop() {
        if (states.empty())
            return;
        states.pop_back();
    }
};

string enum_to_TokenType(int type) {
    if (type == TokenType::Number) return "Number";
    if (type == TokenType::Ident) return "Ident";
    if (type == TokenType::Str) return "String";
    if (type == TokenType::Chr) return "Char";
    if (type == TokenType::Boolean) return "Boolean";
    if (type == TokenType::Operator) return "Operator";
    if (type == TokenType::SignOperator) return "SignOperator";
    if (type == TokenType::Assign) return "Assign";
    if (type == TokenType::Sign) return "Sign";
    if (type == TokenType::LParen) return "LParen";
    if (type == TokenType::RParen) return "RParen";
    if (type == TokenType::LBracket) return "LBracket";
    if (type == TokenType::RBracket) return "RBracket";
    if (type == TokenType::LBrace) return "LBrace";
    if (type == TokenType::RBrace) return "RBrace";
    if (type == TokenType::Increment) return "Increment";
    if (type == TokenType::Decrement) return "Decrement";
    if (type == TokenType::IO) return "IO";
    if (type == TokenType::Semicolon) return "Semicolon";
    if (type == TokenType::EndOfFile) return "EOF";
    if (type == TokenType::Null) return "Null";
    if (type == TokenType::Undefined) return "Undefined";
    return "Void";
}

string enum_to_DataType(int type) {
    if (type == DataType::Int) return "int";
    if (type == DataType::Float) return "float";
    if (type == DataType::Char) return "char";
    if (type == DataType::String) return "string";
    if (type == DataType::Bool) return "bool";
    return "void";
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

struct Token {
    TokenType type;
    string val;
    int line = 1;
};

// 陣列：就是一堆 Variable 的集合
// 自訂物件：本質上就是屬性名稱(string)與屬性值(Variable)的映射字典
using ArrayType = vector<Variable>;
using ObjectType = unordered_map<string, Variable>;

struct Variable {
    DataType type;
    // 直接儲存原生型別，並利用 shared_ptr 來管理大型或遞迴結構的記憶體
    variant<monostate, int, double, bool, char, string, 
            shared_ptr<ArrayType>, shared_ptr<ObjectType>> val;
    int size = -1; // size = -1 not array

    // 預設建構子初始化為 Null (monostate)
    Variable() : val(monostate{}) { update_type(); }
    Variable(int v) : val(v) { update_type(); }
    Variable(double v) : val(v) { update_type(); }
    Variable(bool v) : val(v) { update_type(); }
    Variable(char v) : val(v) { update_type(); }
    Variable(const string &v) : val(v) { update_type(); }
    Variable(const char *v) : val(string(v)) { update_type(); }
    Variable(shared_ptr<ArrayType> v) : val(v) { update_type(); }
    Variable(shared_ptr<ObjectType> v) : val(v) { update_type(); }

    // 支援直接傳入 TokenType / string 值進行解析建構
    // 初始化暫時設定為1
    Variable(DataType t, const string &v) : type(t) {
        if (t == DataType::Int) val = stoi(v.empty() ? "1" : v);
        else if (t == DataType::Float) val = stod(v.empty() ? "1.0" : v);
        else if (t == DataType::Bool) val = (v == "true");
        else if (t == DataType::Char) val = v.empty() ? '\0' : v[0];
        else if (t == DataType::String) val = v;
        else val = monostate{};
        update_type();
    }

    void update_type() {
        if (holds_alternative<int>(val)) type = DataType::Int;
        else if (holds_alternative<double>(val)) type = DataType::Float;
        else if (holds_alternative<bool>(val)) type = DataType::Bool;
        else if (holds_alternative<char>(val)) type = DataType::Char;
        else if (holds_alternative<string>(val)) type = DataType::String;
        else type = DataType::Void;
    }

private:
    bool is_comparable(const Variable &var1, const Variable &var2) {
        return visit(overloaded{
            [](const string &, const string &) { return true; },
            [](const auto &a, const auto &b) {
                auto is_numeric = [](const auto &v) {
                    using T = decay_t<decltype(v)>;
                    return is_same_v<T, int> || is_same_v<T, double> ||
                           is_same_v<T, char> || is_same_v<T, bool>;
                };
                return is_numeric(a) && is_numeric(b);
            }},
            var1.val, var2.val);
    }

    Variable bool_evaluate(const Variable &var1, const string &op, const Variable &var2) {
        // 建立一個 Lambda，利用 visit 與 overloaded 安全提取數值
        auto extract_numeric_value = [](const Variable &v) -> double {
            // visit 接受一個 Lambda 函式和一個 variant
            // 它會將 variant 中的值傳入 Lambda 函式中，並回傳 Lambda 函式的回傳值
            // overloaded 是一個模板結構，它接受多個 Lambda
            // 函式，並將它們包裝成一個單一的 Lambda 函式
            return visit(overloaded{
                [](double d) -> double { return d; },
                [](int i) -> double { return static_cast<double>(i); },
                [](bool b) -> double { return b ? 1.0 : 0.0; },
                [](char c) -> double { return static_cast<double>(c); },
                [](const auto &) -> double {
                    throw runtime_error("Error in operator bool: unsupported type conversion to double");
                }
            }, v.val);
        };

        double n1 = extract_numeric_value(var1);
        double n2 = extract_numeric_value(var2);
        bool result = false;

        if (op == "==") result = (abs(n1 - n2) <= ErrorValue);
        else if (op == "!=") result = (abs(n1 - n2) > ErrorValue);
        else if (op == "<") result = (n1 + ErrorValue < n2);
        else if (op == ">") result = (n1 > n2 + ErrorValue);
        else if (op == "<=") result = (n1 + ErrorValue <= n2);
        else if (op == ">=") result = (n1 >= n2 + ErrorValue);
        else throw runtime_error("Unsupported operator: " + op);

        // 3. 直接利用建構子，回傳一個封裝了「原生 bool」的 Variable！
        return Variable(result);
    }

public:
    explicit operator bool() const {
        return visit(overloaded{
            [](int i) -> bool { return i != 0; },
            [](double d) -> bool { return d != 0.0; },
            [](bool b) -> bool { return b; },
            [](char c) -> bool { return c != '\0'; },
            [](const string &s) -> bool { return !s.empty(); },
            [](const auto &) -> bool {
                throw runtime_error("Unsupported type for bool conversion");
            }},
			this->val);
    }

    Variable operator+() {
        return visit(overloaded{
            [](int i) -> Variable { return Variable(i); },
            [](double d) -> Variable { return Variable(d); },
            [](const auto &) -> Variable {
                throw runtime_error("Error in operator unary +");
            }},
            this->val);
    }

    Variable operator-() {
        return visit(overloaded{
            [](int i) -> Variable { return Variable(-i); },
            [](double d) -> Variable { return Variable(-d); },
            [](const auto &) -> Variable {
                throw runtime_error("Error in operator unary -");
            }},
            this->val);
    }

    Variable operator!() {
        return visit(overloaded{
            [](bool b) -> Variable { return Variable(!b); },
            [](const auto &) -> Variable {
                throw runtime_error("Error in operator unary !");
            }},
        	this->val);
    }

    Variable operator+(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable { return Variable(a + b); },
            [](double a, double b) -> Variable { return Variable(a + b); },
            [](int a, double b) -> Variable { return Variable(a + b); },
            [](double a, int b) -> Variable { return Variable(a + b); },
            [](const string &a, const string &b) -> Variable { return Variable(a + b); },
            [](const string &a, char b) -> Variable { return Variable(a + string(1, b)); },
            [](char a, const string &b) -> Variable { return Variable(string(1, a) + b); },
            [](char a, char b) -> Variable { return Variable(string(1, a) + string(1, b)); },
            [](const shared_ptr<ArrayType> &a, const string &b) -> Variable {
                string s = "";
                for (const auto &v : *a) {
                    if (holds_alternative<char>(v.val))
                        s += get<char>(v.val);
                }
                return Variable(s + b);
            },
            [](const string &a, const shared_ptr<ArrayType> &b) -> Variable {
                string s = "";
                for (const auto &v : *b) {
                    if (holds_alternative<char>(v.val))
                        s += get<char>(v.val);
                }
                return Variable(a + s);
            },
            [](const auto &, const auto &) -> Variable {
                throw runtime_error("Error in operator+");
            }},
        this->val, var2.val);
    }

    Variable operator-(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable { return Variable(a - b); },
            [](double a, double b) -> Variable { return Variable(a - b); },
            [](int a, double b) -> Variable { return Variable(a - b); },
            [](double a, int b) -> Variable { return Variable(a - b); },
            [](const auto &, const auto &) -> Variable {
                throw runtime_error("Error in operator-");
            }},
        this->val, var2.val);
    }

    Variable operator*(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable { return Variable(a * b); },
            [](double a, double b) -> Variable { return Variable(a * b); },
            [](int a, double b) -> Variable { return Variable(a * b); },
            [](double a, int b) -> Variable { return Variable(a * b); },
            [](const auto &, const auto &) -> Variable {
                throw runtime_error("Error in operator*");
            }},
        this->val, var2.val);
    }

    Variable operator/(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable {
                if (b == 0)
                    throw runtime_error("Error in operator/: division by zero");
                if (a % b == 0)
                    return Variable(a / b);
                return Variable(static_cast<double>(a) / b);
            },
            [](double a, double b) -> Variable {
                if (b == 0.0)
                    throw runtime_error("Error in operator/: division by zero");
                return Variable(a / b);
            },
            [](int a, double b) -> Variable {
                if (b == 0.0)
                    throw runtime_error("Error in operator/: division by zero");
                return Variable(a / b);
            },
            [](double a, int b) -> Variable {
                if (b == 0)
                    throw runtime_error("Error in operator/: division by zero");
                return Variable(a / b);
            },
            [](const auto &, const auto &) -> Variable {
                throw runtime_error("Error in operator/");
            }},
        this->val, var2.val);
    }

    Variable operator%(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable {
                if (b == 0)
                    throw runtime_error("Error in operator%: division by zero");
                return Variable(a % b);
            },
            [](const auto &, const auto &) -> Variable {
                throw runtime_error("Error in operator%: operands must be integers");
            }},
        this->val, var2.val);
    }

    Variable operator==(const Variable &var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error(
                "Error in operator==: incomparable variable: " +
                enum_to_DataType(this->type) + ", " + enum_to_DataType(var2.type)
            );
        }
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable {
                return Variable(a == b);
            },
            [&](const auto &, const auto &) -> Variable {
                return bool_evaluate(*this, "==", var2);
            }},
        this->val, var2.val);
    }

    Variable operator!=(const Variable &var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator!=: incomparable variable");
        }
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable {
                return Variable(a != b);
            },
            [&](const auto &, const auto &) -> Variable {
                return bool_evaluate(*this, "!=", var2);
            }},
        this->val, var2.val);
    }

    Variable operator>=(const Variable &var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator>=: incomparable variable");
        }
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable {
                return Variable(a >= b);
            },
            [&](const auto &, const auto &) -> Variable {
                return bool_evaluate(*this, ">=", var2);
            }},
        this->val, var2.val);
    }

    Variable operator<=(const Variable &var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator<=: incomparable variable");
        }
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable {
                return Variable(a <= b);
            },
            [&](const auto &, const auto &) -> Variable {
                return bool_evaluate(*this, "<=", var2);
            }},
        this->val, var2.val);
    }

    Variable operator>(const Variable &var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator>: incomparable variable");
        }
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable {
                return Variable(a > b);
            },
            [&](const auto &, const auto &) -> Variable {
                return bool_evaluate(*this, ">", var2);
            }},
        this->val, var2.val);
    }

    Variable operator<(const Variable &var2) {
        if (!is_comparable(*this, var2)) {
            throw runtime_error("Error in operator<: incomparable variable");
        }
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable {
                return Variable(a < b);
            },
            [&](const auto &, const auto &) -> Variable {
                return bool_evaluate(*this, "<", var2);
            }},
        this->val, var2.val);
    }

    Variable operator&&(const Variable &var2) {
        return Variable(bool(*this) && bool(var2));
    }

    Variable operator||(const Variable &var2) {
        return Variable(bool(*this) || bool(var2));
    }

    Variable operator+=(const Variable &var2) { return *this = *this + var2; }
    Variable operator-=(const Variable &var2) { return *this = *this - var2; }
    Variable operator*=(const Variable &var2) { return *this = *this * var2; }
    Variable operator/=(const Variable &var2) { return *this = *this / var2; }
    Variable operator%=(const Variable &var2) { return *this = *this % var2; }
};

struct FunctionParam {
    DataType type;
    string name;
};

struct Function {
    DataType return_type;
    vector<FunctionParam> params;
    string content; // 含外層大括號
};

struct Environment {
    unordered_map<string, Variable> ident_table;
    shared_ptr<Environment> parent;

    // 建構子，方便直接指定外層環境
    Environment(shared_ptr<Environment> p = nullptr) : parent(p) {}

    // 尋找變數 (Lookup) - 由內而外找
    // 回傳指標，這樣才能夠直接修改它的值
    Variable *get(const string &name) {
        if (ident_table.find(name) != ident_table.end()) {
            return &ident_table[name]; // 在當前層找到
        }
        if (parent != nullptr) {
            return parent->get(name); // 去外層找
        }
        return nullptr; // 一路找到 top 都沒有，代表未定義
    }

    bool declare(const string &name, const Variable &val) {
        if (ident_table.find(name) != ident_table.end()) {
            return false;
        }
        ident_table[name] = val;
        return true;
    }

    // 賦值更新 (Assignment) - 尋找現有變數並更新
    bool set(const string &name, const Variable &val) {
        if (ident_table.find(name) != ident_table.end()) {
            ident_table[name] = val;
            return true;
        }
        if (parent != nullptr) {
            return parent->set(name, val);
        }
        return false;
    }
};

// ========================================Global Lists========================================

auto global_env = make_shared<Environment>();
auto cur_env = global_env;

unordered_map<string, Function> builtin_func_table = {
    {"ListAllVariables", Function({DataType::Void, {}, ""})},
    {"ListAllFunctions", Function({DataType::Void, {}, ""})},
    {"ListVariable", Function({DataType::Void, {{DataType::String, "name"}}, ""})},
    {"ListFunction", Function({DataType::Void, {{DataType::String, "name"}}, ""})},
    {"Done", Function({DataType::Void, {}, ""})}
};

unordered_map<string, Function> func_table;

// =, +=, -=, *=, /=, %=, ? :, &&, ||, !, ==, !=, <, >, <=, >=, <<, >>, +, -, *,
// /, %
const unordered_set<string> symbols = {
    "=", "+=", "-=", "*=", "/=", "%=", "?",
    ":", "&&", "||", "!",  "==", "!=", "<",
    ">", "<=", ">=", "<<", ">>", "+",  "-",
    "*", "/",  "%",  "(",  ")",  ",",  ";",
    "[", "]",  "{",  "}",  "\"", "'", 
};

const unordered_set<string> data_types = {
    "int", "float", "char", "bool", "string", "void"
};

const unordered_set<string> keywords = ([]{
    unordered_set<string> combined = {
        "cin",
        "cout",
        "ListAllVariables",
        "ListAllFunctions",
        "ListVariable",
        "ListFunction",
        "Done",
        "if",
        "else",
        "while",
        "for",
        "return",
        "break",
        "continue"
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
    {TokenType::Operator, {Operator, Assign, Increment, Decrement, Semicolon}},
    {TokenType::SignOperator, {Operator, Assign, Increment, Decrement, Semicolon}},
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
string trim(const string &s) {
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
    } catch (const invalid_argument &ia) {
        throw runtime_error("Error in string_to_num with value: " + s);
    } catch (const out_of_range &oor) {
        throw runtime_error("Error in string_to_num with value: " + s);
    }
}

bool is_float(double num) {
    // 可能有點問題
    // cout << num << " " << floor(num) << endl;
    return (abs(num - floor(num)) > ErrorValue);
}

bool is_in(const string &str, const unordered_set<string> &targets) {
    return targets.find(str) != targets.end();
}

bool is_in(const string &str, const unordered_map<string, Function> &targets) {
    return targets.find(str) != targets.end();
}

bool is_in(const string &str, const unordered_map<string, Variable> &targets) {
    return targets.find(str) != targets.end();
}

Variable convert_to_var(const Token tk) {
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

string var_to_string(const Variable &var) {
    return visit(overloaded{
        [](int i) { return to_string(i); },
        [](double d) {
            stringstream ss;
            ss << fixed << setprecision(3) << d;
            return ss.str();
        },
        [](bool b) { return string(b ? "true" : "false"); },
        [](char c) { return string(1, c); },
        [](const string &s) { return s; },
        [](const monostate &) { return string("Null"); },
        [](const auto &) { return string("[Object/Array]"); }
    }, var.val);
}

void print_var(const Variable &var) { cout << var_to_string(var) << endl; }

void print_ident_table(const unordered_map<string, Variable> &ident_table) {
    cout << "Ident table: " << endl;
    for (auto const &[key, val] : ident_table) {
        cout << key << " : " << var_to_string(val) << endl;
    }
}

void print_func_table(const unordered_map<string, Function> &func_table) {
    cout << "Function table: " << endl;
    for (auto const &[key, val] : func_table) {
        cout << key << endl;
    }
}

unordered_map<string, Variable> format_params(const vector<FunctionParam> &params,const vector<Variable> &args) {
    unordered_map<string, Variable> formatted_params;
    if (params.size() != args.size()) {
        throw runtime_error("Invalid function call: expected " + to_string(params.size()) + " arguments, got " + to_string(args.size()));
    }
    for (int i = 0; i < params.size(); i++) {
        if (params[i].type != args[i].type) {
            throw runtime_error("Invalid function call: expected " + enum_to_DataType(params[i].type) + " arguments, got " + enum_to_DataType(args[i].type));
        }
        formatted_params[params[i].name] = args[i];
    }
    return formatted_params;
}

// ========================================Implementation========================================

class Lexer {
private:
    struct Checkpoint {
        size_t idx;
        size_t last_token_start_idx;
        int cur_line;
        int last_skipped_newline_count;
    };

    const string text;
    vector<Checkpoint> checkpoints;
    size_t idx = 0;
    size_t last_token_start_idx = 0;
    int cur_line = 1;
    int last_skipped_newline_count = 0;

    /* 目錄
    Token get_a_token(int skip_tokens = 1);
    Token get_next_token(int skip_tokens = 1);
    Token peek_token(int skip_tokens = 1);
    string get_rest_str();
    void skip_to_newline();
    void reset_line();
    string get_a_block();
    string pretty_print_block();
    void push_checkpoint();
    void pop_checkpoint();
    void back_to_checkpoint();
    void find_first_of(const string &target);
    */

    // 只要一個statement結束就是一個新的statement開始 包含空白 換行 註解等
    Token get_a_token(int skip_tokens = 1) {
        Token tk;
        int skipped_newlines = 0;
        for (int i = 0; i < skip_tokens; i++) {
            skipped_newlines = 0;
            while (idx < text.length()) {
                if (isspace(text[idx])) {
                    if (text[idx] == '\n') {
                        cur_line++;
                        skipped_newlines++;
                    }
                    idx++;
                } else if (idx + 1 < text.length() && text[idx] == '/' && text[idx + 1] == '/') {
                    while (idx < text.length() && text[idx] != '\n') {
                        idx++;
                    }
                    if (idx < text.length() && text[idx] == '\n') {
                        cur_line++;
                        skipped_newlines++;
                        idx++;
                    }
                } else {
                    break;
                }
            }

            last_token_start_idx = idx;
            if (idx >= text.length()) {
                tk = {TokenType::EndOfFile, ""};
                tk.line = cur_line;
                last_skipped_newline_count = skipped_newlines;
                return tk;
            }

            if (text[idx] == '\'') {
                string chr_str;
                idx++;
                if (idx < text.length() && text[idx] == '\\') {
                    chr_str += '\\';
                    idx++;
                    if (idx < text.length()) {
                        chr_str += text[idx];
                        idx++;
                    } else {
                        throw runtime_error("Line " + to_string(cur_line) + " : unexpected token : '''");
                    }
                } else {
                    chr_str += text[idx];
                    idx++;
                }
                if (text[idx] != '\'')
                    throw runtime_error("Line " + to_string(cur_line) + " : unexpected token : '''");
                idx++;
                tk = {TokenType::Chr, chr_str};
            } else if (text[idx] == '"') {
                string str_str;
                idx++;
                while (text[idx] != '"') {
                    if (idx >= text.length())
                        throw runtime_error("Line " + to_string(cur_line) + " : unexpected token : '\"'");
                    if (text[idx] == '\\') {
                        str_str += '\\';
                        idx++;
                        if (idx < text.length()) {
                            str_str += text[idx];
                            idx++;
                        }
                    } else {
                        str_str += text[idx];
                        idx++;
                    }
                }
                if (text[idx] != '"')
                    throw runtime_error("Line " + to_string(cur_line) + " : unexpected token : '\"'");
                idx++;
                tk = {TokenType::Str, str_str};
            } else if (isdigit(text[idx]) || text[idx] == '.') {
                string num_str;

                if (text[idx] == '.') {
                    tk = {TokenType::Point, "."};
                    idx++;
                } else if (isdigit(text[idx])) {
                    while (idx < text.length() && isdigit(text[idx])) {
                        num_str += text[idx];
                        idx++;
                    }
                    tk = {TokenType::Number, num_str};
                }
            } else if (text.compare(idx, 4, "true") == 0) {
                idx += 4;
                tk = {TokenType::Boolean, "true"};
            } else if (text.compare(idx, 5, "false") == 0) {
                idx += 5;
                tk = {TokenType::Boolean, "false"};
            } else if (isalpha(text[idx]) || text[idx] == '_') {
                string ident_str;
                while (idx < text.length() && (isalnum(text[idx]) || text[idx] == '_')) {
                    ident_str += text[idx];
                    idx++;
                }
                tk = {TokenType::Ident, ident_str};
            } else {
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
                    else if (c == '[') {idx += 1; tk = {TokenType::LBracket, "["}; continue;}
                    else if (c == ']') {idx += 1; tk = {TokenType::RBracket, "]"}; continue;}
                    else if (c == '{') {idx += 1; tk = {TokenType::LBrace, "{"}; continue;}
                    else if (c == '}') {idx += 1; tk = {TokenType::RBrace, "}"}; continue;}
                    else if (c == ',') {idx += 1; tk = {TokenType::Comma, ","}; continue;}
                    else if (c == '?') {idx += 1; tk = {TokenType::Operator, "?"}; continue;}
                    else if (c == ':') {idx += 1; tk = {TokenType::Operator, ":"}; continue;}
                    else if (c == ';') {idx += 1; tk = {TokenType::Semicolon, ";"}; continue;}
                    else {idx += 1; tk = {TokenType::Undefined, string("") + c}; continue;}
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
        tk.line = cur_line;
        last_skipped_newline_count = skipped_newlines;
        return tk;
    }
public:
    Lexer(const string &input, int start_line = 1) 
        : text(input), idx(0), cur_line(start_line) {}

    size_t get_idx() const { return idx; }
    size_t get_last_token_start_idx() const { return last_token_start_idx; }

    string get_substring(size_t start, size_t end) const {
        if (start < end && end <= text.length()) {
            return text.substr(start, end - start);
        }
        return "";
    }

    Token get_next_token(int skip_tokens = 1) {
        // get token 同時修改idx
        Token tk = get_a_token(skip_tokens);
        // cout << "tk: " << tk.val << endl;
        return tk;
    }

    Token peek_token(int skip_tokens = 1) {
        size_t start_idx = idx;
        int start_line = cur_line;
        size_t start_last_token_start_idx = last_token_start_idx;
        int start_last_skipped_newline_count = last_skipped_newline_count;
        Token tk = get_a_token(skip_tokens);
        idx = start_idx;
        cur_line = start_line;
        last_token_start_idx = start_last_token_start_idx;
        last_skipped_newline_count = start_last_skipped_newline_count;
        return tk;
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

    void reset_line() {
        cur_line = 1;
        last_skipped_newline_count = 0;
    }

    void finish_outer_statement(Token &next_token) {
        int next_line = max(1, last_skipped_newline_count);
        cur_line = next_line;
        if (next_token.type != TokenType::EndOfFile) {
            next_token.line = next_line;
        }
    }

    string get_a_block() {
        Token tk = get_next_token();
        if (tk.val == "{") {
            int brace_count = 1;
            string block_str = "{";
            while (idx < text.length()) {
                if (text[idx] == '{')
                    brace_count++;
                else if (text[idx] == '}')
                    brace_count--;
                else if (text[idx] == '\n')
                    cur_line++;
                block_str += text[idx];
                idx++;
                if (brace_count == 0)
                    break;
            }
            return block_str;
        } else {
            throw runtime_error("Line " + to_string(tk.line) + " : unexpected token : '" + tk.val + "'");
        }
    }
    string pretty_print_block() {
        Token tk = peek_token();
        if (tk.val != "{") {
            throw runtime_error("Line " + to_string(tk.line) + " : unexpected token : '" + tk.val + "'");
        }
    
        int indent_level = 0;
        string result = "";
        bool is_new_line = true; // 標記目前是否處於新的一行 (用以判斷縮排位置)
        TokenType prev_type = TokenType::Undefined;
        string prev_val = "";
        int brace_count = 0;
    
        while (true) {
            tk = get_next_token(); // 取得下一個 Token (會自動跳過註解與空格)

            if (tk.type == TokenType::EndOfFile) break;
    
            if (tk.val == "{") {
                if (!is_new_line) result += " ";
                result += "{\n";
                brace_count++;
                indent_level++;
                is_new_line = true;
                
            } else if (tk.val == "}") {
                brace_count--;
                indent_level--;
                if (!is_new_line) result += "\n";
                
                // 根據當前層級縮排 (每層 2 個空格)
                result += string(indent_level * 2, ' ') + "}";
                
                if (brace_count == 0) {
                    break; // 大括號已經閉合，結束解析
                } else {
                    result += "\n";
                    is_new_line = true;
                }
                
            } else if (tk.val == ";") {
                if (!is_new_line) result += " ";
                result += ";\n"; // 分號後強制換行
                is_new_line = true;
                
            } else {
                if (is_new_line) {
                    // 如果是新的行，添加縮排
                    result += string(indent_level * 2, ' ');
                    is_new_line = false;
                } else {
                    // 判斷 Token 之間是否需要一空格
                    bool need_space = true;
                    
                    // 特殊處理函數呼叫的情況：例如 AddTwo(x) 的 '(' 前面不加空格
                    if (tk.val == "(" && prev_type == TokenType::Ident) {
                        // 排除 if, while, for 等關鍵字的例外，例如 if (x > 0)
                        if (prev_val != "if" && prev_val != "while" && prev_val != "for") {
                            need_space = false;
                        }
                    }
                    if (need_space) result += " ";
                }
                result += tk.val;
            }
            
            // 紀錄最後一個 Token 的資訊，以供下一次的格式判斷
            prev_type = tk.type;
            prev_val = tk.val;
        }
        
        return result;
    }

    void push_checkpoint() {
        checkpoints.push_back({idx, last_token_start_idx, cur_line, last_skipped_newline_count});
    }

    void pop_checkpoint() { checkpoints.pop_back(); }

    void back_to_checkpoint() {
        const Checkpoint &checkpoint = checkpoints.back();
        idx = checkpoint.idx;
        last_token_start_idx = checkpoint.last_token_start_idx;
        cur_line = checkpoint.cur_line;
        last_skipped_newline_count = checkpoint.last_skipped_newline_count;
    }
    void find_first_of(const string &target) {
        size_t result = text.find_first_of(target, idx);
        if (result != string::npos) {
            idx = result;
        }
    }

    int get_cur_line() { return cur_line; }

    void print_cur_line_content() { 
        int temp_idx = idx;
        cout << "content: ";
        while (temp_idx < text.length() && text[temp_idx] != '\n') {
            cout << text[temp_idx];
            temp_idx++;
        }
        cout << endl; 
    }

};

class Parser {
private:
    Lexer lexer;
    Token prev_token = {TokenType::Undefined, ""};
    Token cur_token;
    vector<Token> parens_stack;
    bool require_semicolon = true;
    const int MAX_STEPS = 100;  

    int recursion_depth = 0; // 遞迴深度（暫時設為一級，需要實作到function calling）
    DataType current_return_type = DataType::Void; 

    bool dry_run = false; // <-- 關鍵點：跳過賦值動作

/* 目錄 (重構版)
    struct ReturnException;
    void next(int skip_tokens = 1, bool is_def = false)

    // ==========================================
    // 1. 運算式解析 (Expression Parsing) - 由下而上嚴格對應優先級
    // ==========================================
    [修改] Variable parse_unary_exp()         // 原 parse_factor。需加入：負號、!、前/後置 ++/--，以及陣列 [ ] 取值。
    [新增] Variable parse_multiplicative_exp()// 取代 parse_term。需加入：取餘數 '%'。
    [新增] Variable parse_additive_exp()      // 取代原 parse_exp 中處理加減法的部分。
    [新增] Variable parse_shift_exp()         // 新增：處理左移 '<<' 與右移 '>>'。
    [新增] Variable parse_relational_exp()    // 取代 parse_relation_exp。專注處理 '<', '>', '<=', '>='。
    [新增] Variable parse_equality_exp()      // 取代 parse_equal_exp。專注處理 '==', '!='。
    [新增] Variable parse_bitwise_and_exp()   // 新增：處理位元 AND '&'。
    [新增] Variable parse_bitwise_xor_exp()   // 新增：處理位元 XOR '^'。
    [新增] Variable parse_bitwise_or_exp()    // 新增：處理位元 OR '|'。
    [修改] Variable parse_logical_and_exp()   // 原 parse_and_exp。處理 '&&'。
    [修改] Variable parse_logical_or_exp()    // 原 parse_or_exp。處理 '||'。
    [修改] Variable parse_conditional_exp()   // 原 parse_bool_exp。專注處理三元運算子 '?' ':'。
    [新增] Variable parse_basic_exp()         // 新增：文法表中的 BasicExpression。負責處理賦值 ('=', '+=', etc.) 及陣列元素賦值。
    [新增] Variable parse_expression()        // 新增：文法表頂層。處理逗號運算子 ',' 串聯多個 BasicExpression。

    // ==========================================
    // 2. 控制流與陳述句 (Control Flow & Statements)
    // ==========================================
    [移除] Variable parse_io(const string &type) // 移除：嚴格文法不包含 cin / cout。
    [修改] bool parse_condition()             // 修改：括號內需改為呼叫頂層的 parse_expression()。
    void skip_token()
    void process_parens_in_skip()
    [修改] void skip_statement()              // 修改：內部需加入對 do-while 結構的略過邏輯。
    void parse_if_else()
    void parse_while()
    [新增] void parse_do_while()              // 新增：實作文法表中的 do-while 迴圈。
    void parse_block()

    // ==========================================
    // 3. 函式與變數宣告 (Declarations)
    // ==========================================
    void parse_function_block(string block_str, unordered_map<string, Variable> formatted_params)
    [修改] vector<StatePair> parse_variable_declaration(DataType type = DataType::Void) 
           // 修改：確保符合文法「宣告時不允許初始化」的規則（也就是不能寫 int x = 5;）。
    [修改] vector<FunctionParam> parse_function_declaration_params() 
           // 修改：文法表 FormalParameters 支援參照（&）與陣列（[ ]），需加入對應解析。
    vector<Variable> parse_function_params()
    [修改] StatePair parse_function_declaration() 
           // 修改：確保對應文法，可能需要相容 void 作為回傳型態或參數的特殊處理。
    [修改] void parse_function_call()         
           // 修改：呼叫時的引數傳遞，需相容陣列與參照型態的傳遞。
    void parse_return(DataType return_type)
    [修改] ReturnState parse_statement(bool sub_statement = false) 
           // 修改核心：移除此處所有對於「=」的判斷與賦值操作，將權力下放給 parse_basic_exp()。

    // ==========================================
    // 4. 基礎 Parser 方法 (Base Methods)
    // ==========================================
    Parser(const string &input)
    bool is_eof() const
    string get_rest_str()
    void skip_to_newline()
    void parse_cmd()
*/

    struct ReturnException {
        Variable value;
    };

    void next(int skip_tokens = 1, bool is_def = false) {
        // 如果 "下一個" 不是預期的 token 就在這階段報錯（這算是屬於較淺層的報錯）
        auto ue_types = unexpected_types[cur_token.type];
        Token next_token = lexer.peek_token(skip_tokens);

        // 第一階段報錯
        if (cur_token.type == TokenType::Undefined) {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unrecognized token with first char : '" + cur_token.val + "'");

        // 後隨型態報錯
        } else if (find(ue_types.begin(), ue_types.end(), next_token.type) != ue_types.end()) {
            throw runtime_error("Line " + to_string(next_token.line) + " : unexpected token : '" + next_token.val + "'");

        // ident第一階段報錯
        } else if (cur_token.type == TokenType::Ident && 
            (cur_env->get(cur_token.val) == nullptr || is_def) && 
            keywords.find(cur_token.val) == keywords.end() &&
            func_table.find(cur_token.val) == func_table.end() &&         // 加入自定義函數檢查
            builtin_func_table.find(cur_token.val) == builtin_func_table.end()) // 加入內建函數檢查
        {
            throw runtime_error("Line " + to_string(cur_token.line) + " : undefined identifier : '" + cur_token.val + "'");
        }
        prev_token = cur_token;
        cur_token = lexer.get_next_token(skip_tokens);
        // cout << "next: " << cur_token.val << " | line: " << cur_token.line << endl;
        return;
    }

    Variable parse_factor() {
        // num, (), sign
        Variable result;
        Token next_token = lexer.peek_token();
        if (cur_token.val == "(") {
            next();
            // (is_return_bool) ? val = parse_bool_exp() : val = parse_exp();
            result = parse_bool_exp();
            if (cur_token.val == ")") {
                next();
            } else {
                throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
            }

            // cout << "cur: " + cur_token.val + " | next: " + lexer.peek_token().val << endl;
        } else if (is_in(cur_token.val, {"+", "-", "!"})) {
            if (cur_token.val == "++" && next_token.type == TokenType::Ident) {
                // ++a
                next();
                Token id_token = cur_token;
                result = parse_factor() + Variable{1};
                if (!dry_run) cur_env->set(id_token.val, result);
            } else if (cur_token.val == "--" && next_token.type == TokenType::Ident) {
                // --a
                next();
                Token id_token = cur_token;
                result = parse_factor() - Variable{1};
                if (!dry_run) cur_env->set(id_token.val, result);
            } else if (cur_token.val == "+") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
                }
                result = parse_factor();
            } else if (cur_token.val == "-") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
                }
                result = -parse_factor();
            } else if (cur_token.val == "!") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
                }
                result = !parse_factor();
            }
        } else if (cur_token.type == TokenType::Number) {
            auto num_tk = cur_token;
            if (lexer.peek_token().type == TokenType::Point) {
                next(); // get Point (".")
                num_tk.val += cur_token.val;
                if (lexer.peek_token().type == TokenType::Number) {
                    next(); // get digits after dot
                    num_tk.val += cur_token.val;
                }

                if (lexer.peek_token().type == TokenType::Point) {
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + lexer.peek_token().val + "'");
                }
            }
            result = convert_to_var(num_tk);
            next();
            
        } else if (cur_token.type == TokenType::Point) {
            auto num_tk = cur_token; // "."
            Token next_token = lexer.peek_token();
            if (next_token.type == TokenType::Number) {
                next(); // get digits after dot
                num_tk.val += cur_token.val;
            } else if (next_token.type == TokenType::Point) {
                throw runtime_error("Line " + to_string(next_token.line) + " : unexpected token : '" + next_token.val + "'");
            }

            result = convert_to_var(num_tk);
            next();

        } else if (cur_token.type == TokenType::Ident) {
            if (lexer.peek_token().val == "(") {
                result = parse_function_call();
            } else {
                Token id_token = cur_token;
                next(); // 略過標識符
                // 拿值
                // 需要錯誤導向
                result = *cur_env->get(id_token.val);
                // cout << "result: " << id_token.val << " | " << result.val << endl;
                if (cur_token.val == "++") {
                    if (!dry_run) {
                        cur_env->set(
                            id_token.val,
                            *cur_env->get(id_token.val) + Variable{result.type, "1"}
                        );
                    }
                    next();
                } else if (cur_token.val == "--") {
                    if (!dry_run) {
                        cur_env->set(
                            id_token.val,
                            *cur_env->get(id_token.val) - Variable{result.type, "1"}
                        );
                    }
                    next();
                }
            }

        } else if (cur_token.type == TokenType::Chr) {
            result = Variable{DataType::Char, cur_token.val};
            next();

        } else if (cur_token.type == TokenType::Str) {
            result = Variable{DataType::String, cur_token.val};
            next();

        } else if (cur_token.type == TokenType::Semicolon) {
            return Variable{DataType::Void, ""};

        } else {
            if (symbols.find(cur_token.val) == symbols.end()) {
                throw runtime_error("Line " + to_string(cur_token.line) + " : unrecognized token with first char : '" + cur_token.val + "'");
            } else {
                throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
            }
        }
        return result;
      }

    Variable parse_term() {
        // *, /
        Variable result = parse_factor();
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
                // else throw runtime_error("Line " + to_string(cur_token.line) + " : error");
            }
        }

        return result;
    }

    Variable parse_exp() {
        // +, -
        Variable result = parse_term();
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

    Variable parse_relation_exp() {
        // < <= > >=
        Variable result = parse_exp();
        if (cur_token.type == TokenType::EndOfFile)
            return result;

        static const unordered_map<string, function<Variable(Variable, Variable)>>
            op_map = {
                {">", [](Variable a, Variable b) { return a > b; }},
                {"<", [](Variable a, Variable b) { return a < b; }},
                {">=", [](Variable a, Variable b) { return a >= b; }},
                {"<=", [](Variable a, Variable b) { return a <= b; }},
                {"==", [](Variable a, Variable b) { return a == b; }},
                {"!=", [](Variable a, Variable b) { return a != b; }}
            };
            auto it = op_map.find(cur_token.val);
            if (it != op_map.end()) {
                next();
                return it->second(result, parse_exp());
            }
            return result;
    }
  
    Variable parse_equal_exp() {
        // == !=
        Variable result = parse_relation_exp();
        if (cur_token.type == TokenType::EndOfFile)
            return result;

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
  
    Variable parse_and_exp() {
        // &&
        Variable result = parse_equal_exp();
        if (cur_token.type == TokenType::EndOfFile)
            return result;

        while (is_in(cur_token.val, {"&&"})) {
            if (cur_token.val == "&&") {
                next();
                result = result && parse_equal_exp();
            }
        }
        return result;
      }
  
    Variable parse_or_exp() {
        // ||
        Variable result = parse_and_exp();
        if (cur_token.type == TokenType::EndOfFile)
            return result;

        while (is_in(cur_token.val, {"||"})) {
            if (cur_token.val == "||") {
                next();
                result = result || parse_and_exp();
            }
        }
        return result;
      }

    Variable parse_bool_exp() { 
        Variable result = parse_or_exp();
        // 已經遍歷了 || 活性最高，故可以將其視為這類表達式
        if (cur_token.val == "?") {
            next();
            Variable true_val = parse_bool_exp(); 
            
            if (cur_token.val != ":") {
                throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
            }
            next();
            Variable false_val = parse_bool_exp();
            
            if (bool(result)) {
                return true_val;
            } else {
                return false_val;
            }
        }
        
        return result; 
    }

    Variable parse_io(const string &type) {
        // cin >> ident >> ident ...
        // cout << bool_exp | exp << bool_exp | exp ...
        Variable result;
        if (type == "cin") {
            next();
            if (cur_token.val != ">>") {
                throw runtime_error("Line " + to_string(cur_token.line) +
                                    " : unexpected token : '" + cur_token.val + "'");
            }

            while (cur_token.val == ">>") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    Variable* target_var = cur_env->get(cur_token.val);
                    if (target_var == nullptr) {
                        throw runtime_error(
                            "Line " + to_string(cur_token.line) +
                            " : undefined identifier : '" + cur_token.val + "'"
                        );
                    }
                    // 目前沒實作cin，所以先空著
                    /*
                    if (target_var->type == DataType::Int) {
                        int input_val;
                        cin >> input_val;
                        *target_var = Variable(input_val);
                    } else if (target_var->type == DataType::Float) {
                        double input_val;
                        cin >> input_val;
                        *target_var = Variable(input_val);
                    } else if (target_var->type == DataType::String) {
                        string input_val;
                        cin >> input_val;
                        *target_var = Variable(input_val);
                    }
                    */
                    next();
                } else {
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
                }
            }
            // cout << "Statement executed ..." << endl;
            result = {DataType::Int, ""};

        } else if (type == "cout") {
            next();
            if (cur_token.val != "<<") {
                throw runtime_error("Line " + to_string(cur_token.line) +
                                    " : unexpected token : '" + cur_token.val + "'");
            }
            while (cur_token.val == "<<") {
                next();
                if (cur_token.type == TokenType::Ident) {
                    if (cur_env->get(cur_token.val) == nullptr) {
                        throw runtime_error("Line " + to_string(cur_token.line) + " : undefined identifier : '" + cur_token.val + "'");
                    }
                    // cout << ident_table[cur_token.val].val;
                    next();
                } else {
                    result = parse_exp();
                    // cout << result.val;
                }
            }
            // cout << "Statement executed ..." << endl;
        } else {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + type + "'");
        }
        return result;
    }

    bool parse_condition() {
        if (cur_token.val != "(") {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }
        next();
        Variable result = parse_bool_exp();
        if (cur_token.val != ")") {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }
        next();
        return bool(result);
    }

    void skip_token() {
        prev_token = cur_token;
        cur_token = lexer.get_next_token(1);
    }

    void process_parens_in_skip() {
        if (cur_token.val == "(" || cur_token.val == "[" || cur_token.val == "{") {
            parens_stack.push_back(cur_token);
        } else if (cur_token.val == ")" || cur_token.val == "]" ||
               cur_token.val == "}") {
            if (!parens_stack.empty()) {
                bool match = false;
                if (cur_token.val == ")" && parens_stack.back().val == "(") match = true;
                if (cur_token.val == "]" && parens_stack.back().val == "[") match = true;
                if (cur_token.val == "}" && parens_stack.back().val == "{") match = true;
                if (match) parens_stack.pop_back();
                else throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
            } else {
                throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
            }
        }
    }

    void skip_statement() {
        size_t start_idx = lexer.get_last_token_start_idx();
        int start_line = cur_token.line;

        if (cur_token.val == "{") {
            parens_stack.push_back(cur_token);
            skip_token();
            while (!parens_stack.empty() && cur_token.type != TokenType::EndOfFile) {
                process_parens_in_skip();
                skip_token();
            }
        } else if (cur_token.val == "if") {
            skip_token();
            if (cur_token.val == "(") {
                parens_stack.push_back(cur_token);
                skip_token();
                while (!parens_stack.empty() && cur_token.type != TokenType::EndOfFile) {
                    process_parens_in_skip();
                    skip_token();
                }
            }
            skip_statement();
            if (cur_token.val == "else") {
                skip_token();
                skip_statement();
            }
        } else if (cur_token.val == "while") {
            skip_token();
            if (cur_token.val == "(") {
                parens_stack.push_back(cur_token);
                skip_token();
                while (!parens_stack.empty() && cur_token.type != TokenType::EndOfFile) {
                    process_parens_in_skip();
                    skip_token();
                }
            }
            skip_statement();
        } else {
            while (cur_token.val != ";" && cur_token.type != TokenType::EndOfFile) {
                process_parens_in_skip();
                skip_token();
            }
            if (cur_token.val == ";") {
                if (!parens_stack.empty())
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : ';'");
                skip_token();
            }
        }

        size_t end_idx = lexer.get_idx();
        string skipped_code = lexer.get_substring(start_idx, end_idx);

        if (!skipped_code.empty()) {
            auto old_env = cur_env;
            cur_env = make_shared<Environment>(old_env);

            try {
                Parser temp_parser(skipped_code, start_line);
                temp_parser.set_dry_run(true);
                temp_parser.parse_statement(false, false);
            } catch (const exception &e) {
                cur_env = old_env;
                throw runtime_error(e.what());
            }

            cur_env = old_env;
        }
    }
    void parse_if_else() {
        if (cur_token.val != "if") {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }

        bool condition_met = false;
        next();

        bool condition = parse_condition();

        if (condition) {
            parse_statement(false, false);
            condition_met = true;
        } else {
            skip_statement();
        }

        while (cur_token.val == "else") {
            next();
            if (cur_token.val == "if") {
                next();
                condition = parse_condition();
                if (!condition_met && condition) {
                    parse_statement(false, false);
                    condition_met = true;
                } else {
                    skip_statement();
                }
            } else {
                if (!condition_met) {
                    parse_statement(false, false);
                    condition_met = true;
                } else {
                    skip_statement();
                }
                break;
            }
        }
    }
    void parse_while() {
        bool condition;
        if (cur_token.val == "while") {
            lexer.push_checkpoint();
            next();
            condition = parse_condition();
            if (!condition) {
                skip_statement();
            }
            int execution_steps = 0;
            while (condition && execution_steps <= MAX_STEPS) {
                execution_steps++;
                parse_statement(false, false);
                lexer.back_to_checkpoint();
                next();
                condition = parse_condition();
                if (!condition || execution_steps > MAX_STEPS) {
                    skip_statement();
                }
            }
            lexer.pop_checkpoint();
        } else {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }
    }
    void parse_block() {
        auto new_env = make_shared<Environment>(cur_env);
        cur_env = new_env;
        try {
            if (cur_token.val == "{") {
                next();
                while (cur_token.val != "}") {
                    parse_statement(false, false);
                }
                next();
            }
        } catch (runtime_error &e) {
            cur_env = cur_env->parent;
            throw runtime_error(string(e.what()));
        }
        cur_env = cur_env->parent;
    }
    void parse_function_block(string block_str, unordered_map<string, Variable> formatted_params) {
        auto new_env = make_shared<Environment>(cur_env);
        cur_env = new_env;
        for (auto &param : formatted_params) {
            cur_env->declare(param.first, param.second);
        }

        Parser block_parser(block_str);
        block_parser.current_return_type = this->current_return_type;

        try {
            if (block_parser.cur_token.val == "{") {
                block_parser.next();
                while (block_parser.cur_token.val != "}" && !block_parser.is_eof()) {
                    block_parser.parse_statement(false, false);
                }
            }
        } catch (...) {
            cur_env = cur_env->parent;
            throw;
        }
        cur_env = cur_env->parent;
    }
    vector<StatePair> parse_variable_declaration(DataType type = DataType::Void) {
        // cur_token is type
        // <VariableDeclaration> ::= <Type> <Ident> [ "[" <Expression> "]" ] ;
        // <MultiVariableDeclaration> ::= <Type> <Ident> [ "[" <Expression> "]" ] {
        // , <Ident> [ "[" <Expression> "]" ] } ;
        vector<StatePair> state_pairs;
        Token id_token, mark_token;
        // 基底型態
        if (is_in(cur_token.val, data_types)) {
            type = DataType_to_enum(cur_token.val);
        } else {
            throw runtime_error("Line " + to_string(cur_token.line) +
                              " : unexpected token : '" + cur_token.val + "'");
        }
        id_token = lexer.peek_token();
        mark_token = lexer.peek_token(2);
        while (true) {
            // 現在在type上
            if (id_token.type != TokenType::Ident) {
                throw runtime_error("Line " + to_string(id_token.line) +
                                " : unexpected token : '" + id_token.val + "'");
            }

            string name = id_token.val;
            State state = State::Definition;
            if (cur_env->get(name) != nullptr) {
                state = State::NewDefinition;
            }

            Variable var = Variable(type, "");

            if (mark_token.val == "[") {
                // 此階段進去看exp
                next(3);
                auto size_var = parse_exp().val;
                int arr_size = 0;
                if (holds_alternative<int>(size_var)) {
                    arr_size = get<int>(size_var);
                } else if (holds_alternative<double>(size_var)) {
                    arr_size = static_cast<int>(get<double>(size_var));
                } else {
                    throw runtime_error("Line " + to_string(cur_token.line) +
                                      " : unexpected token : '" + cur_token.val + "'");
                }

                if (cur_token.val != "]") {
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
                }
                next(); // move past ]

                var.size = arr_size;
                auto array_ptr = make_shared<ArrayType>(arr_size);
                for (int i = 0; i < arr_size; ++i) {
                    (*array_ptr)[i] = Variable(type, "");
                }
                var.val = array_ptr;
            } else {
                // 此階段跳過兩個標點符號
                next(2);
            }

            cur_env->declare(name, var);
            state_pairs.push_back({name, state});

            id_token = lexer.peek_token();
            mark_token = lexer.peek_token(2);
            // cout << "id_token: " << id_token.val << " | mark_token: " <<
            // mark_token.val << endl;

            if (cur_token.val == ";") {
                break;
            }
        }

        return state_pairs;
    }

    vector<FunctionParam> parse_function_declaration_params() {
        // <Params> ::= ( <Type> <Ident> { , <Type> <Ident> } | <Empty> )
        // 定義函式的參數
        vector<FunctionParam> params;
        Token next_token1 = lexer.peek_token();
        Token next_token2 = lexer.peek_token(2);
        auto parse_params = [&]() -> void {
            Token next_token1 = lexer.peek_token();
            Token next_token2 = lexer.peek_token(2);
            if (is_in(next_token1.val, data_types)) {
                if (next_token2.type == TokenType::Ident) {
                    params.push_back(
                        {DataType_to_enum(next_token1.val), next_token2.val});
                    next(3);
                } else {
                    throw runtime_error("Line " + to_string(next_token2.line) + " : unexpected token : '" + next_token2.val + "'");
                }
            } else {
                throw runtime_error("Line " + to_string(next_token1.line) + " : unexpected token : '" + next_token1.val + "'");
            }
        };
        if (cur_token.val == "(") {
            if (next_token1.val == ")") {
                next(); // move to ')' to let parse_function_declaration handle it
                return params;
            } else {
                parse_params();
                while (cur_token.val == ",") {
                    parse_params();
                }
                if (cur_token.val != ")") {
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
                }
                // already at ')'
            }
        } else {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }
        return params;
    }

    vector<Variable> parse_function_params() {
        // <Params> ::= "(" <BoolExpression> | <Expression> { , <BoolExpression> |
        // <Expression> } ")" | "()" ident = exp 可能會看成expression
        // 被函式呼叫的參數，也就是輸入參數
        vector<Variable> params;
        Token next_token1 = lexer.peek_token();
        if (cur_token.val == "(") {
            if (next_token1.val == ")") {
                next(2);
                return params;
            } else {
                next();
                params.push_back(parse_bool_exp());
                while (cur_token.val == ",") {
                    next();
                    params.push_back(parse_bool_exp());
                }
                if (cur_token.val != ")") {
                    throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
                }
                next();
            }
        } else {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }
        return params;
    }

    StatePair parse_function_declaration() {
        // <Function> ::= <Type> <Ident> "(" <Params> {"," <Params>} ")" (<Block> | <Statement>) | <BuiltInFunction>
        DataType type = DataType_to_enum(cur_token.val);
        string name = lexer.peek_token().val;
        State state = State::Definition;
        if (cur_token.type != TokenType::Ident && keywords.find(cur_token.val) == keywords.end()) {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }
        next(2);
        if (func_table.find(name) != func_table.end()) {
            state = State::NewDefinition;
        }
        vector<FunctionParam> params = parse_function_declaration_params();
        string block_str = lexer.get_a_block();
        cur_token = lexer.get_next_token();
        func_table[name] = Function{type, params, block_str};
        return {name + "()", state};
    }

    Variable parse_function_call() {
        string function_name = cur_token.val;
        next();
        vector<Variable> params = parse_function_params();
        
        if (function_name == "ListAllVariables") {
            if (!dry_run) ListAllVariables(params);
        } else if (function_name == "ListAllFunctions") {
            if (!dry_run) ListAllFunctions(params);
        } else if (function_name == "ListVariable") {
            if (!dry_run) ListVariable(params);
        } else if (function_name == "ListFunction") {
            if (!dry_run) ListFunction(params);
        } else if (function_name == "Done") {
            if (!dry_run) Done();
        } else if (func_table.find(function_name) != func_table.end()) {
            unordered_map<string, Variable> formatted_params = format_params(func_table[function_name].params, params);
            
            DataType old_return_type = current_return_type;
            current_return_type = func_table[function_name].return_type;

            // --- 關鍵 dry_run 判斷 ---
            if (!dry_run) {
                try {
                    parse_function_block(func_table[function_name].content, formatted_params);
                } catch (ReturnException &re) {
                    current_return_type = old_return_type; 
                    return re.value;
                }
            }
            
            current_return_type = old_return_type; 
            
            // 賦值時，確保回傳的型態符合基底型態，否則之後 parse_exp 會錯
            if (func_table[function_name].return_type == DataType::Void) return Variable();
            return Variable(func_table[function_name].return_type, ""); 
        } else {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + function_name + "'");
        }
        return Variable();
    }

    void parse_return() {
        // <Return> ::= "return" [ <BoolExpression> | <Expression> ] ";"
        next();
        Variable value;
        // 處理回傳型態
        if (cur_token.val != ";") {
            value = parse_bool_exp();
        }

        // 檢查回傳值是否與預期相符
        if (current_return_type == DataType::Void && value.type != DataType::Void) {
            // throw runtime_error("Line " + to_string(cur_token.line) + " : void function cannot return a value");
        } else if (current_return_type != DataType::Void && value.type == DataType::Void) {
            // throw runtime_error("Line " + to_string(cur_token.line) + " : non-void function must return a value");
        } else if (current_return_type != DataType::Void && value.type != current_return_type) {
            // 如果型態不一致，嘗試進行型態轉換
            if (current_return_type == DataType::Float && value.type == DataType::Int) {
                value = Variable(static_cast<double>(get<int>(value.val)));
            } else if (current_return_type == DataType::Int && value.type == DataType::Float) {
                value = Variable(static_cast<int>(get<double>(value.val)));
            } else if (current_return_type == DataType::Bool && (value.type == DataType::Int || value.type == DataType::Float)) {
                value = Variable(bool(value)); // 依賴內建型態轉換 explicit operator bool
            } else {
                throw runtime_error("Line " + to_string(cur_token.line) + " : return type mismatch (" + enum_to_DataType(value.type) + " to " + enum_to_DataType(current_return_type) + ")");
            }
        }

        // 強制完成解析
        if (cur_token.val != ";") {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }
        next();
        throw ReturnException{value};
    }

    ReturnState parse_statement(bool sub_statement = false, bool reset_after_statement = true) {
        static const unordered_map<string, DataType> type_map = {
            {"int", DataType::Int},
            {"float", DataType::Float},
            {"bool", DataType::Bool},
            {"char", DataType::Char},
            {"string", DataType::String}
        };
        ReturnState return_states;
        return_states.clear();

        Token id_token = lexer.peek_token(1);
        string mark = lexer.peek_token(2).val;
        vector<StatePair> states;

        if (cur_token.val == "(") {
            next();
            parse_statement(true, false);
            if (cur_token.val != ")") {
                throw runtime_error(
                    "Line " + to_string(cur_token.line) +
                    " : unexpected token : '" + cur_token.val + "'"
                );
            }
            return_states.push({"", State::Statement});
            require_semicolon = true;
            next();
        } else if (cur_token.type == TokenType::Ident &&
                   type_map.find(cur_token.val) != type_map.end() &&
                   id_token.type == TokenType::Ident &&
                   keywords.find(id_token.val) == keywords.end()) {
            if (mark == "(") {
                return_states.push(parse_function_declaration());
                require_semicolon = false;
            } else if (mark == ";" || mark == "[" || mark == ",") {
                states = parse_variable_declaration();
                return_states.states.insert(return_states.states.end(), states.begin(), states.end());
                require_semicolon = true;
            } else {
                throw runtime_error(
                    "Line " + to_string(cur_token.line) +
                    " : unexpected token : '" + cur_token.val + "'"
                );
            }
        } else if (cur_token.type == TokenType::Ident &&
                   lexer.peek_token().val == "=" &&
                   cur_env->get(cur_token.val) != nullptr) {
            Token id_token = cur_token;
            next(2);
            Variable result = parse_bool_exp();
            if (!dry_run) {
                cur_env->set(id_token.val, result);
            }
            return_states.push({"", State::Statement});
            require_semicolon = true;
        } else if (cur_token.val == "if" || cur_token.val == "else") {
            parse_if_else();
            return_states.push({"", State::Statement});
            require_semicolon = false;
        } else if (cur_token.val == "while") {
            parse_while();
            return_states.push({"", State::Statement});
            require_semicolon = false;
        } else if (cur_token.val == "cin" || cur_token.val == "cout") {
            parse_io(cur_token.val);
            return_states.push({"", State::Statement});
            require_semicolon = true;
        } else if (cur_token.type == TokenType::Ident &&
                   func_table.find(cur_token.val) != func_table.end()) {
            parse_function_call();
            return_states.push({"", State::Statement});
            require_semicolon = true;
        } else if (cur_token.val == "{") {
            parse_block();
            return_states.push({"", State::Statement});
            require_semicolon = false;
        } else if (cur_token.val == "return") {
            parse_return();
            return_states.push({"", State::Statement});
            return return_states;
        } else {
            parse_bool_exp();
            return_states.push({"", State::Statement});
            require_semicolon = true;
        }

        if (!sub_statement) {
            if (require_semicolon && cur_token.type != TokenType::Semicolon) {
                throw runtime_error(
                    "Line " + to_string(cur_token.line) +
                    " : unexpected token : '" + cur_token.val + "'"
                );
            } else if (cur_token.type == TokenType::Semicolon) {
                if (reset_after_statement) {
                    prev_token = cur_token;
                    cur_token = lexer.get_next_token(1);
                    lexer.finish_outer_statement(cur_token);
                } else {
                    next();
                }
            } else if (reset_after_statement) {
                lexer.finish_outer_statement(cur_token);
            }
        }
        return return_states;
    }
public:
    void set_dry_run(bool mode) { dry_run = mode; } // <-- 關鍵：跳過賦值
    
    Parser(const string &input, int start_line = 1) 
        : lexer(input, start_line) {
        cur_token = lexer.get_next_token();
    }

    bool is_eof() const { return cur_token.type == TokenType::EndOfFile; }

    string get_rest_str() { return lexer.get_rest_str(); }

    void skip_to_newline() {
        lexer.skip_to_newline();
        if (!lexer.get_rest_str().empty()) {
            cur_token = lexer.get_next_token();
        } else {
            cur_token = {TokenType::EndOfFile, ""};
        }
    }

    void reset_line() {
        lexer.reset_line();
        if (cur_token.type != TokenType::EndOfFile) {
            cur_token.line = 1;
        }
    }

    void recover_after_error() {
        lexer.skip_to_newline();
        lexer.reset_line();
        require_semicolon = true;
        if (!lexer.get_rest_str().empty()) {
            cur_token = lexer.get_next_token();
        } else {
            cur_token = {TokenType::EndOfFile, ""};
        }
    }

    void parse_cmd() {
        if (!dry_run) cout << "> "; // 印出 prompt
        auto return_state = parse_statement();
        for (auto &state : return_state.states) {
            if (!dry_run) { // 印出解析紀錄
                if (state.second == State::Definition) {
                    cout << "Definition of " << state.first << " entered ..." << endl;
                } else if (state.second == State::NewDefinition) {
                    cout << "New definition of " << state.first << " entered ..." << endl;
                } else if (state.second == State::Statement) {
                    cout << "Statement executed ..." << endl;
                }
            }
        }
        return_state.clear();
        if (cur_token.type == TokenType::EndOfFile) {
            return;
        }
    }
};

// ========================================Built-in Functions========================================

void ListAllVariables(const vector<Variable> &variables) {
    vector<string> var_names;
    for (const auto &pair : cur_env->ident_table) {
        var_names.push_back(pair.first);
    }
    sort(var_names.begin(), var_names.end());
    for (const auto &name : var_names) {
        cout << name << endl;
    }
}

void ListAllFunctions(const vector<Variable> &functions) {
    vector<string> func_names;
    for (const auto &pair : func_table) {
        func_names.push_back(pair.first);
    }
    sort(func_names.begin(), func_names.end());
    for (const auto &name : func_names) {
        cout << name + "()" << endl;
    }
}

void ListVariable(const vector<Variable> &variables) {
    string name = var_to_string(
        format_params(builtin_func_table["ListVariable"].params, variables)["name"]);
    if (cur_env->get(name) != nullptr) {
        Variable var = *cur_env->get(name);
        if (holds_alternative<shared_ptr<ArrayType>>(var.val)) {
            cout << enum_to_DataType(var.type) << " " << name << "[ " << var.size
                 << " ] ;" << endl;
        } else {
            cout << enum_to_DataType(var.type) << " " << name << " ;" << endl;
        }
    } else {
        cout << "Undefined variable : '" << name << "'" << endl;
    }
}

void ListFunction(const vector<Variable> &functions) {
    string name = var_to_string(
        format_params(builtin_func_table["ListFunction"].params, functions)["name"]);
    if (func_table.find(name) != func_table.end()) {
        Function f = func_table.at(name);
        cout << enum_to_DataType(f.return_type) << " " << name << "( ";
        for (int i = 0; i < f.params.size(); i++) {
            cout << enum_to_DataType(f.params[i].type) << " " << f.params[i].name;
            if (i < f.params.size() - 1) {
                cout << ", ";
            }
        }
        Lexer lexer(f.content);
        cout << " ) " << lexer.pretty_print_block() << endl;
    } else {
        cout << "Undefined function : '" << name << "'" << endl;
    }
}

void Done() {
    cout << "Our-C exited ..." << endl;
    exit(0);
}

void parse_wrapper(Parser &parser) {
      while (!parser.is_eof()) {
        try {
            parser.parse_cmd();
        } catch (const exception &e) {
            cout << e.what() << endl;
            parser.recover_after_error();
        }
    }
}

int main() {
    // ios::sync_with_stdio(false);
    // cin.tie(0);
    cout << fixed << setprecision(3);
    cout << "Our-C running ..." << endl;

    // ifstream file("test/data.txt"); // 檔案測試
    // stringstream ss;
    // ss << file.rdbuf(); // 將整個檔案內容寫入 stringstream
    // string content_ = ss.str();
    // auto start = content_.find_first_of("\n") + 1, end = content_.length();
    // string content = content_.substr(start, end - start + 1);

	string content, _; // 跳過測試
    cin >> _; // 忽略標題
    cin.ignore();
    char c;
    while (cin.get(c)) {
        content += c;
    }
	
    /*
    // 持續讀取輸入，直到遇見 Done() 為止，用於直接將測資餵給 cin
    string line;
    while (getline(cin, line)) {
        content += line + "\n";
        if (line.find("Done()") != string::npos) {
            break;
        }
    }
    */

    Parser parser(content);
    parse_wrapper(parser);
    return 0;
}
