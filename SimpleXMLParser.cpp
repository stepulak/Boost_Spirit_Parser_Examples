#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/variant/recursive_wrapper.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>

#include <string>
#include <iostream>
#include <vector>
#include <functional>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

#define BIND_VALUE(method, bind_point) phoenix::bind(&method, &bind_point, qi::_1)

/// primitive XML parser for parsing simple XML files, eg.:
/// <?info?>
/// <tag>
///   <tag2>
///     text
///     <tag3 key="value" key2="value2"/>
///   </tag2>
///   <tag4 key="value"/>
/// </tag>
namespace XML {
	struct XMLNode {
		bool isPaired = false;
		bool closing = false;
		std::string name = {};
		std::string body = {};
		std::vector<std::pair<std::string, std::string>> keyValuePairs;
		std::unique_ptr<XMLNode> next;

		XMLNode& CreateNext() {
			next = std::make_unique<XMLNode>();
			return *next;
		}
	};

	XMLNode _emptyNode;

	class Builder {
	private:

		bool isValidXML = true;
		std::vector<std::string> tagStack;
		std::unique_ptr<XMLNode> xmlTree;
		std::reference_wrapper<XMLNode> currentNode = std::ref(_emptyNode);

		void CheckAndCreateTree() {
			if (xmlTree == nullptr) {
				xmlTree = std::make_unique<XMLNode>();
				currentNode = std::ref(*xmlTree);
			}
		}

		void StackIsInvalid() {
			isValidXML = false;
			std::cerr << "Invalid XML" << std::endl;
		}

	public:

		Builder() = default;

		void Postprocessing() {
			if (!tagStack.empty()) {
				StackIsInvalid();
			}
		}

		void PushSingleTag(const std::string& tag) {
			//std::cout << "push single tag " << tag << std::endl;
			if (isValidXML) {
				auto& node = currentNode.get().CreateNext();
				node.isPaired = false;
				node.name = tag;
				currentNode = std::ref(node);
			}
		}

		void PushPairTag(const std::string& tag) {
			//std::cout << "push pair tag " << tag << std::endl;
			if (isValidXML) {
				CheckAndCreateTree();
				auto& node = currentNode.get().CreateNext();
				node.isPaired = true;
				node.closing = false;
				node.name = tag;
				currentNode = std::ref(node);
				tagStack.push_back(tag);
			}
		}

		void PopPairTag(const std::string& tag) {
			//std::cout << "pop pair tag " << tag << std::endl;
			if (isValidXML) {
				if (tagStack.back() == tag) {
					auto& node = currentNode.get().CreateNext();
					node.isPaired = true;
					node.closing = true;
					node.name = tag;
					currentNode = std::ref(node);
					tagStack.pop_back();
				}
				else {
					StackIsInvalid();
				}
			}
		}

		void SetBody(const std::string& body) {
			//std::cout << "set body " << body << std::endl;
			if (isValidXML) {
				if (xmlTree == nullptr) {
					StackIsInvalid();
					return;
				}
				auto& node = currentNode.get();
				if (node.isPaired) {
					node.body = body;
				}
				else {
					StackIsInvalid();
				}
			}
		}

		void AddParameterName(const std::string& paramName) {
			//std::cout << "add parameter name " << paramName << std::endl;
			if (isValidXML) {
				if (xmlTree == nullptr) {
					StackIsInvalid();
					return;
				}
				auto& node = currentNode.get();
				if (node.isPaired == false) {
					node.keyValuePairs.emplace_back(paramName, std::string());
				}
				else {
					StackIsInvalid();
				}
			}
		}

		void AddParameterValue(std::string value) {
			//std::cout << "add parameter value " << value << std::endl;
			// Are you empty? Then do not be
			if (value.empty()) {
				value = " ";
			}
			if (isValidXML) {
				if (xmlTree == nullptr) {
					StackIsInvalid();
					return;
				}
				auto& node = currentNode.get();
				if (node.isPaired == false && node.keyValuePairs.size() > 0 && node.keyValuePairs.back().second.empty()) {
					node.keyValuePairs.back().second = std::move(value);
				}
				else {
					StackIsInvalid();
				}
			}
		}

		bool IsValidXML() const {
			return isValidXML && xmlTree != nullptr && xmlTree->next != nullptr;
		}

