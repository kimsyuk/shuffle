#include "workspace/workspace.h"

#include <iostream>
#include <utility>
#include <map>

#include "console.h"
#include "utils.h"
#include "suggestion.h"
#include "workspace/snippetmgr.h"
#include "parsedcmd.h"
#include "cmdparser.h"

using std::cout, std::endl, std::cin, std::stringstream;

map<string, Workspace *> wsMap;

path Workspace::currentDirectory() {
    return dir;
}

void Workspace::moveDirectory(path newDir) {
    if (!is_directory(newDir)) {
        error("Directory '$0' not found.", {newDir.string()});
        return;
    }
    dir = std::move(newDir);

    if (dir.string()[dir.string().length() - 1] == '\\' || dir.string()[dir.string().length() - 1] == '/')
        dir = dir.parent_path();
}

vector<string> Workspace::getHistory() {
    return history;
}

void Workspace::addHistory(const string&s) {
    history.push_back(s);
    historyIndex = static_cast<int>(history.size());
}

string Workspace::historyUp() {
    if (history.empty()) return "";

    if (0 > historyIndex - 1) return history[historyIndex];
    return history[--historyIndex];
}

string Workspace::historyDown() {
    if (history.empty()) return "";

    if (history.size() <= historyIndex + 1) return history[history.size() - 1];
    return history[++historyIndex];
}

ParsedCommand Workspace::parse(const string&input) {
    if (std::smatch matches; regex_match(input, matches, regex("(\\w*)\\s*=\\s*(\"([^\"]*)\"|([\\s\\S]+)*)"))) {
        const string name = matches[1].str();
        string value;
        if (matches[2].matched) {
            value = matches[2].str();
        }
        else {
            value = matches[3].str();
        }
        if (value[0] == '$') {
            variables[name] = parse(value.substr(1)).executeApp(this);
        }
        else variables[name] = value;

        return ParsedCommand(VARIABLE);
    }

    vector<string> inSpl = splitBySpace(input);
    if (inSpl.empty()) return {};

    bool isSnippetFound = false;
    for (const auto&item: snippets) {
        if (item->getName() != inSpl[0]) continue;
        isSnippetFound = true;

        cout << "[*] " << item->getTarget() << endl << endl;
        parse(item->getTarget()).executeApp(this);
    }

    if (isSnippetFound) return ParsedCommand(SNIPPET);

    const shared_ptr<Command> app = findCommand(inSpl[0]);

    vector<string> args;
    for (int i = 1; i < inSpl.size(); ++i) {
        for (const auto&[name, value]: variables) {
            inSpl[i] = replace(inSpl[i], "{" + name + "}", value);
        }
        args.push_back(inSpl[i]);
    }

    ParsedCommand parsed = parseCommand(app.get(), args);
    return parsed;
}

vector<string> makeDictionary(const vector<shared_ptr<Command>>&cmds) {
    vector<string> dictionary;
    dictionary.reserve(cmds.size());
    for (const auto&item: cmds) {
        dictionary.push_back(item->getName());
    }
    return dictionary;
}

string getSuggestion(const Workspace&ws, const string&input) {
    vector<string> args = split(input, regex(R"(\s+)"));
    string suggestion;
    if (input[input.length() - 1] == ' ') args.emplace_back("");

    if (args.size() == 1) {
        suggestion = findSuggestion(ws, args[0], makeDictionary(commands))[0];
    }
    else {
        const shared_ptr<Command> cmd = findCommand(args[0]);
        if (cmd == nullptr) return "";

        const size_t cur = args.size() - 1;

        vector<string> usedOptions;
        for (int i = 1; i < args.size(); ++i) {
            if (string item = args[i]; item[0] == '-') {
                usedOptions.push_back(item.substr(1));
            }
            else {
                if (i <= 1 || args[i - 1][0] != '-') {
                    usedOptions.push_back(item);
                }
            }
        }

        if (args[cur][0] == '-') {
            // Find unused options
            vector<string> dict;
            for (const auto&item: cmd->getOptions()) {
                if (std::find(usedOptions.begin(), usedOptions.end(), item.name) == usedOptions.end()) {
                    dict.push_back(item.name);
                }
            }

            suggestion = findSuggestion(ws, args[cur].substr(1), dict)[0];
        }
        else {
            vector<string> dict;

            // For boolean options
            for (const auto&item: cmd->getOptions())
                if (item.type == BOOL_T &&
                    std::find(usedOptions.begin(), usedOptions.end(), item.name) == usedOptions.end())
                    dict.push_back(item.name);

            // For subcommands
            for (const auto&item: cmd->getSubcommands())
                dict.push_back(item->getName());

            suggestion = findSuggestion(ws, args[cur], dict)[0];
        }
    }
    return suggestion;
}

string getHint([[maybe_unused]] const Workspace&ws, const string&input) {
    const vector<string> args = split(input, regex(R"(\s+)"));

    if (args.size() == 1) {
        if (const shared_ptr<Command> command = findCommand(args[0]); command == nullptr) return "";
        else return command->createHint();
    }

    shared_ptr<Command> final = findCommand(args[0]);
    if (final == nullptr) return "";

    for (int i = 1; i < args.size(); i++) {
        shared_ptr<Command> sub = findCommand(args[i], final->getSubcommands());
        if (sub == nullptr) {
            return final->createHint();
        }
        final = sub;
    }

    return final->createHint();
}

