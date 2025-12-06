#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <sstream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iomanip>
#include "Commands.h"
#include <regex>
#include <sys/syscall.h>
#include <linux/limits.h>
#include <dirent.h>
#include <time.h>

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
#define SYSINFO_BUFFER_SIZE 2048
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
        if (jobsVector[i]->getJobId() == jobId) {
            jobsVector.erase(jobsVector.begin() + i);
            return;
        }
    }
}

void JobsList::removeFinishedJobs() {
    auto it = jobsVector.begin();

    while (it != jobsVector.end()) {
        pid_t pid = it.operator*()->getPid();
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
        if (job->getJobId() > maxId) {
            maxId = job->getJobId();
        }
    }
    return maxId + 1;
}

void JobsList::send_SIGKILL_to_all_jobs() {
    for (auto &job: jobsVector) {
        kill(job->getPid(), SIGKILL);
    }
}


bool JobsList::is_there_a_job_with_pid(const int pid) {
    removeFinishedJobs();
    for (const auto &job : jobsVector) {
        if (job->getPid() == pid) {
            return true;
        }
    }
    return false;
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
    removeFinishedJobs();
    for (JobEntry* job: jobsVector) {
        if (job->getJobId() == jobId)
            return job;
    }
    return nullptr;
}


void JobsList::addJob(Command *cmd, pid_t pid_to_use) {
    removeFinishedJobs();
    pid_t m_pid = pid_to_use;
    string cmdLine = cmd->getCmdLine_Print();
    JobEntry* newJob = new JobEntry(m_pid, cmdLine);
    newJob->set_jobID(this->getNextJobID());
    jobsVector.push_back(newJob);
   // cout << "added: "<< newJob->getCommandLine() << endl;
}


void JobsList::printJobsList_forJOBS() {
    removeFinishedJobs();
    string resault;
    for (const auto &job : jobsVector) {
        resault += "[" + std::to_string(job->getJobId()) + "] " +
                       job->getCommandLine()  + "\n";
       // std::cout << job->getCommandLine() << endl;
    }
    std::cout << resault;
}

void JobsList::printJobsList_forQUIT() {
    removeFinishedJobs();
    cout << "smash: sending SIGKILL signal to " << this->jobsVector.size() << " jobs:" << endl;
    string resault;
    for (const auto &job : jobsVector) {
        resault += std::to_string(job->getPid()) + ": " +
                       job->getCommandLine()  + "\n";
    }
    std::cout << resault;
    send_SIGKILL_to_all_jobs();
    exit(0);
}


SmallShell::SmallShell() :
previousDir(nullptr) , aliasVector({}), m_job_list(new JobsList()) {
}

