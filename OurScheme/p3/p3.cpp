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
#include <functional>

using namespace std;
using int64 = long long;

template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// ========================================Forward Declarations and Types========================================

enum TokenType {
    LEFT_PAREN, RIGHT_PAREN, INT, STRING, DOT, FLOAT, NIL, T, QUOTE, SYMBOL, CONS, EndOfFile, Undefined,
};

struct Token;
struct Node;
struct Environment;
class Lexer;
class Parser;

struct Function {
    string name;
    size_t expected_count;
    bool is_ge;
    function<shared_ptr<Node>(const vector<shared_ptr<Node>> &)> func;

    Function(const string &n, function<shared_ptr<Node>(const vector<shared_ptr<Node>> &)> f) : name(n), func(f) {}
};

using PossibleTypes = variant<monostate, int64, double, bool, char, string, Function>;

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
    // 當前token的內容指向佐子樹，剩餘內容指向柚子樹(右子樹)
    TokenType type;
    PossibleTypes val;
    shared_ptr<Node> car;
    shared_ptr<Node> cdr;

    // Default constructor
    Node() : type(TokenType::Undefined), val(monostate{}), car(nullptr), cdr(nullptr) {}
    // Atom constructor
    Node(TokenType t, PossibleTypes v) : type(t), val(v), car(nullptr), cdr(nullptr) {}
    // Cons constructor
    Node(shared_ptr<Node> l, shared_ptr<Node> r) : type(TokenType::CONS), val(monostate{}), car(l), cdr(r) {}
};

struct Environment {
    unordered_map<string, shared_ptr<Node>> bindings;
    shared_ptr<Environment> parent;

    Environment(shared_ptr<Environment> p = nullptr) : parent(p) {}
};

// ========================================Function Declarations========================================

bool is_in(const string &op, const unordered_set<string> &targets);
void check_params_count_and_type(const string &func_name, const vector<shared_ptr<Node>> &args, size_t expected_count, 
                                 const unordered_set<TokenType> &expected_types, bool is_ge = false);
bool is_pure_list(const shared_ptr<Node> &node);
int get_list_length(const shared_ptr<Node> &node);
string get_s_exp_string(const shared_ptr<Node> &node, int indent, bool first_on_line);
string pretty_print(const shared_ptr<Node> &node);
bool is_system_primitive(const string &name);
int64 get_int_val(const shared_ptr<Node> &n);
double get_float_val(const shared_ptr<Node> &n);
bool is_equal(const shared_ptr<Node> &n1, const shared_ptr<Node> &n2);
shared_ptr<Node> do_arithmetic(const string &op, const vector<shared_ptr<Node>> &args);
shared_ptr<Node> is_specific_type(const shared_ptr<Node> &node, unordered_set<TokenType> type);
shared_ptr<Node> is_not_specific_type(const shared_ptr<Node> &node, unordered_set<TokenType> type);
shared_ptr<Node> do_comparison(const string &op, const vector<shared_ptr<Node>> &args);
shared_ptr<Node> do_string_comparison(const string &op, const vector<shared_ptr<Node>> &args);
bool get_expected_params_info(const string &func_name, size_t &expected_count, bool &is_ge);
shared_ptr<Node> eval(const shared_ptr<Node> &node, const shared_ptr<Environment> &env, bool is_top_level = false);
void print_s_exp(const shared_ptr<Node> &root);
void parse_wrapper(Parser &parser);

// ========================================Global Variables and Maps========================================

bool DEBUG = false;

shared_ptr<Environment> global_env;
shared_ptr<Environment> cur_env = global_env;

const unordered_set<string> keywords = {
    "nil", "#f", "t", "#t", "quote", "define", "set!", "let", "cond", "lambda", "if", "and", "or"
};

const unordered_map<string, pair<size_t, bool>> built_in_functions_info = {
    {"+", {2, true}}, {"-", {2, true}}, {"*", {2, true}}, {"/", {2, true}},
    {"cons", {2, false}}, {"car", {1, false}}, {"cdr", {1, false}},
    {"list", {0, true}}, // list 接受 0 個以上的參數
    {"atom?", {1, false}}, {"pair?", {1, false}}, {"list?", {1, false}},
    {"null?", {1, false}}, {"integer?", {1, false}}, {"real?", {1, false}},
    {"number?", {1, false}}, {"string?", {1, false}}, {"boolean?", {1, false}},
    {"symbol?", {1, false}}, {"not", {1, false}},
    {">", {2, true}}, {">=", {2, true}}, {"<", {2, true}}, {"<=", {2, true}}, {"=", {2, true}},
    {"string-append", {2, true}}, {"string>?", {2, true}}, {"string<?", {2, true}}, {"string=?", {2, true}},
    {"eqv?", {2, false}}, {"equal?", {2, false}}
};

