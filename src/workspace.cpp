#include "workspace.h"

#include <iostream>
#include <regex>
#include <utility>
#include <sstream>
#include <map>

#include "console.h"
#include "commandmgr.h"
#include "utils/utils.h"
#include "suggestion.h"
#include "sapp/downloader.h"
#include "sapp/sapp.h"
#include "version.h"

using namespace std;
using namespace std::filesystem;

map<string, Workspace> wsMap;

path Workspace::currentDirectory() {
  return dir;
}

void Workspace::moveDirectory(path newDir) {
  if (!is_directory(newDir)) {
    error("Directory '$0' not found.", {newDir.string()});
    return;
  }
  cout << dir.string() << endl;
  dir = std::move(newDir);
}

void Workspace::addHistory(const string &s) {
  history.push_back(s);
  historyIndex = historyIndex - 1;
}

string Workspace::historyUp() {
  if (0 > historyIndex - 1) return "";
  return history[--historyIndex];
}

string Workspace::historyDown() {
  if (history.size() <= historyIndex + 1) return "";
  return history[++historyIndex];
}
void Workspace::execute(const string &input) {
  vector<string> args = split(input, regex(R"(\s+)"));
  if (args.empty()) return;

  if (args[0] == "shfl") {
    for (const auto &item : commands) {
      cout << item.get()->getName() << "\n";
    }
    if (args.size() < 2) {
      too_many_arguments();
      return;
    }

    if (args[1] == "reload") {
      info("Reloading command...");
      loadCommands();
      success("Reloaded all commands!");
    } else if (args[1] == "apps") {
      if (args.size() < 2) {
        too_many_arguments();
        return;
      }
      if (args[2] == "add") {
        if (args.size() < 4) {
          too_many_arguments();
          return;
        }

        for (int i = 3; i < args.size(); ++i) {
          addSAPP(args[i]);
        }
      }
    } else if (args[1] == "update") {
      string latest = trim(readTextFromWeb("https://raw.githubusercontent.com/shflterm/shuffle/main/LATEST"));
      if (latest != SHUFFLE_VERSION.to_string()) {
        cout << "New version available: " << SHUFFLE_VERSION.to_string() << " -> " << latest << endl;
        updateShuffle();
      } else {
        cout << "You are using the latest version of Shuffle." << endl;
      }
    }

    return;
  }

  bool isCommandFounded = false;
  for (const auto &item : commands) {
    Command &cmd = *item;
    Command *command = &cmd;
    if (command->getName() != args[0]) continue;
    isCommandFounded = true;

    vector<string> newArgs;
    for (int i = 1; i < args.size(); ++i) newArgs.push_back(args[i]);

    Workspace &ws = (*this);
    if (dynamic_cast<SAPPCommand *>(command) != nullptr) {
      dynamic_cast<SAPPCommand *>(command)->run(*this, newArgs);
    } else {
      command->run(ws, args);
    }
//    } else if (command.getType() == EXECUTABLE) {
//      string cmd = command.getValue();
//      for (const auto &arg : newArgs) {
//        cmd.append(" ").append(arg);
//      }
//      system(cmd.c_str());
//    }
    break;
  } // Find Commands

  if (!isCommandFounded) {
    error("Sorry. Command '$0' not found.", {args[0]});
    pair<int, Command> similarWord = {1000000000, Command("")};
    for (const auto &item : commands) {
      Command &command = *item;
      int dist = levenshteinDist(args[0], command.getName());
      if (dist < similarWord.first) similarWord = {dist, command};
    }

    if (similarWord.first > 1) warning("Please make sure you entered the correct command.");
    else warning("Did you mean '$0'?", {similarWord.second.getName()});
  }
}

vector<string> getSuggestion(const Workspace &ws, const string &input) {
  vector<string> args = split(input, regex(R"(\s+)"));
  vector<string> suggestion;
  if (args.size() == 1) {
    suggestion = findSuggestion(ws, args[args.size() - 1], nullptr, commands);
  } else {
    Command *final = findCommand(args[0]);
    if (final == nullptr) return {""};

    for (int i = 1; i < args.size() - 1; i++) {
      Command *sub = findCommand(args[i], final->getChildren());
      if (sub == nullptr) return {""};
      final = sub;
    }

    suggestion = findSuggestion(ws, args[args.size() - 1], final, final->getChildren());

    delete final;
  }
  if (suggestion.empty()) return {""};

  return suggestion;
}

string Workspace::prompt() {
  stringstream ss;
  if (!name.empty())
    ss << FG_YELLOW << "[" << name << "] ";

  ss << FG_CYAN << "(";
  if (dir == dir.root_path())
    ss << dir.root_name().string();
  else if (dir.parent_path() == dir.root_path())
    ss << dir.root_name().string() << "/" << dir.filename().string();
  else
    ss << dir.root_name().string() << "/../" << dir.filename().string();
  ss << ")";

  ss << FG_YELLOW << " \u2192 " << RESET;
  return ss.str();
}

void Workspace::inputPrompt(bool enableSuggestion) {
  cout << prompt();
  cout.flush();

  int suggestionIndex = 0;
  string input;
  if (enableSuggestion) {
    char c;
    while (true) {
      c = readChar();
      eraseFromCursor();

      if (c == '\b' || c == 127) {
        if (!input.empty()) {
          gotoxy(wherex() - 1, wherey());
          cout << ' ';
          gotoxy(wherex() - 1, wherey());
          input = input.substr(0, input.length() - 1);
        }
      } else if (c == '\n' || c == '\r') {
        break;
      } else if (c == '\t') {
        size_t suggestions = getSuggestion(*this, input).size();
        if (++suggestionIndex >= suggestions) suggestionIndex = 0;
      }  else if (c == ' ') {
        string suggestion = getSuggestion(*this, input)[suggestionIndex];
        input += suggestion + " ";
        cout << "\033[0m" << suggestion << " ";
      } else if (c == 38/*UP ARROW*/) {
        gotoxy(wherex() - (int) input.size(), wherey());
        eraseFromCursor();
        input = historyUp();
        cout << input;
      } else if (c == 40/*DOWN ARROW*/) {
        gotoxy(wherex() - (int) input.size(), wherey());
        eraseFromCursor();
        input = historyDown();
        cout << input;
      } else if (c == '@') {
        gotoxy(wherex() - (int) input.size() - 2, wherey());
        eraseFromCursor();
        cout << FG_YELLOW << "@ " << RESET;
        string wsName;
        getline(cin, wsName);

        white();
        if (wsMap.find(wsName) != wsMap.end()) {
          currentWorkspace = &wsMap[wsName];
        } else {
          info("{FG_BLUE}New workspace created: {BG_GREEN}$0", {wsName});
          currentWorkspace = new Workspace(wsName);
        }
        return;
      } else {
        cout << "\033[0m" << c;
        input += c;
        suggestionIndex = 0;
      }

      string suggestion = getSuggestion(*this, input)[suggestionIndex];
      cout << "\033[90m" << suggestion << RESET;
      cout.flush();
      gotoxy(wherex() - (int) suggestion.length(), wherey());
    }
    white();
  } else {
    getline(cin, input);
  }

  if (!input.empty()) {
    execute(input);
    addHistory(input);
  }
}

Workspace::Workspace(const string &name) : name(name) {
  wsMap[name] = *this;
}
Workspace::Workspace() = default;
