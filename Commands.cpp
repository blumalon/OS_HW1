#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <fcntl.h>
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
    for (unsigned int i = 0; i < jobsVector.size(); ++i) {
        if (jobsVector[i].getJobId() == jobId) {
            jobsVector.erase(jobsVector.begin() + i);
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
    pid_t m_pid = cmd->getPid();
    string cmdLine = cmd->getCmdLine();
    JobEntry* newJob = new JobEntry(m_pid);
    newJob->set_jobID(this->getNextJobID());
    for (unsigned int i = 0; i< jobsVector.size(); i++) {
        if (jobsVector[i].getPid() == -2 || (jobsVector.begin() + i) == jobsVector.end()) {
            jobsVector[i] = *newJob;
        }
    }
}

void JobsList::printJobsList_forJOBS() {
    removeFinishedJobs();
    string resault;
    for (const auto &job : jobsVector) {
        resault += "[" + std::to_string(job.getJobId()) + "] " +
                       job.getCommandLine()  + "\n";
    }
    std::cout << resault << std::endl;
}
/*
void JobsList::printJobsList_forQUIT() {
    removeFinishedJobs();
    cout << "smash: sending SIGKILL signal to " << this->jobsVector.size() << " jobs:" << endl;

    string resault;
    for (const auto &job : jobsVector) {
        resault += "[" + std::to_string(job.getJobId()) + "] " +
                       job.getCommandLine()  + "\n";
    }
    std::cout << resault << std::endl;
}
*/

SmallShell::SmallShell() :
previousDir(nullptr) , aliasVector({}), m_job_list(new JobsList()){}

SmallShell::~SmallShell() {
    delete m_job_list;
}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    char* args[COMMAND_MAX_ARGS];
    char** argv = args;
    int argc = _parseCommandLine(cmd_line, argv);
    if (_trim(string(cmd_line)).empty()) return nullptr;
    //NEED TO FIX/ YOU RUIN THE ORIGINAL CMDLINE THAT IS SUPPOSED TO BE SAVED////////
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
///////////////////////////////////////////////////////////////////////////////////////
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

    if (string(argv[0]).compare("jobs") == 0) {
        return new JobsCommand(cmd_line);
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

    string line = cmd_line;
    for (auto ch: line) {
        if(ch == '|') {
            return new PipeCommand(cmd_line);
        }
    }
    if (string(argv[0]).compare("alias") == 0) {
        return new AliasCommand(cmd_line);
    }
    if (string(argv[0]).compare("whoami") == 0) {
        return new WhoAmICommand(cmd_line);
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

Command::~Command() = default;

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
}

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {/*
    bool end_of_task = true;
    for (auto& ch : cmd_line) {
        if (ch != WHITESPACE) {
            end_of_task = false;
        }
        if (ch == '*' || ch == '?')
            am_i_complex = true;
        if (end_of_task && ch == '&')
            am_i_in_background = true;
        end_of_task = true;
    }
    if (am_i_in_background) {
        _removeBackgroundSign((char*)cmd_line);
    }*/
}

void ExternalCommand::execute() {
    unsigned int ssize = this->getCmdLine().size() + 1;
    char* cpy_line = (char*)malloc(ssize * sizeof(char));
   for (unsigned int i = 0; i < ssize - 1; i++) {
       cpy_line[i] = this->getCmdLine()[i];
   }
    cpy_line[ssize - 1] = '\0';
    pid_t pid1 = fork();
    if (pid1 == -1) {
        free(cpy_line);
        return;
    }
    if (pid1 == 0) { // child proccess
        if (am_i_complex) {
            char bash_path[] = "/bin/bash";
            char flag[] = "-c";
            char* args[] = { bash_path, flag, cpy_line, nullptr };
            execv(bash_path, args);
            exit(1); // if we got here, the execv FAILED
        } else {
            char* bash_args[20]; // Assuming max 20 args per requirements
            int i = 0;
            char* token = strtok(cpy_line, " \t\n");
            while (token != nullptr && i < 19) {
                bash_args[i++] = token;
                token = strtok(nullptr, " \t\n");
            }
            bash_args[i] = nullptr;
            execvp(bash_args[0], bash_args);
            exit(1);
        }
    } else {//parent proccess
        free(cpy_line);
        if (am_i_in_background) {
            SmallShell::getInstance().getJobList()->addJob(this, pid1);
        }
        else {
            waitpid(pid1, nullptr, 0);
        }
    }
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
    cout << "To Do" << endl;
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
    int my_pipe[2];
    while (pipe(my_pipe) == -1) {}
    pid_t pid1 = fork();
    while (pid1 == -1) {
        pid1 = fork();
    }
    if (pid1 == 0) { //the child proccess
        close(my_pipe[0]); //close read end
        int fd_to_write = (this->am_i_with_AND) ? STDERR_FILENO : STDOUT_FILENO;
        int dup_worked = dup2(my_pipe[1], fd_to_write); //redirect stdout to write end of pipe
        while (dup_worked == -1) {
            dup_worked = dup2(my_pipe[1], fd_to_write);
        }
        close(my_pipe[1]); //close write end of pipe
        firstCommand->setPID(getppid());
        SmallShell::getInstance().getJobList()->addJob(firstCommand, 0);
        firstCommand->execute();
        exit(0);
    }
    pid_t pid2 = fork();
    while (pid2 == -1) {
        pid2 = fork();
    }
    if (pid2 == 0) { //the second child proccess
        close(my_pipe[1]); //close write end of pipe
        int dup_worked = dup2(my_pipe[0], 0); //redirect stdin to read end of pipe
        while (dup_worked == -1) {
            dup_worked = dup2(my_pipe[0], 0);
        }
        close(my_pipe[0]); //close read end of pipe
        secondCommand->setPID(getppid());
        SmallShell::getInstance().getJobList()->addJob(secondCommand, 0);
        secondCommand->execute();
        exit(0);
    }
    close(my_pipe[0]);
    close(my_pipe[1]);
    waitpid(pid1, nullptr, 0);
    waitpid(pid2, nullptr, 0);
}

WhoAmICommand::WhoAmICommand(const char* cmd_line) : Command(cmd_line){}

void WhoAmICommand::execute() {
    uid_t my_uid = getuid();
    gid_t my_gid = getgid();
    std::string username = "idk";
    std::string home_directory = "idk";
    std::string lineBuff;
    int fd = open("/etc/passwd", O_RDONLY);
    if (fd == -1) {
        throw runtime_error("Could not open /etc/passwd");
    }
    char ch;
    std::vector<std::string> segments;
    std::string current_segment;
    while ((read(fd,&ch, 1) > 0)) {// read file char by char
        if (ch != '\n') {// if not end of line
            lineBuff += ch;
        } else {
            for (auto c : lineBuff) {// parse line into segments
                if (c != ':') {          //seperated by ':'
                    current_segment += c;
                } else {
                    segments.push_back(current_segment);
                    current_segment.clear();
                }
            }
            segments.push_back(current_segment);
            if (segments.size() >= 6) {
                unsigned int line_uid = stoi(segments[2]);
                if (line_uid == my_uid) {
                    username = segments[0];
                    home_directory = segments[5];
                    break;
                }
            }
            lineBuff.clear();
        }
        current_segment.clear();
        segments.clear();
    }
    std::cout << username <<  std::endl;
    std::cout << my_uid << std::endl;
    std::cout << my_gid <<  std::endl;
    std::cout << home_directory <<  std::endl;
    close (fd);
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
        }*/
