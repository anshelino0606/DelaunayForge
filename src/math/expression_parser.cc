#include "expression_parser.h"
#include <stack>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cerrno>

namespace fem {

const std::unordered_map<std::string, int> ExpressionParser::operator_precedence_ = {
    {"+", 1}, {"-", 1},
    {"*", 2}, {"/", 2}, {"%", 2},
    {"^", 3}
};

const std::unordered_map<std::string, int> ExpressionParser::function_arg_counts_ = {
    {"sin", 1}, {"cos", 1}, {"tan", 1},
    {"asin", 1}, {"acos", 1}, {"atan", 1},
    {"sinh", 1}, {"cosh", 1}, {"tanh", 1},
    {"exp", 1}, {"log", 1}, {"ln", 1}, {"log10", 1},
    {"sqrt", 1}, {"abs", 1}, {"floor", 1}, {"ceil", 1},
    {"sign", 1},
    {"pow", 2}, {"atan2", 2}, {"min", 2}, {"max", 2}, {"mod", 2}
};

ExpressionParser::ExpressionParser() {}

bool ExpressionParser::is_operator(char c) const {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%';
}

bool ExpressionParser::is_digit(char c) const {
    return std::isdigit(static_cast<unsigned char>(c));
}

bool ExpressionParser::is_alpha(char c) const {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool ExpressionParser::is_function(const std::string& name) const {
    return function_arg_counts_.find(name) != function_arg_counts_.end();
}

int ExpressionParser::precedence(const std::string& op) const {
    auto it = operator_precedence_.find(op);
    return (it != operator_precedence_.end()) ? it->second : -1;
}

bool ExpressionParser::is_left_associative(const std::string& op) const {
    return op != "^";
}

std::optional<std::vector<Token>> ExpressionParser::tokenize(const std::string& expr) {
    std::vector<Token> tokens;
    size_t i = 0;
    
    while (i < expr.length()) {
        char c = expr[i];
        
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        
        if (is_digit(c) || (c == '.' && i + 1 < expr.length() && is_digit(expr[i + 1]))) {
            size_t start = i;
            bool has_dot = false;
            bool has_exp = false;
            
            while (i < expr.length()) {
                c = expr[i];
                if (is_digit(c)) {
                    ++i;
                } else if (c == '.' && !has_dot && !has_exp) {
                    has_dot = true;
                    ++i;
                } else if ((c == 'e' || c == 'E') && !has_exp) {
                    has_exp = true;
                    ++i;
                    if (i < expr.length() && (expr[i] == '+' || expr[i] == '-')) {
                        ++i;
                    }
                } else {
                    break;
                }
            }
            
            std::string num_str = expr.substr(start, i - start);

            errno = 0;
            char* end = nullptr;
            const double v = std::strtod(num_str.c_str(), &end);
            if (end == num_str.c_str() || *end != '\0' || errno == ERANGE) {
                error_message_ = "Invalid number format: " + num_str;
                return std::nullopt;
            }

            Token tok;
            tok.type = TokenType::Number;
            tok.value = num_str;
            tok.number_value = v;
            tokens.push_back(tok);
            continue;
        }
        
        if (is_alpha(c)) {
            size_t start = i;
            while (i < expr.length() && (is_alpha(expr[i]) || is_digit(expr[i]))) {
                ++i;
            }
            
            std::string name = expr.substr(start, i - start);
            
            std::string name_lower = name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                         [](unsigned char c) { return std::tolower(c); });
            
            Token tok;
            tok.value = name_lower;
            
            if (i < expr.length() && expr[i] == '(') {
                if (is_function(name_lower)) {
                    tok.type = TokenType::Function;
                } else {
                    error_message_ = "Unknown function: " + name;
                    return std::nullopt;
                }
            } else {
                tok.type = TokenType::Variable;
            }
            
            tokens.push_back(tok);
            continue;
        }
        
        if (c == '(') {
            Token tok;
            tok.type = TokenType::LeftParen;
            tok.value = "(";
            tokens.push_back(tok);
            ++i;
            continue;
        }
        
        if (c == ')') {
            Token tok;
            tok.type = TokenType::RightParen;
            tok.value = ")";
            tokens.push_back(tok);
            ++i;
            continue;
        }
        
        if (is_operator(c)) {
            Token tok;
            tok.type = TokenType::Operator;
            tok.value = std::string(1, c);
            tokens.push_back(tok);
            ++i;
            continue;
        }
        
        if (c == ',') {
            Token tok;
            tok.type = TokenType::Operator;
            tok.value = ",";
            tokens.push_back(tok);
            ++i;
            continue;
        }
        
        error_message_ = std::string("Unexpected character: ") + c;
        return std::nullopt;
    }
    
    return tokens;
}

std::optional<std::vector<Token>> ExpressionParser::infix_to_rpn(const std::vector<Token>& tokens) {
    std::vector<Token> output;
    std::stack<Token> operators;
    std::stack<int> arg_counts;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& tok = tokens[i];
        
        switch (tok.type) {
            case TokenType::Number:
            case TokenType::Variable:
                output.push_back(tok);
                break;
                
            case TokenType::Function:
                operators.push(tok);
                arg_counts.push(1);
                break;
                
            case TokenType::Operator:
                if (tok.value == ",") {
                    while (!operators.empty() && operators.top().type != TokenType::LeftParen) {
                        output.push_back(operators.top());
                        operators.pop();
                    }
                    if (!arg_counts.empty()) {
                        arg_counts.top()++;
                    }
                } else {
                    bool is_unary = (i == 0) || 
                                   (tokens[i-1].type == TokenType::Operator) ||
                                   (tokens[i-1].type == TokenType::LeftParen);
                    
                    if (is_unary && tok.value == "-") {
                        Token zero;
                        zero.type = TokenType::Number;
                        zero.value = "0";
                        zero.number_value = 0.0;
                        output.push_back(zero);
                    }
                    
                    while (!operators.empty() && 
                           operators.top().type == TokenType::Operator &&
                           ((is_left_associative(tok.value) && 
                             precedence(tok.value) <= precedence(operators.top().value)) ||
                            (!is_left_associative(tok.value) && 
                             precedence(tok.value) < precedence(operators.top().value)))) {
                        output.push_back(operators.top());
                        operators.pop();
                    }
                    
                    operators.push(tok);
                }
                break;
                
            case TokenType::LeftParen:
                operators.push(tok);
                break;
                
            case TokenType::RightParen:
                while (!operators.empty() && operators.top().type != TokenType::LeftParen) {
                    output.push_back(operators.top());
                    operators.pop();
                }
                
                if (operators.empty()) {
                    error_message_ = "Mismatched parentheses";
                    return std::nullopt;
                }
                
                operators.pop();
                
                if (!operators.empty() && operators.top().type == TokenType::Function) {
                    Token func = operators.top();
                    operators.pop();
                    
                    if (!arg_counts.empty()) {
                        func.number_value = arg_counts.top();
                        arg_counts.pop();
                    }
                    
                    output.push_back(func);
                }
                break;
        }
    }
    
