#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>

#include <string>
#include <vector>
#include <functional>
#include <map>

/**
	Parser and executer of my primitive (not Turing complete) assembly-like language

	Example:

	create function_name(param1, param2,...) {
		create x,
		create y,
		setval x 5,
		setval y 10,
		add y y,
		mul x y,
		sub y x,
		div x y,
		setvar param1 x,
		setvar param2 y,
		print x
	}
*/

#define BIND_VALUE(member_method, bind_point) phoenix::bind(&member_method, &bind_point, qi::_1)

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

namespace L
{

class Function {
private:

    struct Command {
        std::string commandName;
        std::string param1;
        std::string param2;

        Command(const std::string& cmdName)
            : commandName(cmdName) {}
    };

    bool isValid = true;
    std::string name;
    std::vector<std::string> params;
    std::vector<Command> commands;

public:

    Function() = default;

    void SetName(const std::string& n) {
        //std::cout << "setting name " << n << std::endl;
        name = n;
    }

    void AddFunctionParameter(const std::string& p) {
        //std::cout << "adding function parameter " << p << std::endl;
        params.push_back(p);
    }

    void AddCommand(const std::string& c) {
        //std::cout << "adding command " << c << std::endl;
        commands.emplace_back(c);
    }

    void AddCommandParameter(const std::string& p) {
        //std::cout << "adding command parameter " << p << std::endl;
        if (!commands.empty()) {
            auto& b = commands.back();
            if (b.param1.empty()) {
                b.param1 = p;
            }
            else if (b.param2.empty()) {
                b.param2 = p;
            }
            else {
                isValid = false;
            }
        }
    }

    bool CheckValidity() {
        if (name.empty()) {
            isValid = false;
        }
        return isValid;
    }

    void Execute() const {
        if (!isValid) {
            return;
        }
        std::cout << "Executing:" << std::endl;

        std::map<std::string, int> variables;
        bool shouldQuit = false;

        auto checkVariable = [&](const std::string& s) -> bool {
            if (variables.find(s) == variables.end()) {
                std::cerr << "Error: Variable " << s << " does not exist" << std::endl;
                shouldQuit = true;
                return false;
            }
            return true;
        };

        // Initialize parameters as variables
        for (const auto& p : params) {
            variables[p] = 0;
        }

        for (const auto& c : commands) {
            if (shouldQuit) {
                return;
            }

            if (c.commandName == "create") {
                variables[c.param1] = 0;
            }
            else if (c.commandName == "setval") {
                if (checkVariable(c.param1)) {
                    variables[c.param1] = std::atoi(c.param2.c_str());
                }
            }
            else if (c.commandName == "setvar") {
                if (checkVariable(c.param1) && checkVariable(c.param2)) {
                    variables[c.param1] = variables[c.param2];
                }
            }
            else if (c.commandName == "print") {
                if (checkVariable(c.param1)) {
                    std::cout << c.param1 << " = " << variables[c.param1] << std::endl;
                }
            }
            else if (c.commandName == "add") {
                if (checkVariable(c.param1) && checkVariable(c.param2)) {
                    variables[c.param1] += variables[c.param2];
                }
            }
            else if (c.commandName == "sub") {
                if (checkVariable(c.param1) && checkVariable(c.param2)) {
                    variables[c.param1] -= variables[c.param2];
                }
            }
            else if (c.commandName == "mul") {
                if (checkVariable(c.param1) && checkVariable(c.param2)) {
                    variables[c.param1] *= variables[c.param2];
                }
            }
            else if (c.commandName == "div") {
                if (checkVariable(c.param1) && checkVariable(c.param2)) {
                    if (variables[c.param2] != 0) {
                        variables[c.param1] /= variables[c.param2];
                    }
                    else {
                        std::cerr << "Division by zero" << std::endl;
                        return;
                    }
                }
            }
        }

        std::cout << "Variables stats:" << std::endl;
        for (const auto& p : variables) {
            std::cout << p.first << " = " << p.second << std::endl;
        }
    }
};

template<typename It>
class FunctionParser : public qi::grammar<It, std::string(), ascii::space_type>
{
private:

    qi::rule<It, std::string(), ascii::space_type> startRule;
    qi::rule<It, std::string(), ascii::space_type> paramRule;
    qi::rule<It, std::string(), ascii::space_type> bodyRule;

    Function func;

public:

    FunctionParser() : FunctionParser::base_type(startRule)
    {
        startRule =
            qi::lit("create")
            >> qi::as_string[+(qi::alnum - qi::char_('('))][BIND_VALUE(Function::SetName, func)]
            >> qi::char_('(')
            >> *paramRule
            >> qi::char_(')')
            >> qi::char_('{')
            >> -bodyRule
            >> qi::char_('}');

        paramRule =
            qi::as_string[+(qi::alnum)][BIND_VALUE(Function::AddFunctionParameter, func)]
            >> -qi::char_(',');

        bodyRule =
            qi::as_string[qi::lexeme[+qi::alnum - qi::char_('}')]][BIND_VALUE(Function::AddCommand, func)]
            >> -qi::as_string[qi::lexeme[+qi::alnum - qi::char_('}')]][BIND_VALUE(Function::AddCommandParameter, func)]
            >> -qi::as_string[qi::lexeme[+qi::alnum - qi::char_('}')]][BIND_VALUE(Function::AddCommandParameter, func)]
            >> -(qi::char_(',') >> bodyRule);
    }

    Function& GetParsedFunction() {
        return func;
    }
};
}

template<typename It>
void ParseAndExecute(It first, It end)
{
    L::FunctionParser<It> parser;
    std::string s;
    bool succ = qi::phrase_parse(first, end, parser, ascii::space, s);
    auto& func = parser.GetParsedFunction();

    if (succ && func.CheckValidity()) {
	std::cout << "Parsing successful" << std::endl;
	func.Execute();
    }
    else {
	std::cout << "Parsing failed" << std::endl;
    }
}

int main() {
    std::string line;
    std::stringstream ss;
    
    do {
	std::getline(std::cin, line);
        ss << line;
    } while (!line.empty());
    
    auto str = ss.str();
    ParseAndExecute(str.cbegin(), str.cend());

    return 0;
}
