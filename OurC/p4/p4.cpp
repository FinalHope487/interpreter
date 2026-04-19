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
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// ========================================definition========================================

const double ErrorValue = 1e-9;

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
    Identifier, Constant, Symbol, EndOfFile, Undefined,
};

enum DataType {
    Int, Float, Char, String, Bool, Special, Void,
};

enum State { 
    Definition, NewDefinition, Statement, 
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
    int start_idx = 0;
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
    // const auto 型別代表未支援 會引發錯誤的型別
    variant<monostate, int64, double, bool, char, string, SpecialType,
            shared_ptr<ArrayType>, shared_ptr<ObjectType>> val;
    int64 size = -1; // size = -1 not array

    // { val } 對應型別數值建構子 留空為 Null 型別 用以直接轉換
    Variable() : val(monostate{}) { update_type(); }
    Variable(int64 v) : val(v) { update_type(); }
    Variable(double v) : val(v) { update_type(); }
    Variable(bool v) : val(v) { update_type(); }
    Variable(char v) : val(v) { update_type(); }
    Variable(const string &v) : val(v) { update_type(); }
    Variable(const char *v) : val(string(v)) { update_type(); }
    Variable(shared_ptr<ArrayType> v) : val(v) { update_type(); }
    Variable(shared_ptr<ObjectType> v) : val(v) { update_type(); } // TODO: 未實作的處理自訂資料型態

    // { DataType, size } 用以初始化變數和 Array
    Variable(DataType t, int64 size = -1, const string &v = "") : type(t), size(size) {
        if (size != -1) {
            val = make_shared<ArrayType>(size, Variable(t));
        } else {
            if (t == DataType::Int) val = stoll(v.empty() ? "0" : v);
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
        if (holds_alternative<int64>(val)) type = DataType::Int;
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
        if (t == DataType::Int) return Variable((int64)0);
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
                    return is_same_v<T, int64> || is_same_v<T, double> ||
                           is_same_v<T, char> || is_same_v<T, bool>;
                };
                return is_numeric(a) && is_numeric(b);
            }},
        var1.val, var2.val);
    }

    Variable bool_evaluate(const Variable &var1, const string &op, const Variable &var2) {
        auto extract_numeric_value = [](const Variable &v) -> double {
            return visit(overloaded{
                [](double d) -> double { return d; },
                [](int64 i) -> double { return static_cast<double>(i); },
                [](char c) -> double { return static_cast<double>(c); },
                [](bool b) -> double { return b ? 1.0 : 0.0; },
                [](const auto &) -> double { return 0.0; }
            }, v.val);
        };

        double n1 = extract_numeric_value(var1);
        double n2 = extract_numeric_value(var2);
        bool result = false;

        if (var1.type == DataType::Float || var2.type == DataType::Float) {
            if (op == "==") result = (abs(n1 - n2) <= ErrorValue);
            else if (op == "!=") result = (abs(n1 - n2) > ErrorValue);
            else if (op == "<") result = (n1 < n2 - ErrorValue);
            else if (op == ">") result = (n1 > n2 + ErrorValue);
            else if (op == "<=") result = (n1 <= n2 + ErrorValue);
            else if (op == ">=") result = (n1 >= n2 - ErrorValue);
            else return Variable();
        } else {
            if (op == "==") result = (n1 == n2);
            else if (op == "!=") result = (n1 != n2);
            else if (op == "<") result = (n1 < n2);
            else if (op == ">") result = (n1 > n2);
            else if (op == "<=") result = (n1 <= n2);
            else if (op == ">=") result = (n1 >= n2);
            else return Variable();
        }

        return Variable(result);
    }

