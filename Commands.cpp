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

void JobsList::removeJobById(int jobId) {
    for (vector<JobEntry>::iterator i = jobsVector.begin(); i != jobsVector.end(); ++i) {
        if (i->getJobId() == jobId) {
            jobsVector.erase(i);
            return;
        }
    }
}

void JobsList::removeFinishedJobs() {
    auto it = jobsVector.begin();

    while (it != jobsVector.end()) {
        pid_t pid = it->getPid();
        int status;

        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == pid) {
            it = jobsVector.erase(it);
        }
        else if (result == -1) {
            it = jobsVector.erase(it);
        }
        else {
            ++it;
        }
    }
}

int JobsList::getNextJobID() {
    removeFinishedJobs();
    int maxId = 0;
    for (const auto &job : jobsVector) {
        if (job.getJobId() > maxId) {
            maxId = job.getJobId();
        }
    }
    return maxId + 1;
}

bool JobsList::is_there_a_job_with_pid(const int pid) {
    removeFinishedJobs();
    for (const auto &job : jobsVector) {
        if (job.getPid() == pid) {
            return true;
        }
    }
    return false;
}

void JobsList::addJob(Command *cmd, bool isStopped) {
    removeFinishedJobs();
    pid_t pid = cmd->getPid();
    string cmdLine = cmd->getCmdLine();
    JobEntry newJob;
    jobsVector.push_back(newJob);
}

void JobsList::printJobsList() {
    removeFinishedJobs();
    string resault;
    for (const auto &job : jobsVector) {
        resault += "[" + std::to_string(job.getJobId()) + "] " +
                       job.getCommandLine()  + "\n";
    }
}

SmallShell::SmallShell() : previousDir(nullptr) , aliasVector({})
{
    // TODO: add your implementation
}

SmallShell::~SmallShell() {
    // TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    char* args[COMMAND_MAX_ARGS];
    char** argv = args;
    int argc = _parseCommandLine(cmd_line, argv);
    if (argc == 0) return nullptr;

    for (const char &ch : string(cmd_line)) {
        if (ch == '|') {
            return new PipeCommand(cmd_line);
        }
    }
    if (string(argv[0]).compare("chprompt") == 0) {
        if(argc == 1) return new ChangePrompt("");
        return new ChangePrompt(argv[1]);
    }

    if (string(argv[0]).compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }

    if (string(argv[0]).compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }
/*
    if (string(argv[0]).compare("cd") == 0) {
        if(!argv[1])
        {
            char* pSecWord = &argv[1];
            return new ChangeDirCommand(pSecWord);
        }
    }
*/
    if (string(argv[0]).compare("alias") == 0) {
        return new AliasCommand(cmd_line);
    }
    if (string(argv[0]).compare("unalias") == 0) {
        return new UnAliasCommand(cmd_line);
    }
    if (string(argv[0]).compare("sysinfo") == 0) {
        return new SysInfoCommand(cmd_line);
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
    for (const auto& pair : this->aliasVector) {
        cout << pair.first
                  << "='" << pair.second << "'" << endl;
    }
}

bool AliasExists(const std::string& newName)
{
    for (const auto& pair : SmallShell::getInstance().getAliasVector()) {
        if (pair.first == newName) {
            return true;
        }
    }
    return false;
}

void AliasRemove(const std::string& Name)
{
    vector<pair<string,string>>& vector = SmallShell::getInstance().getAliasVector();
    for (auto it = vector.begin(); it != vector.end();) {
        if (it->first == Name) {
            vector.erase(it);
            return;
        }
        ++it;
    }
}

void SmallShell::addAlias(char** argv)
{
    int name_end = string(argv[1]).find_first_of('=');
    string name = string(argv[1]).substr(0, name_end);
    int command_length = string(argv[1]).length() - (name_end + 2);
    string command_string = string(argv[1]).substr(name_end + 2, command_length - 1);
    if(!AliasExists(name))
    {
        this->aliasVector.push_back({name, command_string});
    }
    else
    {
        cout << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
    }
}

void AliasCommand::execute()
{
    const char* raw_cmd_line = this->cmd_line;
    string cmd_s = _trim(raw_cmd_line);
    char* args[COMMAND_MAX_ARGS];
    char** argv = args;
    int argc = _parseCommandLine(raw_cmd_line, argv);
    if (argc == 1)
    {
        SmallShell::getInstance().printAlias();
        return;
    }
    const std::regex pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
    if(std::regex_match(cmd_s, pattern))
    {
        SmallShell::getInstance().addAlias(argv);
    }
    else
    {
        cout << "smash error: alias: invalid alias format" << endl;
    }
}

UnAliasCommand::UnAliasCommand(const char* cmd_line) : BuiltInCommand("")
{
    this->cmd_line = cmd_line;
}

void UnAliasCommand::execute()
{
    const char* raw_cmd_line = this->cmd_line;
    char* args[COMMAND_MAX_ARGS];
    char** argv = args;
    int argc = 0;
    argc = _parseCommandLine(raw_cmd_line, argv);
    if (argc == 1)
    {
        cout << "smash error: unalias: not enough arguments" << endl;
    }
    else
    {
        int i = 1;
        while(i < argc)
        {
            if(!AliasExists(std::string(argv[i])))
            {
                cout << "smash error: unalias: " << argv[i] << " alias does not exist" << endl;
                return;
            }
            else
            {
                AliasRemove(std::string(argv[i]));
            }
            i++;
        }
    }
    for (int i = 0; i < argc; ++i) {
        free(argv[i]);
    }
}

SysInfoCommand::SysInfoCommand(const char* cmd_line) : BuiltInCommand("")
{

}

void SysInfoCommand::execute()
{

}

PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line) {
    std::string s1, s2;
    bool foundPipe = false;
    bool found_AND_after_pipe = false;
    for (const char &ch : std::string(cmd_line)) {
        if (ch == '|') {
            foundPipe = true;
            continue;
        }
        if (foundPipe) {
            if (ch == '&') {
                found_AND_after_pipe = true;
                continue;
            }
            s2 += ch;
        }else {
            s1 += ch;
        }
    }
    firstCommand = SmallShell::getInstance().CreateCommand(s1.c_str());
    secondCommand = SmallShell::getInstance().CreateCommand(s2.c_str());
    am_i_with_AND = found_AND_after_pipe;
}

void PipeCommand::execute() {

}



