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
/*
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
    int jobId = getNextJobID();
    JobEntry newJob;
    newJob.setJobId(jobId);
    newJob.setPid(pid);
    newJob.setCommandLine(cmdLine);
    newJob.setStopped(isStopped);
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
*/
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
    if (_trim(string(cmd_line)).empty()) return nullptr;

    for (auto& pair : this->aliasVector)
    {
        if (pair.first.compare(string(argv[0])) == 0)
        {
            string new_command_line = pair.second;
            for (int i = 1; i < argc; ++i) {
                new_command_line += " ";
                new_command_line += argv[i];
                free(argv[i]);
            }
            if (argv[0] != nullptr) free(argv[0]);
            const char* new_command_line_ptr = new_command_line.c_str();
            char* strPtr = strdup(new_command_line_ptr);
            argc = _parseCommandLine(strPtr, argv);
            break;
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

    if (string(argv[0]).compare("cd") == 0) {
        if (argc > 2)
        {
            cout << "smash error: cd: too many arguments" << endl;
        }
        else if (argv[1] != nullptr)
        {
            return new ChangeDirCommand(argv[1]);
        }
    }

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
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    if (cmd)
    {
        cmd->execute();
        delete cmd;
        return;
    }
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


ChangeDirCommand::ChangeDirCommand(char* path) : BuiltInCommand("") , moveTo(path){
}

void ChangeDirCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    char* prevPath = *smash.getPreviousDirPtr();
    if (!prevPath && string(moveTo).compare("-") == 0)
    {
        cout << "smash error: cd: OLDPWD not set" << endl;
        return;
    }
    char* old_cwd = getcwd(nullptr, 0);
    if (!old_cwd) cout << "smash error: getcwd failed" << endl;
    if (prevPath != nullptr && string(moveTo).compare("-") == 0)
    {
        moveTo = prevPath;
        smash.setPreviousDirPtr(old_cwd);
        chdir(moveTo);
    }
    else
    {
        smash.setPreviousDirPtr(old_cwd);
        chdir(moveTo);
    }
}


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

void SmallShell::addAlias(char** argv, const char* cmd_line)
{
    char* command_line = strdup(string(cmd_line).c_str());
    int name_end = string(command_line).find('=');
    int name_start = string(command_line).find_first_of(' ');
    string name = string(command_line).substr(name_start + 1, name_end - name_start - 1);
    size_t start_quote_pos = string(command_line).find('\'');
    size_t end_quote_pos = string(command_line).rfind('\'');
    string command_string = string(command_line).substr(start_quote_pos + 1, end_quote_pos - start_quote_pos - 1);
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
        SmallShell::getInstance().addAlias(argv, cmd_line);
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
    cout << "hello" << endl;
}


void PipeCommand::execute() {

}
/*
UnSetEnvCommand::UnSetEnvCommand(const char* cmd_line) : BuiltInCommand("")
{

}

void UnSetEnvCommand::execute()
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
*/