unordered_map<string, pair<size_t, bool>> user_defined_functions_info;

const unordered_map<string, Function> built_in_functions = {
    {"+", Function("+", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("+", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_arithmetic("+", args);
    })},
    {"-", Function("-", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("-", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_arithmetic("-", args);
    })},
    {"*", Function("*", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("*", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_arithmetic("*", args);
    })},
    {"/", Function("/", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("/", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_arithmetic("/", args);
    })},
    {"cons", Function("cons", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("cons", args, 2, {});
        return make_shared<Node>(args[0], args[1]);
    })},
    {"car", Function("car", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("car", args, 1, {TokenType::CONS});
        return args[0]->car;
    })},
    {"cdr", Function("cdr", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("cdr", args, 1, {TokenType::CONS});
        return args[0]->cdr;
    })},
    {"list", Function("list", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        shared_ptr<Node> head = make_shared<Node>(TokenType::NIL, "nil");
        for (int i = (int)args.size() - 1; i >= 0; i--) {
            head = make_shared<Node>(args[i], head);
        }
        return head;
    })},
    {"atom?", Function("atom?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("atom?", args, 1, {});
        return is_not_specific_type(args[0], {TokenType::CONS});
    })},
    {"pair?", Function("pair?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("pair?", args, 1, {});
        return is_specific_type(args[0], {TokenType::CONS});
    })},
    {"list?", Function("list?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("list?", args, 1, {});
        return is_pure_list(args[0]) ? make_shared<Node>(TokenType::T, "t") : make_shared<Node>(TokenType::NIL, "nil");
    })},
    {"null?", Function("null?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("null?", args, 1, {});
        return is_specific_type(args[0], {TokenType::NIL});
    })},
    {"integer?", Function("integer?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("integer?", args, 1, {});
        return is_specific_type(args[0], {TokenType::INT});
    })},
    {"real?", Function("real?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("real?", args, 1, {});
        return is_specific_type(args[0], {TokenType::INT, TokenType::FLOAT});
    })},
    {"number?", Function("number?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("number?", args, 1, {});
        return is_specific_type(args[0], {TokenType::INT, TokenType::FLOAT});
    })},
    {"string?", Function("string?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("string?", args, 1, {});
        return is_specific_type(args[0], {TokenType::STRING});
    })},
    {"boolean?", Function("boolean?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("boolean?", args, 1, {});
        return is_specific_type(args[0], {TokenType::T, TokenType::NIL});
    })},
    {"symbol?", Function("symbol?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("symbol?", args, 1, {});
        return is_specific_type(args[0], {TokenType::SYMBOL});
    })},
    {"not", Function("not", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("not", args, 1, {});
        return is_specific_type(args[0], {TokenType::NIL});
    })},
    {">", Function(">", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type(">", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_comparison(">", args);
    })},
    {">=", Function(">=", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type(">=", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_comparison(">=", args);
    })},
    {"<", Function("<", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("<", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_comparison("<", args);
    })},
    {"<=", Function("<=", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("<=", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_comparison("<=", args);
    })},
    {"=", Function("=", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("=", args, 2, {TokenType::INT, TokenType::FLOAT}, true);
        return do_comparison("=", args);
    })},
    {"string-append", Function("string-append", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("string-append", args, 2, {TokenType::STRING}, true);
        string concat = "";
        for (const auto &arg : args) {
            string s = get<string>(arg->val);
            if (s.length() >= 2 && s.front() == '"' && s.back() == '"') {
                s = s.substr(1, s.length() - 2);
            }
            concat += s;
        }
        return make_shared<Node>(TokenType::STRING, "\"" + concat + "\"");
    })},
    {"string>?", Function("string>?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("string>?", args, 2, {TokenType::STRING}, true);
        return do_string_comparison("string>?", args);
    })},
    {"string<?", Function("string<?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("string<?", args, 2, {TokenType::STRING}, true);
        return do_string_comparison("string<?", args);
    })},
    {"string=?", Function("string=?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("string=?", args, 2, {TokenType::STRING}, true);
        return do_string_comparison("string=?", args);
    })},
    {"eqv?", Function("eqv?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("eqv?", args, 2, {});
        if (args[0] == args[1]) return make_shared<Node>(TokenType::T, "t");
         if (args[0]->type == TokenType::CONS || args[0]->type == TokenType::STRING ||
            args[1]->type == TokenType::CONS || args[1]->type == TokenType::STRING) {
            return make_shared<Node>(TokenType::NIL, "nil");
        }
        // 此後 args 皆為 Atom 可直接使用 is_equal 進行比較
        return is_equal(args[0], args[1]) ? make_shared<Node>(TokenType::T, "t") : make_shared<Node>(TokenType::NIL, "nil");
    })},
    {"equal?", Function("equal?", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        check_params_count_and_type("equal?", args, 2, {});
        return is_equal(args[0], args[1]) ? make_shared<Node>(TokenType::T, "t") : make_shared<Node>(TokenType::NIL, "nil");
    })},
    {"clean-environment", Function("clean-environment", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        throw runtime_error("ERROR (incorrect number of arguments) : clean-environment");
    })},
    {"exit", Function("exit", [](const vector<shared_ptr<Node>> &args) -> shared_ptr<Node> {
        throw runtime_error("ERROR (incorrect number of arguments) : exit");
    })}
};

