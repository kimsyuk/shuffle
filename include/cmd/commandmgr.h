//
// Created by winch on 5/5/2023.
//

#ifndef SHUFFLE_INCLUDE_COMMANDMGR_H_
#define SHUFFLE_INCLUDE_COMMANDMGR_H_

#include <vector>
#include <string>
#include <memory>
#include <functional>

#include "workspace.h"

using std::pair, std::shared_ptr;

typedef std::function<void(Workspace *, std::map<std::string, std::string> &)> cmd_t;

void do_nothing(Workspace *ws, map<string, string> &optionValues);

enum OptionType {
    TEXT_T,
    BOOL_T,
    INT_T,
};

class CommandOption {
public:
    string name;
    OptionType type;
    vector<string> aliases;

    CommandOption(string name, OptionType type);
    CommandOption(string name, OptionType type, const vector<string> &aliases);
};

class Command {
protected:
    string name;
    string description;
    vector<shared_ptr<Command>> subcommands;
    vector<CommandOption> options;
    cmd_t cmd;

public:
    [[nodiscard]] const string &getName() const;

    [[nodiscard]] const string &getDescription() const;

    [[nodiscard]] const vector<shared_ptr<Command>> &getSubcommands() const;

    [[nodiscard]] const vector<CommandOption> &getOptions() const;

    virtual void run(Workspace *ws, map<string, string> &optionValues) const;

    Command(string name, string description, const vector<shared_ptr<Command>> &subcommands, const vector<CommandOption> &options,
            cmd_t cmd);

    Command(string name, string description, const vector<shared_ptr<Command>> &subcommands, cmd_t cmd);

    Command(string name, string description, const vector<CommandOption> &options, cmd_t cmd);

    Command(string name, string description, cmd_t cmd);

    explicit Command(string name);

    shared_ptr<Command> parent;
};

class CommandData {
public:
    string name;
};

extern vector<shared_ptr<Command>> commands;

void loadDefaultCommands();

vector<CommandData> getRegisteredCommands();

bool addRegisteredCommand(const CommandData &data);

void clearCommands();

shared_ptr<Command> findCommand(const string &name, const vector<shared_ptr<Command>> &DICTIONARY);

shared_ptr<Command> findCommand(const string &name);

#endif //SHUFFLE_INCLUDE_COMMANDMGR_H_
