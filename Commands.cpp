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
extern char** environ;
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

struct linux_dirent {
    unsigned long  current_ino;
    unsigned long  current_off;
    unsigned short current_reclen;
    char           current_name[];
};


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

    if (string(argv[0]).compare("showpid") == 0 || string(argv[0]).compare("showpid&") == 0) {
      return new ShowPidCommand(cmd_line);
    }

    if (string(argv[0]).compare("jobs") == 0 || string(argv[0]).compare("jobs&") == 0) {
        return new JobsCommand(cmd_line);
    }

    if (string(argv[0]).compare("pwd") == 0 || string(argv[0]).compare("pwd&") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }

    if (string(argv[0]).compare("cd") == 0 ) {
        if (argc > 2)
        {
            if (string(argv[2]).compare("&") == 0)
                if (argc == 3) {
                    return new ChangeDirCommand(argv[1]);
                }
            throw std::invalid_argument("smash error: cd: too many arguments");
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
        if (argc > 2){
            if (argc > 3)
                throw std::invalid_argument("smash error: fg: invalid arguments");
            ssize_t idx = string(argv[2]).find_first_not_of(" ");
            string last_arg = string(argv[2]).substr(idx, idx+1);
            if (last_arg.compare("&") == 0) {
                int num_id;
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
        if (argc == 2) return new DiskUsageCommand(cmd_line, string(argv[1]));
        if (argc == 1)
        {
            return new DiskUsageCommand(cmd_line, "./");
        }
    }
    if (string(argv[0]).compare("unsetenv") == 0) {
        if (argc == 1) cerr << "smash error: unsetenv: not enough arguments" << endl;
        return new UnSetEnvCommand(cmd_line);
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
    cmd_to_print = std::string(cmd_line);
    if (cmd_to_print.size() < 2)
        return;
   cmdLine = string(cmd_line);
    for (auto &ch: cmdLine) {
        if (ch == '&')
            ch == ' ';
    }
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
        if (args[1][0] != '-' || args[1].size() > 3) {
            throw std::invalid_argument("smash error: kill: invalid arguments");
        }
        string signum;
        unsigned int i = 1;
        while (i < args[1]. size() - 1) {
            signum += args[1][i];
            i++;
        }
        if (args[1][i] != '&') {
            signum+= args[1][i];
        }
        signum_to_send = stoi(signum);
        string jobID;
        i = 0;
        while (i < args[2].size() - 1) {
            if (args[2][i] < '0' || args[2][i] > '9')
                throw std::invalid_argument("smash error: kill: invalid arguments");
            jobID += args[2][i];
            i++;
        }
        if (args[2][i] == '&') {
            job_id = stoi(jobID);
            return;
        }
        if (args[2][i] < '0' || args[2][i] > '9')
            throw std::invalid_argument("smash error: kill: invalid arguments");
        jobID += args[2][i];
        job_id = stoi(jobID);
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
    this ->cmd_to_print = std::string(cmd_line);
    bool first_AND = true;
    for (auto &ch: cmdLine) {
        if (ch == '*' || ch == '?')
            am_i_complex = true;
        if (ch == '&' && first_AND) {
            am_i_in_background = true;
            ch = ' ';
            first_AND = false;
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
        throw std::runtime_error("smash error: fork failed");
    }
    if (pid1 == 0) { // child proccess
        setpgrp();
        if (am_i_complex) {
            char bash_path[] = "/bin/bash";
            char flag[] = "-c";
            char* args[] = { bash_path, flag, cpy_line, nullptr };
            execv(bash_path, args);
            throw std::runtime_error("smash error: execv failed");
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
            throw std::runtime_error("smash error: execvp failed");
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
    if (!getcwd(buffer, sizeof(buffer))){
        throw std::runtime_error("smash error: getcwd failed");
    }
    cout << buffer << endl;
}


ChangeDirCommand::ChangeDirCommand(char* path) : BuiltInCommand("") , moveTo(path){
}

void ChangeDirCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    char* prevPath = *smash.getPreviousDirPtr();
    if (!prevPath && string(moveTo).compare("-") == 0)
    {
        cerr << "smash error: cd: OLDPWD not set" << endl;
        return;
    }
    char* old_cwd = getcwd(nullptr, 0);
    if (!old_cwd) cerr << "smash error: getcwd failed" << endl;
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
        //SmallShell::getInstance().getAliasVector().empty();
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
        // string flip = cmd_s;
        // int start = 0, end = cmd_s.size() - 1;
        // while (start < end) {
        //     char cpy = flip[start];
        //     flip[start] = flip[end];
        //     flip[end] = cpy;
        //     start++; --end;
        // }
        // size_t spot = flip.find_first_of("'");

        cerr << "smash error: alias: invalid alias format" << endl;
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
        cerr << "smash error: unalias: not enough arguments" << endl;
    }
    else
    {
        int i = 1;
        while(i < argc)
        {
            if(!AliasExists(std::string(argv[i])))
            {
                cerr << "smash error: unalias: " << argv[i] << " alias does not exist" << endl;
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
    for (const char &ch : std::string(cmd_line)) {
        if (foundPipe == false) {
            if (ch == '|') {
                foundPipe = true;
            } else {
                s1 += ch;
            }
        } else {
            s2 += ch;
        }
    }
   if (s2[0] == '&') {
       am_i_with_AND = true;
       s2 = s2.substr(1, s2.size() - 1);
   }
    firstCommand = SmallShell::getInstance().CreateCommand(s1.c_str());
    secondCommand = SmallShell::getInstance().CreateCommand(s2.c_str());
}



void PipeCommand::execute() {
    int my_pipe[2];
    while (pipe(my_pipe) == -1) {}
    pid_t pid1 = fork();
    if (pid1 == -1)
        throw std::runtime_error("smash error: fork failed");
    if (pid1 == 0) { //the child proccess
        close(my_pipe[0]); //close read end
        setpgrp();
        int fd_to_write = (this->am_i_with_AND) ? STDERR_FILENO : STDOUT_FILENO;
        int dup_worked = dup2(my_pipe[1], fd_to_write); //redirect stdout to write end of pipe
        if (dup_worked == -1)
            throw std::runtime_error("smash error: dup2 failed");
        close(my_pipe[1]); //close write end of pipe
        firstCommand->setPID(getppid());
        SmallShell::getInstance().getJobList()->addJob(firstCommand, 0);
        firstCommand->execute();
        exit(0);
    }
    pid_t pid2 = fork();
    if (pid2 == -1)
        throw std::runtime_error("smash error: fork failed");
    if (pid2 == 0) { //the second child proccess
        close(my_pipe[1]); //close write end of pipe
        setpgrp();
        int dup_worked = dup2(my_pipe[0], 0); //redirect stdin to read end of pipe
        if (dup_worked == -1)
            throw std::runtime_error("smash error: dup2 failed");
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
    SmallShell::getInstance().getJobList()->removeFinishedJobs();
    if (SmallShell::getInstance().getJobList()->jobsVector.size() == 0)
        throw std::invalid_argument("smash error: fg: jobs list is empty");
    JobsList::JobEntry* to_bring = SmallShell::getInstance().getJobList()->getJobById(jobID_to_foreground);
    if (to_bring == nullptr) {
        string to_throw = "smash error: fg: job-id "+to_string(jobID_to_foreground)+" does not exist";
        throw std::invalid_argument(to_throw);
    }
    pid_t PID = to_bring->getPid();
    cout << to_bring->getCommandLine() << " " << (int)(PID) <<endl;
    waitpid(PID, nullptr, WNOHANG);
    SmallShell::getInstance().getJobList()->removeJobById(jobID_to_foreground);
}

DiskUsageCommand::DiskUsageCommand(const char *cmd_line, string path) : Command(cmd_line)
{
    this->cmd_line = cmd_line;
    this->path = path;
}

size_t DUAux(string path){
    int block_size = 512;
    struct stat sb;
    if (lstat(path.c_str(), &sb) == -1) {
        perror("smash error: lstat failed");
        return 0;
    }
    size_t total_size_of_me = sb.st_blocks * block_size;

    if (S_ISDIR(sb.st_mode)) {
        int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
        if (fd == -1) {
            return total_size_of_me;
        }
        char buffer[4096];
        while (true) {
            long num_read = syscall(SYS_getdents, fd, buffer, sizeof(buffer));
            if (num_read == -1 || num_read == 0) break;

            for (long bpos = 0; bpos < num_read;) {
                struct linux_dirent* current = (struct linux_dirent*)(buffer + bpos);
                string currentName = current->current_name;
                if (currentName != "." && currentName != "..") {
                    string next_path;
                    if (path.back() == '/')
                        next_path = path + currentName;
                    else
                        next_path = path + "/" + currentName;
                    total_size_of_me += DUAux(next_path);
                }
                bpos += current->current_reclen;
            }
        }
        close(fd);
    }
    return total_size_of_me;
}


void DiskUsageCommand::execute()
{
        cout << "Total disk usage: " << DUAux(path) << " KB" << endl;
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
        perror("smash error: dup Failed");
        return;
    }

    int fd = open(path.c_str(), flags, 0644);
    if (fd == -1) {
        cerr << "smash error: Could Not Open File" << endl;
        return;
    }

    if (dup2(fd, STDOUT_FILENO) == -1)
    {
        cerr << "smash error: dup2 Failed" << endl;
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
        cerr << "smash error: dup2 failed" << endl;
    }
    close(stdout_temp);
}




UnSetEnvCommand::UnSetEnvCommand(const char* command_line) : BuiltInCommand("")
{
    const char* raw_cmd_line = command_line;
    char* args[COMMAND_MAX_ARGS];
    char** argv = args;
    int argc = 0;
    argc = _parseCommandLine(raw_cmd_line, argv);
    this->args = argv;
    this->agrc = argc;
}

void UnSetEnvCommand::execute()
{
    string varName, envVar;
    bool var_found = false;

    for(int j = 0; j < agrc - 1 ; j++)
    {
        varName = string(args[j+1]);
        int i = 0;
        while (environ[i] != nullptr) {
            envVar = string(environ[i]);
            size_t eq_pos = envVar.find('=');
            string current_Var = envVar.substr(0, eq_pos);
            if (varName == current_Var) {
                var_found = true;
                int k = i;
                while (environ[k] != nullptr) {
                    environ[k] = environ[k + 1];
                    k++;
                }
                break;
            }
            i++;
        }
        if (!var_found)
        {
            cerr << "smash error: unsetenv: " << varName << " does not exist" << endl;
            return;
        }
        var_found = false;
    }
}