unordered_map<string, Function> user_defined_functions;

// ========================================Helper Functions Implementation========================================

bool is_in(const string &str, const unordered_set<string> &targets) { return targets.find(str) != targets.end(); }

void check_params_count_and_type(const string &func_name, const vector<shared_ptr<Node>> &args, size_t expected_count, const unordered_set<TokenType> &expected_types, bool is_ge) {
    if (!is_ge && args.size() != expected_count) {
        throw runtime_error("ERROR (incorrect number of arguments) : " + func_name);
    } else if (is_ge && args.size() < expected_count) {
        throw runtime_error("ERROR (incorrect number of arguments) : " + func_name);
    }
    if (!expected_types.empty()) {
        for (const auto &arg : args) {
            if (expected_types.find(arg->type) == expected_types.end()) {
                throw runtime_error("ERROR (" + func_name + " with incorrect argument type) : " + pretty_print(arg));
            }
        }
    }
}

bool is_pure_list(const shared_ptr<Node> &node) {
    if (node == nullptr) return true;
    if (node->type == TokenType::NIL) return true;
    if (node->type != TokenType::CONS) return false;
    shared_ptr<Node> cur_node = node;
    while (cur_node != nullptr && cur_node->type == TokenType::CONS) cur_node = cur_node->cdr;
    return cur_node == nullptr || cur_node->type == TokenType::NIL;
}

int get_list_length(const shared_ptr<Node> &node) {
    int len = 0;
    shared_ptr<Node> cur_node = node;
    while (cur_node != nullptr && cur_node->type == TokenType::CONS) {
        len++;
        cur_node = cur_node->cdr;
    }
    return len;
}

string get_s_exp_string(const shared_ptr<Node> &node, int indent, bool first_on_line) {
    if (node == nullptr) return "";
    stringstream ss;
    if (node->type != TokenType::CONS) {
        if (first_on_line) ss << string(indent, ' ');
        if (node->type == TokenType::NIL) {
            ss << "nil";
        } else if (node->type == TokenType::T) {
            ss << "#t";
        } else if (node->type == TokenType::INT) {
            if (holds_alternative<int64>(node->val)) {
                ss << get<int64>(node->val);
            } else if (holds_alternative<string>(node->val)) {
                try {
                    long long val = stoll(get<string>(node->val));
                    ss << val;
                } catch (...) {
                    ss << get<string>(node->val);
                }
            }
        } else if (node->type == TokenType::FLOAT) {
            if (holds_alternative<double>(node->val)) {
                stringstream temp;
                temp << fixed << setprecision(3) << get<double>(node->val);
                ss << temp.str();
            } else if (holds_alternative<string>(node->val)) {
                try {
                    double val = stod(get<string>(node->val));
                    stringstream temp;
                    temp << fixed << setprecision(3) << val;
                    ss << temp.str();
                } catch (...) {
                    ss << get<string>(node->val);
                }
            }
        } else if (holds_alternative<Function>(node->val)) {
            ss << "#<procedure " << get<Function>(node->val).name << ">";
        } else if (holds_alternative<string>(node->val)) {
            ss << get<string>(node->val);
        }
        ss << "\n";
    } else {
        if (first_on_line) ss << string(indent, ' ');
        ss << "( ";
        
        shared_ptr<Node> cur_node = node;
        ss << get_s_exp_string(cur_node->car, indent + 2, false);
        
        while (cur_node->cdr != nullptr && cur_node->cdr->type == TokenType::CONS) {
            cur_node = cur_node->cdr;
            ss << get_s_exp_string(cur_node->car, indent + 2, true);
        }
        
        if (cur_node->cdr != nullptr && cur_node->cdr->type != TokenType::NIL) {
            ss << string(indent + 2, ' ') << ".\n";
            ss << get_s_exp_string(cur_node->cdr, indent + 2, true);
        }
        
        ss << string(indent, ' ') << ")\n";
    }
    return ss.str();
}