		std::unique_ptr<XMLNode> GetXMLTree() {
			if (IsValidXML()) {
				auto tree = std::move(xmlTree->next);
				xmlTree.reset(nullptr);
				return std::move(tree);
			}
			return nullptr;
		}
	};

	template<typename It>
	class Parser : public qi::grammar<It, std::string(), ascii::space_type>
	{
	private:

		qi::rule<It, std::string(), ascii::space_type> startRule;
		qi::rule<It, std::string(), ascii::space_type> paramRule;
		qi::rule<It, std::string(), ascii::space_type> singleTagRule;
		qi::rule<It, std::string(), ascii::space_type> pairTagRule;
		qi::rule<It, std::string(), ascii::space_type> bodyRule;

		Builder builder;

	public:

		Parser() : Parser::base_type(startRule)
		{
			// Oh what a mess... AND STILL VALID C++ :(... (I would shot myself for this Perl like program)

			startRule =
				*(qi::lit("<?") >> +(qi::char_ - qi::char_("?>")) >> qi::lit("?>"))
				>> *(singleTagRule | pairTagRule);

			paramRule =
				qi::as_string[+qi::alnum][BIND_VALUE(Builder::AddParameterName, builder)]
				>> qi::lit("=\"")
				>> qi::as_string[+qi::alnum][BIND_VALUE(Builder::AddParameterValue, builder)]
				>> qi::lit("\"");

			singleTagRule =
				qi::char_('<')
				>> qi::as_string[qi::lexeme[+(qi::alnum - qi::char_(">"))] >> !qi::lit(">")][BIND_VALUE(Builder::PushSingleTag, builder)]
				>> *paramRule
				>> qi::lit("/>");

			pairTagRule =
				qi::lit("<")
				>> qi::as_string[qi::lexeme[+(qi::alnum - qi::char_(">")) >> qi::lit(">")]][BIND_VALUE(Builder::PushPairTag, builder)]
				>> *(singleTagRule | bodyRule | pairTagRule)
				>> qi::lit("</")
				>> qi::as_string[qi::lexeme[+(qi::alnum - qi::char_('>')) >> qi::lit(">")]][BIND_VALUE(Builder::PopPairTag, builder)];

			bodyRule = qi::as_string[!qi::lit("<") >> +qi::alnum >> !qi::lit(">")][BIND_VALUE(Builder::SetBody, builder)];
		}

		Builder& GetBuilder() {
			return builder;
		}
	};


	void PrintXMLTree(const XML::XMLNode& node, int lvl = 0) {
		if (node.isPaired && node.closing) {
			lvl = lvl - 1;
		}

		static auto printSpaces = [](int l) {
			for (; l > 0; l--) {
				std::cout << ' ';
			}
		};

		printSpaces(lvl);

		if (node.isPaired) {
			if (!node.closing) {
				std::cout << '<' << node.name << '>' << std::endl;
				if (!node.body.empty()) {
					printSpaces(lvl + 1);
					std::cout << node.body << std::endl;
				}
				if (node.next != nullptr) {
					PrintXMLTree(*node.next, lvl + 1);
				}
			}
			else {
				std::cout << "</" << node.name << '>' << std::endl;
				if (node.next != nullptr) {
					PrintXMLTree(*node.next, lvl);
				}
			}
		}
		else {
			std::cout << '<' << node.name;
			for (const auto& p : node.keyValuePairs) {
				std::cout << ' ' << p.first << "=\"" << p.second << "\"";
			}
			std::cout << "/>" << std::endl;
			if (node.next != nullptr) {
				PrintXMLTree(*node.next, lvl);
			}
		}
	}
}

template<typename It>
void ParseAndPrint(It begin, It end)
{
	XML::Parser<It> parser;
	bool succ = qi::phrase_parse(begin, end, parser, ascii::space, std::string());
	auto& builder = parser.GetBuilder();

	if (succ && builder.IsValidXML()) {
		std::cout << "Parsing successful" << std::endl;
		auto tree = builder.GetXMLTree();
		XML::PrintXMLTree(*tree);
	}
	else {
		std::cout << "Parsing failed" << std::endl;
	}
}

int main()
{
	std::string line;

	do {
		std::getline(std::cin, line);
		ParseAndPrint(line.cbegin(), line.cend());
	} while (!line.empty());

	return 0;
}