    while (!operators.empty()) {
        if (operators.top().type == TokenType::LeftParen) {
            error_message_ = "Mismatched parentheses";
            return std::nullopt;
        }
        output.push_back(operators.top());
        operators.pop();
    }
    
    return output;
}

std::optional<double> ExpressionParser::evaluate_function(const std::string& name, double arg) {
    if (name == "sin") return std::sin(arg);
    if (name == "cos") return std::cos(arg);
    if (name == "tan") return std::tan(arg);
    if (name == "asin") return std::asin(arg);
    if (name == "acos") return std::acos(arg);
    if (name == "atan") return std::atan(arg);
    if (name == "sinh") return std::sinh(arg);
    if (name == "cosh") return std::cosh(arg);
    if (name == "tanh") return std::tanh(arg);
    if (name == "exp") return std::exp(arg);
    if (name == "log" || name == "ln") return std::log(arg);
    if (name == "log10") return std::log10(arg);
    if (name == "sqrt") return std::sqrt(arg);
    if (name == "abs") return std::abs(arg);
    if (name == "floor") return std::floor(arg);
    if (name == "ceil") return std::ceil(arg);
    if (name == "sign") return (arg > 0) ? 1.0 : ((arg < 0) ? -1.0 : 0.0);
    
    error_message_ = "Unknown function: " + name;
    return std::nullopt;
}

