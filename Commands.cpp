#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <regex>

#include <linux/limits.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() : previousDir(nullptr) , alliasVector({}){
    // TODO: add your implementation
}

SmallShell::~SmallShell() {
    // TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    string secondWord;
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    size_t rest_start_pos = cmd_s.find_first_not_of(" \n", cmd_s.find_first_of(" \n"));
    if (cmd_s.find_first_of(" \n") == string::npos) {
        secondWord = "";
    }
    else
    {
        string remaining_args = cmd_s.substr(rest_start_pos);
        secondWord = remaining_args.substr(0, remaining_args.find_first_of(" \n"));
    }

    if (firstWord.compare("chprompt") == 0) {
      return new ChangePrompt(secondWord);
    }

    if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }

    if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }
/*
    if (firstWord.compare("cd") == 0) {
        if(!secondWord.empty())
        {
            char* pSecWord = &secondWord[0];
            return new ChangeDirCommand(pSecWord);
        }
    }
*/
    if (firstWord.compare("alias") == 0) {
        return new AliasCommand(cmd_line);
    }
    // For example:
    /*


    else if ...
    .....
    */
    else {
      return new ExternalCommand(cmd_line);
    }

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

ChangePrompt::ChangePrompt(std::string const& prompt) : BuiltInCommand(""){
    this->prompt = prompt;
}

void ChangePrompt::execute(){
    SmallShell& smash = SmallShell::getInstance();
    if(this->prompt.compare("")){
        smash.setPrompt(this->prompt);
    }
    else{
        smash.setPrompt("smash");
    }
}

Command::Command(const char *cmd_line) {
}

Command::~Command() = default;

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
}

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {

}

void ExternalCommand::execute() {
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(""){

}

void ShowPidCommand::execute(){
    std::cout << "smash pid is " << getpid() << std::endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(""){
}

void GetCurrDirCommand::execute(){
    char buffer[PATH_MAX];
    if (!getcwd(buffer, sizeof(buffer))) cout << "error!" << endl;
    else cout << buffer << endl;
}
/*
ChangeDirCommand::ChangeDirCommand(char* path, char** plastPwd) : BuiltInCommand("") , previousDir(plastPwd) , moveTo(path){
}

void ChangeDirCommand::execute()
{
    char* old_cwd = getcwd(nullptr, 0);
    if (!old_cwd) cout << "alocate error (add)" << endl;
    if ((strcmp(moveTo,"-") == 0) && previousDir == nullptr)
    {
        cout << "smash error: cd: OLDPWD not set" << endl;
        free(old_cwd);
    }
    else if ((strcmp(moveTo,"-") == 0) && previousDir != nullptr)
    {
        char* temp = *previousDir;
        *this->previousDir = old_cwd;
        old_cwd = temp;

        if(chdir(moveTo) != 0)
        {
            perror("chdir failed");
            if (strcmp(this->moveTo, "-") == 0) {
                char* temp2 = *this->previousDir;
                *this->previousDir = old_cwd;
                free(temp2);
            } else {
                free(old_cwd);
            }
        }
        else
            {
                if (strcmp(this->moveTo, "-") != 0) {
                    free(*this->previousDir);
                    *this->previousDir = old_cwd;
                }
            }
        }
    }
*/
AliasCommand::AliasCommand(const char* cmd_line) : BuiltInCommand("")
{
    this->cmd_line = cmd_line;
}

void SmallShell::printAlias()
{
    for (const auto& pair : this->alliasVector) {
        cout << pair.first
                  << "='" << pair.second << "'" << endl;
    }
}

void SmallShell::addAlias(std::string newAlias)
{
    size_t equals_pos = newAlias.find('=');
    std::string name = newAlias.substr(0, equals_pos);
    size_t start_quote_pos = newAlias.find('\'');
    size_t end_quote_pos = newAlias.rfind('\'');
    size_t command_start = start_quote_pos + 1;
    size_t command_length = end_quote_pos - command_start;
    std::string command_string = newAlias.substr(command_start, command_length);

    this->alliasVector.push_back({name, command_string});
}

void AliasCommand::execute()
{
    std::string cmd_s = _trim(std::string(this->cmd_line));
    size_t first_space = cmd_s.find_first_of(WHITESPACE);
    size_t arg_start = cmd_s.find_first_not_of(WHITESPACE, first_space);

    if (arg_start == std::string::npos)
    {
        SmallShell::getInstance().printAlias();
        return;
    }
    const std::regex pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
    if(std::regex_match(cmd_s, pattern))
    {
        std::string alias_arg = cmd_s.substr(arg_start);
        SmallShell::getInstance().addAlias(alias_arg);
    }
    else
    {
        cout << "bad syntax" << endl;
    }
}