SmallShell::~SmallShell() {
    delete m_job_list;
}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    char* args[COMMAND_MAX_ARGS];
    char** argv = args;
    //char* original_comman_line = strdup(cmd_line);
    bool is_alias = false;
    string new_command_line;
    const char* new_command_line_ptr = nullptr;
    int argc = _parseCommandLine(cmd_line, argv);
    if (_trim(string(cmd_line)).empty()) return nullptr;
    //NEED TO UPDATE WHERE TO SEND ORIGINAL_COMMAND_LINE////////
    for (auto& pair : this->aliasVector)
    {
        if (pair.first.compare(string(argv[0])) == 0) {
            is_alias = true;
            new_command_line = pair.second;
            for (int i = 1; i < argc; ++i) {
                new_command_line += " ";
                new_command_line += argv[i];
                free(argv[i]);
            }
            if (argv[0] != nullptr) free(argv[0]);
            new_command_line_ptr = new_command_line.c_str();
            char* strPtr = strdup(new_command_line_ptr);
            argc = _parseCommandLine(strPtr, argv);
            free(strPtr);
            break;
        }
    }
    if (string(argv[0]).compare("alias") == 0) {
        return new AliasCommand(cmd_line);
    }
    string command_to_check = is_alias ? (new_command_line) : string(cmd_line);
    for (unsigned int i = 0; i < command_to_check.size() - 1 ; i++){
        if (command_to_check[i] == '>' && command_to_check[i+1] == '>')
        {
            int command_end = command_to_check.find_last_of('>');
            string command = command_to_check.substr(0, command_end - 1);
            string path = command_to_check.substr(command_end + 1, std::string::npos);
            return new RedirectionCommand(command,path, true, false);
        }
    }
    for (const char &ch : command_to_check) {
        if (ch == '>') {
            int command_end = command_to_check.find_first_of(ch);
            string command = command_to_check.substr(0, command_end);
            string path = command_to_check.substr(command_end + 1, std::string::npos);
                return new RedirectionCommand(command,path, false, true);
        }
    }


    if (argc == 0) return nullptr;

    for (const char &ch : is_alias ? string(new_command_line_ptr) : string(cmd_line)) {
        if (ch == '|') {
            if (is_alias) return new PipeCommand(new_command_line_ptr);
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
            cerr << "smash error: cd: too many arguments" << endl;
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

    if (string(argv[0]).compare("fg") == 0) {
        if (argc > 2)
        {
            throw std::invalid_argument("smash error: fg: invalid arguments");
        }
        if (argc == 2) {
            int num_id = 1;
            try {
                num_id = stoi(string(argv[1]));
            } catch(std::exception &e) {
                throw std::invalid_argument("smash error: fg: invalid arguments");
            }
            if (num_id < 0) {
                string to_throw = "smash error: fg: job-id "+std::to_string(num_id)+" does not exist";
                throw std::invalid_argument(to_throw);
            }
            return new ForegroundCommand(cmd_line, num_id);
        }
        return new ForegroundCommand(cmd_line);
    }
    if (string(argv[0]).compare("kill") == 0) {
        return new KillCommand(cmd_line, m_job_list);
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
    if (string(argv[0]).compare("quit") == 0) {
        if(argv[1] && string(argv[1]).compare("kill") == 0)
            return new QuitCommand(cmd_line, this->m_job_list, true);
        return new QuitCommand(cmd_line, this->m_job_list, false);
    }
    if (string(argv[0]).compare("du") == 0) {
        if (argc < 2) cerr << "smash error: du: too many arguments" << endl;
        if (argc == 2) return new DiskUsageCommand(cmd_line, argv[2], false);
        if (argc == 1) return new DiskUsageCommand(cmd_line, argv[2], true);
    }
    // For example:
    /*


    else if ...
    .....
    */
    else {
        if(is_alias)
        {
            const char* new_command_line_ptr = new_command_line.c_str();
            char* newStr = strdup(new_command_line_ptr);
            return new ExternalCommand(newStr);
        }
        return new ExternalCommand(cmd_line);
    }

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd =CreateCommand(cmd_line);
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

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line) {
    cmdLine = _trim(cmdLine);
    std::vector<string> args;
    string segment;
    for (auto ch:cmdLine) {
        if (WHITESPACE.find(ch) == false) {
            segment += ch;
        } else {
            args.push_back(segment);
            segment.clear();
        }
    }
    if (args.size() > 2) {
        if (args[1][0] != '-' || args[1].size() != 2) {
            throw std::invalid_argument("smash error: kill: invalid arguments");
        }
        string signum;
        signum += args[1][1];
        signum_to_send = stoi(signum);
        if (args[2].size() != 1)
            throw std::invalid_argument("smash error: kill: invalid arguments");
        job_id = stoi (args[2]);
        return;
    }
    throw std::invalid_argument("smash error: kill: invalid arguments");
}

void KillCommand::execute() {
    JobsList::JobEntry* job_to_signal = SmallShell::getInstance().getJobList()->getJobById(job_id);
    if (!job_to_signal) {
        string to_throw;
        to_throw += "smash error: kill: job-id ";
        to_throw[to_throw.size() - 2] = (char)('0' + job_id);
        to_throw += " does not exist";
        throw std::out_of_range(to_throw);
    }
    if (signum_to_send < 0 || signum_to_send > 31)
        throw std::out_of_range("smash error: kill failed");
    pid_t pid_of_job = job_to_signal->getPid();
    kill (pid_of_job, signum_to_send);
    cout << "signal number " <<signum_to_send<< " was sent to pid " << pid_of_job;
}



ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {
    std::string to_check = std::string(cmd_line);
    this ->cmd_to_print = to_check;
    for (auto ch: to_check) {
        if (ch == '*' || ch == '?')
            am_i_complex = true;
        if (ch == '&')
            am_i_in_background = true;
    }
    if (am_i_in_background) {
        //_removeBackgroundSign((char*)cmd_line);
        for (auto &c : cmdLine) {
            if (c == '&')
                c = ' ';
        }
    }
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
        setpgrp();
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
            exit(1);//if we got here,
        }
    } else {//parent proccess
        free(cpy_line);
        if (am_i_in_background) {
            SmallShell::getInstance().getJobList()->addJob(this, pid1);
        }
        else {
            SmallShell::getInstance().pid_of_foreGround = pid1;
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
    size_t start_quote_pos = string(command_line).find_first_of('\'');
    size_t end_quote_pos = string(command_line).rfind('\'');
    string command_string = string(command_line).substr(start_quote_pos + 1, end_quote_pos - start_quote_pos - 1);
    if(!AliasExists(name))
    {
        this->aliasVector.push_back({name, command_string});
    }
    else
    {
        cerr << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
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
        cout << SmallShell::getInstance().getAliasVector().empty();
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

string get_kernel_release()
{
    char buffer[SYSINFO_BUFFER_SIZE];
    int fd = open("/proc/sys/kernel/osrelease", O_RDONLY);
    if (fd == -1) {
        std::cerr << "smash error: open failed on " << "/proc/sys/kernel/osrelease" << std::endl;
    }
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    }
    close(fd);
    string word = strtok(buffer, WHITESPACE.c_str());
    return word;
}
string get_system_type()
{
    char buffer[SYSINFO_BUFFER_SIZE];
    int fd = open("/proc/version", O_RDONLY);
    if (fd == -1) {
        std::cerr << "smash error: open failed on " << "/proc/version " << std::endl;
    }
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    }
    close(fd);
    string word = strtok(buffer, WHITESPACE.c_str());
    return word;
}
string get_hostname()
{
    char buffer[SYSINFO_BUFFER_SIZE];
    int fd = open("/proc/sys/kernel/hostname", O_RDONLY);
    if (fd == -1) {
        std::cerr << "smash error: open failed on " << "/proc/sys/kernel/hostname" << std::endl;
    }
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    }
    close(fd);
    string word = strtok(buffer, WHITESPACE.c_str());
    return word;
}

string get_boot_time()
{
    struct timespec timeElapsed;
    struct timespec currentTime;
    if(clock_gettime(CLOCK_BOOTTIME, &timeElapsed) != 0)
    {
        cerr << "Failed in getting boot time" << endl;
    }
    if(clock_gettime(CLOCK_REALTIME, &currentTime) != 0)
    {
        cerr << "Failed in getting current time" << endl;
    }
    time_t bootTimeInSec = currentTime.tv_sec - timeElapsed.tv_sec;
    struct tm* bootTime = std::localtime(&bootTimeInSec);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", bootTime);
    return string(buffer);
}

void SysInfoCommand::execute()
{

    cout << "System: " << get_system_type() << endl;
    cout << "Hostname: " << get_hostname() << endl;
    cout << "Kernel: " << get_kernel_release() << endl;
    cout << "Architecture: x86_64" << endl;
    cout << "Boot Time: " << get_boot_time() << endl;;
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
        setpgrp();
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
        setpgrp();
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

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs, bool isKill) : BuiltInCommand("")
{
    this->isKill = isKill;
    this->jobs = jobs;
}

void QuitCommand::execute()
{
    if (!isKill) exit(0);
    JobsList* jobL = SmallShell::getInstance().getJobList();
    jobL->printJobsList_forQUIT();
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, int id):BuiltInCommand(cmd_line), jobID_to_foreground(id) {}
ForegroundCommand::ForegroundCommand(const char *cmd_line):BuiltInCommand(cmd_line) {
    jobID_to_foreground = SmallShell::getInstance().getJobList()->getMaxID();
}

void ForegroundCommand::execute() {
    JobsList::JobEntry* to_bring = SmallShell::getInstance().getJobList()->getJobById(jobID_to_foreground);
    pid_t PID = to_bring->getPid();
    cout << to_bring->getCommandLine() << " " << (int)(PID) <<endl;
    waitpid(PID, nullptr, WNOHANG);
    SmallShell::getInstance().getJobList()->removeJobById(jobID_to_foreground);
}

DiskUsageCommand::DiskUsageCommand(const char *cmd_line, char* path, bool current) : Command(cmd_line)
{
    this->cmdLine = cmd_line;
    this->path = path;
    this->current = current;
}

// taken from man page
struct linux_dirent
{
    unsigned long d_ino;
    off_t d_off;
    unsigned short d_reclen;
    char d_name[];
};

int calcDiskAux(const char *path)
{
    int totalUsage = 0, blockSize = 512;
    struct stat sb;
    int lstatRes = lstat(path, &sb);

    if (lstatRes == -1)
    {
        perror("smash error: lstat failed");
        return -1;
    }

    // Usage of dir in path itself
    totalUsage += sb.st_blocks * blockSize;

    int dir = open(path, O_RDONLY);
    if (dir == -1)
    {
        perror("smash error: dir failed");
        return -1;
    }

    int bufferSize = 1024;
    char buffer[bufferSize];
    long getDentsRes;


    for (;;)
    {
        getDentsRes = syscall(SYS_getdents, dir, buffer, bufferSize);
        if (getDentsRes == -1)
        {
            perror("smash error: getdents failed");
            return -1;
        }
        else if (getDentsRes == 0)
        {
            break;
        }
        else
        {
            int idx = 0;
            struct linux_dirent *d;
            while (idx < getDentsRes)
            {
                d = (struct linux_dirent *)(buffer + idx);
                // already calculated the dir usage itself
                if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
                {
                    idx += d->d_reclen;
                    continue;
                }
                string fullPath = string(path) + "/" + d->d_name;
                struct stat sb2;
                //using fstatat instead of lstat because of path too long error
                int lstatRes2 = fstatat(dir, d->d_name, &sb2, AT_SYMLINK_NOFOLLOW);
                if (lstatRes2 == -1)
                {
                    perror("smash error: fstatat failed");
                    return -1;
                }
                else if (S_ISDIR(sb2.st_mode))
                {
                    totalUsage += calcDiskAux(fullPath.c_str());
                }
                else if (!S_ISLNK(sb2.st_mode))
                { // not sure if needs to check if reg file
                    totalUsage += sb2.st_blocks * blockSize;
                }
                idx += d->d_reclen;
            }
        }
    }
    close(dir);
    return totalUsage;
}

void DiskUsageCommand::execute()
{
    int res = -1;
    if (current)
    {
        char buffer[PATH_MAX];
        if (!getcwd(buffer, sizeof(buffer))) cout << "error!" << endl;
        res = calcDiskAux(buffer);
    }
    else
    {
        res = calcDiskAux(path);
    }
    res += 1023;
    res /= 1024;
    if (res == -1)
    cout << "Total disk usage: " << res << " KB" << endl;
}

RedirectionCommand::RedirectionCommand(std::string command,std::string path, bool is_append, bool is_overwrite) : Command("")
{
    this->command = _trim(command);
    this->path = _trim(path);
    this->is_append = is_append;
    this->is_overwrite = is_overwrite;
}

void RedirectionCommand::execute()
{
    int flags = O_WRONLY | O_CREAT;
    if (is_append) flags |= O_APPEND;
    if (is_overwrite) flags |= O_TRUNC;

    int stdout_temp = dup(STDOUT_FILENO);
    if (stdout_temp == -1) {
        perror("dup Failed");
        return;
    }

    int fd = open(path.c_str(), flags, 0644);
    if (fd == -1) {
        cerr << "Could Not Open File" << endl;
        return;
    }

    if (dup2(fd, STDOUT_FILENO) == -1)
    {
        cerr << "dup2 Failed" << endl;
        close(fd);
        return;
    }
    close(fd);
    Command* newCommand = SmallShell::getInstance().CreateCommand(command.c_str());
    if (newCommand)
    {
        newCommand->execute();
        delete newCommand;
    }
    if (dup2(stdout_temp, STDOUT_FILENO) == -1) {
        cerr << "smash error: dup2 restore failed" << endl;
    }
    close(stdout_temp);
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