std::optional<double> ExpressionParser::evaluate_function_2arg(const std::string& name, 
                                                                double arg1, double arg2) {
    if (name == "pow") return std::pow(arg1, arg2);
    if (name == "atan2") return std::atan2(arg1, arg2);
    if (name == "min") return std::min(arg1, arg2);
    if (name == "max") return std::max(arg1, arg2);
    if (name == "mod") return std::fmod(arg1, arg2);
    
    error_message_ = "Unknown 2-argument function: " + name;
    return std::nullopt;
}

std::optional<double> ExpressionParser::evaluate_rpn(
    const std::vector<Token>& rpn,
    const std::unordered_map<std::string, double>& variables) 
{
    std::stack<double> stack;
    
    for (const Token& tok : rpn) {
        switch (tok.type) {
            case TokenType::Number:
                stack.push(tok.number_value);
                break;
                
            case TokenType::Variable: {
                auto it = variables.find(tok.value);
                if (it == variables.end()) {
                    error_message_ = "Undefined variable: " + tok.value;
                    return std::nullopt;
                }
                stack.push(it->second);
                break;
            }
                
            case TokenType::Operator: {
                if (stack.size() < 2) {
                    error_message_ = "Invalid expression: insufficient operands";
                    return std::nullopt;
                }
                
                double b = stack.top(); stack.pop();
                double a = stack.top(); stack.pop();
                
                if (tok.value == "+") stack.push(a + b);
                else if (tok.value == "-") stack.push(a - b);
                else if (tok.value == "*") stack.push(a * b);
                else if (tok.value == "/") {
                    if (std::abs(b) < 1e-15) {
                        error_message_ = "Division by zero";
                        return std::nullopt;
                    }
                    stack.push(a / b);
                }
                else if (tok.value == "^") stack.push(std::pow(a, b));
                else if (tok.value == "%") stack.push(std::fmod(a, b));
                else {
                    error_message_ = "Unknown operator: " + tok.value;
                    return std::nullopt;
                }
                break;
            }
                
            case TokenType::Function: {
                int arg_count = static_cast<int>(tok.number_value);
                
                if (arg_count == 1) {
                    if (stack.empty()) {
                        error_message_ = "Invalid expression: insufficient operands for function";
                        return std::nullopt;
                    }
                    double arg = stack.top(); stack.pop();
                    auto result = evaluate_function(tok.value, arg);
                    if (!result) return std::nullopt;
                    stack.push(*result);
                } else if (arg_count == 2) {
                    if (stack.size() < 2) {
                        error_message_ = "Invalid expression: insufficient operands for function";
                        return std::nullopt;
                    }
                    double arg2 = stack.top(); stack.pop();
                    double arg1 = stack.top(); stack.pop();
                    auto result = evaluate_function_2arg(tok.value, arg1, arg2);
                    if (!result) return std::nullopt;
                    stack.push(*result);
                }
                break;
            }
                
            default:
                error_message_ = "Unexpected token in RPN";
                return std::nullopt;
        }
    }
    
    if (stack.size() != 1) {
        error_message_ = "Invalid expression: incorrect number of values";
        return std::nullopt;
    }
    
    return stack.top();
}

std::optional<double> ExpressionParser::evaluate(
    const std::string& expr,
    const std::unordered_map<std::string, double>& variables) 
{
    error_message_.clear();
    
    auto tokens = tokenize(expr);
    if (!tokens) return std::nullopt;
    
    auto rpn = infix_to_rpn(*tokens);
    if (!rpn) return std::nullopt;
    
    return evaluate_rpn(*rpn, variables);
}

bool ExpressionParser::validate(const std::string& expr) {
    error_message_.clear();
    
    auto tokens = tokenize(expr);
    if (!tokens) return false;
    
    auto rpn = infix_to_rpn(*tokens);
    return rpn.has_value();
}

std::vector<std::string> ExpressionParser::get_variables(const std::string& expr) {
    auto tokens = tokenize(expr);
    if (!tokens) return {};
    
    std::vector<std::string> vars;
    for (const Token& tok : *tokens) {
        if (tok.type == TokenType::Variable) {
            if (std::find(vars.begin(), vars.end(), tok.value) == vars.end()) {
                vars.push_back(tok.value);
            }
        }
    }
    
    return vars;
}

} // namespace fem