public:
    explicit operator bool() const {
        return visit(overloaded{
            [](int64 i) -> bool { return i != 0; },
            [](double d) -> bool { return d != 0.0; },
            [](bool b) -> bool { return b; },
            [](char c) -> bool { return c != '\0'; },
            [](const string &s) -> bool { return !s.empty(); },
            [](const auto &) -> bool { return false; }
        }, this->val);
    }

    Variable operator+() {
        return visit(overloaded{
            [](int64 i) -> Variable { return Variable(i); },
            [](double d) -> Variable { return Variable(d); },
            [&](const auto &) -> Variable { return zeroed(this->type); }
        }, this->val);
    }

    Variable operator-() {
        return visit(overloaded{
            [](int64 i) -> Variable { return Variable(-i); },
            [](double d) -> Variable { return Variable(-d); },
            [&](const auto &) -> Variable { return zeroed(this->type); }
        }, this->val);
    }

    Variable operator!() {
        return Variable(!bool(*this));
    }

    Variable operator+(const Variable &var2) {
        // Coercion: if either side is string (and not an array), result is string concatenation
        if ((this->type == DataType::String || var2.type == DataType::String) &&
            !holds_alternative<shared_ptr<ArrayType>>(this->val) &&
            !holds_alternative<shared_ptr<ArrayType>>(var2.val)) {
            return Variable(var_to_string(*this) + var_to_string(var2));
        }

        return visit(overloaded{
            [](int64 a, int64 b) -> Variable { return Variable(a + b); },
            [](double a, double b) -> Variable { return Variable(a + b); },
            [](int64 a, double b) -> Variable { return Variable(a + b); },
            [](double a, int64 b) -> Variable { return Variable(a + b); },
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
            [&](const auto &, const auto &) -> Variable { return zeroed(promote(this->type, var2.type)); }
        }, this->val, var2.val);
    }

    Variable operator-(const Variable &var2) {
        return visit(overloaded{
            [](int64 a, int64 b) -> Variable { return Variable(a - b); },
            [](double a, double b) -> Variable { return Variable(a - b); },
            [](int64 a, double b) -> Variable { return Variable(a - b); },
            [](double a, int64 b) -> Variable { return Variable(a - b); },
            [&](const auto &, const auto &) -> Variable { return zeroed(promote(this->type, var2.type)); }
        }, this->val, var2.val);
    }

    Variable operator*(const Variable &var2) {
        return visit(overloaded{
            [](int64 a, int64 b) -> Variable { return Variable(a * b); },
            [](double a, double b) -> Variable { return Variable(a * b); },
            [](int64 a, double b) -> Variable { return Variable(a * b); },
            [](double a, int64 b) -> Variable { return Variable(a * b); },
            [&](const auto &, const auto &) -> Variable { return zeroed(promote(this->type, var2.type)); }
        }, this->val, var2.val);
    }

    Variable operator/(const Variable &var2) {
        return visit(overloaded{
            [](int64 a, int64 b) -> Variable {
                if (b == 0) return Variable((int64)0);
                return Variable(a / b);
            },
            [](double a, double b) -> Variable {
                if (b == 0.0) return Variable(0.0); // TODO: 暫時性的處理
                return Variable(a / b);
            },
            [](int64 a, double b) -> Variable {
                if (b == 0.0) return Variable(0.0);
                return Variable(static_cast<double>(a) / b);
            },
            [](double a, int64 b) -> Variable {
                if (b == 0) return Variable(0.0);
                return Variable(a / static_cast<double>(b));
            },
            [&](const auto &, const auto &) -> Variable { return zeroed(promote(this->type, var2.type)); }
        }, this->val, var2.val);
    }

    Variable operator%(const Variable &var2) {
        return visit(overloaded{
            [](int64 a, int64 b) -> Variable {
                if (b == 0) return Variable((int64)0);
                return Variable(a % b);
            },
            [&](const auto &, const auto &) -> Variable { return zeroed(promote(this->type, var2.type)); }
        }, this->val, var2.val);
    }

    Variable operator==(const Variable &var2) {
        if (!is_comparable(*this, var2)) return Variable(false);
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable { return Variable(a == b); },
            [&](const auto &, const auto &) -> Variable { return bool_evaluate(*this, "==", var2); }
        }, this->val, var2.val);
    }

    Variable operator!=(const Variable &var2) {
        if (!is_comparable(*this, var2)) return Variable(true);
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable { return Variable(a != b); },
            [&](const auto &, const auto &) -> Variable { return bool_evaluate(*this, "!=", var2); }
        }, this->val, var2.val);
    }

    Variable operator>=(const Variable &var2) {
        if (!is_comparable(*this, var2)) return Variable(false);
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable { return Variable(a >= b); },
            [&](const auto &, const auto &) -> Variable { return bool_evaluate(*this, ">=", var2); }
        }, this->val, var2.val);
    }

    Variable operator<=(const Variable &var2) {
        if (!is_comparable(*this, var2)) return Variable(false);
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable { return Variable(a <= b); },
            [&](const auto &, const auto &) -> Variable { return bool_evaluate(*this, "<=", var2); }
        }, this->val, var2.val);
    }

    Variable operator>(const Variable &var2) {
        if (!is_comparable(*this, var2)) return Variable(false);
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable { return Variable(a > b); },
            [&](const auto &, const auto &) -> Variable { return bool_evaluate(*this, ">", var2); }
        }, this->val, var2.val);
    }

    Variable operator<(const Variable &var2) {
        if (!is_comparable(*this, var2)) return Variable(false);
        return visit(overloaded{
            [](const string &a, const string &b) -> Variable { return Variable(a < b); },
            [&](const auto &, const auto &) -> Variable { return bool_evaluate(*this, "<", var2); }
        }, this->val, var2.val);
    }

    Variable operator<<(const Variable &var2) {
        return visit(overloaded{
            [](int64 a, int64 b) -> Variable {
                if (b < 0) return Variable((int64)0);
                if (b == 0) return Variable(a);
                return Variable(a << b);
            },
            [&](const auto &, const auto &) -> Variable {
                return zeroed(promote(this->type, var2.type));
            }
        }, this->val, var2.val);
    }

    Variable operator>>(const Variable &var2) {
        return visit(overloaded{
            [](int64 a, int64 b) -> Variable {
                if (b < 0) return Variable((int64)0);
                if (b == 0) return Variable(a);
                return Variable(a >> b);
            },
            [&](const auto &, const auto &) -> Variable { return zeroed(promote(this->type, var2.type)); }
        }, this->val, var2.val);
    }

    Variable operator&&(const Variable &var2) {
        return Variable(bool(*this) && bool(var2));
    }

    Variable operator||(const Variable &var2) {
        return Variable(bool(*this) || bool(var2));
    }

    Variable operator&(const Variable &var2) {
        auto extract_int = [](const Variable &v) -> int64 {
            return visit(overloaded{
                [](int64 i) { return i; },
                [](bool b) { return b ? 1LL : 0LL; },
                [](char c) { return static_cast<int64>(static_cast<unsigned char>(c)); },
                [](const auto &) { return 0LL; }
            }, v.val);
        };
        return Variable(extract_int(*this) & extract_int(var2));
    }

    Variable operator^(const Variable &var2) {
        auto extract_int = [](const Variable &v) -> int64 {
            return visit(overloaded{
                [](int64 i) { return i; },
                [](bool b) { return b ? 1LL : 0LL; },
                [](char c) { return static_cast<int64>(static_cast<unsigned char>(c)); },
                [](const auto &) { return 0LL; }
            }, v.val);
        };
        return Variable(extract_int(*this) ^ extract_int(var2));
    }

    Variable operator|(const Variable &var2) {
        auto extract_int = [](const Variable &v) -> int64 {
            return visit(overloaded{
                [](int64 i) { return i; },
                [](bool b) { return b ? 1LL : 0LL; },
                [](char c) { return static_cast<int64>(static_cast<unsigned char>(c)); },
                [](const auto &) { return 0LL; }
            }, v.val);
        };
        return Variable(extract_int(*this) | extract_int(var2));
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
    int64 size = -1;
    bool is_ref = false;
};

struct Function {
    DataType return_type;
    vector<FunctionParam> params;
    vector<Token> tokens; // 全部的 token
    bool has_void_param = false;
};

struct Environment {
    unordered_map<string, Variable> ident_table; // 存放「值傳遞」產生的副本。
    unordered_map<string, Variable*> ref_table; // 存放「引用傳遞」產生的指標。
    shared_ptr<Environment> parent;

