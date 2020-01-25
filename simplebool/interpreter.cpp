#include <iostream>
#include <sstream>
#include <stack>
#include <vector>

namespace lexer {
struct Token {
    enum class Category {
        VARIABLE,
        LAMBDA,
        LAMBDA_DOT,
        OPEN_PAREN,
        CLOSE_PAREN,
        COLON,
        ARROW,

        KEYWORD_BOOL,

        MARKER_END,
        MARKER_INVALID
    };

    Category category = Category::MARKER_INVALID;
    std::string text;
};

std::ostream& operator<<(std::ostream& out, Token token);

class Lexer {
    static const std::string kLambdaInputSymbol;
    static const std::string kKeywordBool;

   public:
    Lexer(std::istringstream&& in) : in_(std::move(in)) {}

    Token NextToken() {
        if (is_cached_token_valid) {
            if (cached_token.category == Token::Category::MARKER_INVALID) {
                throw std::invalid_argument("Error: invalid token: " +
                                            cached_token.text);
            }

            is_cached_token_valid = false;
            return cached_token;
        }

        Token token;
        char next_char = 0;
        std::ostringstream token_text_out;
        while (in_.get(next_char) && !IsSeparator(next_char)) {
            token_text_out << next_char;
        }

        auto token_text = token_text_out.str();

        if (token_text == kLambdaInputSymbol) {
            token.category = Token::Category::LAMBDA;
        } else if (token_text == kKeywordBool) {
            token.category = Token::Category::KEYWORD_BOOL;
        } else if (!token_text.empty()) {
            token.category = Token::Category::VARIABLE;
            token.text = token_text;
        } else if (next_char == '(') {
            // There is no token before next_char, then create a token of it and
            // clear next_char.
            token.category = Token::Category::OPEN_PAREN;
            next_char = 0;
        } else if (next_char == ')') {
            // There is no token before next_char, then create a token of it and
            // clear next_char.
            token.category = Token::Category::CLOSE_PAREN;
            next_char = 0;
        } else if (next_char == '.') {
            // There is no token before next_char, then create a token of it and
            // clear next_char.
            token.category = Token::Category::LAMBDA_DOT;
            next_char = 0;
        } else if (next_char == ':') {
            // There is no token before next_char, then create a token of it and
            // clear next_char.
            token.category = Token::Category::COLON;
            next_char = 0;
        } else if (next_char == '-') {
            if (in_.get(next_char) && next_char == '>') {
                token.category = Token::Category::ARROW;
            } else {
                token.text = '-';
                token.category = Token::Category::MARKER_INVALID;
            }

            next_char = 0;
        } else if (!in_) {
            token.category = Token::Category::MARKER_END;
        } else {
            // Must be whitespace, eat it.
            token = NextToken();
        }

        // There is a token before next_char, then return that token and cache
        // next_char's token for next call to NextToken().
        if (next_char == '.') {
            is_cached_token_valid = true;
            cached_token.category = Token::Category::LAMBDA_DOT;
        } else if (next_char == '(') {
            is_cached_token_valid = true;
            cached_token.category = Token::Category::OPEN_PAREN;
        } else if (next_char == ')') {
            is_cached_token_valid = true;
            cached_token.category = Token::Category::CLOSE_PAREN;
        } else if (next_char == ':') {
            is_cached_token_valid = true;
            cached_token.category = Token::Category::COLON;
        } else if (next_char == '-') {
            is_cached_token_valid = true;

            if (in_.get(next_char) && next_char == '>') {
                cached_token.category = Token::Category::ARROW;
            } else {
                cached_token.text = '-';
                cached_token.category = Token::Category::MARKER_INVALID;
            }
        }

        if (token.category == Token::Category::MARKER_INVALID) {
            throw std::invalid_argument("Error: invalid token: " + token.text);
        }

        return token;
    }

   private:
    bool IsSeparator(char c) {
        return std::string{" .():-"}.find(c) != std::string::npos;
    }

   private:
    std::istringstream in_;
    bool is_cached_token_valid = false;
    Token cached_token;
};

const std::string Lexer::kLambdaInputSymbol = "l";
const std::string Lexer::kKeywordBool = "Bool";

std::ostream& operator<<(std::ostream& out, Token token) {
    switch (token.category) {
        case Token::Category::LAMBDA:
            out << "λ";
            break;
        case Token::Category::KEYWORD_BOOL:
            out << "Ɓ";
            break;
        case Token::Category::VARIABLE:
            out << token.text;
            break;
        case Token::Category::LAMBDA_DOT:
            out << ".";
            break;
        case Token::Category::OPEN_PAREN:
            out << "(";
            break;
        case Token::Category::CLOSE_PAREN:
            out << ")";
            break;
        case Token::Category::COLON:
            out << ":";
            break;
        case Token::Category::ARROW:
            out << "→";
            break;
        case Token::Category::MARKER_END:
            out << "<END>";
            break;

        case Token::Category::MARKER_INVALID:
            out << "<INVALID>";
            break;

        default:
            out << "<ILLEGAL_TOKEN>";
    }

    return out;
}
}  // namespace lexer

