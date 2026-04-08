#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <cmath>

namespace fem {

enum class TokenType {
    Number,
    Variable,
    Operator,
    Function,
    LeftParen,
    RightParen
};

struct Token {
    TokenType type;
    std::string value;
    double number_value = 0.0;
};

class ExpressionParser {
public:
    ExpressionParser();
    
    std::optional<double> evaluate(const std::string& expr, 
                                   const std::unordered_map<std::string, double>& variables);
    
    bool validate(const std::string& expr);
    
    std::vector<std::string> get_variables(const std::string& expr);
    
    const std::string& last_error() const { return error_message_; }
    
private:
    std::string error_message_;
    
    std::optional<std::vector<Token>> tokenize(const std::string& expr);
    
    std::optional<std::vector<Token>> infix_to_rpn(const std::vector<Token>& tokens);
    
    std::optional<double> evaluate_rpn(const std::vector<Token>& rpn,
                                       const std::unordered_map<std::string, double>& variables);
    
    int precedence(const std::string& op) const;
    bool is_left_associative(const std::string& op) const;
    
    std::optional<double> evaluate_function(const std::string& name, double arg);
    std::optional<double> evaluate_function_2arg(const std::string& name, double arg1, double arg2);
    
    bool is_operator(char c) const;
    bool is_function(const std::string& name) const;
    bool is_digit(char c) const;
    bool is_alpha(char c) const;
    
    static const std::unordered_map<std::string, int> operator_precedence_;
    static const std::unordered_map<std::string, int> function_arg_counts_;
};

} // namespace fem