string pretty_print(const shared_ptr<Node> &node) {
    string s = get_s_exp_string(node, 0, false);
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

bool is_system_primitive(const string &name) {
    static const unordered_set<string> primitives = {
        "+", "-", "*", "/", "cons", "car", "cdr",
        "atom?", "pair?", "list?", "null?", "integer?", "real?", "number?", "string?", "boolean?", "symbol?",
        "not", "and", "or", ">", ">=", "<", "<=", "=", "string-append", "string>?", "string<?", "string=?",
        "eqv?", "equal?", "begin", "if", "cond", "clean-environment", "exit", "define", "quote", "nil", "t"
    };
    return primitives.find(name) != primitives.end();
}

int64 get_int_val(const shared_ptr<Node> &n) {
    if (holds_alternative<int64>(n->val)) return get<int64>(n->val);
    return stoll(get<string>(n->val));
}

double get_float_val(const shared_ptr<Node> &n) {
    if (holds_alternative<double>(n->val)) return get<double>(n->val);
    return stod(get<string>(n->val));
}

bool is_equal(const shared_ptr<Node> &n1, const shared_ptr<Node> &n2) {
    if (n1 == n2) return true;
    if (n1 == nullptr || n2 == nullptr) return false;
    if (n1->type != n2->type) return false; // 在此之後的 type 必定相同
    if (n1->type == TokenType::CONS) {
        return is_equal(n1->car, n2->car) && is_equal(n1->cdr, n2->cdr);
    }
    if (n1->type == TokenType::INT) {
        return get_int_val(n1) == get_int_val(n2);
    }
    if (n1->type == TokenType::FLOAT) {
        return abs(get_float_val(n1) - get_float_val(n2)) < 1e-9;
    }
    if (n1->type == TokenType::NIL) return true;
    if (n1->type == TokenType::T) return true;
    if (n1->type == TokenType::SYMBOL || n1->type == TokenType::STRING) {
        return get<string>(n1->val) == get<string>(n2->val);
    }
    if (holds_alternative<Function>(n1->val) && holds_alternative<Function>(n2->val)) {
        return get<Function>(n1->val).name == get<Function>(n2->val).name;
    }
    return false;
}

shared_ptr<Node> do_arithmetic(const string &op, const vector<shared_ptr<Node>> &args) {
    bool has_float = false;
    for (const auto &arg : args) {
        if (arg->type == TokenType::FLOAT) has_float = true;
    }
    if (has_float) {
        double result = (args[0]->type == TokenType::INT) ? get_int_val(args[0]) : get_float_val(args[0]);
        for (size_t i = 1; i < args.size(); i++) {
            double val = (args[i]->type == TokenType::INT) ? get_int_val(args[i]) : get_float_val(args[i]);
            if (op == "+") result += val;
            else if (op == "-") result -= val;
            else if (op == "*") result *= val;
            else if (op == "/") {
                if (abs(val) < 1e-9) throw runtime_error("ERROR (division by zero) : /");
                result /= val;
            }
        }
        return make_shared<Node>(TokenType::FLOAT, result);
    } else {
        int64 result = get_int_val(args[0]);
        for (size_t i = 1; i < args.size(); i++) {
            int64 val = get_int_val(args[i]);
            if (op == "+") result += val;
            else if (op == "-") result -= val;
            else if (op == "*") result *= val;
            else if (op == "/") {
                if (val == 0) throw runtime_error("ERROR (division by zero) : /");
                result /= val;
            }
        }
        return make_shared<Node>(TokenType::INT, result);
    }
}

shared_ptr<Node> is_specific_type(const shared_ptr<Node> &node, unordered_set<TokenType> type) {
    return (node != nullptr && type.find(node->type) != type.end()) ? 
            make_shared<Node>(TokenType::T, "t") : make_shared<Node>(TokenType::NIL, "nil");
}

shared_ptr<Node> is_not_specific_type(const shared_ptr<Node> &node, unordered_set<TokenType> type) {
    return (node == nullptr || type.find(node->type) == type.end()) ? 
            make_shared<Node>(TokenType::T, "t") : make_shared<Node>(TokenType::NIL, "nil");
}

shared_ptr<Node> do_comparison(const string &op, const vector<shared_ptr<Node>> &args) {
    double prev_val = (args[0]->type == TokenType::INT) ? get_int_val(args[0]) : get_float_val(args[0]);
    bool result = true;
    for (size_t i = 1; i < args.size(); i++) {
        double cur_val = (args[i]->type == TokenType::INT) ? get_int_val(args[i]) : get_float_val(args[i]);
        if (op == ">") result = result && (prev_val > cur_val);
        else if (op == ">=") result = result && (prev_val >= cur_val);
        else if (op == "<") result = result && (prev_val < cur_val);
        else if (op == "<=") result = result && (prev_val <= cur_val);
        else if (op == "=") result = result && (abs(prev_val - cur_val) < 1e-9);
        prev_val = cur_val;
    }
    return result ? make_shared<Node>(TokenType::T, "t") : make_shared<Node>(TokenType::NIL, "nil");
}

shared_ptr<Node> do_string_comparison(const string &op, const vector<shared_ptr<Node>> &args) {
    string prev_str = get<string>(args[0]->val);
    if (prev_str.length() >= 2 && prev_str.front() == '"' && prev_str.back() == '"') {
        prev_str = prev_str.substr(1, prev_str.length() - 2);
    }
    bool result = true;
    for (size_t i = 1; i < args.size(); i++) {
        string cur_str = get<string>(args[i]->val);
        if (cur_str.length() >= 2 && cur_str.front() == '"' && cur_str.back() == '"') {
            cur_str = cur_str.substr(1, cur_str.length() - 2);
        }

        if (op == "string>?") result = result && (prev_str > cur_str);
        else if (op == "string<?") result = result && (prev_str < cur_str);
        else if (op == "string=?") result = result && (prev_str == cur_str);
        prev_str = cur_str;
    }
    return result ? make_shared<Node>(TokenType::T, "t") : make_shared<Node>(TokenType::NIL, "nil");
}

bool get_expected_params_info(const string &func_name, size_t &expected_count, bool &is_ge) {
    auto it = built_in_functions_info.find(func_name);
    if (it != built_in_functions_info.end()) {
        expected_count = it->second.first;
        is_ge = it->second.second;
        return true;
    } 
    auto it2 = user_defined_functions_info.find(func_name);
    if (it2 != user_defined_functions_info.end()) {
        expected_count = it2->second.first;
        is_ge = it2->second.second;
        return true;
    }
    return false;
}

void print_s_exp(const shared_ptr<Node> &root) {
    cout << pretty_print(root) << endl;
}

// ========================================Core Interpreter Implementation========================================

shared_ptr<Node> eval(const shared_ptr<Node> &node, const shared_ptr<Environment> &env, bool is_top_level) {
    if (node == nullptr) return nullptr;
    
    if (node->type != TokenType::CONS) {
        if (node->type == TokenType::SYMBOL) {
            string symbol_name = get<string>(node->val);
            shared_ptr<Environment> cur_node = env;
            while (cur_node != nullptr) {
                auto it = cur_node->bindings.find(symbol_name);
                if (it != cur_node->bindings.end()) return it->second;
                cur_node = cur_node->parent;
            }
            throw runtime_error("ERROR (unbound symbol) : " + symbol_name);
        }
        return node;
    }
    
    if (!is_pure_list(node)) {
        throw runtime_error("ERROR (non-list) : " + pretty_print(node));
    }
    
    auto first = node->car;
    if (first == nullptr) {
        throw runtime_error("ERROR (attempt to apply non-function) : nil");
    }
    
    if (first->type == TokenType::SYMBOL) {
        string symbol = get<string>(first->val);
        if (symbol == "define") {
            if (!is_top_level) {
                throw runtime_error("ERROR (level of DEFINE)");
            }
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (non-list) : " + pretty_print(node));
            }
            int len = get_list_length(node->cdr);
            shared_ptr<Node> symbol_node = (len >= 1) ? node->cdr->car : nullptr;
            shared_ptr<Node> exp_node = (len >= 2) ? node->cdr->cdr->car : nullptr;
            if (len != 2 || symbol_node == nullptr || symbol_node->type != TokenType::SYMBOL || is_system_primitive(get<string>(symbol_node->val))) {
                throw runtime_error("ERROR (DEFINE format) : " + pretty_print(node));
            }
            string name = get<string>(symbol_node->val);
            auto val = eval(exp_node, env, false);
            if (val == nullptr) {
                throw runtime_error("ERROR (no return value) : " + pretty_print(exp_node));
            }
            env->bindings[name] = val;
            cout << name << " defined" << endl;
            return nullptr;
        } else if (symbol == "let") { 
            
        } else if (symbol == "lambda") { 
            
        } else if (symbol == "quote") {
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (non-list) : " + pretty_print(node));
            }
            int len = get_list_length(node->cdr);
            shared_ptr<Node> arg = (len >= 1) ? node->cdr->car : nullptr;
            if (len != 1) {
                throw runtime_error("ERROR (incorrect number of arguments) : quote");
            }
            return arg;
        } else if (symbol == "if") { // (if test then [else]) : if 評估 test 為真則回傳 then 的值，否則回傳 else 的值，如果跳至 else 時不存在則丟出錯誤
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (non-list) : " + pretty_print(node));
            }
            int len = get_list_length(node->cdr);
            shared_ptr<Node> test_node = (len >= 1) ? node->cdr->car : nullptr;
            shared_ptr<Node> then_node = (len >= 2) ? node->cdr->cdr->car : nullptr;
            shared_ptr<Node> else_node = (len >= 3) ? node->cdr->cdr->cdr->car : nullptr;
            if (len < 2 || len > 3) {
                throw runtime_error("ERROR (incorrect number of arguments) : if");
            }
            auto test_val = eval(test_node, env, false);
            if (test_val == nullptr) {
                throw runtime_error("ERROR (unbound test-condition) : " + pretty_print(test_node));
            }
            bool condition = true;
            if (test_val->type == TokenType::NIL) {
                condition = false;
            }
            if (condition) {
                return eval(then_node, env, false);
            } else {
                if (else_node != nullptr) {
                    return eval(else_node, env, false);
                } else {
                    throw runtime_error("ERROR (no return value) : " + pretty_print(node));
                }
            }
        } else if (symbol == "cond") { // 比起 if 可以包含多個條件分支，每個條件分支中可以包含多個表達式，並回傳最後一個表達式的值
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (COND format) : " + pretty_print(node));
            }
            vector<shared_ptr<Node>> cond_clauses;
            auto cur_node = node->cdr;
            while (cur_node != nullptr && cur_node->type == TokenType::CONS) {
                cond_clauses.push_back(cur_node->car);
                cur_node = cur_node->cdr;
            }
            if (cond_clauses.empty()) {
                throw runtime_error("ERROR (COND format) : " + pretty_print(node));
            }
            for (size_t i = 0; i < cond_clauses.size(); i++) {
                if (!is_pure_list(cond_clauses[i]) || get_list_length(cond_clauses[i]) < 2 || 
                    cond_clauses[i]->type == TokenType::NIL) {
                    throw runtime_error("ERROR (COND format) : " + pretty_print(node));
                }
            }
            for (size_t i = 0; i < cond_clauses.size(); i++) {
                auto cond_clause = cond_clauses[i];
                auto first_element = cond_clause->car;
                bool is_last = (i == cond_clauses.size() - 1); // 判斷是否為最後一個 clause
                bool condition = false;
                shared_ptr<Node> test_val = nullptr;
                
                if (first_element->type == TokenType::SYMBOL && get<string>(first_element->val) == "else" && is_last) {
                    condition = true;
                } else {
                    test_val = eval(first_element, env, false);
                    if (test_val == nullptr) {
                        throw runtime_error("ERROR (unbound test-condition) : " + pretty_print(first_element));
                    }
                    if (test_val->type != TokenType::NIL) {
                        condition = true;
                    }
                }
                
                if (condition) {
                    auto rest_nodes = cond_clause->cdr;
                    if (rest_nodes == nullptr || rest_nodes->type == TokenType::NIL) {
                        if (test_val != nullptr) return test_val;
                        return make_shared<Node>(TokenType::T, "t");
                    }
                    shared_ptr<Node> last_val = nullptr;
                    auto cur_exp = rest_nodes;
                    while (cur_exp != nullptr && cur_exp->type == TokenType::CONS) {
                        last_val = eval(cur_exp->car, env, false);
                        cur_exp = cur_exp->cdr;
                    }
                    return last_val;
                }
            }
            throw runtime_error("ERROR (no return value) : " + pretty_print(node));
        } else if (symbol == "begin") {
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (non-list) : " + pretty_print(node));
            }
            auto cur_node = node->cdr;
            if (cur_node == nullptr || cur_node->type == TokenType::NIL) {
                throw runtime_error("ERROR (incorrect number of arguments) : begin");
            }
            shared_ptr<Node> last_val = nullptr;
            while (cur_node != nullptr && cur_node->type == TokenType::CONS) {
                last_val = eval(cur_node->car, env, false);
                cur_node = cur_node->cdr;
            }
            return last_val;
        } else if (symbol == "clean-environment") {
            if (!is_top_level) {
                throw runtime_error("ERROR (level of CLEAN-ENVIRONMENT)");
            }
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (non-list) : " + pretty_print(node));
            }
            if (node->cdr != nullptr && node->cdr->type != TokenType::NIL) {
                throw runtime_error("ERROR (incorrect number of arguments) : clean-environment");
            }
            env->bindings.clear();
            for (auto const &[name, func] : built_in_functions) {
                auto func_node = make_shared<Node>();
                func_node->type = TokenType::Undefined;
                func_node->val = func;
                env->bindings[name] = func_node;
            }
            cout << "environment cleaned" << endl;
            return nullptr;
        } else if (symbol == "exit") {
            if (!is_top_level) {
                throw runtime_error("ERROR (level of EXIT)");
            }
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (non-list) : " + pretty_print(node));
            }
            if (node->cdr != nullptr && node->cdr->type != TokenType::NIL) {
                throw runtime_error("ERROR (incorrect number of arguments) : exit");
            }
            cout << endl << "Thanks for using OurScheme!" << endl;
            exit(0);
        } else if (symbol == "and") {
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (non-list) : " + pretty_print(node));
            }
            vector<shared_ptr<Node>> args;
            auto cur_node = node->cdr;
            while (cur_node != nullptr && cur_node->type == TokenType::CONS) {
                args.push_back(cur_node->car);
                cur_node = cur_node->cdr;
            }
            if (args.size() < 2) {
                throw runtime_error("ERROR (incorrect number of arguments) : and");
            }
            shared_ptr<Node> val = nullptr;
            for (size_t i = 0; i < args.size(); i++) {
                val = eval(args[i], env, false);
                if (val == nullptr) {
                    throw runtime_error("ERROR (unbound condition) : " + pretty_print(args[i]));
                }
                if (val->type == TokenType::NIL) {
                    return val;
                }
            }
            return val;
        } else if (symbol == "or") {
            if (!is_pure_list(node)) {
                throw runtime_error("ERROR (non-list) : " + pretty_print(node));
            }
            vector<shared_ptr<Node>> args;
            auto cur_node = node->cdr;
            while (cur_node != nullptr && cur_node->type == TokenType::CONS) {
                args.push_back(cur_node->car);
                cur_node = cur_node->cdr;
            }
            if (args.size() < 2) {
                throw runtime_error("ERROR (incorrect number of arguments) : or");
            }
            shared_ptr<Node> val = nullptr;
            for (size_t i = 0; i < args.size(); i++) {
                val = eval(args[i], env, false);
                if (val == nullptr) {
                    throw runtime_error("ERROR (unbound condition) : " + pretty_print(args[i]));
                }
                if (val->type != TokenType::NIL) {
                    return val;
                }
            }
            return val;
        }
    }
    
    auto func_val = eval(first, env, false);
    if (func_val == nullptr) {
        if (first->type == TokenType::SYMBOL) {
            throw runtime_error("ERROR (unbound symbol) : " + get<string>(first->val));
        } else {
            throw runtime_error("ERROR (attempt to apply non-function) : nil");
        }
    }
    if (!holds_alternative<Function>(func_val->val)) {
        throw runtime_error("ERROR (attempt to apply non-function) : " + pretty_print(func_val));
    }
    
    Function func_object = get<Function>(func_val->val);
    size_t expected_count = 0;
    bool is_ge = false;
    // 檢查參數數量是否正確
    if (get_expected_params_info(func_object.name, expected_count, is_ge)) {
        size_t actual_args_count = get_list_length(node->cdr);
        if (!is_ge && actual_args_count != expected_count) {
            throw runtime_error("ERROR (incorrect number of arguments) : " + func_object.name);
        } else if (is_ge && actual_args_count < expected_count) {
            throw runtime_error("ERROR (incorrect number of arguments) : " + func_object.name);
        }
    }
    // 檢查後確定正確才開始使用 eval 函數求值
    vector<shared_ptr<Node>> eval_args;
    auto cur_node = node->cdr;
    while (cur_node != nullptr && cur_node->type == TokenType::CONS) {
        auto arg_val = eval(cur_node->car, env, false);
        if (arg_val == nullptr) {
            throw runtime_error("ERROR (unbound parameter) : " + pretty_print(cur_node->car));
        }
        eval_args.push_back(arg_val);
        cur_node = cur_node->cdr;
    }
    
    return func_object.func(eval_args);
}