namespace parser {

class Term {
    friend std::ostream& operator<<(std::ostream&, const Term&);

   public:
    static Term Lambda(std::string arg_name) {
        Term result;
        result.lambda_arg_name_ = arg_name;
        result.is_lambda_ = true;

        return result;
    }

    static Term Variable(std::string var_name, int de_bruijn_idx) {
        Term result;
        result.variable_name_ = var_name;
        result.de_bruijn_idx_ = de_bruijn_idx;
        result.is_variable_ = true;

        return result;
    }

    static Term Application(std::unique_ptr<Term> lhs,
                            std::unique_ptr<Term> rhs) {
        Term result;
        result.is_application_ = true;
        result.application_lhs_ = std::move(lhs);
        result.application_rhs_ = std::move(rhs);

        return result;
    }

    Term() = default;

    Term(const Term&) = delete;
    Term& operator=(const Term&) = delete;

    Term(Term&&) = default;
    Term& operator=(Term&&) = default;

    bool IsLambda() const { return is_lambda_; }

    bool IsVariable() const { return is_variable_; }

    bool IsApplication() const { return is_application_; }

    bool IsInvalid() const {
        if (IsLambda()) {
            return lambda_arg_name_.empty() || !lambda_body_;
        } else if (IsVariable()) {
            return variable_name_.empty();
        } else if (IsApplication()) {
            return !application_lhs_ || !application_rhs_;
        }

        return true;
    }

    void Combine(Term&& term) {
        if (term.IsInvalid()) {
            throw std::invalid_argument(
                "Term::Combine() received an invalid Term.");
        }

        if (IsLambda()) {
            if (lambda_body_) {
                if (is_complete_lambda_) {
                    *this =
                        Application(std::make_unique<Term>(std::move(*this)),
                                    std::make_unique<Term>(std::move(term)));

                    is_lambda_ = false;
                    lambda_body_ = nullptr;
                    lambda_arg_name_ = "";
                    is_complete_lambda_ = false;
                } else {
                    lambda_body_->Combine(std::move(term));
                }
            } else {
                lambda_body_ = std::make_unique<Term>(std::move(term));
            }
        } else if (IsVariable()) {
            *this = Application(std::make_unique<Term>(std::move(*this)),
                                std::make_unique<Term>(std::move(term)));

            is_variable_ = false;
            variable_name_ = "";
        } else if (IsApplication()) {
            *this = Application(std::make_unique<Term>(std::move(*this)),
                                std::make_unique<Term>(std::move(term)));
        } else {
            *this = std::move(term);
        }
    }

    /*
     * Shifts the de Bruijn indices of all free variables inside this Term up by
     * distance amount. For an example use, see Term::Substitute(int, Term&).
     */
    void Shift(int distance) {
        std::function<void(int, Term&)> walk = [&distance, &walk](
                                                   int binding_context_size,
                                                   Term& term) {
            if (term.IsVariable()) {
                if (term.de_bruijn_idx_ >= binding_context_size) {
                    term.de_bruijn_idx_ += distance;
                }
            } else if (term.IsLambda()) {
                walk(binding_context_size + 1, *term.lambda_body_);
            } else if (term.IsApplication()) {
                walk(binding_context_size, *term.application_lhs_);
                walk(binding_context_size, *term.application_rhs_);
            } else {
                throw std::invalid_argument("Trying to shift an invalid term.");
            }
        };

        walk(0, *this);
    }

    /**
     * Substitutes variable (that is, the de Brijun idex of a variable) with the
     * term sub.
     */
    void Substitute(int variable, Term& sub) {
        if (IsInvalid() || sub.IsInvalid()) {
            throw std::invalid_argument(
                "Trying to substitute using invalid terms.");
        }

        std::function<void(int, Term&)> walk = [&variable, &sub, &walk](
                                                   int binding_context_size,
                                                   Term& term) {
            if (term.IsVariable()) {
                // Adjust variable according to the current binding
                // depth before comparing term's index.
                if (term.de_bruijn_idx_ == variable + binding_context_size) {
                    // Shift sub up by binding_context_size distance since sub
                    // is now substituted in binding_context_size deep context.
                    sub.Shift(binding_context_size);
                    std::swap(term, sub);
                }
            } else if (term.IsLambda()) {
                walk(binding_context_size + 1, *term.lambda_body_);
            } else if (term.IsApplication()) {
                walk(binding_context_size, *term.application_lhs_);
                walk(binding_context_size, *term.application_rhs_);
            }
        };

        walk(0, *this);
    }

