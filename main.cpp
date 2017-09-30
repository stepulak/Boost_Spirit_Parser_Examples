#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/variant/recursive_wrapper.hpp>
#include <boost/phoenix/core.hpp>

#include <string>
#include <iostream>
#include <vector>
#include <functional>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

#define BIND_VALUE(static_method) phoenix::bind(&static_method, qi::_1)

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

	// For spirit parser
	// I'm using Builder namespace for creating XML tree (more manual work, but I find it more fancy)
	struct Expression {
		std::string expression;

		Expression& operator=(const std::string& str) {
			expression = str;
			return *this;
		}
	};

	struct XMLNode {
		std::string name = {};
		bool isPaired = false;
		bool closing = false;
		std::string body = {};
		std::vector<std::pair<std::string, std::string>> keyValuePairs;
		std::unique_ptr<XMLNode> next;

		XMLNode& GetNext() {
			next = std::make_unique<XMLNode>();
			return *next;
		}
	};
	
	// We have to use functions for binding qi::_1 via phoenix::bind during parsing...
	namespace Builder {
		namespace {
			XMLNode _emptyNode;

			std::vector<std::string> tagStack;
			bool isValidXML;
			std::unique_ptr<XMLNode> xmlTree;
			std::reference_wrapper<XMLNode> currentNode(_emptyNode);

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
		} // namespace

		void Init() {
			tagStack.clear();
			isValidXML = true;
			xmlTree.reset(nullptr);
		}

		void Postprocessing() {
			if (!tagStack.empty()) {
				StackIsInvalid();
			}
		}

		void PushSingleTag(const std::string& tag) {
			//std::cout << "push single tag " << tag << std::endl;
			if (isValidXML) {
				auto& node = currentNode.get().GetNext();
				node.isPaired = false;
				node.name = tag;
				currentNode = std::ref(node);
			}
		}

		void PushPairTag(const std::string& tag) {
			//std::cout << "push pair tag " << tag << std::endl;
			if (isValidXML) {
				CheckAndCreateTree();
				auto& node = currentNode.get().GetNext();
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
					auto& node = currentNode.get().GetNext();
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
					node.keyValuePairs.push_back(std::pair<std::string, std::string>(paramName, {}));
				}
				else {
					StackIsInvalid();
				}
			}
		}

		void AddParameterValue(std::string value) {
			//std::cout << "add parameter value " << value << std::endl;
			// Is it empty? Then do not be
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

		bool IsValidXML() {
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

	} // namespace

	template<typename It>
	class Parser : public qi::grammar<It, Expression(), ascii::space_type>
	{
	private:

		qi::rule<It, Expression(), ascii::space_type> startRule;
		qi::rule<It, Expression(), ascii::space_type> tagRule;
		qi::rule<It, Expression(), ascii::space_type> paramRule;
		qi::rule<It, Expression(), ascii::space_type> singleTagRule;
		qi::rule<It, Expression(), ascii::space_type> pairTagRule;
		qi::rule<It, Expression(), ascii::space_type> bodyRule;

	public:

		Parser() : Parser::base_type(startRule)
		{
			Builder::Init();

			// Oh what a mess... AND STILL VALID C++ :(... (I would shot myself for this Perl like program)

			startRule =
				*(qi::lit("<?") >> +(qi::char_ - qi::char_("?>")) >> qi::lit("?>"))
				>> *(singleTagRule | pairTagRule);

			paramRule =
				qi::as_string[+qi::alnum][BIND_VALUE(Builder::AddParameterName)]
				>> qi::lit("=\"")
				>> qi::as_string[+qi::alnum][BIND_VALUE(Builder::AddParameterValue)]
				>> qi::lit("\"");

			singleTagRule =
				qi::char_('<')
				>> qi::as_string[qi::lexeme[+(qi::alnum - qi::char_(">"))] >> !qi::lit(">")][BIND_VALUE(Builder::PushSingleTag)]
				>> *paramRule
				>> qi::lit("/>");

			pairTagRule %=
				qi::lit("<")
				>> qi::as_string[qi::lexeme[+(qi::alnum - qi::char_(">")) >> qi::lit(">")]][BIND_VALUE(Builder::PushPairTag)]
				>> *(singleTagRule | bodyRule | pairTagRule)
				>> qi::lit("</")
				>> qi::as_string[qi::lexeme[+(qi::alnum - qi::char_('>')) >> qi::lit(">")]][BIND_VALUE(Builder::PopPairTag)];

			bodyRule = qi::as_string[!qi::lit("<") >> +qi::alnum >> !qi::lit(">")][BIND_VALUE(Builder::SetBody)];
		}
	};
}

BOOST_FUSION_ADAPT_STRUCT(XML::Expression, (std::string, expression));

template<typename It>
bool parse(It begin, It end)
{
	XML::Parser<It> parser;
	XML::Expression expression;
	bool ret = qi::phrase_parse(begin, end, parser, ascii::space, expression);
	XML::Builder::Postprocessing();
	return ret;
}

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

int main()
{
	std::string line;

	do {
		std::getline(std::cin, line);
		bool succ = parse(line.cbegin(), line.cend());

		if (succ && XML::Builder::IsValidXML()) {
			std::cout << "Parsing successful" << std::endl;
			auto tree = XML::Builder::GetXMLTree();
			PrintXMLTree(*tree);
		}
		else {
			std::cout << "Parsing failed" << std::endl;
		}
	} while (!line.empty());

	return 0;
}