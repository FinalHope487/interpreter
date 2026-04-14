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
#include <variant>
#include <vector>

using namespace std;

bool DEBUG = false;

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

bool is_in(const string &op, const unordered_set<string> &targets);
bool is_in(const string &op, const unordered_map<string, Function> &targets);
bool is_in(const string &op, const unordered_map<string, Variable> &targets);
Variable convert_to_var(const Token tk);
string var_to_string(const Variable &var);
unordered_map<string, Variable>
format_params(const vector<FunctionParam> &params, const vector<Variable> &args);

// 將這些函數加入map中
void ListAllVariables(); // variables sorted (from smallest to greatest)
void ListAllFunctions(); // functions sorted
void ListVariable(const vector<Variable> &variables); // the definition of a particular variable
void ListFunction(const vector<Variable> &functions); // the definition of a particular function
void Done(); // exit the interpreter

enum TokenType {
    Identifier,
    Constant,
    Symbol,
    EndOfFile,
    Undefined,
};

enum DataType {
    Int,
    Float,
    Char,
    String,
    Bool,
    Special,
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
    if (type == TokenType::Identifier) return "Identifier";
    if (type == TokenType::Constant) return "Constant";
    if (type == TokenType::Symbol) return "Symbol";
    if (type == TokenType::EndOfFile) return "EOF";
    if (type == TokenType::Undefined) return "Undefined";
    return "Void";
}