string Workspace::prompt() const {
    stringstream ss;
    if (!name.empty())
        ss << fg_yellow << "[" << name << "] ";

    ss << fg_cyan << "(";
    if (dir == dir.root_path())
        ss << dir.root_name().string();
    else if (dir.parent_path() == dir.root_path())
        ss << dir.root_name().string() << "/" << dir.filename().string();
    else
        ss << dir.root_name().string() << "/.../" << dir.filename().string();
    ss << ")";

    ss << fg_yellow << " \u2192 " << reset;
    return ss.str();
}

void Workspace::inputPrompt() {
    cout << prompt();

    string input;
    if (isAnsiSupported()) {
        const int x = wherex();
        cout << endl;
        cout << teleport(x, wherey() - 1);
    }

    while (true) {
        int c = readChar();
        cout << erase_cursor_to_end;

        switch (c) {
            case '\b':
            case 127: {
                if (input.empty()) break;

                if (isAnsiSupported()) cout << teleport(wherex() - 1, wherey()) << erase_cursor_to_end;
                else cout << "\b";

                input = input.substr(0, input.length() - 1);
                break;
            }
            case '\n':
            case '\r': {
                cout << endl;

                if (!input.empty()) {
                    cout << erase_line;
                    addHistory(input);
                    ParsedCommand parsed = parse(input);
                    if (!parsed.isSuccessed()) {
                        vector<string> inSpl = splitBySpace(input);
                        error("Sorry. Command '$0' not found.", {inSpl[0]});
                        pair similarWord = {1000000000, Command("")};
                        for (const auto&cmd: commands) {
                            if (int dist = levenshteinDist(inSpl[0], cmd->getName());
                                dist < similarWord.first)
                                similarWord = {dist, *cmd};
                        }

                        if (similarWord.first > 1) warning("Please make sure you entered the correct command.");
                        else warning("Did you mean '$0'?", {similarWord.second.getName()});
                        return;
                    }

                    parsed.executeApp(this);
                }
                return;
            }
            case '\t': {
                string suggestion = getSuggestion(*this, input);
                input += suggestion;
                cout << "\033[0m" << suggestion;
                break;
            }
#ifdef _WIN32
            case 224: {
                const int i = readChar();
                const int mv = static_cast<int>(input.size());
                switch (i) {
                    case 72: {
                        cout << teleport(wherex() - mv, wherey());
                        cout << erase_cursor_to_end;
                        input = historyUp();
                        cout << input;
                        break;
                    }
                    case 80: {
                        cout << teleport(wherex() - mv, wherey());
                        cout << erase_cursor_to_end;
                        input = historyDown();
                        cout << input;
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
#elif defined(__linux__) || defined(__APPLE__)
                case 27: {
                    const int mv = static_cast<int>(input.size());
                    switch (readChar()) {
                        case 91: {
                            switch (readChar()) {
                                case 65: {
                                    cout << teleport(wherex() - mv, wherey());
                                    cout << erase_cursor_to_end;
                                    input = historyUp();
                                    cout << input;
                                    break;
                                }
                                case 66: {
                                    cout << teleport(wherex() - mv, wherey());
                                    cout << erase_cursor_to_end;
                                    input = historyDown();
                                    cout << input;
                                    break;
                                }
                                default:
                                    break;
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case 94: {
                    cout << "C";
                    break;
                }
#endif
            case '@': {
                cout << teleport(wherex() - static_cast<int>(input.size()) - 2, wherey());
                cout << erase_cursor_to_end;
                cout << fg_yellow << "@ " << reset;
                string wsName;
                getline(cin, wsName);

                cout << endl;
                if (wsMap.find(wsName) != wsMap.end()) {
                    currentWorkspace = wsMap[wsName];
                }
                else {
                    info("{FG_BLUE}New workspace created: {BG_GREEN}$0", {wsName});
                    currentWorkspace = new Workspace(wsName);
                }
                return;
            }
            case '&': {
                cout << teleport(wherex() - static_cast<int>(input.size()) - 2, wherey());
                cout << erase_cursor_to_end;
                cout << fg_yellow << "& " << reset;
                string command;
                getline(cin, command);

                system(command.c_str());
                return;
            }
            default: {
                string character(1, static_cast<char>(c));
                cout << reset << character;
                input += character;
            }
        }

        if (isAnsiSupported()) {
            string suggestion = getSuggestion(*this, input);
            cout << fgb_black << suggestion << reset;
            cout << teleport(wherex() - static_cast<int>(suggestion.size()), wherey());

            string hint = getHint(*this, input + suggestion);
            const int xPos = wherex() - static_cast<int>(hint.size()) / 2;
            cout << save_cursor_pos
                    << teleport(xPos < 0 ? 0 : xPos, wherey() + 1)
                    << erase_line
                    << fgb_black << hint
                    << restore_cursor_pos;
        }
    }
}

string Workspace::getName() {
    return name;
}

Workspace::Workspace(
    const string&name) : name(name) {
    wsMap[name] = this;
}

Workspace::Workspace() = default;
