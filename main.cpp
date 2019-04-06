#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#include <functional>
#include <optional>
#include <iostream>
#include <vector>
#include <map>

namespace stdinext
{
struct ArgvView
{
    const int argc;
    char** const argv;
    int index = 1; // Skip argv[0]
};

struct Macro
{
    std::string keyword;
    std::string body;
    bool isAssignment = false;
};

struct BulitinEntry
{
    using BuiltinHandler = std::function<std::optional<std::string> (const Macro&)>;

    BuiltinHandler handler;
    std::string description;
};

std::optional<std::string> onEXIT(const Macro&);
std::optional<std::string> onEOF(const Macro&);
std::optional<std::string> onDEL(const Macro&);
std::optional<std::string> onHELP(const Macro&);
std::optional<std::string> onEVAL(const Macro&);
std::optional<std::string> onCERR(const Macro&);
std::optional<std::string> onCOUT(const Macro&);
template<typename T> void parseLineUntilEOF(T&);

static std::map<std::string, std::string>           userDefinedMarcos;
static const std::map<std::string, BulitinEntry>    builtinMarcos = {
    {"EXIT", {&onEXIT, "Exit the process"}},
    {"EOF",  {&onEOF,  "Raise EOF signal. Stop the current stream and start another"}},
    {"DEL",  {&onDEL,  "Remove one user-defined marco"}},
    {"HELP", {&onHELP, "Print help"}},
    {"EVAL", {&onEVAL, "Eval"}},
    {"CERR", {&onCERR, "Output to cerr"}},
    {"COUT", {&onCOUT, "Output to cout"}}
};
static bool toCout = true;

template<bool WITH_COLOR = true, typename... Args>
void printToCerr(const Args&... args)
{
    static constexpr auto RED = "\033[1;31m";
    static constexpr auto RESET = "\033[1;0m";

    if constexpr (WITH_COLOR)
    {
        std::cerr << RED;
    }

    ((std::cerr << args), ...);

    if constexpr (WITH_COLOR)
    {
        std::cerr << RESET;
    }

    std::cerr << "\n";
}

template<bool WITH_COLOR = true>
void printCommand()
{
    printToCerr<WITH_COLOR>("Builtin macro: (Not overwritable, nor removable)");
    for(const auto& item : builtinMarcos)
    {
        printToCerr<WITH_COLOR>(" #",item.first, "\t\t", item.second.description);
    }

    printToCerr<WITH_COLOR>("\nUser-defined marco:");
    for(const auto& item : userDefinedMarcos)
    {
        printToCerr<WITH_COLOR>(" #", item.first, "=", item.second);
    }

    printToCerr<WITH_COLOR>(R"(
Add a user-defined macro:
#{CMD}={text text2...}
 - Support the format from boost::format %% %1% %2%, etc
 - If the output is started with #, another round of evaluation will be done. (Max 99 times).

Calling a macro:
#{CMD} {arg1 arg2...})");
}

void printProcesstHelp()
{
     printToCerr<false>(R"(Allow you to pipe in from stdin "and then" read from keyboard. A simple script engine is also included)");
     printCommand<false>();
}

std::optional<std::string> onEXIT(const Macro&)
{
    exit(0);
    return {};
}

std::optional<std::string> onEOF(const Macro&)
{
    return {};
}

std::optional<std::string> onDEL(const Macro& macro)
{
    if (builtinMarcos.count(macro.body))
    {
        printToCerr("Bulitin macro cannot be removed: ", macro.body);
    }
    else
    {
        userDefinedMarcos.erase(macro.body);
    }
    return "";
}

std::optional<std::string> onHELP(const Macro&)
{
    printCommand();
    return "";
}

std::optional<std::string> onEVAL(const Macro& macro)
{
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(macro.body.c_str(), "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }

    FILE* fd = pipe.get();
    parseLineUntilEOF(fd);
    return "";
}

std::optional<std::string> onCERR(const Macro&)
{
    toCout = false;
    return "";
}

std::optional<std::string> onCOUT(const Macro&)
{
    toCout = true;
    return "";
}

template<typename T, typename It>
void applyFormat(T& item, It cur, It end)
{
    if (cur != end)
    {
        applyFormat(item % *cur, cur + 1, end);
    }
}

std::string applyArguments(const std::string& format, const std::string& args)
{
    std::vector<std::string> arr;
    boost::split(arr, args, boost::is_space(), boost::token_compress_on);

    auto item = boost::format(format);
    item.exceptions(boost::io::all_error_bits ^ (boost::io::too_many_args_bit | boost::io::too_few_args_bit));
    applyFormat(item, arr.begin(), arr.end());

    return item.str();
}

std::optional<std::string> processMacro(const Macro& macro)
{
    if (macro.keyword.empty())
    {
        return "";
    }
    else if (macro.isAssignment)
    {
        if (builtinMarcos.count(macro.keyword))
        {
            printToCerr("Bulitin macro cannot be overwritten: ", macro.keyword);
        }
        else
        {
            userDefinedMarcos[macro.keyword] = macro.body;
        }
        return "";
    }
    else
    {
        if (auto it = builtinMarcos.find(macro.keyword); it != builtinMarcos.end())
        {
            return it->second.handler(macro);
        }
        else
        {
            if (auto it2 = userDefinedMarcos.find(macro.keyword); it2 != userDefinedMarcos.end())
            {
                return applyArguments(it2->second, macro.body);
            }
            else
            {
                printToCerr("The macro ", macro.keyword, " is not defined");
                return "";
            }
        }
    }
}

std::optional<Macro> parseMacro(const std::string& line)
{
    if (line[0] == '#')
    {
        if (line.size() < 2 || line[1] == '!' || line[1] == '#')
        // too short                      #!                ##
        {
            return Macro{};
        }
        else
        {
            const auto pos = line.find_first_of(" =");

            std::string keyword = line.substr(1, pos - 1);
            std::string body = (pos == std::string::npos || pos == line.size()) ? "" : line.substr(pos + 1);

            return Macro{keyword, body, line[pos] == '='};
        }
    }
    else
    {
        return {};
    }
}

std::optional<std::string> evaluate(const std::string& line, int stackLevel = 0)
{
    if (stackLevel >= 99)
    {
        printToCerr("Stack overflow");
        return line;
    }
    else if (auto result = parseMacro(line); result)
    {
        auto output = processMacro(*result);
        if (output && (*output)[0] == '#')
        {
            return evaluate(*output, stackLevel+1);
        }
        return output;
    }
    else
    {
        return line;
    }
}

void output(const std::string& str)
{
    if (!str.empty())
    {
        if (toCout)
        {
            std::cout << str << std::endl;
        }
        else
        {
            printToCerr(str);
        }
    }
}

bool getline(ArgvView& view, std::string& input)
{
    if (view.index < view.argc)
    {
        input = view.argv[view.index++];
        return true;
    }
    return false;
}

bool getline(FILE* fp, std::string& input)
{
    std::vector<char> buf;

    int c;
    while ((c = fgetc(fp)) != EOF && c != '\n')
    {
        buf.push_back(static_cast<char>(c));
    }

    if (buf.size())
    {
        buf.push_back('\0');
        input = &*buf.begin();
        return true;
    }
    else
    {
        input = "";
    }

    return (c != EOF);
}

template<typename T>
void parseLineUntilEOF(T& in)
{
    const auto getlineWithColor = [](T& in, std::string& out)
    {
        static constexpr auto GREEN = "\033[1;32m";
        static constexpr auto RESET = "\033[1;0m";
        using std::getline;
        using stdinext::getline;
        std::cerr << GREEN;
        const bool ret = getline(in, out);
        std::cerr << RESET;
        return ret;
    };

    std::string line;
    while (getlineWithColor(in, line))
    {
        auto result = evaluate(line);
        if (result)
        {
            output(*result);
        }
        else
        {
            break;
        }
    }
}
} // namespace stdinext

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        if (std::string("--help") == argv[1] ||
            std::string("-h")     == argv[1])
        {
            stdinext::printProcesstHelp();
            exit(0);
        }
    }
    {
        stdinext::ArgvView argument{argc, argv};
        stdinext::parseLineUntilEOF(argument);
    }
    {
        FILE* fp = fdopen(dup(fileno(stdin)), "r");
        stdinext::parseLineUntilEOF(fp);
        (void) freopen("/dev/tty", "r", stdin);
    }

    while(true)
    {
        stdinext::parseLineUntilEOF(std::cin);
    }
    return 0;
}
