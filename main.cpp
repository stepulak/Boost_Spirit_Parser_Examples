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
		create x
		create y
		setval x 5
		setval y 10
		add y y
		mul x y
		sub y x
		div x y
		setvar param1 x
		setvar param2 y
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
		std::vector<std::string> param;
		std::vector<Command> commands;

	public:

		Function() = default;

		void SetName(const std::string& n) {
			//std::cout << "setting name " << n << std::endl;
			name = n;
		}

		void AddFunctionParameter(const std::string& p) {
			//std::cout << "adding function parameter " << p << std::endl;
			param.push_back(p);
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
				>> '('
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
void ParseAndRun(It first, It end)
{
	L::FunctionParser<It> parser;
	bool succ = qi::phrase_parse(first, end, parser, ascii::space, std::string());
	auto& func = parser.GetParsedFunction();

	// TODO check first iterator
	// TODO replace first iterator with begin
	if (succ && func.CheckValidity()) {
		std::cout << "Parsing successful" << std::endl;
	}
	else {
		std::cout << "Parsing failed" << std::endl;
	}
}

int main() {
	std::string line;

	do {
		std::getline(std::cin, line);
		ParseAndRun(line.cbegin(), line.cend());
	} while (!line.empty());

	return 0;
}