    Term& LambdaBody() {
        if (!IsLambda()) {
            throw std::invalid_argument("Invalid Lambda term.");
        }

        return *lambda_body_;
    }

    Term& ApplicationLHS() {
        if (!IsApplication()) {
            throw std::invalid_argument("Invalide application term.");
        }

        return *application_lhs_;
    }

    Term& ApplicationRHS() {
        if (!IsApplication()) {
            throw std::invalid_argument("Invalide application term.");
        }

        return *application_rhs_;
    }

    bool is_complete_lambda_ = false;

   private:
    bool is_lambda_ = false;
    std::string lambda_arg_name_ = "";
    std::unique_ptr<Term> lambda_body_{};

    bool is_variable_ = false;
    std::string variable_name_ = "";
    int de_bruijn_idx_ = -1;

    bool is_application_ = false;
    std::unique_ptr<Term> application_lhs_{};
    std::unique_ptr<Term> application_rhs_{};
};

std::ostream& operator<<(std::ostream& out, const Term& term) {
    if (term.IsVariable()) {
        out << "[" << term.variable_name_ << "=" << term.de_bruijn_idx_ << "]";
    } else if (term.IsLambda()) {
        out << "{λ " << term.lambda_arg_name_ << ". " << *term.lambda_body_
            << "}";
    } else if (term.IsApplication()) {
        out << "(" << *term.application_lhs_ << " <- " << *term.application_rhs_
            << ")";
    } else {
        out << "<ERROR>";
    }

    return out;
}

class Parser {
    using Token = lexer::Token;

   public:
    Parser(std::istringstream&& in) : lexer_(std::move(in)) {}

    Term ParseProgram() {
        Token next_token;
        std::stack<Term> term_stack;
        term_stack.emplace(Term());
        int balance_parens = 0;
        // Contains a list of bound variables in order of binding. For example,
        // for a term λ x. λ y. x y, this list would eventually contains {"x" ,
        // "y"} in that order. This is used to assign de Bruijn indices/static
        // distances to bound variables (ref: tapl,§6.1).
        std::vector<std::string> bound_variables;

        while ((next_token = lexer_.NextToken()).category !=
               Token::Category::MARKER_END) {
            if (next_token.category == Token::Category::LAMBDA) {
                auto lambda_arg = ParseVariable();
                bound_variables.push_back(lambda_arg.text);
                ParseColon();
                ParseType();
                term_stack.emplace(Term::Lambda(lambda_arg.text));
            } else if (next_token.category == Token::Category::VARIABLE) {
                auto bound_variable_it =
                    std::find(std::begin(bound_variables),
                              std::end(bound_variables), next_token.text);
                int de_bruijn_idx = -1;

                if (bound_variable_it != std::end(bound_variables)) {
                    de_bruijn_idx = std::distance(bound_variable_it,
                                                  std::end(bound_variables)) -
                                    1;
                } else {
                    // The naming context for free variables (ref: tapl,§6.1.2)
                    // is chosen to be the ASCII code of a variable's name.
                    //
                    // NOTE: Only single-character variable names are currecntly
                    // supported as free variables.
                    de_bruijn_idx = bound_variables.size() +
                                    (std::tolower(next_token.text[0]) - 'a');
                }

                term_stack.top().Combine(
                    Term::Variable(next_token.text, de_bruijn_idx));
            } else if (next_token.category == Token::Category::OPEN_PAREN) {
                term_stack.emplace(Term());
                ++balance_parens;
            } else if (next_token.category == Token::Category::CLOSE_PAREN) {
                CombineStackTop(term_stack);

                // A prenthesized λ-abstration is equivalent to a double
                // parenthesized term since we push a new term on the stack for
                // each lambda.
                if (term_stack.top().IsLambda()) {
                    // Mark the λ as complete so that terms to its right won't
                    // be combined to its body.
                    term_stack.top().is_complete_lambda_ = true;
                    // λ's variable is no longer part of the current binding
                    // context, therefore pop it.
                    bound_variables.pop_back();
                    CombineStackTop(term_stack);
                }

                --balance_parens;
            } else {
                throw std::invalid_argument(
                    "Unexpected token: " +
                    (std::ostringstream() << next_token).str());
            }
        }

        if (balance_parens != 0) {
            throw std::invalid_argument(
                "Invalid term: probably because a ( is not matched by a )");
        }

        while (term_stack.size() > 1) {
            CombineStackTop(term_stack);
        }

        return std::move(term_stack.top());
    }