string enum_to_DataType(int type) {
    if (type == DataType::Int) return "int";
    if (type == DataType::Float) return "float";
    if (type == DataType::Char) return "char";
    if (type == DataType::String) return "string";
    if (type == DataType::Bool) return "bool";
    if (type == DataType::Special) return "special";
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

// 定義special型別存放cin cout
struct SpecialType {
    string val;
};
// 陣列：就是一堆 Variable 的集合
// 自訂物件：本質上就是屬性名稱(string)與屬性值(Variable)的映射字典
using ArrayType = vector<Variable>;
using ObjectType = unordered_map<string, Variable>;

struct Variable {
    DataType type;
    // 直接儲存原生型別，並利用 shared_ptr 來管理大型或遞迴結構的記憶體
    variant<monostate, int, double, bool, char, string, SpecialType,
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
    Variable(DataType t, int size = -1, const string &v = "") : type(t), size(size) {
        if (size > -1) {
            val = make_shared<ArrayType>(size, zeroed(t));
        } else {
            if (t == DataType::Int) val = stoi(v.empty() ? "0" : v);
            else if (t == DataType::Float) val = stod(v.empty() ? "0.0" : v);
            else if (t == DataType::Bool) val = (v == "true");
            else if (t == DataType::Char) val = v.empty() ? '\0' : v[0];
            else if (t == DataType::String) val = v;
            else if (t == DataType::Special) val = SpecialType{v};
            else val = monostate{};
        }
        update_type();
    }

    void update_type() {
        if (holds_alternative<int>(val)) type = DataType::Int;
        else if (holds_alternative<double>(val)) type = DataType::Float;
        else if (holds_alternative<bool>(val)) type = DataType::Bool;
        else if (holds_alternative<char>(val)) type = DataType::Char;
        else if (holds_alternative<string>(val)) type = DataType::String;
        else if (holds_alternative<SpecialType>(val)) type = DataType::Special;
        else if (holds_alternative<shared_ptr<ArrayType>>(val)) { /* keep original base type */ }
        else if (holds_alternative<shared_ptr<ObjectType>>(val)) { /* keep original type */ }
        else type = DataType::Void;
    }
    static DataType promote(DataType t1, DataType t2) {
        if (t1 == DataType::String || t2 == DataType::String) return DataType::String;
        if (t1 == DataType::Float || t2 == DataType::Float) return DataType::Float;
        if (t1 == DataType::Int || t2 == DataType::Int) return DataType::Int;
        if (t1 == DataType::Char || t2 == DataType::Char) return DataType::Char;
        if (t1 == DataType::Bool || t2 == DataType::Bool) return DataType::Bool;
        return DataType::Void;
    }

    static Variable zeroed(DataType t) {
        if (t == DataType::Int) return Variable(0);
        if (t == DataType::Float) return Variable(0.0);
        if (t == DataType::Char) return Variable('\0');
        if (t == DataType::String) return Variable(string(""));
        if (t == DataType::Bool) return Variable(false);
        return Variable();
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
                    /* throw runtime_error("Error in operator bool: unsupported type conversion to double"); */
                    return 0.0;
                }
            }, v.val);
        };

        double n1 = extract_numeric_value(var1);
        double n2 = extract_numeric_value(var2);
        bool result = false;

        if (op == "==") result = (abs(n1 - n2) <= ErrorValue);
        else if (op == "!=") result = (abs(n1 - n2) > ErrorValue);
        else if (op == "<") result = (n1 < n2 + ErrorValue);
        else if (op == ">") result = (n1 > n2 - ErrorValue);
        else if (op == "<=") result = (n1 <= n2 + ErrorValue);
        else if (op == ">=") result = (n1 >= n2 - ErrorValue);
        else {
            /* throw runtime_error("Unsupported operator: " + op); */
            return Variable();
        }

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
                /* throw runtime_error("Unsupported type for bool conversion"); */
                return false;
            }},
			this->val);
    }

    Variable operator+() {
        return visit(overloaded{
            [](int i) -> Variable { return Variable(i); },
            [](double d) -> Variable { return Variable(d); },
            [&](const auto &) -> Variable {
                /* throw runtime_error("Error in operator unary +"); */
                return zeroed(this->type);
            }},
        	this->val);
    }

    Variable operator-() {
        return visit(overloaded{
            [](int i) -> Variable { return Variable(-i); },
            [](double d) -> Variable { return Variable(-d); },
            [&](const auto &) -> Variable {
                /* throw runtime_error("Error in operator unary -"); */
                return zeroed(this->type);
            }},
            this->val);
    }

    Variable operator!() {
        return visit(overloaded{
            [](bool b) -> Variable { return Variable(!b); },
            [](const auto &) -> Variable {
                /* throw runtime_error("Error in operator unary !"); */
                return Variable(false);
            }},
        	this->val);
    }

    Variable operator+(const Variable &var2) {
        // Coercion: if either side is string (and not an array), result is string concatenation
        if ((this->type == DataType::String || var2.type == DataType::String) &&
            !holds_alternative<shared_ptr<ArrayType>>(this->val) &&
            !holds_alternative<shared_ptr<ArrayType>>(var2.val)) {
            return Variable(var_to_string(*this) + var_to_string(var2));
        }

        return visit(overloaded{
            [](int a, int b) -> Variable { return Variable(a + b); },
            [](double a, double b) -> Variable { return Variable(a + b); },
            [](int a, double b) -> Variable { return Variable(a + b); },
            [](double a, int b) -> Variable { return Variable(a + b); },
            [](const string &a, const string &b) -> Variable { return Variable(a + b); },
            [](const string &a, char b) -> Variable { return Variable(a + string(1, b)); },
            [](char a, const string &b) -> Variable { return Variable(string(1, a) + b); },
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
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator+"); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator-(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable { return Variable(a - b); },
            [](double a, double b) -> Variable { return Variable(a - b); },
            [](int a, double b) -> Variable { return Variable(a - b); },
            [](double a, int b) -> Variable { return Variable(a - b); },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator-"); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator*(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable { return Variable(a * b); },
            [](double a, double b) -> Variable { return Variable(a * b); },
            [](int a, double b) -> Variable { return Variable(a * b); },
            [](double a, int b) -> Variable { return Variable(a * b); },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator*"); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator/(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable {
                if (b == 0) {
                    /* throw runtime_error("Error in operator/: division by zero"); */
                    return Variable(0);
                }
                if (a % b == 0)
                    return Variable(a / b);
                return Variable(static_cast<double>(a) / b);
            },
            [](double a, double b) -> Variable {
                if (b == 0.0) {
                    /* throw runtime_error("Error in operator/: division by zero"); */
                    return Variable(0.0);
                }
                return Variable(a / b);
            },
            [](int a, double b) -> Variable {
                if (b == 0.0) {
                    /* throw runtime_error("Error in operator/: division by zero"); */
                    return Variable(0.0);
                }
                return Variable(a / b);
            },
            [](double a, int b) -> Variable {
                if (b == 0) {
                    /* throw runtime_error("Error in operator/: division by zero"); */
                    return Variable(0.0);
                }
                return Variable(a / b);
            },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator/"); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator%(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable {
                if (b == 0) {
                    /* throw runtime_error("Error in operator%: division by zero"); */
                    return Variable(0);
                }
                return Variable(a % b);
            },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator%: operands must be integers"); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator==(const Variable &var2) {
        if (!is_comparable(*this, var2)) {
            /* throw runtime_error(...) */
            return Variable(false);
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
            /* throw runtime_error("Error in operator!=: incomparable variable"); */
            return Variable(true); // Technically != if incomparable? But return Variable() is safer. 
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
            /* throw runtime_error("Error in operator>=: incomparable variable"); */
            return Variable(false);
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
            /* throw runtime_error("Error in operator<=: incomparable variable"); */
            return Variable(false);
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
            /* throw runtime_error("Error in operator>: incomparable variable"); */
            return Variable(false);
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
            /* throw runtime_error("Error in operator<: incomparable variable"); */
            return Variable(false);
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

    Variable operator<<(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable {
                if (b <= 0) {
                    /* throw runtime_error("Error in operator<<: shift amount must be greater than zero"); */
                    return Variable(0);
                }
                return Variable(a << b);
            },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator<<: operands must be integers"); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator>>(const Variable &var2) {
        return visit(overloaded{
            [](int a, int b) -> Variable {
                if (b <= 0) {
                    /* throw runtime_error("Error in operator>>: shift amount must be greater than zero"); */
                    return Variable(0);
                }
                return Variable(a >> b);
            },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator>>: operands must be integers"); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator&&(const Variable &var2) {
        return Variable(bool(*this) && bool(var2));
    }

    Variable operator||(const Variable &var2) {
        return Variable(bool(*this) || bool(var2));
    }

    Variable operator&(const Variable &var2) {
        // int char bool
        return visit(overloaded{
            [](const int &a, const int &b) -> Variable { return Variable(a & b); },
            [](const int &a, const char &b) -> Variable { return Variable(a & b); },
            [](const int &a, const bool &b) -> Variable { return Variable(a & b); },
            [](const char &a, const int &b) -> Variable { return Variable(a & b); },
            [](const char &a, const char &b) -> Variable { return Variable(a & b); },
            [](const char &a, const bool &b) -> Variable { return Variable(a & b); },
            [](const bool &a, const int &b) -> Variable { return Variable(a & b); },
            [](const bool &a, const char &b) -> Variable { return Variable(a & b); },
            [](const bool &a, const bool &b) -> Variable { return Variable(a & b); },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator&: invalid data type ..."); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator^(const Variable &var2) {
        // int char bool
        return visit(overloaded{
            [](const int &a, const int &b) -> Variable { return Variable(a ^ b); },
            [](const int &a, const char &b) -> Variable { return Variable(a ^ b); },
            [](const int &a, const bool &b) -> Variable { return Variable(a ^ b); },
            [](const char &a, const int &b) -> Variable { return Variable(a ^ b); },
            [](const char &a, const char &b) -> Variable { return Variable(a ^ b); },
            [](const char &a, const bool &b) -> Variable { return Variable(a ^ b); },
            [](const bool &a, const int &b) -> Variable { return Variable(a ^ b); },
            [](const bool &a, const char &b) -> Variable { return Variable(a ^ b); },
            [](const bool &a, const bool &b) -> Variable { return Variable(a ^ b); },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator^: invalid data type ..."); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator|(const Variable &var2) {
        // int char bool
        return visit(overloaded{
            [](const int &a, const int &b) -> Variable { return Variable(a | b); },
            [](const int &a, const char &b) -> Variable { return Variable(a | b); },
            [](const int &a, const bool &b) -> Variable { return Variable(a | b); },
            [](const char &a, const int &b) -> Variable { return Variable(a | b); },
            [](const char &a, const char &b) -> Variable { return Variable(a | b); },
            [](const char &a, const bool &b) -> Variable { return Variable(a | b); },
            [](const bool &a, const int &b) -> Variable { return Variable(a | b); },
            [](const bool &a, const char &b) -> Variable { return Variable(a | b); },
            [](const bool &a, const bool &b) -> Variable { return Variable(a | b); },
            [&](const auto &, const auto &) -> Variable {
                /* throw runtime_error("Error in operator|: invalid data type ..."); */
                return zeroed(promote(this->type, var2.type));
            }},
        this->val, var2.val);
    }

    Variable operator+=(const Variable &var2) { return *this = *this + var2; }
    Variable operator-=(const Variable &var2) { return *this = *this - var2; }
    Variable operator*=(const Variable &var2) { return *this = *this * var2; }
    Variable operator/=(const Variable &var2) { return *this = *this / var2; }
    Variable operator%=(const Variable &var2) { return *this = *this % var2; }
};

struct ReturnException {
    Variable value;
};

struct FunctionParam {
    DataType type;
    string name;
    int size = -1;
    bool is_ref = false;
};

struct Function {
    DataType return_type;
    vector<FunctionParam> params;
    vector<Token> tokens; // 全部的 token
};

struct Environment {
    unordered_map<string, Variable> ident_table;
    shared_ptr<Environment> parent;

    // 建構子，方便直接指定外層環境
    Environment(shared_ptr<Environment> p = nullptr) : parent(p) {}

    void global_init() {
        declare("cin", Variable{DataType::Special, -1, "cin"});
        declare("cout", Variable{DataType::Special, -1, "cout"});
    }

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
    {"ListAllVariables", {DataType::Void, {}, {}}},
    {"ListAllFunctions", {DataType::Void, {}, {}}},
    {"ListVariable", {DataType::Void, {{DataType::String, "name"}}, {}}},
    {"ListFunction", {DataType::Void, {{DataType::String, "name"}}, {}}},
    {"Done", {DataType::Void, {}, {}}}
};

unordered_map<string, Function> func_table;

const unordered_set<string> symbols = {
    "=", "+=", "-=", "*=", "/=", "%=", "?",
    ":", "&&", "||", "!",  "==", "!=", "<",
    ">", "<=", ">=", "<<", ">>", "+",  "-",
    "*", "/",  "%",  "(",  ")",  ",",  ";",
    "[", "]",  "{",  "}",  "\"", 
};

const unordered_set<string> data_types = {
    "int", "float", "char", "bool", "string", "void"
};

const unordered_set<string> keywords = ([]{
    unordered_set<string> combined = {
        "true",
        "false",
        "if",
        "else",
        "do",
        "while",
        // "for",
        "return",
        "break",
        "continue"
    };
    combined.insert(data_types.begin(), data_types.end());
    return combined;
}());

// unexpected next token types
// unordered_map<TokenType, vector<TokenType>> unexpected_types = {
//     {TokenType::Number, {Sign, Assign, Ident, LParen}},
//     {TokenType::Dot, {Sign, Assign, Ident, LParen, Dot}},
//     {TokenType::Ident, {Sign}},
//     {TokenType::Str, {Sign, Assign, Increment, Decrement, LParen}},
//     {TokenType::Chr, {Sign, Assign, LParen}},
//     {TokenType::Boolean, {Sign, Assign, Increment, Decrement, LParen}}, // 需要檢查
//     {TokenType::Operator, {Operator, Assign, Increment, Decrement, Semicolon}},
//     {TokenType::SignOperator, {Operator, Assign, Increment, Decrement, Semicolon}},
//     {TokenType::Sign, {Operator, Assign, Increment, Decrement}},
//     {TokenType::Assign, {Operator, Assign, Increment, Decrement}},
//     {TokenType::Increment, {Operator, Assign, Increment, Decrement, LParen}},
//     {TokenType::Decrement, {Operator, Assign, Increment, Decrement, LParen}},
//     {TokenType::LParen, {Operator, Assign}},
//     {TokenType::RParen, {Sign, Assign, Increment, Decrement}},
//     {TokenType::LBracket, {Operator, Assign}},
//     {TokenType::RBracket, {Sign}},
//     {TokenType::LBrace, {Operator, Assign}},
//     {TokenType::RBrace, {Sign, Assign, Increment, Decrement}},
//     {TokenType::Comma, {Operator, Assign}},
//     {TokenType::Ref, {Operator, Assign, Increment, Decrement}},
//     {TokenType::Semicolon, {}},
//     {TokenType::EndOfFile, {}},
//     {TokenType::Null, {}},
//     {TokenType::Undefined, {}},
// };

// ========================================Function Definition========================================
// const string& 傳引用(保護正本) const string 傳值(會複製一份副本且保護副本)
bool is_in(const string &str, const unordered_set<string> &targets) {
    return targets.find(str) != targets.end();
}

bool is_in(const string &str, const unordered_map<string, Function> &targets) {
    return targets.find(str) != targets.end();
}

bool is_in(const string &str, const unordered_map<string, Variable> &targets) {
    return targets.find(str) != targets.end();
}

Variable convert_to_var(const Token tk, int size = -1) {
    // 根據 Variable 的設計 (假設 Variable 有 type: DataType 和 val: variant)
    if (tk.type == TokenType::Number) {
        if (tk.val.find('.') != string::npos) {
            // 浮點數
            return Variable{DataType::Float, size, tk.val};
        } else {
            // 整數
            return Variable{DataType::Int, size, tk.val};
        }
    } else if (tk.type == TokenType::Str) {
        return Variable{DataType::String, size, tk.val};
    } else if (tk.type == TokenType::Chr) {
        return Variable{DataType::Char, size, tk.val};
    } else if (tk.type == TokenType::Boolean) {
        return Variable{DataType::Bool, size, tk.val};
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
        [](const SpecialType &s) { return s.val; },
        [](const monostate &) { return string("Null"); },
        [](const auto &) { return string("[Object/Array]"); }
    }, var.val);
}

unordered_map<string, Variable> format_params(const vector<FunctionParam> &params,const vector<Variable> &args) {
    unordered_map<string, Variable> formatted_params;
    static const unordered_map<DataType, vector<DataType>> valid_conversions = {
        {DataType::Int, {DataType::Int, DataType::Float}},
        {DataType::Float, {DataType::Int, DataType::Float}},
    };
    if (params.size() != args.size()) {
        throw runtime_error("Invalid function call: expected " + to_string(params.size()) + " arguments, got " + to_string(args.size()));
    }
    for (int i = 0; i < (int)params.size(); i++) {
        Variable final_arg = args[i];
        if (params[i].type != args[i].type) {
            auto it = valid_conversions.find(params[i].type);
            bool can_convert = false;
            if (it != valid_conversions.end()) {
                for (DataType src_type : it->second) {
                    if (src_type == args[i].type) {
                        can_convert = true;
                        break;
                    }
                }
            }

            if (can_convert && params[i].size == -1 && args[i].size == -1) {
                if (params[i].type == DataType::Int) {
                    final_arg = Variable(static_cast<int>(get<double>(args[i].val)));
                } else if (params[i].type == DataType::Float) {
                    final_arg = Variable(static_cast<double>(get<int>(args[i].val)));
                }
            } else {
                throw runtime_error("Invalid function call: expected " + enum_to_DataType(params[i].type) + " arguments, got " + enum_to_DataType(args[i].type));
            }
        }
        formatted_params[params[i].name] = final_arg;
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
        size_t token_ptr;
    };

    const string text;
    vector<Token> tokens_source;
    bool from_tokens = false;
    size_t token_ptr = 0;

    vector<Checkpoint> checkpoints;
    size_t idx = 0;
    size_t last_token_start_idx = 0;
    int cur_line = 1;
    int last_skipped_newline_count = 0;

    Token cur_token;

    // 只要一個statement結束就是一個新的statement開始 包含空白 換行 註解等
    Token get_a_token(int skip_tokens = 1) {
        if (from_tokens) {
            token_ptr += skip_tokens - 1;
            if (token_ptr < tokens_source.size()) {
                Token tk = tokens_source[token_ptr++];
                last_skipped_newline_count = 0;
                return tk;
            }
            return {TokenType::EndOfFile, "", (int)tokens_source.size() > 0 ? tokens_source.back().line : 1};
        }
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
                // 檢查是否符合 'c' 的格式，其餘情況皆為 unrecognized token (且不吃掉後續字元)
                if (idx + 2 < text.length() && text[idx + 2] == '\'' && text[idx + 1] != '\n') {
                    tk = {TokenType::Chr, string(1, text[idx + 1])};
                    idx += 3;
                } else {
                    tk = {TokenType::Undefined, "'"};
                    idx++;
                }
            } else if (text[idx] == '"') {
                string str_str;
                idx++;
                // double quote 必會閉合
                while (text[idx] != '"') { 
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
                idx++;
                tk = {TokenType::Str, str_str};
            } else if (isdigit(text[idx]) || text[idx] == '.') {
                string num_str;

                if (text[idx] == '.') {
                    tk = {TokenType::Dot, "."};
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
                    else if (c == '%') {idx += 1; tk = {TokenType::Operator, "%"}; continue;}
                    else if (c == '&') {idx += 1; tk = {TokenType::Operator, "&"}; continue;}
                    else if (c == '|') {idx += 1; tk = {TokenType::Operator, "|"}; continue;}
                    else if (c == '^') {idx += 1; tk = {TokenType::Operator, "^"}; continue;}
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
        : text(input), from_tokens(false), idx(0), cur_line(start_line) {}

    Lexer(const vector<Token> &tokens) 
        : text(""), tokens_source(tokens), from_tokens(true), token_ptr(0), cur_line(1) {
        if (!tokens.empty()) cur_line = tokens[0].line;
    }

    size_t get_idx() const { return idx; }
    size_t get_last_token_start_idx() const { return last_token_start_idx; }

    string get_substring(size_t start, size_t end) const {
        if (start < end && end <= text.length()) {
            return text.substr(start, end - start);
        }
        return "";
    }

    Token get_next_token() {
        Token tk = get_a_token(1);
        cur_token = tk;
        return tk;
    }

    Token peek_token(int skip_tokens = 1) {
        size_t start_idx = idx;
        int start_line = cur_line;
        size_t start_last_token_start_idx = last_token_start_idx;
        int start_last_skipped_newline_count = last_skipped_newline_count;
        size_t start_token_ptr = token_ptr;

        Token tk = get_a_token(skip_tokens);

        idx = start_idx;
        cur_line = start_line;
        last_token_start_idx = start_last_token_start_idx;
        last_skipped_newline_count = start_last_skipped_newline_count;
        token_ptr = start_token_ptr;
        return tk;
    }

    string get_rest_str() {
        if (idx < text.length()) {
            return text.substr(idx);
        }
        return "";
    }

    void skip_to_newline() {
        if (from_tokens) {
            // Already at the end?
            if (token_ptr >= tokens_source.size()) return;
            // The current token in the Lexer might be behind our Parser's cur_token.
            // But Parser::recover_after_error will call get_next_token() later.
            // We want skip_to_newline to move the source pointer to the start of the next line.
            int start_line = tokens_source[token_ptr].line;
            while (token_ptr < tokens_source.size() && tokens_source[token_ptr].line == start_line) {
                token_ptr++;
            }
            return;
        }
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
        if (from_tokens) return;
        int next_line = max(1, last_skipped_newline_count);
        cur_line = next_line;
        if (next_token.type != TokenType::EndOfFile) {
            next_token.line = next_line;
        }
    }

    vector<Token> get_a_block() {
        vector<Token> tokens;
        // 先用字元掃描方式抓出完整區塊，避免 get_next_token 影響全域 Lexer 狀態 (如 last_skipped_newline_count)
        if (from_tokens) return tokens; // from_tokens 模式不應進入此方法
        
        while (idx < text.length() && isspace(text[idx])) {
            if (text[idx] == '\n') cur_line++;
            idx++;
        }
        if (idx >= text.length() || text[idx] != '{') return tokens;

        size_t start_idx = idx;
        int brace_count = 1;
        idx++; // 跳過第一個 {
        
        while (idx < text.length() && brace_count > 0) {
            if (text[idx] == '{') brace_count++;
            else if (text[idx] == '}') brace_count--;
            else if (text[idx] == '\n') cur_line++;
            idx++;
        }
        
        string block_str = text.substr(start_idx, idx - start_idx);
        
        // 使用臨時 Lexer 將字串轉為 Token 向量
        Lexer temp_lexer(block_str);
        Token tk;
        do {
            tk = temp_lexer.get_next_token();
            if (tk.type != TokenType::EndOfFile) {
                tokens.push_back(tk);
            }
        } while (tk.type != TokenType::EndOfFile);
        
        return tokens;
    }
    string pretty_print_block(const vector<Token>& tokens) {
        int indent_level = 0;
        string result = "";
        bool is_new_line = true; // 標記目前是否處於新的一行 (用以判斷縮排位置)
        TokenType prev_type = TokenType::Undefined;
        string prev_val = "";
        int brace_count = 0;
    
        for (const auto& tk : tokens) {
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
                    if ((tk.val == "(" || tk.val == "[" || tk.val == "++" || tk.val == "--") && 
                        prev_type == TokenType::Ident) {
                        // 排除 if, while, for 等關鍵字的例外，例如 if (x > 0)
                        if (tk.val == "(") {
                            if (prev_val != "if" && prev_val != "while" && prev_val != "for") {
                                need_space = false;
                            }
                        } else {
                            need_space = false;
                        }
                    } else if (tk.type == TokenType::Ident && (prev_val == "++" || prev_val == "--")) {
                        need_space = false;
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
        checkpoints.push_back({idx, last_token_start_idx, cur_line, last_skipped_newline_count, token_ptr});
    }

    void pop_checkpoint() { checkpoints.pop_back(); }

    void back_to_checkpoint() {
        const Checkpoint &checkpoint = checkpoints.back();
        idx = checkpoint.idx;
        last_token_start_idx = checkpoint.last_token_start_idx;
        cur_line = checkpoint.cur_line;
        last_skipped_newline_count = checkpoint.last_skipped_newline_count;
        token_ptr = checkpoint.token_ptr;
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
    bool is_global = true;


/* 
    [修改] ❌void parse_function_call()
           // 修改：呼叫時的引數傳遞，需相容陣列與參照型態的傳遞。
*/

    void throw_error(int debug_No = 0) {
        // return "unrecognize token with first char" and "unexpected token"
        if (DEBUG) cout << "Debug mode: No. " << debug_No << endl;
        if (cur_token.type != TokenType::Ident && !is_in(string("") + cur_token.val[0], symbols)) {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unrecognized token with first char : '" + cur_token.val[0] + "'");
        } else {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token : '" + cur_token.val + "'");
        }
    }

    void throw_undefined_id_error(Token id_token, int debug_No = 0) {
        if (DEBUG) cout << "Debug id mode: No. " << debug_No << endl;
        throw runtime_error("Line " + to_string(id_token.line) + " : undefined identifier : '" + id_token.val + "'");
    }

    void next() {
        // 第一階段報錯
        if (cur_token.type == TokenType::Undefined) {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unrecognized token with first char : '" + cur_token.val[0] + "'");
        }
        prev_token = cur_token;
        cur_token = lexer.get_next_token();
    }

    Variable* parse_ident_lvalue() {
        // cur_token 應為 ident
        Token id_token = cur_token;
        Variable* target_var = cur_env->get(id_token.val);
        if (target_var == nullptr) throw_undefined_id_error(id_token, 1);
        next();

        // 處理陣列下標 [ ] (不支援多維陣列)
        if (cur_token.val == "[") {
            next();
            Variable index_var = parse_expression();
            if (cur_token.val != "]") throw_error(1);
            next();
            
            if (auto arr_ptr = get_if<shared_ptr<ArrayType>>(&target_var->val)) {
                int idx = 0;
                if (auto i = get_if<int>(&index_var.val)) {
                    idx = *i;
                } else {
                    // throw runtime_error("Line " + to_string(cur_token.line) + " : array index must be an integer");
                }
                
                if (idx >= 0 && idx < (*arr_ptr)->size()) {
                    target_var = &((**arr_ptr)[idx]); 
                } else {
                    // throw runtime_error("Line " + to_string(id_token.line) + " : undefined identifier : '" + id_token.val + "'");
                }
            } else {
                // throw runtime_error("Line " + to_string(id_token.line) + " : '" + id_token.val + "' is not an array");
            }
        }
        return target_var;
    }

    Variable parse_ident_rvalue() {
        // cur_token 
        Token id_token = cur_token;
        if (lexer.peek_token().val == "(") {
            return parse_function_call();
        } else {
            Variable* target_var = cur_env->get(id_token.val);
            if (target_var == nullptr) {
                if (cur_token.type == TokenType::Ident) throw_undefined_id_error(id_token, 2);
                else throw_error(2);
            }
            next(); // 消耗 ident
            Variable result = *target_var;
            if (cur_token.val == "[") {
                next();
                Variable index_var = parse_expression();
                if (cur_token.val != "]") throw_error(3);
                next();
                
                if (auto arr_ptr = get_if<shared_ptr<ArrayType>>(&result.val)) {
                    int idx = 0;
                    if (auto i = get_if<int>(&index_var.val)) {
                        idx = *i;
                    } else if (auto d = get_if<double>(&index_var.val)) {
                        idx = static_cast<int>(*d);
                    } else {
                        // throw runtime_error("Line " + to_string(cur_token.line) + " : array index must be an integer");
                    }
                    
                    if (idx >= 0 && idx < (*arr_ptr)->size()) {
                        result = (**arr_ptr)[idx];
                    } else {
                        // throw runtime_error("Line " + to_string(id_token.line) + " : array index out of bounds");
                    }
                } else {
                    // throw runtime_error("Line " + to_string(id_token.line) + " : '" + id_token.val + "' is not an array");
                }
            }
            return result;
        }
    }

    Variable parse_unary_exp(bool is_signed = false) {
        // num, ident, function call, (), sign, ++, --
        Variable result;
        Token next_token = lexer.peek_token();
        // is_signed 為true時前後不可接++ --
        if (cur_token.val == "(") {
            // if (debug) cout << "Debug unary exp: " << cur_token.val << " " << next_token.val << endl;
            next();
            result = parse_expression();
            // expression 的值為最後一個basic expression的值
            if (cur_token.val == ")") {
                next();
                return result;
            } else {
                throw_error(4);
            }
        }
        // sign TODO: 這裡有問題 考慮移除result都改為 return
        if (is_in(cur_token.val, {"+", "-", "!"})) {
            if (cur_token.val == "+") {
                next();
                return parse_unary_exp(true);
            } else if (cur_token.val == "-") {
                next();
                return -parse_unary_exp(true);
            } else if (cur_token.val == "!") {
                next();
                return !parse_unary_exp(true);
            }
        } else if (cur_token.val == "++" || cur_token.val == "--") {
            string op = cur_token.val;
            if (is_signed) throw_error(35); // 符號運算子後不可接 ++/--
            next(); // 消耗 ++/--
            
            // 語法規範：前置 ++/-- 後面必須接一個 Ident 或 Array Element (L-value)
            if (cur_token.type != TokenType::Ident) throw_error(36);
            
            Token id_token = cur_token;
            // 這裡暫時不支持對陣列元素進行前置 ++/-- (看語法要求，若需支持則調用 lvalue 解析)
            // 為了保持簡單，先實作 Ident 的情況
            Variable *target_var = parse_ident_lvalue(); 
            
            if (op == "++") *target_var = *target_var + Variable{1};
            else *target_var = *target_var - Variable{1};
            
            result = *target_var;
            return result; // 前置運算直接回傳更新後的值
        }

        // num 1, 1., .1, 1.0
        if (cur_token.type == TokenType::Number) {
            auto num_tk = cur_token;
            if (lexer.peek_token().val == ".") {
                next(); // get number
                next(); // get "."
                num_tk.val += cur_token.val;
                if (lexer.peek_token().type == TokenType::Number) {
                    next(); // get digits after dot
                    num_tk.val += cur_token.val;
                }
            }
            result = convert_to_var(num_tk, -1);
            next();
        } else if (cur_token.type == TokenType::Dot) {
            auto num_tk = cur_token; // "."
            Token next_token = lexer.peek_token();
            if (next_token.type == TokenType::Number) {
                next(); // get digits after dot
                num_tk.val += cur_token.val;
            }
            result = convert_to_var(num_tk, -1);
            next();
        // ident or function call
        } else if (cur_token.type == TokenType::Ident) {
            Token id_token = cur_token;
            if (cur_token.val == "cin" || cur_token.val == "cout") {
                next();
                return Variable{DataType::Special, -1, id_token.val};
            }
            
            // 若為關鍵字意味著必然不是變數等 應跳至parse statement被捕捉
            if (is_in(cur_token.val, keywords)) throw_error(37);
            
            if (lexer.peek_token().val == "(") {
                result = parse_function_call();
            } else {
                result = parse_ident_rvalue();
                if (cur_token.val == "++" || cur_token.val == "--") {
                    if (is_signed) throw_error(38);
                    string op = cur_token.val;
                    next(); // 消耗 ++/--
                    
                    if (!dry_run) {
                        Variable *ref = cur_env->get(id_token.val);
                        if (ref) {
                            if (op == "++") *ref = *ref + Variable{1};
                            else *ref = *ref - Variable{1};
                        }
                    }
                }
            }
        } else if (cur_token.type == TokenType::Chr) {
            result = Variable{DataType::Char, -1, cur_token.val};
            next();
        } else if (cur_token.type == TokenType::Str) {
            result = Variable{DataType::String, -1, cur_token.val};
            next();
        } else if (cur_token.type == TokenType::Boolean) {
            result = Variable{DataType::Bool, -1, cur_token.val};
            next();
        } else {
            throw_error(8);
        }
        return result;
    }

    Variable parse_multiplicative_exp() {
        // *, /
        Variable result = parse_unary_exp();
        while (is_in(cur_token.val, unordered_set<string>{"*", "/", "%"})) {
            if (cur_token.val == "*") {
                next();
                result = result * parse_unary_exp();
            }
            if (cur_token.val == "/") {
                next();
                result = result / parse_unary_exp();
            }
            if (cur_token.val == "%") {
                next();
                result = result % parse_unary_exp();
            }
        }
        return result;
    }

    Variable parse_additive_exp() {
        // +, -
        Variable result = parse_multiplicative_exp();
        while (is_in(cur_token.val, unordered_set<string>{"+", "-"})) {
            if (cur_token.val == "+") {
                next();
                result = result + parse_multiplicative_exp();
            }
            if (cur_token.val == "-") {
                next();
                result = result - parse_multiplicative_exp();
            }
        }
        return result;
    }

    Variable parse_shift_exp() {
        // << >> ：cout/cin 為 DataType::Special（SpecialType 存名稱），其餘為位元運算
        Variable result = parse_additive_exp();
        while (is_in(cur_token.val, unordered_set<string>{"<<", ">>"})) {
            const SpecialType *sp = get_if<SpecialType>(&result.val);
            if (sp && sp->val == "cout" && cur_token.val == "<<") {
                // 規範：cout << Expression 的值為 Expression 本身（鏈狀則為最後一個 Expression）
                while (cur_token.val == "<<") {
                    next();
                    Variable out = parse_expression();
                    // cout << "test | out.type: " << enum_to_DataType(out.type) << endl;
                    // cout << var_to_string(out) << endl;
                    result = out;
                }
            } else if (sp && sp->val == "cin" && cur_token.val == ">>") {
                while (cur_token.val == ">>") {
                    next();
                    result = parse_expression();
                    // 直譯器規範未實作 cin
                }
            } else {
                if (cur_token.val == "<<") {
                    next();
                    result = result << parse_additive_exp();
                } else if (cur_token.val == ">>") {
                    next();
                    result = result >> parse_additive_exp();
                }
            }
        }
        return result;
    }   

    Variable parse_relational_exp() {
        // < <= > >=
        Variable result = parse_shift_exp();
        static const unordered_map<string, function<Variable(Variable, Variable)>>
            op_map = {
                {">", [](Variable a, Variable b) { return a > b; }},
                {"<", [](Variable a, Variable b) { return a < b; }},
                {">=", [](Variable a, Variable b) { return a >= b; }},
                {"<=", [](Variable a, Variable b) { return a <= b; }}
            };
        auto it = op_map.find(cur_token.val);
        if (it != op_map.end()) {
            next();
            return it->second(result, parse_shift_exp());
        }
        return result;
    }
  
    Variable parse_equality_exp() {
        // == !=
        Variable result = parse_relational_exp();
        while (is_in(cur_token.val, unordered_set<string>{"==", "!="})) {
            if (cur_token.val == "==") {
                next();
                result = result == parse_relational_exp();
            }
            if (cur_token.val == "!=") {
                next();
                result = result != parse_relational_exp();
            }
        }
        return result;
    }

    // Variable parse_bitwise_and_exp()
    // Variable parse_bitwise_xor_exp()
    // Variable parse_bitwise_or_exp() 

    // placeholder (因為PAL不支援位元運算)
    Variable parse_bitwise_exp() {
        Variable result = parse_equality_exp();
        while (is_in(cur_token.val, {"&", "|", "^"})) {
            if (cur_token.val == "&") {
                next();
                result = parse_equality_exp();
            }
            if (cur_token.val == "|") {
                next();
                result = parse_equality_exp();
            }
            if (cur_token.val == "^") {
                next();
                result = parse_equality_exp();
            }
        }
        return result;
    }
  
    Variable parse_logical_and_exp() {
        // &&
        Variable result = parse_bitwise_exp();
        while (cur_token.val == "&&") {
            next();
            result = result && parse_bitwise_exp();
        }
        return result;
    }
  
    Variable parse_logical_or_exp() {
        // ||
        Variable result = parse_logical_and_exp();
        while (cur_token.val == "||") {
            next();
            result = result || parse_logical_and_exp();
        }
        return result;
    }
    
    Variable parse_conditional_exp() { 
        // ? :
        Variable result = parse_logical_or_exp();
        if (cur_token.val == "?") {
            // 先備份進入這個 AST 節點前的 dry_run 狀態
            bool prev_dry_run = get_dry_run(); 
            try {
                next();
                Variable true_val, false_val;
                if (!bool(result)) {
                    set_dry_run(true);
                    true_val = parse_basic_exp(); 
                    set_dry_run(prev_dry_run); // 嚴格還原，而非設為 false
                } else {
                    true_val = parse_basic_exp(); 
                }
                if (cur_token.val != ":") throw_error(9);
                next();
                
                if (bool(result)) {
                    set_dry_run(true);
                    false_val = parse_basic_exp();
                    set_dry_run(prev_dry_run); // 嚴格還原，而非設為 false
                } else {
                    false_val = parse_basic_exp();
                }

                if (bool(result)) return true_val;
                else return false_val;
                
            } catch (...) {
                set_dry_run(prev_dry_run); 
                throw;
            }
        }
        return result; 
    }

    Variable parse_basic_exp() {
        // 賦值 = += -= *= /= %=
        // BasicExpression : Identifier [ '[' Expression ']' ] AssignmentOperator BasicExpression 
        //                 | ConditionalExpression
        bool is_assign = false;
        // TODO: 改掉此處邏輯
        if (cur_token.type == TokenType::Ident) {
            Token next_token = lexer.peek_token(1);
            if (is_in(next_token.val, {"=", "+=", "-=", "*=", "/=", "%="})) {
                is_assign = true;
            } else if (next_token.val == "[") {
                int i = 2;
                int b_count = 1;
                while (b_count > 0) {
                    Token t = lexer.peek_token(i++);
                    if (t.type == TokenType::EndOfFile || t.type == TokenType::Semicolon) break;
                    if (t.val == "[") b_count++;
                    else if (t.val == "]") b_count--;
                }
                if (b_count == 0) {
                    Token t_after = lexer.peek_token(i);
                    if (is_in(t_after.val, {"=", "+=", "-=", "*=", "/=", "%="})) {
                        is_assign = true;
                    }
                }
            }
        }

        if (is_assign) {
            Variable* lval = parse_ident_lvalue();
            string op = cur_token.val;
            next();
            Variable exp_val = parse_basic_exp();
            Variable result;
            if (!dry_run) {
                if (op == "=") *lval = exp_val;
                else if (op == "+=") *lval += exp_val;
                else if (op == "-=") *lval -= exp_val;
                else if (op == "*=") *lval *= exp_val;
                else if (op == "/=") *lval /= exp_val;
                else if (op == "%=") *lval %= exp_val;
                result = *lval;
            }
            return result;
        } else {
            return parse_conditional_exp();
        }
    }

    Variable parse_expression() {
        // ,
        Variable result = parse_basic_exp();
        while (cur_token.val == ",") {
            next();
            result = parse_basic_exp();
        }
        return result;
    }

    bool parse_condition() {
        // cur_token is '(' end after ')'
        // ( Expression )
        if (cur_token.val != "(") {
            throw_error(10);
        }
        next();
        Variable result = parse_expression();
        if (DEBUG && cur_token.val == ")") cout << "Debug condition: " << cur_token.val << " " << lexer.peek_token().val << endl; 
        if (cur_token.val != ")") {
            throw_error(11);
        }
        next();
        return bool(result);
    }

    void skip_token() {
        prev_token = cur_token;
        cur_token = lexer.get_next_token();
    }

    void process_parens_in_skip() {
        if (cur_token.val == "(" || cur_token.val == "[" || cur_token.val == "{") {
            parens_stack.push_back(cur_token);
        } else if (cur_token.val == ")" || cur_token.val == "]" || cur_token.val == "}") {
            if (!parens_stack.empty()) {
                bool match = false;
                if (cur_token.val == ")" && parens_stack.back().val == "(") match = true;
                if (cur_token.val == "]" && parens_stack.back().val == "[") match = true;
                if (cur_token.val == "}" && parens_stack.back().val == "{") match = true;
                if (match) parens_stack.pop_back();
                else throw_error(12);
            } else {
                throw_error(13);
            }
        }
    }

    void skip_statement() {
        // TODO 這裡會如何處理邏輯
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
        } else if (cur_token.val == "do") {
            skip_token();
            skip_statement();
            if (cur_token.val == "while") {
                skip_token();
                if (cur_token.val == "(") {
                    parens_stack.push_back(cur_token);
                    skip_token();
                    while (!parens_stack.empty() && cur_token.type != TokenType::EndOfFile) {
                        process_parens_in_skip();
                        skip_token();
                    }
                }
                if (cur_token.val == ";") skip_token();
                else throw_error(14);
            } else {
                throw_error(15);
            }
        } else {
            while (cur_token.val != ";" && cur_token.type != TokenType::EndOfFile) {
                process_parens_in_skip();
                skip_token();
            }
            if (cur_token.val == ";") {
                if (!parens_stack.empty()) {
                    throw_error(16);
                }
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
                temp_parser.set_global(false);
                temp_parser.parse_statement(false);
            } catch (const exception &e) {
                cur_env = old_env;
                throw runtime_error(e.what());
            }

            cur_env = old_env;
        }
    }
    void parse_if_else() {
        // start at "if", end after "}"
        bool condition_met = false;
        next();
        bool condition = parse_condition();
        if (condition) {
            parse_statement(false);
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
                    parse_statement(false);
                    condition_met = true;
                } else {
                    skip_statement();
                }
            } else {
                if (!condition_met) {
                    parse_statement(false);
                    condition_met = true;
                } else {
                    skip_statement();
                }
                break;
            }
        }
    }

    void parse_while() {
        // start at "while", end after "}"
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
                parse_statement(false);
                lexer.back_to_checkpoint();
                next();
                condition = parse_condition();
                if (!condition || execution_steps > MAX_STEPS) {
                    skip_statement();
                }
            }
            lexer.pop_checkpoint();
        } else {
            throw_error(17);
        }
    }

    void parse_do_while() {
        // start at "do", end after ";"
        bool condition;
        if (cur_token.val == "do") {
            lexer.push_checkpoint(); // 從do後面開始
            next();

            int execution_steps = 0;
            do {
                execution_steps++;
                bool old_is_global = is_global;
                is_global = false;
                parse_statement(false);
                is_global = old_is_global;
                if (cur_token.val != "while") {
                    throw_error(18);
                }
                next();
                condition = parse_condition();
                if (cur_token.val != ";") throw_error(19);
                // 關鍵修改：這裡不再消耗 ';'，保留到statement解析中
                if (condition && execution_steps <= MAX_STEPS) {
                    lexer.back_to_checkpoint();
                    next();
                } else {
                    // 不消耗 next(); 留給 statement 處理
                    break;
                }
            } while (true);
            lexer.pop_checkpoint();
        } else {
            throw_error(20);
        }
    }

    void parse_block() {
        // start at "{", end at "}"
        auto new_env = make_shared<Environment>(cur_env);
        cur_env = new_env;
        try {
            if (cur_token.val == "{") {
                bool old_is_global = is_global;
                is_global = false;
                next();
                while (cur_token.val != "}") {
                    parse_statement(false);
                }
                is_global = old_is_global;
                next();
            }
        } catch (runtime_error &e) {
            cur_env = cur_env->parent;
            throw runtime_error(string(e.what()));
        } catch (...) {
            cur_env = cur_env->parent;
            throw;
        }
        cur_env = cur_env->parent;
    }
    void parse_function_block(const vector<Token> &tokens, unordered_map<string, Variable> formatted_params) {
        // start at "{", end at "}"
        // CompoundStatement : '{' { LocalDeclaration | Statement } '}'
        auto new_env = make_shared<Environment>(cur_env);
        cur_env = new_env;
        for (auto &param : formatted_params) {
            cur_env->declare(param.first, param.second);
        }

        Parser block_parser(tokens);
        block_parser.current_return_type = this->current_return_type;

        try {
            if (block_parser.cur_token.val == "{") {
                block_parser.set_global(false);
                block_parser.next();
                while (block_parser.cur_token.val != "}") {
                    block_parser.parse_statement(false);
                }
            }
        } catch (...) {
            cur_env = cur_env->parent;
            throw;
        }
        cur_env = cur_env->parent;
    }
    vector<StatePair> parse_variable_declaration(DataType type) {
        // start at ident, end at ";"
        // <VariableDeclaration> ::= <Type> <Ident> [ "[" <Expression> "]" ] { "," <Ident> [ "[" <Expression> "]" ] } ";"
        vector<StatePair> state_pairs;
        
        struct PendingDeclaration {
            string name;
            Variable var;
            State state;
        };
        vector<PendingDeclaration> pending_declarations;

        auto parse_an_identifier_declaration = [&]() -> void {
            // like { ident , } 假設是ident 若不是則會在後面被next捕捉到
            if (keywords.find(cur_token.val) != keywords.end() || is_in(cur_token.val, symbols)) {
                throw_error(21);
            }
            State cur_state = State::Definition;
            Token id_token = cur_token;
            if (cur_env->get(id_token.val) != nullptr) {
                cur_state = State::NewDefinition;
            }

            Variable new_var;
            if (lexer.peek_token().val == "[") {
                next(); // move to '['
                next(); // move to expression start
                Variable size_var = parse_expression();
                int size = -1;
                if (auto i = get_if<int>(&size_var.val)) {
                    size = *i;
                }
                if (cur_token.val != "]") {
                    throw_error(22);
                }
                next();
                new_var = Variable{type, size};
            } else {
                next();
                new_var = Variable{type};
            }
            pending_declarations.push_back({id_token.val, new_var, cur_state});
        };

        parse_an_identifier_declaration();
        while (cur_token.val == ",") {
            next();
            parse_an_identifier_declaration();
        }

        // 只有在全部解析成功後，才正式宣告到環境中
        if (cur_token.val != ";") throw_error(24);
        for (auto &decl : pending_declarations) {
            cur_env->declare(decl.name, decl.var);
            state_pairs.push_back({decl.name, decl.state});
        }

        return state_pairs;
    }

    vector<FunctionParam> parse_function_declaration_params() {
        // start at "(", end after ")"
        // <Params> ::= ( <Type> <Ident> { , <Type> <Ident> } | <Empty> | <VOID> )
        vector<FunctionParam> params;
        auto parse_a_param = [&]() -> void {
            DataType type = DataType_to_enum(cur_token.val);
            next(); // move to ident
            int size = -1;
            bool is_ref = false;
            if (cur_token.val == "&") {
                is_ref = true;
                next();
            }
            if (cur_token.type == TokenType::Ident && keywords.find(cur_token.val) == keywords.end()) {
                Token id_token = cur_token;
                next();
                if (cur_token.val == "[") {
                    next(); // move to size
                    Variable size_var = parse_expression();
                    if (auto i = get_if<int>(&size_var.val)) {
                        size = *i;
                    } else {
                        throw_error(25);
                    }
                    if (cur_token.val != "]") {
                        throw_error(26);
                    }
                    next();
                }
                params.push_back({type, id_token.val, size, is_ref});
            } else {
                throw_error(27);
            }
        };
        if (cur_token.val == "(") {
            // () and (void)
            if (lexer.peek_token().val == ")") {
                next(); // 走到 )
            } else if (lexer.peek_token().val == "void" && lexer.peek_token(2).val == ")") {
                next(); // 走到 void
                next(); // 走到 )
            } else {
                next();
                parse_a_param();
                while (cur_token.val == ",") {
                    next();
                    parse_a_param();
                }
                if (cur_token.val != ")") {
                    throw_error(28);
                }
                // next(); // 消耗 ) 為了保留 '{' 在原位給 get_a_block
            }
        } else {
            throw_error(29);
        }
        return params;
    }

    StatePair parse_function_declaration(DataType type) {
        // start at ident, next token is "("
        // FunctionDefinition           : '(' [ VOID | FormalParameters ] ')' CompoundStatement
        // Example: (int a, float b[5]) { ... }
        // TODO: void 處理
        State state = State::Definition;
        string name = cur_token.val;
        if (func_table.find(name) != func_table.end()) state = State::NewDefinition;
        next(); // move to "("

        vector<FunctionParam> params = parse_function_declaration_params();
        
        // 宣告時先行驗證語法 (dry run)，同時檢查未定義變數
        bool old_dry_run = dry_run;
        dry_run = true;
        DataType old_return_type = current_return_type;
        current_return_type = type;
        
        // 建立一個臨時的作用域，把函數參數放進去，避免 dry_run 報出 "未定義變數"
        auto temp_env = make_shared<Environment>(cur_env);
        for (const auto& p : params) {
            temp_env->declare(p.name, Variable(p.type, p.size));
        }
        auto parent_env = cur_env;
        cur_env = temp_env; // 切換到臨時作用域，當前變數查找會從這裡開始往上 (get 方法)
        
        // 記住準備進入大括號前的起點位置 (此時 cur_token 是 ')')
        lexer.push_checkpoint();
        bool old_is_global = is_global;
        is_global = false;

        try {
            next(); // 消耗 ')'，載入 '{'
            if (cur_token.val != "{") {
                throw_error(40); // 應該要是 '{'
            }
            next(); // 進到 '{' 裡面
            while (cur_token.val != "}") {
                parse_statement(false);
            }
            next(); // 離開 '}'
        } catch (ReturnException &re) {
             // 如果在 try 內拋出 return_exception 代表語法大致正確走到結尾，我們依然因為不在執行器內，所以當成語法正確處理
             // 但正常 parse_statement 解析 return 應該要接得住，如果漏出來這裡就捕捉避免錯誤
        } catch (...) {
            // 語法有錯或未定義變數拋出 Exception！
            // 拋棄掉所有我們設定好的東西，並把錯誤丟到最外面製造連鎖崩潰效應
            is_global = old_is_global;
            dry_run = old_dry_run;
            current_return_type = old_return_type;
            cur_env = parent_env;
            lexer.pop_checkpoint(); // 因為拋出例外放棄註冊，我們直接丟棄這個 checkpoint，不退回！
            throw; // 再次丟出！讓 parse_wrapper 接收
        }
        is_global = old_is_global;
        
        // 走到這裡代表 function block 裡面沒有任何語法錯誤！安全！
        dry_run = old_dry_run;
        current_return_type = old_return_type;
        cur_env = parent_env;
        
        // 將 lexer 退回大括號的起點
        lexer.back_to_checkpoint();
        lexer.pop_checkpoint(); // 用完即丟
        
        // 這次我們可以安心地把整包抓出來當庫存
        vector<Token> tokens = lexer.get_a_block();
        cur_token = lexer.get_next_token(); // 同步下一顆 token
        
        func_table[name] = Function{type, params, tokens};
        return {name + "()", state}; // 輸入至註冊表時不需顯示參數
    }

    vector<Variable> parse_function_params() {
        // start at '(', end at ';'
        // <Params> ::= "(" <Expression> { , <Expression> } ")" | "()"
        // 被函式呼叫的參數，也就是輸入參數
        vector<Variable> params;
        if (cur_token.val == "(") {
            next();
            if (cur_token.val == ")") {
                next();
                return params;
            } else {
                params.push_back(parse_conditional_exp());
                while (cur_token.val == ",") {
                    next();
                    params.push_back(parse_conditional_exp());
                }
                if (cur_token.val != ")") {
                    throw_error(30);
                }
                next();
            }
        } else {
            throw_error(31);
        }
        return params;
    }

    Variable parse_function_call() {
        // start at ident, end at ";"
        // FunctionCall ::= Identifier [ '(' [ Parameters ] ')' ]
        Token function_token = cur_token;
        string function_name = cur_token.val;
        next();
        vector<Variable> params = parse_function_params();
        
        // 在輸入數量不符的參數時一律不執行也不報錯
        if (function_name == "ListAllVariables") {
            if (!dry_run && params.size() == 0) ListAllVariables(); 
        } else if (function_name == "ListAllFunctions") {
            if (!dry_run && params.size() == 0) ListAllFunctions();
        } else if (function_name == "ListVariable") {
            if (!dry_run && params.size() == 1) ListVariable(params);
        } else if (function_name == "ListFunction") {
            if (!dry_run && params.size() == 1) ListFunction(params);
        } else if (function_name == "Done") {
            if (!dry_run && params.size() == 0) Done(); 
        // TODO: 如何檢查自訂函數的輸入值? 此時只須確保文法正確
        } else if (func_table.find(function_name) != func_table.end()) {
            unordered_map<string, Variable> formatted_params = format_params(func_table[function_name].params, params);
            DataType old_return_type = current_return_type;
            current_return_type = func_table[function_name].return_type;
            if (!dry_run) {
                try {
                    parse_function_block(func_table[function_name].tokens, formatted_params);
                } catch (ReturnException &re) {
                    current_return_type = old_return_type; 
                    return re.value;
                }
            }
            current_return_type = old_return_type;
            // 函數輸出可以不用依照宣告型別
            // TODO: 在輸入型別或數量不符時印出警告測試訊息 (意味著程式本身有錯) 因測資保證輸入正確型別及數量
            if (func_table[function_name].return_type == DataType::Void) return Variable();
            return Variable(func_table[function_name].return_type);
        } else {
            throw_undefined_id_error(function_token);
        }
        return Variable();
    }

    void parse_return() {
        // <Return> ::= "return" [ <BoolExpression> | <Expression> ] ";"
        next();
        Variable value;
        // 處理回傳型態
        if (cur_token.val != ";") value = parse_expression();
        if (cur_token.val != ";") throw_error(32);
        next();
        if (!dry_run) throw ReturnException{value};
    }

    ReturnState parse_statement(bool reset_after_statement = true) {
        // 進入所有方法的第一個token必須保證是正確的
        /*
        Statement   : ';' 
                    | Expression ';' 
                    | RETURN [ Expression ] ';'                             (return)
                    | CompoundStatement                                     ('{')
                    | IF '(' Expression ')' Statement [ ELSE Statement ]    ('if')
                    | WHILE '(' Expression ')' Statement                    ('while')
                    | DO Statement WHILE '(' Expression ')' ';'             ('do')
        */
        static const unordered_map<string, DataType> type_map = {
            {"int", DataType::Int},
            {"float", DataType::Float},
            {"bool", DataType::Bool},
            {"char", DataType::Char},
            {"string", DataType::String},
            {"void", DataType::Void} // 支援 void 作為 function declaration
        };
        ReturnState return_states;
        return_states.clear();

        Token next_token = lexer.peek_token(1);
        vector<StatePair> states;
        require_semicolon = true; // 預設每個 statement 都需要分號，除了 block or flow control

        // 條件 1: Function Definition / Variable Declaration (非純 Statement 標準文法，但為 Global Scope 宣告入口)
        Token type_token = cur_token;
        if (type_token.type == TokenType::Ident && type_map.find(type_token.val) != type_map.end()) {    
            if (!is_global && type_token.val == "void") throw_error(41); // DataType only, VOID only global function
            next(); // 現在在ident
            if (cur_token.type != TokenType::Ident) throw_error(33);
            if (lexer.peek_token().val == "(") {
                if (!is_global) throw_error(42); // FunctionDefinition only in GlobalDefinition
                // 實作: Function Declaration 以Ident進入function / type ident "("
                return_states.push(parse_function_declaration(type_map.at(type_token.val)));
                require_semicolon = false;
            } else {
                if (type_token.val == "void") throw_error(43); // VariableDeclaration must use DataType (no VOID)
                // 實作: Variable Declaration / type ident ...
                states = parse_variable_declaration(type_map.at(type_token.val));
                return_states.states.insert(return_states.states.end(), states.begin(), states.end());
                require_semicolon = true;
            }
        // 條件 2: IF '(' Expression ')' Statement [ ELSE Statement ]
        } else if (cur_token.val == "if") {
            bool old_is_global = is_global;
            is_global = false;
            parse_if_else();
            is_global = old_is_global;
            return_states.push({"", State::Statement});
            require_semicolon = false; // 已包含最後的 ';' 處理
        // 條件 3: WHILE '(' Expression ')' Statement
        } else if (cur_token.val == "while") {
            bool old_is_global = is_global;
            is_global = false;
            parse_while();
            is_global = old_is_global;
            return_states.push({"", State::Statement});
            require_semicolon = false; 
        // 條件 4: DO Statement WHILE '(' Expression ')' ';'
        } else if (cur_token.val == "do") {
            bool old_is_global = is_global;
            is_global = false;
            parse_do_while();
            is_global = old_is_global;
            return_states.push({"", State::Statement});
            require_semicolon = false; // parse_do_while 內部已經處理了其結尾必備的 ';'
        // 條件 5: CompoundStatement ( { ... } )
        } else if (cur_token.val == "{") {
            parse_block();
            return_states.push({"", State::Statement});
            require_semicolon = false; // block 結束符號是 '}' 故不需要分號
        // 條件 6: RETURN [ Expression ] ';'
        } else if (cur_token.val == "return") {
            parse_return();
            // parse_return 會 throw ReturnException，正常運作時不會執行到這裡
            return_states.push({"", State::Statement});
            return return_states;
        // 條件 7: ';' (Empty Statement)
        } else if (cur_token.type == TokenType::Semicolon) {
            return_states.push({"", State::Statement});
            require_semicolon = true; // 由底下的掃描邏輯來消耗這個 ';'
        // 條件 8: Expression ';'
        } else {
            parse_expression();
            return_states.push({"", State::Statement});
            require_semicolon = true;
        }

        // 後處理：分號檢測與狀態更新 TODO: 找出這裡的問題
        if (require_semicolon && cur_token.type != TokenType::Semicolon) {
            throw_error(34);
        } else if (require_semicolon && cur_token.type == TokenType::Semicolon) {
            if (reset_after_statement) {
                prev_token = cur_token;
                cur_token = lexer.get_next_token();
                lexer.finish_outer_statement(cur_token);
            } else {
                next(); // 單純略過 ';'
            }
        } else if (reset_after_statement) {
            lexer.finish_outer_statement(cur_token); // 無需分號但需要 reset 行狀態時
        }
        return return_states;
    }
public:
    void set_dry_run(bool mode) { dry_run = mode; }
    bool get_dry_run() const { return dry_run; }
    void set_global(bool mode) { is_global = mode; }
    bool get_global() const { return is_global; }
    
    Parser(const string &input, int start_line = 1) 
        : lexer(input, start_line) {
        cur_token = lexer.get_next_token();
    }

    Parser(const vector<Token> &tokens) 
        : lexer(tokens) {
        cur_token = lexer.get_next_token();
    }

    bool is_eof() const { return cur_token.type == TokenType::EndOfFile; }

    string get_rest_str() { return lexer.get_rest_str(); }

    void skip_to_newline() {
        lexer.skip_to_newline();
        if (!lexer.get_rest_str().empty()) 
            cur_token = lexer.get_next_token();
        else 
            cur_token = {TokenType::EndOfFile, ""};
    }

    void reset_line() {
        lexer.reset_line();
        if (cur_token.type != TokenType::EndOfFile) cur_token.line = 1;
    }

    void recover_after_error() {
        lexer.skip_to_newline();
        lexer.reset_line();
        require_semicolon = true;
        is_global = true;
        if (!lexer.get_rest_str().empty()) {
            if (DEBUG) cout << "Debug: " << cur_token.val << endl;
            cur_token = lexer.get_next_token();
            if (DEBUG) cout << "Debug: " << cur_token.val << endl;
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

void ListAllVariables() {
    vector<string> var_names;
    for (const auto &pair : cur_env->ident_table) {
        if (pair.second.type != DataType::Special) {
            var_names.push_back(pair.first); // name
        }
    }
    sort(var_names.begin(), var_names.end());
    for (const auto &name : var_names) {
        cout << name << endl;
    }
}

void ListAllFunctions() {
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
        // TODO: 需要實作ArrayType邏輯
        // if (holds_alternative<shared_ptr<ArrayType>>(var.val)) {
        if (var.size != -1) {
            cout << enum_to_DataType(var.type) << " " << name << "[ " << var.size
                 << " ] ;" << endl;
        } else if (var.type == DataType::Special) {
            cout << name << " is a system variable.";
        } else {
            cout << enum_to_DataType(var.type) << " " << name << " ;" << endl;
        }
    } else {
        cout << "Undefined variable : '" << name << "'" << endl; // 理論上不會觸發這條路徑
    }
}

void ListFunction(const vector<Variable> &functions) {
    string name = var_to_string(
        format_params(builtin_func_table["ListFunction"].params, functions)["name"]);
    if (func_table.find(name) != func_table.end()) {
        Function f = func_table.at(name);
        cout << enum_to_DataType(f.return_type) << " " << name << "( ";
        for (int i = 0; i < (int)f.params.size(); i++) {
            cout << enum_to_DataType(f.params[i].type) << " ";
            if (f.params[i].is_ref) cout << "& ";
            cout << f.params[i].name;
            if (f.params[i].size != -1) cout << "[ " << f.params[i].size << " ]";
            if (i < (int)f.params.size() - 1) {
                cout << ", ";
            }
        }
        Lexer lexer("");
        cout << " ) " << lexer.pretty_print_block(f.tokens) << endl;
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
        } catch (ReturnException &re) {
            // cout << re.value << endl;
            // parser.recover_after_error();
            cout << "Statement executed ..." << endl;
        }
    }
}

int main() {
    cout << fixed << setprecision(3);
    global_env->global_init();
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

    Parser parser(content);
    parse_wrapper(parser);
    return 0;
}