// ========================================Lexer and Parser Implementation========================================

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
            if (isspace(c) || c == '(' || c == ')' || c == '\'' || c == '"' || c == ';') break;
            val += c;
            idx++;
        }

        if (val == ".") return Token{TokenType::DOT, val, cur_line, col, start_idx, idx};
        if (val == "nil" || val == "#f") return Token{TokenType::NIL, val, cur_line, col, start_idx, idx};
        if (val == "t" || val == "#t") return Token{TokenType::T, val, cur_line, col, start_idx, idx};
        if (is_valid_int(val)) return Token{TokenType::INT, val, cur_line, col, start_idx, idx};
        if (is_valid_float(val)) return Token{TokenType::FLOAT, val, cur_line, col, start_idx, idx};
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
        if (tk.type == TokenType::EndOfFile) 
            throw runtime_error("ERROR (no more input) : END-OF-FILE encountered");
        string err = "ERROR (unexpected token) : " + expected + " expected when token at Line " +
                     to_string(tk.line) + " Column " + to_string(tk.column) + " is >>" + tk.val + "<<";
        throw runtime_error(err);
    }

    shared_ptr<Node> parse_s_exp_internal() {
        if (cur_token.type == TokenType::Undefined) throw runtime_error(cur_token.val); // TODO: 檢查這行是否會有問題
        if (cur_token.type == TokenType::EndOfFile) throw_unexpected_error(cur_token, "atom or '('");

        if (cur_token.type == TokenType::LEFT_PAREN) {
            next();
            if (cur_token.type == TokenType::RIGHT_PAREN) {
                next();
                return make_shared<Node>(TokenType::NIL, "nil");
            }
            shared_ptr<Node> car = parse_s_exp_internal();
            shared_ptr<Node> head = make_shared<Node>(car, make_shared<Node>(TokenType::NIL, "nil"));
            shared_ptr<Node> cur_node = head;
            while (cur_token.type != TokenType::RIGHT_PAREN && cur_token.type != TokenType::DOT && cur_token.type != TokenType::EndOfFile) {
                shared_ptr<Node> next_el = parse_s_exp_internal();
                shared_ptr<Node> next_node = make_shared<Node>(next_el, make_shared<Node>(TokenType::NIL, "nil"));
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

    bool is_eof() { return cur_token.type == TokenType::EndOfFile; }

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

// ========================================Main Flow Implementation========================================

void parse_wrapper(Parser &parser) {
    cout << "Welcome to OurScheme!\n\n";
    
    while (true) {
        cout << "> ";
        cout.flush();
        shared_ptr<Node> root = nullptr;
        try {
            root = parser.parse_s_exp();
        } catch (const exception &e) {
            cout << e.what() << endl;
            parser.recover_after_error();
            if (parser.is_eof()) {
                break;
            }
            cout << endl;
            continue;
        }
        
        if (root == nullptr) break;
        
        try {
            auto result = eval(root, global_env, true);
            if (result != nullptr) {
                print_s_exp(result);
            }
            cout << endl; // Prints a blank line after output
        } catch (const exception &e) {
            cout << e.what() << endl;
            cout << endl; // Prints a blank line after output
        }
        
        parser.reset_for_next_s_exp();
    }
    
    cout << "Thanks for using OurScheme!" << endl;
}

int main() {
    cout << fixed << setprecision(3);

    global_env = make_shared<Environment>();
    for (auto const &[name, func] : built_in_functions) {
        auto func_node = make_shared<Node>();
        func_node->type = TokenType::Undefined;
        func_node->val = func;
        global_env->bindings[name] = func_node;
    }
    cur_env = global_env;

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