    void CombineStackTop(std::stack<Term>& term_stack) {
        if (term_stack.size() < 2) {
            throw std::invalid_argument(
                "Invalid term: probably because a ( is not matched by a )");
        }

        Term top = std::move(term_stack.top());
        term_stack.pop();
        term_stack.top().Combine(std::move(top));
    }

    Token ParseVariable() {
        auto token = lexer_.NextToken();

        return (token.category == Token::Category::VARIABLE)
                   ? token
                   : throw std::logic_error("Expected to parse a variable.");
    }

    std::vector<Token> ParseType() {
        std::vector<Token> type;

        while (true) {
            Token next = lexer_.NextToken();
            (next.category == Token::Category::KEYWORD_BOOL)
                ? type.push_back(next)
                : throw std::logic_error("Expected to parse Bool.");

            next = lexer_.NextToken();

            // So far, types can only exist in abstraction definitions and is
            // always expected to be followed by a dot. Therefore, ther is no
            // need to implement buffering or the ability to look ahead tokens
            // from the lexer.
            if (next.category == Token::Category::LAMBDA_DOT) {
                // type.push_back(next);
                break;
            } else {
                (next.category == Token::Category::ARROW)
                    ? type.push_back(next)
                    : throw std::logic_error("Expected to parse an arrow.");
            }
        }

        return type;
    }

    Token ParseColon() {
        auto token = lexer_.NextToken();

        return (token.category == Token::Category::COLON)
                   ? token
                   : throw std::logic_error("Expected to parse a dot.");
    }

    Token ParseDot() {
        auto token = lexer_.NextToken();

        return (token.category == Token::Category::LAMBDA_DOT)
                   ? token
                   : throw std::logic_error("Expected to parse a dot.");
    }

    Token ParseCloseParen() {
        auto token = lexer_.NextToken();

        return (token.category == Token::Category::CLOSE_PAREN)
                   ? token
                   : throw std::logic_error("Expected to parse a ')'.");
    }

   private:
    lexer::Lexer lexer_;
};
}  // namespace parser

namespace interpreter {
class Interpreter {
    using Term = parser::Term;

   public:
    void Interpret(Term& program) { Eval(program); }

    void Eval(Term& term) {
        try {
            Eval1(term);
            Eval(term);
        } catch (std::invalid_argument&) {
        }
    }

    void Eval1(Term& term) {
        auto term_subst_top = [](Term& s, Term& t) {
            // Adjust the free variables in s by increasing their static
            // distances by 1. That's because s will now be embedded one lever
            // deeper in t (i.e. t's bound variable will be replaced by s).
            s.Shift(1);
            t.Substitute(0, s);
            // Because of the substitution, one level of abstraction was peeled
            // off. Account for that by decreasing the static distances of the
            // free variables in t by 1.
            t.Shift(-1);
            // NOTE: For more details see: tapl,§6.3.
        };

        if (term.IsApplication() && term.ApplicationLHS().IsLambda() &&
            IsValue(term.ApplicationRHS())) {
            term_subst_top(term.ApplicationRHS(),
                           term.ApplicationLHS().LambdaBody());
            std::swap(term, term.ApplicationLHS().LambdaBody());
        } else if (term.IsApplication() && IsValue(term.ApplicationLHS())) {
            Eval1(term.ApplicationRHS());
        } else if (term.IsApplication()) {
            Eval1(term.ApplicationLHS());
        } else {
            throw std::invalid_argument("No applicable rule.");
        }
    }

    bool IsValue(const Term& term) { return term.IsLambda(); }
};
}  // namespace interpreter

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr
            << "Error: expected input program as a command line argument.\n";
        return 1;
    }

    lexer::Lexer lexer{std::istringstream{argv[1]}};

    for (auto token = lexer.NextToken();
         token.category != lexer::Token::Category::MARKER_END;
         token = lexer.NextToken()) {
        std::cout << token << " ";
    }

    std::cout << "\n";

    // parser::Parser parser{std::istringstream{argv[1]}};
    // auto program = parser.ParseProgram();

    // std::cout << "   " << program << "\n";

    // interpreter::Interpreter interpreter;
    // interpreter.Interpret(program);

    // std::cout << "=> " << program << "\n";

    return 0;
}