    Environment(shared_ptr<Environment> p = nullptr) : parent(p) {}

    void global_init() {
        declare("cin", Variable{DataType::Special, -1, "cin"});
        declare("cout", Variable{DataType::Special, -1, "cout"});
    }

    // 由內而外尋找變數 
    // 回傳指標，這樣才能夠直接修改它的值
    Variable* get(const string &name) {
        if (ref_table.find(name) != ref_table.end()) {
            return ref_table[name]; // 在參照表中找到
        }
        if (ident_table.find(name) != ident_table.end()) {
            return &ident_table[name]; // 在當前層找到
        }
        if (parent != nullptr) {
            return parent->get(name); // 去外層找
        }
        return nullptr; // 一路找到 top 都沒有，代表未定義
    }

    bool declare(const string &name, const Variable &val) {
        if (ident_table.find(name) != ident_table.end() || ref_table.find(name) != ref_table.end()) {
            return false;
        }
        ident_table[name] = val;
        return true;
    }

    bool declare_ref(const string &name, Variable* ref) {
        if (ident_table.find(name) != ident_table.end() || ref_table.find(name) != ref_table.end()) {
            return false;
        }
        ref_table[name] = ref;
        return true;
    }

    // 尋找現有變數並更新
    bool set(const string &name, const Variable &val) {
        Variable* var = get(name);
        if (var != nullptr) {
            *var = val;
            return true;
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
    "[", "]",  "{",  "}",  "\"", "&",  "|",
    "^", // "."
};

const unordered_set<string> data_types = {
    "int", "float", "char", "bool", "string", "void"
};

const unordered_set<string> keywords = ([]{
    unordered_set<string> combined = {
        "true", "false",
        "if", "else",
        "do", "while",
        "return", 
        // "break","continue"
    };
    combined.insert(data_types.begin(), data_types.end());
    return combined;
}());

// ========================================Function Definition========================================
// const string& 傳引用(保護正本) const string 傳值(會複製一份副本且保護副本)
bool is_in(const string &str, const unordered_set<string> &targets) { return targets.find(str) != targets.end(); }
bool is_in(const string &str, const unordered_map<string, Function> &targets) { return targets.find(str) != targets.end(); }
bool is_in(const string &str, const unordered_map<string, Variable> &targets) { return targets.find(str) != targets.end(); }

Variable convert_to_var(const Token tk, int64 size = -1) {
    if (tk.type == TokenType::Constant) {
        if (tk.val == "true" || tk.val == "false") {
            return Variable{DataType::Bool, size, tk.val};
        } else if (tk.val.front() == '\'') {
            // Char constant: 'a'
            string content = "";
            if (tk.val.size() >= 3) content = tk.val.substr(1, tk.val.size() - 2);
            return Variable{DataType::Char, size, content};
        } else if (tk.val.front() == '"') {
            // String constant: "hello"
            string content = "";
            if (tk.val.size() >= 2) content = tk.val.substr(1, tk.val.size() - 2);
            return Variable{DataType::String, size, content};
        } else {
            // Number constant: 35, 35.67, .35, 35.
            if (tk.val.find('.') != string::npos) {
                return Variable{DataType::Float, size, tk.val};
            } else {
                return Variable{DataType::Int, size, tk.val};
            }
        }
    } else {
        throw runtime_error("Error in convert_to_var(): " + tk.val + " is not a constant");
    }
}

string var_to_string(const Variable &var) {
    return visit(overloaded{
        [](int64 i) { return to_string(i); },
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

// 在測資12之後可能要處裡錯誤
Variable coerce_variable(const Variable &var, DataType target_type) {
    if (var.type == target_type) return var;
    if (target_type == DataType::Int) {
        return visit(overloaded{
            [](int64 i) { return Variable(i); },
            [](double d) { return Variable(static_cast<int64>(d)); },
            [](bool b) { return Variable(b ? 1LL : 0LL); },
            [](char c) { return Variable(static_cast<int64>(c)); },
            [](const auto&) { return Variable((int64)0); }
        }, var.val);
    } else if (target_type == DataType::Float) {
        return visit(overloaded{
            [](int64 i) { return Variable(static_cast<double>(i)); },
            [](double d) { return Variable(d); },
            [](bool b) { return Variable(b ? 1.0 : 0.0); },
            [](char c) { return Variable(static_cast<double>(c)); },
            [](const auto&) { return Variable(0.0); }
        }, var.val);
    } else if (target_type == DataType::Bool) {
        return Variable(bool(var));
    } else if (target_type == DataType::String) {
        return Variable(var_to_string(var));
    } else if (target_type == DataType::Char) {
        return visit(overloaded{
            [](int64 i) { return Variable(static_cast<char>(i)); },
            [](double d) { return Variable(static_cast<char>(d)); },
            [](char c) { return Variable(c); },
            [](const auto&) { return Variable('\0'); }
        }, var.val);
    }
    return var;
}

unordered_map<string, Variable> format_params(const vector<FunctionParam> &params, const vector<Variable> &args) {
    unordered_map<string, Variable> formatted_params;
    static const unordered_map<DataType, vector<DataType>> valid_conversions = {
        {DataType::Int, {DataType::Int, DataType::Float}},
        {DataType::Float, {DataType::Int, DataType::Float}},
    };
    if (DEBUG && params.size() != args.size()) {
        throw runtime_error("Line 0 : Invalid function call: expected " + to_string(params.size()) + " arguments, got " + to_string(args.size()));
    }
    for (int i = 0; i < (int)params.size(); i++) {
        Variable final_arg;
        if (i < (int)args.size()) {
            final_arg = args[i];
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
                        final_arg = Variable(static_cast<int64>(get<double>(args[i].val)));
                    } else if (params[i].type == DataType::Float) {
                        final_arg = Variable(static_cast<double>(get<int64>(args[i].val)));
                    }
                } else if (DEBUG) {
                    throw runtime_error("Line 0 : Invalid function call: expected " + 
                                        enum_to_DataType(params[i].type) + " arguments, got " + enum_to_DataType(args[i].type));
                }
            }
        } else {
            final_arg = Variable::zeroed(params[i].type);
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
    int cur_line = 1;
    int last_skipped_newline_count = 0;

    Token cur_token;

    // 只要一個statement結束就是一個新的statement開始 包含空白 換行 註解等
    Token get_a_token(int skip_tokens = 1) {
        if (skip_tokens < 1) skip_tokens = 1;
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
                    while (idx < text.length() && text[idx] != '\n') idx++;
                    if (idx < text.length() && text[idx] == '\n') {
                        cur_line++;
                        skipped_newlines++;
                        idx++;
                    }
                } else break;
            }

            if (idx >= text.length()) {
                tk.type = TokenType::EndOfFile;
                tk.val = "";
                tk.line = cur_line;
                last_skipped_newline_count = skipped_newlines;
                return tk;
            }
            tk.start_idx = idx;

            if (text[idx] == '\'') {
                if (idx + 2 < text.length() && text[idx + 2] == '\'' && text[idx + 1] != '\n') {
                    tk.type = TokenType::Constant;
                    tk.val = text.substr(idx, 3);
                    idx += 3;
                } else {
                    tk.type = TokenType::Undefined;
                    tk.val = string(1, text[idx]);
                    idx++;
                }
            } else if (text[idx] == '"') {
                string parsed_string = "\"";
                idx++;
                while (idx < text.length() && text[idx] != '"') {
                    if (text[idx] == '\\' && idx + 1 < text.length()) {
                        idx++; 
                        if (text[idx] == 'n') parsed_string += '\n';
                        else if (text[idx] == 't') parsed_string += '\t'; 
                        else if (text[idx] == '"') parsed_string += '"';  
                        else parsed_string += text[idx]; 
                    } else {
                        parsed_string += text[idx];
                    }
                    idx++;
                }
                if (text[idx] == '"') parsed_string += '"';
                if (idx < text.length()) idx++; // consume "
                tk.type = TokenType::Constant;
                tk.val = parsed_string;
            } else if (isdigit(text[idx]) || (text[idx] == '.' && idx + 1 < text.length() && isdigit(text[idx+1]))) {
                size_t start = idx;
                bool has_dot = false;
                if (text[idx] == '.') {
                    has_dot = true;
                    idx++;
                }
                while (idx < text.length() && isdigit(text[idx])) idx++;
                if (!has_dot && idx < text.length() && text[idx] == '.') {
                    idx++;
                    while (idx < text.length() && isdigit(text[idx])) idx++;
                }
                tk.type = TokenType::Constant;
                tk.val = text.substr(start, idx - start);
            } else if (isalpha(text[idx]) || text[idx] == '_') {
                size_t start = idx;
                while (idx < text.length() && (isalnum(text[idx]) || text[idx] == '_')) idx++;
                string s = text.substr(start, idx - start);
                if (s == "true" || s == "false") {
                    tk.type = TokenType::Constant;
                    tk.val = s;
                } else {
                    tk.type = TokenType::Identifier;
                    tk.val = s;
                }
            } else {
                bool found_two = false;
                if (idx + 1 < text.length()) {
                    string s2 = text.substr(idx, 2);
                    static const unordered_set<string> s2_syms = {
                        "+=", "-=", "*=", "/=", "%=", "==", ">=", "<=", "!=", "&&", "||", "++", "--", "<<", ">>"
                    };
                    if (s2_syms.count(s2)) {
                        tk.type = TokenType::Symbol;
                        tk.val = s2;
                        idx += 2;
                        found_two = true;
                    }
                }
                if (!found_two) {
                    string s1 = string(1, text[idx]);
                    if (symbols.count(s1)) {
                        tk.type = TokenType::Symbol;
                        tk.val = s1;
                    } else {
                        tk.type = TokenType::Undefined;
                        tk.val = s1;
                    }
                    idx++;
                }
            }
        }
        tk.line = cur_line;
        last_skipped_newline_count = skipped_newlines;

        if (is_in(tk.val, {"do"})) assert(false);
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
        int start_last_skipped_newline_count = last_skipped_newline_count;
        size_t start_token_ptr = token_ptr;

        Token tk = get_a_token(skip_tokens);

        idx = start_idx;
        cur_line = start_line;
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
                        (tk.type == TokenType::Identifier || prev_type == TokenType::Identifier)) {
                        // 排除 if, while, for 等關鍵字的例外，例如 if (x > 0)
                        if (tk.val == "(") {
                            if (prev_val != "if" && prev_val != "while" && prev_val != "for") {
                                need_space = false;
                            }
                        } else {
                            need_space = false;
                        }
                    } else if (tk.type == TokenType::Identifier && (prev_val == "++" || prev_val == "--") ||
                               prev_val == "(" && tk.val == ")") {
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

    size_t get_token_ptr() const { return token_ptr; }

    void push_checkpoint(long long override_idx = -1, int override_line = -1, long long override_token_ptr = -1) {
        size_t save_idx = (override_idx == -1) ? idx : (size_t)override_idx;
        int save_line = (override_line == -1) ? cur_line : override_line;
        size_t save_token_ptr = (override_token_ptr == -1) ? token_ptr : (size_t)override_token_ptr;
        checkpoints.push_back({save_idx, save_line, last_skipped_newline_count, save_token_ptr});
    }

    void pop_checkpoint() { 
        checkpoints.pop_back(); 
    }

    void back_to_checkpoint() {
        const Checkpoint &checkpoint = checkpoints.back();
        idx = checkpoint.idx;
        cur_line = checkpoint.cur_line;
        last_skipped_newline_count = checkpoint.last_skipped_newline_count;
        token_ptr = checkpoint.token_ptr;
    }
};

class Parser {
private:
    Lexer lexer;
    Token cur_token;
    bool require_semicolon = true;

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
        if (cur_token.type == TokenType::Undefined && !is_in(string("") + cur_token.val[0], symbols)) {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unrecognized token with first char '" + cur_token.val[0] + "'");
        } else {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unexpected token '" + cur_token.val + "'");
        }
    }

    void throw_undefined_id_error(Token id_token, int debug_No = 0) {
        if (DEBUG) cout << "Debug id mode: No. " << debug_No << endl;
        throw runtime_error("Line " + to_string(id_token.line) + " : undefined identifier '" + id_token.val + "'");
    }

    void next() {
        // 第一階段報錯
        if (cur_token.type == TokenType::Undefined) {
            throw runtime_error("Line " + to_string(cur_token.line) + " : unrecognized token with first char '" + cur_token.val[0] + "'");
        }
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
                int64 idx = 0;
                if (auto i = get_if<int64>(&index_var.val)) {
                    idx = *i;
                } else if (auto d = get_if<double>(&index_var.val)) { // 可能需要處理錯誤
                    idx = static_cast<int64>(*d);
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

    // parse_ident_rvalue 傳入 lval_ptr 用於獲取變數位址，以便後續處理 ++/-- 等 Side Effect
    Variable parse_ident_rvalue(Variable** lval_ptr = nullptr) {
        // 
        Token id_token = cur_token;
        if (lexer.peek_token().val == "(") {
            return parse_function_call();
        } else {
            Variable* target_var = cur_env->get(id_token.val);
            if (target_var == nullptr) {
                if (cur_token.type == TokenType::Identifier) throw_undefined_id_error(id_token, 2);
                else throw_error(2);
            }
            next(); // 消耗 ident
            Variable result = *target_var;
            if (lval_ptr) *lval_ptr = target_var; // 紀錄變數在環境中的位址

            if (cur_token.val == "[") {
                next();
                Variable index_var = parse_expression();
                if (cur_token.val != "]") throw_error(3);
                next();
                
                if (auto arr_ptr = get_if<shared_ptr<ArrayType>>(&target_var->val)) {
                    int64 idx = 0;
                    if (auto i = get_if<int64>(&index_var.val)) {
                        idx = *i;
                    } else if (auto d = get_if<double>(&index_var.val)) {
                        idx = static_cast<int64>(*d);
                    } else {
                        // throw runtime_error("Line " + to_string(cur_token.line) + " : array index must be an integer");
                    }
                    
                    if (idx >= 0 && idx < (*arr_ptr)->size()) {
                        result = (**arr_ptr)[idx];
                        if (lval_ptr) *lval_ptr = &((**arr_ptr)[idx]); // 紀錄陣列元素的具體位址，而非整個陣列
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
            
            Variable* target_var = parse_ident_lvalue(); 
            if (!dry_run && target_var) {
                // 套用副作用前進行類型檢查與轉型 (coerce)，確保 ++/-- 後不改變變數原始類型
                if (op == "++") *target_var = coerce_variable(*target_var + Variable{(int64)1}, target_var->type);
                else *target_var = coerce_variable(*target_var - Variable{(int64)1}, target_var->type);
            }
            
            return target_var ? *target_var : Variable();
        }

        // num 1, 1., .1, 1.0
        if (cur_token.type == TokenType::Constant) {
            result = convert_to_var(cur_token, -1);
            next();
        // ident or function call
        } else if (cur_token.type == TokenType::Identifier) {
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
                Variable* lval_ptr = nullptr;
                // 獲取位址以便處理後置 ++/--
                result = parse_ident_rvalue(&lval_ptr);
                if (cur_token.val == "++" || cur_token.val == "--") {
                    if (is_signed) throw_error(38);
                    string op = cur_token.val;
                    next(); // 消耗 ++/--
                    
                    if (!dry_run && lval_ptr) {
                        // 後置運算：先回傳舊值 (result)，但在背景更新儲存空間 (*lval_ptr)
                        if (op == "++") *lval_ptr = coerce_variable(*lval_ptr + Variable{(int64)1}, lval_ptr->type);
                        else *lval_ptr = coerce_variable(*lval_ptr - Variable{(int64)1}, lval_ptr->type);
                    }
                }
            }
        } else {
            throw_error(8);
        }
        return result;
    }

    Variable parse_multiplicative_exp() {
        // * / %
        Variable result = parse_unary_exp();
        while (is_in(cur_token.val, unordered_set<string>{"*", "/", "%"})) {
            string op = cur_token.val;
            next();
            if (op == "*") result = result * parse_unary_exp();
            else if (op == "/") result = result / parse_unary_exp();
            else if (op == "%") result = result % parse_unary_exp();
        }
        return result;
    }

    Variable parse_additive_exp() {
        // + -
        Variable result = parse_multiplicative_exp();
        while (is_in(cur_token.val, unordered_set<string>{"+", "-"})) {
            string op = cur_token.val;
            next();
            if (op == "+") result = result + parse_multiplicative_exp();
            else if (op == "-") result = result - parse_multiplicative_exp();
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
                    Variable out = parse_additive_exp();
                    // cout << "test | out.type: " << enum_to_DataType(out.type) << endl;
                    if (!dry_run) cout << var_to_string(out);
                    result = out;
                }
            } else if (sp && sp->val == "cin" && cur_token.val == ">>") {
                while (cur_token.val == ">>") {
                    next();
                    result = parse_additive_exp();
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
        while (is_in(cur_token.val, unordered_set<string>{"<", "<=", ">", ">="})) {
            string op = cur_token.val;
            next();
            if (op == "<") result = result < parse_shift_exp();
            else if (op == "<=") result = result <= parse_shift_exp();
            else if (op == ">") result = result > parse_shift_exp();
            else if (op == ">=") result = result >= parse_shift_exp();
        }
        return result;
    }
  
    Variable parse_equality_exp() {
        // == !=
        Variable result = parse_relational_exp();
        while (is_in(cur_token.val, unordered_set<string>{"==", "!="})) {
            string op = cur_token.val;
            next();
            if (op == "==") result = result == parse_relational_exp();
            else if (op == "!=") result = result != parse_relational_exp();
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
                result = result & parse_equality_exp();
            } else if (cur_token.val == "|") {
                next();
                result = result | parse_equality_exp();
            } else if (cur_token.val == "^") {
                next();
                result = result ^ parse_equality_exp();
            }
        }
        return result;
    }
  
    Variable parse_logical_and_exp() {
        // &&
        Variable result = parse_bitwise_exp();
        while (cur_token.val == "&&") {
            next();
            Variable rhs = parse_bitwise_exp();
            result = result && rhs;
        }
        return result;
    }
  
    Variable parse_logical_or_exp() {
        // ||
        Variable result = parse_logical_and_exp();
        while (cur_token.val == "||") {
            next();
            Variable rhs = parse_logical_and_exp();
            result = result || rhs;
        }
        return result;
    }
    
    Variable parse_conditional_exp() { 
        // ? :
        Variable result = parse_logical_or_exp();
        if (cur_token.val == "?") {
            // 先備份進入前的 dry_run 狀態
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
                    set_dry_run(prev_dry_run);
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
        if (cur_token.type == TokenType::Identifier) {
            Token next_token = lexer.peek_token(1);
            if (is_in(next_token.val, {"=", "+=", "-=", "*=", "/=", "%="})) {
                is_assign = true;
            } else if (next_token.val == "[") {
                int i = 2;
                int b_count = 1;
                while (b_count > 0) {
                    Token t = lexer.peek_token(i++); // TODO: 需要修改
                    if (t.type == TokenType::EndOfFile || t.val == ";") break;
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
                if (op == "=") *lval = coerce_variable(exp_val, lval->type);
                else if (op == "+=") *lval = coerce_variable(*lval + exp_val, lval->type);
                else if (op == "-=") *lval = coerce_variable(*lval - exp_val, lval->type);
                else if (op == "*=") *lval = coerce_variable(*lval * exp_val, lval->type);
                else if (op == "/=") *lval = coerce_variable(*lval / exp_val, lval->type);
                else if (op == "%=") *lval = coerce_variable(*lval % exp_val, lval->type);
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

    bool parse_condition(bool force_dry_run = false) {
        // cur_token is '(' end after ')'
        // ( Expression )
        bool prev_dry_run = get_dry_run();
        if (force_dry_run) set_dry_run(true);

        try {
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
            set_dry_run(prev_dry_run);
            return bool(result);
        } catch (...) {
            set_dry_run(prev_dry_run);
            throw; // 將狀態復原後，再把例外向上拋出
        }
    }

    void skip_token() {
        cur_token = lexer.get_next_token();
    }

    void skip_statement() {
        bool old_dry_run = get_dry_run();
        set_dry_run(true);

        auto old_env = cur_env;
        cur_env = make_shared<Environment>(old_env);

        try {
            parse_statement(false); 
        } catch (...) {
            cur_env = old_env;
            set_dry_run(old_dry_run);
            throw; // 此時主 Lexer 的指標精準停在引發錯誤的那顆 Token 上！
        }

        cur_env = old_env;
        set_dry_run(old_dry_run);
    }

    void parse_scoped_statement(bool reset_after_statement = true) {
        // 預讀當前的 Token，判斷接下來是要解析 Block 還是單行 Statement
        if (cur_token.val == "{") {
            // 情況 A：這是一個 Block '{ ... }'
            // 交給 parse_statement，其內部的 parse_block() 會自行處理 Environment 切換
            parse_statement(reset_after_statement);
        } else {
            // 情況 B：這是一行單獨的 Statement (例如 int x = 1; 或 x++;)
            // 手動加上「隱形的作用域」來防止變數污染
            auto old_env = cur_env;
            cur_env = make_shared<Environment>(old_env);
            
            try {
                parse_statement(reset_after_statement);
            } catch (...) {
                cur_env = old_env;
                throw;
            }
            
            cur_env = old_env;
        }
    }

    void parse_if_else() {
        // start at "if", end after "}"
        if (!dry_run) {
            lexer.push_checkpoint(cur_token.start_idx, cur_token.line, lexer.get_token_ptr() > 0 ? lexer.get_token_ptr() - 1 : 0);
            skip_statement();
            lexer.back_to_checkpoint();
            lexer.pop_checkpoint();
            cur_token = lexer.get_next_token();
        }

        bool condition_met = false;
        next();
        bool condition = parse_condition();
        if (condition) {
            parse_scoped_statement(false);
            condition_met = true;
        } else {
            skip_statement();
        }

        while (cur_token.val == "else") {
            next();
            if (cur_token.val == "if") {
                next();
                condition = parse_condition(condition_met);

                if (!condition_met && condition) {
                    parse_scoped_statement(false);
                    condition_met = true;
                } else {
                    skip_statement();
                }
            } else {
                if (!condition_met) {
                    parse_scoped_statement(false);
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
        if (!dry_run) {
            lexer.push_checkpoint(cur_token.start_idx, cur_token.line, lexer.get_token_ptr() > 0 ? lexer.get_token_ptr() - 1 : 0);
            skip_statement();
            lexer.back_to_checkpoint();
            lexer.pop_checkpoint();
            cur_token = lexer.get_next_token();
        }

        bool condition;
        if (cur_token.val == "while") {
            lexer.push_checkpoint(); 
            next();
            condition = parse_condition();
            
            if (dry_run) {
                skip_statement();
                lexer.pop_checkpoint();
                return;
            }

            if (!condition) skip_statement();
            
            try {
                while (condition) {
                    parse_scoped_statement(false);
                    lexer.back_to_checkpoint();
                    next();
                    condition = parse_condition();
                    if (!condition) {
                        skip_statement();
                    }
                }
            } catch (...) {
                // 攔截到 ReturnException，先清理 Checkpoint，再將例外向上拋出
                lexer.pop_checkpoint();
                throw;
            }
            
            lexer.pop_checkpoint(); // 迴圈正常結束，清理 Checkpoint
        } else {
            throw_error(17);
        }
    }

    void parse_do_while() {
        // start at "do", end after ";"
        if (!dry_run) {
            lexer.push_checkpoint(cur_token.start_idx, cur_token.line, lexer.get_token_ptr() > 0 ? lexer.get_token_ptr() - 1 : 0);
            skip_statement();
            lexer.back_to_checkpoint();
            lexer.pop_checkpoint();
            cur_token = lexer.get_next_token();
        }

        bool condition;
        if (cur_token.val == "do") {
            lexer.push_checkpoint();
            next();

            if (dry_run) {
                bool old_is_global = is_global;
                is_global = false;
                skip_statement();
                is_global = old_is_global;
                
                if (cur_token.val != "while") {
                    throw_error(18);
                }
                next();
                condition = parse_condition();
                
                if (cur_token.val != ";") throw_error(19);
                next(); 
                
                lexer.pop_checkpoint();
                return;
            }

            // 加入 try-catch 保護層
            try {
                do {
                    bool old_is_global = is_global;
                    is_global = false;
                    parse_scoped_statement(false);
                    is_global = old_is_global;
                    
                    if (cur_token.val != "while") {
                        throw_error(18);
                    }
                    next();
                    condition = parse_condition();
                    
                    if (cur_token.val != ";") throw_error(19);
                    
                    if (condition) {
                        lexer.back_to_checkpoint();
                        next();
                    } else {
                        next(); // Skip the ;
                        break;
                    }
                } while (true);
            } catch (...) {
                // 攔截到 ReturnException，清理狀態再丟出
                lexer.pop_checkpoint();
                throw;
            }
            
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
    // 執行函數 Block
    void parse_function_block(const vector<Token> &tokens, const vector<FunctionParam> &f_params, 
                              const vector<Variable> &arg_values, const vector<Variable*> &arg_ptrs) {
        // start at "{", end at "}"
        // 函數的 env parent 指向 global_env（靜態作用域語義），而非呼叫方的局部 env
        auto saved_env = cur_env;
        auto new_env = make_shared<Environment>(global_env);
        cur_env = new_env;
        // 初始化 : 建立函數參數環境 
        // TODO: 如果傳入時需要轉型會如何處理? (例如傳入float到int &參數) 現有規範下內部參數不論型態為何都會單純與外部同步
        for (int i = 0; i < (int)f_params.size(); i++) {
            if (f_params[i].is_ref) { 
                // 傳參照：如果引數是 lvalue，則直接在環境中指向該位址
                if (i < (int)arg_ptrs.size() && arg_ptrs[i] != nullptr) {
                    // 若成功, arg_ptrs[i]指向該變數的位址
                    cur_env->declare_ref(f_params[i].name, arg_ptrs[i]);
                } else {
                    // 拋出語義錯誤：無法將非左值綁定到引用參數
                    throw runtime_error("Semantic Error: non-const reference to type " 
                        + enum_to_DataType(f_params[i].type) + " cannot bind to an rvalue.");
                }
            } else {
                // 傳值：建立新的變數副本
                Variable val = (i < (int)arg_values.size()) ? arg_values[i] : Variable::zeroed(f_params[i].type);
                // 處理轉型 (例如傳入 float 到 int 參數)
                if (i < (int)arg_values.size() && f_params[i].type != arg_values[i].type && arg_values[i].size == -1) {
                    val = coerce_variable(arg_values[i], f_params[i].type);
                }
                cur_env->declare(f_params[i].name, val);
            }
        }
        // 開始執行函數
        Parser block_parser(tokens);
        block_parser.current_return_type = this->current_return_type;

        try {
            if (block_parser.cur_token.val == "{") {
                block_parser.set_global(false); // 禁用GlobalDefinition
                block_parser.next();
                while (block_parser.cur_token.val != "}") {
                    block_parser.parse_statement(false);
                }
            }
        } catch (ReturnException &re) {
            cur_env = saved_env;
            throw; // 重新拋出 ReturnException
        } catch (...) {
            cur_env = saved_env;
            throw;
        }
        cur_env = saved_env;
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
                int64 size = -1;
                if (auto i = get_if<int64>(&size_var.val)) {
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

    vector<FunctionParam> parse_function_declaration_params(bool &has_void) {
        has_void = false;
        // start at "(", end after ")"
        // <Params> ::= ( <Type> <Ident> { , <Type> <Ident> } | <Empty> | <VOID> )
        vector<FunctionParam> params;
        auto parse_a_param = [&]() -> void {
            DataType type = DataType_to_enum(cur_token.val);
            next(); // move to ident
            int64 size = -1;
            bool is_ref = false;
            if (cur_token.val == "&") {
                is_ref = true;
                next();
            }
            if (cur_token.type == TokenType::Identifier && keywords.find(cur_token.val) == keywords.end()) {
                Token id_token = cur_token;
                next();
                if (cur_token.val == "[") {
                    next(); // move to size
                    Variable size_var = parse_expression();
                    if (auto i = get_if<int64>(&size_var.val)) {
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
            next(); // move past "("
            if (cur_token.val == ")") return params; // () case

            if (cur_token.val == "void") {
                if (lexer.peek_token().val == ")") {
                    next(); // move to )
                    has_void = true;
                    return params; // (void) case
                } else {
                    throw_error(87);
                }
            }

            // Normal parameters
            while (true) {
                parse_a_param();
                if (cur_token.val == ",") {
                    next();
                } else if (cur_token.val == ")") {
                    break;
                } else {
                    throw_error(28);
                }
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

        bool has_void = false;
        vector<FunctionParam> params = parse_function_declaration_params(has_void);
        
        // 1. 在輸入函數定義時先將函數名稱跟參數表填入function table中 (允許遞迴呼叫)
        func_table[name] = Function{type, params, {}, has_void};

        // 宣告時先行驗證語法 (dry run)，同時檢查未定義變數
        bool old_dry_run = dry_run;
        dry_run = true;
        DataType old_return_type = current_return_type;
        current_return_type = type;
        
        // 建立一個臨時的作用域（parent 指向 global_env），把函數參數放進去，避免 dry_run 報出 "未定義變數"
        auto temp_env = make_shared<Environment>(global_env);
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
                // 文法正確 捕捉return避免錯誤
            }
            
            is_global = old_is_global;
            dry_run = old_dry_run;
            current_return_type = old_return_type;
            cur_env = parent_env;
            
            // 將 lexer 退回大括號的起點
            lexer.back_to_checkpoint();
            lexer.pop_checkpoint();
            
            vector<Token> tokens = lexer.get_a_block();
            cur_token = lexer.get_next_token(); // 同步下一顆 token
            
            // 2. 在成功解析完畢後再將內容填入
            func_table[name].tokens = tokens;
            return {name + "()", state}; 

        } catch (...) {
            // 解析失敗，還原狀態
            is_global = old_is_global;
            dry_run = old_dry_run;
            current_return_type = old_return_type;
            cur_env = parent_env;
            lexer.pop_checkpoint(); 
            
            // 3. 若解析宣告內容過程發生錯誤則再從 table 中移除函數
            func_table.erase(name);
            throw; 
        }
    }

    // 同時回傳每個 arg 對應的外部變數位址（lvalue ptr），用於 Pass-by-Reference
    pair<vector<Variable>, vector<Variable*>> parse_function_params_with_ptrs() {
        // start at '(', end after ')'
        vector<Variable> params;
        vector<Variable*> arg_ptrs; // 扮演「傳送門」的角色，在解析階段記住來源位址，在初始化階段將該位址「借給」函數內的別名。
        if (cur_token.val == "(") {
            next();
            if (cur_token.val == ")") {
                next();
                return {params, arg_ptrs};
            } else {
                auto parse_one = [&]() {
                    Variable* lval_ptr = nullptr;
                    // 此 if 為嘗試偵測當前的輸入參數是否為 lvalue (Identifier 或 Array Element) 且不檢查型別
                    // 若可為左值 則回傳實際指標 其餘的輸入參數 (如 123, a + 1) 則為 nullptr
                    if (cur_token.type == TokenType::Identifier) {
                        Token id_token = cur_token;
                        Token next_token = lexer.peek_token(1);
                        if (next_token.val == "," || next_token.val == ")") { // 簡單變數
                            lval_ptr = cur_env->get(id_token.val); 
                        } else if (next_token.val == "[") { // 陣列元素：使用 checkpoint 嘗試解析左值
                            bool prev_dry_run = get_dry_run();
                            set_dry_run(true);
                            lexer.push_checkpoint();
                            try {
                                lval_ptr = parse_ident_lvalue();
                                if (cur_token.val != "," && cur_token.val != ")") {
                                    lval_ptr = nullptr;
                                }
                            } catch (...) {
                                lval_ptr = nullptr;
                            }
                            set_dry_run(prev_dry_run);
                            lexer.back_to_checkpoint();
                            lexer.pop_checkpoint();
                            cur_token = id_token;
                        }
                    }
                    // 接著才解析右值並一併將先前的lval_ptr傳入
                    params.push_back(parse_basic_exp());
                    arg_ptrs.push_back(lval_ptr);
                };
                parse_one();
                while (cur_token.val == ",") {
                    next();
                    parse_one();
                }
                if (cur_token.val != ")") {
                    throw_error(30);
                }
                next();
            }
        } else {
            throw_error(31);
        }
        return {params, arg_ptrs};
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
                params.push_back(parse_basic_exp());
                while (cur_token.val == ",") {
                    next();
                    params.push_back(parse_basic_exp());
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
        // FunctionCall ::= Identifier '(' [ Parameters ] ')'
        Token function_token = cur_token;
        string function_name = cur_token.val;
        next();
        auto [params, arg_ptrs] = parse_function_params_with_ptrs();
        
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
            const vector<Variable> &arg_values = params; // Re-use already parsed params
            
            DataType old_return_type = current_return_type;
            current_return_type = func_table[function_name].return_type;
            if (!dry_run) {
                try {
                    parse_function_block(func_table[function_name].tokens, func_table[function_name].params, arg_values, arg_ptrs);
                } catch (ReturnException &re) {
                    current_return_type = old_return_type;
                    return re.value;
                }
            }
            current_return_type = old_return_type;
            // 函數輸出可以不用依照宣告型別
            if (func_table[function_name].return_type == DataType::Void) return Variable();
            return Variable(func_table[function_name].return_type);
        } else {
            throw_undefined_id_error(function_token, 2);
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
        if (type_token.type == TokenType::Identifier && type_map.find(type_token.val) != type_map.end()) {    
            if (!is_global && type_token.val == "void") throw_error(41); // DataType only, VOID only global function
            next(); // 現在在ident
            if (cur_token.type != TokenType::Identifier) throw_error(33);
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
        } else if (cur_token.val == ";") {
            return_states.push({"", State::Statement});
            require_semicolon = true; // 由底下的掃描邏輯來消耗這個 ';'
        // 條件 8: Expression ';'
        } else {
            parse_expression();
            return_states.push({"", State::Statement});
            require_semicolon = true;
        }

        // 後處理：分號檢測與狀態更新 TODO: 找出這裡的問題
        if (require_semicolon && cur_token.val != ";") {
            throw_error(34);
        } else if (require_semicolon && cur_token.val == ";") {
            if (reset_after_statement) {
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
        cout << enum_to_DataType(f.return_type) << " " << name << "(";
        if (f.params.empty()) {
            if (f.has_void_param) cout << " void ";
        } else {
            cout << " ";
            for (int i = 0; i < (int)f.params.size(); i++) {
                cout << enum_to_DataType(f.params[i].type) << " ";
                if (f.params[i].is_ref) cout << "& ";
                cout << f.params[i].name;
                if (f.params[i].size != -1) cout << "[ " << f.params[i].size << " ]";
                if (i < (int)f.params.size() - 1) {
                    cout << ", ";
                }
            }
            cout << " ";
        }
        Lexer lexer("");
        cout << ") " << lexer.pretty_print_block(f.tokens) << endl;
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

	string content, _; // 跳過測試
    cin >> _; // 忽略標題
    cin.ignore();
    char c;
    while (cin.get(c)) {
        content += c;
    }
    
    cout << "Our-C running ..." << endl;
    Parser parser(content);
    parse_wrapper(parser);
    return 0;
}